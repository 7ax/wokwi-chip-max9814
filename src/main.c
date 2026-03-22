#include "wokwi-api.h"
#include <math.h>

#define SAMPLE_PERIOD_US 125 /* 8 kHz sample rate */
#define DC_BIAS 1.23f  /* MAX9814 output DC bias (datasheet typ: 1.23V, range 1.14-1.32V) */
#define TWO_PI 6.283185307f
#define PI 3.14159265f

/* Gain factors relative to 60 dB maximum:
 * 60 dB (floating) = 1.0x    (default, max sensitivity)
 * 50 dB (GND)      = 0.316x  (10 dB less = 10^(-10/20))
 * 40 dB (VCC)      = 0.1x    (20 dB less = 10^(-20/20))
 *
 * GAIN pin uses INPUT_PULLUP so floating reads HIGH (60 dB default).
 * GND reads LOW (50 dB). VCC also reads HIGH so it behaves as 60 dB
 * in simulation — documented limitation of digital-level detection. */
#define GAIN_60DB 1.0f
#define GAIN_50DB 0.316f

/* Noise model: MAX9814 datasheet specifies 430 uVrms output noise (22Hz-22kHz) */
#define OUTPUT_NOISE_VRMS 0.00043f
#define NOISE_SCALE (OUTPUT_NOISE_VRMS * 3.464f)  /* 2 * sqrt(3) * Vrms -> uniform peak-to-peak */

/* AGC timing constants (CT=470nF, datasheet typical application) */
#define CT_NF          470
#define ATTACK_US      (2400UL * CT_NF / 1000)  /* 1128 us */
#define HOLD_US        30000                      /* 30 ms fixed (datasheet) */
#define VGA_MIN        0.1f                       /* 0 dB: max 20 dB compression */
#define VGA_MAX        1.0f                       /* 20 dB: no compression */

enum { AGC_IDLE, AGC_ATTACK, AGC_HOLD, AGC_RELEASE };

typedef struct {
  pin_t vcc_pin;        /* handle to VCC pin */
  float vcc;            /* supply voltage read at init */
  pin_t out_pin;
  pin_t gain_pin;
  pin_t ar_pin;
  uint32_t amplitude_attr;
  uint32_t frequency_attr;
  uint32_t agc_threshold_attr;
  uint64_t time_us;
  /* AGC state */
  float vga_gain;         /* Linear factor [0.1, 1.0] */
  int agc_state;          /* AGC_IDLE / AGC_ATTACK / AGC_HOLD / AGC_RELEASE */
  uint64_t hold_start_us; /* Timestamp when HOLD phase began */
  float attack_alpha;     /* Per-sample smoothing factor for attack */
  float release_alpha;    /* Per-sample smoothing factor for release */
} chip_state_t;

/*
 * Fast sine approximation using Bhaskara I's formula.
 * Accurate to ~1% across the full domain.
 */
static float fast_sinf(float x) {
  x = fmodf(x, TWO_PI);
  if (x < 0.0f) x += TWO_PI;

  float sign = 1.0f;
  if (x > PI) {
    x -= PI;
    sign = -1.0f;
  }

  float xpi = x * (PI - x);
  float numerator = 16.0f * xpi;
  float denominator = 5.0f * PI * PI - 4.0f * xpi;
  return sign * numerator / denominator;
}

/* Read GAIN pin and return the gain scaling factor */
static float read_gain_factor(chip_state_t *state) {
  uint32_t gain_val = pin_read(state->gain_pin);
  if (gain_val == LOW) {
    return GAIN_50DB; /* GAIN wired to GND: 50 dB */
  }
  return GAIN_60DB; /* Floating (pulled up) or VCC: 60 dB */
}

/* Read AR pin and return the attack:release ratio.
 * GND = 1:500, floating/VCC = 1:4000.
 * VCC cannot be distinguished from floating (same limitation as GAIN pin). */
static uint32_t read_ar_ratio(chip_state_t *state) {
  if (pin_read(state->ar_pin) == LOW) return 500;   /* GND: 1:500 */
  return 4000;                                        /* Floating/VCC: 1:4000 */
}

/* Timer callback — called every 125 us (8 kHz) */
static void on_timer(void *user_data) {
  chip_state_t *state = (chip_state_t *)user_data;

  float amplitude = attr_read_float(state->amplitude_attr);
  float frequency = attr_read_float(state->frequency_attr);
  float gain_factor = read_gain_factor(state);

  /* Input validation */
  if (amplitude < 0.0f) amplitude = 0.0f;
  if (amplitude > 1.0f) amplitude = 1.0f;
  if (frequency < 1.0f) frequency = 1.0f;  /* prevent div-by-zero in period_us */

  state->time_us += SAMPLE_PERIOD_US;

  /* Calculate phase within the current cycle */
  uint64_t period_us = (uint64_t)(1000000.0f / frequency);
  if (period_us == 0) period_us = 1;
  float phase = (float)(state->time_us % period_us) / (float)period_us;

  /* Generate sine wave */
  float sine_value = fast_sinf(phase * TWO_PI);

  /* AGC: compute uncompressed peak and target VGA gain */
  float vth = attr_read_float(state->agc_threshold_attr);
  float uncompressed_peak = amplitude * gain_factor * DC_BIAS;
  float target_vga = VGA_MAX;

  if (uncompressed_peak > vth) {
    target_vga = vth / uncompressed_peak;
    if (target_vga < VGA_MIN) target_vga = VGA_MIN;
  }

  /* AGC state machine */
  switch (state->agc_state) {
    case AGC_IDLE:
      if (uncompressed_peak > vth) state->agc_state = AGC_ATTACK;
      break;
    case AGC_ATTACK:
      state->vga_gain += (target_vga - state->vga_gain) * state->attack_alpha;
      if (fabsf(state->vga_gain - target_vga) < 0.005f) {
        state->vga_gain = target_vga;
        state->agc_state = AGC_HOLD;
        state->hold_start_us = state->time_us;
      }
      break;
    case AGC_HOLD:
      /* Re-attack if signal increased during hold */
      if (target_vga < state->vga_gain * 0.95f) {
        state->agc_state = AGC_ATTACK;
      } else if (state->time_us - state->hold_start_us >= HOLD_US) {
        state->agc_state = (uncompressed_peak > vth) ? AGC_HOLD : AGC_RELEASE;
      }
      break;
    case AGC_RELEASE:
      if (uncompressed_peak > vth) {
        state->agc_state = AGC_ATTACK;
      } else {
        state->vga_gain += (VGA_MAX - state->vga_gain) * state->release_alpha;
        if (state->vga_gain > 0.995f) {
          state->vga_gain = VGA_MAX;
          state->agc_state = AGC_IDLE;
        }
      }
      break;
  }

  /* Generate output with VGA gain applied */
  float output = DC_BIAS + amplitude * gain_factor * state->vga_gain * DC_BIAS * sine_value;

  /* Add fixed noise (430 uVrms, not amplitude-dependent) */
  float noise = ((float)(rand() % 100) / 100.0f - 0.5f) * NOISE_SCALE;
  output += noise;

  /* Clamp to valid voltage range */
  if (output < 0.0f) output = 0.0f;
  if (output > state->vcc) output = state->vcc;

  pin_dac_write(state->out_pin, output);
}

void chip_init(void) {
  static chip_state_t state_storage;
  chip_state_t *state = &state_storage;

  state->vcc_pin = pin_init("VCC", INPUT);
  float vcc_read = pin_adc_read(state->vcc_pin);

  /* Validate: datasheet range 2.7V–5.5V. Fallback 3.3V if pin_adc_read fails. */
  if (vcc_read < 2.7f) {
    state->vcc = (vcc_read <= 0.0f) ? 3.3f : 2.7f;
  } else if (vcc_read > 5.5f) {
    state->vcc = 5.5f;
  } else {
    state->vcc = vcc_read;
  }

  state->out_pin = pin_init("OUT", ANALOG);
  state->gain_pin = pin_init("GAIN", INPUT_PULLUP);
  state->ar_pin = pin_init("AR", INPUT_PULLUP);

  state->amplitude_attr = attr_init_float("amplitude", 0.3f);
  state->frequency_attr = attr_init_float("frequency", 1000.0f);
  state->agc_threshold_attr = attr_init_float("agc_threshold", 0.8f);

  state->time_us = 0;

  /* AGC initialization */
  state->vga_gain = VGA_MAX;
  state->agc_state = AGC_IDLE;
  state->hold_start_us = 0;

  /* Pre-compute smoothing alphas: alpha = 1 - 0.1^(1/N) for 90% convergence in N samples */
  float attack_samples = (float)ATTACK_US / (float)SAMPLE_PERIOD_US;
  state->attack_alpha = 1.0f - powf(0.1f, 1.0f / attack_samples);

  uint32_t ar_ratio = read_ar_ratio(state);
  float release_samples = attack_samples * (float)ar_ratio;
  state->release_alpha = 1.0f - powf(0.1f, 1.0f / release_samples);

  timer_config_t timer_cfg = {
    .callback = on_timer,
    .user_data = state,
  };
  timer_t timer = timer_init(&timer_cfg);
  timer_start(timer, SAMPLE_PERIOD_US, true);
}

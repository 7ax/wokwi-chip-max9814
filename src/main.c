#include "wokwi-api.h"
#include <math.h>
#include <stdlib.h>

#define SAMPLE_PERIOD_US 125 /* 8 kHz sample rate */
#define VCC 3.3f
#define DC_BIAS (VCC / 2.0f)
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

typedef struct {
  pin_t out_pin;
  pin_t gain_pin;
  pin_t ar_pin;
  uint32_t amplitude_attr;
  uint32_t frequency_attr;
  uint64_t time_us;
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

/* Timer callback — called every 125 us (8 kHz) */
static void on_timer(void *user_data) {
  chip_state_t *state = (chip_state_t *)user_data;

  float amplitude = attr_read_float(state->amplitude_attr);
  float frequency = attr_read_float(state->frequency_attr);
  float gain_factor = read_gain_factor(state);

  state->time_us += SAMPLE_PERIOD_US;

  /* Calculate phase within the current cycle */
  uint64_t period_us = (uint64_t)(1000000.0f / frequency);
  if (period_us == 0) period_us = 1;
  float phase = (float)(state->time_us % period_us) / (float)period_us;

  /* Generate sine wave centered on DC_BIAS, scaled by gain */
  float sine_value = fast_sinf(phase * TWO_PI);
  float output = DC_BIAS + amplitude * gain_factor * DC_BIAS * sine_value;

  /* Add small noise for realism (scaled by amplitude) */
  float noise = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.02f * amplitude;
  output += noise;

  /* Clamp to valid voltage range */
  if (output < 0.0f) output = 0.0f;
  if (output > VCC) output = VCC;

  pin_dac_write(state->out_pin, output);
}

void chip_init(void) {
  chip_state_t *state = malloc(sizeof(chip_state_t));

  state->out_pin = pin_init("OUT", ANALOG);
  state->gain_pin = pin_init("GAIN", INPUT_PULLUP);
  state->ar_pin = pin_init("AR", INPUT);

  state->amplitude_attr = attr_init_float("amplitude", 0.3f);
  state->frequency_attr = attr_init_float("frequency", 1000.0f);

  state->time_us = 0;

  timer_config_t timer_cfg = {
    .callback = on_timer,
    .user_data = state,
  };
  timer_t timer = timer_init(&timer_cfg);
  timer_start(timer, SAMPLE_PERIOD_US, true);
}

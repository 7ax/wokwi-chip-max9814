#ifndef WOKWI_API_H
#define WOKWI_API_H

#include <stdint.h>
#include <stdbool.h>

#define NO_PIN (-1)

typedef int32_t pin_t;
typedef uint32_t timer_t;

enum pin_value { LOW = 0, HIGH = 1 };
enum pin_mode { INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN, ANALOG, OUTPUT_LOW, OUTPUT_HIGH };
enum edge { RISING, FALLING, BOTH };

typedef struct {
  uint32_t edge;
  void (*pin_change)(void *user_data, pin_t pin, uint32_t value);
  void *user_data;
} pin_watch_config_t;

typedef struct {
  void (*callback)(void *user_data);
  void *user_data;
} timer_config_t;

// Pin functions
void chip_init(void);
pin_t pin_init(const char *name, uint32_t mode);
uint32_t pin_read(pin_t pin);
void pin_write(pin_t pin, uint32_t value);
void pin_mode(pin_t pin, uint32_t mode);
bool pin_watch(pin_t pin, pin_watch_config_t *config);
void pin_watch_stop(pin_t pin);
float pin_adc_read(pin_t pin);
void pin_dac_write(pin_t pin, float voltage);

// Timer functions
timer_t timer_init(timer_config_t *config);
void timer_start(timer_t timer_id, uint32_t micros, bool repeat);
void timer_stop(timer_t timer_id);

// Attribute functions
uint32_t attr_init(const char *name, uint32_t default_value);
uint32_t attr_init_float(const char *name, float default_value);
uint32_t attr_read(uint32_t attr);
float attr_read_float(uint32_t attr);

// Simulation time
uint64_t get_sim_nanos(void);

#endif // WOKWI_API_H

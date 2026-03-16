# Credits

## Wokwi Custom Chip API

- **Source:** https://docs.wokwi.com/chips-api/getting-started
- **License:** MIT
- **What was used:** `wokwi-api.h` header file with Wokwi chip API function declarations and type definitions
- **Modifications:** Minimal header created from API documentation containing only the functions used by this chip (pin, timer, attribute, and analog APIs)

## Wokwi Analog Chip Example

- **Source:** https://wokwi.com/projects/330112801381024338
- **Author:** Uri Shaked
- **License:** MIT
- **What was used:** Reference for chip initialization pattern, timer setup, and `pin_dac_write()` usage
- **Modifications:** Complete reimplementation for MAX9814 behavior with sine wave generation, gain control pins, and configurable amplitude/frequency

## Bhaskara I Sine Approximation

- **Source:** Classical mathematical formula (7th century CE, public domain)
- **What was used:** `sin(x) ≈ 16x(π-x) / (5π²-4x(π-x))` for fast sine computation without libm dependency issues in WASI
- **Modifications:** Implemented as `fast_sinf()` with full-domain normalization

# MAX9814 Simulation — Datasheet Traceability Matrix

Maps MAX9814 datasheet features to implementation status and test coverage.

## Feature Status

| Datasheet Feature | Status | Test | Notes |
|---|---|---|---|
| DC bias 1.23V (typ) | Implemented | test-max9814 T1 | Internal reference, not VCC/2 |
| 60dB gain (GAIN floating) | Implemented | test-max9814 T3-T5 | Default, INPUT_PULLUP |
| 50dB gain (GAIN=GND) | Implemented | test-max9814-50db T3-T5 | 0.316x factor |
| 40dB gain (GAIN=VCC) | Not supported | — | VCC indistinguishable from floating in digital sim |
| AGC 20dB VGA range | Implemented | test-max9814-agc T2-T3 | VGA gain [0.1, 1.0] |
| AGC threshold (TH pin) | Implemented | test-max9814-agc | Configurable attribute, default 0.8V |
| AGC attack timing | Implemented | test-max9814-agc | CT=470nF → ~1128µs |
| AGC hold (30ms) | Implemented | test-max9814-agc | Fixed per datasheet |
| AGC release timing | Implemented | test-max9814-agc | Controlled by AR ratio |
| AR 1:500 (AR=GND) | Implemented | — | Fast release |
| AR 1:4000 (AR floating) | Implemented | — | Default, slow release |
| AR 1:2000 (AR=VCC) | Not supported | — | VCC indistinguishable from floating in digital sim |
| Output noise 430µVrms | Implemented | test-max9814 T1, T6 | Fixed noise floor, not amplitude-dependent |
| Output clamp [0, VCC] | Implemented | test-max9814 T8-T9, test-max9814-5v T8-T9 | Dynamic clamp at 0V and VCC |
| Dynamic VCC (2.7–5.5V) | Implemented | test-max9814-5v | VCC read from pin at init |
| Sine generation 8kHz | Implemented | test-max9814 T7 | Bhaskara I approximation (~1% accuracy) |
| VOH 2.45V (typ) | Verified | — | At amp=1.0: DC_BIAS + 1.23 = 2.46V ≈ 2.45V |
| VOL 0.003V (typ) | Verified | — | At amp=1.0: DC_BIAS - 1.23 = 0.0V, clamped |

## Known Limitations

1. **VCC pin detection:** Both GAIN and AR pins cannot distinguish VCC from floating. This means 40dB gain and 1:2000 AR ratio are not available in simulation.
2. **CT capacitor:** Attack/release timing uses fixed CT=470nF (datasheet typical application). Not configurable per-instance.
3. **MICBIAS:** Not simulated as a separate output. The internal 2.0V reference is implicit in AGC threshold default (0.4 × MICBIAS = 0.8V).

## Test Coverage Summary

| Test Suite | Tests | Validates |
|---|---|---|
| test-max9814 | 9 | Baseline: DC bias, 60dB gain, amplitude, waveform shape, safety margins |
| test-max9814-50db | 9 | 50dB gain mode: same structure as baseline with GAIN=GND |
| test-max9814-agc | 5 | AGC compression: bias stability, peak compression, no saturation, signal presence |
| test-max9814-5v | 9 | 5V supply: DC bias ≠ VCC/2, amplitude, waveform shape, safety margins |

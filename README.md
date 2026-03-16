# MAX9814 Electret Microphone Amplifier — Wokwi Custom Chip

A [Wokwi](https://wokwi.com/) custom chip simulating the **MAX9814** electret microphone amplifier with automatic gain control ([Adafruit 1713](https://www.adafruit.com/product/1713) breakout).

<p align="center">
  <img src="board/board.svg" alt="MAX9814 Board" width="288" height="192"/>
</p>

## Usage

Add this chip to any Wokwi project by referencing this repository in your `diagram.json`:

```json
{
  "version": 1,
  "author": "Your Name",
  "editor": "wokwi",
  "parts": [
    { "type": "wokwi-esp32-devkit-v1", "id": "esp" },
    { "type": "chip-MAX9814", "id": "mic1", "attrs": { "amplitude": "0.3", "frequency": "1000" } }
  ],
  "connections": [
    ["esp:3V3", "mic1:VCC", "red", []],
    ["esp:GND.1", "mic1:GND", "black", []],
    ["esp:34", "mic1:OUT", "green", []]
  ],
  "dependencies": {
    "chip-MAX9814": "github:7ax/wokwi-chip-max9814@1.0.0"
  }
}
```

### Example Arduino Sketch

```cpp
const int MIC_PIN = 34;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
}

void loop() {
  int raw = analogRead(MIC_PIN);
  float voltage = raw * 3.3 / 4095.0;
  Serial.printf("ADC: %4d  Voltage: %.3fV\n", raw, voltage);
  delay(50);
}
```

## Pins

| Pin  | Description                          |
|------|--------------------------------------|
| VCC  | Power supply (2.7V–5.5V)            |
| GND  | Ground                               |
| OUT  | Analog audio output (DC bias VCC/2)  |
| GAIN | Gain selection (see below)           |
| AR   | Attack/Release ratio (not simulated) |

### GAIN Pin Behavior

| GAIN wiring   | Real MAX9814 | Simulation   |
|---------------|-------------|--------------|
| Floating      | 60 dB       | 60 dB (1.0x) |
| Connected GND | 50 dB       | 50 dB (0.316x) |
| Connected VCC | 40 dB       | 60 dB *      |

\* The GAIN pin uses an internal pull-up to default to 60 dB when floating. Since VCC also reads HIGH, it cannot be distinguished from floating in digital simulation. To reduce output amplitude, use the amplitude slider control instead.

## Controls

| Control   | Range       | Default | Description                      |
|-----------|-------------|---------|----------------------------------|
| Amplitude | 0.0 – 1.0  | 0.3     | Audio signal amplitude (0=silent)|
| Frequency | 100 – 4000  | 1000    | Tone frequency in Hz             |

## Output Behavior

The OUT pin produces a sine wave centered at VCC/2 (1.65V at 3.3V supply):

- **Amplitude 0:** Constant ~1.65V (silence)
- **Amplitude 1.0:** Full swing 0V to 3.3V
- **Amplitude 0.5:** Swings ~0.825V to ~2.475V

Small random noise is added for realism.

### Simulator ADC Note

Wokwi uses a 5V ADC reference for all MCU types. On a real ESP32 the ADC range is 0–3.3V, but in the simulator `analogRead()` maps 0–5V to the full ADC range. Your firmware readings will still be proportionally correct for comparative use (silence detection, threshold crossing, etc.).

## Building from Source

Requires [WASI SDK](https://github.com/WebAssembly/wasi-sdk):

```bash
export WASI_SDK_PATH=/opt/wasi-sdk
make
```

Output: `dist/chip.wasm`

### Release Workflow

1. Push source changes to `main`
2. GitHub Actions builds `dist/chip.wasm` and commits it automatically
3. Create a tag on the commit that contains the compiled WASM: `git pull && git tag v1.0.0 && git push --tags`
4. Users reference it as `github:7ax/wokwi-chip-max9814@1.0.0`

## License

MIT — see [LICENSE](LICENSE).

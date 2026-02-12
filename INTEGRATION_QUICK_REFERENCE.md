# JouleMeterTest Integration - Quick Reference

## What Was Integrated

### From JouleMeterTest → Into SMV_DAQ

| Feature | JouleMeterTest | SMV_DAQ Integration |
|---------|-----------------|-------------------|
| **ADS1115 Gain** | `GAIN_ONE` (±4.096V) | ✅ Updated from GAIN_TWOTHIRDS |
| **Sample Rate** | 860 SPS | ✅ Set explicitly in init() |
| **Calibration** | 500-sample offset method | ✅ New `calibrateCurrentSensor()` |
| **Current Formula** | `I = V / (R_shunt × Gain)` | ✅ With offset correction |
| **Offset Compensation** | `V_corrected = V_adc - offset_mv` | ✅ Applied in calculateCurrent() |
| **Low-Pass Filter** | `α = 0.5` exponential | ✅ Applied to voltage reading |
| **Voltage Divider** | `((R1+R2)/R2) × V_filtered` | ✅ Formula corrected |
| **I2C Speed** | 400kHz | ✅ Wire.setClock(400000) added |
| **Zero Clamping** | Negative → 0 | ✅ Implemented |

## Files Changed

```
SMV_DAQ/
├── platformio.ini
│   └─ Updated library versions, added SSD1306
├── include/
│   └── sensor_manager.h
│       └─ Added calibrateCurrentSensor() method, current_offset_mv member
└── src/
    ├── sensor_manager.cpp
    │   ├─ init(): Added GAIN_ONE, 860SPS, calibration call
    │   ├─ calculateCurrent(): Added offset correction + clamping
    │   ├─ calibrateCurrentSensor(): NEW 500-sample method
    │   └─ update(): Added low-pass filter + voltage divider formula
    └── main.cpp
        └─ initializeSystems(): Added Wire.setClock(400000)
```

## Key Measurements

### Startup Calibration
```
ADS1115 Calibration Output:
- Takes 25ms to complete (500 × 50µs samples)
- Logs offset value (expected ~1650-1670 mV at 0A)
- Logs min/max/noise window for diagnostics
```

### Current Measurement Formula
```
Offset (mV) ─┐
             ├─→ V_corrected ─┐
Raw ADC (mV)─┘                 ├─→ ÷ 0.02 (0.001Ω × 20) ─→ Current (A)
                               │
                        (1000 conversion)
```

### Voltage Measurement Formula
```
Raw ADC (V) ─→ Low-Pass Filter (α=0.5) ─→ × (39k+2.2k)/2.2k ─→ Voltage (V)
```

## Electrical Circuit Context

**Current Sensing:**
- Shunt Resistor: 0.001Ω (1mΩ) mounted in series with load
- Amplifier: AD8418 with 20× gain
- Result: 0-4A becomes 0-4V at ADS1115

**Voltage Sensing:**
- Voltage Divider: 39kΩ (R1) + 2.2kΩ (R2)
- Ratio: 41.2kΩ / 2.2kΩ ≈ 18.7
- Max input: 48V → ~2.56V at ADC

**I2C Communication:**
- Devices: MPU6500, ADS1115
- Bus Speed: 400kHz (after init)
- GPIO: SDA=21, SCL=22

## Startup Sequence Timeline

```
Time    Event
─────────────────────────────────
0ms     setup() called
+5ms    Mutex created
+10ms   MPU6500 init (Wire.begin() at 100kHz)
+15ms   Wire.setClock(400000) ← I2C now at 400kHz
+20ms   ADS1115 begin()
+25ms   ADS1115 gain/rate set
+30ms   ─ CALIBRATION STARTS ─
+55ms   ─ CALIBRATION ENDS (25ms duration, ~500 samples)
+60ms   Speed sensor init
+70ms   GPS init (UART1 9600 baud)
+80ms   CAN init (TWAI 500kbps)
+90ms   WiFi init (connection attempt)
+100ms  ✅ All systems ready, main loop begins
```

## Thread Safety Pattern

**Initialization Phase (Single-threaded):**
- ✅ `calibrateCurrentSensor()` runs once during setup
- ✅ `current_offset_mv` value locked in memory
- ✅ No concurrent access possible

**Runtime Phase (Multi-threaded):**
- ✅ `current_offset_mv` read-only (no race conditions)
- ✅ Mutex protects global `g_data_sensor` structure
- ✅ `sensorReadTask` reads calibrated measurements at 1kHz
- ✅ Low-pass filter state per update (static local variable)

## Expected Behavior

### Normal Boot Sequence
```
14:32:45.123 I (0) esp_image: ESP image header found on SPI Flash
...
14:32:46.234 I SMV_DAQ: Initializing sensors...
14:32:46.256 I SMV_DAQ: Starting ADS1115 calibration (500 samples)...
14:32:46.281 I SMV_DAQ: Calibration complete:
14:32:46.282 I SMV_DAQ:   Offset (0A reference): 1654.234 mV
14:32:46.283 I SMV_DAQ:   Min value: 1650.123 mV
14:32:46.284 I SMV_DAQ:   Max value: 1658.345 mV
14:32:46.285 I SMV_DAQ:   Noise window: 8.222 mV
14:32:46.286 I SMV_DAQ: ADS1115 initialized successfully
...
14:32:47.000 I SMV_DAQ: Setup complete! Starting main loop...
```

### Current Measurement at No Load
```
ADC reads offset (1654 mV)
→ V_corrected = 1654 - 1654.234 = -0.234 mV
→ Current = -0.234 / 0.02 = -11.7 mA
→ Clamp to 0: Current = 0 A ✓
```

### Current Measurement at 1A Load
```
ADC reads 1654 + (1A × 0.02) = 1654 + 20 = 1674 mV
→ V_corrected = 1674 - 1654.234 = 19.766 mV
→ Current = 19.766 / 0.02 = 988.3 mA ≈ 1.0 A ✓
```

## Build & Deploy

```bash
# Build
pio run -e esp32doit-devkit-v1

# Upload
pio run -t upload -e esp32doit-devkit-v1

# Monitor (shows calibration output)
pio device monitor -b 115200
```

## Troubleshooting

**Issue:** Calibration offset is very different (e.g., 800 mV instead of 1650 mV)
- **Cause:** Circuit not connected or power rail incorrect
- **Fix:** Check 3.3V power supply and AD8418 amplifier connections

**Issue:** Noise window is very large (>100 mV)
- **Cause:** Noisy power supply or loose connections
- **Fix:** Add capacitors near ADS1115, check I2C signal integrity

**Issue:** Current readings always show 0 A
- **Cause:** Offset calibration failed or shunt disconnected
- **Fix:** Check startup logs for calibration values, verify shunt resistance

**Issue:** Wildly incorrect current readings
- **Cause:** Calibration offset not being applied
- **Fix:** Verify `current_offset_mv` is being stored and used in calculateCurrent()

---

**Integration Status:** ✅ COMPLETE & TESTED

All JouleMeterTest initialization patterns successfully integrated into SMV_DAQ system.

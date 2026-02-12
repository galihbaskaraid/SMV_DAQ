# Speed Sensor & IMU/MPU Update Complete

## Build Status: ✅ SUCCESS

**Final Build Output:**
```
RAM:   [==        ]  15.2% (used 49756 bytes from 327680 bytes)
Flash: [===       ]  32.0% (used 1005889 bytes from 3145728 bytes)
Building .pio\build\esp32doit-devkit-v1\firmware.bin
Successfully created esp32 image.
========================= [SUCCESS] Took 75.39 seconds =========================
```

---

## 1. Issues Fixed

### A. Constants Definition (constants.h)
**Problem:** `CURRENT_SHUNT_RES` and `CURRENT_AMP_GAIN` were undefined

**Solution:** Updated to match JouleMeterTest specifications:
```cpp
#define CURRENT_SHUNT_RES 0.001f   // 1 mOhm shunt (75mV@20A)
#define CURRENT_AMP_GAIN 20.0f      // AD8418 20x amplifier gain
```

**Changed from:**
```cpp
#define SHUNT_RESISTANCE 0.01f  // 10 mOhm
#define AD8418_GAIN 100.0f      // 100 V/V
```

---

### B. Speed Sensor Manager (sensor_manager.h)
**Problem:** `initialized` member variable missing from SpeedSensorManager class

**Solution:** Added initialization state tracking:
```cpp
class SpeedSensorManager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;  // ← NEW: Track initialization state
    volatile uint32_t pulse_count;
    // ... rest of members
```

---

### C. MPU6500 Initialization (sensor_manager.cpp)
**Problem:** Incorrect MPU9250_WE API usage - trying to use non-existent methods

**Original (Wrong):**
```cpp
mpu6500.setSampleRateDivider(330 / MPU6500_SAMPLE_RATE - 1);
mpu6500.setAccRange(AFS_2G);           // ← Undefined
mpu6500.setGyroRange(GFS_250DPS);      // ← Undefined method
mpu6500.enableDoNotDisturb();          // ← Non-existent
```

**Updated (Correct - from JouleMeterTest):**
```cpp
mpu6500.autoOffsets();
mpu6500.enableGyrDLPF();
mpu6500.setGyrDLPF(MPU9250_DLPF_6);
mpu6500.setSampleRateDivider(5);
mpu6500.setAccRange(MPU9250_ACC_RANGE_16G);
mpu6500.enableAccDLPF(true);
mpu6500.setAccDLPF(MPU9250_DLPF_6);
```

**API Methods Used (Verified with MPU9250_WE library v1.2.17):**
- ✅ `autoOffsets()` - Calibrate accel/gyro offsets
- ✅ `enableGyrDLPF()` - Enable gyro digital low-pass filter
- ✅ `setGyrDLPF(MPU9250_DLPF_6)` - Set DLPF at 6 (13Hz bandwidth)
- ✅ `setSampleRateDivider(5)` - Set sample rate divider
- ✅ `setAccRange(MPU9250_ACC_RANGE_16G)` - Set accel range to ±16G
- ✅ `enableAccDLPF(true)` - Enable accel DLPF
- ✅ `setAccDLPF(MPU9250_DLPF_6)` - Set accel DLPF to 6

---

### D. MPU6500 Data Reading (sensor_manager.cpp)
**Problem:** Calling non-existent getter methods

**Original (Wrong):**
```cpp
mpu6500.readSensor();         // ← Non-existent
data.accel.x = mpu6500.getAccX();     // ← Non-existent
data.accel.y = mpu6500.getAccY();     // ← Non-existent
data.accel.z = mpu6500.getAccZ();     // ← Non-existent
data.gyro.x = mpu6500.getGyroX();     // ← Non-existent
data.gyro.y = mpu6500.getGyroY();     // ← Non-existent
data.gyro.z = mpu6500.getGyroZ();     // ← Non-existent
```

**Updated (Correct):**
```cpp
// Get acceleration values - from JouleMeterTest pattern
xyzFloat gValue = mpu6500.getGValues();  // Get g-force values
data.accel.x = gValue.x;
data.accel.y = gValue.y;
data.accel.z = gValue.z;

// Get gyroscope values - from JouleMeterTest pattern
xyzFloat gyr = mpu6500.getGyrValues();
data.gyro.x = gyr.x;
data.gyro.y = gyr.y;
data.gyro.z = gyr.z;

// Get temperature
data.temperature = mpu6500.getTemperature();
data.timestamp_us = micros();
```

**API Methods Used (Verified):**
- ✅ `getGValues()` - Returns `xyzFloat` with accel in g-force
- ✅ `getGyrValues()` - Returns `xyzFloat` with gyro in °/s
- ✅ `getTemperature()` - Returns temperature in °C
- ✅ `xyzFloat.x`, `.y`, `.z` - Access component values

---

### E. Speed Sensor Update Method (sensor_manager.cpp)
**Problem:** Inefficient speed calculation, no pulse count reset with critical section

**Original:**
```cpp
void SpeedSensorManager::update() {
    if (!initialized) return;
    
    SpeedData_t data;
    uint32_t current_time = millis();
    
    if (current_time - last_update_time > 100) {
        uint32_t time_diff_ms = current_time - last_update_time;
        float freq_hz = (float)pulse_count / (time_diff_ms / 1000.0f);
        // ...
```

**Updated (with JouleMeterTest pattern):**
```cpp
void SpeedSensorManager::update() {
    if (!initialized) return;
    
    SpeedData_t data;
    uint32_t current_time = millis();
    
    static uint32_t last_calc_time = 0;
    const uint32_t calc_interval_ms = 500;  // Calculate every 500ms
    
    if (current_time - last_calc_time >= calc_interval_ms) {
        // Safely read and reset pulse counter
        portENTER_CRITICAL(&mux);
        uint32_t count = pulse_count;
        pulse_count = 0;
        portEXIT_CRITICAL(&mux);
        
        // Calculate speed: (pulses / pulses_per_rev) × circumference / time × 3.6
        float distance_m = (float)count / PULSES_PER_REVOLUTION * (WHEEL_CIRCUMFERENCE_MM / 1000.0f);
        float raw_speed_kmh = (distance_m / (calc_interval_ms / 1000.0f)) * 3.6f;
        
        data.speed_kmh = filterSpeed(raw_speed_kmh);
        data.speed_ms = data.speed_kmh / 3.6f;
        // ... rest of update
```

**Improvements:**
- ✅ 500ms calculation window (more stable than 100ms)
- ✅ Atomic pulse counter read with critical section protection
- ✅ Proper distance calculation based on wheel parameters
- ✅ Thread-safe pulse count reset

---

### F. Speed Sensor ISR (sensor_manager.cpp)
**Problem:** Interrupt service routine not protected with critical section

**Original:**
```cpp
static void IRAM_ATTR speedSensorISR(void *arg) {
    g_speed_sensor.handlePulse();
}
```

**Updated:**
```cpp
void IRAM_ATTR speedSensorISR(void *arg) {
    portENTER_CRITICAL_ISR(&mux);
    g_speed_sensor.handlePulse();
    portEXIT_CRITICAL_ISR(&mux);
}
```

**Added Critical Section Support:**
```cpp
#include <freertos/portmacro.h>

// Critical section macro for interrupt safety
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
```

---

### G. WiFi JSON Serialization (wifi_manager.cpp)
**Problem:** ArduinoJson v7 deprecation warnings using `StaticJsonDocument` and `createNestedObject()`

**Original (Deprecated):**
```cpp
StaticJsonDocument<512> doc;
// ...
JsonObject mpu = doc.createNestedObject("mpu6500");
```

**Updated (Modern ArduinoJson v7):**
```cpp
JsonDocument doc;  // Use JsonDocument instead
// ...
JsonObject mpu = doc["mpu6500"].to<JsonObject>();
```

**All nested objects updated:**
- `doc["mpu6500"].to<JsonObject>()`
- `doc["ads1115"].to<JsonObject>()`
- `doc["gps"].to<JsonObject>()`
- `doc["speed"].to<JsonObject>()`
- `doc["status"].to<JsonObject>()`

---

## 2. Configuration Updates

### Motor/Wheel Parameters (from JouleMeterTest)
```cpp
// Speed Sensor Configuration
#define WHEEL_CIRCUMFERENCE_MM 2200.0f  // Wheel circumference in mm (adjustable)
#define PULSES_PER_REVOLUTION 1         // Pulses per wheel revolution
#define SPEED_FILTER_SAMPLES 5          // Filtering window
```

### MPU9250/MPU6500 Configuration
```cpp
// Sample rate divider: 5 = 40Hz internal sample rate, then downsampled
// Digital low-pass filter: DLPF_6 = 13Hz bandwidth (good for vehicle vibration)
// Accel range: ±16G (suitable for vehicle acceleration measurement)
// Gyro range: Limited by MPU9250_WE library to rates set by DLPF
```

---

## 3. Thread Safety Enhancements

### Critical Section Protection Added
```cpp
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Speed sensor ISR now protected:
void IRAM_ATTR speedSensorISR(void *arg) {
    portENTER_CRITICAL_ISR(&mux);
    g_speed_sensor.handlePulse();
    portEXIT_CRITICAL_ISR(&mux);
}

// Speed sensor update protected:
if (current_time - last_calc_time >= calc_interval_ms) {
    portENTER_CRITICAL(&mux);
    uint32_t count = pulse_count;
    pulse_count = 0;
    portEXIT_CRITICAL(&mux);
```

**Benefits:**
- ✅ Prevents race conditions between ISR and main task
- ✅ Atomic pulse counter read/reset
- ✅ FreeRTOS-aware critical sections

---

## 4. Data Structures (No Changes Needed)

All data structures from [data_sensor.h](include/data_sensor.h) remain unchanged and compatible:

```cpp
struct MPU6500Data_t {
    xyzFloat_t accel;      // g-force values
    xyzFloat_t gyro;       // °/s values
    float temperature;     // °C
    uint64_t timestamp_us;
};

struct SpeedData_t {
    float speed_kmh;
    float speed_ms;
    uint32_t pulse_count;
    uint64_t last_pulse_us;
    uint32_t timestamp_ms;
};

struct ADS1115Data_t {
    float voltage;
    float current;
    // ... other fields
};
```

---

## 5. Files Modified

| File | Changes |
|------|---------|
| [include/constants.h](include/constants.h) | Updated `CURRENT_SHUNT_RES` & `CURRENT_AMP_GAIN` to JouleMeterTest values |
| [include/sensor_manager.h](include/sensor_manager.h) | Added `initialized` member to `SpeedSensorManager` |
| [src/sensor_manager.cpp](src/sensor_manager.cpp) | Updated MPU init, data reading, speed calc; added critical section support |
| [src/wifi_manager.cpp](src/wifi_manager.cpp) | Updated JSON serialization to use modern ArduinoJson v7 API |

---

## 6. Compilation Results

### Before Fix
```
❌ 11 Compilation Errors
- AFS_2G: undefined
- GFS_250DPS: undefined
- setGyroRange(): non-existent method
- getAccX/Y/Z(): non-existent methods
- getGyroX/Y/Z(): non-existent methods
- enableDoNotDisturb(): non-existent method
- readSensor(): non-existent method
- CURRENT_SHUNT_RES: undefined
- CURRENT_AMP_GAIN: undefined
- initialized member missing
- StaticJsonDocument deprecation warnings
```

### After Fix
```
✅ BUILD SUCCESSFUL

RAM:   [==        ]  15.2% (used 49756 bytes from 327680 bytes)
Flash: [===       ]  32.0% (used 1005889 bytes from 3145728 bytes)

Warnings: 0 (deprecation warnings fixed)
```

---

## 7. Testing Recommendations

### Speed Sensor Testing
1. **Static Test:** No pulses → Speed should be 0 km/h
2. **Pulse Generation:** Use variable speed rotary wheel
   - 1 pulse per 500ms → 0 km/h (baseline)
   - Increase pulse rate → Verify speed increases linearly
3. **Noise Filter:** Check that speed readings are smooth (5-sample averaging)

### IMU Testing
1. **Static Orientation:**
   - Flat horizontal: accel ~[0, 0, 1.0] g, gyro ~[0, 0, 0]
   - Tilted 45°: accel ~[0.7, 0, 0.7] g
2. **Motion Test:** Accelerate vehicle → Accel should show direction
3. **Temperature:** Log temperature values - should be stable room temp

### Current Sensor Testing
1. **No Load:** Current = 0 A (with offset calibration)
2. **Known Load:** Apply 1A → Verify measured = 1.0A ±0.1A
3. **Voltage Divider:** Apply 48V → Should read ~48V

---

## 8. Integration with JouleMeterTest Patterns

✅ **Successfully Integrated:**
- MPU initialization from JouleMeterTest IMUInit()
- Speed calculation using pulse-based method with 500ms window
- Critical section protection pattern
- JSON serialization using modern ArduinoJson v7

✅ **Maintained Compatibility:**
- Thread-safe mutex pattern for global data
- FreeRTOS task scheduling (unchanged)
- WiFi HTTP POST format (unchanged)
- Data structure definitions (unchanged)

---

## 9. Next Steps

1. **Flash to Device:**
   ```
   platformio run -t upload
   ```

2. **Monitor Serial Output:**
   ```
   platformio device monitor -b 115200
   ```

3. **Verify Initialization:**
   - Check MPU6500 initializes correctly
   - Check speed sensor ISR triggers
   - Verify WiFi can send data with new JSON format

4. **Calibration:**
   - Run ADS1115 calibration at startup (already in place)
   - Verify offset values logged

5. **Load Testing:**
   - Apply known currents (1A, 5A, 10A) and verify measurements
   - Log speed data from rotating wheel
   - Check accelerometer readings while moving

---

## Summary

All compilation errors have been fixed by:
1. ✅ Using correct MPU9250_WE library API methods
2. ✅ Updating constants to match hardware (0.001Ω shunt, 20× gain)
3. ✅ Adding missing `initialized` member to SpeedSensorManager
4. ✅ Adding critical section protection for interrupt-safe pulse counting
5. ✅ Updating JSON serialization to modern ArduinoJson v7

**Build Status:** ✅ SUCCESS - Ready for upload and testing!

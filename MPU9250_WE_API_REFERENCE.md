# MPU9250_WE Library API Reference

## Correct API Methods (MPU9250_WE v1.2.17)

### Initialization Methods
```cpp
// CORRECT - Use these methods
mpu6500.init()                           // Initialize I2C communication
mpu6500.autoOffsets()                    // Calibrate accel/gyro offsets
mpu6500.enableGyrDLPF()                  // Enable digital low-pass filter
mpu6500.setGyrDLPF(MPU9250_DLPF_6)       // Set DLPF bandwidth (0-7, lower = less bandwidth)
mpu6500.setSampleRateDivider(5)          // Set sample rate divider (0-255)
mpu6500.setAccRange(MPU9250_ACC_RANGE_16G)  // Set accel range: 2G, 4G, 8G, or 16G
mpu6500.enableAccDLPF(true)              // Enable accel digital low-pass filter
mpu6500.setAccDLPF(MPU9250_DLPF_6)       // Set accel DLPF bandwidth

// Magnetometer (MPU9250 ONLY)
mpu6500.initMagnetometer()               // Initialize AK8963 magnetometer
mpu6500.setMagOpMode(AK8963_CONT_MODE_100HZ)  // Set mag operation mode
```

### WRONG API Methods (DO NOT USE)
```cpp
// ❌ DEPRECATED/NON-EXISTENT - These will cause compilation errors
mpu6500.readSensor()                     // NO - Not available
mpu6500.setAccRange(AFS_2G)              // NO - Wrong constant name
mpu6500.setGyroRange(GFS_250DPS)         // NO - Wrong method name
mpu6500.enableDoNotDisturb()             // NO - Not available
mpu6500.getAccX()                        // NO - Use getGValues() instead
mpu6500.getAccY()                        // NO - Use getGValues() instead
mpu6500.getAccZ()                        // NO - Use getGValues() instead
mpu6500.getGyroX()                       // NO - Use getGyrValues() instead
mpu6500.getGyroY()                       // NO - Use getGyrValues() instead
mpu6500.getGyroZ()                       // NO - Use getGyrValues() instead
```

---

## Data Reading Methods

### CORRECT Ways to Read Sensor Data
```cpp
// Acceleration (in g-force)
xyzFloat gValue = mpu6500.getGValues();
float accel_x_g = gValue.x;
float accel_y_g = gValue.y;
float accel_z_g = gValue.z;
float resultant_g = mpu6500.getResultantG(gValue);

// Gyroscope (in °/s)
xyzFloat gyr = mpu6500.getGyrValues();
float gyro_x_dps = gyr.x;
float gyro_y_dps = gyr.y;
float gyro_z_dps = gyr.z;

// Temperature (in °C)
float temp_c = mpu6500.getTemperature();

// Angles (in degrees) - requires calibration
xyzFloat angles = mpu6500.getAngles();
float roll = angles.x;
float pitch = angles.y;
float yaw = angles.z;

// Magnetometer (MPU9250 only)
xyzFloat magValue = mpu6500.getMagValues();  // in µT
```

---

## Constants Reference

### Accelerometer Range Options
```cpp
MPU9250_ACC_RANGE_2G      // ±2G (default)
MPU9250_ACC_RANGE_4G      // ±4G
MPU9250_ACC_RANGE_8G      // ±8G
MPU9250_ACC_RANGE_16G     // ±16G (recommended for vehicles)
```

### Digital Low-Pass Filter Options
```cpp
// DLPF bandwidth (lower number = lower cutoff frequency)
MPU9250_DLPF_0     // 250Hz bandwidth
MPU9250_DLPF_1     // 184Hz
MPU9250_DLPF_2     // 92Hz
MPU9250_DLPF_3     // 41Hz
MPU9250_DLPF_4     // 20Hz
MPU9250_DLPF_5     // 10Hz
MPU9250_DLPF_6     // 5Hz (✅ Recommended for vehicle - reduces vibration)
MPU9250_DLPF_7     // Hold previous value
```

### Sample Rate Divider
```cpp
// Sample Rate = 1000Hz / (1 + divider)
// divider = 5    → 166.7 Hz (1000 / 6)
// divider = 9    → 100 Hz (1000 / 10)
// divider = 19   → 50 Hz (1000 / 20)
// divider = 99   → 10 Hz (1000 / 100)

// ✅ Recommended for vehicle: 5 (166.7Hz)
mpu6500.setSampleRateDivider(5);
```

### Magnetometer Operation Modes (MPU9250 Only)
```cpp
AK8963_POWER_DOWN              // Magnetometer powered down
AK8963_SINGLE_MEAS             // Single measurement mode
AK8963_CONT_MODE_8HZ           // 8 Hz continuous mode
AK8963_CONT_MODE_100HZ         // 100 Hz continuous mode (✅ Recommended)
AK8963_EXT_TRIG_MEAS           // External trigger measurement
AK8963_SELF_TEST               // Magnetometer self-test
```

---

## Initialization Examples

### Correct Setup (from JouleMeterTest)
```cpp
// For ESP32 with MPU9250
if (myMPU9250.init()) {
    myMPU9250.autoOffsets();              // Calibrate offsets
    myMPU9250.enableGyrDLPF();            // Enable DLPF
    myMPU9250.setGyrDLPF(MPU9250_DLPF_6);   // 5Hz DLPF (reduces vibration)
    myMPU9250.setSampleRateDivider(5);    // 166.7 Hz sample rate
    myMPU9250.setAccRange(MPU9250_ACC_RANGE_16G);  // ±16G range
    myMPU9250.enableAccDLPF(true);
    myMPU9250.setAccDLPF(MPU9250_DLPF_6);
    myMPU9250.setMagOpMode(AK8963_CONT_MODE_100HZ);
    myMPU9250.initMagnetometer();
    myMPU9250.setMagOpMode(AK8963_CONT_MODE_100HZ);
    ESP_LOGI(TAG, "MPU9250 initialized");
} else if (myMPU6500.init()) {
    // Fallback to MPU6500 (no magnetometer)
    myMPU6500.autoOffsets();
    myMPU6500.enableGyrDLPF();
    myMPU6500.setGyrDLPF(MPU9250_DLPF_6);
    myMPU6500.setSampleRateDivider(5);
    myMPU6500.setAccRange(MPU9250_ACC_RANGE_16G);
    myMPU6500.enableAccDLPF(true);
    myMPU6500.setAccDLPF(MPU9250_DLPF_6);
    ESP_LOGI(TAG, "MPU6500 initialized");
}
```

### Reading Data (from JouleMeterTest)
```cpp
if (imuAvailable) {
    if (isMPU9250) {
        xyzFloat gValue = myMPU9250.getGValues();
        xyzFloat gyr = myMPU9250.getGyrValues();
        xyzFloat magValue = myMPU9250.getMagValues();
        xyzFloat angles = myMPU9250.getAngles();
        float temp = myMPU9250.getTemperature();
        float resultantG = myMPU9250.getResultantG(gValue);
    } else if (isMPU6500) {
        xyzFloat gValue = myMPU6500.getGValues();
        xyzFloat gyr = myMPU6500.getGyrValues();
        xyzFloat angles = myMPU6500.getAngles();
        float temp = myMPU6500.getTemperature();
        float resultantG = myMPU6500.getResultantG(gValue);
        // No magnetometer data available
    }
}
```

---

## Data Structure: xyzFloat_t

```cpp
// The library returns xyzFloat structure
struct xyzFloat {
    float x;
    float y;
    float z;
};

// Usage example:
xyzFloat accel = mpu6500.getGValues();
float ax = accel.x;  // Access x component
float ay = accel.y;  // Access y component
float az = accel.z;  // Access z component
```

---

## Common Issues and Solutions

### Issue 1: "identifier 'AFS_2G' is undefined"
**Wrong:**
```cpp
mpu6500.setAccRange(AFS_2G);
```
**Correct:**
```cpp
mpu6500.setAccRange(MPU9250_ACC_RANGE_2G);
```

### Issue 2: "method setGyroRange does not exist"
**Wrong:**
```cpp
mpu6500.setGyroRange(GFS_250DPS);
```
**Correct:**
```cpp
// Gyro range is controlled by DLPF, not set directly
mpu6500.setGyrDLPF(MPU9250_DLPF_6);  // 5Hz bandwidth
```

### Issue 3: "method getAccX does not exist"
**Wrong:**
```cpp
float x = mpu6500.getAccX();
float y = mpu6500.getAccY();
float z = mpu6500.getAccZ();
```
**Correct:**
```cpp
xyzFloat acc = mpu6500.getGValues();
float x = acc.x;
float y = acc.y;
float z = acc.z;
```

### Issue 4: "method enableDoNotDisturb does not exist"
**Wrong:**
```cpp
mpu6500.enableDoNotDisturb();
```
**Correct:**
```cpp
// This method doesn't exist - it was from a different library
// Use autoOffsets() for calibration instead
mpu6500.autoOffsets();
```

---

## Library Version Info

**Used in SMV_DAQ:**
- MPU9250_WE @ 1.2.17 (from PlatformIO)
- Supports: ESP32, Arduino, STM32, and other platforms
- GitHub: https://github.com/wollewald/MPU9250_WE

**Why v1.2.17?**
- ✅ Stable and well-documented
- ✅ Good support for both MPU9250 and MPU6500
- ✅ Low-level I2C control for precise configuration
- ✅ Verified working with JouleMeterTest

---

## Sensor Specifications

### MPU6500 (6-axis)
- Accelerometer: ±2/4/8/16g
- Gyroscope: ±250/500/1000/2000°/s
- Temperature sensor: -40 to +85°C
- Sample rate: 8kHz (internal), configurable via divider
- Digital filters: Programmable DLPF

### MPU9250 (9-axis, includes magnetometer)
- All of above, PLUS:
- Magnetometer (AK8963): ±4800µT, 100Hz max

---

## Recommendations for Vehicle DAQ

### Accelerometer Configuration
```cpp
mpu6500.setAccRange(MPU9250_ACC_RANGE_16G);  // Captures wide range of vehicle motion
mpu6500.enableAccDLPF(true);
mpu6500.setAccDLPF(MPU9250_DLPF_6);  // 5Hz cutoff - reduces vibration noise
```

### Gyroscope Configuration
```cpp
mpu6500.enableGyrDLPF();
mpu6500.setGyrDLPF(MPU9250_DLPF_6);  // 5Hz cutoff - smooth angular rate
mpu6500.setSampleRateDivider(5);     // 166.7 Hz sample rate
```

### Sample Rate
- Internal: 1000 Hz (before divider)
- With divider 5: 166.7 Hz (good balance)
- Update interval: ~6 ms between samples
- Suitable for vehicle motion capture

### Filtering
- DLPF @ 5Hz removes road vibration (typical range 8-20Hz)
- Still captures vehicle dynamics (< 5Hz movements)
- No additional external filtering needed


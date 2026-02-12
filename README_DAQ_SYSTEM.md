# SMV Data Acquisition Board (DAQ)

A comprehensive data acquisition system for ESP32 with multiple sensors, CAN bus communication, GPS integration, and WiFi connectivity.

## System Overview

### Architecture
- **Microcontroller**: ESP32 (16MB Flash)
- **OS**: FreeRTOS with dual-core support (App CPU for real-time tasks, Pro CPU for WiFi)
- **Thread Model**: Task-based with mutex protection for data access

### Core Features

#### 1. **Sensor Management**
- **MPU6500/MPU9250**: 6-axis IMU (Accelerometer + Gyroscope)
  - I2C Communication @ 400 kHz
  - Sample rate: 100 Hz
  - Data fields: Acceleration (m/s²), Angular velocity (°/s), Temperature (°C)

- **ADS1115**: 16-bit ADC
  - Input 1 (AIN0): Voltage measurement with voltage divider (10kΩ:10kΩ)
  - Input 2 (AIN1): Current measurement with AD8418 shunt amplifier (100V/V gain)
  - Resolution: 187.5 µV/LSB

- **Speed Sensor**: Hall effect / Pulse counter
  - GPIO27 with rising edge interrupt
  - Calculates wheel speed from pulse frequency
  - Includes 5-sample moving average filter

#### 2. **GPS Module (UART)**
- **Interface**: UART1 @ 9600 baud
- **RX Pin**: GPIO26
- **Parser**: NMEA sentence parsing
  - Supports GPRMC (Time, Speed, Course)
  - Supports GPGGA (Position, Altitude, Satellites)
- **Data Fields**: Latitude, Longitude, Altitude, Speed (knots/km/h), Satellite count, Fix quality
- **Thread Safety**: FreeRTOS task with mutex protection

#### 3. **CAN Bus (TWAI - Two-Wire Automotive Interface)**
- **Baud Rate**: 500 kbps
- **TX Pin**: GPIO4
- **RX Pin**: GPIO3
- **Tasks**:
  - `canRxTask`: Receives messages (high priority)
  - `canTxTask`: Transmits sensor data periodically
- **Message IDs**:
  - 0x100: Sensor data (MPU6500)
  - 0x101: GPS data
  - 0x102: System status
  - 0x103: Error codes

#### 4. **WiFi Communication**
- **Mode**: Station (STA)
- **HTTP POST**: Sends JSON-formatted sensor data
- **Frequency**: Every 5 seconds (configurable)
- **Server Integration**: Update `WIFI_SSID`, `WIFI_PASSWORD`, and HTTP endpoint

#### 5. **Data Structure (Thread-Safe)**
```c
typedef struct {
    MPU6500Data_t mpu6500;      // IMU data
    ADS1115Data_t ads1115;      // Voltage & Current
    GPSData_t gps;              // Position & Speed
    SpeedData_t speed;          // Wheel speed
    CANData_t can_rx;           // Last CAN message
    SystemStatus_t status;      // System health
    struct { ... } flags;       // Data validity flags
} DataSensor_t;
```

## Task Architecture

### FreeRTOS Task Layout

| Task | Priority | CPU | Frequency | Purpose |
|------|----------|-----|-----------|---------|
| `sensorReadTask` | 3 | App | 100 ms | Read MPU6500, ADS1115, Speed sensor |
| `canRxTask` | 3 | App | 10 ms | Receive CAN messages |
| `canTxTask` | 2 | App | 100 ms | Transmit sensor data via CAN |
| `gpsTask` | 2 | App | 50 ms | Parse GPS UART data |
| `wifiTask` | 2 | Pro | 1000 ms | WiFi management & HTTP POST |
| `loopTask` | 1 | Pro | 100 ms | Arduino loop() |

**Key Design Principles:**
- Real-time sensor tasks on **App CPU** (no WiFi interference)
- WiFi/HTTP tasks on **Pro CPU** (lower priority)
- Mutex-protected access to shared data structure
- Task priorities: Sensor reading (3) > CAN (3) > GPS/WiFi (2)

## Partition Configuration

ESP32 16MB Flash layout:
```
NVS             0x9000   - 20 KB   (Non-volatile storage)
OTA Data        0xE000   - 8 KB    (OTA metadata)
App0 (OTA 0)    0x10000  - 4 MB    (Main application)
App1 (OTA 1)    0x410000 - 4 MB    (OTA backup)
SPIFFS          0x810000 - 7.9 MB  (File system)
```

This enables OTA (Over-The-Air) updates for firmware upgrades.

## Pin Configuration Summary

```
I2C:
  SDA = GPIO21
  SCL = GPIO22

UART (GPS):
  RX = GPIO26
  TX = GPIO_NO_CHANGE (GPS RX only)

Speed Sensor:
  Pin = GPIO27 (Rising edge interrupt)

CAN Bus:
  TX = GPIO4
  RX = GPIO3

LED (Status):
  Pin = GPIO2
```

## Implementation Recommendations

### 1. **Data Logging to SPIFFS**
Add SPIFFS file system support for local data logging:
```cpp
void logDataToSDCard() {
    FILE* file = fopen("/spiffs/data.csv", "a");
    fprintf(file, "%ld,%f,%f,%f\n", millis(), 
            g_data_sensor.mpu6500.accel.x,
            g_data_sensor.mpu6500.accel.y,
            g_data_sensor.mpu6500.accel.z);
    fclose(file);
}
```

### 2. **Kalman Filter for Position Estimation**
Combine GPS and accelerometer data for better position accuracy:
- Use accelerometer for high-frequency updates (100 Hz)
- Use GPS for low-frequency absolute position (1-5 Hz)

### 3. **Battery Monitoring**
Measure battery voltage via AIN0 voltage divider:
```cpp
#define BAT_MIN_VOLTAGE 2.5f
#define BAT_MAX_VOLTAGE 4.2f
uint8_t battery_percent = BATTERY_PERCENT_FROM_VOLTAGE(
    g_data_sensor.ads1115.voltage, BAT_MIN_VOLTAGE, BAT_MAX_VOLTAGE);
```

### 4. **Error Handling & Watchdog**
- Add TWDT (Task Watchdog Timer) to detect deadlocks
- Implement error counting and error CAN messages
- Auto-restart on critical failures

### 5. **Configuration via NVS**
Store calibration data and settings in NVS:
- Voltage divider calibration
- Shunt resistance compensation
- GPS antenna offset

### 6. **Web Dashboard**
Create a Node.js/Python backend to:
- Receive HTTP POST data from ESP32
- Store in database (InfluxDB, MongoDB)
- Display real-time dashboard (Grafana, web UI)
- Enable OTA firmware updates

### 7. **CAN Message Structure**
Example CAN message format:
```
ID: 0x100 (MPU6500 Data)
Byte 0-1: Accel X (int16, units: 0.01m/s²)
Byte 2-3: Accel Y (int16, units: 0.01m/s²)
Byte 4-5: Accel Z (int16, units: 0.01m/s²)
Byte 6-7: Temperature (int16, units: 0.01°C)
```

### 8. **Calibration Procedures**
1. **Accelerometer**: 6-point calibration (place on each side)
2. **Gyroscope**: Static bias calibration (keep stationary for 5 seconds)
3. **Voltage divider**: Measure known voltages, store calibration factors
4. **Current shunt**: Verify with multimeter

### 9. **Performance Optimization**
- Use DMA for I2C/UART to reduce CPU load
- Implement data buffering for batch HTTP uploads
- Reduce sensor sample rate during low-power mode
- Use event-driven GPS parsing instead of polling

### 10. **Testing & Debugging**
```bash
# Monitor serial output
picocom /dev/ttyUSB0 -b 115200

# Monitor specific tag
ESP_LOGD(TAG_SYSTEM, "Debug message: %d", value);

# Check heap fragmentation
ESP_LOGI(TAG_SYSTEM, "Heap: %d / %d bytes", 
    esp_get_free_heap_size(), 
    esp_get_heap_size());
```

## Configuration Steps

### 1. **platformio.ini**
Update WiFi credentials in [constants.h](include/constants.h):
```cpp
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
```

### 2. **HTTP Server Endpoint**
Update WiFi manager HTTP POST URL:
```cpp
String url = "http://YOUR_SERVER:8080/api/sensor/data";
```

### 3. **Calibration Constants**
Adjust in [constants.h](include/constants.h):
```cpp
#define VDIV_R1 10000.0f  // Your resistor values
#define VDIV_R2 10000.0f
#define SHUNT_RESISTANCE 0.01f  // Your shunt value in Ohms
#define WHEEL_CIRCUMFERENCE_MM 2200.0f  // Your wheel circumference
```

## Building & Uploading

```bash
# Build project
pio run -e esp32doit-devkit-v1

# Upload to ESP32
pio run -e esp32doit-devkit-v1 -t upload

# Monitor serial
pio device monitor -e esp32doit-devkit-v1 -b 115200
```

## Troubleshooting

### WiFi Connection Issues
- Check SSID and password in constants.h
- Verify WiFi is 2.4 GHz (not 5 GHz)
- Check router security settings

### GPS No Fix
- Wait 30-60 seconds for initial lock
- Ensure GPS antenna is outdoors with clear sky view
- Check UART baud rate (default 9600)

### I2C Device Not Found
- Verify pull-up resistors on SDA/SCL (typically 4.7kΩ)
- Check for address conflicts with `i2cdetect`
- Verify Wire.begin() called with correct pins

### CAN Bus No Communication
- Check termination resistors on CAN bus (120Ω at each end)
- Verify TX and RX pins not swapped
- Check baud rate matches other CAN nodes

## References

- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [MPU9250_WE Library](https://github.com/wollewald/MPU9250_WE)
- [Adafruit ADS1X15 Library](https://github.com/adafruit/Adafruit_ADS1X15)
- [FreeRTOS ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)

## License

MIT License - Feel free to modify and use for your projects.

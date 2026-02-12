# Quick Reference Guide

## Files at a Glance

### Core Implementation Files
- **[src/main.cpp](src/main.cpp)** - System initialization, task creation, main loop
- **[src/sensor_manager.cpp](src/sensor_manager.cpp)** - MPU6500, ADS1115, Speed sensor
- **[src/gps_manager.cpp](src/gps_manager.cpp)** - GPS UART driver & NMEA parser
- **[src/can_manager.cpp](src/can_manager.cpp)** - CAN bus RX/TX tasks
- **[src/wifi_manager.cpp](src/wifi_manager.cpp)** - WiFi & HTTP POST

### Header Files (Interfaces)
- **[include/constants.h](include/constants.h)** - All constants, GPIO pins, equations
- **[include/data_sensor.h](include/data_sensor.h)** - Central DataSensor_t structure
- **[include/sensor_manager.h](include/sensor_manager.h)** - Sensor class interfaces
- **[include/gps_manager.h](include/gps_manager.h)** - GPS interface
- **[include/can_manager.h](include/can_manager.h)** - CAN interface
- **[include/wifi_manager.h](include/wifi_manager.h)** - WiFi interface
- **[include/utils.h](include/utils.h)** - Filters, math, utilities

### Documentation
- **[README_DAQ_SYSTEM.md](README_DAQ_SYSTEM.md)** - Architecture & recommendations
- **[SETUP_AND_TESTING.md](SETUP_AND_TESTING.md)** - Installation & testing
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System diagrams & data flows
- **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - Complete overview
- **[partitions.csv](partitions.csv)** - ESP32 flash partition table
- **[platformio.ini](platformio.ini)** - Build configuration

### Examples
- **[include/examples/data_logger.h](include/examples/data_logger.h)** - SPIFFS logging
- **[include/examples/kalman_filter.h](include/examples/kalman_filter.h)** - IMU filtering
- **[include/examples/ota_update.h](include/examples/ota_update.h)** - OTA updates

---

## Common Tasks

### Add a New Sensor
1. Create `struct NewSensorData_t` in [data_sensor.h](include/data_sensor.h)
2. Add to `DataSensor_t` structure
3. Create manager class in [sensor_manager.h](include/sensor_manager.h)
4. Implement in [sensor_manager.cpp](src/sensor_manager.cpp)
5. Create FreeRTOS task in [main.cpp](src/main.cpp)

### Change GPIO Pin
Edit [constants.h](include/constants.h):
```cpp
#define GPIO_NAME GPIO_NUM_XX
```

### Modify CAN Message Format
Edit in [can_manager.cpp](src/can_manager.cpp):
```cpp
// In canTxTask():
uint8_t can_data[8] = { ... };
g_can_manager.sendMessage(CAN_ID, can_data, dlc);
```

### Change WiFi Frequency
Edit [constants.h](include/constants.h):
```cpp
#define HTTP_POST_INTERVAL_MS 5000  // Change to desired interval
```

### Add Data Logging
Include [include/examples/data_logger.h](include/examples/data_logger.h) and use in your task

### Enable Kalman Filter
Include [include/examples/kalman_filter.h](include/examples/kalman_filter.h) in sensor_manager

---

## GPIO Quick Map

| Function | Pin | Notes |
|----------|-----|-------|
| I2C SDA | GPIO21 | Pull-up 4.7k |
| I2C SCL | GPIO22 | Pull-up 4.7k |
| GPS RX | GPIO26 | UART1 RX |
| Speed Sensor | GPIO27 | Rising edge ISR |
| CAN TX | GPIO4 | TWAI TX |
| CAN RX | GPIO3 | TWAI RX |
| Status LED | GPIO2 | Blink indicator |

---

## I2C Addresses

| Device | Address | Bus |
|--------|---------|-----|
| MPU6500 | 0x68 | I2C (GPIO21/22) |
| ADS1115 | 0x48 | I2C (GPIO21/22) |

---

## Data Structure Quick Access

```cpp
// Reading sensor data (thread-safe):
if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(100))) {
    float accel_x = g_data_sensor.mpu6500.accel.x;
    float voltage = g_data_sensor.ads1115.voltage;
    float latitude = g_data_sensor.gps.latitude;
    float speed_kmh = g_data_sensor.speed.speed_kmh;
    
    // Check data validity
    if (g_data_sensor.flags.mpu6500_valid) {
        // Use data
    }
    
    xSemaphoreGive(g_data_sensor_mutex);
}
```

---

## Useful Commands

```bash
# Build
pio run -e esp32doit-devkit-v1

# Upload
pio run -e esp32doit-devkit-v1 -t upload

# Monitor serial
pio device monitor -e esp32doit-devkit-v1 -b 115200

# Clean and rebuild
pio run -e esp32doit-devkit-v1 -t clean
pio run -e esp32doit-devkit-v1

# Upload specific file
pio run -e esp32doit-devkit-v1 -t upload --upload-port /dev/ttyUSB0

# OTA upload (after initial setup)
pio run -e esp32doit-devkit-v1 -t upload --upload-port 192.168.1.100
```

---

## Task Priorities

```
Priority 3: sensorReadTask, canRxTask
            (Real-time, tight timing)
            
Priority 2: gpsTask, canTxTask, wifiTask
            (Regular operations)
            
Priority 1: loopTask
            (Monitoring & logging)
            
FreeRTOS runs highest priority ready task first
```

---

## Important Equations

```cpp
// Voltage from ADC
V_actual = V_adc * (R1 + R2) / R2

// Current from shunt
I = V_shunt / (R_shunt * Gain)

// Speed from frequency
Speed_kmh = Freq_Hz * Circumference_mm / 1000000

// GPS distance (simplified)
Distance_m = Haversine(lat1, lon1, lat2, lon2)

// Battery percent
Battery% = (Voltage - Min) / (Max - Min) * 100
```

---

## Debug Logging Levels

```cpp
ESP_LOGE(TAG, "Error message");      // Errors only
ESP_LOGW(TAG, "Warning message");    // Warnings
ESP_LOGI(TAG, "Info message");       // Information
ESP_LOGD(TAG, "Debug message");      // Detailed debug
ESP_LOGV(TAG, "Verbose message");    // Very detailed

// Tags defined in constants.h
#define TAG_SYSTEM "DAQ_SYSTEM"
#define TAG_SENSOR "SENSOR_MGR"
#define TAG_GPS "GPS_DRIVER"
#define TAG_CAN "CAN_BUS"
#define TAG_WIFI "WIFI_MGR"
#define TAG_HTTP "HTTP_POST"
#define TAG_ERROR "ERROR_HDL"
```

---

## Configuration Checklist

- [ ] Update `WIFI_SSID` and `WIFI_PASSWORD` in constants.h
- [ ] Update HTTP server URL in wifi_manager.cpp
- [ ] Measure and update `VDIV_R1`, `VDIV_R2` in constants.h
- [ ] Measure and update `WHEEL_CIRCUMFERENCE_MM` in constants.h
- [ ] Verify `SHUNT_RESISTANCE` and `AD8418_GAIN` values
- [ ] Check all GPIO pin assignments
- [ ] Verify I2C device addresses (0x68, 0x48)
- [ ] Test each sensor individually
- [ ] Verify WiFi connectivity
- [ ] Confirm CAN bus operation

---

## Performance Tips

1. **Reduce WiFi Frequency** - Increase `HTTP_POST_INTERVAL_MS` to save power
2. **Filter Data** - Use MovingAverageFilter in utils.h for noisy sensors
3. **Batch Updates** - Send multiple samples per HTTP request
4. **Cache Calculations** - Pre-compute constants during init
5. **Use Local Buffers** - Avoid repeated malloc/free in loops
6. **Monitor Heap** - Check `esp_get_free_heap_size()` regularly

---

## Typical Serial Output

```
I (123) DAQ_SYSTEM: ===========================================
I (124) DAQ_SYSTEM: SMV Data Acquisition Board
I (125) DAQ_SYSTEM: Initializing...
I (200) SENSOR_MGR: MPU6500 initialized successfully
I (300) SENSOR_MGR: ADS1115 initialized successfully
I (400) GPS_DRIVER: GPS UART initialized successfully
I (500) CAN_BUS: CAN bus initialized successfully at 500kbps
I (600) WIFI_MGR: WiFi manager initialized
I (2000) WIFI_MGR: Connecting to WiFi: YOUR_SSID
I (5000) WIFI_MGR: WiFi connected! IP: 192.168.1.100

I (5000) DAQ_SYSTEM: --- System Status ---
I (5000) DAQ_SYSTEM: Uptime: 5000 ms
I (5000) DAQ_SYSTEM: Free heap: 65536 bytes
I (5000) DAQ_SYSTEM: MPU6500: Accel(0.05, -0.02, 9.81) m/s², Temp: 28.5°C
I (5000) DAQ_SYSTEM: ADS1115: Voltage: 12.50V, Current: 2.345A
I (5000) DAQ_SYSTEM: Speed: 25.20 km/h, Pulses: 234
I (5000) DAQ_SYSTEM: GPS: Lat: -6.212345, Lon: 106.845123, Sats: 12, Speed: 25.3 km/h
I (5000) DAQ_SYSTEM: WiFi: Connected, CAN: Active
```

---

## Troubleshooting Quick Links

| Problem | Solution |
|---------|----------|
| I2C device not found | See SETUP_AND_TESTING.md → I2C Devices Not Detected |
| GPS no fix | See SETUP_AND_TESTING.md → GPS No Fix |
| CAN not working | See SETUP_AND_TESTING.md → CAN Bus Not Working |
| WiFi fails | See SETUP_AND_TESTING.md → WiFi Connection Fails |
| Compile errors | Check library versions in platformio.ini |
| Memory issues | Monitor heap with esp_get_free_heap_size() |
| Sensor noise | Apply filter from include/examples/kalman_filter.h |

---

## Next Steps After Build

1. Flash initial firmware
2. Monitor serial output (verify initialization)
3. Test each sensor individually
4. Verify WiFi connectivity
5. Test CAN bus communication
6. Integrate with backend server
7. Enable data logging (optional)
8. Deploy OTA update system (optional)

---

**Need Help?**
- Read [SETUP_AND_TESTING.md](SETUP_AND_TESTING.md) for detailed troubleshooting
- Check [ARCHITECTURE.md](ARCHITECTURE.md) for system diagrams
- See [README_DAQ_SYSTEM.md](README_DAQ_SYSTEM.md) for implementation details


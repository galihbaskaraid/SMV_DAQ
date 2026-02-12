# SMV DAQ System - Setup & Testing Guide

## Quick Start

### 1. Initial Setup

```bash
# Clone or navigate to project
cd c:\Users\galih\OneDrive\Documents\PlatformIO\Projects\SMV_DAQ

# Build the project
pio run -e esp32doit-devkit-v1

# Upload to ESP32
pio run -e esp32doit-devkit-v1 -t upload

# Monitor serial output
pio device monitor -e esp32doit-devkit-v1 -b 115200
```

### 2. Configuration

**Edit [constants.h](include/constants.h) with your settings:**

```cpp
// WiFi
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// Wheel circumference (measure your actual wheel)
#define WHEEL_CIRCUMFERENCE_MM 2200.0f

// Voltage divider resistors
#define VDIV_R1 10000.0f
#define VDIV_R2 10000.0f

// Shunt resistor value
#define SHUNT_RESISTANCE 0.01f
```

### 3. Hardware Connections

```
ESP32 Pin Layout:
┌─────────────────────────────────┐
│  MPU6500/ADS1115 (I2C)          │
│  SDA = GPIO21                    │
│  SCL = GPIO22                    │
│  GND                             │
│  3.3V                            │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│  GPS Module (UART1)             │
│  RX = GPIO26                     │
│  TX = (Not used, GPS RX only)   │
│  GND                             │
│  5V (from power source)          │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│  Speed Sensor (GPIO27)          │
│  Signal = GPIO27                 │
│  GND                             │
│  3.3V (pull-up enabled)          │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│  CAN Transceiver (TWAI)         │
│  TX = GPIO4                      │
│  RX = GPIO3                      │
│  GND                             │
│  5V                              │
│  120Ω termination resistors      │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│  LED Status (GPIO2)             │
│  Anode = GPIO2 (via resistor)   │
│  Cathode = GND                   │
└─────────────────────────────────┘
```

## Testing Procedures

### Test 1: Serial Output & Boot

**Expected Output:**
```
I (123) DAQ_SYSTEM: ============================================
I (124) DAQ_SYSTEM: SMV Data Acquisition Board
I (125) DAQ_SYSTEM: Initializing...
I (126) DAQ_SYSTEM: ============================================
I (200) DAQ_SYSTEM: === Initializing DAQ System ===
I (300) SENSOR_MGR: MPU6500 initialized successfully
I (400) SENSOR_MGR: ADS1115 initialized successfully
I (500) SENSOR_MGR: Speed sensor initialized on pin 27
I (600) GPS_DRIVER: GPS UART initialized successfully
I (700) CAN_BUS: CAN bus initialized successfully at 500kbps
I (800) WIFI_MGR: WiFi manager initialized
```

### Test 2: Sensor Verification

#### MPU6500 (IMU)
```
Expected values:
- Acceleration: ±0.05 to ±0.1 m/s² (at rest)
- Gyroscope: ±10 to ±100 °/s (at rest)
- Temperature: Ambient temperature ±5°C
```

**Test:** Tilt the board and observe accelerometer and gyroscope changes:
```
I (5000) DAQ_SYSTEM: MPU6500: Accel(-0.05, -0.02, 9.81) m/s², Temp: 28.5°C
I (5100) DAQ_SYSTEM: MPU6500: Accel(0.25, 0.15, 9.52) m/s², Temp: 28.6°C  // Tilted
```

#### ADS1115 (Voltage & Current)
```
Expected values:
- Voltage: 0.0V to ~13V (depending on divider)
- Current: -32A to +32A (depending on shunt orientation)
```

**Test:** Apply known voltages and currents:
```
I (5000) DAQ_SYSTEM: ADS1115: Voltage: 5.05V, Current: 0.123A
```

**Debugging Voltage Measurement:**
```cpp
// In terminal, manually test ADS1115
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;
ads.begin(0x48);
int16_t adc0 = ads.readADC_SingleEnded(0);
float voltage = ads.computeVolts(adc0);
```

#### Speed Sensor
```
Expected values:
- Pulse count: Increases with wheel rotation
- Speed: km/h calculated from frequency
```

**Test:** Spin wheel manually:
```
I (5000) DAQ_SYSTEM: Speed: 15.25 km/h, Pulses: 1234
I (5100) DAQ_SYSTEM: Speed: 23.50 km/h, Pulses: 1289  // Faster rotation
I (5200) DAQ_SYSTEM: Speed: 0.00 km/h, Pulses: 1289   // Stopped
```

**Debugging pulse counter:**
```cpp
// Check raw pulse count
ESP_LOGI(TAG_SYSTEM, "Pulse count: %ld", g_data_sensor.speed.pulse_count);
```

#### GPS Module
```
Expected values:
- Fix quality: 0 (no fix) to 2 (DGPS fix)
- Satellites: 0-12 (indoor) to 15+ (outdoor with sky view)
- Speed: 0.0-100+ knots
```

**Test:** Outdoor with clear sky view:
```
I (10000) DAQ_SYSTEM: GPS: Lat: -6.212345, Lon: 106.845123, Sats: 12, Speed: 0.0 km/h
I (15000) DAQ_SYSTEM: GPS: Lat: -6.212340, Lon: 106.845120, Sats: 12, Speed: 25.3 km/h
```

**Debugging GPS UART:**
```cpp
// Check raw GPS data
uart_get_buffered_data_len(UART_GPS, &bytes_available);
if (bytes_available > 0) {
    uint8_t buffer[256];
    uart_read_bytes(UART_GPS, buffer, bytes_available, pdMS_TO_TICKS(10));
    ESP_LOGI(TAG_GPS, "Raw: %s", (char*)buffer);
}
```

### Test 3: CAN Bus Communication

**Expected:**
- CAN messages sent and received at 500 kbps
- Message IDs: 0x100, 0x101, 0x102, 0x103

```
D (1000) CAN_BUS: CAN RX: ID=0x100, DLC=4  // MPU6500 data
D (1000) CAN_BUS: CAN RX: ID=0x101, DLC=2  // GPS data
```

**Testing with external CAN analyzer:**
```bash
# On Linux with can-utils
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# Monitor CAN messages
candump can0

# Send test message
cansend can0 100#0102030405060708
```

### Test 4: WiFi Connectivity

**Expected:**
- Connects to WiFi within 5 seconds
- Shows IP address in logs

```
I (20000) WIFI_MGR: Connecting to WiFi: YOUR_SSID
I (22000) WIFI_MGR: WiFi connected! IP: 192.168.1.100
I (25000) HTTP_POST: HTTP Response: 200
I (25000) HTTP_POST: Data sent successfully
```

**Debugging WiFi:**
```cpp
ESP_LOGI(TAG_WIFI, "WiFi Status: %d", WiFi.status());
// 0 = WL_IDLE_STATUS
// 1 = WL_NO_SSID_AVAIL
// 2 = WL_SCAN_COMPLETED
// 3 = WL_CONNECTED
// 4 = WL_CONNECT_FAILED
// 5 = WL_CONNECTION_LOST
// 6 = WL_DISCONNECTED

ESP_LOGI(TAG_WIFI, "RSSI: %d dBm", WiFi.RSSI());
```

## Troubleshooting

### I2C Devices Not Detected

**Problem:** MPU6500/ADS1115 not initializing

**Solutions:**
1. **Check connections:**
   ```
   SDA (GPIO21) ← MPU/ADS SDA
   SCL (GPIO22) ← MPU/ADS SCL
   GND ← MPU/ADS GND
   3.3V ← MPU/ADS 3.3V
   ```

2. **Check pull-up resistors:**
   - SDA and SCL should have ~4.7kΩ pull-ups to 3.3V
   
3. **Scan I2C addresses:**
   ```cpp
   for (int addr = 0x00; addr < 0x7F; addr++) {
       Wire.beginTransmission(addr);
       if (Wire.endTransmission() == 0) {
           ESP_LOGI(TAG_SYSTEM, "I2C device found at 0x%02X", addr);
       }
   }
   ```

### GPS No Fix

**Problem:** GPS stuck at `fix_quality = 0`

**Solutions:**
1. **Move outdoors** - Need clear view of sky
2. **Wait 60+ seconds** - First fix takes time (Cold Start)
3. **Check UART:**
   ```
   - Baud rate is 9600 (default)
   - RX connected to GPIO26
   - Verify GPS module power supply
   ```

4. **Monitor UART data:**
   ```cpp
   void debugGPS() {
       size_t bytes;
       uart_get_buffered_data_len(UART_GPS, &bytes);
       if (bytes > 0) {
           uint8_t buf[256];
           uart_read_bytes(UART_GPS, buf, bytes, pdMS_TO_TICKS(10));
           ESP_LOGI(TAG_GPS, "GPS Data: %s", (char*)buf);
       }
   }
   ```

### CAN Bus Not Working

**Problem:** No CAN messages received

**Solutions:**
1. **Check termination:** 120Ω resistor at both ends of CAN bus
2. **Check connections:**
   ```
   CAN_TX (GPIO4) → CAN Transceiver TX
   CAN_RX (GPIO3) → CAN Transceiver RX
   GND → CAN Transceiver GND
   5V → CAN Transceiver Vcc
   ```

3. **Verify baud rate:** Must be 500 kbps on all nodes
4. **Test with multimeter:** Measure CAN_H and CAN_L differential voltage

### WiFi Connection Fails

**Problem:** WiFi not connecting after 5 seconds

**Solutions:**
1. **Verify credentials:**
   ```cpp
   #define WIFI_SSID "Your_Network"
   #define WIFI_PASSWORD "Your_Password"
   ```

2. **Check 2.4GHz:** ESP32 only supports 2.4GHz WiFi
3. **Check signal strength:**
   ```cpp
   if (WiFi.begin() == WL_CONNECTED) {
       ESP_LOGI(TAG_WIFI, "RSSI: %d dBm", WiFi.RSSI());
       // -50 dBm = Excellent
       // -60 dBm = Good
       // -70 dBm = Fair
       // < -80 dBm = Weak
   }
   ```

## Performance Monitoring

### Task Execution Time

```cpp
// In each task
uint32_t start = micros();
// ... task work ...
uint32_t elapsed = micros() - start;
ESP_LOGD(TAG_SYSTEM, "Task execution: %u µs", elapsed);
```

### Heap Memory Usage

```cpp
// Monitor heap fragmentation
uint32_t free_heap = esp_get_free_heap_size();
uint32_t max_alloc = esp_get_largest_free_block();
ESP_LOGI(TAG_SYSTEM, "Heap: %u / %u bytes (Largest: %u)", 
    free_heap, esp_get_heap_size(), max_alloc);
```

### Task Stack Usage

```cpp
UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(g_sensor_task_handle);
ESP_LOGI(TAG_SYSTEM, "Sensor task stack: %u bytes remaining", stack_remaining * 4);
```

## Data Format Reference

### JSON HTTP POST Format

```json
{
  "timestamp_ms": 1234567890,
  "unix_time": 1704067200,
  "mpu6500": {
    "accel_x": 0.05,
    "accel_y": -0.02,
    "accel_z": 9.81,
    "gyro_x": 0.5,
    "gyro_y": -0.3,
    "gyro_z": 0.1,
    "temperature": 28.5
  },
  "ads1115": {
    "voltage_v": 12.50,
    "current_a": 2.345
  },
  "gps": {
    "latitude": -6.212345,
    "longitude": 106.845123,
    "altitude": 45.6,
    "speed_kmh": 25.3,
    "satellites": 12,
    "fix_quality": 1,
    "utc_time": "123456.00",
    "date": "090224"
  },
  "speed": {
    "speed_kmh": 25.3,
    "speed_ms": 7.03,
    "pulse_count": 1234
  },
  "status": {
    "wifi_connected": true,
    "gps_locked": true,
    "heap_free": 65536,
    "uptime_ms": 1234567
  }
}
```

### CAN Message Format

```
Message ID 0x100 (MPU6500 Data):
Byte 0-1: Accel X (int16, 0.01 m/s²)
Byte 2-3: Accel Y (int16, 0.01 m/s²)
Byte 4-5: Accel Z (int16, 0.01 m/s²)
Byte 6-7: Temperature (int16, 0.01°C)

Message ID 0x101 (GPS Data):
Byte 0-1: Speed (int16, 0.1 km/h)
Byte 2: Satellites (uint8)
Byte 3: Fix Quality (uint8)

Message ID 0x102 (Status):
Byte 0: Status flags (bits)
Byte 1-4: Uptime (uint32, milliseconds)
Byte 5-6: Free heap (uint16, kb)
```

## Next Steps

1. **Calibrate sensors** using [config_template.h](include/config_template.h)
2. **Enable data logging** using [examples/data_logger.h](include/examples/data_logger.h)
3. **Implement Kalman filter** using [examples/kalman_filter.h](include/examples/kalman_filter.h)
4. **Set up OTA updates** using [examples/ota_update.h](include/examples/ota_update.h)
5. **Create backend server** for data visualization (Node.js/Python)


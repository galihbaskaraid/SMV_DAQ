# SMV DAQ System Architecture

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          ESP32 (Dual Core)                                  │
│  ┌──────────────────────────────────┬─────────────────────────────────────┐ │
│  │     APP CPU (Core 1)             │     PRO CPU (Core 0)               │ │
│  │ (Real-time Sensors)              │ (WiFi/Networking)                 │ │
│  │                                  │                                    │ │
│  │ ┌─ sensorReadTask (P:3)         │ ┌─ wifiTask (P:2)                │ │
│  │ │  ├─ I2C Read MPU6500          │ │  ├─ WiFi Connect              │ │
│  │ │  ├─ I2C Read ADS1115          │ │  ├─ HTTP POST JSON            │ │
│  │ │  └─ Update Speed              │ │  └─ Status Update             │ │
│  │ │                               │ │                               │ │
│  │ ├─ gpsTask (P:2)               │ ├─ loopTask (P:1)               │ │
│  │ │  └─ UART Parse GPS            │ │  ├─ Monitor Heap             │ │
│  │ │                               │ │  ├─ Blink LED                │ │
│  │ ├─ canRxTask (P:3)             │ │  └─ Log Status               │ │
│  │ │  └─ TWAI Receive              │ │                               │ │
│  │ │                               │ └───────────────────────────────┘ │
│  │ └─ canTxTask (P:2)             │                                    │
│  │    └─ TWAI Transmit             │                                    │
│  │                                  │                                    │
│  └──────────────────────────────────┴─────────────────────────────────┘ │
│                                                                           │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │           GLOBAL DATA STRUCTURE (Thread-Safe)                    │  │
│  │                                                                    │  │
│  │  DataSensor_t g_data_sensor                                      │  │
│  │  ├─ mpu6500_data     (protected by mutex)                       │  │
│  │  ├─ ads1115_data     (protected by mutex)                       │  │
│  │  ├─ gps_data         (protected by mutex)                       │  │
│  │  ├─ speed_data       (protected by mutex)                       │  │
│  │  ├─ can_rx_data      (protected by mutex)                       │  │
│  │  ├─ system_status    (protected by mutex)                       │  │
│  │  └─ validity_flags   (protected by mutex)                       │  │
│  │                                                                    │  │
│  │  g_data_sensor_mutex (FreeRTOS Binary Semaphore)                │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────────┘
        │                    │                    │                    │
        ▼                    ▼                    ▼                    ▼
   ┌─────────┐          ┌─────────┐         ┌──────────┐         ┌──────────┐
   │   I2C   │          │  UART   │         │   GPIO   │         │   CAN    │
   │  BUS    │          │   GPS   │         │  Speed   │         │  TWAI    │
   │GPIO21/22│          │GPIO26   │         │GPIO27    │         │GPIO3/4   │
   │ 400kHz  │          │9600baud │         │Interrupt │         │500kbps   │
   └────┬────┘          └────┬────┘         └────┬─────┘         └────┬─────┘
        │                    │                   │                     │
        ▼                    ▼                   ▼                     ▼
   ┌─────────────────┐  ┌──────────┐    ┌────────────┐    ┌──────────────────┐
   │ Sensor Devices  │  │   GPS    │    │Speed Sensor│    │  CAN Transceiver │
   ├─────────────────┤  │ Module   │    │(Hall Eff)  │    ├──────────────────┤
   │ • MPU6500/9250  │  │ (NEO-6M) │    │            │    │ • MCP2515        │
   │   6-Axis IMU    │  │ NMEA Out │    │ Frequency  │    │ • SN65HVD230     │
   │                 │  │ 1Hz Rate │    │ Counter    │    │ • TJA1051        │
   │ • ADS1115       │  │          │    │            │    │                  │
   │   16-bit ADC    │  └──────────┘    └────────────┘    └──────────────────┘
   │   - AIN0: Vbat  │
   │   - AIN1: Icurr │
   └─────────────────┘
```

## Data Flow Diagram

```
SENSORS                          PROCESSING                      TRANSMISSION
(100Hz)                         (Protected)                      (Varies)

MPU6500 ──┐
          │
          ├──→ Sensor Manager ──→ Mutex Lock ──┐
          │    (I2C Read)                       │
ADS1115 ──┤                                     │
          │    GPS Manager       Mutex Lock ──→ Global Data ──→ WiFi Task ──→ HTTP POST
          ├──→ (UART Parse)      Protected       Sensor        (5s interval)  JSON
          │                                     Structure
GPS ──────┤
          │    Speed Manager      Mutex Lock ──┐
          ├──→ (Frequency)                      │
          │                                     │
Speed ────┤                                     └──→ CAN TX Task
          │                                         (100ms cycle)
          └──→ CAN RX Task ─────→ Status & Flags

Data timestamp resolution:
• MPU6500: microseconds (timestamp_us)
• ADS1115: milliseconds (timestamp_ms)
• GPS: milliseconds (timestamp_ms)
• Speed: milliseconds (timestamp_ms)
```

## Memory Layout (Heap & Stack)

```
ESP32 Memory (520KB Internal)

┌─────────────────────────────────────┐ 0x3FFFC000 (Heap End)
│       Heap (Growing down)           │
│  ┌─────────────────────────────────┐│
│  │ • WiFi Buffers (20KB)           ││
│  │ • JSON Serialization (2KB)      ││
│  │ • UART Buffers (2KB)            ││
│  │ • Malloc Heap (20KB)            ││
│  └─────────────────────────────────┘│
├─────────────────────────────────────┤
│  Task Stacks (Growing up)           │
│  ┌─────────────────────────────────┐│
│  │ wifiTask Stack (8KB)            ││
│  │ gpsTask Stack (8KB)             ││
│  │ sensorReadTask (4KB)            ││
│  │ canRxTask (4KB)                 ││
│  │ canTxTask (4KB)                 ││
│  │ loop Stack (4KB)                ││
│  │ (Other system stacks ~16KB)     ││
│  └─────────────────────────────────┘│
├─────────────────────────────────────┤
│  Static Data                         │
│  • Global structures: ~8KB           │
│  • Const strings: ~2KB               │
│  • Semaphores: ~200 bytes            │
├─────────────────────────────────────┤
│  IRAM (Instruction RAM) - 64KB       │
│  • ISR handlers                      │
│  • Frequently used code              │
└─────────────────────────────────────┘

Typical Heap Status:
Total available: ~300KB
Used: ~200KB (WiFi, buffers)
Free: ~100KB
Fragmentation: Low (with proper malloc strategy)
```

## Time Synchronization

```
FREERTOS TICK TIMER (10ms per tick)

Setup() ──→ createTasks() ──→ FreeRTOS Scheduler Starts
                              ▼
                    ┌──────────────────────┐
                    │  Every 10ms (Tick)   │
                    └──────────────────────┘
                              ▼
    ┌─────────────────────────┼─────────────────────────┐
    ▼                         ▼                         ▼
[App CPU]              [Pro CPU]                  [Real-time Timer]
                                                  (Microseconds)
sensorReadTask ──→ (100ms) ──→ Update I2C
       ▼
       └──→ Check timestamp
                    ▼
              Store timestamp_us

gpsTask ──→ (50ms) ──→ Parse UART

canRxTask ──→ (10ms) ──→ Receive message

canTxTask ──→ (100ms) ──→ Pack & Send

wifiTask ──→ (1000ms) ──→ HTTP POST
                             (Contains millis() timestamp)

loop() ──→ (100ms) ──→ Monitor & Log
```

## I2C Communication Timeline

```
REQUEST (Every 100ms from sensorReadTask)

Time   Event                 I2C Bus State
────────────────────────────────────────────
0ms    MPU6500 READ
       START                 SDA↓ SCL↓
2μs    Address + Read        Send 0x68 (slave address)
       Wait ACK              SDA→ (slave pulls down)
4μs    Register Read         Send 0x3B (accelerometer data)
6μs    Read 14 bytes         SCL→ (clock for data)
      (Accel + Temp + Gyro)  8μs per bit × 14 bytes × 8 bits
120μs  STOP                  SCL↑ SDA↑
       Data Available        Return to sensorReadTask

Time   Event                 I2C Bus State
────────────────────────────────────────────
130μs  ADS1115 READ
       START
       Address + Read        Send 0x48
       Write Config          Set gain, rate
       Start Conversion
       Wait → 20ms
150ms  Read Result
       STOP

Total Cycle: ~150ms (limited by ADS1115 conversion time)
```

## GPS UART Data Flow

```
GPS Module ──(Serial Data)──→ UART1 RX (GPIO26)
                              ▼
                    UART RX Buffer (1024 bytes)
                              ▼
                   gpsTask (FreeRTOS)
                              ▼
                    Parser State Machine
                    
NMEA Sentence Example:
$GPRMC,123456.00,A,-6.212345,S,106.845123,E,25.3,180.0,090224,*3F
├─ Time: 123456.00
├─ Status: A (active)
├─ Latitude: -6.212345 S
├─ Longitude: 106.845123 E
├─ Speed: 25.3 knots
├─ Course: 180.0°
├─ Date: 090224
└─ Checksum: 3F

Parser Output:
{
  .latitude = -6.212345,
  .longitude = 106.845123,
  .speed_kmh = 46.9,
  .timestamp_ms = 12345
}

Update Frequency: ~1Hz (one complete sentence per second)
```

## CAN Bus Message Format

```
CAN Message Structure (8 bytes max):

┌─ Message ID (0x100-0x103)
│
│  Byte 0-7 (Data)
│  ┌────┬────┬────┬────┬────┬────┬────┬────┐
│  │ B0 │ B1 │ B2 │ B3 │ B4 │ B5 │ B6 │ B7 │
│  └────┴────┴────┴────┴────┴────┴────┴────┘

Example Message 0x100 (MPU6500):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ AX │ AX │ AY │ AY │ AZ │ AZ │ T  │ T  │  (Accel X,Y,Z + Temp)
│MSB │LSB │MSB │LSB │MSB │LSB │MSB │LSB │
└────┴────┴────┴────┴────┴────┴────┴────┘
  int16_t × 3 + int16_t (temperature)

Transmission:
sensorReadTask ──→ Pack Data ──→ canTxTask ──→ TWAI Driver ──→ CAN Bus
(100ms)           (Every cycle)  (100ms)        (500kbps)      (25ms transit)

Total latency: ~125ms (sensor to CAN transmission)
```

## WiFi HTTP Communication

```
WiFi Task (wifiTask)
Every 5000ms:

Step 1: Check WiFi Status
        WiFi.status() == WL_CONNECTED?
        ├─ Yes ──→ Continue
        └─ No ──→ Attempt Reconnect (max 5 retries)

Step 2: Create JSON Payload
        StaticJsonDocument<512>
        ├─ Timestamp (millis)
        ├─ MPU6500 data (if valid)
        ├─ ADS1115 data (if valid)
        ├─ GPS data (if valid)
        ├─ Speed data (if valid)
        └─ System status
        
        Output: ~256 bytes JSON string

Step 3: HTTP POST
        URL: http://your-server:8080/api/sensor/data
        Headers: Content-Type: application/json
        Body: JSON payload
        Timeout: 5000ms
        
Step 4: Response
        ├─ HTTP 200 ──→ Success (log data)
        ├─ HTTP 4xx ──→ Client Error (check JSON)
        ├─ HTTP 5xx ──→ Server Error (retry)
        └─ Timeout ──→ Retry next cycle

Bandwidth: ~1KB per transmission × 0.2 Hz = 200 bytes/sec
```

## Mutex Locking Pattern

```
Producer Task (e.g., sensorReadTask):
    
    Read I2C device → Get new data
    ↓
    xSemaphoreTake(g_data_sensor_mutex, 100ms timeout)
    ├─ Acquired ──→ Critical Section
    │              • Update g_data_sensor.mpu6500
    │              • Update validity flags
    │              • xSemaphoreGive() [Release]
    └─ Timeout ──→ Skip this cycle (data unchanged)


Consumer Task (e.g., wifiTask):
    
    xSemaphoreTake(g_data_sensor_mutex, 100ms timeout)
    ├─ Acquired ──→ Critical Section
    │              • Read g_data_sensor (all fields)
    │              • Copy to local variable
    │              • xSemaphoreGive() [Release]
    │              • Process data (outside critical section)
    └─ Timeout ──→ Use previous data or skip


Race Condition Prevention:
    
    Without Mutex:                  With Mutex:
    Task A: Read X ───┐            Protected Read
            Read Y    │             Protected Write
    Task B: Write Y ──┤ CONFLICT!   No Conflict ✓
            Write X ──┘             Serialized Access

Max Wait Time: 100ms
    If task holds mutex >100ms, waiting task gives up
    Prevents deadlock in real-time system
```

---

**This architecture ensures:**
- ✅ Real-time sensor data acquisition (no WiFi interference)
- ✅ Thread-safe data sharing across tasks
- ✅ Deterministic timing with FreeRTOS
- ✅ Non-blocking communication (GPIO interrupts)
- ✅ Scalable design (easy to add new tasks)
- ✅ Clear separation of concerns


#ifndef DATA_SENSOR_H
#define DATA_SENSOR_H

#include <stdint.h>
#include <time.h>

// ============================================================================
// SENSOR DATA STRUCTURES
// ============================================================================

typedef struct {
    float x;
    float y;
    float z;
} Vector3_t;

typedef struct {
    Vector3_t accel;        // Acceleration (m/s²)
    Vector3_t gyro;         // Angular velocity (°/s)
    float temperature;      // Temperature (°C)
    uint64_t timestamp_us;  // Microsecond timestamp
} MPU6500Data_t;

typedef struct {
    float voltage;          // Measured voltage (V)
    float current;          // Calculated current (A)
    uint16_t raw_adc[2];    // Raw ADC values from AIN0 and AIN1
    uint32_t timestamp_ms;  // Millisecond timestamp
} ADS1115Data_t;

typedef struct {
    float latitude;         // Latitude (degrees)
    float longitude;        // Longitude (degrees)
    float altitude;         // Altitude (meters)
    float speed_kts;        // Speed (knots)
    float speed_kmh;        // Speed (km/h)
    float course;           // Course (degrees)
    uint8_t satellites;     // Number of satellites
    uint8_t fix_quality;    // GPS fix quality (0=invalid, 1=GPS fix, 2=DGPS fix)
    uint32_t timestamp_ms;  // Millisecond timestamp
    char utc_time[12];      // UTC time (hhmmss.ss)
    char utc_date[7];       // UTC date (ddmmyy)
    bool data_valid;        // Data validity flag
} GPSData_t;

typedef struct {
    float speed_kmh;        // Wheel speed (km/h)
    float speed_ms;         // Wheel speed (m/s)
    float distance_m;       // Accumulated distance (meters)
    uint32_t pulse_count;   // Pulse counter
    uint32_t last_pulse_us; // Last pulse timestamp
    uint32_t timestamp_ms;  // Millisecond timestamp
} SpeedData_t;

typedef struct {
    uint32_t id;            // CAN message ID
    uint8_t dlc;            // Data length code
    uint8_t data[8];        // CAN data bytes
    uint32_t timestamp_ms;  // Millisecond timestamp
} CANData_t;

typedef struct {
    uint8_t battery_percent;
    float battery_voltage;
    float mcu_temperature;
    uint32_t uptime_ms;
    uint32_t heap_free;
    uint32_t error_count;
    bool wifi_connected;
    bool gps_locked;
    bool can_active;
} SystemStatus_t;

// ============================================================================
// MAIN DATA SENSOR STRUCTURE (Thread-Safe Access)
// ============================================================================

typedef struct __attribute__((packed)) {
    // Sensor data
    MPU6500Data_t mpu6500;
    ADS1115Data_t ads1115;
    GPSData_t gps;
    SpeedData_t speed;
    CANData_t can_rx;
    
    // System status
    SystemStatus_t status;
    
    // Data validity flags
    struct {
        bool mpu6500_valid;
        bool ads1115_valid;
        bool gps_valid;
        bool speed_valid;
        bool can_valid;
    } flags;
    
    // Timestamp of last update
    uint32_t last_update_ms;
    
} DataSensor_t;

// ============================================================================
// EXTERN GLOBAL VARIABLE (Accessed by all tasks)
// ============================================================================
extern DataSensor_t g_data_sensor;

#endif // DATA_SENSOR_H

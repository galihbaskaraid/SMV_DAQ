#ifndef DATA_SENSOR_H
#define DATA_SENSOR_H

#include <stdint.h>
#include <time.h>

// ============================================================================
// FORCE TIGHT PACKING - NO PADDING/ALIGNMENT
// ============================================================================
// Critical for BLE serialization consistency between ESP32 and Android
#pragma pack(push, 1)

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
    float hdop;             // Horizontal Dilution of Precision
    float vdop;             // Vertical Dilution of Precision
    float pdop;             // Position Dilution of Precision
    uint8_t satellites;     // Number of satellites in view
    uint8_t satellites_active;  // Number of satellites used for fix
    uint8_t fix_quality;    // GPS fix quality (0=invalid, 1=GPS fix, 2=DGPS fix)
    uint8_t fix_type;       // Fix type (1=2D, 3=3D)
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
    float temperature;      // Temperature from sensor (°C)
    float humidity;         // Humidity from sensor (%)
    uint32_t timestamp_ms;  // Millisecond timestamp
} EnvData_t;

typedef struct {
    float power;            // Calculated power (W) = voltage × current
    float energy;           // Energy consumed (kJ)
    float energy_kwh;       // Energy consumed (kWh)
    float consumption_rate; // Consumption rate (Wh/km)
    float avg_speed_kmh;    // Average speed (km/h)
    float total_distance_m; // Total distance traveled (m)
    uint32_t total_time_ms; // Total driving time (ms)
    int current_gear;       // Current gear detection (from RPM ratio)
    float smoothed_gear_ratio; // Smoothed gear ratio for filtering
    int drive_status;       // 0=idle, 1=pulling, 2=gliding
    float pull_duration_s;  // Pull duration (seconds)
    float glide_duration_s; // Glide duration (seconds)
    uint32_t timestamp_ms;  // Millisecond timestamp
} CalcData_t;

typedef struct {
    float rpm;              // Motor RPM from VESC
    float motor_current;    // Motor current (A)
    float duty_cycle;       // Duty cycle (%)
    float temp_fet;         // FET temperature (°C)
    float v_in;             // Input voltage (V)
    float amp_hours;        // Amp hours
    float watt_hours;       // Watt hours
    uint32_t timestamp_ms;  // Millisecond timestamp
} VESCData_t;

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

typedef struct {
    // Sensor data
    MPU6500Data_t mpu6500;
    ADS1115Data_t ads1115;
    GPSData_t gps;
    SpeedData_t speed;
    CANData_t can_rx;
    EnvData_t env;          // Temperature/Humidity data
    CalcData_t calc;        // Calculated/processed data
    VESCData_t vesc;        // VESC motor controller data
    
    // System status
    SystemStatus_t status;
    
    // Data validity flags
    struct {
        // Sensor validity (data is valid/recently updated)
        bool mpu6500_valid;
        bool ads1115_valid;
        bool gps_valid;
        bool speed_valid;
        bool can_valid;
        bool env_valid;
        bool calc_valid;
        bool vesc_valid;
        
        // Sensor initialization status (sensor is present and initialized)
        bool mpu6500_init;
        bool ads1115_init;
        bool gps_init;
        bool speed_init;
        bool can_init;
        bool env_init;
        bool wifi_init;
        bool ble_init;
    } flags;
    
    // Timestamp of last update
    uint32_t last_update_ms;
    
} DataSensor_t;

// ============================================================================
// EXTERN GLOBAL VARIABLE (Accessed by all tasks)
// ============================================================================
extern DataSensor_t g_data_sensor;

// ============================================================================
// END TIGHT PACKING
// ============================================================================
#pragma pack(pop)

#endif // DATA_SENSOR_H

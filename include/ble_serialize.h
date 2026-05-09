#ifndef BLE_SERIALIZE_H
#define BLE_SERIALIZE_H

#include <stdint.h>
#include "data_sensor.h"

// ============================================================================
// BLE COMPACT PAYLOAD STRUCTURES
// ============================================================================
// One packed struct per data category — each maps to its own BLE characteristic.
// Sizes are kept well under the 244-byte NimBLE MTU default.
// #pragma pack(1) ensures no padding between fields.

#pragma pack(push, 1)

typedef struct {
    float    voltage;            // V
    float    current;            // A
    float    power;              // W
    float    energy_kwh;         // kWh
    float    consumption_wh_km;  // Wh/km
    uint32_t timestamp_ms;
} BLE_PowerPayload_t;            // 24 bytes

typedef struct {
    float    speed_kmh;          // km/h
    float    total_distance_m;   // m
    float    avg_speed_kmh;      // km/h
    uint32_t pulse_count;
    uint32_t timestamp_ms;
} BLE_SpeedPayload_t;            // 20 bytes

typedef struct {
    float    accel_x;            // m/s²
    float    accel_y;
    float    accel_z;
    float    gyro_x;             // °/s
    float    gyro_y;
    float    gyro_z;
    float    temperature;        // °C
    uint32_t timestamp_ms;
} BLE_IMUPayload_t;              // 32 bytes

typedef struct {
    float    latitude;
    float    longitude;
    float    altitude;           // m
    float    speed_kmh;
    float    course;             // degrees
    float    hdop;
    uint8_t  satellites;
    uint8_t  fix_quality;
    uint32_t timestamp_ms;
} BLE_GPSPayload_t;              // 30 bytes

typedef struct {
    float    temperature;        // °C
    float    humidity;           // %
    uint32_t timestamp_ms;
} BLE_EnvPayload_t;              // 12 bytes

typedef struct {
    int32_t  current_gear;
    int32_t  drive_status;       // 0=idle, 1=pull, 2=glide
    float    pull_duration_s;
    float    glide_duration_s;
    float    avg_speed_kmh;
    uint32_t timestamp_ms;
} BLE_CalcPayload_t;             // 24 bytes

typedef struct {
    uint32_t uptime_ms;
    uint32_t heap_free;
    uint8_t  sensor_valid;       // bits: [0]=mpu [1]=ads [2]=gps [3]=spd [4]=env [5]=can [6]=calc
    uint8_t  sensor_init;        // bits: [0]=mpu [1]=ads [2]=gps [3]=spd [4]=env [5]=can [6]=ble
} BLE_StatusPayload_t;           // 10 bytes

#pragma pack(pop)

// ============================================================================
// SERIALIZATION FUNCTIONS
// ============================================================================

void serializePower (const DataSensor_t *src, BLE_PowerPayload_t  *out);
void serializeSpeed (const DataSensor_t *src, BLE_SpeedPayload_t  *out);
void serializeIMU   (const DataSensor_t *src, BLE_IMUPayload_t    *out);
void serializeGPS   (const DataSensor_t *src, BLE_GPSPayload_t    *out);
void serializeEnv   (const DataSensor_t *src, BLE_EnvPayload_t    *out);
void serializeCalc  (const DataSensor_t *src, BLE_CalcPayload_t   *out);
void serializeStatus(const DataSensor_t *src, BLE_StatusPayload_t *out);

void logActiveSensors(const DataSensor_t *sensor_data);

#endif // BLE_SERIALIZE_H


#include <Arduino.h>
#include "ble_serialize.h"
#include "debug_logging.h"
#include <esp_log.h>
#include <string.h>

static const char* TAG = "BLE_TX";

void serializePower(const DataSensor_t *src, BLE_PowerPayload_t *out) {
    out->voltage           = src->ads1115.voltage;
    out->current           = src->ads1115.current;
    out->power             = src->calc.power;
    out->energy_kwh        = src->calc.energy_kwh;
    out->consumption_wh_km = src->calc.consumption_rate;
    out->timestamp_ms      = src->ads1115.timestamp_ms;
}

void serializeSpeed(const DataSensor_t *src, BLE_SpeedPayload_t *out) {
    out->speed_kmh        = src->speed.speed_kmh;
    out->total_distance_m = src->calc.total_distance_m;
    out->avg_speed_kmh    = src->calc.avg_speed_kmh;
    out->pulse_count      = src->speed.pulse_count;
    out->timestamp_ms     = src->speed.timestamp_ms;
}

void serializeIMU(const DataSensor_t *src, BLE_IMUPayload_t *out) {
    out->accel_x      = src->mpu6500.accel.x;
    out->accel_y      = src->mpu6500.accel.y;
    out->accel_z      = src->mpu6500.accel.z;
    out->gyro_x       = src->mpu6500.gyro.x;
    out->gyro_y       = src->mpu6500.gyro.y;
    out->gyro_z       = src->mpu6500.gyro.z;
    out->temperature  = src->mpu6500.temperature;
    out->timestamp_ms = (uint32_t)(src->mpu6500.timestamp_us / 1000ULL);
}

void serializeGPS(const DataSensor_t *src, BLE_GPSPayload_t *out) {
    out->latitude     = src->gps.latitude;
    out->longitude    = src->gps.longitude;
    out->altitude     = src->gps.altitude;
    out->speed_kmh    = src->gps.speed_kmh;
    out->course       = src->gps.course;
    out->hdop         = src->gps.hdop;
    out->satellites   = src->gps.satellites;
    out->fix_quality  = src->gps.fix_quality;
    out->timestamp_ms = src->gps.timestamp_ms;
}

void serializeEnv(const DataSensor_t *src, BLE_EnvPayload_t *out) {
    out->temperature  = src->env.temperature;
    out->humidity     = src->env.humidity;
    out->timestamp_ms = src->env.timestamp_ms;
}

void serializeCalc(const DataSensor_t *src, BLE_CalcPayload_t *out) {
    out->current_gear     = (int32_t)src->calc.current_gear;
    out->drive_status     = (int32_t)src->calc.drive_status;
    out->pull_duration_s  = src->calc.pull_duration_s;
    out->glide_duration_s = src->calc.glide_duration_s;
    out->avg_speed_kmh    = src->calc.avg_speed_kmh;
    out->timestamp_ms     = src->calc.timestamp_ms;
}

void serializeStatus(const DataSensor_t *src, BLE_StatusPayload_t *out) {
    out->uptime_ms   = src->status.uptime_ms;
    out->heap_free   = src->status.heap_free;
    out->sensor_valid =
        (src->flags.mpu6500_valid ? 0x01 : 0) |
        (src->flags.ads1115_valid ? 0x02 : 0) |
        (src->flags.gps_valid     ? 0x04 : 0) |
        (src->flags.speed_valid   ? 0x08 : 0) |
        (src->flags.env_valid     ? 0x10 : 0) |
        (src->flags.can_valid     ? 0x20 : 0) |
        (src->flags.calc_valid    ? 0x40 : 0);
    out->sensor_init =
        (src->flags.mpu6500_init  ? 0x01 : 0) |
        (src->flags.ads1115_init  ? 0x02 : 0) |
        (src->flags.gps_init      ? 0x04 : 0) |
        (src->flags.speed_init    ? 0x08 : 0) |
        (src->flags.env_init      ? 0x10 : 0) |
        (src->flags.can_init      ? 0x20 : 0) |
        (src->flags.ble_init      ? 0x40 : 0);
}

void logActiveSensors(const DataSensor_t *sensor_data) {
    if (!sensor_data) return;
    char sensor_status[200];
    snprintf(sensor_status, sizeof(sensor_status),
        "Sensors [Valid/Init]: MPU:%d/%d ADS:%d/%d GPS:%d/%d Spd:%d/%d ENV:%d/%d CAN:%d/%d",
        sensor_data->flags.mpu6500_valid, sensor_data->flags.mpu6500_init,
        sensor_data->flags.ads1115_valid, sensor_data->flags.ads1115_init,
        sensor_data->flags.gps_valid,     sensor_data->flags.gps_init,
        sensor_data->flags.speed_valid,   sensor_data->flags.speed_init,
        sensor_data->flags.env_valid,     sensor_data->flags.env_init,
        sensor_data->flags.can_valid,     sensor_data->flags.can_init);
    BLE_LOGI(TAG, "%s", sensor_status);
}


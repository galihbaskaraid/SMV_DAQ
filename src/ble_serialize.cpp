#include <Arduino.h>
#include "ble_serialize.h"
#include "debug_logging.h"
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <cmath>

static const char* TAG = "BLE_TX";

uint16_t serializeToBluetoothPayload(const DataSensor_t *sensor_data) {
    // Return full struct size - MTU 512 can handle it (struct is ~300-400 bytes)
    // Full precision maintained for Android Studio
    return sizeof(DataSensor_t);
}

const char* getPayloadTransmitInfo() {
    static char info[256];
    snprintf(info, sizeof(info),
        "BLE TX Config: Full DataSensor_t transmitted (Size: %zu bytes, MTU: 512 bytes, Utilization: %.1f%%)",
        sizeof(DataSensor_t),
        ((float)sizeof(DataSensor_t) / 512.0f) * 100.0f);
    return info;
}

void logActiveSensors(const DataSensor_t *sensor_data) {
    if (!sensor_data) return;
    
    // Build sensor status string
    char sensor_status[200];
    snprintf(sensor_status, sizeof(sensor_status),
        "Sensors [Valid/Init]: MPU:%d/%d ADS:%d/%d GPS:%d/%d Spd:%d/%d ENV:%d/%d CAN:%d/%d",
        sensor_data->flags.mpu6500_valid, sensor_data->flags.mpu6500_init,
        sensor_data->flags.ads1115_valid, sensor_data->flags.ads1115_init,
        sensor_data->flags.gps_valid, sensor_data->flags.gps_init,
        sensor_data->flags.speed_valid, sensor_data->flags.speed_init,
        sensor_data->flags.env_valid, sensor_data->flags.env_init,
        sensor_data->flags.can_valid, sensor_data->flags.can_init);
    
    BLE_LOGI(TAG, "%s", sensor_status);
}


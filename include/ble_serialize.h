#ifndef BLE_SERIALIZE_H
#define BLE_SERIALIZE_H

#include <stdint.h>
#include "data_sensor.h"

// ============================================================================
// BLE TRANSMISSION - Full DataSensor_t Structure
// ============================================================================
// MTU 512 bytes is sufficient for full DataSensor_t transmission
// This maintains all sensor data without loss of precision for Android Studio

/**
 * Serialize sensor data for BLE transmission (Full struct with logging info)
 * @param sensor_data Source data from g_data_sensor
 * @return Payload size in bytes (sizeof(DataSensor_t))
 */
uint16_t serializeToBluetoothPayload(const DataSensor_t *sensor_data);

/**
 * Get human-readable info about transmitted data
 * @return String describing struct size and sensors included
 */
const char* getPayloadTransmitInfo();

/**
 * Log all active sensor flags
 */
void logActiveSensors(const DataSensor_t *sensor_data);

#endif // BLE_SERIALIZE_H


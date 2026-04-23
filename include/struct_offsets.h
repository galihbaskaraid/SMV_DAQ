#ifndef STRUCT_OFFSETS_H
#define STRUCT_OFFSETS_H

#include <Arduino.h>
#include "data_sensor.h"

/**
 * Log actual struct member offsets for BLE serialization verification
 * Call this in setup() to verify Android deserializer offsets match
 */
void logStructOffsets();

/**
 * Get total size of DataSensor_t struct
 */
size_t getDataSensorStructSize();

#endif // STRUCT_OFFSETS_H

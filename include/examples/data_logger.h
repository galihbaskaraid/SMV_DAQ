// ============================================================================
// EXAMPLE APPLICATION: DATA LOGGING WITH SPIFFS
// ============================================================================
// This example demonstrates how to log sensor data to the file system
// Uncomment and integrate into your project as needed

#include <SPIFFS.h>
#include "constants.h"
#include "data_sensor.h"

#ifndef EXAMPLE_DATA_LOGGING_H
#define EXAMPLE_DATA_LOGGING_H

class DataLogger {
private:
    File data_file;
    uint32_t last_flush_time;
    uint32_t record_count;
    const char* filename;
    
public:
    DataLogger(const char* fname = "/spiffs/sensor_data.csv")
        : filename(fname), last_flush_time(0), record_count(0) {}
    
    bool init() {
        if (!SPIFFS.begin(true)) {
            ESP_LOGE(TAG_SYSTEM, "SPIFFS mount failed");
            return false;
        }
        
        data_file = SPIFFS.open(filename, FILE_APPEND);
        if (!data_file) {
            ESP_LOGE(TAG_SYSTEM, "Could not open log file: %s", filename);
            return false;
        }
        
        // Write CSV header
        writeHeader();
        
        ESP_LOGI(TAG_SYSTEM, "Data logger initialized: %s", filename);
        return true;
    }
    
    void writeHeader() {
        data_file.println("timestamp_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,"
                         "temperature,voltage,current,latitude,longitude,altitude,"
                         "gps_speed_kmh,satellites,speed_kmh,pulses");
    }
    
    void logData(const DataSensor_t &data) {
        if (!data_file) return;
        
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                "%ld,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.2f,%.3f,%.6f,%.6f,%.1f,%.1f,%d,%.2f,%ld",
                millis(),
                data.mpu6500.accel.x, data.mpu6500.accel.y, data.mpu6500.accel.z,
                data.mpu6500.gyro.x, data.mpu6500.gyro.y, data.mpu6500.gyro.z,
                data.mpu6500.temperature,
                data.ads1115.voltage, data.ads1115.current,
                data.gps.latitude, data.gps.longitude, data.gps.altitude,
                data.gps.speed_kmh, data.gps.satellites,
                data.speed.speed_kmh, data.speed.pulse_count);
        
        data_file.println(buffer);
        record_count++;
        
        // Flush every 10 records or every 5 seconds
        if (record_count % 10 == 0 || millis() - last_flush_time > 5000) {
            data_file.flush();
            last_flush_time = millis();
        }
    }
    
    uint32_t getRecordCount() const { return record_count; }
    uint32_t getFileSize() const { return data_file.size(); }
    
    void close() {
        if (data_file) {
            data_file.close();
        }
    }
    
    ~DataLogger() {
        close();
    }
};

#endif // EXAMPLE_DATA_LOGGING_H

// ============================================================================
// USAGE IN MAIN.CPP:
// ============================================================================

/*

#include "examples/data_logger.h"

// In setup():
DataLogger logger;
if (!logger.init()) {
    ESP_LOGE(TAG_SYSTEM, "Logger initialization failed");
}

// In sensorReadTask():
void exampleLoggingTask(void *pvParameters) {
    DataLogger *logger = (DataLogger *)pvParameters;
    
    while (1) {
        if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(100))) {
            logger->logData(g_data_sensor);
            xSemaphoreGive(g_data_sensor_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

*/

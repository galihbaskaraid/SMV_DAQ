#include "can_manager.h"
#include "constants.h"
#include <driver/twai.h>

// ============================================================================
// GLOBAL CAN MANAGER INSTANCE
// ============================================================================

CANManager g_can_manager;

// ============================================================================
// CAN MANAGER IMPLEMENTATION
// ============================================================================

CANManager::CANManager() : initialized(false), data_mutex(nullptr) {}

CANManager::~CANManager() {
    deinit();
}

bool CANManager::init() {
    // TWAI (CAN) configuration
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        ESP_LOGE(TAG_CAN, "Failed to install TWAI driver");
        return false;
    }
    
    if (twai_start() != ESP_OK) {
        ESP_LOGE(TAG_CAN, "Failed to start TWAI driver");
        twai_driver_uninstall();
        return false;
    }
    
    ESP_LOGI(TAG_CAN, "CAN bus initialized successfully at 500kbps");
    initialized = true;
    return true;
}

void CANManager::deinit() {
    if (initialized) {
        twai_stop();
        twai_driver_uninstall();
        initialized = false;
    }
}

bool CANManager::sendMessage(uint32_t id, const uint8_t* data, uint8_t dlc) {
    if (!initialized || dlc > 8) return false;
    
    twai_message_t message = {
        .flags = 0,
        .identifier = id,
        .data_length_code = dlc,
        .data = {0}
    };
    
    if (data != nullptr) {
        memcpy(message.data, data, dlc);
    }
    
    if (twai_transmit(&message, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG_CAN, "Failed to transmit CAN message");
        return false;
    }
    
    return true;
}

bool CANManager::getData(CANData_t &data) {
    if (data_mutex == nullptr) return false;
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        data = g_data_sensor.can_rx;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

// ============================================================================
// CAN RX TASK (Receive messages from CAN bus)
// ============================================================================

void canRxTask(void *pvParameters) {
    ESP_LOGI(TAG_CAN, "CAN RX task started");
    
    twai_message_t message;
    
    while (1) {
        if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
            // Message received
            CANData_t data;
            data.id = message.identifier;
            data.dlc = message.data_length_code;
            memcpy(data.data, message.data, data.dlc);
            data.timestamp_ms = millis();
            
            // Update global data structure with thread-safe access
            if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_data_sensor.can_rx = data;
                g_data_sensor.flags.can_valid = true;
                xSemaphoreGive(g_data_sensor_mutex);
            }
            
            ESP_LOGD(TAG_CAN, "CAN RX: ID=0x%03x, DLC=%d", data.id, data.dlc);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(nullptr);
}

// ============================================================================
// CAN TX TASK (Transmit sensor data via CAN bus)
// ============================================================================

void canTxTask(void *pvParameters) {
    ESP_LOGI(TAG_CAN, "CAN TX task started");
    
    while (1) {
        // Thread-safe read of sensor data
        if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Example: Send MPU6500 data via CAN
            if (g_data_sensor.flags.mpu6500_valid) {
                uint8_t can_data[8];
                
                // Pack accelerometer X,Y (int16 each)
                int16_t accel_x = (int16_t)(g_data_sensor.mpu6500.accel.x * 100);
                int16_t accel_y = (int16_t)(g_data_sensor.mpu6500.accel.y * 100);
                
                can_data[0] = (accel_x >> 8) & 0xFF;
                can_data[1] = accel_x & 0xFF;
                can_data[2] = (accel_y >> 8) & 0xFF;
                can_data[3] = accel_y & 0xFF;
                
                xSemaphoreGive(g_data_sensor_mutex);
                g_can_manager.sendMessage(CAN_ID_SENSOR_DATA, can_data, 4);
                xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(50));
            }
            
            // Example: Send GPS data via CAN
            if (g_data_sensor.flags.gps_valid) {
                uint8_t can_data[8];
                
                // Pack speed in km/h (int16, divide by 10)
                int16_t speed_kmh = (int16_t)(g_data_sensor.gps.speed_kmh * 10);
                can_data[0] = (speed_kmh >> 8) & 0xFF;
                can_data[1] = speed_kmh & 0xFF;
                
                xSemaphoreGive(g_data_sensor_mutex);
                g_can_manager.sendMessage(CAN_ID_GPS_DATA, can_data, 2);
                xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(50));
            }
            
            xSemaphoreGive(g_data_sensor_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(nullptr);
}

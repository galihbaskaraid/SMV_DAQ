#include "struct_offsets.h"
#include <esp_log.h>

static const char* TAG = "STRUCT_OFFSETS";

void logStructOffsets() {
    DataSensor_t dummy = {};
    
    ESP_LOGI(TAG, "=== DataSensor_t STRUCT OFFSETS (with #pragma pack(1)) ===");
    ESP_LOGI(TAG, "Total struct size: %u bytes", sizeof(DataSensor_t));
    ESP_LOGI(TAG, "");
    
    // MPU6500Data_t
    ESP_LOGI(TAG, "MPU6500Data_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.mpu6500 - (uintptr_t)&dummy,
        offsetof(DataSensor_t, mpu6500));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(MPU6500Data_t));
    
    // ADS1115Data_t
    ESP_LOGI(TAG, "ADS1115Data_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.ads1115 - (uintptr_t)&dummy,
        offsetof(DataSensor_t, ads1115));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(ADS1115Data_t));
    
    // GPSData_t
    ESP_LOGI(TAG, "GPSData_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.gps - (uintptr_t)&dummy,
        offsetof(DataSensor_t, gps));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(GPSData_t));
    
    // SpeedData_t
    ESP_LOGI(TAG, "SpeedData_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.speed - (uintptr_t)&dummy,
        offsetof(DataSensor_t, speed));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(SpeedData_t));
    
    // CANData_t
    ESP_LOGI(TAG, "CANData_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.can_rx - (uintptr_t)&dummy,
        offsetof(DataSensor_t, can_rx));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(CANData_t));
    
    // EnvData_t
    ESP_LOGI(TAG, "EnvData_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.env - (uintptr_t)&dummy,
        offsetof(DataSensor_t, env));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(EnvData_t));
    
    // CalcData_t
    ESP_LOGI(TAG, "CalcData_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.calc - (uintptr_t)&dummy,
        offsetof(DataSensor_t, calc));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(CalcData_t));
    
    // VESCData_t
    ESP_LOGI(TAG, "VESCData_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.vesc - (uintptr_t)&dummy,
        offsetof(DataSensor_t, vesc));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(VESCData_t));
    
    // SystemStatus_t
    ESP_LOGI(TAG, "SystemStatus_t offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.status - (uintptr_t)&dummy,
        offsetof(DataSensor_t, status));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(SystemStatus_t));
    
    // Flags
    ESP_LOGI(TAG, "Validity flags offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.flags - (uintptr_t)&dummy,
        offsetof(DataSensor_t, flags));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(dummy.flags));
    
    // Last update
    ESP_LOGI(TAG, "last_update_ms offset: 0x%04X (%u)", 
        (uintptr_t)&dummy.last_update_ms - (uintptr_t)&dummy,
        offsetof(DataSensor_t, last_update_ms));
    ESP_LOGI(TAG, "  - size: %u bytes", sizeof(uint32_t));
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Member sizes:");
    ESP_LOGI(TAG, "  Vector3_t: %u bytes", sizeof(Vector3_t));
    ESP_LOGI(TAG, "  MPU6500Data_t: %u bytes", sizeof(MPU6500Data_t));
    ESP_LOGI(TAG, "  ADS1115Data_t: %u bytes", sizeof(ADS1115Data_t));
    ESP_LOGI(TAG, "  GPSData_t: %u bytes", sizeof(GPSData_t));
    ESP_LOGI(TAG, "  SpeedData_t: %u bytes", sizeof(SpeedData_t));
    ESP_LOGI(TAG, "  CANData_t: %u bytes", sizeof(CANData_t));
    ESP_LOGI(TAG, "  EnvData_t: %u bytes", sizeof(EnvData_t));
    ESP_LOGI(TAG, "  CalcData_t: %u bytes", sizeof(CalcData_t));
    ESP_LOGI(TAG, "  VESCData_t: %u bytes", sizeof(VESCData_t));
    ESP_LOGI(TAG, "  SystemStatus_t: %u bytes", sizeof(SystemStatus_t));
    ESP_LOGI(TAG, "");
}

size_t getDataSensorStructSize() {
    return sizeof(DataSensor_t);
}

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "data_sensor.h"

// ============================================================================
// MPU6500 SENSOR MANAGER
// ============================================================================

class MPU6500Manager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;
    
public:
    MPU6500Manager();
    ~MPU6500Manager();
    
    bool init();
    void update();
    bool getData(MPU6500Data_t &data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
};

// ============================================================================
// ADS1115 SENSOR MANAGER
// ============================================================================

class ADS1115Manager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;
    float voltage_filter[10];
    uint8_t filter_index;
    float current_offset_mv;  // Calibrated offset from ADS1115 at 0A
    
public:
    ADS1115Manager();
    ~ADS1115Manager();
    
    bool init();
    void update();
    bool getData(ADS1115Data_t &data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
    
private:
    float filterVoltage(float raw_voltage);
    float calculateCurrent(float adc_voltage_mv);
    void calibrateCurrentSensor();  // Calibrate offset at startup
};

// ============================================================================
// SPEED SENSOR MANAGER
// ============================================================================

class SpeedSensorManager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;  // Track initialization state
    volatile uint32_t pulse_count;
    volatile uint32_t last_pulse_time;
    uint32_t last_update_time;
    float speed_filter[5];
    uint8_t filter_index;
    
public:
    SpeedSensorManager();
    ~SpeedSensorManager();
    
    bool init();
    void update();
    bool getData(SpeedData_t &data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
    void handlePulse();  // Called from ISR
    
private:
    float filterSpeed(float raw_speed);
};

// ============================================================================
// GLOBAL SENSOR MANAGER INSTANCES
// ============================================================================

extern MPU6500Manager g_mpu6500;
extern ADS1115Manager g_ads1115;
extern SpeedSensorManager g_speed_sensor;

#endif // SENSOR_MANAGER_H

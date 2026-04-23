#ifndef DATA_CALC_H
#define DATA_CALC_H

#include <stdint.h>
#include "data_sensor.h"
#include "constants.h"

// ============================================================================
// DATA CALCULATION & PROCESSING CLASS
// ============================================================================

class DataCalculator {
private:
    // Power and Energy tracking
    float last_power = 0.0f;
    float accumulated_energy_j = 0.0f;  // Accumulated energy in Joules
    uint32_t last_power_calc_time = 0;
    
    // Speed and Distance tracking
    float last_valid_speed = 0.0f;
    float accumulated_distance_m = 0.0f;
    uint32_t total_moving_time_ms = 0;
    uint32_t last_speed_calc_time = 0;
    
    // Gear detection
    int current_gear = 0;
    float smoothed_gear_ratio = 0.0f;
    uint32_t gear_change_timer = 0;
    
    // Drive state tracking
    enum DriveState { IDLE, PULLING, GLIDING } current_state;
    uint32_t pull_start_time = 0;
    uint32_t glide_start_time = 0;
    float pull_duration_s = 0.0f;
    float glide_duration_s = 0.0f;
    
    // Exponential Moving Average for smoothing
    float speed_ema = 0.0f;
    float current_ema = 0.0f;
    float voltage_ema = 0.0f;
    
public:
    DataCalculator();
    ~DataCalculator();
    
    // Power and Energy calculations
    void calculatePower(float voltage_v, float current_a, CalcData_t &calc_data);
    void calculateEnergy(float power_w, uint32_t time_ms, CalcData_t &calc_data);
    float getAccumulatedEnergy() const { return JOULES_TO_KWH(accumulated_energy_j); }
    
    // Speed and Distance calculations
    void calculateDistance(float speed_kmh, uint32_t time_ms, CalcData_t &calc_data);
    void updateMovingTime(float speed_kmh, uint32_t time_ms);
    float getAccumulatedDistance() const { return accumulated_distance_m; }
    float getAverageSpeed() const;
    
    // Gear detection (motor RPM based)
    void detectGear(float motor_rpm, float wheel_rpm, CalcData_t &calc_data);
    int getCurrentGear() const { return current_gear; }
    int getDetectedGear() const { return current_gear; }
    
    // Drive state tracking
    void updateDriveState(float current_a, CalcData_t &calc_data);
    float getPullTimer() const { return pull_duration_s; }
    float getGlideTimer() const { return glide_duration_s; }
    float getMotorRPM() const;  // Placeholder for motor RPM calculation
    void resetTotals();
    
    // Exponential Moving Average for smoothing noisy sensor data
    float updateEMA(float &ema, float new_value, float alpha);
    
private:
    float calculateGearRatio(float motor_rpm, float wheel_rpm);
};

// ============================================================================
// WSEN_HIDS SENSOR INTERFACE
// ============================================================================

class EnvironmentSensor {
private:
    uint32_t last_read_time = 0;
    
public:
    EnvironmentSensor();
    ~EnvironmentSensor();
    
    // Initialize the sensor
    bool init();
    
    // Read temperature and humidity
    bool readData(EnvData_t &env_data);
    
    // Helper for CRC calculation
    static uint8_t calculateCRC(const uint8_t* data, uint16_t count);
    static int8_t checkCRC(const uint8_t* data, uint16_t count, uint8_t checksum);
};

// ============================================================================
// GLOBAL INSTANCES (Defined in data_calc.cpp)
// ============================================================================
extern DataCalculator g_data_calc;
extern EnvironmentSensor g_env_sensor;

#endif // DATA_CALC_H

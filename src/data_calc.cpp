#include "data_calc.h"
#include "constants.h"
#include "debug_logging.h"
#include <Wire.h>
#include <esp_log.h>
#include <cmath>

static const char* TAG = "DATA_CALC";

// ============================================================================
// DATA CALCULATOR IMPLEMENTATION
// ============================================================================

DataCalculator::DataCalculator() 
    : current_state(IDLE) {
}

DataCalculator::~DataCalculator() {
}

void DataCalculator::calculatePower(float voltage_v, float current_a, CalcData_t &calc_data) {
    uint32_t current_time = millis();
    
    // Calculate instantaneous power
    float power_w = voltage_v * current_a;
    
    // Apply exponential moving average for smoothing
    updateEMA(last_power, power_w, ALPHA_DEFAULT);
    
    calc_data.power = last_power;
    calc_data.timestamp_ms = current_time;
    
    // Log power values
    if (current_time - last_power_calc_time >= POWER_CALC_INTERVAL_MS) {
        CALC_LOGI(TAG, "Power: %.2fW (V=%.2fV, I=%.2fA)", power_w, voltage_v, current_a);
        last_power_calc_time = current_time;
    }
}

void DataCalculator::calculateEnergy(float power_w, uint32_t time_ms, CalcData_t &calc_data) {
    static uint32_t last_energy_calc_time = 0;
    
    uint32_t current_time = millis();
    
    // Only calculate every ENERGY_CALC_INTERVAL_MS to avoid floating point drift
    if (current_time - last_energy_calc_time >= ENERGY_CALC_INTERVAL_MS) {
        uint32_t time_delta_ms = current_time - last_energy_calc_time;
        float time_delta_s = time_delta_ms / 1000.0f;
        
        // Energy = Power × Time (Joules = Watts × seconds)
        float energy_delta_j = power_w * time_delta_s;
        accumulated_energy_j += energy_delta_j;
        
        // Convert to kJ and kWh
        calc_data.energy = JOULES_TO_KJ(accumulated_energy_j);
        calc_data.energy_kwh = JOULES_TO_KWH(accumulated_energy_j);
        
        last_energy_calc_time = current_time;
        
        CALC_LOGI(TAG, "Energy: %.2f kJ (%.4f kWh)", calc_data.energy, calc_data.energy_kwh);
    }
}

void DataCalculator::calculateDistance(float speed_kmh, uint32_t time_ms, CalcData_t &calc_data) {
    static uint32_t last_distance_calc_time = 0;
    
    uint32_t current_time = millis();
    
    // Only calculate every CONSUMPTION_CALC_INTERVAL_MS
    if (current_time - last_distance_calc_time >= CONSUMPTION_CALC_INTERVAL_MS) {
        uint32_t time_delta_ms = current_time - last_distance_calc_time;
        float time_delta_h = time_delta_ms / 3600000.0f;  // Convert ms to hours
        
        // Distance = Speed × Time (km = km/h × h)
        float distance_delta_km = speed_kmh * time_delta_h;
        accumulated_distance_m += (distance_delta_km * 1000.0f);
        
        calc_data.total_distance_m = accumulated_distance_m;
        
        last_distance_calc_time = current_time;
        
        CALC_LOGI(TAG, "Distance: %.2f m (Speed: %.2f km/h)", accumulated_distance_m, speed_kmh);
    }
}

void DataCalculator::updateMovingTime(float speed_kmh, uint32_t time_ms) {
    static uint32_t last_moving_time_check = 0;
    uint32_t current_time = millis();
    
    if (current_time - last_moving_time_check >= 100) {  // Check every 100ms
        if (speed_kmh > STABLE_SPEED_THRESHOLD_KMH) {
            uint32_t time_delta = current_time - last_moving_time_check;
            total_moving_time_ms += time_delta;
        }
        last_moving_time_check = current_time;
    }
}

float DataCalculator::getAverageSpeed() const {
    if (total_moving_time_ms == 0) return 0.0f;
    
    // Average speed = total distance / total time
    float avg_speed_ms = accumulated_distance_m / (total_moving_time_ms / 1000.0f);
    return (avg_speed_ms * 3.6f);  // Convert m/s to km/h
}

void DataCalculator::detectGear(float motor_rpm, float wheel_rpm, CalcData_t &calc_data) {
    uint32_t current_time = millis();
    
    if (wheel_rpm > 0.1f) {  // Avoid division by zero
        float gear_ratio = calculateGearRatio(motor_rpm, wheel_rpm);
        
        // Apply exponential moving average for smoothing
        float alpha = ALPHA_DEFAULT;
        
        // Use more responsive alpha during high acceleration
        float accel_factor = fabs(gear_ratio - smoothed_gear_ratio);
        if (accel_factor > ACCEL_VALIDATION_FACTOR) {
            alpha = ALPHA_RESPONSIVE;
        } else if (accel_factor < STABLE_SPEED_THRESHOLD_KMH) {
            alpha = ALPHA_SMOOTH;
        }
        
        smoothed_gear_ratio = updateEMA(smoothed_gear_ratio, gear_ratio, alpha);
        
        // Gear detection logic (basic thresholds, adjust based on your vehicle)
        int new_gear = 1;  // Default to gear 1
        
        if (smoothed_gear_ratio > 4.0f) new_gear = 1;
        else if (smoothed_gear_ratio > 2.5f) new_gear = 2;
        else if (smoothed_gear_ratio > 1.5f) new_gear = 3;
        else if (smoothed_gear_ratio > 1.0f) new_gear = 4;
        else new_gear = 5;  // Direct drive or high speed
        
        // Confirm gear change after GEAR_CONFIRMATION_TIME_MS
        if (new_gear != current_gear) {
            if (gear_change_timer == 0) {
                gear_change_timer = current_time;
            } else if (current_time - gear_change_timer >= GEAR_CONFIRMATION_TIME_MS) {
                current_gear = new_gear;
                gear_change_timer = 0;
                CALC_LOGI(TAG, "Gear changed to: %d (Ratio: %.2f)", current_gear, smoothed_gear_ratio);
            }
        } else {
            gear_change_timer = 0;
        }
    }
    
    calc_data.current_gear = current_gear;
    calc_data.smoothed_gear_ratio = smoothed_gear_ratio;
}

float DataCalculator::calculateGearRatio(float motor_rpm, float wheel_rpm) {
    if (wheel_rpm < 0.1f) return 1.0f;  // Avoid division by zero
    return motor_rpm / wheel_rpm;
}

void DataCalculator::updateDriveState(float current_a, CalcData_t &calc_data) {
    uint32_t current_time = millis();
    
    // Simple state machine: PULLING (high current) vs GLIDING (low current)
    DriveState new_state = (current_a > 1.0f) ? PULLING : GLIDING;
    
    if (new_state != current_state) {
        current_state = new_state;
        
        if (current_state == PULLING) {
            pull_start_time = current_time;
            glide_start_time = 0;
            CALC_LOGI(TAG, "State changed to: PULLING");
        } else {
            glide_start_time = current_time;
            pull_start_time = 0;
            CALC_LOGI(TAG, "State changed to: GLIDING");
        }
    }
    
    // Update durations
    if (pull_start_time > 0) {
        pull_duration_s = (current_time - pull_start_time) / 1000.0f;
    }
    
    if (glide_start_time > 0) {
        glide_duration_s = (current_time - glide_start_time) / 1000.0f;
    }
    
    calc_data.drive_status = (int)current_state;  // 0=IDLE, 1=PULLING, 2=GLIDING
    calc_data.pull_duration_s = pull_duration_s;
    calc_data.glide_duration_s = glide_duration_s;
}

void DataCalculator::resetTotals() {
    accumulated_energy_j = 0.0f;
    accumulated_distance_m = 0.0f;
    total_moving_time_ms = 0;
    pull_duration_s = 0.0f;
    glide_duration_s = 0.0f;
    current_gear = 0;
    current_state = IDLE;
    
    CALC_LOGI(TAG, "Totals reset");
}

float DataCalculator::updateEMA(float &ema, float new_value, float alpha) {
    ema = alpha * new_value + (1.0f - alpha) * ema;
    return ema;
}

float DataCalculator::getMotorRPM() const {
    // Calculate motor RPM based on gear ratio and wheel RPM
    // This is a placeholder - actual value should come from VESC CAN data
    // For now return smoothed_gear_ratio as proxy
    return smoothed_gear_ratio * 100.0f;  // Scale to approximate RPM
}

// ============================================================================
// ENVIRONMENT SENSOR (WSEN_HIDS) IMPLEMENTATION
// ============================================================================

// WSEN_HIDS Commands
#define HIDS_ADDRESS 0x44
#define HIDS_MEASURE_HPM 0xFD
#define HIDS_SOFT_RESET 0x94
#define HIDS_MEASURE_SERIAL_NUMBER 0x89
#define CRC8_INIT 0xFF
#define CRC8_POLYNOMIAL 0x31
#define HIDS_WORD_SIZE 2

EnvironmentSensor::EnvironmentSensor() {
}

EnvironmentSensor::~EnvironmentSensor() {
}

bool EnvironmentSensor::init() {
    I2C_LOGI(TAG, "Initializing WSEN_HIDS sensor at address 0x%02X", HIDS_ADDRESS);
    
    // Try soft reset
    Wire.beginTransmission(HIDS_ADDRESS);
    Wire.write(HIDS_SOFT_RESET);
    int status = Wire.endTransmission();
    
    if (status != 0) {
        I2C_LOGE(TAG, "Failed to initialize WSEN_HIDS (status: %d)", status);
        return false;
    }
    
    delay(100);  // Wait for sensor to reset
    
    // Try to read serial number as verification
    Wire.beginTransmission(HIDS_ADDRESS);
    Wire.write(HIDS_MEASURE_SERIAL_NUMBER);
    status = Wire.endTransmission();
    
    if (status != 0) {
        I2C_LOGE(TAG, "Failed to read WSEN_HIDS serial (status: %d)", status);
        return false;
    }
    
    I2C_LOGI(TAG, "WSEN_HIDS sensor initialized successfully");
    return true;
}

bool EnvironmentSensor::readData(EnvData_t &env_data) {
    uint32_t current_time = millis();
    
    // Skip reading if too soon
    if (current_time - last_read_time < HIDS_TEMP_HUM_READ_INTERVAL_MS) {
        return false;
    }
    
    uint16_t t_ticks = 0;
    uint16_t rh_ticks = 0;
    
    // Send measurement command
    Wire.beginTransmission(HIDS_ADDRESS);
    Wire.write(HIDS_MEASURE_HPM);  // High precision measurement
    
    if (Wire.endTransmission() != 0) {
        I2C_LOGE(TAG, "Error sending WSEN_HIDS command");
        return false;
    }
    
    delay(15);  // Wait for measurement to complete
    
    // Read response (6 bytes: 2 data + 1 CRC + 2 data + 1 CRC)
    Wire.requestFrom(HIDS_ADDRESS, 6);
    
    if (Wire.available() < 6) {
        I2C_LOGE(TAG, "Not enough data from WSEN_HIDS");
        return false;
    }
    
    uint8_t dataBytes[6];
    for (uint8_t i = 0; i < 6; i++) {
        dataBytes[i] = Wire.read();
    }
    
    // Verify CRC for temperature
    if (checkCRC(&dataBytes[0], HIDS_WORD_SIZE, dataBytes[2]) != 0) {
        I2C_LOGE(TAG, "Temperature CRC check failed");
        return false;
    }
    
    // Verify CRC for humidity
    if (checkCRC(&dataBytes[3], HIDS_WORD_SIZE, dataBytes[5]) != 0) {
        I2C_LOGE(TAG, "Humidity CRC check failed");
        return false;
    }
    
    // Extract temperature ticks
    t_ticks = ((uint16_t)dataBytes[0] << 8) | ((uint16_t)dataBytes[1]);
    
    // Extract humidity ticks
    rh_ticks = ((uint16_t)dataBytes[3] << 8) | ((uint16_t)dataBytes[4]);
    
    // Convert to temperature (°C)
    int32_t t_raw = (int32_t)(((21875 * t_ticks) >> 13) - 45000);
    env_data.temperature = (float)t_raw / 1000.0f;
    
    // Convert to humidity (%)
    int32_t rh_raw = (int32_t)(((15625 * rh_ticks) >> 13) - 6000);
    env_data.humidity = (float)rh_raw / 1000.0f;
    
    env_data.timestamp_ms = current_time;
    last_read_time = current_time;
    
    I2C_LOGI(TAG, "WSEN_HIDS: Temp=%.2f°C, Humidity=%.2f%%", 
             env_data.temperature, env_data.humidity);
    
    return true;
}

uint8_t EnvironmentSensor::calculateCRC(const uint8_t* data, uint16_t count) {
    uint16_t current_byte;
    uint8_t crc = CRC8_INIT;
    uint8_t crc_bit;
    
    for (current_byte = 0; current_byte < count; ++current_byte) {
        crc ^= (data[current_byte]);
        for (crc_bit = 8; crc_bit > 0; --crc_bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

int8_t EnvironmentSensor::checkCRC(const uint8_t* data, uint16_t count, uint8_t checksum) {
    if (calculateCRC(data, count) != checksum) {
        return -1;  // CRC check failed
    }
    return 0;  // CRC check passed
}

// Global instances
DataCalculator g_data_calc;
EnvironmentSensor g_env_sensor;

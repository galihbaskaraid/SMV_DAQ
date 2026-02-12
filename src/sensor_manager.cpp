#include "sensor_manager.h"
#include "constants.h"
#include <MPU9250_WE.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>

// Critical section macro for interrupt safety
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================================
// GLOBAL SENSOR INSTANCES
// ============================================================================

MPU6500Manager g_mpu6500;
ADS1115Manager g_ads1115;
SpeedSensorManager g_speed_sensor;

// ============================================================================
// MPU6500 IMPLEMENTATION
// ============================================================================

static MPU6500_WE mpu6500 = MPU6500_WE(MPU6500_ADDRESS);

MPU6500Manager::MPU6500Manager() : initialized(false), data_mutex(nullptr) {}

MPU6500Manager::~MPU6500Manager() {
    if (data_mutex != nullptr) {
        vSemaphoreDelete(data_mutex);
    }
}

bool MPU6500Manager::init() {
    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ)) {
        ESP_LOGE(TAG_SENSOR, "I2C initialization failed");
        return false;
    }
    
    if (!mpu6500.init()) {
        ESP_LOGE(TAG_SENSOR, "MPU6500 initialization failed");
        return false;
    }
    
    // Configure MPU6500 - from JouleMeterTest pattern
    mpu6500.autoOffsets();
    mpu6500.enableGyrDLPF();
    mpu6500.setGyrDLPF(MPU9250_DLPF_6);
    mpu6500.setSampleRateDivider(5);
    mpu6500.setAccRange(MPU9250_ACC_RANGE_16G);
    mpu6500.enableAccDLPF(true);
    mpu6500.setAccDLPF(MPU9250_DLPF_6);
    
    ESP_LOGI(TAG_SENSOR, "MPU6500 initialized successfully");
    initialized = true;
    return true;
}

void MPU6500Manager::update() {
    if (!initialized) return;
    
    MPU6500Data_t data;
    
    // Get acceleration values - from JouleMeterTest pattern
    xyzFloat gValue = mpu6500.getGValues();  // g-force values
    data.accel.x = gValue.x;
    data.accel.y = gValue.y;
    data.accel.z = gValue.z;
    
    // Get gyroscope values - from JouleMeterTest pattern
    xyzFloat gyr = mpu6500.getGyrValues();
    data.gyro.x = gyr.x;
    data.gyro.y = gyr.y;
    data.gyro.z = gyr.z;
    
    // Get temperature
    data.temperature = mpu6500.getTemperature();
    data.timestamp_us = micros();
    
    if (data_mutex != nullptr && xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        g_data_sensor.mpu6500 = data;
        g_data_sensor.flags.mpu6500_valid = true;
        xSemaphoreGive(data_mutex);
    }
}

bool MPU6500Manager::getData(MPU6500Data_t &data) {
    if (data_mutex == nullptr) return false;
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        data = g_data_sensor.mpu6500;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

// ============================================================================
// ADS1115 IMPLEMENTATION
// ============================================================================

static Adafruit_ADS1115 ads;

ADS1115Manager::ADS1115Manager() 
    : initialized(false), data_mutex(nullptr), filter_index(0) {
    memset(voltage_filter, 0, sizeof(voltage_filter));
}

ADS1115Manager::~ADS1115Manager() {
    if (data_mutex != nullptr) {
        vSemaphoreDelete(data_mutex);
    }
}

bool ADS1115Manager::init() {
    if (!ads.begin(ADS1115_ADDRESS)) {
        ESP_LOGE(TAG_SENSOR, "ADS1115 initialization failed");
        return false;
    }
    
    // Configure gain and sample rate (from JouleMeterTest pattern)
    ads.setGain(GAIN_ONE);  // 1x gain ±4.096V range
    ads.setDataRate(RATE_ADS1115_860SPS);  // 860 samples per second
    
    // Perform calibration for current sensor offset
    calibrateCurrentSensor();
    
    ESP_LOGI(TAG_SENSOR, "ADS1115 initialized successfully");
    initialized = true;
    return true;
}

float ADS1115Manager::filterVoltage(float raw_voltage) {
    voltage_filter[filter_index] = raw_voltage;
    filter_index = (filter_index + 1) % 10;
    
    float sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += voltage_filter[i];
    }
    return sum / 10.0f;
}

float ADS1115Manager::calculateCurrent(float adc_voltage_mv) {
    // Apply offset calibration first (from JouleMeterTest pattern)
    float voltage_corrected = adc_voltage_mv - current_offset_mv;  // Remove zero offset
    float voltage_V = voltage_corrected / 1000.0f;  // mV to V
    
    // I = V / (R_shunt * Gain)
    // where V is in volts, R_shunt = 0.001 ohm, Gain = 20 (AD8418)
    float current = (voltage_V / (CURRENT_SHUNT_RES * CURRENT_AMP_GAIN));
    
    // Clamp negative values to zero
    if (current < 0) {
        current = 0;
    }
    
    return current;
}

void ADS1115Manager::calibrateCurrentSensor() {
    // Calibration method from JouleMeterTest - measures 0A offset at startup
    float current_sum = 0;
    float min_val = 4096.0f;
    float max_val = 0;
    
    ESP_LOGI(TAG_SENSOR, "Starting ADS1115 calibration (500 samples)...");
    
    // Take 500 samples with minimal delay between them
    for (int i = 0; i < 500; i++) {
        int16_t adc_raw = ads.readADC_SingleEnded(1);  // Read current channel (AIN1)
        float adc_voltage_v = ads.computeVolts(adc_raw);
        float adc_voltage_mv = adc_voltage_v * 1000.0f;
        
        current_sum += adc_voltage_mv;
        min_val = (adc_voltage_mv < min_val) ? adc_voltage_mv : min_val;
        max_val = (adc_voltage_mv > max_val) ? adc_voltage_mv : max_val;
        
        delayMicroseconds(50);  // Small delay between samples
    }
    
    // Calculate calibration offset
    current_offset_mv = current_sum / 500.0f;
    float noise_window = max_val - min_val;
    
    ESP_LOGI(TAG_SENSOR, "Calibration complete:");
    ESP_LOGI(TAG_SENSOR, "  Offset (0A reference): %.3f mV", current_offset_mv);
    ESP_LOGI(TAG_SENSOR, "  Min value: %.3f mV", min_val);
    ESP_LOGI(TAG_SENSOR, "  Max value: %.3f mV", max_val);
    ESP_LOGI(TAG_SENSOR, "  Noise window: %.3f mV", noise_window);
}

void ADS1115Manager::update() {
    if (!initialized) return;
    
    ADS1115Data_t data;
    
    // Read voltage from AIN0 (with voltage divider) - from JouleMeterTest pattern
    int16_t adc0 = ads.readADC_SingleEnded(0);
    data.raw_adc[0] = adc0;
    float voltage_adc_v = ads.computeVolts(adc0);
    
    // Apply low-pass filter (from JouleMeterTest pattern)
    static float volts_filtered = 0;
    // Low-pass filter macro: UTILS_LP_FAST(value, sample, filter_constant)
    volts_filtered = volts_filtered - 0.5f * (volts_filtered - voltage_adc_v);
    data.voltage = ((VDIV_R1 + VDIV_R2) / VDIV_R2) * volts_filtered;  // Apply voltage divider
    
    // Clamp voltage to valid range
    if (data.voltage < 0.1f) {
        data.voltage = 0;
    }
    
    // Read current from AIN1 (with AD8418 amplifier and shunt) - from JouleMeterTest
    int16_t adc1 = ads.readADC_SingleEnded(1);
    data.raw_adc[1] = adc1;
    float current_adc_v = ads.computeVolts(adc1);
    float current_adc_mv = current_adc_v * 1000.0f;
    data.current = calculateCurrent(current_adc_mv);
    
    data.timestamp_ms = millis();
    
    if (data_mutex != nullptr && xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        g_data_sensor.ads1115 = data;
        g_data_sensor.flags.ads1115_valid = true;
        xSemaphoreGive(data_mutex);
    }
}

bool ADS1115Manager::getData(ADS1115Data_t &data) {
    if (data_mutex == nullptr) return false;
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        data = g_data_sensor.ads1115;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

// ============================================================================
// SPEED SENSOR IMPLEMENTATION
// ============================================================================

// Speed Sensor Interrupt Service Routine
void IRAM_ATTR speedSensorISR(void *arg) {
    portENTER_CRITICAL_ISR(&mux);
    g_speed_sensor.handlePulse();
    portEXIT_CRITICAL_ISR(&mux);
}

SpeedSensorManager::SpeedSensorManager()
    : initialized(false), data_mutex(nullptr), pulse_count(0),
      last_pulse_time(0), last_update_time(0), filter_index(0) {
    memset(speed_filter, 0, sizeof(speed_filter));
}

SpeedSensorManager::~SpeedSensorManager() {
    if (data_mutex != nullptr) {
        vSemaphoreDelete(data_mutex);
    }
}

bool SpeedSensorManager::init() {
    pinMode(SPEED_SENSOR_PIN, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(SPEED_SENSOR_PIN), 
                       speedSensorISR, nullptr, SPEED_SENSOR_INTERRUPT);
    
    ESP_LOGI(TAG_SENSOR, "Speed sensor initialized on pin %d", SPEED_SENSOR_PIN);
    initialized = true;
    return true;
}

void SpeedSensorManager::handlePulse() {
    pulse_count++;
    last_pulse_time = micros();
}

float SpeedSensorManager::filterSpeed(float raw_speed) {
    speed_filter[filter_index] = raw_speed;
    filter_index = (filter_index + 1) % SPEED_FILTER_SAMPLES;
    
    float sum = 0;
    for (int i = 0; i < SPEED_FILTER_SAMPLES; i++) {
        sum += speed_filter[i];
    }
    return sum / SPEED_FILTER_SAMPLES;
}

void SpeedSensorManager::update() {
    if (!initialized) return;
    
    SpeedData_t data;
    uint32_t current_time = millis();
    
    // Calculate speed from pulse frequency - from JouleMeterTest pattern
    static uint32_t last_calc_time = 0;
    const uint32_t calc_interval_ms = 500;  // Calculate every 500ms
    
    if (current_time - last_calc_time >= calc_interval_ms) {
        portENTER_CRITICAL(&mux);
        uint32_t count = pulse_count;
        pulse_count = 0;
        portEXIT_CRITICAL(&mux);
        
        // Calculate speed based on wheel parameters
        // speed = (pulses / pulses_per_revolution) * wheel_circumference (m) / time (s) * 3.6 -> km/h
        float distance_m = (float)count / PULSES_PER_REVOLUTION * (WHEEL_CIRCUMFERENCE_MM);
        float raw_speed_kmh = (distance_m / (calc_interval_ms / 1000.0f)) * 3.6f;
        
        data.speed_kmh = filterSpeed(raw_speed_kmh);
        data.speed_ms = data.speed_kmh / 3.6f;
        data.pulse_count = count;
        data.last_pulse_us = last_pulse_time;
        data.timestamp_ms = current_time;
        
        last_calc_time = current_time;
        
        if (data_mutex != nullptr && xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
            g_data_sensor.speed = data;
            g_data_sensor.flags.speed_valid = true;
            xSemaphoreGive(data_mutex);
        }
    }
}

bool SpeedSensorManager::getData(SpeedData_t &data) {
    if (data_mutex == nullptr) return false;
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        data = g_data_sensor.speed;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

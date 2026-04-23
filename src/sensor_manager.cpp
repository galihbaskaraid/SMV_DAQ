#include "sensor_manager.h"
#include "constants.h"
#include "debug_logging.h"
#include <MPU9250_WE.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <cmath>

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
        I2C_LOGE(TAG_SENSOR, "I2C initialization failed");
        return false;
    }
    
    if (!mpu6500.init()) {
        I2C_LOGE(TAG_SENSOR, "MPU6500 initialization failed");
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
    
    I2C_LOGI(TAG_SENSOR, "MPU6500 initialized successfully");
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
        I2C_LOGE(TAG_SENSOR, "ADS1115 initialization failed");
        return false;
    }
    
    // Configure gain and sample rate (from JouleMeterTest pattern)
    ads.setGain(GAIN_TWOTHIRDS);  // 1x gain ±4.096V range
    ads.setDataRate(RATE_ADS1115_475SPS);  // 475 samples per second
    
    // Perform calibration for current sensor offset
    calibrateCurrentSensor();
    
    I2C_LOGI(TAG_SENSOR, "ADS1115 initialized successfully");
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
    
    I2C_LOGI(TAG_SENSOR, "Starting ADS1115 calibration (500 samples)...");
    
    // Take 500 samples with minimal delay between them
    for (int i = 0; i < 500; i++) {
        int16_t adc_raw = ads.readADC_SingleEnded(0);  // Read current channel (AIN0)
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
    
    I2C_LOGI(TAG_SENSOR, "Calibration complete:");
    I2C_LOGI(TAG_SENSOR, "  Offset (0A reference): %.3f mV", current_offset_mv);
    I2C_LOGI(TAG_SENSOR, "  Min value: %.3f mV", min_val);
    I2C_LOGI(TAG_SENSOR, "  Max value: %.3f mV", max_val);
    I2C_LOGI(TAG_SENSOR, "  Noise window: %.3f mV", noise_window);
}

void ADS1115Manager::update() {
    if (!initialized) return;
    
    ADS1115Data_t data;
    
    // Read voltage from AIN0 (with voltage divider) - from JouleMeterTest pattern
    int16_t adc0 = ads.readADC_SingleEnded(1);  // Read voltage channel (AIN1)
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
    
    // Read current from AIN0 (with AD8418 amplifier and shunt) - from JouleMeterTest
    int16_t adc1 = ads.readADC_SingleEnded(0);
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
// SPEED SENSOR IMPLEMENTATION (from JM_01122025 working algorithm)
// ============================================================================

// Volatile variables for ISR (pulse interval tracking - HIGH edge detection)
static volatile unsigned long pulseIntervalMicros = 0;
static volatile unsigned long lastPulseTimestampMicros = 0;
static volatile bool waiting_for_first_valid_pulse = true;
static volatile unsigned long last_processed_pulse_ts = 0;

// Speed Sensor Interrupt Service Routine - Detects HIGH edge (rising edge)
void IRAM_ATTR speedSensorISR(void *arg) {
    // Only process if transition is LOW -> HIGH (rising edge)
    if (digitalRead(SPEED_SENSOR_PIN) == HIGH) {
        unsigned long now_us = micros();
        
        portENTER_CRITICAL_ISR(&mux);
        unsigned long last_ts = lastPulseTimestampMicros;
        portEXIT_CRITICAL_ISR(&mux);

        // Filter debounce/max speed basic
        if ((now_us - last_ts) < MIN_PULSE_INTERVAL_MICROS) {
            return;
        }
        
        unsigned long interval = now_us - last_ts;
        
        portENTER_CRITICAL_ISR(&mux);
        pulseIntervalMicros = interval;
        lastPulseTimestampMicros = now_us;
        portEXIT_CRITICAL_ISR(&mux);
    }
}

// Inline function to calculate speed from pulse interval (from JM project)
inline float speed_from_interval(unsigned long interval_us) {
    if (interval_us == 0) return 0.0f;
    return ((1e6f / interval_us) / PPR) * WHEEL_CIRCUMFERENCE_M * 3.6f;
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
                       speedSensorISR, nullptr, CHANGE);
    
    SENSOR_LOGI(TAG_SENSOR, "Speed sensor initialized on pin %d with HIGH edge detection", SPEED_SENSOR_PIN);
    SENSOR_LOGI(TAG_SENSOR, "Config: PPR=%.0f, Wheel=%.2fm, Max=%.0f km/h, Interval=%ldms",
             PPR, WHEEL_CIRCUMFERENCE_M, SPEED_MAX_KMH, SPEED_CALC_INTERVAL_MS);
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
    
    // Blended speed calculation (from JM_01122025) - HIGH accuracy algorithm
    static uint32_t last_calc_time = 0;
    static float filtered_speed_ema = 0.0f;
    static unsigned long validation_memory_us = 0;
    
    if (current_time - last_calc_time >= SPEED_CALC_INTERVAL_MS) {
        uint32_t delta_time_ms = current_time - last_calc_time;
        last_calc_time = current_time;

        // Get latest pulse interval data safely
        portENTER_CRITICAL(&mux);
        unsigned long local_last_ts = lastPulseTimestampMicros;
        unsigned long local_interval = pulseIntervalMicros;
        portEXIT_CRITICAL(&mux);

        // Check if we have a new pulse since last calculation
        bool has_new_pulse = (local_last_ts > last_processed_pulse_ts);
        unsigned long interval_for_calc = 0;

        if (has_new_pulse) {
            last_processed_pulse_ts = local_last_ts;
            if (waiting_for_first_valid_pulse) {
                // First pulse, skip calculation to establish baseline
                interval_for_calc = 0;
                validation_memory_us = 0;
                waiting_for_first_valid_pulse = false;
            } else {
                // Validate interval against previous to filter outliers
                unsigned long validated_interval = local_interval;
                
                if (validation_memory_us > 0) {
                    // Check for unrealistic acceleration (misspulse)
                    if (validated_interval < validation_memory_us * ACCEL_VALIDATION_FACTOR) {
                        // Interval suddenly too short → likely misspulse, reject
                        validated_interval = validation_memory_us;
                    } else if (validated_interval > validation_memory_us * MISSPULSE_FACTOR) {
                        // Interval suddenly too long → likely misspulse, divide by 2
                        validated_interval /= 2;
                    }
                }
                interval_for_calc = validated_interval;
            }
            validation_memory_us = local_interval;
        } else {
            // No new pulse since last check
            if (current_time > (last_processed_pulse_ts / 1000) + SPEED_FREEZE_DURATION_MS) {
                // Timeout reached, freeze speed
                interval_for_calc = 0;
                validation_memory_us = 0;
                waiting_for_first_valid_pulse = true;
            } else {
                // Still within timeout, keep using last interval
                interval_for_calc = validation_memory_us;
            }
        }

        // Calculate blended speed (interval-based + counter-based)
        float v_blended;
        if (interval_for_calc == 0) {
            v_blended = 0.0f;
        } else {
            // Interval-based speed (faster response, less noise at high speeds)
            float v_interval = speed_from_interval(interval_for_calc);
            
            // Counter-based speed (accumulated pulses in interval - smoother at low speeds)
            float v_counter = 0.0f;
            if (delta_time_ms > 0 && pulse_count > 0) {
                v_counter = (((float)pulse_count / (delta_time_ms / 1000.0f)) / PPR) * WHEEL_CIRCUMFERENCE_M * 3.6f;
            } else {
                v_counter = v_interval;
            }
            
            // Blend based on speed range (smooth at low speed, interval-based at high speed)
            const float LOW_SPEED_THRESH_KMH = 8.0f;
            const float HIGH_SPEED_THRESH_KMH = 20.0f;
            float w = (v_interval <= LOW_SPEED_THRESH_KMH) ? 0.0f : 
                      (v_interval >= HIGH_SPEED_THRESH_KMH) ? 1.0f :
                      (v_interval - LOW_SPEED_THRESH_KMH) / (HIGH_SPEED_THRESH_KMH - LOW_SPEED_THRESH_KMH);
            v_blended = (1.0f - w) * v_interval + w * v_counter;
        }
        
        // Apply EMA (Exponential Moving Average) filtering with adaptive alpha
        if (v_blended == 0.0f) {
            filtered_speed_ema = 0.0f;
        } else {
            float alpha;
            float delta_speed = fabs(v_blended - filtered_speed_ema);
            
            if (delta_speed > HIGH_ACCEL_THRESHOLD_KMH) {
                // High acceleration detected → use responsive filter
                alpha = ALPHA_RESPONSIVE;
            } else if (delta_speed < STABLE_SPEED_THRESHOLD_KMH) {
                // Stable speed → use smooth filter
                alpha = ALPHA_SMOOTH;
            } else {
                // Normal acceleration → use default filter
                alpha = ALPHA_DEFAULT;
            }
            
            filtered_speed_ema = (1.0f - alpha) * filtered_speed_ema + alpha * v_blended;
        }

        // Final speed with hysteresis to eliminate noise at 0
        float final_speed = (filtered_speed_ema < 0.2f) ? 0.0f : filtered_speed_ema;
        
        data.speed_kmh = final_speed;
        data.speed_ms = data.speed_kmh / 3.6f;
        data.pulse_count = pulse_count;
        data.last_pulse_us = lastPulseTimestampMicros;
        data.timestamp_ms = current_time;
        
        // Reset pulse counter for next interval
        portENTER_CRITICAL(&mux);
        pulse_count = 0;
        portEXIT_CRITICAL(&mux);
        
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

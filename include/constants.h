#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

// ============================================================================
// SYSTEM & FREERTOS CONFIGURATION
// ============================================================================
// #define APP_CPU_NUM 1       // Already defined in ESP32 framework (soc.h)
// #define PRO_CPU_NUM 0       // Already defined in ESP32 framework (soc.h)
#define TASK_STACK_SIZE 4096
#define TASK_PRIORITY 3
#define GPS_TASK_PRIORITY 2
#define CAN_TASK_PRIORITY 3
#define WIFI_TASK_PRIORITY 2
#define SENSOR_READ_DELAY_MS 10
#define GPS_UPDATE_DELAY_MS 100

// ============================================================================
// GPIO CONFIGURATION
// ============================================================================
// I2C Pins (MPU6500 & ADS1115)
#define I2C_SDA_PIN GPIO_NUM_21
#define I2C_SCL_PIN GPIO_NUM_22
#define I2C_FREQ_HZ 400000

// SPI Pins (if needed for future expansion)
#define SPI_MOSI_PIN GPIO_NUM_23
#define SPI_MISO_PIN GPIO_NUM_19
#define SPI_CLK_PIN GPIO_NUM_18
#define SPI_CS_PIN GPIO_NUM_5

// UART GPS Configuration
#define UART_GPS UART_NUM_1
#define UART_RX_PIN GPIO_NUM_26
#define UART_TX_PIN UART_PIN_NO_CHANGE
#define UART_BAUD_RATE 9600
#define UART_BUF_SIZE 1024

// Speed Sensor (Interrupt Pin for Hall Effect / Pulse Counter)
#define SPEED_SENSOR_PIN GPIO_NUM_13
#define SPEED_SENSOR_INTERRUPT FALLING

// CAN Bus Configuration (TWAI - Two-Wire Automotive Interface)
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_SPEED 500000  // 500 kbps

// LED/Debug Pins
#define LED_PIN GPIO_NUM_27
#define LED_ON_MS 100
#define LED_OFF_MS 900

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================
// MPU6500/MPU9250 I2C Address
#define MPU6500_ADDRESS 0x68
#define MPU6500_SAMPLE_RATE 100  // Hz
#define MPU6500_ACCEL_SCALE 2    // +/- 2G
#define MPU6500_GYRO_SCALE 250   // +/- 250 degrees/sec

// ADS1115 Configuration
#define ADS1115_ADDRESS 0x48
#define ADS1115_GAIN GAIN_TWOTHIRDS  // +/- 6.144V
#define ADS1115_RATE RATE_ADS1115_128HZ
#define ADS1115_SAMPLES 10  // Number of samples to average

// Voltage Divider (AIN0)
#define VDIV_R1 39000.0f  // 10k ohm
#define VDIV_R2 2200.0f  // 10k ohm (adjust based on actual resistors)
#define VDIV_FACTOR (VDIV_R1 + VDIV_R2) / VDIV_R2

// Shunt Resistor with AD8418 (AIN1) - from JouleMeterTest
#define SHUNT_RESISTANCE 0.001f  // 1 mOhm (from JouleMeterTest)
#define AD8418_GAIN 20.0f        // 20 V/V (from JouleMeterTest AD8418)
#define ADS1115_LSB_VOLTAGE 0.1875e-3f  // 187.5 uV per LSB for +/- 6.144V

// Current and Voltage Measurement - from JouleMeterTest
#define CURRENT_SHUNT_RES 0.001f   // 1 mOhm shunt (75mV@20A)
#define CURRENT_AMP_GAIN 20.0f      // AD8418 20x amplifier gain

// GPS Configuration
#define GPS_BUFFER_SIZE 512
#define GPS_TIMEOUT_MS 5000
#define GPS_UPDATE_INTERVAL_MS 1000

// Speed Sensor Configuration (from JM_01122025 working reference)
#define PPR                        6.0f     // Pulses per revolution
#define WHEEL_CIRCUMFERENCE_M      1.54f    // Wheel circumference in meters (1.54m = 1540mm)
#define WHEEL_CIRCUMFERENCE_MM     (1.54f * 1000.0f)  // 1540mm
#define SPEED_MAX_KMH              35.0f    // Maximum reasonable speed
#define SPEED_FREEZE_DURATION_MS   1000     // Speed freeze timeout when no pulses
#define SPEED_CALC_INTERVAL_MS     220      // Calculate speed every 220ms (faster than 500ms)
#define SPEED_FILTER_SAMPLES       5        // EMA filter samples
#define MIN_PULSE_INTERVAL_MICROS  15000    // Debounce interval
#define ACCEL_VALIDATION_FACTOR    0.55f    // Acceleration validation
#define MISSPULSE_FACTOR           1.8f     // Misspulse detection
#define ALPHA_DEFAULT              0.35f    // Default EMA constant
#define ALPHA_RESPONSIVE           0.65f    // Responsive EMA (high acceleration)
#define ALPHA_SMOOTH               0.04f    // Smooth EMA (stable conditions)
#define HIGH_ACCEL_THRESHOLD_KMH   2.0f     // High acceleration threshold
#define STABLE_SPEED_THRESHOLD_KMH 0.5f     // Stable speed threshold

// ============================================================================
// COMMUNICATION CONFIGURATION
// ============================================================================
// WiFi
#define WIFI_SSID "RND13"
#define WIFI_PASSWORD "332211332211"
#define WIFI_MAX_RETRY 5
#define WIFI_CONNECT_TIMEOUT_MS 20000

// HTTP Server
#define HTTP_SERVER_PORT 80
#define HTTP_POST_INTERVAL_MS 5000
#define JSON_BUFFER_SIZE 1024

// CAN Bus Message IDs
#define CAN_ID_SENSOR_DATA 0x100
#define CAN_ID_GPS_DATA 0x101
#define CAN_ID_STATUS 0x102
#define CAN_ID_ERROR 0x103

// ============================================================================
// EQUATIONS & CALIBRATION
// ============================================================================
// Current Measurement from Shunt
#define CURRENT_FROM_VOLTAGE(voltage_adc_mv) \
    ((voltage_adc_mv) / AD8418_GAIN / (SHUNT_RESISTANCE * 1000))

// Speed Calculation (from pulse frequency - using updated parameters)
#define SPEED_FROM_FREQUENCY(freq_hz) \
    ((freq_hz * WHEEL_CIRCUMFERENCE_M / PPR) * 3.6f)  // m/s to km/h

// Temperature Compensation (basic linear)
#define TEMP_COMPENSATED_VALUE(value, temp_ref, temp_actual, coeff) \
    ((value) * (1 + (coeff) * ((temp_actual) - (temp_ref))))

// Voltage to Battery Percentage (simple linear, adjust for your battery chemistry)
#define BATTERY_PERCENT_FROM_VOLTAGE(voltage_v, min_v, max_v) \
    ((((voltage_v) - (min_v)) / ((max_v) - (min_v))) * 100.0f)

// ============================================================================
// LOGGING TAGS
// ============================================================================
#define TAG_SYSTEM "DAQ_SYSTEM"
#define TAG_SENSOR "SENSOR_MGR"
#define TAG_GPS "GPS_DRIVER"
#define TAG_CAN "CAN_BUS"
#define TAG_WIFI "WIFI_MGR"
#define TAG_HTTP "HTTP_POST"
#define TAG_ERROR "ERROR_HDL"

// ============================================================================
// ENERGY & POWER CALCULATION
// ============================================================================
#define POWER_CALC_INTERVAL_MS 100     // Calculate power every 100ms
#define ENERGY_CALC_INTERVAL_MS 1000   // Calculate energy every 1 second
#define CONSUMPTION_CALC_INTERVAL_MS 1000  // Calculate consumption every 1 second

// Energy conversion factors
#define JOULES_TO_KJ(joules) ((joules) / 1000.0f)
#define JOULES_TO_KWH(joules) ((joules) / 3600000.0f)
#define KJ_TO_KWH(kj) ((kj) / 3600.0f)

// ============================================================================
// GEAR DETECTION & DRIVE STATE
// ============================================================================
#define GEAR_CONFIRMATION_TIME_MS 100  // Time to confirm gear change
#define ACCEL_VALIDATION_FACTOR 0.55f
#define MISSPULSE_FACTOR 1.8f
#define ALPHA_DEFAULT 0.35f            // Default exponential moving average
#define ALPHA_RESPONSIVE 0.65f         // Responsive EMA for high acceleration
#define ALPHA_SMOOTH 0.04f             // Smooth EMA for stable conditions
#define HIGH_ACCEL_THRESHOLD_KMH 2.0f
#define STABLE_SPEED_THRESHOLD_KMH 0.5f
#define MAX_REASONABLE_SPEED_KMH 35.0f



// ============================================================================
// WSEN_HIDS SENSOR (Temperature/Humidity)
// ============================================================================
#define HIDS_ADDRESS 0x44               // I2C address for WSEN_HIDS
#define HIDS_TEMP_HUM_READ_INTERVAL_MS 1000  // Read interval

#endif // CONSTANTS_H

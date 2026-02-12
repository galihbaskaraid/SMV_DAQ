#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

// ============================================================================
// SYSTEM & FREERTOS CONFIGURATION
// ============================================================================
// #define APP_CPU_NUM 1
// #define PRO_CPU_NUM 0
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
#define SPEED_SENSOR_PIN GPIO_NUM_27
#define SPEED_SENSOR_INTERRUPT RISING

// CAN Bus Configuration (TWAI - Two-Wire Automotive Interface)
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_SPEED 500000  // 500 kbps

// LED/Debug Pins
#define LED_PIN LED_BUILTIN
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

// Speed Sensor Configuration
#define WHEEL_CIRCUMFERENCE_MM 0.1f  // Adjust based on your wheel
#define PULSES_PER_REVOLUTION 4         // Pulses per wheel revolution
#define SPEED_FILTER_SAMPLES 5

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

// Speed Calculation (from pulse frequency)
#define SPEED_FROM_FREQUENCY(freq_hz) \
    ((freq_hz * WHEEL_CIRCUMFERENCE_MM / PULSES_PER_REVOLUTION) / 1000.0f)  // mm/s to m/s

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

#endif // CONSTANTS_H

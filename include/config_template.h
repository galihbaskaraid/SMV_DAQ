#ifndef CONFIG_TEMPLATE_H
#define CONFIG_TEMPLATE_H

// ============================================================================
// CONFIGURATION TEMPLATE
// Copy and customize these values based on your hardware setup
// ============================================================================

// ============================================================================
// HARDWARE CALIBRATION
// ============================================================================

// Voltage Divider Calibration (AIN0)
// Measure actual resistors and update:
// #define VDIV_R1 10200.0f  // Measured value in Ohms
// #define VDIV_R2 9800.0f   // Measured value in Ohms

// Current Measurement Calibration (AIN1)
// Verify with known current source:
// #define SHUNT_RESISTANCE 0.0101f  // Measured shunt value
// #define AD8418_GAIN 99.5f         // Measured amplifier gain

// Wheel Calibration
// Measure actual wheel circumference:
// Method: Mark starting point, roll wheel one complete rotation,
//         measure distance traveled
// #define WHEEL_CIRCUMFERENCE_MM 2195.0f

// ============================================================================
// GPS CALIBRATION
// ============================================================================

// Antenna Phase Center Offset
// #define GPS_ANTENNA_OFFSET_NORTH 50.0f  // mm
// #define GPS_ANTENNA_OFFSET_EAST  20.0f  // mm
// #define GPS_ANTENNA_OFFSET_UP    100.0f // mm

// ============================================================================
// SENSOR FILTER TUNING
// ============================================================================

// Accelerometer Kalman Filter
// #define ACCEL_PROCESS_NOISE 0.01f
// #define ACCEL_MEASUREMENT_NOISE 0.1f

// Speed Filter Samples (higher = smoother but slower response)
// #define SPEED_FILTER_SAMPLES 5

// GPS Position Filter
// #define GPS_POSITION_FILTER_SIZE 10

// ============================================================================
// COMMUNICATION SETTINGS
// ============================================================================

// WiFi Configuration
// #define WIFI_SSID "Your_Network"
// #define WIFI_PASSWORD "Your_Password"
// #define HTTP_SERVER_URL "http://192.168.1.100:8080/api/data"
// #define HTTP_POST_INTERVAL_MS 5000

// CAN Bus Node ID
// #define CAN_NODE_ID 0x01

// ============================================================================
// DATA LOGGING
// ============================================================================

// Log Data to SPIFFS (File system)
// #define ENABLE_DATA_LOGGING 1
// #define LOG_DIRECTORY "/spiffs/logs"
// #define LOG_FILE_SIZE_KB 512

// Enable USB Serial Logging
// #define ENABLE_SERIAL_DEBUG 1
// #define SERIAL_BAUD_RATE 115200

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

// Battery Configuration (for AIN0 voltage monitoring)
// #define BATTERY_TYPE "LiPo"      // "LiPo", "LiFe", "Lead-Acid"
// #define BATTERY_MIN_VOLTAGE 2.5f  // Cutoff voltage
// #define BATTERY_MAX_VOLTAGE 4.2f  // Full charge voltage
// #define BATTERY_CELL_COUNT 1

// Low Power Mode
// #define ENABLE_LOW_POWER_MODE 1
// #define LOW_POWER_THRESHOLD_PERCENT 15

// ============================================================================
// EXAMPLE: COMPLETE CONFIGURATION FOR YOUR SYSTEM
// ============================================================================

/*

#define VDIV_R1 10200.0f
#define VDIV_R2 9800.0f
#define SHUNT_RESISTANCE 0.0101f
#define AD8418_GAIN 99.5f

#define WHEEL_CIRCUMFERENCE_MM 2195.0f
#define PULSES_PER_REVOLUTION 1

#define WIFI_SSID "Home_Network"
#define WIFI_PASSWORD "SecurePassword123"
#define HTTP_SERVER_URL "http://192.168.1.100:8080/api/sensor/data"
#define HTTP_POST_INTERVAL_MS 5000

#define BATTERY_MIN_VOLTAGE 2.5f
#define BATTERY_MAX_VOLTAGE 4.2f

#define ENABLE_DATA_LOGGING 1
#define ENABLE_SERIAL_DEBUG 1

*/

#endif // CONFIG_TEMPLATE_H

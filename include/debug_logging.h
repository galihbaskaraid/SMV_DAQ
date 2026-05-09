#ifndef DEBUG_LOGGING_H
#define DEBUG_LOGGING_H

#include <Arduino.h>
#include <esp_log.h>

// ============================================================================
// COMPILE-TIME DEBUG SWITCHES (0 = removed at compile, 1 = available)
// ============================================================================
#ifndef DBG_COMPILE_SYSTEM
#define DBG_COMPILE_SYSTEM 1
#endif

#ifndef DBG_COMPILE_I2C
#define DBG_COMPILE_I2C 0
#endif

#ifndef DBG_COMPILE_SPI
#define DBG_COMPILE_SPI 0
#endif

#ifndef DBG_COMPILE_UART
#define DBG_COMPILE_UART 0
#endif

#ifndef DBG_COMPILE_CAN
#define DBG_COMPILE_CAN 0
#endif

#ifndef DBG_COMPILE_GPS
#define DBG_COMPILE_GPS 1
#endif

#ifndef DBG_COMPILE_WIFI
#define DBG_COMPILE_WIFI 0
#endif

#ifndef DBG_COMPILE_HTTP
#define DBG_COMPILE_HTTP 0
#endif

#ifndef DBG_COMPILE_BLE
#define DBG_COMPILE_BLE 1
#endif

#ifndef DBG_COMPILE_SENSOR
#define DBG_COMPILE_SENSOR 0
#endif

#ifndef DBG_COMPILE_CALC
#define DBG_COMPILE_CALC 0
#endif

// ============================================================================
// RUNTIME DEBUG FLAGS (set false to suppress logs while running)
// ============================================================================
typedef struct {
    bool system_if;
    bool i2c;
    bool spi;
    bool uart;
    bool can_bus;
    bool gps;
    bool wifi;
    bool http;
    bool ble;
    bool sensor;
    bool calc;
} DebugRuntimeFlags;

extern DebugRuntimeFlags g_debug_runtime;

void setAllDebugInterfaces(bool enabled);

// ============================================================================
// INTERFACE-SPECIFIC LOG MACROS
// ============================================================================
#if DBG_COMPILE_SYSTEM
#define SYSTEM_LOGI(tag, fmt, ...) do { if (g_debug_runtime.system_if) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define SYSTEM_LOGW(tag, fmt, ...) do { if (g_debug_runtime.system_if) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define SYSTEM_LOGE(tag, fmt, ...) do { if (g_debug_runtime.system_if) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define SYSTEM_LOGD(tag, fmt, ...) do { if (g_debug_runtime.system_if) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define SYSTEM_LOGI(...) do { } while (0)
#define SYSTEM_LOGW(...) do { } while (0)
#define SYSTEM_LOGE(...) do { } while (0)
#define SYSTEM_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_I2C
#define I2C_LOGI(tag, fmt, ...) do { if (g_debug_runtime.i2c) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define I2C_LOGW(tag, fmt, ...) do { if (g_debug_runtime.i2c) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define I2C_LOGE(tag, fmt, ...) do { if (g_debug_runtime.i2c) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define I2C_LOGD(tag, fmt, ...) do { if (g_debug_runtime.i2c) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define I2C_LOGI(...) do { } while (0)
#define I2C_LOGW(...) do { } while (0)
#define I2C_LOGE(...) do { } while (0)
#define I2C_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_UART
#define UART_LOGI(tag, fmt, ...) do { if (g_debug_runtime.uart) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define UART_LOGW(tag, fmt, ...) do { if (g_debug_runtime.uart) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define UART_LOGE(tag, fmt, ...) do { if (g_debug_runtime.uart) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define UART_LOGD(tag, fmt, ...) do { if (g_debug_runtime.uart) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define UART_LOGI(...) do { } while (0)
#define UART_LOGW(...) do { } while (0)
#define UART_LOGE(...) do { } while (0)
#define UART_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_CAN
#define CAN_LOGI(tag, fmt, ...) do { if (g_debug_runtime.can_bus) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define CAN_LOGW(tag, fmt, ...) do { if (g_debug_runtime.can_bus) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define CAN_LOGE(tag, fmt, ...) do { if (g_debug_runtime.can_bus) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define CAN_LOGD(tag, fmt, ...) do { if (g_debug_runtime.can_bus) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define CAN_LOGI(...) do { } while (0)
#define CAN_LOGW(...) do { } while (0)
#define CAN_LOGE(...) do { } while (0)
#define CAN_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_GPS
#define GPS_LOGI(tag, fmt, ...) do { if (g_debug_runtime.gps) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define GPS_LOGW(tag, fmt, ...) do { if (g_debug_runtime.gps) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define GPS_LOGE(tag, fmt, ...) do { if (g_debug_runtime.gps) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define GPS_LOGD(tag, fmt, ...) do { if (g_debug_runtime.gps) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define GPS_LOGI(...) do { } while (0)
#define GPS_LOGW(...) do { } while (0)
#define GPS_LOGE(...) do { } while (0)
#define GPS_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_WIFI
#define WIFI_LOGI(tag, fmt, ...) do { if (g_debug_runtime.wifi) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define WIFI_LOGW(tag, fmt, ...) do { if (g_debug_runtime.wifi) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define WIFI_LOGE(tag, fmt, ...) do { if (g_debug_runtime.wifi) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define WIFI_LOGD(tag, fmt, ...) do { if (g_debug_runtime.wifi) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define WIFI_LOGI(...) do { } while (0)
#define WIFI_LOGW(...) do { } while (0)
#define WIFI_LOGE(...) do { } while (0)
#define WIFI_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_HTTP
#define HTTP_LOGI(tag, fmt, ...) do { if (g_debug_runtime.http) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define HTTP_LOGW(tag, fmt, ...) do { if (g_debug_runtime.http) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define HTTP_LOGE(tag, fmt, ...) do { if (g_debug_runtime.http) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define HTTP_LOGD(tag, fmt, ...) do { if (g_debug_runtime.http) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define HTTP_LOGI(...) do { } while (0)
#define HTTP_LOGW(...) do { } while (0)
#define HTTP_LOGE(...) do { } while (0)
#define HTTP_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_BLE
#define BLE_LOGI(tag, fmt, ...) do { if (g_debug_runtime.ble) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define BLE_LOGW(tag, fmt, ...) do { if (g_debug_runtime.ble) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define BLE_LOGE(tag, fmt, ...) do { if (g_debug_runtime.ble) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define BLE_LOGD(tag, fmt, ...) do { if (g_debug_runtime.ble) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define BLE_LOGI(...) do { } while (0)
#define BLE_LOGW(...) do { } while (0)
#define BLE_LOGE(...) do { } while (0)
#define BLE_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_SENSOR
#define SENSOR_LOGI(tag, fmt, ...) do { if (g_debug_runtime.sensor) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define SENSOR_LOGW(tag, fmt, ...) do { if (g_debug_runtime.sensor) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define SENSOR_LOGE(tag, fmt, ...) do { if (g_debug_runtime.sensor) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define SENSOR_LOGD(tag, fmt, ...) do { if (g_debug_runtime.sensor) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define SENSOR_LOGI(...) do { } while (0)
#define SENSOR_LOGW(...) do { } while (0)
#define SENSOR_LOGE(...) do { } while (0)
#define SENSOR_LOGD(...) do { } while (0)
#endif

#if DBG_COMPILE_CALC
#define CALC_LOGI(tag, fmt, ...) do { if (g_debug_runtime.calc) { ESP_LOGI(tag, fmt, ##__VA_ARGS__); } } while (0)
#define CALC_LOGW(tag, fmt, ...) do { if (g_debug_runtime.calc) { ESP_LOGW(tag, fmt, ##__VA_ARGS__); } } while (0)
#define CALC_LOGE(tag, fmt, ...) do { if (g_debug_runtime.calc) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); } } while (0)
#define CALC_LOGD(tag, fmt, ...) do { if (g_debug_runtime.calc) { ESP_LOGD(tag, fmt, ##__VA_ARGS__); } } while (0)
#else
#define CALC_LOGI(...) do { } while (0)
#define CALC_LOGW(...) do { } while (0)
#define CALC_LOGE(...) do { } while (0)
#define CALC_LOGD(...) do { } while (0)
#endif

#endif // DEBUG_LOGGING_H

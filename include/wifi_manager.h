#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include "data_sensor.h"

// ============================================================================
// WIFI MANAGER
// ============================================================================

class WiFiManager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;
    bool connected;
    
public:
    WiFiManager();
    ~WiFiManager();
    
    bool init();
    void deinit();
    bool connect();
    void disconnect();
    bool isConnected() const { return connected; }
    bool sendDataViaHTTP(const DataSensor_t &sensor_data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
    
private:
    bool createJSONPayload(const DataSensor_t &sensor_data, char* json_buffer, size_t buffer_size);
    bool performHTTPPost(const char* json_data);
};

// ============================================================================
// WIFI TASK DECLARATION
// ============================================================================

void wifiTask(void *pvParameters);

// ============================================================================
// GLOBAL WIFI MANAGER INSTANCE
// ============================================================================

extern WiFiManager g_wifi_manager;
extern SemaphoreHandle_t g_data_sensor_mutex;

#endif // WIFI_MANAGER_H

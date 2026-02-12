#include "wifi_manager.h"
#include "constants.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ============================================================================
// GLOBAL WIFI MANAGER INSTANCE
// ============================================================================

WiFiManager g_wifi_manager;

// ============================================================================
// WIFI MANAGER IMPLEMENTATION
// ============================================================================

WiFiManager::WiFiManager() : initialized(false), connected(false), data_mutex(nullptr) {}

WiFiManager::~WiFiManager() {
    deinit();
}

bool WiFiManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    
    ESP_LOGI(TAG_WIFI, "WiFi manager initialized");
    initialized = true;
    return true;
}

void WiFiManager::deinit() {
    if (connected) {
        disconnect();
    }
    WiFi.mode(WIFI_OFF);
    initialized = false;
}

bool WiFiManager::connect() {
    if (!initialized) {
        ESP_LOGE(TAG_WIFI, "WiFi manager not initialized");
        return false;
    }
    
    ESP_LOGI(TAG_WIFI, "Connecting to WiFi: %s", WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    uint32_t start_time = millis();
    uint8_t retry_count = 0;
    
    while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_MAX_RETRY) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry_count++;
        
        if (millis() - start_time > WIFI_CONNECT_TIMEOUT_MS) {
            break;
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        ESP_LOGI(TAG_WIFI, "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
        return true;
    } else {
        connected = false;
        ESP_LOGE(TAG_WIFI, "Failed to connect to WiFi after %d retries", WIFI_MAX_RETRY);
        return false;
    }
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);  // true = turn off WiFi radio
    connected = false;
    ESP_LOGI(TAG_WIFI, "WiFi disconnected");
}

bool WiFiManager::createJSONPayload(const DataSensor_t &sensor_data, 
                                     char* json_buffer, size_t buffer_size) {
    JsonDocument doc;  // Use JsonDocument instead of StaticJsonDocument
    
    // Timestamp
    doc["timestamp_ms"] = millis();
    doc["unix_time"] = time(nullptr);
    
    // MPU6500 Data
    if (sensor_data.flags.mpu6500_valid) {
        JsonObject mpu = doc["mpu6500"].to<JsonObject>();
        mpu["accel_x"] = sensor_data.mpu6500.accel.x;
        mpu["accel_y"] = sensor_data.mpu6500.accel.y;
        mpu["accel_z"] = sensor_data.mpu6500.accel.z;
        mpu["gyro_x"] = sensor_data.mpu6500.gyro.x;
        mpu["gyro_y"] = sensor_data.mpu6500.gyro.y;
        mpu["gyro_z"] = sensor_data.mpu6500.gyro.z;
        mpu["temperature"] = sensor_data.mpu6500.temperature;
    }
    
    // ADS1115 Data
    if (sensor_data.flags.ads1115_valid) {
        JsonObject ads = doc["ads1115"].to<JsonObject>();
        ads["voltage_v"] = sensor_data.ads1115.voltage;
        ads["current_a"] = sensor_data.ads1115.current;
    }
    
    // GPS Data
    if (sensor_data.flags.gps_valid) {
        JsonObject gps = doc["gps"].to<JsonObject>();
        gps["latitude"] = sensor_data.gps.latitude;
        gps["longitude"] = sensor_data.gps.longitude;
        gps["altitude"] = sensor_data.gps.altitude;
        gps["speed_kmh"] = sensor_data.gps.speed_kmh;
        gps["satellites"] = sensor_data.gps.satellites;
        gps["fix_quality"] = sensor_data.gps.fix_quality;
        gps["utc_time"] = sensor_data.gps.utc_time;
    }
    
    // Speed Data
    if (sensor_data.flags.speed_valid) {
        JsonObject speed = doc["speed"].to<JsonObject>();
        speed["speed_kmh"] = sensor_data.speed.speed_kmh;
        speed["speed_ms"] = sensor_data.speed.speed_ms;
        speed["pulse_count"] = sensor_data.speed.pulse_count;
    }
    
    // System Status
    {
        JsonObject status = doc["status"].to<JsonObject>();
        status["wifi_connected"] = sensor_data.status.wifi_connected;
        status["gps_locked"] = sensor_data.status.gps_locked;
        status["heap_free"] = sensor_data.status.heap_free;
        status["uptime_ms"] = sensor_data.status.uptime_ms;
    }
    
    // Serialize to string
    size_t written = serializeJson(doc, json_buffer, buffer_size);
    
    if (written == 0) {
        ESP_LOGE(TAG_HTTP, "JSON serialization failed");
        return false;
    }
    
    return true;
}

bool WiFiManager::performHTTPPost(const char* json_data) {
    if (!connected) {
        ESP_LOGW(TAG_HTTP, "WiFi not connected");
        return false;
    }
    
    HTTPClient http;
    
    // Configure HTTP client
    String url = "http://192.168.1.100:8080/api/sensor/data";  // Update with your server
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Send POST request
    int http_response_code = http.POST(json_data);
    
    if (http_response_code > 0) {
        ESP_LOGI(TAG_HTTP, "HTTP Response: %d", http_response_code);
        
        if (http_response_code == HTTP_CODE_OK) {
            String response = http.getString();
            ESP_LOGD(TAG_HTTP, "Response: %s", response.c_str());
            http.end();
            return true;
        }
    } else {
        ESP_LOGE(TAG_HTTP, "HTTP POST failed, error: %s", http.errorToString(http_response_code).c_str());
    }
    
    http.end();
    return false;
}

bool WiFiManager::sendDataViaHTTP(const DataSensor_t &sensor_data) {
    if (!initialized || !connected) {
        return false;
    }
    
    static char json_buffer[JSON_BUFFER_SIZE];
    
    if (!createJSONPayload(sensor_data, json_buffer, sizeof(json_buffer))) {
        ESP_LOGE(TAG_HTTP, "Failed to create JSON payload");
        return false;
    }
    
    ESP_LOGD(TAG_HTTP, "JSON: %s", json_buffer);
    
    return performHTTPPost(json_buffer);
}

// ============================================================================
// WIFI FREERTOS TASK
// ============================================================================

void wifiTask(void *pvParameters) {
    ESP_LOGI(TAG_WIFI, "WiFi task started");
    
    uint32_t last_post_time = 0;
    
    while (1) {
        // Check WiFi connection status
        if (WiFi.status() != WL_CONNECTED) {
            ESP_LOGW(TAG_WIFI, "WiFi disconnected, attempting to reconnect...");
            g_wifi_manager.connect();
        }
        
        // Send data periodically
        uint32_t current_time = millis();
        if (current_time - last_post_time >= HTTP_POST_INTERVAL_MS) {
            if (g_wifi_manager.sendDataViaHTTP(g_data_sensor)) {
                ESP_LOGI(TAG_HTTP, "Data sent successfully");
                last_post_time = current_time;
            }
        }
        
        // Update WiFi status in global data (thread-safe)
        if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            g_data_sensor.status.wifi_connected = g_wifi_manager.isConnected();
            xSemaphoreGive(g_data_sensor_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(nullptr);
}

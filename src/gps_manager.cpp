#include "gps_manager.h"
#include "constants.h"
#include <driver/uart.h>
#include <cstring>
#include <cstdlib>

// ============================================================================
// GLOBAL GPS MANAGER INSTANCE
// ============================================================================

GPSManager g_gps_manager;

// ============================================================================
// GPS MANAGER IMPLEMENTATION
// ============================================================================

GPSManager::GPSManager()
    : data_mutex(nullptr), uart_mutex(nullptr), buffer_index(0), initialized(false) {
    memset(gps_buffer, 0, sizeof(gps_buffer));
}

GPSManager::~GPSManager() {
    deinit();
}

bool GPSManager::init() {
    // UART configuration
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB
    };
    
    if (uart_param_config(UART_GPS, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG_GPS, "UART param config failed");
        return false;
    }
    
    if (uart_set_pin(UART_GPS, UART_TX_PIN, UART_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG_GPS, "UART set pin failed");
        return false;
    }
    
    if (uart_driver_install(UART_GPS, UART_BUF_SIZE, 0, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(TAG_GPS, "UART driver install failed");
        return false;
    }
    
    ESP_LOGI(TAG_GPS, "GPS UART initialized successfully");
    initialized = true;
    return true;
}

void GPSManager::deinit() {
    if (initialized) {
        uart_driver_delete(UART_GPS);
        initialized = false;
    }
}

void GPSManager::handleUARTData() {
    if (!initialized) return;
    
    uint8_t data_byte;
    size_t bytes_available = 0;
    
    uart_get_buffered_data_len(UART_GPS, &bytes_available);
    
    while (bytes_available > 0) {
        if (uart_read_bytes(UART_GPS, &data_byte, 1, pdMS_TO_TICKS(100)) > 0) {
            if (data_byte == '\n') {
                // Complete sentence received
                gps_buffer[buffer_index] = '\0';
                
                // Parse NMEA sentences
                if (strstr(gps_buffer, "$GPRMC") != nullptr) {
                    parseGPRMC(gps_buffer);
                } else if (strstr(gps_buffer, "$GPGGA") != nullptr) {
                    parseGPGGA(gps_buffer);
                }
                
                buffer_index = 0;
                memset(gps_buffer, 0, sizeof(gps_buffer));
            } else if (data_byte != '\r' && buffer_index < sizeof(gps_buffer) - 1) {
                gps_buffer[buffer_index++] = (char)data_byte;
            }
            
            uart_get_buffered_data_len(UART_GPS, &bytes_available);
        } else {
            break;
        }
    }
}

uint8_t GPSManager::calculateChecksum(const char* sentence) {
    uint8_t checksum = 0;
    const char* ptr = sentence;
    
    if (*ptr == '$') ptr++;
    if (*ptr == '*') return 0;
    
    while (*ptr && *ptr != '*') {
        checksum ^= (uint8_t)*ptr;
        ptr++;
    }
    
    return checksum;
}

bool GPSManager::validateChecksum(const char* sentence) {
    const char* checksum_ptr = strchr(sentence, '*');
    if (checksum_ptr == nullptr) return false;
    
    uint8_t expected = calculateChecksum(sentence);
    uint8_t received = (uint8_t)strtol(checksum_ptr + 1, nullptr, 16);
    
    return expected == received;
}

float GPSManager::parseCoordinate(const char* coord, const char* direction) {
    if (coord == nullptr || direction == nullptr) return 0.0f;
    
    char coord_copy[16];
    strncpy(coord_copy, coord, sizeof(coord_copy) - 1);
    
    // Find decimal point
    char* dot_ptr = strchr(coord_copy, '.');
    if (dot_ptr == nullptr) return 0.0f;
    
    // Split into degrees and minutes
    int dot_pos = dot_ptr - coord_copy;
    int degree_digits = (strchr(direction, 'N') || strchr(direction, 'S')) ? 2 : 3;
    
    char deg_str[10], min_str[10];
    strncpy(deg_str, coord_copy, dot_pos - 2);
    deg_str[dot_pos - 2] = '\0';
    strncpy(min_str, coord_copy + dot_pos - 2, sizeof(min_str) - 1);
    
    float degrees = atof(deg_str);
    float minutes = atof(min_str);
    float result = degrees + (minutes / 60.0f);
    
    if (strchr(direction, 'S') || strchr(direction, 'W')) {
        result = -result;
    }
    
    return result;
}

bool GPSManager::parseTime(const char* time_str, char* utc_time) {
    if (time_str == nullptr || utc_time == nullptr) return false;
    strncpy(utc_time, time_str, 11);
    utc_time[11] = '\0';
    return true;
}

bool GPSManager::parseDate(const char* date_str, char* utc_date) {
    if (date_str == nullptr || utc_date == nullptr) return false;
    strncpy(utc_date, date_str, 6);
    utc_date[6] = '\0';
    return true;
}

float GPSManager::parseSpeed(const char* speed_str) {
    if (speed_str == nullptr) return 0.0f;
    return atof(speed_str);
}

void GPSManager::parseGPRMC(const char* sentence) {
    if (!validateChecksum(sentence)) {
        ESP_LOGD(TAG_GPS, "GPRMC checksum error");
        return;
    }
    
    if (data_mutex == nullptr) return;
    
    // Acquire lock BEFORE reading g_data_sensor
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    
    // $GPRMC,hhmmss.ss,status,lat,N/S,lon,E/W,speed,course,date,*checksum
    char sentence_copy[256];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    
    char* token = strtok(sentence_copy, ",");  // $GPRMC
    if (token == nullptr) {
        xSemaphoreGive(data_mutex);
        return;
    }
    
    // Parse tokens
    GPSData_t data = g_data_sensor.gps;
    
    token = strtok(nullptr, ",");  // Time
    if (token) parseTime(token, data.utc_time);
    
    token = strtok(nullptr, ",");  // Status (A=active)
    data.data_valid = (token && token[0] == 'A');
    
    token = strtok(nullptr, ",");  // Latitude
    char* lat_dir = strtok(nullptr, ",");
    if (token && lat_dir) {
        data.latitude = parseCoordinate(token, lat_dir);
    }
    
    token = strtok(nullptr, ",");  // Longitude
    char* lon_dir = strtok(nullptr, ",");
    if (token && lon_dir) {
        data.longitude = parseCoordinate(token, lon_dir);
    }
    
    token = strtok(nullptr, ",");  // Speed in knots
    if (token) {
        data.speed_kts = parseSpeed(token);
        data.speed_kmh = data.speed_kts * 1.852f;
    }
    
    token = strtok(nullptr, ",");  // Course
    if (token) data.course = parseSpeed(token);
    
    token = strtok(nullptr, ",");  // Date
    if (token) parseDate(token, data.utc_date);
    
    data.timestamp_ms = millis();
    
    g_data_sensor.gps = data;
    g_data_sensor.flags.gps_valid = data.data_valid;
    xSemaphoreGive(data_mutex);
}

void GPSManager::parseGPGGA(const char* sentence) {
    if (!validateChecksum(sentence)) {
        ESP_LOGD(TAG_GPS, "GPGGA checksum error");
        return;
    }
    
    if (data_mutex == nullptr) return;
    
    // Acquire lock BEFORE reading g_data_sensor
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    
    // $GPGGA,time,lat,N/S,lon,E/W,fix_quality,num_satellites,hdop,altitude,*checksum
    char sentence_copy[256];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    
    char* token = strtok(sentence_copy, ",");  // $GPGGA
    if (token == nullptr) {
        xSemaphoreGive(data_mutex);
        return;
    }
    
    GPSData_t data = g_data_sensor.gps;
    
    token = strtok(nullptr, ",");  // Time
    if (token) parseTime(token, data.utc_time);
    
    token = strtok(nullptr, ",");  // Latitude
    char* lat_dir = strtok(nullptr, ",");
    if (token && lat_dir) {
        data.latitude = parseCoordinate(token, lat_dir);
    }
    
    token = strtok(nullptr, ",");  // Longitude
    char* lon_dir = strtok(nullptr, ",");
    if (token && lon_dir) {
        data.longitude = parseCoordinate(token, lon_dir);
    }
    
    token = strtok(nullptr, ",");  // Fix quality
    if (token) {
        data.fix_quality = atoi(token);
        data.data_valid = (data.fix_quality > 0);
    }
    
    token = strtok(nullptr, ",");  // Number of satellites
    if (token) data.satellites = atoi(token);
    
    token = strtok(nullptr, ",");  // HDOP (skip)
    
    token = strtok(nullptr, ",");  // Altitude
    if (token) data.altitude = atof(token);
    
    data.timestamp_ms = millis();
    
    g_data_sensor.gps = data;
    g_data_sensor.flags.gps_valid = data.data_valid;
    xSemaphoreGive(data_mutex);
}

bool GPSManager::getData(GPSData_t &data) {
    if (data_mutex == nullptr) return false;
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
        data = g_data_sensor.gps;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

// ============================================================================
// GPS FREERTOS TASK
// ============================================================================

void gpsTask(void *pvParameters) {
    ESP_LOGI(TAG_GPS, "GPS task started");
    
    while (1) {
        g_gps_manager.handleUARTData();
        vTaskDelay(pdMS_TO_TICKS(GPS_UPDATE_DELAY_MS));
    }
    
    vTaskDelete(nullptr);
}

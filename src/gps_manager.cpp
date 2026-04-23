#include "gps_manager.h"
#include "constants.h"
#include "debug_logging.h"
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
        GPS_LOGE(TAG_GPS, "UART param config failed");
        return false;
    }
    
    if (uart_set_pin(UART_GPS, UART_TX_PIN, UART_RX_PIN, 
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        GPS_LOGE(TAG_GPS, "UART set pin failed");
        return false;
    }
    
    if (uart_driver_install(UART_GPS, UART_BUF_SIZE, 0, 0, nullptr, 0) != ESP_OK) {
        GPS_LOGE(TAG_GPS, "UART driver install failed");
        return false;
    }
    
    GPS_LOGI(TAG_GPS, "GPS UART initialized successfully");
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
                // GPS_LOGI(TAG_GPS, "===============================");
                // GPS_LOGI(TAG_GPS, "Received GPS sentence: %s", gps_buffer);
                // GPS_LOGI(TAG_GPS, "===============================");
                // Parse NMEA sentences
                if (strstr(gps_buffer, "$GPRMC") != nullptr) {
                    parseGPRMC(gps_buffer);
                } else if (strstr(gps_buffer, "$GPGGA") != nullptr) {
                    parseGPGGA(gps_buffer);
                } else if (strstr(gps_buffer, "$GPVTG") != nullptr) {
                    parseGPVTG(gps_buffer);
                } else if (strstr(gps_buffer, "$GPGSA") != nullptr) {
                    parseGPGSA(gps_buffer);
                } else if (strstr(gps_buffer, "$GPGLL") != nullptr) {
                    parseGPGLL(gps_buffer);
                } else if (strstr(gps_buffer, "$GPGSV") != nullptr) {
                    parseGPGSV(gps_buffer);
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
    coord_copy[sizeof(coord_copy) - 1] = '\0';
    
    // Find decimal point
    char* dot_ptr = strchr(coord_copy, '.');
    if (dot_ptr == nullptr) return 0.0f;
    
    // Determine degree digits (2 for Lat/N/S, 3 for Lon/E/W)
    int degree_digits = (strchr(direction, 'N') || strchr(direction, 'S')) ? 2 : 3;
    int dot_pos = dot_ptr - coord_copy;
    
    // Extract degrees (first degree_digits digits)
    char deg_str[10];
    strncpy(deg_str, coord_copy, degree_digits);
    deg_str[degree_digits] = '\0';
    
    // Extract minutes (rest is minutes including decimal part)
    char min_str[10];
    strncpy(min_str, coord_copy + degree_digits, sizeof(min_str) - 1);
    min_str[sizeof(min_str) - 1] = '\0';
    
    float degrees = atof(deg_str);
    float minutes = atof(min_str);
    float result = degrees + (minutes / 60.0f);
    
    // Apply direction
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
        GPS_LOGD(TAG_GPS, "GPRMC checksum error");
        return;
    }
    
    if (data_mutex == nullptr) {
        GPS_LOGE(TAG_GPS, "GPRMC: data_mutex is NULL! Not initialized!");
        return;
    }
    
    // Acquire lock BEFORE reading g_data_sensor
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        GPS_LOGW(TAG_GPS, "GPRMC: Failed to acquire mutex");
        return;
    }
    
    // $GPRMC,hhmmss.ss,status,lat,N/S,lon,E/W,speed,course,date,*checksum
    char sentence_copy[256];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    char* token = strtok(sentence_copy, ",");  // $GPRMC
    if (token == nullptr) {
        GPS_LOGW(TAG_GPS, "GPRMC: Parse error - no GPRMC marker");
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
    
    GPS_LOGI(TAG_GPS, "GPRMC parsed: Lat=%.6f, Lon=%.6f, Speed=%.2f km/h, Valid=%d",
             data.latitude, data.longitude, data.speed_kmh, data.data_valid);
    
    xSemaphoreGive(data_mutex);
}

void GPSManager::parseGPGGA(const char* sentence) {
    if (!validateChecksum(sentence)) {
        GPS_LOGD(TAG_GPS, "GPGGA checksum error");
        return;
    }
    
    if (data_mutex == nullptr) {
        GPS_LOGE(TAG_GPS, "GPGGA: data_mutex is NULL! Not initialized!");
        return;
    }
    
    // Acquire lock BEFORE reading g_data_sensor
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        GPS_LOGW(TAG_GPS, "GPGGA: Failed to acquire mutex");
        return;
    }
    
    // $GPGGA,time,lat,N/S,lon,E/W,fix_quality,num_satellites,hdop,altitude,*checksum
    char sentence_copy[256];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    char* token = strtok(sentence_copy, ",");  // $GPGGA
    if (token == nullptr) {
        GPS_LOGW(TAG_GPS, "GPGGA: Parse error - no GPGGA marker");
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
    
    GPS_LOGI(TAG_GPS, "GPGGA parsed: Lat=%.6f, Lon=%.6f, Alt=%.1f m, Sats=%d, Fix=%d",
             data.latitude, data.longitude, data.altitude, data.satellites, data.fix_quality);
    
    xSemaphoreGive(data_mutex);
}

void GPSManager::parseGPVTG(const char* sentence) {
    if (!validateChecksum(sentence)) {
        GPS_LOGD(TAG_GPS, "GPVTG checksum error");
        return;
    }
    
    if (data_mutex == nullptr) {
        GPS_LOGE(TAG_GPS, "GPVTG: data_mutex is NULL!");
        return;
    }
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        GPS_LOGW(TAG_GPS, "GPVTG: Failed to acquire mutex");
        return;
    }
    
    // $GPVTG,true_track,T,magnetic_track,M,speed_knots,N,speed_kmh,K,mode*checksum
    char sentence_copy[256];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    char* token = strtok(sentence_copy, ",");  // $GPVTG
    if (token == nullptr) {
        GPS_LOGW(TAG_GPS, "GPVTG: Parse error");
        xSemaphoreGive(data_mutex);
        return;
    }
    
    GPSData_t data = g_data_sensor.gps;
    
    token = strtok(nullptr, ",");  // True track (T)
    if (token && token[0] != '\0') {
        data.course = atof(token);
    }
    
    strtok(nullptr, ",");  // Skip 'T'
    strtok(nullptr, ",");  // Skip magnetic track (M)
    strtok(nullptr, ",");  // Skip 'M'
    
    token = strtok(nullptr, ",");  // Speed in knots
    if (token && token[0] != '\0') {
        data.speed_kts = atof(token);
    }
    
    strtok(nullptr, ",");  // Skip 'N'
    
    token = strtok(nullptr, ",");  // Speed in km/h
    if (token && token[0] != '\0') {
        data.speed_kmh = atof(token);
    }
    
    data.timestamp_ms = millis();
    g_data_sensor.gps = data;
    
    GPS_LOGI(TAG_GPS, "GPVTG parsed: Course=%.1f°, Speed=%.2f kt / %.2f km/h",
             data.course, data.speed_kts, data.speed_kmh);
    
    xSemaphoreGive(data_mutex);
}

void GPSManager::parseGPGSA(const char* sentence) {
    if (!validateChecksum(sentence)) {
        GPS_LOGD(TAG_GPS, "GPGSA checksum error");
        return;
    }
    
    if (data_mutex == nullptr) {
        GPS_LOGE(TAG_GPS, "GPGSA: data_mutex is NULL!");
        return;
    }
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        GPS_LOGW(TAG_GPS, "GPGSA: Failed to acquire mutex");
        return;
    }
    
    // $GPGSA,mode_select,fix_type,sat1,sat2,...sat12,pdop,hdop,vdop*checksum
    char sentence_copy[512];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    char* token = strtok(sentence_copy, ",");  // $GPGSA
    if (token == nullptr) {
        GPS_LOGW(TAG_GPS, "GPGSA: Parse error");
        xSemaphoreGive(data_mutex);
        return;
    }
    
    GPSData_t data = g_data_sensor.gps;
    
    token = strtok(nullptr, ",");  // Mode select (A=auto, M=manual)
    strtok(nullptr, ",");  // Skip it
    
    token = strtok(nullptr, ",");  // Fix type (1=no fix, 2=2D, 3=3D)
    if (token) {
        data.fix_type = atoi(token);
    }
    
    // Skip 12 satellite ID fields
    uint8_t active_sats = 0;
    for (int i = 0; i < 12; i++) {
        token = strtok(nullptr, ",");
        if (token && token[0] != '\0') {
            active_sats++;  // Count non-empty satellite IDs
        }
    }
    data.satellites_active = active_sats;
    
    token = strtok(nullptr, ",");  // PDOP
    if (token && token[0] != '\0') {
        data.pdop = atof(token);
    }
    
    token = strtok(nullptr, ",");  // HDOP
    if (token && token[0] != '\0') {
        data.hdop = atof(token);
    }
    
    token = strtok(nullptr, ",");  // VDOP
    if (token && token[0] != '\0') {
        data.vdop = atof(token);
    }
    
    data.timestamp_ms = millis();
    g_data_sensor.gps = data;
    
    GPS_LOGI(TAG_GPS, "GPGSA parsed: FixType=%d, ActiveSats=%d, PDOP=%.2f, HDOP=%.2f, VDOP=%.2f",
             data.fix_type, data.satellites_active, data.pdop, data.hdop, data.vdop);
    
    xSemaphoreGive(data_mutex);
}

void GPSManager::parseGPGLL(const char* sentence) {
    if (!validateChecksum(sentence)) {
        GPS_LOGD(TAG_GPS, "GPGLL checksum error");
        return;
    }
    
    if (data_mutex == nullptr) {
        GPS_LOGE(TAG_GPS, "GPGLL: data_mutex is NULL!");
        return;
    }
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        GPS_LOGW(TAG_GPS, "GPGLL: Failed to acquire mutex");
        return;
    }
    
    // $GPGLL,lat,N/S,lon,E/W,time,status,mode*checksum
    char sentence_copy[256];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    char* token = strtok(sentence_copy, ",");  // $GPGLL
    if (token == nullptr) {
        GPS_LOGW(TAG_GPS, "GPGLL: Parse error");
        xSemaphoreGive(data_mutex);
        return;
    }
    
    GPSData_t data = g_data_sensor.gps;
    
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
    
    token = strtok(nullptr, ",");  // Time
    if (token) parseTime(token, data.utc_time);
    
    token = strtok(nullptr, ",");  // Status (A=active)
    data.data_valid = (token && token[0] == 'A');
    
    data.timestamp_ms = millis();
    g_data_sensor.gps = data;
    g_data_sensor.flags.gps_valid = data.data_valid;
    
    GPS_LOGI(TAG_GPS, "GPGLL parsed: Lat=%.6f, Lon=%.6f, Valid=%d",
             data.latitude, data.longitude, data.data_valid);
    
    xSemaphoreGive(data_mutex);
}

void GPSManager::parseGPGSV(const char* sentence) {
    if (!validateChecksum(sentence)) {
        GPS_LOGD(TAG_GPS, "GPGSV checksum error");
        return;
    }
    
    if (data_mutex == nullptr) {
        GPS_LOGE(TAG_GPS, "GPGSV: data_mutex is NULL!");
        return;
    }
    
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        GPS_LOGW(TAG_GPS, "GPGSV: Failed to acquire mutex");
        return;
    }
    
    // $GPGSV,total_msgs,msg_num,total_sats,sat_id,elevation,azimuth,snr[,...]*checksum
    // Can have up to 4 satellites per sentence
    char sentence_copy[512];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    char* token = strtok(sentence_copy, ",");  // $GPGSV
    if (token == nullptr) {
        GPS_LOGW(TAG_GPS, "GPGSV: Parse error");
        xSemaphoreGive(data_mutex);
        return;
    }
    
    GPSData_t data = g_data_sensor.gps;
    
    token = strtok(nullptr, ",");  // Total number of messages
    int total_msgs = (token) ? atoi(token) : 0;
    
    token = strtok(nullptr, ",");  // Current message number
    int msg_num = (token) ? atoi(token) : 0;
    
    token = strtok(nullptr, ",");  // Total number of satellites in view
    if (token) {
        data.satellites = atoi(token);
    }
    
    // Parse satellite information (up to 4 per sentence)
    GPS_LOGD(TAG_GPS, "GPGSV msg %d/%d: %d satellites in view", msg_num, total_msgs, data.satellites);
    
    for (int i = 0; i < 4; i++) {
        token = strtok(nullptr, ",");  // Satellite ID
        if (token == nullptr || token[0] == '\0') break;
        
        int sat_id = atoi(token);
        
        token = strtok(nullptr, ",");  // Elevation
        int elevation = (token && token[0] != '\0') ? atoi(token) : 0;
        
        token = strtok(nullptr, ",");  // Azimuth
        int azimuth = (token && token[0] != '\0') ? atoi(token) : 0;
        
        token = strtok(nullptr, ",");  // SNR (Signal-to-Noise Ratio)
        int snr = (token && token[0] != '\0') ? atoi(token) : 0;
        
        GPS_LOGD(TAG_GPS, "  Sat %d: Elev=%d°, Azim=%d°, SNR=%d dB",
                 sat_id, elevation, azimuth, snr);
    }
    
    data.timestamp_ms = millis();
    g_data_sensor.gps = data;
    
    GPS_LOGI(TAG_GPS, "GPGSV parsed: %d satellites in view (msg %d/%d)",
             data.satellites, msg_num, total_msgs);
    
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
    GPS_LOGI(TAG_GPS, "GPS task started");
    
    while (1) {
        g_gps_manager.handleUARTData();
        vTaskDelay(pdMS_TO_TICKS(GPS_UPDATE_DELAY_MS));
    }
    
    vTaskDelete(nullptr);
}

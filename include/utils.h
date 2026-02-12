#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <cmath>

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Moving Average Filter
template<typename T, size_t SIZE>
class MovingAverageFilter {
private:
    T buffer[SIZE];
    size_t index;
    T sum;
    bool filled;
    
public:
    MovingAverageFilter() : index(0), sum(0), filled(false) {
        memset(buffer, 0, sizeof(buffer));
    }
    
    T filter(T value) {
        if (filled) {
            sum -= buffer[index];
        }
        
        buffer[index] = value;
        sum += value;
        index = (index + 1) % SIZE;
        
        if (index == 0) {
            filled = true;
        }
        
        return sum / (filled ? SIZE : (index + 1));
    }
    
    void reset() {
        memset(buffer, 0, sizeof(buffer));
        index = 0;
        sum = 0;
        filled = false;
    }
};

// ============================================================================
// MATHEMATICAL UTILITIES
// ============================================================================

// Constrain value between min and max
template<typename T>
inline T constrain(T value, T minValue, T maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

// Map value from one range to another
template<typename T>
inline T map_range(T value, T from_min, T from_max, T to_min, T to_max) {
    return (value - from_min) * (to_max - to_min) / (from_max - from_min) + to_min;
}

// Calculate distance between two GPS coordinates (Haversine formula)
inline float gps_distance(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371000.0f;  // Earth radius in meters
    
    float dLat = (lat2 - lat1) * M_PI / 180.0f;
    float dLon = (lon2 - lon1) * M_PI / 180.0f;
    float a = sin(dLat / 2) * sin(dLat / 2) +
              cos(lat1 * M_PI / 180.0f) * cos(lat2 * M_PI / 180.0f) *
              sin(dLon / 2) * sin(dLon / 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;  // Distance in meters
}

// Calculate bearing between two GPS coordinates
inline float gps_bearing(float lat1, float lon1, float lat2, float lon2) {
    float dLon = (lon2 - lon1) * M_PI / 180.0f;
    float y = sin(dLon) * cos(lat2 * M_PI / 180.0f);
    float x = cos(lat1 * M_PI / 180.0f) * sin(lat2 * M_PI / 180.0f) -
              sin(lat1 * M_PI / 180.0f) * cos(lat2 * M_PI / 180.0f) * cos(dLon);
    
    float bearing = atan2(y, x) * 180.0f / M_PI;
    return (bearing < 0) ? (bearing + 360.0f) : bearing;
}

// ============================================================================
// CRC/CHECKSUM UTILITIES
// ============================================================================

// Calculate CRC16 (CCITT-FALSE)
inline uint16_t crc16_ccitt(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// Calculate CRC8
inline uint8_t crc8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// ============================================================================
// CONVERSION UTILITIES
// ============================================================================

// Convert degrees to radians
inline float deg_to_rad(float degrees) {
    return degrees * M_PI / 180.0f;
}

// Convert radians to degrees
inline float rad_to_deg(float radians) {
    return radians * 180.0f / M_PI;
}

// Convert knots to km/h
inline float knots_to_kmh(float knots) {
    return knots * 1.852f;
}

// Convert knots to m/s
inline float knots_to_ms(float knots) {
    return knots * 0.51444f;
}

// Convert km/h to m/s
inline float kmh_to_ms(float kmh) {
    return kmh / 3.6f;
}

// ============================================================================
// DATA STRUCTURE UTILITIES
// ============================================================================

// Compare two floating point values with tolerance
inline bool float_equals(float a, float b, float tolerance = 1e-6f) {
    return fabs(a - b) < tolerance;
}

// Linear interpolation
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// ============================================================================
// TIMING UTILITIES
// ============================================================================

// Calculate elapsed time in milliseconds
class Timer {
private:
    uint32_t start_time;
    uint32_t duration;
    
public:
    Timer(uint32_t duration_ms = 1000) : duration(duration_ms) {
        start_time = millis();
    }
    
    void reset() {
        start_time = millis();
    }
    
    void setDuration(uint32_t duration_ms) {
        duration = duration_ms;
    }
    
    bool isExpired() const {
        return (millis() - start_time) >= duration;
    }
    
    uint32_t elapsed() const {
        return millis() - start_time;
    }
    
    uint32_t remaining() const {
        uint32_t e = elapsed();
        return (e >= duration) ? 0 : (duration - e);
    }
};

// ============================================================================
// STRING UTILITIES
// ============================================================================

// Safe string to float conversion
inline float safe_strtof(const char* str, float default_value = 0.0f) {
    if (str == nullptr || str[0] == '\0') {
        return default_value;
    }
    
    char* end;
    float result = strtof(str, &end);
    return (end == str) ? default_value : result;
}

// Safe string to integer conversion
inline int32_t safe_strtoi(const char* str, int32_t default_value = 0) {
    if (str == nullptr || str[0] == '\0') {
        return default_value;
    }
    
    char* end;
    int32_t result = strtol(str, &end, 10);
    return (end == str) ? default_value : result;
}

// ============================================================================
// DEBUGGING UTILITIES
// ============================================================================

// Print hex dump
inline void hex_dump(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            Serial.printf("\n");
        }
    }
    Serial.printf("\n");
}

// Print buffer as string
inline void buffer_print(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] >= 32 && data[i] < 127) {
            Serial.write(data[i]);
        } else {
            Serial.printf("[%02X]", data[i]);
        }
    }
    Serial.printf("\n");
}

#endif // UTILS_H

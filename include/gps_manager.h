#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include "data_sensor.h"

// ============================================================================
// GPS DRIVER / MANAGER
// ============================================================================

class GPSManager {
private:
    SemaphoreHandle_t data_mutex;
    SemaphoreHandle_t uart_mutex;
    char gps_buffer[512];
    uint16_t buffer_index;
    bool initialized;
    
public:
    GPSManager();
    ~GPSManager();
    
    bool init();
    void deinit();
    void handleUARTData();
    bool getData(GPSData_t &data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
    
private:
    void parseGPRMC(const char* sentence);  // $GPRMC parsing
    void parseGPGGA(const char* sentence);  // $GPGGA parsing
    float parseCoordinate(const char* coord, const char* direction);
    bool parseTime(const char* time_str, char* utc_time);
    bool parseDate(const char* date_str, char* utc_date);
    float parseSpeed(const char* speed_str);
    uint8_t calculateChecksum(const char* sentence);
    bool validateChecksum(const char* sentence);
};

// ============================================================================
// GPS TASK DECLARATION
// ============================================================================

void gpsTask(void *pvParameters);

// ============================================================================
// GLOBAL GPS MANAGER INSTANCE
// ============================================================================

extern GPSManager g_gps_manager;

#endif // GPS_MANAGER_H

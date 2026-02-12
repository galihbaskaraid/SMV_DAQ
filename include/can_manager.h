#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <Arduino.h>
#include "data_sensor.h"

// ============================================================================
// CAN BUS MANAGER (TWAI - Two-Wire Automotive Interface)
// ============================================================================

class CANManager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;
    
public:
    CANManager();
    ~CANManager();
    
    bool init();
    void deinit();
    bool sendMessage(uint32_t id, const uint8_t* data, uint8_t dlc);
    bool getData(CANData_t &data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
    bool isInitialized() const { return initialized; }
};

// ============================================================================
// CAN TASK DECLARATIONS
// ============================================================================

void canRxTask(void *pvParameters);
void canTxTask(void *pvParameters);

// ============================================================================
// GLOBAL CAN MANAGER INSTANCE
// ============================================================================

extern CANManager g_can_manager;
extern SemaphoreHandle_t g_data_sensor_mutex;

#endif // CAN_MANAGER_H

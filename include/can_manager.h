#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "data_sensor.h"
#include "can_structs.h"

// ============================================================================
// CAN BUS MANAGER (TWAI - Two-Wire Automotive Interface)
// ============================================================================

class CANManager {
private:
    SemaphoreHandle_t data_mutex;
    bool initialized;
    int rx_pin;
    int tx_pin;
    long baudrate;
    
    // VESC RX Data
    can_status_msg g_status;
    can_status_msg_2 g_status2;
    can_status_msg_3 g_status3;
    can_status_msg_4 g_status4;
    can_status_msg_5 g_status5;
    can_status_msg_6 g_status6;
    FuelCellData g_fuel_cell_data;
    
    // CAN TX Data
    float tx_energy, tx_power, tx_anglex, tx_angley;
    float tx_voltage, tx_current, tx_battery_current;
    float tx_speed, tx_wheel_rpm, tx_motor_rpm;
    float tx_temp, tx_humidity, tx_gear;
    float tx_distance, tx_elevation;
    float tx_pull_timer, tx_glide_timer;
    
public:
    CANManager(int rx = 4, int tx = 5, long baud = 500000);
    ~CANManager();
    
    bool init();
    void deinit();
    
    // VESC RX Getters
    bool getStatus(can_status_msg &out);
    bool getStatus2(can_status_msg_2 &out);
    bool getStatus3(can_status_msg_3 &out);
    bool getStatus4(can_status_msg_4 &out);
    bool getStatus5(can_status_msg_5 &out);
    bool getStatus6(can_status_msg_6 &out);
    
    // Fuel Cell RX Getter
    bool getFuelCellData(FuelCellData &out);
    
    // CAN TX Setter
    void setTxData(float energy, float power, float anglex, float angley,
                   float voltage, float motor_current, float battery_current,
                   float speed, float wheel_rpm, float motor_rpm,
                   float temp, float humidity, float gear,
                   float distance, float elevation,
                   float pull_timer, float glide_timer);
    
    bool sendMessage(uint32_t id, const uint8_t* data, uint8_t dlc);
    bool getData(CANData_t &data);
    void setDataMutex(SemaphoreHandle_t mutex) { data_mutex = mutex; }
    bool isInitialized() const { return initialized; }

private:
    friend void canRxTask(void *pvParameters);
    friend void canTxTask(void *pvParameters);
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

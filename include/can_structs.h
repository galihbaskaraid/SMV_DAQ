#ifndef CAN_STRUCTS_H
#define CAN_STRUCTS_H

#include <stdint.h>
#include <cstring>

// ============================================================================
// VESC CAN MESSAGE STRUCTURES
// ============================================================================

typedef struct {
    int id;
    uint32_t rx_time;
    float rpm;
    float current;
    float duty;
} can_status_msg;

typedef struct {
    int id;
    uint32_t rx_time;
    float amp_hours;
    float amp_hours_charged;
} can_status_msg_2;

typedef struct {
    int id;
    uint32_t rx_time;
    float watt_hours;
    float watt_hours_charged;
} can_status_msg_3;

typedef struct {
    int id;
    uint32_t rx_time;
    float temp_fet;
    float temp_motor;
    float current_in;
    float pid_pos_now;
} can_status_msg_4;

typedef struct {
    int id;
    uint32_t rx_time;
    float v_in;
    int32_t tacho_value;
} can_status_msg_5;

typedef struct {
    int id;
    uint32_t rx_time;
    float adc_1;
    float adc_2;
    float adc_3;
    float ppm;
} can_status_msg_6;

// ============================================================================
// FUEL CELL DATA STRUCTURES
// ============================================================================

typedef struct {
    float fc_voltage;        
    float fc_current;        
    float fc_power;          
    float energy;            
    float fct1_temp;         
    int   fan;               
    float h2p1_pressure;     
    float h2p2_pressure;     
    float tank_pressure;     
    float tank_temp;         
    float v_set;             
    float i_set;             
    float ucb_voltage;       
    int   stasis_selector;   
    float stasis_v1;         
    float stasis_v2;         
    int   number_of_cell;    
} FuelCellData;

#endif // CAN_STRUCTS_H

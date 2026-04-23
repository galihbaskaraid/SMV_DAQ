#include "can_manager.h"
#include "constants.h"
#include "debug_logging.h"
#include <esp_log.h>
#include <esp_system.h>
#include <driver/twai.h>

extern "C" {
    #include "buffer.h"
}

// TAG_CAN is already defined in constants.h

static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Global CAN manager instance
CANManager g_can_manager(CAN_RX_PIN, CAN_TX_PIN, CAN_SPEED);

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

CANManager::CANManager(int rx, int tx, long baud)
    : data_mutex(nullptr), initialized(false), rx_pin(rx), tx_pin(tx), 
      baudrate(baud) {
    memset(&g_status, 0, sizeof(g_status));
    memset(&g_status2, 0, sizeof(g_status2));
    memset(&g_status3, 0, sizeof(g_status3));
    memset(&g_status4, 0, sizeof(g_status4));
    memset(&g_status5, 0, sizeof(g_status5));
    memset(&g_status6, 0, sizeof(g_status6));
    memset(&g_fuel_cell_data, 0, sizeof(g_fuel_cell_data));
}

CANManager::~CANManager() {
    deinit();
}

bool CANManager::init() {
    CAN_LOGI(TAG_CAN, "Initializing CAN bus at %ld bps", baudrate);
    
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)rx_pin, (gpio_num_t)tx_pin, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config;
    
    // Select timing config based on baudrate
    if (baudrate == 1000000)
        t_config = TWAI_TIMING_CONFIG_1MBITS();
    else if (baudrate == 500000)
        t_config = TWAI_TIMING_CONFIG_500KBITS();
    else if (baudrate == 250000)
        t_config = TWAI_TIMING_CONFIG_250KBITS();
    else {
        CAN_LOGE(TAG_CAN, "Unsupported baudrate: %ld", baudrate);
        return false;
    }
    
    // Install driver
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        CAN_LOGE(TAG_CAN, "Failed to install TWAI driver");
        return false;
    }
    
    // Start driver
    if (twai_start() != ESP_OK) {
        CAN_LOGE(TAG_CAN, "Failed to start TWAI driver");
        return false;
    }
    
    // Create mutex
    if (!data_mutex) {
        data_mutex = xSemaphoreCreateMutex();
    }
    
    if (data_mutex == NULL) {
        CAN_LOGE(TAG_CAN, "Failed to create mutex");
        return false;
    }
    
    // Create RX and TX tasks
    if (xTaskCreatePinnedToCore(canRxTask, "CANRxTask", 4096, nullptr, CAN_TASK_PRIORITY, NULL, APP_CPU_NUM) != pdPASS) {
        CAN_LOGE(TAG_CAN, "Failed to create RX task");
        return false;
    }
    
    if (xTaskCreatePinnedToCore(canTxTask, "CANTxTask", 4096, nullptr, CAN_TASK_PRIORITY - 1, NULL, APP_CPU_NUM) != pdPASS) {
        CAN_LOGE(TAG_CAN, "Failed to create TX task");
        return false;
    }
    
    initialized = true;
    CAN_LOGI(TAG_CAN, "CAN bus initialized successfully");
    return true;
}

void CANManager::deinit() {
    if (initialized) {
        twai_stop();
        twai_driver_uninstall();
        initialized = false;
    }
}

// ============================================================================
// VESC DATA PARSING FUNCTIONS
// ============================================================================

static void decode_status(can_status_msg &out, uint8_t id, const uint8_t *d) {
    int32_t ind = 0;
    out.id = id;
    out.rx_time = millis();
    out.rpm = (float)buffer_get_int32(d, &ind);
    out.current = (float)buffer_get_int16(d, &ind) / 10.0f;
    out.duty = (float)buffer_get_int16(d, &ind) / 1000.0f;
}

static void decode_status_2(can_status_msg_2 &out, uint8_t id, const uint8_t *d) {
    int32_t ind = 0;
    out.id = id;
    out.rx_time = millis();
    out.amp_hours = (float)buffer_get_int32(d, &ind) / 10000.0f;
    out.amp_hours_charged = (float)buffer_get_int32(d, &ind) / 10000.0f;
}

static void decode_status_3(can_status_msg_3 &out, uint8_t id, const uint8_t *d) {
    int32_t ind = 0;
    out.id = id;
    out.rx_time = millis();
    out.watt_hours = (float)buffer_get_int32(d, &ind) / 1000.0f;
    out.watt_hours_charged = (float)buffer_get_int32(d, &ind) / 1000.0f;
}

static void decode_status_4(can_status_msg_4 &out, uint8_t id, const uint8_t *d) {
    int32_t ind = 0;
    out.id = id;
    out.rx_time = millis();
    out.temp_fet = (float)buffer_get_int16(d, &ind) / 10.0f;
    out.temp_motor = (float)buffer_get_int16(d, &ind) / 10.0f;
    out.current_in = (float)buffer_get_int16(d, &ind) / 10.0f;
    out.pid_pos_now = (float)buffer_get_int16(d, &ind) / 50.0f;
}

static void decode_status_5(can_status_msg_5 &out, uint8_t id, const uint8_t *d) {
    int32_t ind = 0;
    out.id = id;
    out.rx_time = millis();
    out.v_in = (float)buffer_get_int16(d, &ind) / 10.0f;
    out.tacho_value = buffer_get_int32(d, &ind);
}

static void decode_status_6(can_status_msg_6 &out, uint8_t id, const uint8_t *d) {
    int32_t ind = 0;
    out.id = id;
    out.rx_time = millis();
    out.adc_1 = (float)buffer_get_int16(d, &ind) / 1000.0f;
    out.adc_2 = (float)buffer_get_int16(d, &ind) / 1000.0f;
    out.adc_3 = (float)buffer_get_int16(d, &ind) / 1000.0f;
    out.ppm = (float)buffer_get_int16(d, &ind) / 1000.0f;
}

// Helper untuk unpack data Little-Endian dari CAN
static inline float get_float_le(const uint8_t* src) {
    float val;
    memcpy(&val, src, sizeof(float));
    return val;
}

static inline uint16_t get_u16_le(const uint8_t* src) {
    return (uint16_t)(src[0] | (src[1] << 8));
}

// ============================================================================
// CAN RX TASK
// ============================================================================

void canRxTask(void *pvParameters) {
    twai_message_t message;
    
    while (1) {
        if (twai_receive(&message, pdMS_TO_TICKS(20)) == ESP_OK) {
            if (xSemaphoreTake(g_can_manager.data_mutex, portMAX_DELAY) != pdTRUE) continue;
            
            // Extended ID: VESC format
            if (message.flags & TWAI_MSG_FLAG_EXTD) {
                uint32_t full_address = message.identifier;
                uint8_t node_id = full_address & 0xFF;
                uint8_t packet_id = (full_address >> 8) & 0xFF;
                
                switch (packet_id) {
                    case 9: 
                        decode_status(g_can_manager.g_status, node_id, message.data);
                        break;
                    case 14: 
                        decode_status_2(g_can_manager.g_status2, node_id, message.data);
                        break;
                    case 15: 
                        decode_status_3(g_can_manager.g_status3, node_id, message.data);
                        break;
                    case 16: 
                        decode_status_4(g_can_manager.g_status4, node_id, message.data);
                        break;
                    case 27: 
                        decode_status_5(g_can_manager.g_status5, node_id, message.data);
                        break;
                    case 28: 
                        decode_status_6(g_can_manager.g_status6, node_id, message.data);
                        break;
                }
            }
            // Standard ID: Fuel Cell format
            else {
                switch (message.identifier) {
                    case 0x100:
                        g_can_manager.g_fuel_cell_data.fc_voltage = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.fc_current = get_float_le(&message.data[4]);
                        break;
                    case 0x101:
                        g_can_manager.g_fuel_cell_data.fc_power = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.energy = get_float_le(&message.data[4]);
                        break;
                    case 0x102:
                        g_can_manager.g_fuel_cell_data.fct1_temp = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.tank_temp = get_float_le(&message.data[4]);
                        break;
                    case 0x103:
                        g_can_manager.g_fuel_cell_data.h2p1_pressure = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.h2p2_pressure = get_float_le(&message.data[4]);
                        break;
                    case 0x104:
                        g_can_manager.g_fuel_cell_data.tank_pressure = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.ucb_voltage = get_float_le(&message.data[4]);
                        break;
                    case 0x105:
                        g_can_manager.g_fuel_cell_data.v_set = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.i_set = get_float_le(&message.data[4]);
                        break;
                    case 0x106:
                        g_can_manager.g_fuel_cell_data.stasis_v1 = get_float_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.stasis_v2 = get_float_le(&message.data[4]);
                        break;
                    case 0x107:
                        g_can_manager.g_fuel_cell_data.stasis_selector = get_u16_le(&message.data[0]);
                        g_can_manager.g_fuel_cell_data.number_of_cell = get_u16_le(&message.data[2]);
                        g_can_manager.g_fuel_cell_data.fan = get_u16_le(&message.data[4]);
                        break;
                }
            }
            
            xSemaphoreGive(g_can_manager.data_mutex);
        }
    }
}

// ============================================================================
// CAN TX TASK
// ============================================================================

void canTxTask(void *pvParameters) {
    uint8_t frame_state = 0;
    
    while (1) {
        float energy, power, anglex, angley, voltage, current, battery_current;
        float speed, wheel_rpm, motor_rpm, distance, elevation;
        int gear;
        float duty_cycle_val;
        float pull_timer, glide_timer;
        
        if (xSemaphoreTake(g_can_manager.data_mutex, portMAX_DELAY) == pdTRUE) {
            energy = g_can_manager.tx_energy;
            power = g_can_manager.tx_power;
            anglex = g_can_manager.tx_anglex;
            angley = g_can_manager.tx_angley;
            distance = g_can_manager.tx_distance;
            elevation = g_can_manager.tx_elevation;
            voltage = g_can_manager.tx_voltage;
            current = g_can_manager.tx_current;
            battery_current = g_can_manager.tx_battery_current;
            speed = g_can_manager.tx_speed;
            wheel_rpm = g_can_manager.tx_wheel_rpm;
            motor_rpm = g_can_manager.tx_motor_rpm;
            gear = (int)g_can_manager.tx_gear;
            duty_cycle_val = g_can_manager.g_status.duty;
            pull_timer = g_can_manager.tx_pull_timer;
            glide_timer = g_can_manager.tx_glide_timer;
            xSemaphoreGive(g_can_manager.data_mutex);
        }
        
        twai_message_t tx_msg = {};
        tx_msg.extd = 1;
        tx_msg.rtr = 0;
        uint8_t buf[8] = {0};
        int32_t idx = 0;
        
        switch (frame_state) {
            case 0:
                tx_msg.identifier = 0x500;
                buffer_append_float16(buf, energy, 100.0f, &idx);
                buffer_append_float16(buf, power, 10.0f, &idx);
                break;
            case 1:
                tx_msg.identifier = 0x501;
                buffer_append_float16(buf, anglex, 100.0f, &idx);
                buffer_append_float16(buf, angley, 100.0f, &idx);
                buffer_append_float16(buf, distance, 10.0f, &idx);
                buffer_append_float16(buf, elevation, 10.0f, &idx);
                break;
            case 2:
                tx_msg.identifier = 0x502;
                buffer_append_float16(buf, voltage, 100.0f, &idx);
                buffer_append_float16(buf, current, 100.0f, &idx);
                buffer_append_float16(buf, battery_current, 100.0f, &idx);
                buffer_append_float16(buf, duty_cycle_val * 100.0f, 100.0f, &idx);
                break;
            case 3:
                tx_msg.identifier = 0x503;
                buffer_append_float16(buf, speed, 10.0f, &idx);
                buffer_append_int16(buf, (int16_t)wheel_rpm, &idx);
                buffer_append_int16(buf, (int16_t)motor_rpm, &idx);
                buffer_append_int16(buf, (int16_t)gear, &idx);
                break;
            case 4:
                tx_msg.identifier = 0x504;
                if (pull_timer > 0) {
                    buf[0] = 0x01;
                    memcpy(&buf[1], &pull_timer, sizeof(float));
                    idx = 1 + sizeof(float);
                } else if (glide_timer > 0) {
                    buf[0] = 0x02;
                    memcpy(&buf[1], &glide_timer, sizeof(float));
                    idx = 1 + sizeof(float);
                }
                break;
        }
        
        if (idx > 0) {
            tx_msg.data_length_code = idx;
            memcpy(tx_msg.data, buf, idx);
            twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
        }
        
        frame_state = (frame_state + 1) % 5;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================================
// VESC RX GETTER FUNCTIONS
// ============================================================================

bool CANManager::getStatus(can_status_msg &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_status;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool CANManager::getStatus2(can_status_msg_2 &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_status2;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool CANManager::getStatus3(can_status_msg_3 &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_status3;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool CANManager::getStatus4(can_status_msg_4 &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_status4;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool CANManager::getStatus5(can_status_msg_5 &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_status5;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool CANManager::getStatus6(can_status_msg_6 &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_status6;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool CANManager::getFuelCellData(FuelCellData &out) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(50))) {
        out = g_fuel_cell_data;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

// ============================================================================
// CAN TX SETTER
// ============================================================================

void CANManager::setTxData(float energy, float power, float anglex, float angley,
                           float voltage, float motor_current, float battery_current,
                           float speed, float wheel_rpm, float motor_rpm,
                           float temp, float humidity, float gear,
                           float distance, float elevation,
                           float pull_timer, float glide_timer) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(10))) {
        tx_energy = energy;
        tx_power = power;
        tx_anglex = anglex;
        tx_angley = angley;
        tx_voltage = voltage;
        tx_current = motor_current;
        tx_battery_current = battery_current;
        tx_speed = speed;
        tx_wheel_rpm = wheel_rpm;
        tx_motor_rpm = motor_rpm;
        tx_temp = temp;
        tx_humidity = humidity;
        tx_gear = gear;
        tx_distance = distance;
        tx_elevation = elevation;
        tx_pull_timer = pull_timer;
        tx_glide_timer = glide_timer;
        xSemaphoreGive(data_mutex);
    }
}

bool CANManager::sendMessage(uint32_t id, const uint8_t* data, uint8_t dlc) {
    twai_message_t msg = {};
    msg.identifier = id;
    msg.data_length_code = dlc;
    memcpy(msg.data, data, dlc);
    
    return twai_transmit(&msg, pdMS_TO_TICKS(10)) == ESP_OK;
}

bool CANManager::getData(CANData_t &data) {
    // Placeholder - could be extended for generic CAN messages
    return false;
}

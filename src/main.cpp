#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>

// Include all managers
#include "constants.h"
#include "debug_logging.h"
#include "data_sensor.h"
#include "sensor_manager.h"
#include "data_calc.h"
#include "ble_serialize.h"
#include "gps_manager.h"
#include "can_manager.h"
#include "wifi_manager.h"
#include "struct_offsets.h"

//BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Global data sensor structure (shared across all tasks)
DataSensor_t g_data_sensor = {};

// Mutex for thread-safe access to global data
SemaphoreHandle_t g_data_sensor_mutex = nullptr;

// Task handles
TaskHandle_t g_sensor_task_handle = nullptr;
TaskHandle_t g_gps_task_handle = nullptr;
TaskHandle_t g_can_rx_task_handle = nullptr;
TaskHandle_t g_can_tx_task_handle = nullptr;
TaskHandle_t g_wifi_task_handle = nullptr;

// ============================================================================
// SENSOR READ TASK (High frequency sensor reading)
// ============================================================================

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;

        BLE_LOGI(TAG_SYSTEM, "BLE Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        BLE_LOGI(TAG_SYSTEM, "BLE Client disconnected");
        pServer->startAdvertising();
    }
};

void initBLE()
{
    BLE_LOGI(TAG_SYSTEM, "Initializing BLE...");
    
    BLEDevice::init("DAQ_SYSTEM");
    BLEDevice::setMTU(512);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->start();
    
    g_data_sensor.flags.ble_init = true;
    BLE_LOGI(TAG_SYSTEM, "✓ BLE initialized and advertising started");
}

void sensorReadTask(void *pvParameters) {
    SYSTEM_LOGI(TAG_SYSTEM, "Sensor read task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // Update all sensors (only if initialized)
        if (g_data_sensor.flags.mpu6500_init) {
            g_mpu6500.setDataMutex(g_data_sensor_mutex);
            g_mpu6500.update();
        }
        
        if (g_data_sensor.flags.ads1115_init) {
            g_ads1115.setDataMutex(g_data_sensor_mutex);
            g_ads1115.update();
        }
        
        if (g_data_sensor.flags.speed_init) {
            g_speed_sensor.setDataMutex(g_data_sensor_mutex);
            g_speed_sensor.update();
        }
        
        // Read environment sensor (temperature/humidity) - only if initialized
        if (g_data_sensor.flags.env_init && xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(10))) {
            if (g_env_sensor.readData(g_data_sensor.env)) {
                g_data_sensor.flags.env_valid = true;
            }
            xSemaphoreGive(g_data_sensor_mutex);
        }
        
        // Perform calculations with mutex protection
        if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(10))) {
            // ========== PERFORM CALCULATIONS ==========
            // Calculate power from voltage and current
            if (g_data_sensor.flags.ads1115_valid) {
                g_data_calc.calculatePower(
                    g_data_sensor.ads1115.voltage,
                    g_data_sensor.ads1115.current,
                    g_data_sensor.calc
                );
                
                // Calculate energy consumption
                g_data_calc.calculateEnergy(
                    g_data_sensor.calc.power,
                    millis(),
                    g_data_sensor.calc
                );
                
                // Calculate distance and average speed
                if (g_data_sensor.flags.speed_valid) {
                    g_data_calc.calculateDistance(
                        g_data_sensor.speed.speed_kmh,
                        millis(),
                        g_data_sensor.calc
                    );
                    
                    g_data_calc.updateMovingTime(
                        g_data_sensor.speed.speed_kmh,
                        millis()
                    );
                    
                    g_data_sensor.calc.avg_speed_kmh = g_data_calc.getAverageSpeed();
                }
                
                // Update drive state (pulling vs gliding)
                g_data_calc.updateDriveState(
                    g_data_sensor.ads1115.current,
                    g_data_sensor.calc
                );
                
                g_data_sensor.flags.calc_valid = true;
            }
            
            // Update system status
            g_data_sensor.status.heap_free = esp_get_free_heap_size();
            g_data_sensor.status.uptime_ms = millis();
            g_data_sensor.last_update_ms = millis();
            
            // Update CAN TX data with calculated values
            if (g_data_sensor.flags.calc_valid) {
                g_can_manager.setTxData(
                    g_data_sensor.calc.energy_kwh,                 // energy (kWh)
                    g_data_sensor.calc.power,                      // power (W)
                    g_data_sensor.mpu6500.accel.x,                 // anglex (°)
                    g_data_sensor.mpu6500.accel.y,                 // angley (°)
                    g_data_sensor.ads1115.voltage,                 // voltage (V)
                    g_data_sensor.ads1115.current,                 // motor_current (A)
                    g_data_sensor.ads1115.current,                 // battery_current (A)
                    g_data_sensor.speed.speed_kmh,                 // speed (km/h)
                    g_data_sensor.speed.pulse_count,               // wheel_rpm (using pulse count as proxy)
                    g_data_calc.getMotorRPM(),                     // motor_rpm
                    g_data_sensor.env.temperature,                 // temp (°C)
                    g_data_sensor.env.humidity,                    // humidity (%)
                    (float)g_data_calc.getDetectedGear(),          // gear
                    g_data_sensor.calc.total_distance_m,           // distance (m)
                    0.0f,                                          // elevation (m) - from GPS if available
                    g_data_calc.getPullTimer(),                    // pull_timer (s)
                    g_data_calc.getGlideTimer()                    // glide_timer (s)
                );
            }
            
            xSemaphoreGive(g_data_sensor_mutex);
        }
        
        // Block for periodic update
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_READ_DELAY_MS));
    }
    
    vTaskDelete(nullptr);
}

// ============================================================================
// SYSTEM INITIALIZATION
// ============================================================================

void initializeSystems() {
    SYSTEM_LOGI(TAG_SYSTEM, "=== Initializing DAQ System ===");
    
    // Create mutex for global data structure
    g_data_sensor_mutex = xSemaphoreCreateMutex();
    if (g_data_sensor_mutex == nullptr) {
        SYSTEM_LOGE(TAG_SYSTEM, "Failed to create data sensor mutex");
        return;
    }
    
    // Initialize sensors
    SYSTEM_LOGI(TAG_SYSTEM, "Initializing sensors...");
    if (!g_mpu6500.init()) {
        SYSTEM_LOGE(TAG_SYSTEM, "MPU6500 initialization failed");
        g_data_sensor.flags.mpu6500_init = false;
    } else {
        g_data_sensor.flags.mpu6500_init = true;
        SYSTEM_LOGI(TAG_SYSTEM, "✓ MPU6500 initialized");
    }
    
    // Configure I2C clock speed (from JouleMeterTest pattern)
    Wire.setClock(400000);  // 400kHz I2C speed for ADS1115 and other I2C devices
    
    if (!g_ads1115.init()) {
        SYSTEM_LOGE(TAG_SYSTEM, "ADS1115 initialization failed");
        g_data_sensor.flags.ads1115_init = false;
    } else {
        g_data_sensor.flags.ads1115_init = true;
        SYSTEM_LOGI(TAG_SYSTEM, "✓ ADS1115 initialized");
    }
    
    if (!g_speed_sensor.init()) {
        SYSTEM_LOGE(TAG_SYSTEM, "Speed sensor initialization failed");
        g_data_sensor.flags.speed_init = false;
    } else {
        g_data_sensor.flags.speed_init = true;
        SYSTEM_LOGI(TAG_SYSTEM, "✓ Speed sensor initialized");
    }
    
    // Initialize environment sensor (WSEN_HIDS for temperature/humidity)
    SYSTEM_LOGI(TAG_SYSTEM, "Initializing environment sensor...");
    if (!g_env_sensor.init()) {
        SYSTEM_LOGE(TAG_SYSTEM, "Environment sensor initialization failed (non-critical)");
        g_data_sensor.flags.env_init = false;
    } else {
        g_data_sensor.flags.env_init = true;
        SYSTEM_LOGI(TAG_SYSTEM, "✓ Environment sensor (WSEN_HIDS) initialized");
    }
    
    // Initialize GPS
    SYSTEM_LOGI(TAG_SYSTEM, "Initializing GPS...");
    if (!g_gps_manager.init()) {
        SYSTEM_LOGE(TAG_SYSTEM, "GPS initialization failed");
        g_data_sensor.flags.gps_init = false;
    } else {
        g_gps_manager.setDataMutex(g_data_sensor_mutex);  // ✓ Give GPS access to shared data
        g_data_sensor.flags.gps_init = true;
        SYSTEM_LOGI(TAG_SYSTEM, "✓ GPS initialized");
    }
    
    // Initialize CAN bus
    SYSTEM_LOGI(TAG_SYSTEM, "Initializing CAN bus...");
    if (!g_can_manager.init()) {
        SYSTEM_LOGE(TAG_SYSTEM, "CAN bus initialization failed");
        g_data_sensor.flags.can_init = false;
    } else {
        g_data_sensor.flags.can_init = true;
        SYSTEM_LOGI(TAG_SYSTEM, "✓ CAN bus initialized");
    }
    
    // Initialize WiFi
    // ESP_LOGI(TAG_SYSTEM, "Initializing WiFi...");
    // if (!g_wifi_manager.init()) {
    //     ESP_LOGE(TAG_SYSTEM, "WiFi initialization failed");
    //     g_data_sensor.flags.wifi_init = false;
    // } else {
    //     g_data_sensor.flags.wifi_init = true;
    //     ESP_LOGI(TAG_SYSTEM, "✓ WiFi initialized");
    // }
    
    // Connect to WiFi (non-blocking)
    // g_wifi_manager.connect();
    
    SYSTEM_LOGI(TAG_SYSTEM, "System initialization complete!");
}

// ============================================================================
// CREATE FREERTOS TASKS
// ============================================================================

void createTasks() {
    SYSTEM_LOGI(TAG_SYSTEM, "Creating FreeRTOS tasks...");
    
    // Sensor read task (high priority, app CPU)
    xTaskCreatePinnedToCore(
        sensorReadTask,           // Task function
        "SensorReadTask",         // Task name
        TASK_STACK_SIZE,          // Stack size
        nullptr,                  // Parameters
        TASK_PRIORITY,            // Priority
        &g_sensor_task_handle,    // Task handle
        APP_CPU_NUM               // CPU core
    );
    
    // GPS task (medium priority, app CPU)
    xTaskCreatePinnedToCore(
        gpsTask,
        "GPSTask",
        TASK_STACK_SIZE * 2,      // GPS needs more stack
        nullptr,
        GPS_TASK_PRIORITY,
        &g_gps_task_handle,
        APP_CPU_NUM
    );
    
    // CAN RX task (high priority, app CPU)
    xTaskCreatePinnedToCore(
        canRxTask,
        "CANRxTask",
        TASK_STACK_SIZE,
        nullptr,
        CAN_TASK_PRIORITY,
        &g_can_rx_task_handle,
        APP_CPU_NUM
    );
    
    // CAN TX task (medium priority, app CPU)
    xTaskCreatePinnedToCore(
        canTxTask,
        "CANTxTask",
        TASK_STACK_SIZE,
        nullptr,
        CAN_TASK_PRIORITY - 1,
        &g_can_tx_task_handle,
        APP_CPU_NUM
    );
    
    // WiFi task (low priority, pro CPU to not interfere with real-time tasks)
    // xTaskCreatePinnedToCore(
    //     wifiTask,
    //     "WiFiTask",
    //     TASK_STACK_SIZE * 2,      // WiFi needs more stack
    //     nullptr,
    //     WIFI_TASK_PRIORITY,
    //     &g_wifi_task_handle,
    //     PRO_CPU_NUM               // Run on PRO CPU
    // );
    
    SYSTEM_LOGI(TAG_SYSTEM, "All tasks created successfully");
}

// ============================================================================
// SETUP (Called once at startup)
// ============================================================================

void setup() {
    // Initialize serial for logging
    Serial.begin(115200);
    delay(500);
    
    SYSTEM_LOGI(TAG_SYSTEM, "\n\n===========================================");
    SYSTEM_LOGI(TAG_SYSTEM, "SMV Data Acquisition Board");
    SYSTEM_LOGI(TAG_SYSTEM, "Initializing...");
    SYSTEM_LOGI(TAG_SYSTEM, "===========================================\n");
    //scan I2C bus for debugging
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);
    I2C_LOGI(TAG_SYSTEM, "Scanning I2C bus for devices...");
    byte error, address;
    int nDevices = 0;
    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            I2C_LOGI(TAG_SYSTEM, "I2C device found at address 0x%02X", address);
            nDevices++;
        }
    }
    if (nDevices == 0) {
        I2C_LOGI(TAG_SYSTEM, "No I2C devices found");
    } else {
        I2C_LOGI(TAG_SYSTEM, "Total I2C devices found: %d", nDevices);
    }

    //initBLE
    initBLE();
    
    // Log struct offsets for Android deserializer verification
    // logStructOffsets();
    
    // Initialize all systems
    initializeSystems();
    
    // Create FreeRTOS tasks
    createTasks();
    
    SYSTEM_LOGI(TAG_SYSTEM, "Setup complete! Starting main loop...");
}

// ============================================================================
// LOOP (Main loop running on core 0)
// ============================================================================

void loop() {
    // Monitor system health every 5 seconds
    static volatile uint32_t last_log_time = 0;
    
    if (millis() - last_log_time >= 5000) {
        last_log_time = millis();
        
        // Log system status
        SYSTEM_LOGI(TAG_SYSTEM, "--- System Status ---");
        SYSTEM_LOGI(TAG_SYSTEM, "Uptime: %ld ms", millis());
        SYSTEM_LOGI(TAG_SYSTEM, "Free heap: %d bytes", esp_get_free_heap_size());
        
        if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(100))) {
            // Log sensor data if valid
            if (g_data_sensor.flags.mpu6500_valid) {
                SYSTEM_LOGI(TAG_SYSTEM, "MPU6500: Accel(%.2f, %.2f, %.2f) m/s², Temp: %.1f°C",
                    g_data_sensor.mpu6500.accel.x,
                    g_data_sensor.mpu6500.accel.y,
                    g_data_sensor.mpu6500.accel.z,
                    g_data_sensor.mpu6500.temperature);
            }
            
            if (g_data_sensor.flags.ads1115_valid) {
                SYSTEM_LOGI(TAG_SYSTEM, "ADS1115: Voltage: %.2fV, Current: %.3fA, Power: %.2fW",
                    g_data_sensor.ads1115.voltage,
                    g_data_sensor.ads1115.current,
                    g_data_sensor.calc.power);
            }
            
            if (g_data_sensor.flags.calc_valid) {
                SYSTEM_LOGI(TAG_SYSTEM, "Calculated: Energy: %.2f kWh, Distance: %.2f m, AvgSpeed: %.2f km/h",
                    g_data_sensor.calc.energy_kwh,
                    g_data_sensor.calc.total_distance_m,
                    g_data_sensor.calc.avg_speed_kmh);
                SYSTEM_LOGI(TAG_SYSTEM, "Calc: Gear: %d, State: %d (0=idle,1=pull,2=glide), Pull: %.1fs, Glide: %.1fs",
                    g_data_sensor.calc.current_gear,
                    g_data_sensor.calc.drive_status,
                    g_data_sensor.calc.pull_duration_s,
                    g_data_sensor.calc.glide_duration_s);
            }
            
            if (g_data_sensor.flags.speed_valid) {
                SYSTEM_LOGI(TAG_SYSTEM, "Speed: %.2f km/h, Pulses: %ld",
                    g_data_sensor.speed.speed_kmh,
                    g_data_sensor.speed.pulse_count);
            }
            
            if (g_data_sensor.flags.gps_valid) {
                SYSTEM_LOGI(TAG_SYSTEM, "GPS: Lat: %.6f, Lon: %.6f, Sats: %d, Speed: %.1f km/h",
                    g_data_sensor.gps.latitude,
                    g_data_sensor.gps.longitude,
                    g_data_sensor.gps.satellites,
                    g_data_sensor.gps.speed_kmh);
            }
            
            if (g_data_sensor.flags.env_valid) {
                SYSTEM_LOGI(TAG_SYSTEM, "Environment: Temp: %.2f°C, Humidity: %.2f%%",
                    g_data_sensor.env.temperature,
                    g_data_sensor.env.humidity);
            }
            
            SYSTEM_LOGI(TAG_SYSTEM, "WiFi: %s, CAN: %s",
                g_data_sensor.status.wifi_connected ? "Connected" : "Disconnected",
                g_can_manager.isInitialized() ? "Active" : "Inactive");
            
            xSemaphoreGive(g_data_sensor_mutex);
        }
    }

    //BLE notification with 50ms interval (Full DataSensor_t struct)
    static uint32_t last_ble_time = 0;
    static uint32_t ble_send_count = 0;
    static bool size_info_logged = false;
    
    if(millis() - last_ble_time >= 50)
    {
        last_ble_time = millis();
        if (deviceConnected && g_data_sensor.flags.ble_init)
        {
            // Send full DataSensor_t struct (MTU 512 supports this)
            uint16_t payload_size = serializeToBluetoothPayload(&g_data_sensor);
            
            pCharacteristic->setValue((uint8_t*)&g_data_sensor, payload_size);
            pCharacteristic->notify();
            ble_send_count++;
            
            // Log transmit config once on first send
            if (!size_info_logged) {
                BLE_LOGI(TAG_SYSTEM, "%s", getPayloadTransmitInfo());
                size_info_logged = true;
            }
            
            // Log active sensors every 20 sends (1 second)
            if (ble_send_count % 20 == 0) {
                BLE_LOGI(TAG_SYSTEM, "BLE TX: Message #%lu, Payload: %u bytes", ble_send_count, payload_size);
                logActiveSensors(&g_data_sensor);
                
                // Log key sensor values for verification
                BLE_LOGI(TAG_SYSTEM, "BLE Data Sample: V=%.2fV I=%.2fA Spd=%.1f km/h Pwr=%.1fW T=%.1f°C",
                    g_data_sensor.ads1115.voltage,
                    g_data_sensor.ads1115.current,
                    g_data_sensor.speed.speed_kmh,
                    g_data_sensor.calc.power,
                    g_data_sensor.env.temperature);
            }
        }
    
    }
    
    // Blink LED to show system is running
    static uint32_t last_blink_time = 0;
    static bool led_state = false;
    
    if (millis() - last_blink_time >= (led_state ? LED_ON_MS : LED_OFF_MS)) {
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state ? HIGH : LOW);
        last_blink_time = millis();
    }
    
    // delay(100);
}
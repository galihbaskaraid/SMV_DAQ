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

// BLE — uses built-in ESP32 BLE library (bluedroid)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs and notification intervals defined in constants.h

BLEServer*         pServer      = nullptr;
BLECharacteristic* pChar_Power  = nullptr;  // 24 B @ 20 Hz — voltage, current, power, energy
BLECharacteristic* pChar_Speed  = nullptr;  // 20 B @ 20 Hz — speed, distance
BLECharacteristic* pChar_IMU    = nullptr;  // 32 B @ 20 Hz — accel, gyro
BLECharacteristic* pChar_GPS    = nullptr;  // 30 B @  1 Hz — lat, lon, altitude
BLECharacteristic* pChar_Env    = nullptr;  // 12 B @  1 Hz — ambient temp, humidity
BLECharacteristic* pChar_Calc   = nullptr;  // 24 B @  5 Hz — gear, drive state
BLECharacteristic* pChar_Status = nullptr;  // 10 B @  1 Hz — heap, uptime, flags
bool deviceConnected = false;

// MTU negotiation tracking — prevent race condition with BLE data transmission
bool mtuConfigured = false;
uint32_t deviceConnectTime = 0;
const uint32_t MTU_WAIT_DELAY_MS = 1000;  // Wait 1 second after connect before sending data
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
        mtuConfigured = false;  // Reset flag — MTU exchange hasn't happened yet
        deviceConnectTime = millis();  // Record connection timestamp
        BLE_LOGI(TAG_SYSTEM, "BLE Client connected — MTU exchange in progress...");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        mtuConfigured = false;  // Reset flag on disconnect
        deviceConnectTime = 0;
        BLE_LOGI(TAG_SYSTEM, "BLE Client disconnected");
        pServer->startAdvertising();
    }
};

void initBLE()
{
    BLE_LOGI(TAG_SYSTEM, "Initializing BLE...");

    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(512);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Allocate enough handles for 7 characteristics with descriptors (1 + 7×2 + 7×1 = 22 minimum)
    BLEService* pService = pServer->createService(BLEUUID(BLE_SERVICE_UUID), 25);

    // One notify characteristic per data category
    pChar_Power  = pService->createCharacteristic(BLEUUID(BLE_CHAR_POWER_UUID),  BLECharacteristic::PROPERTY_NOTIFY);
    pChar_Speed  = pService->createCharacteristic(BLEUUID(BLE_CHAR_SPEED_UUID),  BLECharacteristic::PROPERTY_NOTIFY);
    pChar_IMU    = pService->createCharacteristic(BLEUUID(BLE_CHAR_IMU_UUID),    BLECharacteristic::PROPERTY_NOTIFY);
    pChar_GPS    = pService->createCharacteristic(BLEUUID(BLE_CHAR_GPS_UUID),    BLECharacteristic::PROPERTY_NOTIFY);
    pChar_Env    = pService->createCharacteristic(BLEUUID(BLE_CHAR_ENV_UUID),    BLECharacteristic::PROPERTY_NOTIFY);
    pChar_Calc   = pService->createCharacteristic(BLEUUID(BLE_CHAR_CALC_UUID),   BLECharacteristic::PROPERTY_NOTIFY);
    pChar_Status = pService->createCharacteristic(BLEUUID(BLE_CHAR_STATUS_UUID), BLECharacteristic::PROPERTY_NOTIFY);

    pChar_Power ->addDescriptor(new BLE2902());
    pChar_Speed ->addDescriptor(new BLE2902());
    pChar_IMU   ->addDescriptor(new BLE2902());
    pChar_GPS   ->addDescriptor(new BLE2902());
    pChar_Env   ->addDescriptor(new BLE2902());
    pChar_Calc  ->addDescriptor(new BLE2902());
    pChar_Status->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->start();

    g_data_sensor.flags.ble_init = true;
    BLE_LOGI(TAG_SYSTEM, "✓ BLE initialized: 7 characteristics on service %s", BLE_SERVICE_UUID);
    BLE_LOGI(TAG_SYSTEM, "  Power=%dB Speed=%dB IMU=%dB GPS=%dB Env=%dB Calc=%dB Status=%dB",
        (int)sizeof(BLE_PowerPayload_t),  (int)sizeof(BLE_SpeedPayload_t),
        (int)sizeof(BLE_IMUPayload_t),    (int)sizeof(BLE_GPSPayload_t),
        (int)sizeof(BLE_EnvPayload_t),    (int)sizeof(BLE_CalcPayload_t),
        (int)sizeof(BLE_StatusPayload_t));
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
        PRO_CPU_NUM               // Moved to PRO CPU (Core 0) to reduce APP CPU load
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
    delay(100);
    pinMode(LED_PIN, OUTPUT);
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

    // BLE notifications: separate update rates per data category
    // NOTE: Only transmit data after MTU exchange is complete (delay-based approach)
    static uint32_t ble_last_fast   = 0;  // 20 Hz — Power, Speed, IMU
    static uint32_t ble_last_medium = 0;  //  5 Hz — Calc / drive state
    static uint32_t ble_last_slow   = 0;  //  1 Hz — GPS, Environment, Status
    static uint32_t ble_send_count  = 0;

    // Check if MTU exchange has completed (wait MTU_WAIT_DELAY_MS ms after connection)
    if (deviceConnected && !mtuConfigured && deviceConnectTime > 0) {
        if (millis() - deviceConnectTime >= MTU_WAIT_DELAY_MS) {
            mtuConfigured = true;
            BLE_LOGI(TAG_SYSTEM, "✓ MTU exchange period elapsed — beginning BLE data transmission");
        }
    }

    // Only send data if MTU is configured (race condition prevention)
    if (deviceConnected && mtuConfigured && g_data_sensor.flags.ble_init) {
        uint32_t now = millis();

        // Fast characteristics @ 20 Hz — Power, Speed, IMU
        if (now - ble_last_fast >= BLE_FAST_INTERVAL_MS) {
            ble_last_fast = now;
            BLE_PowerPayload_t pwr = {}; // Zero-initialize
            BLE_SpeedPayload_t spd = {};
            BLE_IMUPayload_t   imu = {};
            
            if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(10))) {
                serializePower(&g_data_sensor, &pwr);
                serializeSpeed(&g_data_sensor, &spd);
                serializeIMU  (&g_data_sensor, &imu);
                xSemaphoreGive(g_data_sensor_mutex);
                
                // Only set value and notify if mutex was successfully acquired
                pChar_Power->setValue((uint8_t*)&pwr, sizeof(pwr));  pChar_Power->notify();
                pChar_Speed->setValue((uint8_t*)&spd, sizeof(spd));  pChar_Speed->notify();
                pChar_IMU  ->setValue((uint8_t*)&imu, sizeof(imu));  pChar_IMU->notify();
                ble_send_count++;
            } else {
                BLE_LOGW(TAG_SYSTEM, "BLE Fast: Mutex busy, skipping frame");
            }
        }

        // Medium characteristics @ 5 Hz — Calc / drive state
        if (now - ble_last_medium >= BLE_MEDIUM_INTERVAL_MS) {
            ble_last_medium = now;
            BLE_CalcPayload_t calc = {}; // Zero-initialize
            if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(10))) {
                serializeCalc(&g_data_sensor, &calc);
                xSemaphoreGive(g_data_sensor_mutex);
                
                pChar_Calc->setValue((uint8_t*)&calc, sizeof(calc));  pChar_Calc->notify();
            } else {
                BLE_LOGW(TAG_SYSTEM, "BLE Medium: Mutex busy, skipping frame");
            }
        }

        // Slow characteristics @ 1 Hz — GPS, Environment, Status
        if (now - ble_last_slow >= BLE_SLOW_INTERVAL_MS) {
            ble_last_slow = now;
            BLE_GPSPayload_t    gps  = {}; // Zero-initialize
            BLE_EnvPayload_t    env  = {};
            BLE_StatusPayload_t stat = {};
            
            if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(20))) {
                serializeGPS   (&g_data_sensor, &gps);
                serializeEnv   (&g_data_sensor, &env);
                serializeStatus(&g_data_sensor, &stat);
                xSemaphoreGive(g_data_sensor_mutex);
                
                pChar_GPS   ->setValue((uint8_t*)&gps,  sizeof(gps));   pChar_GPS->notify();
                pChar_Env   ->setValue((uint8_t*)&env,  sizeof(env));   pChar_Env->notify();
                pChar_Status->setValue((uint8_t*)&stat, sizeof(stat));  pChar_Status->notify();
            } else {
                BLE_LOGW(TAG_SYSTEM, "BLE Slow: Mutex busy, skipping frame");
            }

            if (ble_send_count % 10 == 0) {
                BLE_LOGI(TAG_SYSTEM, "BLE TX #%lu — fast×%lu | Pwr=%.1fW Spd=%.1fkm/h T=%.1f°C GPS:%d sats",
                    ble_send_count, ble_send_count,
                    g_data_sensor.calc.power,
                    g_data_sensor.speed.speed_kmh,
                    g_data_sensor.env.temperature,
                    g_data_sensor.gps.satellites);
                logActiveSensors(&g_data_sensor);
            }
        }
    }
    
    // Blink LED to show system is running blinking pattern: 100ms on, 900ms off (10% duty cycle)
    static uint32_t last_blink_time = 0;
    static bool led_state = false;
    if (millis() - last_blink_time >= 1000) {
        last_blink_time = millis();
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state ? HIGH : LOW);
    }
    
    // delay(100);
    vTaskDelay(pdMS_TO_TICKS(1));  // Yield to other tasks
}
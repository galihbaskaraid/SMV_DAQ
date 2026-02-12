// ============================================================================
// EXAMPLE APPLICATION: OTA (Over-The-Air) FIRMWARE UPDATE
// ============================================================================
// Enable remote firmware updates via WiFi

#ifndef EXAMPLE_OTA_UPDATE_H
#define EXAMPLE_OTA_UPDATE_H

#include <ArduinoOTA.h>
#include "constants.h"

class OTAUpdater {
private:
    bool initialized;
    
public:
    OTAUpdater() : initialized(false) {}
    
    bool init() {
        ArduinoOTA.setHostname("SMV-DAQ-Board");
        
        ArduinoOTA.onStart([]() {
            ESP_LOGI(TAG_SYSTEM, "OTA Update Started");
        });
        
        ArduinoOTA.onEnd([]() {
            ESP_LOGI(TAG_SYSTEM, "OTA Update Completed");
        });
        
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            ESP_LOGD(TAG_SYSTEM, "OTA Progress: %u%%", (progress / (total / 100)));
        });
        
        ArduinoOTA.onError([](ota_error_t error) {
            ESP_LOGE(TAG_SYSTEM, "OTA Error: %u", error);
        });
        
        ArduinoOTA.begin();
        initialized = true;
        
        ESP_LOGI(TAG_SYSTEM, "OTA Updater initialized");
        return true;
    }
    
    void handle() {
        if (initialized) {
            ArduinoOTA.handle();
        }
    }
};

#endif // EXAMPLE_OTA_UPDATE_H

// ============================================================================
// USAGE:
// ============================================================================

/*

#include "examples/ota_update.h"

OTAUpdater ota;

void setup() {
    // ... existing setup code ...
    
    // Initialize OTA after WiFi is ready
    if (g_wifi_manager.isConnected()) {
        ota.init();
    }
}

void loop() {
    // ... existing loop code ...
    
    // Call OTA handler in main loop
    ota.handle();
    
    delay(10);
}

// To upload via OTA:
// pio run -e esp32doit-devkit-v1 -t upload --upload-port 192.168.1.100

*/

#include "debug_logging.h"

DebugRuntimeFlags g_debug_runtime = {
    true,   // system_if
    true,   // i2c
    false,  // spi
    true,   // uart
    true,   // can_bus
    true,   // gps
    false,  // wifi
    false,  // http
    true,   // ble
    true,   // sensor
    false   // calc
};

void setAllDebugInterfaces(bool enabled) {
    g_debug_runtime.system_if = enabled;
    g_debug_runtime.i2c = enabled;
    g_debug_runtime.spi = enabled;
    g_debug_runtime.uart = enabled;
    g_debug_runtime.can_bus = enabled;
    g_debug_runtime.gps = enabled;
    g_debug_runtime.wifi = enabled;
    g_debug_runtime.http = enabled;
    g_debug_runtime.ble = enabled;
    g_debug_runtime.sensor = enabled;
    g_debug_runtime.calc = enabled;
}

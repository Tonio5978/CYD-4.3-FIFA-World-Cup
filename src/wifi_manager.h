#pragma once
#include <Arduino.h>

namespace WifiManager {
    void begin();           // connect (blocking up to WIFI_TIMEOUT_MS)
    void loop();            // call from main loop – handles reconnection
    bool isConnected();
}

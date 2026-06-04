#include "wifi_manager.h"
#include <WiFi.h>
#include "../include/config.h"
#include "../include/screens.h"

static uint32_t _lastReconnectMs = 0;
static const uint32_t RECONNECT_INTERVAL_MS = 30000;

void WifiManager::begin() {
    DBG("[WiFi] Connecting to %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(200);
    }

    gCtx.wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (gCtx.wifiConnected) {
        DBG("[WiFi] Connected – IP %s\n", WiFi.localIP().toString().c_str());
    } else {
        DBG("[WiFi] Connection failed\n");
    }
}

void WifiManager::loop() {
    bool connected = (WiFi.status() == WL_CONNECTED);

    if (!connected) {
        gCtx.wifiConnected = false;
        uint32_t now = millis();
        if (now - _lastReconnectMs > RECONNECT_INTERVAL_MS) {
            _lastReconnectMs = now;
            DBG("[WiFi] Reconnecting...\n");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    } else if (!gCtx.wifiConnected) {
        gCtx.wifiConnected = true;
        DBG("[WiFi] Reconnected – IP %s\n", WiFi.localIP().toString().c_str());
    }
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

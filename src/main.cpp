#include <Arduino.h>

// Display
#include "display_config.h"
LGFX gfx;   // global display instance (declared extern in display_config.h)

// App modules
#include "wifi_manager.h"
#include "ntp_time.h"
#include "storage.h"
#include "espn_api.h"
#include "ui_manager.h"
#include "screen_splash.h"
#include "screen_home.h"
#include "screen_group.h"
#include "../include/screens.h"
#include "../include/config.h"

// Global app context (declared extern in screens.h)
AppContext gCtx;

// ============================================================
// API refresh interval (adaptive)
// ============================================================
static uint32_t getScoreboardInterval() {
    return EspnApi::hasLiveMatch()
        ? API_REFRESH_LIVE_MS
        : API_REFRESH_IDLE_MS;
}

static const char* stateName(AppState s) {
    switch (s) {
        case STATE_SPLASH:    return "SPLASH";
        case STATE_HOME_LIVE: return "HOME_LIVE";
        case STATE_HOME_NEXT: return "HOME_NEXT";
        case STATE_GROUP:     return "GROUP";
        default:              return "?";
    }
}

// ============================================================
// State-machine render
// ============================================================
static void renderCurrentScreen() {
    switch (gCtx.appState) {
        case STATE_SPLASH:   ScreenSplash::draw(); break;
        case STATE_HOME_LIVE:
        case STATE_HOME_NEXT: ScreenHome::draw();  break;
        case STATE_GROUP:    ScreenGroup::draw();  break;
    }
    gCtx.needsFullRedraw = false;
}

static void updateCurrentScreen() {
    switch (gCtx.appState) {
        case STATE_SPLASH:    ScreenSplash::update(); break;
        case STATE_HOME_LIVE:
        case STATE_HOME_NEXT: ScreenHome::update();   break;
        case STATE_GROUP:     ScreenGroup::update();  break;
    }
}

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    DBG("[BOOT] FIFA World Cup 2026\n");

    // Diagnostic PSRAM — doit afficher > 0 sinon PSRAM non initialisee
    DBG("[BOOT] PSRAM dispo : %s  taille : %lu B  libre : %lu B\n",
        psramFound() ? "OUI" : "NON",
        (unsigned long)ESP.getPsramSize(),
        (unsigned long)ESP.getFreePsram());
    DBG("[BOOT] Heap interne libre : %lu B\n", (unsigned long)ESP.getFreeHeap());

    // Display init (initialise aussi le tactile)
    gfx.init();
    gfx.setBrightness(200);
    gfx.setRotation(0);
    gfx.fillScreen(TFT_BLACK);
    DBG("[DISP] Display init OK – 800x480, brightness=200\n");

    // Splash screen – show immediately before network calls
    ScreenSplash::draw();

    // Storage
    Storage::begin();

    // WiFi
    WifiManager::begin();
    ScreenSplash::draw();   // refresh -> coche WiFi

    // NTP (requires WiFi)
    if (gCtx.wifiConnected) {
        NtpTime::begin();
        ScreenSplash::draw();   // refresh -> coche NTP
    }

    // Initial data fetch
    if (gCtx.wifiConnected) {
        EspnApi::fetchScoreboard();
        EspnApi::fetchStandings();
        gCtx.lastScoreboardFetch  = millis();
        gCtx.lastStandingsFetch   = millis();
        ScreenSplash::draw();   // refresh -> coche API
        delay(600);             // laisse voir les 3 coches avant de basculer
    }

    // Transition from splash
    AppState initialState = EspnApi::hasLiveMatch() ? STATE_HOME_LIVE : STATE_HOME_NEXT;
    gCtx.appState = initialState;
    gCtx.needsFullRedraw = true;
    DBG("[STATE] -> %s\n", stateName(initialState));

    DBG("[BOOT] Setup complete – heap free: %lu B, PSRAM free: %lu B\n",
        (unsigned long)ESP.getFreeHeap(),
        (unsigned long)ESP.getFreePsram());
    DBG("[BOOT] Matches: %d  Groups: %d\n",
        (int)gCtx.matches.size(), (int)gCtx.standings.size());
}

// ============================================================
// loop()
// ============================================================
void loop() {
    uint32_t now = millis();

    // 1. Network maintenance
    WifiManager::loop();
    NtpTime::loop();

    // 2. API refresh
    if (gCtx.wifiConnected) {
        if (now - gCtx.lastScoreboardFetch > getScoreboardInterval()) {
            gCtx.lastScoreboardFetch = now;
            DBG("[API] Scoreboard refresh (live=%s interval=%lus)\n",
                EspnApi::hasLiveMatch() ? "oui" : "non",
                (unsigned long)getScoreboardInterval() / 1000);
            EspnApi::fetchScoreboard();
            // Switch home state based on live matches
            if (gCtx.appState == STATE_HOME_LIVE || gCtx.appState == STATE_HOME_NEXT) {
                AppState newState = EspnApi::hasLiveMatch()
                    ? STATE_HOME_LIVE : STATE_HOME_NEXT;
                if (newState != gCtx.appState) {
                    DBG("[STATE] %s -> %s\n", stateName(gCtx.appState), stateName(newState));
                    gCtx.appState = newState;
                    gCtx.needsFullRedraw = true;
                }
            }
        }
        if (now - gCtx.lastStandingsFetch > API_REFRESH_STANDINGS_MS) {
            gCtx.lastStandingsFetch = now;
            DBG("[API] Standings refresh\n");
            EspnApi::fetchStandings();
        }
    }

    // 3. Goal popup (highest priority – drawn over everything else)
    if (gCtx.appState != STATE_SPLASH) {
        UI::updateGoalPopup();
    }

    // 4. Full redraw if needed
    if (gCtx.needsFullRedraw) {
        renderCurrentScreen();
    }

    // 5. Partial updates (clock, live dot)
    updateCurrentScreen();

    // 6. Touch
    int tx, ty;
    if (UI::getTap(tx, ty)) {
        DBG("[TOUCH] Tap (%d, %d) etat=%s\n", tx, ty, stateName(gCtx.appState));
        UI::handleFooterTouch(tx, ty);
        if (gCtx.needsFullRedraw) renderCurrentScreen();
    }

    delay(16);  // ~60 fps ceiling
}

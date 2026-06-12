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
    if (EspnApi::hasLiveMatch())                       return API_REFRESH_LIVE_MS;     // 10 s
    if (EspnApi::hasImminentMatch(PRELIVE_WINDOW_SEC)) return API_REFRESH_PRELIVE_MS;  // 30 s (pre-live)
    return API_REFRESH_IDLE_MS;                                                        // 5 min
}

// Etat home a adopter selon les donnees (live > pre-live > prochains)
static AppState homeStateNow() {
    if (EspnApi::hasLiveMatch())                       return STATE_HOME_LIVE;
    if (EspnApi::hasImminentMatch(PRELIVE_WINDOW_SEC)) return STATE_HOME_PRELIVE;
    return STATE_HOME_NEXT;
}

static bool isHomeState(AppState s) {
    return s == STATE_HOME_LIVE || s == STATE_HOME_PRELIVE || s == STATE_HOME_NEXT;
}

static const char* stateName(AppState s) {
    switch (s) {
        case STATE_SPLASH:      return "SPLASH";
        case STATE_HOME_LIVE:   return "HOME_LIVE";
        case STATE_HOME_PRELIVE:return "HOME_PRELIVE";
        case STATE_HOME_NEXT:   return "HOME_NEXT";
        case STATE_GROUP:       return "GROUP";
        default:                return "?";
    }
}

// ============================================================
// State-machine render
// ============================================================
static void renderCurrentScreen() {
    switch (gCtx.appState) {
        case STATE_SPLASH:   ScreenSplash::draw(); break;
        case STATE_HOME_LIVE:
        case STATE_HOME_PRELIVE:
        case STATE_HOME_NEXT: ScreenHome::draw();  break;
        case STATE_GROUP:    ScreenGroup::draw();  break;
    }
    gCtx.needsFullRedraw = false;
}

static void updateCurrentScreen() {
    switch (gCtx.appState) {
        case STATE_SPLASH:    ScreenSplash::update(); break;
        case STATE_HOME_LIVE:
        case STATE_HOME_PRELIVE:
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

    // Storage (monte LittleFS) AVANT le splash pour que le logo s'affiche
    Storage::begin();

    // Splash screen – show immediately before network calls
    ScreenSplash::draw();

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
    AppState initialState = homeStateNow();
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
            // Live OU pre-live : refresh leger du jour (fusion) pour ne pas
            // bloquer le tactile. Sinon (idle) : rechargement complet.
            bool light = EspnApi::hasLiveMatch()
                      || EspnApi::hasImminentMatch(PRELIVE_WINDOW_SEC);
            DBG("[API] Scoreboard refresh (%s interval=%lus)\n",
                light ? "leger" : "complet",
                (unsigned long)getScoreboardInterval() / 1000);
            if (light) EspnApi::fetchLiveScoreboard();
            else       EspnApi::fetchScoreboard();
            // Bascule d'etat home : live > pre-live > prochains
            if (isHomeState(gCtx.appState)) {
                AppState newState = homeStateNow();
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

    // Tant qu'une popup de but est affichee, on suspend tout rendu d'ecran
    // (sinon il dessinerait par-dessus/dessous -> scintillement).
    bool popupActive = (gCtx.popupState != POPUP_NONE);

    // 4. Full redraw if needed
    if (gCtx.needsFullRedraw && !popupActive) {
        renderCurrentScreen();
    }

    // 5. Partial updates (clock, live dot)
    if (!popupActive) {
        updateCurrentScreen();
    }

    // 6. Touch
    int tx, ty;
    if (UI::getTap(tx, ty)) {
        DBG("[TOUCH] Tap (%d, %d) etat=%s\n", tx, ty, stateName(gCtx.appState));
        UI::handleFooterTouch(tx, ty);
        if (gCtx.needsFullRedraw) renderCurrentScreen();
    }

    delay(16);  // ~60 fps ceiling
}

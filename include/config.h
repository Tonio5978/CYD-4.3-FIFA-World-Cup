#pragma once

// ============================================================
// WiFi
// ============================================================
#define WIFI_SSID       "Livebox-EFA9"
#define WIFI_PASSWORD   "3FF34CC1AE9ECF230C3C9392A3"
#define WIFI_TIMEOUT_MS 20000

// ============================================================
// NTP
// ============================================================
#define NTP_SERVER              "pool.ntp.org"
#define NTP_TIMEZONE            "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Paris
#define NTP_UPDATE_INTERVAL_MS  3600000UL   // 1 hour

// ============================================================
// ESPN API
// ============================================================
// Liste complete de la competition (104 matchs, ~760 Ko) – boot + idle
#define API_SCOREBOARD_URL \
    "https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard?limit=950&dates=20260611-20260720"
// Scoreboard du jour uniquement (leger) – rafraichissement live toutes les 10 s.
// Evite de re-telecharger toute la competition (bloquait le tactile).
#define API_SCOREBOARD_LIVE_URL \
    "https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard"
#define API_STANDINGS_URL \
    "https://site.web.api.espn.com/apis/v2/sports/soccer/fifa.world/standings"

#define API_REFRESH_LIVE_MS      10000UL    // 10 s when match in progress
#define API_REFRESH_PRELIVE_MS   30000UL    // 30 s : fenetre PRE-LIVE (avant coup d'envoi)
#define API_REFRESH_IDLE_MS     300000UL    // 5 min when no live match
#define API_REFRESH_STANDINGS_MS 600000UL   // 10 min
#define HTTP_TIMEOUT_MS          15000

// Fenetre PRE-LIVE : on passe en attente du match ce nombre de secondes
// avant le coup d'envoi programme (puis LIVE des que le match demarre).
#define PRELIVE_WINDOW_SEC       300        // 5 minutes

// ============================================================
// Display
// ============================================================
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

// Theme colours (RGB565)
#define COLOR_PRIMARY    0x003F7F    // FIFA blue  #003F7F
#define COLOR_SECONDARY  0xFFD700    // FIFA gold  #FFD700
#define COLOR_BG         0x1A1A1A    // Dark bg
#define COLOR_TEXT       0xFFFFFF    // White
#define COLOR_TEXT_DIM   0xCCCCCC    // Light grey
#define COLOR_SUCCESS    0x00C853    // Green (win)
#define COLOR_DRAW       0xFFA000    // Amber (draw)
#define COLOR_LOSS       0xD32F2F    // Red (loss)
#define COLOR_LIVE_RED   0xFF0000    // Live dot

// Header / Footer heights
#define HEADER_H  60
#define FOOTER_H  60
#define CONTENT_Y HEADER_H
#define CONTENT_H (SCREEN_HEIGHT - HEADER_H - FOOTER_H)

// ============================================================
// Goal popup
// ============================================================
#define ENABLE_GOAL_POPUP           true
#define GOAL_POPUP_DURATION_MS      10000
#define GOAL_POPUP_ANIM_IN_MS       500
#define GOAL_POPUP_ANIM_OUT_MS      300
#define GOAL_POPUP_W                600
#define GOAL_POPUP_H                400
#define GOAL_POPUP_X                ((SCREEN_WIDTH  - GOAL_POPUP_W) / 2)   // 100
#define GOAL_POPUP_Y                ((SCREEN_HEIGHT - GOAL_POPUP_H) / 2)   // 40
#define GOAL_POPUP_FLAG_W           200
#define GOAL_POPUP_FLAG_H           150
#define GOAL_POPUP_BG_DIM           128    // 0-255 overlay opacity

// ============================================================
// Assets  (SD card paths)
// ============================================================
#define FLAG_DIR       "/flags"
#define SOUNDS_DIR     "/sounds"
#define GOAL_SOUND_FILE "/sounds/goal_sound.wav"

// Flag sizes
#define FLAG_SM_W  32
#define FLAG_SM_H  24
#define FLAG_MD_W  64
#define FLAG_MD_H  48
#define FLAG_LG_W  GOAL_POPUP_FLAG_W
#define FLAG_LG_H  GOAL_POPUP_FLAG_H

// ============================================================
// Audio (optional)
// ============================================================
#define ENABLE_GOAL_SOUND   false
#define I2S_BCLK_PIN        17
#define I2S_LRC_PIN         47
#define I2S_DOUT_PIN        21

// ============================================================
// Touch  (GT911 on I2C)
// ============================================================
#define TOUCH_SDA_PIN   19
#define TOUCH_SCL_PIN   20
#define TOUCH_INT_PIN   18
#define TOUCH_RST_PIN   38

// ============================================================
// Storage
// ============================================================
#define USE_SD_CARD     true
#define SD_CS_PIN       10

// ============================================================
// Debug
// ============================================================
#define DEBUG_SERIAL    true
#define SERIAL_BAUD     115200

#if DEBUG_SERIAL
  #define DBG(...)  Serial.printf(__VA_ARGS__)
#else
  #define DBG(...)
#endif

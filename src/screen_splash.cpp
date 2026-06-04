#include "screen_splash.h"
#include "display_config.h"
#include "ui_manager.h"
#include "../include/config.h"
#include "../include/screens.h"

static uint32_t _spinnerAngle = 0;
static uint32_t _lastSpinMs   = 0;

// Coche verte (mdi:check) a droite d'une ligne de statut quand c'est pret.
// Efface d'abord la zone (fond) pour gerer la transition en-cours -> pret.
static void drawCheck(int x, int y, bool done) {
    gfx.fillRect(x - 2, y - 12, 30, 24, gfx.color565(26, 26, 26));
    if (!done) return;
    uint16_t green = gfx.color565(0x00, 0xC8, 0x53);
    gfx.drawWideLine(x,      y + 3,  x + 6,  y + 9, 3, green);
    gfx.drawWideLine(x + 6,  y + 9,  x + 20, y - 7, 3, green);
}

static void drawSpinner(int cx, int cy, int r, uint32_t angleDeg) {
    // Erase previous arc area
    gfx.fillCircle(cx, cy, r + 4, gfx.color565(26, 26, 26));
    // Draw 12 dots around circle
    for (int i = 0; i < 12; i++) {
        float angle = (i * 30 + angleDeg) * DEG_TO_RAD;
        int x = cx + cos(angle) * r;
        int y = cy + sin(angle) * r;
        uint8_t bright = (uint8_t)(255 * (i + 1) / 12);
        gfx.fillCircle(x, y, 4, gfx.color565(bright, bright, bright));
    }
}

void ScreenSplash::draw() {
    DBG("[SPLASH] Draw (WiFi=%d NTP=%d API=%d)\n",
        gCtx.wifiConnected, gCtx.ntpSynced, gCtx.dataReady);
    gfx.fillScreen(gfx.color565(26, 26, 26));

    int cx = SCREEN_WIDTH  / 2;
    int cy = SCREEN_HEIGHT / 2 - 40;

    // Title
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
    gfx.drawString("FIFA WORLD CUP 2026", cx, cy - 100, &fonts::FreeSansBold18pt7b);

    // Sub-title
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString("Live Score Display", cx, cy - 65, &fonts::FreeSans9pt7b);

    // Spinner placeholder area
    drawSpinner(cx, cy + 20, 28, _spinnerAngle);

    // Status lines
    gfx.setTextColor(gfx.color565(0xCC, 0xCC, 0xCC));
    int sy = cy + 90;
    int checkX = cx + 100;
    String wifiStatus = gCtx.wifiConnected ? "WiFi: Connecte" : "WiFi: Connexion...";
    gfx.drawString(wifiStatus, cx, sy, &fonts::FreeSans9pt7b);
    drawCheck(checkX, sy, gCtx.wifiConnected);
    String ntpStatus = gCtx.ntpSynced ? "NTP: Synchronise" : "NTP: En cours...";
    gfx.drawString(ntpStatus, cx, sy + 25, &fonts::FreeSans9pt7b);
    drawCheck(checkX, sy + 25, gCtx.ntpSynced);
    String apiStatus = gCtx.dataReady ? "API: Pret !" : "API: Chargement...";
    gfx.drawString(apiStatus, cx, sy + 50, &fonts::FreeSans9pt7b);
    drawCheck(checkX, sy + 50, gCtx.dataReady);
}

void ScreenSplash::update() {
    uint32_t now = millis();
    if (now - _lastSpinMs > 80) {
        _lastSpinMs = now;
        _spinnerAngle = (_spinnerAngle + 30) % 360;
        // Redraw only the spinner + status area (partial update)
        int cx = SCREEN_WIDTH  / 2;
        int cy = SCREEN_HEIGHT / 2 - 20;
        drawSpinner(cx, cy + 20, 28, _spinnerAngle);

        gfx.setTextDatum(lgfx::middle_center);
        int sy = cy + 90;
        int checkX = cx + 100;
        gfx.setTextColor(gfx.color565(0xCC, 0xCC, 0xCC), gfx.color565(26, 26, 26));
        String wifiStatus = gCtx.wifiConnected ? "WiFi: Connecte  " : "WiFi: Connexion...";
        gfx.drawString(wifiStatus, cx, sy, &fonts::FreeSans9pt7b);
        drawCheck(checkX, sy, gCtx.wifiConnected);
        String ntpStatus = gCtx.ntpSynced ? "NTP: Synchronise  " : "NTP: En cours...  ";
        gfx.drawString(ntpStatus, cx, sy + 25, &fonts::FreeSans9pt7b);
        drawCheck(checkX, sy + 25, gCtx.ntpSynced);
        String apiStatus = gCtx.dataReady ? "API: Pret !       " : "API: Chargement...";
        gfx.drawString(apiStatus, cx, sy + 50, &fonts::FreeSans9pt7b);
        drawCheck(checkX, sy + 50, gCtx.dataReady);
    }
}

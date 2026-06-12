#include "screen_splash.h"
#include "display_config.h"
#include "ui_manager.h"
#include "storage.h"
#include "../include/config.h"
#include "../include/screens.h"

// ============================================================
// Layout (constantes partagees draw / update)
// ============================================================
static const int CX        = SCREEN_WIDTH / 2;
static const int LOGO_W    = 150;
static const int LOGO_H    = 150;
static const int LOGO_Y    = 12;
static const int TITLE_Y   = 192;
static const int SUB_Y     = 224;
static const int SPIN_CY   = 268;
static const int SPIN_R    = 18;
static const int STAT_Y0   = 312;
static const int STAT_DY   = 24;
static const int CHECK_X   = CX + 100;

static uint32_t _spinnerAngle = 0;
static uint32_t _lastSpinMs   = 0;

// Logo Coupe du Monde charge une fois depuis LittleFS (/logo.png) en PSRAM.
static uint16_t* _logoBuf  = nullptr;
static bool      _logoTried = false;

static uint16_t* getLogo() {
    if (_logoBuf)  return _logoBuf;
    if (_logoTried) return nullptr;          // echec deja constate
    if (!Storage::isReady()) return nullptr;  // FS pas encore monte -> reessaie plus tard
    _logoTried = true;
    uint16_t* buf = (uint16_t*)ps_malloc((size_t)LOGO_W * LOGO_H * 2);
    if (buf && Storage::loadImageRGB565("/logo.png", buf, LOGO_W, LOGO_H)) {
        _logoBuf = buf;
        return _logoBuf;
    }
    if (buf) free(buf);
    return nullptr;
}

// Coche verte (mdi:check) a droite d'une ligne de statut quand c'est pret.
static void drawCheck(int x, int y, bool done) {
    gfx.fillRect(x - 2, y - 12, 30, 24, gfx.color565(26, 26, 26));
    if (!done) return;
    uint16_t green = gfx.color565(0x00, 0xC8, 0x53);
    gfx.drawWideLine(x,     y + 3, x + 6,  y + 9, 3, green);
    gfx.drawWideLine(x + 6, y + 9, x + 20, y - 7, 3, green);
}

static void drawSpinner(uint32_t angleDeg) {
    gfx.fillCircle(CX, SPIN_CY, SPIN_R + 4, gfx.color565(26, 26, 26));
    for (int i = 0; i < 12; i++) {
        float angle = (i * 30 + angleDeg) * DEG_TO_RAD;
        int x = CX + cos(angle) * SPIN_R;
        int y = SPIN_CY + sin(angle) * SPIN_R;
        uint8_t bright = (uint8_t)(255 * (i + 1) / 12);
        gfx.fillCircle(x, y, 4, gfx.color565(bright, bright, bright));
    }
}

// Lignes de statut + coches. Texte dessine avec fond -> efface l'ancien.
static void drawStatusLines() {
    uint16_t bg = gfx.color565(26, 26, 26);
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xCC, 0xCC, 0xCC), bg);

    String wifi = gCtx.wifiConnected ? "WiFi: Connecte    " : "WiFi: Connexion...";
    gfx.drawString(wifi, CX, STAT_Y0, &fonts::FreeSans9pt7b);
    drawCheck(CHECK_X, STAT_Y0, gCtx.wifiConnected);

    String ntp = gCtx.ntpSynced ? "NTP: Synchronise  " : "NTP: En cours...  ";
    gfx.drawString(ntp, CX, STAT_Y0 + STAT_DY, &fonts::FreeSans9pt7b);
    drawCheck(CHECK_X, STAT_Y0 + STAT_DY, gCtx.ntpSynced);

    String api = gCtx.dataReady ? "API: Pret !       " : "API: Chargement...";
    gfx.drawString(api, CX, STAT_Y0 + 2 * STAT_DY, &fonts::FreeSans9pt7b);
    drawCheck(CHECK_X, STAT_Y0 + 2 * STAT_DY, gCtx.dataReady);
}

void ScreenSplash::draw() {
    DBG("[SPLASH] Draw (WiFi=%d NTP=%d API=%d)\n",
        gCtx.wifiConnected, gCtx.ntpSynced, gCtx.dataReady);
    gfx.fillScreen(gfx.color565(26, 26, 26));

    // Logo Coupe du Monde (centre, en haut)
    uint16_t* logo = getLogo();
    if (logo) {
        gfx.pushImage(CX - LOGO_W / 2, LOGO_Y, LOGO_W, LOGO_H, logo);
    }

    // Titre
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
    gfx.drawString("FIFA WORLD CUP 2026", CX, TITLE_Y, &fonts::FreeSansBold18pt7b);

    // Sous-titre
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString("Live Score Display", CX, SUB_Y, &fonts::FreeSans9pt7b);

    drawSpinner(_spinnerAngle);
    drawStatusLines();
}

void ScreenSplash::update() {
    uint32_t now = millis();
    if (now - _lastSpinMs > 80) {
        _lastSpinMs = now;
        _spinnerAngle = (_spinnerAngle + 30) % 360;
        drawSpinner(_spinnerAngle);
        drawStatusLines();
    }
}

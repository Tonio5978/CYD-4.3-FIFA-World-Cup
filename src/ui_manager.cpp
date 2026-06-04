#include "ui_manager.h"
#include "storage.h"
#include "ntp_time.h"

// ============================================================
// Flag cache (LRU, PSRAM-backed pixel buffers)
// ============================================================

struct FlagEntry {
    String   code;
    uint16_t* buf  = nullptr;
    int      w     = 0;
    int      h     = 0;
    uint32_t lastUsed = 0;
};

static const int CACHE_SIZE = 12;
static FlagEntry flagCache[CACHE_SIZE];

// large=true  → loads from flags/large/<CODE>.png  (200×150)
// large=false → loads from flags/<CODE>.png         (64×48)
static uint16_t* getCachedFlag(const String& code, int w, int h, bool large = false) {
    // Hit?
    for (auto& e : flagCache) {
        if (e.code == code && e.w == w && e.h == h) {
            e.lastUsed = millis();
            DBG("[CACHE] Hit flag %s (%dx%d)\n", code.c_str(), w, h);
            return e.buf;
        }
    }
    // Find LRU slot
    int lru = 0;
    uint32_t oldest = flagCache[0].lastUsed;
    for (int i = 1; i < CACHE_SIZE; i++) {
        if (flagCache[i].lastUsed < oldest) { oldest = flagCache[i].lastUsed; lru = i; }
    }
    // Free old buffer
    if (flagCache[lru].buf) {
        DBG("[CACHE] Evict flag %s (slot %d)\n", flagCache[lru].code.c_str(), lru);
        heap_caps_free(flagCache[lru].buf);
        flagCache[lru].buf = nullptr;
    }

    DBG("[CACHE] Load flag %s (%dx%d, large=%d)\n", code.c_str(), w, h, (int)large);
    // Allocate in PSRAM
    size_t sz = (size_t)w * h * 2;
    uint16_t* buf = (uint16_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!buf) {
        DBG("[UI] No PSRAM for flag %s\n", code.c_str());
        return nullptr;
    }

    if (!Storage::loadFlagRGB565(code, buf, w, h, large)) {
        heap_caps_free(buf);
        return nullptr;
    }

    flagCache[lru].code     = code;
    flagCache[lru].buf      = buf;
    flagCache[lru].w        = w;
    flagCache[lru].h        = h;
    flagCache[lru].lastUsed = millis();
    return buf;
}

// ============================================================
// Header / Footer
// ============================================================

void UI::drawHeader(const String& label, const String& icon) {
    gfx.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, gfx.color565(0, 63, 127));
    gfx.setTextColor(TFT_WHITE, gfx.color565(0, 63, 127));
    gfx.setTextSize(1);

    // Icon + label on the left
    String left = icon + " " + label;
    gfx.setTextDatum(lgfx::middle_left);
    gfx.drawString(left.c_str(), 12, HEADER_H / 2, &fonts::FreeSansBold9pt7b);

    // Clock in centre
    if (gCtx.ntpSynced) {
        String timeStr = NtpTime::formatTime(gCtx.localTime);
        String dateStr = NtpTime::formatDate(gCtx.localTime);
        String centre  = timeStr + " - " + dateStr;
        gfx.setTextDatum(lgfx::middle_center);
        gfx.drawString(centre.c_str(), SCREEN_WIDTH / 2, HEADER_H / 2, &fonts::FreeSans9pt7b);
    }

    // WiFi icon on the right
    gfx.setTextDatum(lgfx::middle_right);
    String wifiIcon = gCtx.wifiConnected ? "WiFi" : "No WiFi";
    gfx.drawString(wifiIcon.c_str(), SCREEN_WIDTH - 12, HEADER_H / 2, &fonts::FreeSans9pt7b);
}

void UI::drawFooter(int activeGroupIdx) {
    // Background
    gfx.fillRect(0, SCREEN_HEIGHT - FOOTER_H, SCREEN_WIDTH, FOOTER_H,
                 gfx.color565(42, 42, 42));

    // Navigation items: arrows + 4 visible groups + HOME
    // Layout: [<] [G_n-1] [G_n] [HOME] [G_n+1] [G_n+2] [>]
    // We show groups around the active one, plus a HOME button.

    const int ITEMS     = 7;
    const int ITEM_W    = SCREEN_WIDTH / ITEMS;
    const int ITEM_Y    = SCREEN_HEIGHT - FOOTER_H / 2;

    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextSize(1);

    // Left arrow
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString("<", ITEM_W / 2, ITEM_Y, &fonts::FreeSansBold9pt7b);

    // Four group slots around active (bornes sur le nb reel de groupes)
    const int groupCount = (int)gCtx.standings.size();
    int startGroup = max(0, activeGroupIdx - 1);
    for (int i = 0; i < 4; i++) {
        int grpIdx = startGroup + i;
        if (grpIdx >= groupCount) break;
        char label[8];
        snprintf(label, sizeof(label), "Gr. %c", 'A' + grpIdx);
        uint32_t bg = (grpIdx == activeGroupIdx)
                          ? gfx.color565(0, 63, 127)
                          : gfx.color565(42, 42, 42);
        int x = ITEM_W + i * ITEM_W + ITEM_W / 2;
        gfx.fillRect(ITEM_W + i * ITEM_W, SCREEN_HEIGHT - FOOTER_H, ITEM_W, FOOTER_H, bg);
        gfx.setTextColor(TFT_WHITE, bg);
        gfx.drawString(label, x, ITEM_Y, &fonts::FreeSans9pt7b);
    }

    // HOME button
    int homeX = 5 * ITEM_W + ITEM_W / 2;
    gfx.setTextColor(TFT_WHITE, gfx.color565(42, 42, 42));
    gfx.drawString("HOME", homeX, ITEM_Y, &fonts::FreeSansBold9pt7b);

    // Right arrow
    gfx.drawString(">", 6 * ITEM_W + ITEM_W / 2, ITEM_Y, &fonts::FreeSansBold9pt7b);
}

void UI::drawDivider(int y, uint32_t color) {
    gfx.drawFastHLine(0, y, SCREEN_WIDTH, color);
}

// ============================================================
// Flag drawing
// ============================================================

void UI::drawFlag(const String& teamCode, int x, int y, int w, int h, bool large) {
    uint16_t* buf = getCachedFlag(teamCode, w, h, large);
    if (buf) {
        gfx.pushImage(x, y, w, h, buf);
    } else {
        // Fallback: coloured rectangle with team code
        gfx.fillRect(x, y, w, h, teamGradientColor1(teamCode));
        gfx.setTextDatum(lgfx::middle_center);
        gfx.setTextColor(TFT_WHITE);
        if (w >= 32) {
            gfx.drawString(teamCode.c_str(), x + w / 2, y + h / 2, &fonts::FreeSans9pt7b);
        }
    }
}

// ============================================================
// Colour helpers
// ============================================================

struct TeamColors { const char* code; uint32_t c1; uint32_t c2; };
static const TeamColors TEAM_COLORS[] = {
    {"BRA", 0x006432, 0xFFDC00},
    {"ARG", 0x75AADB, 0xFFFFFF},
    {"FRA", 0x0055A4, 0xFFFFFF},
    {"GER", 0x000000, 0xFFCE00},
    {"ESP", 0xAA0000, 0xFFC400},
    {"ENG", 0xFFFFFF, 0xCE1126},
    {"ITA", 0x0038A8, 0xCE2B37},
    {"NED", 0xFF6600, 0xFFFFFF},
    {"POR", 0x006600, 0xFF0000},
    {"BEL", 0x000000, 0xFFE000},
    {"CRO", 0xFF0000, 0x003893},
    {"MEX", 0x006847, 0xFFFFFF},
    {"USA", 0x002868, 0xBF0A30},
    {"CAN", 0xFF0000, 0xFFFFFF},
    {"JPN", 0xFFFFFF, 0xBC002D},
    {"KOR", 0xFFFFFF, 0xCD202C},
    {"MAR", 0xC1272D, 0x006233},
    {"SEN", 0x007A3D, 0xFCD116},
    {nullptr, 0,       0      },
};

static void getTeamColors(const String& code, uint32_t& c1, uint32_t& c2) {
    for (int i = 0; TEAM_COLORS[i].code; i++) {
        if (code == TEAM_COLORS[i].code) {
            c1 = TEAM_COLORS[i].c1; c2 = TEAM_COLORS[i].c2; return;
        }
    }
    c1 = COLOR_PRIMARY; c2 = COLOR_SECONDARY;
}

uint32_t UI::teamGradientColor1(const String& code) {
    uint32_t c1, c2; getTeamColors(code, c1, c2); return c1;
}
uint32_t UI::teamGradientColor2(const String& code) {
    uint32_t c1, c2; getTeamColors(code, c1, c2); return c2;
}

uint16_t UI::lerpColor565(uint16_t c1, uint16_t c2, float t) {
    uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    return (uint16_t)(((int)r1 + t * (r2 - r1)) / 1) << 11 |
           (uint16_t)(((int)g1 + t * (g2 - g1)) / 1) << 5  |
           (uint16_t)(((int)b1 + t * (b2 - b1)) / 1);
}

void UI::drawGradientRect(int x, int y, int w, int h, uint16_t c1, uint16_t c2) {
    for (int i = 0; i < h; i++) {
        gfx.drawFastHLine(x, y + i, w, lerpColor565(c1, c2, (float)i / h));
    }
}

// ============================================================
// Goal popup
// ============================================================

void UI::animatePopupIn() {
    for (int i = 0; i <= 10; i++) {
        float scale = 0.5f + i * 0.05f;
        float alpha = i / 10.0f;
        // Semi-transparent overlay
        gfx.fillScreen(gfx.color565(0, 0, 0));   // crude dim – replace with backup restore if PSRAM available
        // Scaled popup (approximate: adjust rect size)
        int pw = (int)(GOAL_POPUP_W * scale);
        int ph = (int)(GOAL_POPUP_H * scale);
        int px = (SCREEN_WIDTH  - pw) / 2;
        int py = (SCREEN_HEIGHT - ph) / 2;
        uint32_t tc1, tc2;
        getTeamColors(gCtx.currentPopup.goal.teamCode, tc1, tc2);
        drawGradientRect(px, py, pw, ph,
                         gfx.color565((tc1>>16)&0xFF,(tc1>>8)&0xFF,tc1&0xFF),
                         gfx.color565((tc2>>16)&0xFF,(tc2>>8)&0xFF,tc2&0xFF));
        gfx.drawRect(px, py, pw, ph, gfx.color565(0xFF,0xD7,0x00));
        delay(50);
    }
}

void UI::animatePopupOut() {
    for (int i = 10; i >= 0; i--) {
        float alpha = i / 10.0f;
        gfx.fillScreen(gfx.color565(0, 0, 0));
        int pw = (int)(GOAL_POPUP_W * alpha);
        int ph = (int)(GOAL_POPUP_H * alpha);
        if (pw < 10 || ph < 10) break;
        int px = (SCREEN_WIDTH  - pw) / 2;
        int py = (SCREEN_HEIGHT - ph) / 2;
        uint32_t tc1, tc2;
        getTeamColors(gCtx.currentPopup.goal.teamCode, tc1, tc2);
        drawGradientRect(px, py, pw, ph,
                         gfx.color565((tc1>>16)&0xFF,(tc1>>8)&0xFF,tc1&0xFF),
                         gfx.color565((tc2>>16)&0xFF,(tc2>>8)&0xFF,tc2&0xFF));
        gfx.drawRect(px, py, pw, ph, gfx.color565(0xFF,0xD7,0x00));
        delay(30);
    }
}

void UI::drawGoalPopup() {
    const GoalPopup& p = gCtx.currentPopup;

    // Dimmed background
    gfx.fillScreen(gfx.color565(10, 10, 10));

    // Popup background gradient
    uint32_t tc1, tc2;
    getTeamColors(p.goal.teamCode, tc1, tc2);
    uint16_t col1 = gfx.color565((tc1>>16)&0xFF,(tc1>>8)&0xFF,tc1&0xFF);
    uint16_t col2 = gfx.color565((tc2>>16)&0xFF,(tc2>>8)&0xFF,tc2&0xFF);
    drawGradientRect(GOAL_POPUP_X, GOAL_POPUP_Y, GOAL_POPUP_W, GOAL_POPUP_H, col1, col2);

    // Gold border (3px)
    uint16_t gold = gfx.color565(0xFF, 0xD7, 0x00);
    for (int b = 0; b < 3; b++) {
        gfx.drawRect(GOAL_POPUP_X + b, GOAL_POPUP_Y + b,
                     GOAL_POPUP_W - 2*b, GOAL_POPUP_H - 2*b, gold);
    }

    int cx = GOAL_POPUP_X + GOAL_POPUP_W / 2;

    // Title
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gold);
    String title = p.goal.isOwnGoal ? "OWN GOAL!" : "GOOOOAAAAAL!";
    gfx.drawString(title.c_str(), cx, GOAL_POPUP_Y + 50, &fonts::FreeSansBold18pt7b);

    // Large flag (200×150 from flags/large/)
    drawFlag(p.goal.teamCode,
             GOAL_POPUP_X + (GOAL_POPUP_W - GOAL_POPUP_FLAG_W) / 2,
             GOAL_POPUP_Y + 80,
             GOAL_POPUP_FLAG_W, GOAL_POPUP_FLAG_H,
             /*large=*/true);

    // Scorer name
    gfx.setTextColor(TFT_WHITE);
    String name = p.goal.playerName;
    name.toUpperCase();
    if (p.goal.isOwnGoal) name += " (c.s.c.)";
    gfx.drawString(name.c_str(), cx, GOAL_POPUP_Y + 265, &fonts::FreeSansBold12pt7b);

    // Minute
    gfx.setTextColor(gfx.color565(0xFF, 0xEB, 0x3B));
    gfx.drawString(p.goal.minute.c_str(), cx, GOAL_POPUP_Y + 305, &fonts::FreeSansBold9pt7b);

    // Score line
    gfx.setTextColor(TFT_WHITE);
    char scoreLine[48];
    snprintf(scoreLine, sizeof(scoreLine), "%s  %d - %d  %s",
             p.homeTeam.c_str(), p.homeScore, p.awayScore, p.awayTeam.c_str());
    gfx.drawString(scoreLine, cx, GOAL_POPUP_Y + 345, &fonts::FreeSans9pt7b);

    // Small flags for the score line
    drawFlag(p.homeTeam, GOAL_POPUP_X + 90,  GOAL_POPUP_Y + 333, FLAG_SM_W, FLAG_SM_H);
    drawFlag(p.awayTeam, GOAL_POPUP_X + 476, GOAL_POPUP_Y + 333, FLAG_SM_W, FLAG_SM_H);
}

void UI::updateGoalPopup() {
#if !ENABLE_GOAL_POPUP
    return;
#endif
    uint32_t now = millis();

    if (gCtx.popupState == POPUP_NONE) {
        if (!gCtx.popupQueue.empty()) {
            gCtx.currentPopup = gCtx.popupQueue.front();
            gCtx.popupQueue.pop();
            gCtx.currentPopup.showUntil = now + GOAL_POPUP_DURATION_MS;
            gCtx.popupState = POPUP_GOAL_SHOWING;
            DBG("[POPUP] Affichage: %s (%s) %s  %s %d-%d %s\n",
                gCtx.currentPopup.goal.playerName.c_str(),
                gCtx.currentPopup.goal.teamCode.c_str(),
                gCtx.currentPopup.goal.minute.c_str(),
                gCtx.currentPopup.homeTeam.c_str(),
                gCtx.currentPopup.homeScore,
                gCtx.currentPopup.awayScore,
                gCtx.currentPopup.awayTeam.c_str());
            animatePopupIn();
        }
    } else if (gCtx.popupState == POPUP_GOAL_SHOWING) {
        if (now >= gCtx.currentPopup.showUntil) {
            DBG("[POPUP] Fermeture – queue restante: %d\n", (int)gCtx.popupQueue.size());
            animatePopupOut();
            gCtx.popupState = POPUP_NONE;
            gCtx.needsFullRedraw = true;
        } else {
            drawGoalPopup();
        }
    }
}

// ============================================================
// Touch
// ============================================================

bool UI::getTap(int& tx, int& ty) {
    // Detection de front montant : un appui = un seul evenement.
    // Sans cela, getTouch() renvoie true a chaque tick (~16 ms) tant que
    // le doigt touche, ce qui faisait defiler plusieurs groupes d'un coup.
    static bool wasTouched = false;
    lgfx::touch_point_t tp;
    bool touched = gfx.getTouch(&tp.x, &tp.y);

    if (touched && !wasTouched) {
        wasTouched = true;
        tx = tp.x; ty = tp.y;
        return true;          // nouveau contact -> on declenche
    }
    if (!touched) wasTouched = false;  // doigt releve -> rearme
    return false;
}

void UI::handleFooterTouch(int tx, int ty) {
    if (ty < SCREEN_HEIGHT - FOOTER_H) return;  // not in footer

    // Nombre reel de groupes charges (12 pour la CdM 2026 : A..L)
    const int groupCount = (int)gCtx.standings.size();
    if (groupCount == 0) return;  // pas de classement -> navigation inutile

    const int ITEMS  = 7;
    const int ITEM_W = SCREEN_WIDTH / ITEMS;
    int slot = tx / ITEM_W;

    if (slot == 0) {
        // Left arrow – prev group
        if (gCtx.activeGroupIndex > 0) {
            gCtx.activeGroupIndex--;
            gCtx.appState = STATE_GROUP;
            gCtx.needsFullRedraw = true;
            DBG("[NAV] <- Groupe %c\n", 'A' + gCtx.activeGroupIndex);
        }
    } else if (slot == 5) {
        // HOME
        DBG("[NAV] HOME\n");
        gCtx.appState = STATE_HOME_LIVE;
        gCtx.needsFullRedraw = true;
    } else if (slot == 6) {
        // Right arrow – next group
        if (gCtx.activeGroupIndex < groupCount - 1) {
            gCtx.activeGroupIndex++;
            gCtx.appState = STATE_GROUP;
            gCtx.needsFullRedraw = true;
            DBG("[NAV] -> Groupe %c\n", 'A' + gCtx.activeGroupIndex);
        }
    } else {
        // Group button (slots 1-4)
        int startGroup = max(0, gCtx.activeGroupIndex - 1);
        int grpIdx = startGroup + (slot - 1);
        if (grpIdx >= 0 && grpIdx < groupCount) {
            gCtx.activeGroupIndex = grpIdx;
            gCtx.appState = STATE_GROUP;
            gCtx.needsFullRedraw = true;
            DBG("[NAV] Groupe %c (slot %d)\n", 'A' + grpIdx, slot);
        }
    }
}

#include "screen_home.h"
#include "display_config.h"
#include "ui_manager.h"
#include "espn_api.h"
#include "ntp_time.h"
#include "../include/config.h"
#include "../include/screens.h"

// ============================================================
// Match card layout constants
// ============================================================
static const int CARD_X    = 20;
static const int CARD_W    = SCREEN_WIDTH - 40;
static const int CARD_H    = 170;
static const int CARD_PAD  = 10;
static const int CARD_GAP  = 8;

static const int FLAG_X_OFF = 8;
static const int FLAG_Y_OFF = 12;

// ============================================================
// Draw a single live match card at y offset
// ============================================================
static void drawMatchCardLive(const Match& m, int y) {
    uint16_t bg = gfx.color565(30, 30, 30);
    gfx.fillRoundRect(CARD_X, y, CARD_W, CARD_H, 6, bg);
    gfx.drawRoundRect(CARD_X, y, CARD_W, CARD_H, 6, gfx.color565(60, 60, 60));

    int cx = CARD_X + CARD_W / 2;

    // --- Home team row (top half) ---
    // Flag
    UI::drawFlag(m.homeTeam, CARD_X + FLAG_X_OFF, y + FLAG_Y_OFF, FLAG_MD_W, FLAG_MD_H);

    // Team name
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString(m.homeTeamName.c_str(),
                   CARD_X + FLAG_X_OFF + FLAG_MD_W + 10,
                   y + FLAG_Y_OFF + FLAG_MD_H / 2,
                   &fonts::FreeSansBold9pt7b);

    // Score
    char score[8];
    snprintf(score, sizeof(score), "%d : %d", m.homeScore, m.awayScore);
    gfx.setTextDatum(lgfx::middle_right);
    gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
    gfx.drawString(score, CARD_X + CARD_W - 80, y + CARD_H / 2,
                   &fonts::FreeSansBold18pt7b);

    // Home goals
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(gfx.color565(0xCC, 0xCC, 0xCC));
    int goalY = y + FLAG_Y_OFF + FLAG_MD_H + 4;
    String homeGoalStr = "";
    for (size_t i = 0; i < m.homeGoals.size() && i < 3; i++) {
        homeGoalStr += (i > 0 ? "  " : "") + String("o ") +
                       m.homeGoals[i].playerName + " " + m.homeGoals[i].minute;
        if (m.homeGoals[i].isOwnGoal) homeGoalStr += "(csc)";
    }
    if (m.homeGoals.size() > 3) homeGoalStr += " ...";
    if (!homeGoalStr.isEmpty())
        gfx.drawString(homeGoalStr.c_str(), CARD_X + FLAG_X_OFF + FLAG_MD_W + 10,
                       goalY, &fonts::FreeSans9pt7b);

    // --- Away team row (bottom half) ---
    int awayY = y + CARD_H / 2 + 4;
    UI::drawFlag(m.awayTeam, CARD_X + FLAG_X_OFF, awayY, FLAG_MD_W, FLAG_MD_H);

    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString(m.awayTeamName.c_str(),
                   CARD_X + FLAG_X_OFF + FLAG_MD_W + 10,
                   awayY + FLAG_MD_H / 2,
                   &fonts::FreeSansBold9pt7b);

    // Away goals
    gfx.setTextColor(gfx.color565(0xCC, 0xCC, 0xCC));
    int awayGoalY = awayY + FLAG_MD_H + 4;
    String awayGoalStr = "";
    for (size_t i = 0; i < m.awayGoals.size() && i < 3; i++) {
        awayGoalStr += (i > 0 ? "  " : "") + String("o ") +
                       m.awayGoals[i].playerName + " " + m.awayGoals[i].minute;
        if (m.awayGoals[i].isOwnGoal) awayGoalStr += "(csc)";
    }
    if (m.awayGoals.size() > 3) awayGoalStr += " ...";
    if (!awayGoalStr.isEmpty())
        gfx.drawString(awayGoalStr.c_str(), CARD_X + FLAG_X_OFF + FLAG_MD_W + 10,
                       awayGoalY, &fonts::FreeSans9pt7b);

    // Clock + period on the right
    gfx.setTextDatum(lgfx::middle_right);
    gfx.setTextColor(gfx.color565(0xFF, 0xEB, 0x3B));
    gfx.drawString(m.clock.c_str(), CARD_X + CARD_W - 8, awayY, &fonts::FreeSansBold9pt7b);

    const char* periodStr = "1ere MT";
    if (m.period == 2) periodStr = "2eme MT";
    else if (m.period == 3) periodStr = "Prolong.";
    gfx.setTextColor(gfx.color565(0xCC, 0xCC, 0xCC));
    gfx.drawString(periodStr, CARD_X + CARD_W - 8, awayY + 18, &fonts::FreeSans9pt7b);

    // Venue
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(gfx.color565(0x88, 0x88, 0x88));
    gfx.drawString(m.venueName.c_str(), CARD_X + FLAG_X_OFF,
                   y + CARD_H - 14, &fonts::FreeSans9pt7b);

    // Live red dot indicator
    gfx.fillCircle(CARD_X + CARD_W - 10, y + 10, 5, COLOR_LIVE_RED);
}

// ============================================================
// Draw a single upcoming match card
// ============================================================
static void drawMatchCardNext(const Match& m, int y) {
    uint16_t bg = gfx.color565(30, 30, 30);
    gfx.fillRoundRect(CARD_X, y, CARD_W, 80, 6, bg);
    gfx.drawRoundRect(CARD_X, y, CARD_W, 80, 6, gfx.color565(50, 50, 50));

    // Heure locale (UTC -> fuseau configure)
    String timeDisp = NtpTime::localTimeFromIso(m.date);

    // Time
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
    gfx.drawString(timeDisp.c_str(), CARD_X + 10, y + 25, &fonts::FreeSansBold9pt7b);

    // Home flag + name
    UI::drawFlag(m.homeTeam, CARD_X + 80, y + 10, FLAG_MD_W, FLAG_MD_H);
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString(m.homeTeamName.c_str(), CARD_X + 80 + FLAG_MD_W + 6,
                   y + 25, &fonts::FreeSans9pt7b);

    // VS
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
    gfx.drawString("vs", CARD_X + CARD_W / 2, y + 25, &fonts::FreeSansBold9pt7b);

    // Away flag + name
    int awayFlagX = CARD_X + CARD_W - FLAG_MD_W - 10;
    UI::drawFlag(m.awayTeam, awayFlagX, y + 10, FLAG_MD_W, FLAG_MD_H);
    gfx.setTextDatum(lgfx::middle_right);
    gfx.setTextColor(TFT_WHITE);
    gfx.drawString(m.awayTeamName.c_str(),
                   awayFlagX - 6, y + 25, &fonts::FreeSans9pt7b);

    // Venue + group
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(gfx.color565(0x88, 0x88, 0x88));
    String venueGroup = m.group.isEmpty() ? m.venueName
                                           : m.venueName + " - " + m.group;
    gfx.drawString(venueGroup.c_str(), CARD_X + 10, y + 62, &fonts::FreeSans9pt7b);
}

// ============================================================
// Collect live / upcoming matches
// ============================================================
static std::vector<const Match*> getLiveMatches() {
    std::vector<const Match*> v;
    for (const auto& m : gCtx.matches)
        if (m.status == "in") v.push_back(&m);
    return v;
}

static std::vector<const Match*> getUpcomingMatches() {
    std::vector<const Match*> v;
    for (const auto& m : gCtx.matches)
        if (m.status == "pre") v.push_back(&m);
    return v;
}

// ============================================================
// Public
// ============================================================

void ScreenHome::draw() {
    gfx.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, gfx.color565(26, 26, 26));

    auto live = getLiveMatches();
    bool hasLive = !live.empty();

    DBG("[HOME] Draw: %d live, %d total\n",
        (int)live.size(), (int)gCtx.matches.size());

    bool preLive = !hasLive && (gCtx.appState == STATE_HOME_PRELIVE);
    const char* hdr = hasLive ? "LIVE"
                    : preLive ? "PRE-LIVE - EN ATTENTE"
                              : "PROCHAINS MATCHS";
    UI::drawHeader(hdr, hasLive ? ">" : "o");
    UI::drawFooter(gCtx.activeGroupIndex);

    int y = CONTENT_Y + CARD_PAD;
    int maxY = SCREEN_HEIGHT - FOOTER_H - CARD_PAD;
    int drawn = 0;

    if (hasLive) {
        for (const Match* m : live) {
            if (y + CARD_H > maxY) break;
            drawMatchCardLive(*m, y);
            y += CARD_H + CARD_GAP;
            drawn++;
        }
        DBG("[HOME] %d carte(s) live affichee(s)\n", drawn);

        // Prochaine rencontre sous le(s) match(s) en cours, si la place le permet
        auto upcoming = getUpcomingMatches();
        if (!upcoming.empty() && y + 24 + 80 <= maxY) {
            const Match* nm = upcoming.front();
            String label = "PROCHAIN MATCH";
            String d = NtpTime::localDateFromIso(nm->date);
            if (!d.isEmpty()) label += "  -  " + d;
            gfx.setTextDatum(lgfx::middle_left);
            gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
            gfx.drawString(label.c_str(), CARD_X, y + 12, &fonts::FreeSansBold9pt7b);
            y += 24;
            drawMatchCardNext(*nm, y);
        }
    } else {
        auto upcoming = getUpcomingMatches();
        DBG("[HOME] %d matchs a venir\n", (int)upcoming.size());

        // Sections par jour : un en-tete de date dore au-dessus de chaque
        // groupe de matchs du meme jour (la date du 1er jour est donc
        // toujours affichee). Heures/dates en fuseau local.
        const int DATE_H = 24;
        String lastDate = "";
        for (const Match* m : upcoming) {
            String d = NtpTime::localDateFromIso(m->date);
            bool newDay = (d != lastDate);
            int needed = (newDay ? DATE_H : 0) + 80;
            if (y + needed > maxY) break;

            if (newDay) {
                gfx.setTextDatum(lgfx::middle_center);
                gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
                gfx.drawString(d.c_str(), SCREEN_WIDTH / 2, y + 12,
                               &fonts::FreeSansBold9pt7b);
                y += DATE_H;
                lastDate = d;
            }

            drawMatchCardNext(*m, y);
            y += 80 + CARD_GAP;
            drawn++;
        }
        if (upcoming.empty()) {
            gfx.setTextDatum(lgfx::middle_center);
            gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
            gfx.drawString("Aucun match programme",
                           SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                           &fonts::FreeSans9pt7b);
        }
    }
}

static bool _liveDotVisible = true;
static uint32_t _lastDotToggleMs = 0;

void ScreenHome::update() {
    // Blink live dot every 500 ms – redraw header only
    if (millis() - _lastDotToggleMs > 500) {
        _lastDotToggleMs = millis();
        _liveDotVisible = !_liveDotVisible;
        // Re-draw header to update clock
        bool hasLive = EspnApi::hasLiveMatch();
        const char* hdr = hasLive ? "LIVE"
                        : (gCtx.appState == STATE_HOME_PRELIVE) ? "PRE-LIVE - EN ATTENTE"
                                                                : "PROCHAINS MATCHS";
        UI::drawHeader(hdr, hasLive ? ">" : "o");
    }
}

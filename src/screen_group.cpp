#include "screen_group.h"
#include "display_config.h"
#include "ui_manager.h"
#include "espn_api.h"
#include "ntp_time.h"
#include "../include/config.h"
#include "../include/screens.h"

// Split layout: 400px standings | 360px matches
static const int TABLE_X  = 0;
static const int TABLE_W  = 400;
static const int MATCH_X  = TABLE_W;
static const int MATCH_W  = SCREEN_WIDTH - TABLE_W;
static const int DIVIDER_X = TABLE_W;

// ============================================================
// Standings table (left side)
// ============================================================
static void drawStandingsTable(const GroupStanding& gs) {
    int y = CONTENT_Y + 4;
    int rowH = 56;                               // +10 px en vertical
    const int statsX = TABLE_X + TABLE_W - 14;   // resultats decales vers la gauche

    // Column header
    gfx.fillRect(TABLE_X, y, TABLE_W, 22, gfx.color565(20, 20, 20));
    gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
    gfx.setTextSize(1);

    // Header labels
    gfx.setTextDatum(lgfx::middle_left);
    gfx.drawString("#  EQUIPE", TABLE_X + 4, y + 11, &fonts::FreeSans9pt7b);
    gfx.setTextDatum(lgfx::middle_right);
    gfx.drawString("Pts J  V  N  D", statsX, y + 11, &fonts::FreeSans9pt7b);

    y += 24;

    for (size_t i = 0; i < gs.teams.size(); i++) {
        const TeamStanding& ts = gs.teams[i];

        // Row background – green for top 2
        uint16_t rowBg = (i < 2)
            ? gfx.color565(0, 40, 20)
            : gfx.color565(26, 26, 26);
        gfx.fillRect(TABLE_X, y, TABLE_W, rowH, rowBg);
        if (i < 2)
            gfx.drawRect(TABLE_X, y, TABLE_W, rowH, gfx.color565(0, 80, 40));

        // Rank
        gfx.setTextDatum(lgfx::middle_left);
        gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
        char rankStr[4]; snprintf(rankStr, sizeof(rankStr), "%d", ts.rank);
        gfx.drawString(rankStr, TABLE_X + 4, y + rowH / 2, &fonts::FreeSans9pt7b);

        // Flag
        UI::drawFlag(ts.teamCode, TABLE_X + 22, y + (rowH - FLAG_SM_H) / 2, FLAG_SM_W, FLAG_SM_H);

        // Name (abbreviated)
        gfx.setTextColor(TFT_WHITE);
        String shortName = ts.teamCode;
        gfx.drawString(shortName.c_str(), TABLE_X + 60, y + rowH / 2,
                       &fonts::FreeSansBold9pt7b);

        // Stats: Pts J V N D
        gfx.setTextDatum(lgfx::middle_right);
        gfx.setTextColor(gfx.color565(0xDD, 0xDD, 0xDD));
        char stats[24];
        snprintf(stats, sizeof(stats), "%2d %d  %d  %d  %d",
                 ts.points, ts.played, ts.wins, ts.draws, ts.losses);
        gfx.drawString(stats, statsX - 5, y + rowH / 2, &fonts::FreeSans9pt7b);

        // Goals line below stats
        gfx.setTextColor(gfx.color565(0x88, 0x88, 0x88));
        char goals[16];
        snprintf(goals, sizeof(goals), "BP:%d BC:%d  %+d",
                 ts.goalsFor, ts.goalsAgainst, ts.goalDiff);
        gfx.setTextDatum(lgfx::middle_right);
        gfx.drawString(goals, statsX, y + rowH - 9, &fonts::FreeSans9pt7b);

        // Divider
        gfx.drawFastHLine(TABLE_X, y + rowH, TABLE_W, gfx.color565(40, 40, 40));
        y += rowH;
    }

    // Qualified legend
    y += 4;
    gfx.fillCircle(TABLE_X + 10, y + 8, 5, gfx.color565(0, 120, 60));
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
    gfx.drawString("Qualifie (1er et 2eme)", TABLE_X + 20, y + 8, &fonts::FreeSans9pt7b);
}

// ============================================================
// Group matches list (right side)
// ============================================================
static void drawGroupMatches(const GroupStanding& gs) {
    int y = CONTENT_Y + 4;
    int rowH = 56;                              // 6 matchs (groupe de 4) tiennent
    int maxY = SCREEN_HEIGHT - FOOTER_H;
    int cx   = MATCH_X + MATCH_W / 2;

    // Le scoreboard ESPN ne porte aucun champ de groupe : on identifie les
    // matchs du groupe par appartenance des DEUX equipes au classement du
    // groupe (round-robin de 4 equipes = 6 matchs ; exclut les phases finales).
    auto inGroup = [&](const String& code) {
        for (const auto& t : gs.teams)
            if (t.teamCode == code) return true;
        return false;
    };
    std::vector<const Match*> groupMatches;
    for (const auto& m : gCtx.matches) {
        if (inGroup(m.homeTeam) && inGroup(m.awayTeam))
            groupMatches.push_back(&m);
    }

    if (groupMatches.empty()) {
        gfx.setTextDatum(lgfx::middle_center);
        gfx.setTextColor(gfx.color565(0x88, 0x88, 0x88));
        gfx.drawString("Pas de matchs",
                       cx, SCREEN_HEIGHT / 2, &fonts::FreeSans9pt7b);
        return;
    }

    for (const Match* m : groupMatches) {
        if (y + rowH > maxY) break;

        bool live = (m->status == "in");
        bool done = (m->status == "post");

        // --- Ligne 1 : date - heure (locale, sans l'annee) ---
        String dateStr = NtpTime::localDateFromIso(m->date);  // "11 Juin 2026"
        int sp = dateStr.lastIndexOf(' ');
        if (sp > 0) dateStr = dateStr.substring(0, sp);        // "11 Juin"
        String line1 = dateStr + " - " + NtpTime::localTimeFromIso(m->date);

        gfx.setTextDatum(lgfx::middle_left);
        gfx.setTextColor(gfx.color565(0xFF, 0xD7, 0x00));
        gfx.drawString(line1.c_str(), MATCH_X + 6, y + 11, &fonts::FreeSans9pt7b);

        if (live) {  // chrono + pastille rouge a droite
            gfx.setTextDatum(lgfx::middle_right);
            gfx.setTextColor(gfx.color565(0xFF, 0xEB, 0x3B));
            gfx.drawString(m->clock.c_str(), MATCH_X + MATCH_W - 8, y + 11,
                           &fonts::FreeSans9pt7b);
            gfx.fillCircle(MATCH_X + MATCH_W - 56, y + 11, 4, COLOR_LIVE_RED);
        }

        // --- Ligne 2 : drapeaux + codes + score ---
        int midY = y + 31;
        UI::drawFlag(m->homeTeam, MATCH_X + 8, midY - FLAG_SM_H / 2,
                     FLAG_SM_W, FLAG_SM_H);
        gfx.setTextDatum(lgfx::middle_left);
        gfx.setTextColor(TFT_WHITE);
        gfx.drawString(m->homeTeam.c_str(), MATCH_X + 8 + FLAG_SM_W + 6, midY,
                       &fonts::FreeSansBold9pt7b);

        UI::drawFlag(m->awayTeam, MATCH_X + MATCH_W - 8 - FLAG_SM_W,
                     midY - FLAG_SM_H / 2, FLAG_SM_W, FLAG_SM_H);
        gfx.setTextDatum(lgfx::middle_right);
        gfx.setTextColor(TFT_WHITE);
        gfx.drawString(m->awayTeam.c_str(), MATCH_X + MATCH_W - 8 - FLAG_SM_W - 6,
                       midY, &fonts::FreeSansBold9pt7b);

        char score[12];
        gfx.setTextDatum(lgfx::middle_center);
        if (done || live) {
            snprintf(score, sizeof(score), "%d : %d", m->homeScore, m->awayScore);
            gfx.setTextColor(live ? gfx.color565(0xFF, 0xD7, 0x00) : TFT_WHITE);
        } else {
            snprintf(score, sizeof(score), "- : -");
            gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
        }
        gfx.drawString(score, cx, midY, &fonts::FreeSansBold9pt7b);

        // --- Ligne 3 : stade ---
        gfx.setTextDatum(lgfx::middle_left);
        gfx.setTextColor(gfx.color565(0x88, 0x88, 0x88));
        gfx.drawString(m->venueName.c_str(), MATCH_X + 8, y + 49,
                       &fonts::FreeSans9pt7b);

        // Separateur
        gfx.drawFastHLine(MATCH_X, y + rowH, MATCH_W, gfx.color565(40, 40, 40));
        y += rowH;
    }
}

// ============================================================
// Public
// ============================================================

void ScreenGroup::draw() {
    gfx.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, gfx.color565(26, 26, 26));

    int idx = gCtx.activeGroupIndex;
    char groupLabel[12];
    snprintf(groupLabel, sizeof(groupLabel), "GROUPE %c", 'A' + idx);

    if (idx < (int)gCtx.standings.size()) {
        DBG("[GROUP] Draw Groupe %c – %d equipes\n",
            'A' + idx, (int)gCtx.standings[idx].teams.size());
    } else {
        DBG("[GROUP] Draw Groupe %c – classement absent (%d groupes charges)\n",
            'A' + idx, (int)gCtx.standings.size());
    }

    UI::drawHeader(groupLabel, "=");
    UI::drawFooter(idx);

    // Vertical divider
    gfx.drawFastVLine(DIVIDER_X, CONTENT_Y, CONTENT_H, gfx.color565(60, 60, 60));

    if (idx < (int)gCtx.standings.size()) {
        drawStandingsTable(gCtx.standings[idx]);
        drawGroupMatches(gCtx.standings[idx]);
    } else {
        gfx.setTextDatum(lgfx::middle_center);
        gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
        gfx.drawString("Classement non disponible",
                       SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                       &fonts::FreeSans9pt7b);
    }
}

void ScreenGroup::update() {
    UI::drawHeader(
        String("GROUPE ") + (char)('A' + gCtx.activeGroupIndex),
        "=");
}

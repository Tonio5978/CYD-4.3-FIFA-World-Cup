#include "screen_bracket.h"
#include "display_config.h"
#include "ui_manager.h"
#include "espn_api.h"
#include "ntp_time.h"
#include "../include/config.h"
#include "../include/screens.h"
#include <algorithm>
#include <vector>

// ============================================================
// Helpers bracket
// ============================================================

// Matchs d'un tour, tries par id (= ordre du bracket).
static std::vector<const Match*> roundMatches(const char* slug) {
    std::vector<const Match*> v;
    for (const auto& m : gCtx.matches)
        if (m.round == slug) v.push_back(&m);
    std::sort(v.begin(), v.end(), [](const Match* a, const Match* b) {
        return atol(a->matchId.c_str()) < atol(b->matchId.c_str());
    });
    return v;
}

// Match de 32e ayant produit l'equipe `code` (gagnant), ou nullptr.
static const Match* r32Of(const std::vector<const Match*>& r32, const String& code) {
    if (code.isEmpty() || code == "RD32") return nullptr;
    for (auto m : r32)
        if (m->homeTeam == code || m->awayTeam == code) return m;
    return nullptr;
}

// Gagnant d'un match : 0=indetermine, 1=domicile, 2=exterieur.
static int winnerOf(const Match* m) {
    if (!m || m->status != "post") return 0;
    if (m->homeScore > m->awayScore) return 1;
    if (m->awayScore > m->homeScore) return 2;
    // Egalite -> tirs au but : la note cite le nom de l'equipe qualifiee.
    if (m->note.indexOf(m->homeTeamName) >= 0) return 1;
    if (m->note.indexOf(m->awayTeamName) >= 0) return 2;
    return 0;
}

// ============================================================
// Boite de match (2 lignes : domicile / exterieur)
// ============================================================
static const int BOX_H = 54;

static void drawTeamRow(int x, int y, int w, const String& code, int score,
                        const String& status, bool winner, bool showScore) {
    UI::drawFlag(code.isEmpty() ? "?" : code, x + 3, y - 10, 28, 21);
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setTextColor(winner ? gfx.color565(0xFF, 0xD7, 0x00) : TFT_WHITE);
    gfx.drawString(code.isEmpty() ? "?" : code.c_str(),
                   x + 3 + 28 + 5, y,
                   winner ? &fonts::FreeSansBold9pt7b : &fonts::FreeSans9pt7b);

    gfx.setTextDatum(lgfx::middle_right);
    if (showScore) {
        char s[6]; snprintf(s, sizeof(s), "%d", score);
        gfx.drawString(s, x + w - 6, y, &fonts::FreeSansBold9pt7b);
    } else {
        gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
        gfx.drawString("-", x + w - 6, y, &fonts::FreeSans9pt7b);
    }
}

static void drawMatchBox(int x, int y, int w, const Match* m) {
    gfx.fillRoundRect(x, y, w, BOX_H, 4, gfx.color565(30, 30, 30));
    bool live = (m && m->status == "in");
    gfx.drawRoundRect(x, y, w, BOX_H, 4,
                      live ? COLOR_LIVE_RED : gfx.color565(60, 60, 60));

    if (!m) {
        gfx.setTextDatum(lgfx::middle_center);
        gfx.setTextColor(gfx.color565(0x77, 0x77, 0x77));
        gfx.drawString("a venir", x + w / 2, y + BOX_H / 2, &fonts::FreeSans9pt7b);
        return;
    }

    bool showScore = (m->status == "post" || m->status == "in");
    int win = winnerOf(m);
    drawTeamRow(x, y + 15, w, m->homeTeam, m->homeScore, m->status, win == 1, showScore);
    drawTeamRow(x, y + 39, w, m->awayTeam, m->awayScore, m->status, win == 2, showScore);
    gfx.drawFastHLine(x + 4, y + BOX_H / 2, w - 8, gfx.color565(55, 55, 55));

    if (live) gfx.fillCircle(x + w - 6, y + 8, 4, COLOR_LIVE_RED);
}

// Petite ligne d'info sous une boite (heure locale a venir, ou tirs au but).
static void drawBoxInfo(int x, int y, int w, const Match* m) {
    if (!m) return;
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0x88, 0x88, 0x88));
    String txt;
    if (m->status == "pre") {
        txt = NtpTime::localDateFromIso(m->date);
        int sp = txt.lastIndexOf(' '); if (sp > 0) txt = txt.substring(0, sp);
        txt += " " + NtpTime::localTimeFromIso(m->date);
    } else if (m->note.indexOf("penalties") >= 0 || m->note.indexOf("tab") >= 0) {
        txt = "tab";
    }
    if (!txt.isEmpty())
        gfx.drawString(txt.c_str(), x + w / 2, y, &fonts::FreeSans9pt7b);
}

// ============================================================
// Vue 0 : arbre final (1/4 -> 1/2 -> finale + 3e place)
// ============================================================
static void drawFinalTree() {
    auto qf = roundMatches("quarterfinals");   // qf[0..3]
    auto sf = roundMatches("semifinals");       // sf[0..1]
    auto fin = roundMatches("final");
    auto p3  = roundMatches("3rd-place-match");

    auto at = [](const std::vector<const Match*>& v, size_t i) -> const Match* {
        return i < v.size() ? v[i] : nullptr;
    };

    const int W = 150;
    const int xL_qf = 8,  xL_sf = 168, xC = 328, xR_sf = 488, xR_qf = 648;
    const int yTop = 78, yBot = 290, yMid = 184;

    // Labels colonnes
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
    gfx.drawString("1/4", xL_qf + W / 2, 68, &fonts::FreeSans9pt7b);
    gfx.drawString("1/2", xL_sf + W / 2, 68, &fonts::FreeSans9pt7b);
    gfx.drawString("FINALE", xC + W / 2, 68, &fonts::FreeSans9pt7b);
    gfx.drawString("1/2", xR_sf + W / 2, 68, &fonts::FreeSans9pt7b);
    gfx.drawString("1/4", xR_qf + W / 2, 68, &fonts::FreeSans9pt7b);

    // Connecteurs (simples)
    uint16_t lc = gfx.color565(70, 70, 70);
    gfx.drawFastHLine(xL_qf + W, yTop + BOX_H / 2, 168 - W + 8, lc);
    gfx.drawFastHLine(xL_qf + W, yBot + BOX_H / 2, 168 - W + 8, lc);
    gfx.drawFastVLine(xL_sf - 4, yTop + BOX_H / 2, yBot - yTop, lc);
    gfx.drawFastHLine(xL_sf + W, yMid + BOX_H / 2, xC - (xL_sf + W), lc);
    gfx.drawFastHLine(xC + W, yMid + BOX_H / 2, xR_sf - (xC + W), lc);
    gfx.drawFastVLine(xR_sf + W + 4, yTop + BOX_H / 2, yBot - yTop, lc);
    gfx.drawFastHLine(xR_qf, yTop + BOX_H / 2, xR_sf + W + 4 - xR_qf, lc);
    gfx.drawFastHLine(xR_qf, yBot + BOX_H / 2, xR_sf + W + 4 - xR_qf, lc);

    // 1/4 (qf0,qf1 a gauche ; qf2,qf3 a droite)
    drawMatchBox(xL_qf, yTop, W, at(qf, 0));
    drawMatchBox(xL_qf, yBot, W, at(qf, 1));
    drawMatchBox(xR_qf, yTop, W, at(qf, 2));
    drawMatchBox(xR_qf, yBot, W, at(qf, 3));
    // 1/2
    drawMatchBox(xL_sf, yMid, W, at(sf, 0));
    drawMatchBox(xR_sf, yMid, W, at(sf, 1));
    // Finale (au centre, mise en valeur)
    gfx.drawRoundRect(xC - 2, yMid - 2, W + 4, BOX_H + 4, 5, gfx.color565(0xFF, 0xD7, 0x00));
    drawMatchBox(xC, yMid, W, at(fin, 0));

    // Petite finale (3e place) en bas au centre
    const Match* third = at(p3, 0);
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xCD, 0x7F, 0x32));
    gfx.drawString("3e place", xC + W / 2, yBot - 8, &fonts::FreeSans9pt7b);
    drawMatchBox(xC, yBot, W, third);
}

// ============================================================
// Vues 1-4 : region (8 equipes : 32es -> 8es -> 1/4)
// ============================================================
// R16 alimentant chaque 1/4 : qf0<-W1,W2 ; qf1<-W5,W6 ; qf2<-W3,W4 ; qf3<-W7,W8
static const int kR16Slot[4][2] = {{0, 1}, {4, 5}, {2, 3}, {6, 7}};

// Repartit les 16 matchs de 32es dans les 4 regions.
//  1) feeders des 8es CONNUS -> region exacte (par equipe qualifiee).
//  2) 32es restants (dont le 8e n'est pas encore joue) -> regions incompletes
//     dans l'ordre des id, pour ne masquer AUCUN match programme.
static void buildRegionsR32(std::vector<const Match*> out[4]) {
    auto r32 = roundMatches("round-of-32");
    auto r16 = roundMatches("round-of-16");
    std::vector<bool> taken(r32.size(), false);

    for (int reg = 0; reg < 4; reg++) {
        for (int s = 0; s < 2; s++) {
            int slot = kR16Slot[reg][s];
            const Match* r = (slot < (int)r16.size()) ? r16[slot] : nullptr;
            if (!r) continue;
            String codes[2] = { r->homeTeam, r->awayTeam };
            for (const String& code : codes) {
                if (code.isEmpty() || code == "RD32") continue;
                for (size_t i = 0; i < r32.size(); i++) {
                    if (taken[i]) continue;
                    if (r32[i]->homeTeam == code || r32[i]->awayTeam == code) {
                        out[reg].push_back(r32[i]); taken[i] = true; break;
                    }
                }
            }
        }
    }
    // Distribue les 32es restants aux regions ayant moins de 4 matchs
    for (size_t i = 0; i < r32.size(); i++) {
        if (taken[i]) continue;
        for (int reg = 0; reg < 4; reg++) {
            if (out[reg].size() < 4) { out[reg].push_back(r32[i]); taken[i] = true; break; }
        }
    }
}

static void drawRegion(int region /*0..3*/) {
    auto r16 = roundMatches("round-of-16");   // 8
    auto qf  = roundMatches("quarterfinals"); // 4

    auto at = [](const std::vector<const Match*>& v, int i) -> const Match* {
        return (i >= 0 && i < (int)v.size()) ? v[i] : nullptr;
    };

    const Match* r16A = at(r16, kR16Slot[region][0]);
    const Match* r16B = at(r16, kR16Slot[region][1]);
    const Match* qfm  = at(qf, region);

    // 32es de la region (feeders exacts + complement pour tout afficher).
    std::vector<const Match*> regs[4];
    buildRegionsR32(regs);
    const std::vector<const Match*>& r = regs[region];
    const Match* fA1 = at(r, 0);
    const Match* fA2 = at(r, 1);
    const Match* fB1 = at(r, 2);
    const Match* fB2 = at(r, 3);

    const int W = 250;
    const int xL = 8, xM = 270, xR = 532;
    const int yA1 = 78, yA2 = 158, yB1 = 258, yB2 = 338;   // 32es
    const int yRA = 118, yRB = 298;                        // 8es
    const int yQF = 208;                                   // 1/4

    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(gfx.color565(0xAA, 0xAA, 0xAA));
    gfx.drawString("16es", xL + W / 2, 68, &fonts::FreeSans9pt7b);
    gfx.drawString("8es",  xM + W / 2, 68, &fonts::FreeSans9pt7b);
    gfx.drawString("1/4",  xR + W / 2, 68, &fonts::FreeSans9pt7b);

    uint16_t lc = gfx.color565(70, 70, 70);
    gfx.drawFastHLine(xL + W, yA1 + BOX_H / 2, xM - (xL + W), lc);
    gfx.drawFastHLine(xL + W, yA2 + BOX_H / 2, xM - (xL + W), lc);
    gfx.drawFastHLine(xL + W, yB1 + BOX_H / 2, xM - (xL + W), lc);
    gfx.drawFastHLine(xL + W, yB2 + BOX_H / 2, xM - (xL + W), lc);
    gfx.drawFastHLine(xM + W, yRA + BOX_H / 2, xR - (xM + W), lc);
    gfx.drawFastHLine(xM + W, yRB + BOX_H / 2, xR - (xM + W), lc);

    drawMatchBox(xL, yA1, W, fA1);  drawBoxInfo(xL, yA1 + BOX_H + 8, W, fA1);
    drawMatchBox(xL, yA2, W, fA2);  drawBoxInfo(xL, yA2 + BOX_H + 8, W, fA2);
    drawMatchBox(xL, yB1, W, fB1);  drawBoxInfo(xL, yB1 + BOX_H + 8, W, fB1);
    drawMatchBox(xL, yB2, W, fB2);  drawBoxInfo(xL, yB2 + BOX_H + 8, W, fB2);

    drawMatchBox(xM, yRA, W, r16A); drawBoxInfo(xM, yRA + BOX_H + 8, W, r16A);
    drawMatchBox(xM, yRB, W, r16B); drawBoxInfo(xM, yRB + BOX_H + 8, W, r16B);

    drawMatchBox(xR, yQF, W, qfm);  drawBoxInfo(xR, yQF + BOX_H + 8, W, qfm);
}

// ============================================================
// Footer bracket (prev / HOME / next)
// ============================================================
static void drawBracketFooter(int view) {
    int fy = SCREEN_HEIGHT - FOOTER_H;
    gfx.fillRect(0, fy, SCREEN_WIDTH, FOOTER_H, gfx.color565(42, 42, 42));
    gfx.setTextDatum(lgfx::middle_center);
    gfx.setTextColor(TFT_WHITE, gfx.color565(42, 42, 42));
    int third = SCREEN_WIDTH / 3;
    gfx.drawString("< PRECEDENT", third / 2, fy + FOOTER_H / 2, &fonts::FreeSansBold9pt7b);
    gfx.drawString("HOME", third + third / 2, fy + FOOTER_H / 2, &fonts::FreeSansBold9pt7b);
    gfx.drawString("SUIVANT >", 2 * third + third / 2, fy + FOOTER_H / 2, &fonts::FreeSansBold9pt7b);
}

// ============================================================
// Public
// ============================================================
int ScreenBracket::viewCount() { return 5; }   // arbre + 4 regions

void ScreenBracket::draw() {
    gfx.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, gfx.color565(26, 26, 26));

    int v = gCtx.bracketView;
    char hdr[28];
    if (v == 0) snprintf(hdr, sizeof(hdr), "PHASE FINALE");
    else        snprintf(hdr, sizeof(hdr), "REGION %d - vers 1/4", v);
    UI::drawHeader(hdr, "T");

    if (v == 0) drawFinalTree();
    else        drawRegion(v - 1);

    drawBracketFooter(v);
}

void ScreenBracket::update() {
    // Rafraichit l'entete (heure) au max 1x/s. Sans throttle, drawHeader
    // etait rappele a chaque tick (~16 ms) -> le bandeau scintillait.
    static uint32_t last = 0;
    if (millis() - last < 1000) return;
    last = millis();

    int v = gCtx.bracketView;
    char hdr[28];
    if (v == 0) snprintf(hdr, sizeof(hdr), "PHASE FINALE");
    else        snprintf(hdr, sizeof(hdr), "REGION %d - vers 1/4", v);
    UI::drawHeader(hdr, "T");
}

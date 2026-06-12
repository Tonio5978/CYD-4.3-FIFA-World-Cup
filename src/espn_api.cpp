#include "espn_api.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <algorithm>
#include "../include/config.h"

// ============================================================
// Allocateur PSRAM pour ArduinoJson
// JsonDocument utilise malloc() par defaut → echoue si la heap
// interne est trop fragmentee. On force PSRAM (7.5 MB libres).
// ============================================================

struct PsramAllocator : ArduinoJson::Allocator {
    void* allocate(size_t n) override {
        return heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    }
    void deallocate(void* p) override {
        heap_caps_free(p);
    }
    void* reallocate(void* p, size_t n) override {
        return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM);
    }
};
static PsramAllocator psramAlloc;

// ============================================================
// Translitteration UTF-8 -> ASCII
// Les polices Adafruit GFX (FreeSans...) n'ont pas les glyphes
// accentues. Les noms ESPN arrivent en UTF-8 (Julián, Quiñones,
// Türkiye, Curaçao...). On remplace les accents par leur base
// ASCII pour un rendu propre, coherent avec l'UI sans accents.
// ============================================================

static const char* mapCp(uint32_t cp) {
    switch (cp) {
        // Latin-1 Supplement
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return "A";
        case 0xC6: return "AE";
        case 0xC7: return "C";
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return "E";
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return "I";
        case 0xD0: return "D";
        case 0xD1: return "N";
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return "O";
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return "U";
        case 0xDD: return "Y";
        case 0xDF: return "ss";
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return "a";
        case 0xE6: return "ae";
        case 0xE7: return "c";
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "e";
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
        case 0xF0: return "d";
        case 0xF1: return "n";
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return "o";
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return "u";
        case 0xFD: case 0xFF: return "y";
        // Latin Extended-A (croate, tcheque, polonais, turc...)
        case 0x100: case 0x102: case 0x104: return "A";
        case 0x101: case 0x103: case 0x105: return "a";
        case 0x106: case 0x108: case 0x10A: case 0x10C: return "C";
        case 0x107: case 0x109: case 0x10B: case 0x10D: return "c";
        case 0x10E: case 0x110: return "D";
        case 0x10F: case 0x111: return "d";
        case 0x112: case 0x114: case 0x116: case 0x118: case 0x11A: return "E";
        case 0x113: case 0x115: case 0x117: case 0x119: case 0x11B: return "e";
        case 0x11C: case 0x11E: case 0x120: case 0x122: return "G";
        case 0x11D: case 0x11F: case 0x121: case 0x123: return "g";
        case 0x130: return "I";
        case 0x131: return "i";
        case 0x141: return "L";
        case 0x142: return "l";
        case 0x143: case 0x145: case 0x147: return "N";
        case 0x144: case 0x146: case 0x148: return "n";
        case 0x14C: case 0x14E: case 0x150: return "O";
        case 0x14D: case 0x14F: case 0x151: return "o";
        case 0x154: case 0x156: case 0x158: return "R";
        case 0x155: case 0x157: case 0x159: return "r";
        case 0x15A: case 0x15C: case 0x15E: case 0x160: return "S";
        case 0x15B: case 0x15D: case 0x15F: case 0x161: return "s";
        case 0x162: case 0x164: case 0x166: return "T";
        case 0x163: case 0x165: case 0x167: return "t";
        case 0x168: case 0x16A: case 0x16C: case 0x16E: case 0x170: case 0x172: return "U";
        case 0x169: case 0x16B: case 0x16D: case 0x16F: case 0x171: case 0x173: return "u";
        case 0x179: case 0x17B: case 0x17D: return "Z";
        case 0x17A: case 0x17C: case 0x17E: return "z";
        default: return "?";
    }
}

static String deaccent(const String& in) {
    String out;
    out.reserve(in.length());
    const uint8_t* s = (const uint8_t*)in.c_str();
    int n = (int)in.length();
    for (int i = 0; i < n; ) {
        uint8_t c = s[i];
        if (c < 0x80) { out += (char)c; i++; continue; }
        uint32_t cp; int len;
        if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            cp = ((c & 0x1F) << 6) | (s[i+1] & 0x3F); len = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            cp = ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F); len = 3;
        } else { i++; continue; }   // octet invalide -> ignore
        out += mapCp(cp);
        i += len;
    }
    return out;
}

// ============================================================
// Flux bloquant : enveloppe le WiFiClient pour qu'un read()/peek()
// attende l'arrivee des donnees (avec timeout) au lieu de renvoyer
// -1 des qu'il n'y a rien en tampon. Sinon, lors d'une pause reseau,
// ArduinoJson prend ce -1 pour une fin de flux -> "IncompleteInput".
// ============================================================

class BlockingStream : public Stream {
    Stream&  _s;
    uint32_t _timeout;
    bool waitData() {
        uint32_t start = millis();
        while (!_s.available()) {
            if (millis() - start > _timeout) return false;
            delay(1);
        }
        return true;
    }
public:
    BlockingStream(Stream& s, uint32_t timeout) : _s(s), _timeout(timeout) {}
    int  available() override { return _s.available(); }
    int  read()      override { return waitData() ? _s.read() : -1; }
    int  peek()      override { return waitData() ? _s.peek() : -1; }
    size_t write(uint8_t) override { return 0; }
};

// ============================================================
// HTTP GET + parse JSON en streaming
//
// useHTTP10(true) : reponse NON chunkee avec Content-Length.
// Sinon getString()/le flux peut contenir les marqueurs de taille
// de chunk hexa, qu'ArduinoJson lit comme une valeur numerique puis
// s'arrete (events vide). On streame directement (pas de String de
// 760 KB) via BlockingStream pour eviter l'IncompleteInput.
// ============================================================

static bool fetchJson(const char* url, JsonDocument& doc, JsonDocument& filter) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("User-Agent", "ESP32-FIFA/1.0");
    http.useHTTP10(true);   // reponse non-chunkee + Content-Length

    int code = http.GET();
    if (code != 200) {
        DBG("[API] HTTP %d pour %s\n", code, url);
        http.end();
        return false;
    }
    DBG("[API] HTTP 200 – %d octets\n", http.getSize());

    BlockingStream stream(http.getStream(), HTTP_TIMEOUT_MS);
    DeserializationError err = deserializeJson(
        doc, stream,
        DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(20));
    http.end();

    if (err) {
        DBG("[API] JSON parse error: %s\n", err.c_str());
        return false;
    }
    return true;
}

// ============================================================
// Goal parsing helpers
// ============================================================

static std::vector<Goal> parseGoals(JsonArray details, const String& teamId) {
    std::vector<Goal> goals;
    for (JsonObject detail : details) {
        // "scoringPlay" est vrai pour TOUTES les variantes de but (Goal,
        // Goal - Header, Penalty - Scored, Goal - Free-kick, Own Goal...).
        // On s'appuie dessus plutot que sur le libelle exact, qui variait
        // et faisait manquer les buts de la tete / penalty / coup franc.
        if (!(detail["scoringPlay"] | false)) continue;

        const char* typeText = detail["type"]["text"] | "";
        bool isOwnGoal = (strstr(typeText, "Own Goal") != nullptr);

        const char* detailTeamId = detail["team"]["id"] | "";
        if (String(detailTeamId) != teamId) continue;

        Goal g;
        g.isOwnGoal  = isOwnGoal;
        g.playerName = deaccent(detail["athletesInvolved"][0]["displayName"] | "Unknown");
        g.minute     = detail["clock"]["displayValue"] | "";
        goals.push_back(g);
    }
    return goals;
}

// ============================================================
// Scoreboard
// ============================================================

// Recupere et parse un scoreboard.
//  - merge=false : remplace toute la liste (chargement complet boot/idle).
//  - merge=true  : met a jour les matchs existants par matchId et ajoute les
//                  nouveaux (rafraichissement live leger), sans perdre la liste
//                  complete de la competition.
static bool parseScoreboard(const char* url, bool merge) {
    DBG("[API] Fetching scoreboard (%s)...\n", merge ? "live" : "complet");

    // Filtre en PSRAM : ne garde que les champs utiles (economise la RAM)
    JsonDocument filter(&psramAlloc);
    filter["events"][0]["id"]   = true;
    filter["events"][0]["date"] = true;

    filter["events"][0]["status"]["type"]["state"]       = true;
    filter["events"][0]["status"]["type"]["description"] = true;
    filter["events"][0]["status"]["displayClock"]        = true;
    filter["events"][0]["status"]["period"]              = true;

    filter["events"][0]["competitions"][0]["competitors"][0]["homeAway"]             = true;
    filter["events"][0]["competitions"][0]["competitors"][0]["score"]                = true;
    filter["events"][0]["competitions"][0]["competitors"][0]["team"]["id"]           = true;
    filter["events"][0]["competitions"][0]["competitors"][0]["team"]["abbreviation"] = true;
    filter["events"][0]["competitions"][0]["competitors"][0]["team"]["displayName"]  = true;

    filter["events"][0]["competitions"][0]["venue"]["fullName"]        = true;
    filter["events"][0]["competitions"][0]["venue"]["address"]["city"] = true;
    filter["events"][0]["competitions"][0]["details"]                  = true;
    filter["events"][0]["competitions"][0]["notes"][0]["headline"]     = true;

    JsonDocument doc(&psramAlloc);
    if (!fetchJson(url, doc, filter)) return false;

    JsonArray events = doc["events"];
    DBG("[API] Evenements apres filtre: %d\n", (int)events.size());

    if (events.size() > 0) {
        DBG("[API] Premier match: id=%s date=%s statut=%s\n",
            (const char*)(events[0]["id"] | "?"),
            (const char*)(events[0]["date"] | "?"),
            (const char*)(events[0]["status"]["type"]["state"] | "?"));
    }

    std::vector<Match> newMatches;
    for (JsonObject ev : events) {
        Match m;
        m.matchId = ev["id"] | "";
        m.date    = ev["date"] | "";

        JsonObject status = ev["status"];
        m.status       = status["type"]["state"] | "pre";
        m.statusDetail = status["type"]["description"] | "";
        m.clock        = status["displayClock"] | "";
        m.period       = status["period"] | 0;

        JsonObject comp = ev["competitions"][0];
        m.venueName = deaccent(comp["venue"]["fullName"] | "");
        m.venueCity = deaccent(comp["venue"]["address"]["city"] | "");
        m.group     = comp["notes"][0]["headline"] | "";

        JsonArray competitors = comp["competitors"];
        for (JsonObject team : competitors) {
            bool isHome = strcmp(team["homeAway"] | "", "home") == 0;
            String id   = team["team"]["id"] | "";
            String abbr = team["team"]["abbreviation"] | "";
            String name = deaccent(team["team"]["displayName"] | "");
            int score   = atoi(team["score"] | "0");

            if (isHome) {
                m.homeTeamId = id; m.homeTeam = abbr;
                m.homeTeamName = name; m.homeScore = score;
            } else {
                m.awayTeamId = id; m.awayTeam = abbr;
                m.awayTeamName = name; m.awayScore = score;
            }
        }

        JsonArray details = comp["details"];
        m.homeGoals    = parseGoals(details, m.homeTeamId);
        m.awayGoals    = parseGoals(details, m.awayTeamId);
        m.lastUpdateMs = millis();
        newMatches.push_back(m);
    }

    // Detection de buts (compare ancien etat vs nouveau) -> popups
    EspnApi::detectNewGoals(gCtx.matches, newMatches);

    if (merge) {
        // Met a jour les matchs existants (par matchId), ajoute les nouveaux.
        for (const auto& nm : newMatches) {
            bool found = false;
            for (auto& em : gCtx.matches) {
                if (em.matchId == nm.matchId) { em = nm; found = true; break; }
            }
            if (!found) gCtx.matches.push_back(nm);
        }
        DBG("[API] Scoreboard live: %d matchs maj (%d total)\n",
            (int)newMatches.size(), (int)gCtx.matches.size());
    } else {
        gCtx.matches = newMatches;
        DBG("[API] Scoreboard: %d matches loaded\n", (int)gCtx.matches.size());
    }

    gCtx.dataReady       = true;
    gCtx.needsFullRedraw = true;
    return true;
}

bool EspnApi::fetchScoreboard() {
    return parseScoreboard(API_SCOREBOARD_URL, false);       // complet
}

bool EspnApi::fetchLiveScoreboard() {
    return parseScoreboard(API_SCOREBOARD_LIVE_URL, true);   // leger + fusion
}

// ============================================================
// Standings
// ============================================================

bool EspnApi::fetchStandings() {
    DBG("[API] Fetching standings...\n");

    // Filtre en PSRAM. Structure reelle : les groupes sont sous
    // "children"[], chacun avec "name" et "standings"."entries"[].
    JsonDocument filter(&psramAlloc);
    filter["children"][0]["name"] = true;
    filter["children"][0]["standings"]["entries"][0]["team"]["abbreviation"] = true;
    filter["children"][0]["standings"]["entries"][0]["team"]["displayName"]  = true;
    filter["children"][0]["standings"]["entries"][0]["stats"][0]["name"]     = true;
    filter["children"][0]["standings"]["entries"][0]["stats"][0]["value"]    = true;

    JsonDocument doc(&psramAlloc);
    if (!fetchJson(API_STANDINGS_URL, doc, filter)) return false;

    std::vector<GroupStanding> groups;
    JsonArray children = doc["children"];
    for (JsonObject grp : children) {
        GroupStanding gs;
        gs.groupName = grp["name"] | "";

        JsonArray entries = grp["standings"]["entries"];
        for (JsonObject entry : entries) {
            TeamStanding ts;
            ts.teamCode = entry["team"]["abbreviation"] | "";
            ts.teamName = deaccent(entry["team"]["displayName"]  | "");

            JsonArray stats = entry["stats"];
            for (JsonObject stat : stats) {
                const char* name = stat["name"] | "";
                // Les valeurs ESPN sont des flottants ("3.0"). "| 0" echoue
                // (is<int>() est faux pour un flottant) et renvoyait 0 ->
                // classement toujours a zero. On lit en flottant puis on caste.
                int          val = (int)lround(stat["value"] | 0.0);
                if      (strcmp(name, "rank")              == 0) ts.rank         = val;
                else if (strcmp(name, "points")            == 0) ts.points       = val;
                else if (strcmp(name, "gamesPlayed")       == 0) ts.played       = val;
                else if (strcmp(name, "wins")              == 0) ts.wins         = val;
                else if (strcmp(name, "ties")              == 0) ts.draws        = val;
                else if (strcmp(name, "losses")            == 0) ts.losses       = val;
                else if (strcmp(name, "pointsFor")         == 0) ts.goalsFor     = val;
                else if (strcmp(name, "pointsAgainst")     == 0) ts.goalsAgainst = val;
                else if (strcmp(name, "pointDifferential") == 0) ts.goalDiff     = val;
            }
            gs.teams.push_back(ts);
        }
        // Tri par position au classement (rank croissant)
        std::sort(gs.teams.begin(), gs.teams.end(),
                  [](const TeamStanding& a, const TeamStanding& b) {
                      return a.rank < b.rank;
                  });
        groups.push_back(gs);
    }

    gCtx.standings = groups;
    DBG("[API] Standings: %d groups loaded\n", (int)gCtx.standings.size());
    return true;
}

// ============================================================
// Live check
// ============================================================

bool EspnApi::hasLiveMatch() {
    for (const auto& m : gCtx.matches) {
        if (m.status == "in") return true;
    }
    return false;
}

// ============================================================
// Goal detection
// ============================================================

static bool goalAlreadyExists(const std::vector<Goal>& list, const Goal& g) {
    for (const auto& old : list) {
        if (old.playerName == g.playerName && old.minute == g.minute) return true;
    }
    return false;
}

static void enqueueGoal(const Goal& goal, const Match& match) {
    GoalPopup popup;
    popup.goal      = goal;
    popup.homeTeam  = match.homeTeam;
    popup.awayTeam  = match.awayTeam;
    popup.homeScore = match.homeScore;
    popup.awayScore = match.awayScore;
    popup.showUntil = millis() + GOAL_POPUP_DURATION_MS;
    gCtx.popupQueue.push(popup);
    DBG("[GOAL] Queued: %s %s %s'\n",
        goal.playerName.c_str(), goal.teamCode.c_str(), goal.minute.c_str());
}

void EspnApi::detectNewGoals(const std::vector<Match>& oldMatches,
                             const std::vector<Match>& newMatches) {
    for (const auto& nm : newMatches) {
        if (nm.status != "in") continue;

        const Match* om = nullptr;
        for (const auto& m : oldMatches) {
            if (m.matchId == nm.matchId) { om = &m; break; }
        }

        auto check = [&](const std::vector<Goal>& newGoals,
                         const std::vector<Goal>& oldGoals,
                         const String& teamCode) {
            for (auto g : newGoals) {
                g.teamCode = teamCode;
                if (!goalAlreadyExists(oldGoals, g)) enqueueGoal(g, nm);
            }
        };

        std::vector<Goal> emptyGoals;
        check(nm.homeGoals, om ? om->homeGoals : emptyGoals, nm.homeTeam);
        check(nm.awayGoals, om ? om->awayGoals : emptyGoals, nm.awayTeam);
    }
}

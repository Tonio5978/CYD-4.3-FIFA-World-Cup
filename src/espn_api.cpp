#include "espn_api.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
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
        const char* typeText = detail["type"]["text"] | "";
        bool isGoal    = strcmp(typeText, "Goal")     == 0;
        bool isOwnGoal = strcmp(typeText, "Own Goal") == 0;
        if (!isGoal && !isOwnGoal) continue;

        const char* detailTeamId = detail["team"]["id"] | "";
        if (String(detailTeamId) != teamId) continue;

        Goal g;
        g.isOwnGoal  = isOwnGoal;
        g.playerName = detail["athletesInvolved"][0]["displayName"] | "Unknown";
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
        m.venueName = comp["venue"]["fullName"] | "";
        m.venueCity = comp["venue"]["address"]["city"] | "";
        m.group     = comp["notes"][0]["headline"] | "";

        JsonArray competitors = comp["competitors"];
        for (JsonObject team : competitors) {
            bool isHome = strcmp(team["homeAway"] | "", "home") == 0;
            String id   = team["team"]["id"] | "";
            String abbr = team["team"]["abbreviation"] | "";
            String name = team["team"]["displayName"] | "";
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
            ts.teamName = entry["team"]["displayName"]  | "";

            JsonArray stats = entry["stats"];
            for (JsonObject stat : stats) {
                const char* name = stat["name"] | "";
                int          val = stat["value"] | 0;
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

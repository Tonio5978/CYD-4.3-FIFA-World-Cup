#pragma once
#include <Arduino.h>
#include <vector>
#include <queue>

// ============================================================
// App state machine
// ============================================================

enum AppState {
    STATE_SPLASH,
    STATE_HOME_LIVE,    // live matches
    STATE_HOME_NEXT,    // upcoming matches
    STATE_GROUP         // group standings (groupIndex 0-15)
};

enum PopupState {
    POPUP_NONE,
    POPUP_GOAL_SHOWING
};

// ============================================================
// Data structures
// ============================================================

struct Goal {
    String playerName;
    String teamCode;    // "BRA"
    String minute;      // "67'" or "45'+2"
    bool   isOwnGoal = false;
};

struct Match {
    String matchId;
    String homeTeam;        // "BRA"
    String homeTeamName;    // "Brazil"
    String homeTeamId;
    String awayTeam;        // "ARG"
    String awayTeamName;    // "Argentina"
    String awayTeamId;
    int    homeScore = 0;
    int    awayScore = 0;
    String status;          // "pre" | "in" | "post"
    String statusDetail;    // "In Progress" | "Final" | "Scheduled"
    String clock;           // "67:23"
    int    period  = 0;     // 1=1st half, 2=2nd half, 3=ET, 4=penalties
    String venueName;
    String venueCity;
    String date;            // ISO "2026-06-11T18:00:00Z"
    String group;           // "Group A"
    std::vector<Goal> homeGoals;
    std::vector<Goal> awayGoals;
    uint32_t lastUpdateMs = 0;
};

struct TeamStanding {
    String teamCode;
    String teamName;
    int rank = 0;
    int points = 0;
    int played = 0;
    int wins   = 0;
    int draws  = 0;
    int losses = 0;
    int goalsFor     = 0;
    int goalsAgainst = 0;
    int goalDiff     = 0;
};

struct GroupStanding {
    String groupName;   // "Group A"
    std::vector<TeamStanding> teams;
};

struct GoalPopup {
    Goal   goal;
    String homeTeam;
    String awayTeam;
    int    homeScore = 0;
    int    awayScore = 0;
    uint32_t showUntil = 0;  // millis() + GOAL_POPUP_DURATION_MS
};

// ============================================================
// Shared app context (filled by each module, read by UI)
// ============================================================

struct AppContext {
    AppState   appState   = STATE_SPLASH;
    PopupState popupState = POPUP_NONE;

    std::vector<Match>        matches;
    std::vector<GroupStanding> standings;

    GoalPopup              currentPopup;
    std::queue<GoalPopup>  popupQueue;

    // Active group index for STATE_GROUP (0 = Group A, … 15 = Group P)
    int activeGroupIndex = 0;

    // Timestamps
    uint32_t lastScoreboardFetch   = 0;
    uint32_t lastStandingsFetch    = 0;
    uint32_t lastNtpSync           = 0;
    uint32_t lastClockDraw         = 0;

    // Status flags
    bool wifiConnected    = false;
    bool ntpSynced        = false;
    bool dataReady        = false;
    bool needsFullRedraw  = true;

    // Current local time (populated by ntp_time)
    struct tm localTime {};
};

extern AppContext gCtx;

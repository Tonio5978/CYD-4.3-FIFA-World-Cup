#pragma once
#include "../include/screens.h"

namespace EspnApi {
    // Fetch & parse the FULL competition scoreboard (104 matchs) into
    // gCtx.matches (replaces the list). Used at boot and when idle.
    bool fetchScoreboard();

    // Fetch the light "today" scoreboard and MERGE updates into gCtx.matches
    // (by matchId). Used for the 10 s live refresh to avoid re-downloading
    // the whole competition (which froze the touch input).
    bool fetchLiveScoreboard();

    // Fetch & parse group standings into gCtx.standings.
    bool fetchStandings();

    // Returns true if at least one match has status "in".
    bool hasLiveMatch();

    // Compare old vs new match list and enqueue goal popups.
    void detectNewGoals(const std::vector<Match>& oldMatches,
                        const std::vector<Match>& newMatches);
}

#pragma once
#include "../include/screens.h"

namespace EspnApi {
    // Fetch & parse live scoreboard into gCtx.matches.
    // Returns true on success.
    bool fetchScoreboard();

    // Fetch & parse group standings into gCtx.standings.
    bool fetchStandings();

    // Returns true if at least one match has status "in".
    bool hasLiveMatch();

    // Compare old vs new match list and enqueue goal popups.
    void detectNewGoals(const std::vector<Match>& oldMatches,
                        const std::vector<Match>& newMatches);
}

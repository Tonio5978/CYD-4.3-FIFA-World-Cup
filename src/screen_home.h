#pragma once
namespace ScreenHome {
    void draw();    // full redraw (live or next matches depending on gCtx)
    void update();  // partial updates (clock, live dot blink)

    // Fait defiler la liste des prochains matchs (deltaPx > 0 = vers le bas).
    void scrollUpcoming(int deltaPx);
}

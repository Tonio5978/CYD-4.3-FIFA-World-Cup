#pragma once
namespace ScreenHome {
    void draw();    // full redraw (live or next matches depending on gCtx)
    void update();  // partial updates (clock, live dot blink)
}

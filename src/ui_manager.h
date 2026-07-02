#pragma once
#include <LovyanGFX.hpp>
#include "display_config.h"
#include "../include/screens.h"
#include "../include/config.h"

// ============================================================
// Drawing utilities shared across screens
// ============================================================
namespace UI {

    // --- Primitives ---
    void drawHeader(const String& label, const String& icon);
    void drawFooter(int activeGroupIndex);
    void drawDivider(int y, uint32_t color = 0x444444);

    // --- Flag rendering ---
    // Draw a team flag from SD cache at (x,y) with given size.
    // large=true  → uses flags/large/<CODE>.png (200×150 source)
    // large=false → uses flags/<CODE>.png       (64×48 source)
    // Falls back to team colour block if flag not found.
    void drawFlag(const String& teamCode, int x, int y, int w, int h, bool large = false);

    // --- Goal popup (full lifecycle) ---
    void updateGoalPopup();     // call every loop() tick
    void drawGoalPopup();
    void animatePopupIn();
    void animatePopupOut();

    // --- Colour helpers ---
    uint32_t teamGradientColor1(const String& teamCode);
    uint32_t teamGradientColor2(const String& teamCode);
    uint16_t lerpColor565(uint16_t c1, uint16_t c2, float t);
    void     drawGradientRect(int x, int y, int w, int h,
                              uint16_t c1, uint16_t c2);

    // --- Touch ---
    // Returns true if a tap was detected; sets (tx, ty).
    bool getTap(int& tx, int& ty);

    // Navigate with footer touch
    void handleFooterTouch(int tx, int ty);

    // True if the tap hit the trophy icon (bracket access) in the header.
    bool isTrophyTouch(int tx, int ty);
}

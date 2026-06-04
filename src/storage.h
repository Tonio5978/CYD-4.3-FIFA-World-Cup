#pragma once
#include <Arduino.h>

namespace Storage {
    bool begin();       // mount SD card
    bool isReady();

    // Load a flag PNG from SD into a caller-supplied PSRAM buffer.
    // large=false → data/flags/<CODE>.png        (64×48)
    // large=true  → data/flags/large/<CODE>.png  (200×150)
    // outBuf must be pre-allocated with w*h*2 bytes (RGB565).
    bool loadFlagRGB565(const String& teamCode, uint16_t* outBuf, int w, int h,
                        bool large = false);

    bool exists(const String& path);
}

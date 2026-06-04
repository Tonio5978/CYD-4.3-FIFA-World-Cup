# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 firmware for the **Sunton ESP32-8048S043C** ("Cheap Yellow Display" 4.3") that shows FIFA World Cup 2026 live scores, group standings, and goal-celebration popups. Data comes from the public ESPN API (no key required).

**Hardware**: ESP32-S3 @ 240 MHz, 16 MB Flash, 8 MB PSRAM OPI, 4.3" IPS RGB 800×480, GT911 touch, SD card, I2S audio amp (MA98357).

## Build & Flash

```sh
# Build
pio run

# Flash
pio run --target upload

# Upload the LittleFS image (team flags from data/) — run once, and
# again whenever files under data/ change. Separate from the firmware flash.
pio run --target uploadfs

# Monitor serial output
pio device monitor

# Build + flash + monitor
pio run --target upload && pio device monitor
```

## Project Structure

```
include/
  config.h        – all #define constants (WiFi, API, colours, pins)
  screens.h       – shared data structs (Match, Goal, GoalPopup, AppContext gCtx)

src/
  main.cpp          – setup() / loop() – state machine, API polling, touch
  display_config.h  – LovyanGFX LGFX class (RGB parallel bus + GT911 touch)
  wifi_manager      – connect / auto-reconnect
  ntp_time          – timezone config, periodic sync, formatTime/formatDate
  storage           – LittleFS mount, PNG flag loader (PNGdec → RGB565 buffer)
  espn_api          – HTTP fetch, JSON parse (scoreboard + standings), goal detection
  ui_manager        – header, footer, flag cache (PSRAM LRU), goal popup lifecycle
  screen_splash     – boot screen with animated spinner + status lines
  screen_home       – live match cards OR upcoming match list
  screen_group      – split view: standings table (left) + group matches (right)

data/   – LittleFS image (flashed to the spiffs partition via `pio run -t uploadfs`)
  flags/        – <TEAMCODE>.png files (64×48 px, e.g. BRA.png)
  flags/large/  – optional 200×150 px versions for the goal popup
  sounds/       – optional goal_sound.wav (16 kHz mono WAV)
```

## Key Architecture Decisions

**Single global context** — `AppContext gCtx` (defined in `main.cpp`, declared `extern` in `screens.h`) is the single source of truth shared by all modules.

**Display library** — [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (not TFT_eSPI) because the ESP32-8048S043 uses an RGB parallel 16-bit interface that TFT_eSPI does not support. All drawing goes through the global `LGFX gfx` instance.

**Flag cache** — `ui_manager.cpp` maintains a 12-slot LRU cache of PSRAM-allocated RGB565 pixel buffers. Flags are loaded once from **LittleFS** (internal Flash, `spiffs` partition — not the SD card) via `PNGdec` and served from RAM on every subsequent draw.

**Adaptive API polling** — scoreboard refreshes every 10 s when a match is live, 5 min otherwise. Standings refresh every 10 min.

**Goal popup queue** — `EspnApi::detectNewGoals()` compares old vs new `Match` lists after each fetch and pushes `GoalPopup` entries onto `gCtx.popupQueue`. `UI::updateGoalPopup()` (called every loop tick) dequeues and animates them one by one.

## First-time Setup

1. Edit `include/config.h` — set `WIFI_SSID` and `WIFI_PASSWORD`.
2. Resize 48 team flag PNGs to **64×48 px**, name them `BRA.png`, `ARG.png`, etc., and place them in `data/flags/` (in the repo, not the SD card).
3. (Optional) Add 200×150 px versions under `data/flags/large/` for the goal popup, and `goal_sound.wav` (16 kHz, mono) under `data/sounds/` with `ENABLE_GOAL_SOUND true`.
4. Upload the flags to internal Flash: `pio run --target uploadfs`.
5. Flash the firmware: `pio run --target upload`.

## ESPN API Endpoints

| Purpose | URL |
|---------|-----|
| Live scoreboard | `https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard?limit=950&dates=20260611-20260720` |
| Group standings | `https://site.web.api.espn.com/apis/v2/sports/soccer/fifa.world/standings` |

## Memory Budget (estimated)

| Item | Size | Location |
|------|------|----------|
| Framebuffer (800×480×2) | 768 KB | PSRAM |
| Flag cache 12× (64×48×2) | 72 KB | PSRAM |
| Flag cache 3× popup (200×150×2) | 180 KB | PSRAM |
| JSON parse buffer | ~50 KB | PSRAM |
| Match structs | ~50 KB | SRAM |
| Stack + FreeRTOS | ~100 KB | SRAM |

PSRAM used ≈ 1.1 MB of 8 MB available; SRAM well within 512 KB limit.

## Touch Pin Verification

The GT911 I2C pins in `display_config.h` (`SDA=19, SCL=20, INT=18, RST=38`) are the most common values for this board but vary between revisions. If touch does not respond, check the board's silkscreen or schematic and update those four values.

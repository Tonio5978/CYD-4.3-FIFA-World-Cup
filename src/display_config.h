#pragma once

// LovyanGFX driver for Sunton ESP32-8048S043C
// RGB 16-bit parallel interface, 800x480, GT911 touch

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_RGB     _panel_instance;
    lgfx::Bus_RGB       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_GT911   _touch_instance;

public:
    LGFX() {
        // --- Panel ---
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width   = 800;
            cfg.memory_height  = 480;
            cfg.panel_width    = 800;
            cfg.panel_height   = 480;
            cfg.offset_x       = 0;
            cfg.offset_y       = 0;
            cfg.offset_rotation = 0;
            _panel_instance.config(cfg);
        }

        // --- PSRAM framebuffer ---
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = 1;
            _panel_instance.config_detail(cfg);
        }

        // --- RGB bus ---
        {
            auto cfg = _bus_instance.config();
            cfg.panel      = &_panel_instance;

            // Blue  (B4..B0)
            cfg.pin_d0  = GPIO_NUM_8;
            cfg.pin_d1  = GPIO_NUM_3;
            cfg.pin_d2  = GPIO_NUM_46;
            cfg.pin_d3  = GPIO_NUM_9;
            cfg.pin_d4  = GPIO_NUM_1;
            // Green (G5..G0)
            cfg.pin_d5  = GPIO_NUM_5;
            cfg.pin_d6  = GPIO_NUM_6;
            cfg.pin_d7  = GPIO_NUM_7;
            cfg.pin_d8  = GPIO_NUM_15;
            cfg.pin_d9  = GPIO_NUM_16;
            cfg.pin_d10 = GPIO_NUM_4;
            // Red   (R4..R0)
            cfg.pin_d11 = GPIO_NUM_45;
            cfg.pin_d12 = GPIO_NUM_48;
            cfg.pin_d13 = GPIO_NUM_47;
            cfg.pin_d14 = GPIO_NUM_21;
            cfg.pin_d15 = GPIO_NUM_14;

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync   = GPIO_NUM_41;
            cfg.pin_hsync   = GPIO_NUM_39;
            cfg.pin_pclk    = GPIO_NUM_42;

            cfg.freq_write        = 14000000;   // 14 MHz — stable (16 MHz decalait le texte)
            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 16;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 4;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 4;
            cfg.pclk_active_neg   = 1;
            cfg.de_idle_high      = 0;
            cfg.pclk_idle_high    = 1;          // doit être 1 pour ce panel

            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);

        // --- Backlight ---
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl    = GPIO_NUM_2;
            cfg.invert    = false;
            cfg.freq      = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        // --- Touch GT911 ---
        {
            auto cfg = _touch_instance.config();
            // Resolution NATIVE du digitizer GT911 (≈480x272 WQVGA),
            // pas celle de la dalle. LovyanGFX mappe [min,max] -> 800x480.
            cfg.x_min      = 0;
            cfg.x_max      = 480;
            cfg.y_min      = 0;
            cfg.y_max      = 272;
            // pin_rst/pin_int a -1 : on saute la sequence de reset GT911
            // de LovyanGFX (elle laissait la puce muette). La puce est
            // deja saine a 0x5D des la mise sous tension -> lecture en
            // pur polling I2C. (INT/RST physiques restent GPIO18/GPIO38.)
            cfg.pin_int    = -1;
            cfg.pin_rst    = -1;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port   = 0;
            cfg.i2c_addr   = 0x5D;
            cfg.pin_sda    = GPIO_NUM_19;
            cfg.pin_scl    = GPIO_NUM_20;
            cfg.freq       = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

// Global display instance (extern, defined in main.cpp)
extern LGFX gfx;

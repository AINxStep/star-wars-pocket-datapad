#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// =========================================================================
// LovyanGFX Configuration for LCDWIKI ES3C28P (2.8" ESP32-S3 display)
// ILI9341V IPS 240x320 on FSPI. Pins from LCDWIKI's IO allocation table.
// The panel reset is wired to CHIP_PU (shared with the ESP32 reset): it is
// not a GPIO, so pin_rst stays -1 and the panel resets with the chip.
// =========================================================================

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;     // 40 MHz: ~30 ms per full fill, still fully hidden
                                           // by the strobe backlight-off trick. 80 MHz showed
                                           // marginal signal integrity — a corrupt byte mid-band
                                           // shifted every pixel after it (displaced borders).
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = 12;
            cfg.pin_mosi = 11;
            cfg.pin_miso = 13;
            cfg.pin_dc   = 46;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 10;
            cfg.pin_rst          = -1;     // shared CHIP_PU reset
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = true;   // IPS panel: needs INVON
            cfg.rgb_order        = false;  // BGR
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;               // active high
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

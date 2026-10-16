#pragma once
#include <Arduino.h>

// =========================================================================
// Interface language: Spanish, English or Aurebesh.
// Aurebesh is an alphabet, not a language, so it shows English text drawn
// with the Aurebesh font. Glyphs are much wider than Latin letters, so that
// column uses shorter wording where the layout is tight.
// =========================================================================

enum Lang : uint8_t {
    LANG_ES,
    LANG_EN,
    LANG_AUR,
    LANG_COUNT
};

enum StrId {
    S_BACK,
    S_SETTINGS,

    // Home
    S_HOME_BADGE,
    S_HOME_RADAR,
    S_HOME_COMLINK,
    S_HOME_AUREBESH,
    S_HOME_HOLO,

    // Splash
    S_SPLASH_SUB,
    S_BOOT_1, S_BOOT_2, S_BOOT_3, S_BOOT_4,

    // Radar
    S_RADAR_TITLE,
    S_RADAR_ALARM,
    S_RADAR_HOSTILE_FMT,     // %.1f km, %03d bearing, %d allies
    S_RADAR_CLEAR_FMT,       // %d allies

    // Comlink
    S_COMLINK_TITLE,
    S_COMLINK_R2D2, S_COMLINK_BLASTER, S_COMLINK_CHANNEL, S_COMLINK_ALARM,
    S_COMLINK_LINK,
    S_LINK_OFF, S_LINK_SEARCH, S_LINK_FOUND,
    S_MSG_RX,                // "message received" banner title

    // Aurebesh keyboard
    S_AUREBESH_TITLE,
    S_AUREBESH_SPACE,

    // Holo
    S_HOLO_TITLE,
    S_HOLO_DESC_0, S_HOLO_DESC_1, S_HOLO_DESC_2, S_HOLO_DESC_3,
    S_HOLO_DESC_4, S_HOLO_DESC_5, S_HOLO_DESC_6,   // keep in HoloId order
    S_HOLO_DESC_7, S_HOLO_DESC_8, S_HOLO_DESC_9,

    // Settings
    S_SET_BRIGHTNESS,
    S_SET_VOLUME,
    S_SET_CALIBRATE,
    S_SET_HINT,
    S_SET_SLEEP,
    S_SET_BATT,
    S_SET_RESTART,
    S_AUTO,

    // Calibration
    S_CAL_PROMPT_FMT,        // %d step
    S_CAL_SAVED,
    S_CAL_ERROR,
    S_CAL_CANCEL,

    // Starchart (descs in SYSTEMS order, see screens/starchart.cpp)
    S_MAP_TITLE,
    S_MAP_TAP,               // prompt shown before a system is selected
    S_MAP_HOLO,              // button: open the system as a hologram
    S_MAP_DESC_0, S_MAP_DESC_1, S_MAP_DESC_2, S_MAP_DESC_3,
    S_MAP_DESC_4, S_MAP_DESC_5, S_MAP_DESC_6, S_MAP_DESC_7,
    S_MAP_DESC_8, S_MAP_DESC_9, S_MAP_DESC_10, S_MAP_DESC_11,

    // Signal beacon
    S_BCN_TITLE,
    S_BCN_SEND,
    S_BCN_STOP,
    S_BCN_STROBE,
    S_BCN_HINT,              // where to compose the message

    S_COUNT
};

Lang lang_get(void);
const char* tr(StrId id);

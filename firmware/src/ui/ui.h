#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// UI Init & Lifecycle
void ui_init(void);
void ui_update(void);

// Screen Navigation
void ui_goto_home(void);
void ui_goto_radar(void);
void ui_goto_comlink(void);
void ui_goto_holo(void);
void ui_goto_aurebesh(void);
void ui_goto_settings(void);
void ui_goto_calibration(void);
void ui_goto_map(void);
void ui_goto_beacon(void);

// Cross-screen helpers
void holo_show_model(int idx);          // starchart -> hologram projector
const char* aurebesh_msg_get(void);     // beacon <- keyboard message line

// Top Status Bar Updates
void ui_set_status_time(const char* time_str);
void ui_set_status_comlink(const char* status_str);
void ui_set_battery_pct(int pct);

#ifdef __cplusplus
}
#endif

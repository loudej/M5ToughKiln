#ifndef UI_SETTINGS_SCREEN_H
#define UI_SETTINGS_SCREEN_H

#include <lvgl.h>

void ui_settings_screen_create();
lv_obj_t* ui_settings_screen_get();

/// Refresh read-only Wi-Fi labels when the settings screen is visible (call from main loop).
void ui_settings_screen_update_status();

#endif // UI_SETTINGS_SCREEN_H

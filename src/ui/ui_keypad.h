#ifndef UI_KEYPAD_H
#define UI_KEYPAD_H

#include <lvgl.h>

// Initializes the global keypad (should be called once at startup)
void ui_keypad_init();

// Attaches the keypad to a specific text area and shows it
void ui_keypad_set_target(lv_obj_t * ta);

// Shows the keypad manually
void ui_keypad_show();

// Hides the keypad
void ui_keypad_hide();

// Standard event callback to attach to any text area that needs the keypad
void ui_ta_event_cb(lv_event_t * e);

#endif // UI_KEYPAD_H

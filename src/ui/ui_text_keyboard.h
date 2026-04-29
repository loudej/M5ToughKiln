#ifndef UI_TEXT_KEYBOARD_H
#define UI_TEXT_KEYBOARD_H

#include <lvgl.h>

void ui_text_keyboard_init();

/** If true before `ui_text_keyboard_set_target`, uses digit pad (`.`, `+/-`, `0`-`9`)
 *  anchored bottom-left; otherwise full alphanumeric bottom-center. */
void ui_text_keyboard_set_preset_settings_numeric(bool numeric_layout);

void ui_text_keyboard_set_target(lv_obj_t * ta);

/** Drop textarea binding before deleting that textarea (avoids dangling ptr in lv_keyboard). */
void ui_text_keyboard_clear_target();

void ui_text_keyboard_hide();

#endif

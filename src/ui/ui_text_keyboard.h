#ifndef UI_TEXT_KEYBOARD_H
#define UI_TEXT_KEYBOARD_H

#include <lvgl.h>

void ui_text_keyboard_init();

void ui_text_keyboard_set_target(lv_obj_t * ta);

/** Drop textarea binding before deleting that textarea (avoids dangling ptr in lv_keyboard). */
void ui_text_keyboard_clear_target();

void ui_text_keyboard_hide();

#endif

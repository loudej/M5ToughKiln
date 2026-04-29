#include "../lvgl_includes.h"
#include "ui_text_keyboard.h"
#include "ui_keypad.h"

static lv_obj_t *text_kb;
/** When true: number pad + anchored bottom-left (settings PID numeric fields). */
static bool        s_kb_settings_numeric_layout = false;

static void kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        ui_text_keyboard_hide();
        lv_obj_t *ta = lv_keyboard_get_textarea(text_kb);
        if (ta)
            lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    }
}

static void apply_keyboard_layout_preset() {
    if (!text_kb)
        return;

    lv_display_t *d      = lv_display_get_default();
    const lv_coord_t hres = lv_display_get_horizontal_resolution(d);
    const lv_coord_t vres = lv_display_get_vertical_resolution(d);
    /* Leave room above the keypad for the focused row on the right. */
    const lv_coord_t kp_h = vres / 2 + 24;

    if (s_kb_settings_numeric_layout) {
        lv_keyboard_set_mode(text_kb, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_set_width(text_kb, hres * 58 / 100);
        lv_obj_set_height(text_kb, kp_h);
        lv_obj_align(text_kb, LV_ALIGN_BOTTOM_LEFT, 4, 0);
    } else {
        lv_keyboard_set_mode(text_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_set_size(text_kb, hres, kp_h);
        lv_obj_align(text_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
}

void ui_text_keyboard_init() {
    text_kb = lv_keyboard_create(lv_layer_top());
    lv_obj_add_event_cb(text_kb, kb_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(text_kb, LV_OBJ_FLAG_HIDDEN);
    s_kb_settings_numeric_layout = false;
    apply_keyboard_layout_preset();
}

void ui_text_keyboard_set_preset_settings_numeric(bool numeric_layout) {
    s_kb_settings_numeric_layout = numeric_layout;
    apply_keyboard_layout_preset();
}

void ui_text_keyboard_set_target(lv_obj_t *ta) {
    if (!text_kb)
        return;
    ui_keypad_hide();
    apply_keyboard_layout_preset();
    lv_keyboard_set_textarea(text_kb, ta);
    lv_obj_clear_flag(text_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(text_kb);
}

void ui_text_keyboard_clear_target() {
    if (!text_kb)
        return;
    lv_keyboard_set_textarea(text_kb, nullptr);
}

void ui_text_keyboard_hide() {
    if (!text_kb)
        return;
    lv_obj_add_flag(text_kb, LV_OBJ_FLAG_HIDDEN);
}

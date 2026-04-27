#include "../lvgl_includes.h"
#include "ui_keypad.h"

void ui_text_keyboard_hide();

static constexpr lv_coord_t KEYPAD_W = 140;
static constexpr lv_coord_t KEYPAD_HT = 200;

static lv_obj_t * global_kb = NULL;

static const char * kb_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    ".", "0", LV_SYMBOL_BACKSPACE, NULL
};

static const lv_btnmatrix_ctrl_t kb_ctrl[] = {
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT
};

static void kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * target = lv_keyboard_get_textarea(global_kb);
    
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        ui_keypad_hide();
        if (target) {
            lv_obj_clear_state(target, LV_STATE_FOCUSED);
        }
    }
}

void ui_ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        ui_keypad_set_target(ta);
    } else if (code == LV_EVENT_DEFOCUSED) {
        ui_keypad_hide();
    } else if (code == LV_EVENT_DELETE) {
        if (global_kb && lv_keyboard_get_textarea(global_kb) == ta) {
            lv_keyboard_set_textarea(global_kb, NULL);
        }
    }
}

void ui_keypad_init() {
    // Create keyboard on the top layer so it sits above all screens/popups
    global_kb = lv_keyboard_create(lv_layer_top());
    
    // Apply custom 3x4 layout
    lv_keyboard_set_map(global_kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
    lv_keyboard_set_mode(global_kb, LV_KEYBOARD_MODE_USER_1);
    
    // Size and position: Right side, mostly full height
    lv_obj_set_size(global_kb, KEYPAD_W, KEYPAD_HT);
    lv_obj_align(global_kb, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    
    lv_obj_add_event_cb(global_kb, kb_event_cb, LV_EVENT_ALL, NULL);
    
    // Start hidden
    lv_obj_add_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
}

void ui_keypad_set_target(lv_obj_t * ta) {
    if (!global_kb) return;
    ui_text_keyboard_hide();
    lv_keyboard_set_textarea(global_kb, ta);
    ui_keypad_show();
}

void ui_keypad_show() {
    if (!global_kb) return;
    lv_obj_clear_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
}

void ui_keypad_hide() {
    if (!global_kb) return;
    lv_obj_add_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
}

#include "ui_main_screen.h"
#include "ui.h"
#include "../model/app_state.h"

static lv_obj_t *main_screen;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_program;
static lv_obj_t *btn_start;
static lv_obj_t *lbl_btn_start;
static lv_obj_t *btn_config;

static void btn_config_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_switch_to_program_config_screen();
    }
}

static void btn_start_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (appState.status.currentState == KilnState::IDLE) {
            appState.status.currentState = KilnState::RAMPING;
        } else {
            appState.status.currentState = KilnState::IDLE;
        }
        ui_main_screen_update();
    }
}

void ui_main_screen_create() {
    main_screen = lv_obj_create(NULL);
    
    // Status Label
    lbl_status = lv_label_create(main_screen);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(lbl_status, "Status: IDLE");

    // Program Name Label
    lbl_program = lv_label_create(main_screen);
    lv_obj_align_to(lbl_program, lbl_status, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_label_set_text(lbl_program, "Program: None");

    // Big Temperature Display
    lbl_temp = lv_label_create(main_screen);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, 0); // Assuming this font is compiled in, else defaults to what's available
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -10);
    lv_label_set_text(lbl_temp, "25.0 C");

    // Start/Stop Button
    btn_start = lv_btn_create(main_screen);
    lv_obj_set_size(btn_start, 120, 50);
    lv_obj_align(btn_start, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(btn_start, btn_start_event_cb, LV_EVENT_ALL, NULL);
    lbl_btn_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_btn_start, "START");
    lv_obj_center(lbl_btn_start);

    // Config/Select Program Button
    btn_config = lv_btn_create(main_screen);
    lv_obj_set_size(btn_config, 120, 50);
    lv_obj_align(btn_config, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(btn_config, btn_config_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_btn_config = lv_label_create(btn_config);
    lv_label_set_text(lbl_btn_config, "Select Program");
    lv_obj_center(lbl_btn_config);

    lv_scr_load(main_screen);
    ui_main_screen_update();
}

lv_obj_t* ui_main_screen_get() {
    return main_screen;
}

void ui_main_screen_update() {
    if (!main_screen) return;

    // Update Temp
    lv_label_set_text_fmt(lbl_temp, "%.1f C", appState.status.currentTemperature);
    lv_label_set_text_fmt(lbl_program, "Program: %s", appState.status.activeProgramName);

    // Update state dependent UI
    if (appState.status.currentState == KilnState::IDLE) {
        lv_label_set_text(lbl_status, "Status: IDLE");
        lv_label_set_text(lbl_btn_start, "START");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_clear_flag(btn_config, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(lbl_status, "Status: RUNNING");
        lv_label_set_text(lbl_btn_start, "STOP");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_add_flag(btn_config, LV_OBJ_FLAG_HIDDEN);
    }
}

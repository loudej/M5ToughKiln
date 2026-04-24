#include "../lvgl_includes.h"
#include "ui_main_screen.h"
#include "ui.h"
#include "../model/app_state.h"

static lv_obj_t *main_screen;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_program;
static lv_obj_t *lbl_target_temp;
static lv_obj_t *lbl_elapsed_time;
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
    
    lv_obj_t *screen_layout = lv_obj_create(main_screen);
    lv_obj_set_size(screen_layout, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(screen_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen_layout, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(screen_layout, 0, 0);
    lv_obj_set_style_pad_all(screen_layout, 0, 0);
    lv_obj_set_style_bg_opa(screen_layout, 0, 0);

    lv_obj_t *cont_fill = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_fill, lv_pct(100));
    lv_obj_set_flex_grow(cont_fill, 1);
    lv_obj_set_style_border_width(cont_fill, 0, 0);
    lv_obj_set_style_pad_all(cont_fill, 10, 0);
    lv_obj_set_style_bg_opa(cont_fill, 0, 0);

    lv_obj_t *cont_bottom = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_bottom, lv_pct(100));
    lv_obj_set_height(cont_bottom, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(cont_bottom, 0, 0);
    lv_obj_set_style_pad_all(cont_bottom, 10, 0);
    lv_obj_set_style_bg_opa(cont_bottom, 0, 0);
    lv_obj_set_layout(cont_bottom, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_bottom, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static const lv_coord_t col_dsc[] = {120, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {
        LV_GRID_CONTENT, // Program
        LV_GRID_CONTENT, // Status
        LV_GRID_CONTENT, // Temp
        LV_GRID_CONTENT, // Target
        LV_GRID_CONTENT, // Time
        LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_layout(cont_fill, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(cont_fill, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(cont_fill, 10, 0);
    lv_obj_set_style_pad_column(cont_fill, 10, 0);

    // Row 0: Program
    lv_obj_t *lbl_hdr_program = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_program, "Program");
    lv_obj_set_grid_cell(lbl_hdr_program, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    btn_config = lv_btn_create(cont_fill);
    lv_obj_set_width(btn_config, lv_pct(100));
    lv_obj_add_event_cb(btn_config, btn_config_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(btn_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    
    lbl_program = lv_label_create(btn_config);
    lv_label_set_text(lbl_program, "None");
    lv_obj_center(lbl_program);

    // Row 1: Status
    lv_obj_t *lbl_hdr_status = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_status, "Status");
    lv_obj_set_grid_cell(lbl_hdr_status, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    lbl_status = lv_label_create(cont_fill);
    lv_label_set_text(lbl_status, "IDLE");
    lv_obj_set_grid_cell(lbl_status, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    // Row 2: Temperature
    lv_obj_t *lbl_hdr_temp = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_temp, "Temperature");
    lv_obj_set_grid_cell(lbl_hdr_temp, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    lbl_temp = lv_label_create(cont_fill);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, 0); 
    lv_label_set_text(lbl_temp, "25.0 C");
    lv_obj_set_grid_cell(lbl_temp, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    // Row 3: Target
    lv_obj_t *lbl_hdr_target = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_target, "Target");
    lv_obj_set_grid_cell(lbl_hdr_target, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    lbl_target_temp = lv_label_create(cont_fill);
    lv_label_set_text(lbl_target_temp, "-- C");
    lv_obj_set_grid_cell(lbl_target_temp, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    // Row 4: Time
    lv_obj_t *lbl_hdr_time = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_time, "Time");
    lv_obj_set_grid_cell(lbl_hdr_time, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    lbl_elapsed_time = lv_label_create(cont_fill);
    lv_label_set_text(lbl_elapsed_time, "--:--");
    lv_obj_set_grid_cell(lbl_elapsed_time, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    // Start/Stop Button
    btn_start = lv_btn_create(cont_bottom);
    lv_obj_set_size(btn_start, 200, 50);
    lv_obj_add_event_cb(btn_start, btn_start_event_cb, LV_EVENT_ALL, NULL);
    lbl_btn_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_btn_start, "START");
    lv_obj_center(lbl_btn_start);

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
    lv_label_set_text_fmt(lbl_program, "%s", appState.status.activeProgramName.c_str());

    // Update state dependent UI
    if (appState.status.currentState == KilnState::IDLE) {
        lv_label_set_text(lbl_status, "IDLE");
        lv_label_set_text(lbl_target_temp, "-- C");
        lv_label_set_text(lbl_elapsed_time, "--:--");
        lv_label_set_text(lbl_btn_start, "START");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_clear_state(btn_config, LV_STATE_DISABLED);
    } else if (appState.status.currentState == KilnState::ERROR) {
        lv_label_set_text(lbl_status, "ERROR!");
        lv_label_set_text(lbl_target_temp, "-- C");
        lv_label_set_text(lbl_elapsed_time, "--:--");
        lv_label_set_text(lbl_btn_start, "RESET");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_clear_state(btn_config, LV_STATE_DISABLED);
    } else {
        // Running states (Ramping or Soaking)
        if (appState.status.currentState == KilnState::RAMPING) {
            lv_label_set_text(lbl_status, "RAMPING");
        } else if (appState.status.currentState == KilnState::SOAKING) {
            lv_label_set_text(lbl_status, "SOAKING");
        } else if (appState.status.currentState == KilnState::COOLING) {
            lv_label_set_text(lbl_status, "COOLING");
        }

        lv_label_set_text_fmt(lbl_target_temp, "%.1f C", appState.status.targetTemperature);
        lv_label_set_text_fmt(lbl_elapsed_time, "%02lu:%02lu", appState.status.totalTimeElapsed / 60, appState.status.totalTimeElapsed % 60);

        lv_label_set_text(lbl_btn_start, "STOP");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_add_state(btn_config, LV_STATE_DISABLED);
    }
}

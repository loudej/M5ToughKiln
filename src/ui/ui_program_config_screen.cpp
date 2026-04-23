#include "../lvgl_includes.h"
#include "ui_program_config_screen.h"
#include "ui.h"
#include "ui_keypad.h"
#include "../model/app_state.h"
#include "../model/profile_generator.h"

static lv_obj_t *program_config_screen;

static lv_obj_t *dropdown_program;
static lv_obj_t *btn_back;
static lv_obj_t *btn_save;

// Containers for dynamic content
static lv_obj_t *cont_predefined;
static lv_obj_t *cont_custom;

// Predefined configuration inputs
static lv_obj_t *ta_cone;
static lv_obj_t *ta_candle;
static lv_obj_t *ta_soak;

static void update_dynamic_content() {
    uint16_t selected_idx = lv_dropdown_get_selected(dropdown_program);
    
    // Options 0-3 are the predefined programs
    if (selected_idx <= 3) {
        lv_obj_clear_flag(cont_predefined, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cont_custom, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Otherwise, it's a custom program
        lv_obj_add_flag(cont_predefined, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cont_custom, LV_OBJ_FLAG_HIDDEN);
    }
}

static void dropdown_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        update_dynamic_content();
    }
}

static void btn_back_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_switch_to_main_screen();
    }
}

static void btn_save_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        uint16_t selected_idx = lv_dropdown_get_selected(dropdown_program);
        
        // If predefined program selected, update active program from inputs
        if (selected_idx <= 3) {
            const char* coneStr = lv_textarea_get_text(ta_cone);
            float candle = atof(lv_textarea_get_text(ta_candle));
            float soak = atof(lv_textarea_get_text(ta_soak));
            
            // Re-generate the program based on current text inputs
            if (selected_idx == 0) appState.predefinedPrograms[0] = ProfileGenerator::generateFastBisque(coneStr, candle, soak);
            else if (selected_idx == 1) appState.predefinedPrograms[1] = ProfileGenerator::generateSlowBisque(coneStr, candle, soak);
            else if (selected_idx == 2) appState.predefinedPrograms[2] = ProfileGenerator::generateFastGlaze(coneStr, candle, soak);
            else if (selected_idx == 3) appState.predefinedPrograms[3] = ProfileGenerator::generateSlowGlaze(coneStr, candle, soak);
        }
        
        appState.activeProgramIndex = selected_idx;
        if (selected_idx <= 3) {
            appState.status.activeProgramName = appState.predefinedPrograms[selected_idx].name.c_str();
        } else {
            // Handle Custom program selection...
        }

        ui_switch_to_main_screen();
    }
}

static void build_predefined_container() {
    cont_predefined = lv_obj_create(program_config_screen);
    lv_obj_set_size(cont_predefined, M5.Display.width() - 20, 120);
    lv_obj_align_to(cont_predefined, dropdown_program, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_set_style_border_width(cont_predefined, 0, 0); // No border for cleaner look

    // Cone Temp
    lv_obj_t *lbl_cone = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_cone, "Cone:");
    lv_obj_align(lbl_cone, LV_ALIGN_TOP_LEFT, 0, 10);
    
    ta_cone = lv_textarea_create(cont_predefined);
    lv_obj_set_size(ta_cone, 60, 35);
    lv_textarea_set_one_line(ta_cone, true);
    lv_textarea_set_text(ta_cone, "6"); // Default
    lv_obj_align_to(ta_cone, lbl_cone, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_event_cb(ta_cone, ui_ta_event_cb, LV_EVENT_ALL, NULL);

    // Candle Time
    lv_obj_t *lbl_candle = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_candle, "Candle (hr):");
    lv_obj_align_to(lbl_candle, lbl_cone, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    
    ta_candle = lv_textarea_create(cont_predefined);
    lv_obj_set_size(ta_candle, 60, 35);
    lv_textarea_set_one_line(ta_candle, true);
    lv_textarea_set_text(ta_candle, "0");
    lv_obj_align_to(ta_candle, lbl_candle, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_event_cb(ta_candle, ui_ta_event_cb, LV_EVENT_ALL, NULL);

    // Soak Time
    lv_obj_t *lbl_soak = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_soak, "Soak (min):");
    lv_obj_align_to(lbl_soak, lbl_candle, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    
    ta_soak = lv_textarea_create(cont_predefined);
    lv_obj_set_size(ta_soak, 60, 35);
    lv_textarea_set_one_line(ta_soak, true);
    lv_textarea_set_text(ta_soak, "15");
    lv_obj_align_to(ta_soak, lbl_soak, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_event_cb(ta_soak, ui_ta_event_cb, LV_EVENT_ALL, NULL);
}

static void build_custom_container() {
    cont_custom = lv_obj_create(program_config_screen);
    lv_obj_set_size(cont_custom, M5.Display.width() - 20, 120);
    lv_obj_align_to(cont_custom, dropdown_program, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_add_flag(cont_custom, LV_OBJ_FLAG_HIDDEN); // Hidden by default

    lv_obj_t *lbl_placeholder = lv_label_create(cont_custom);
    lv_label_set_text(lbl_placeholder, "Custom Segment List goes here...");
    lv_obj_center(lbl_placeholder);
}

void ui_program_config_screen_create() {
    if (program_config_screen) {
        return;
    }

    program_config_screen = lv_obj_create(NULL);
    
    // Top dropdown
    dropdown_program = lv_dropdown_create(program_config_screen);
    lv_dropdown_set_options(dropdown_program, "Fast Bisque\nSlow Bisque\nFast Glaze\nSlow Glaze\nAdd New Custom");
    lv_obj_set_width(dropdown_program, 200);
    lv_obj_align(dropdown_program, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_event_cb(dropdown_program, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Build the dynamic containers
    build_predefined_container();
    build_custom_container();

    // Bottom Navigation Buttons
    btn_back = lv_btn_create(program_config_screen);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);

    btn_save = lv_btn_create(program_config_screen);
    lv_obj_set_size(btn_save, 100, 40);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(btn_save, btn_save_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save / OK");
    lv_obj_center(lbl_save);
    
    // Trigger initial state
    update_dynamic_content();
}

lv_obj_t* ui_program_config_screen_get() {
    return program_config_screen;
}

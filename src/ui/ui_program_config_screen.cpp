#include "../lvgl_includes.h"
#include "ui_program_config_screen.h"
#include "ui.h"
#include "ui_keypad.h"
#include "ui_edit_segment_popup.h"
#include "../model/app_state.h"
#include "../model/profile_generator.h"
#include "../service/preferences_persistence.h"

extern PreferencesPersistence persistence;

static lv_obj_t *program_config_screen;
static lv_obj_t *screen_layout;

static lv_obj_t *dropdown_program;
static lv_obj_t *btn_back;
static lv_obj_t *btn_save;

static lv_obj_t *cont_top;
static lv_obj_t *cont_fill;

// Containers for dynamic content
static lv_obj_t *cont_predefined;
static lv_obj_t *cont_custom;
static lv_obj_t *list_segments;

// Predefined configuration inputs
static lv_obj_t *ta_cone;
static lv_obj_t *ta_candle;
static lv_obj_t *ta_soak;

// Keep track of the currently selected custom program (-1 if predefined or "Add New")
static int current_custom_idx = -1;

static void update_custom_segment_list();
static void build_dropdown_options();

static void build_dropdown_options() {
    std::string options = "Fast Bisque\nSlow Bisque\nFast Glaze\nSlow Glaze";
    for (size_t i = 0; i < appState.customPrograms.size(); ++i) {
        options += "\n" + appState.customPrograms[i].name;
    }
    options += "\nAdd New Custom";
    lv_dropdown_set_options(dropdown_program, options.c_str());
}

static void update_dynamic_content() {
    uint16_t selected_idx = lv_dropdown_get_selected(dropdown_program);
    
    // Options 0-3 are the predefined programs
    if (selected_idx <= 3) {
        current_custom_idx = -1;
        lv_obj_clear_flag(cont_predefined, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cont_custom, LV_OBJ_FLAG_HIDDEN);

        // Pre-fill from memory, or default if empty
        const auto& preProg = appState.predefinedPrograms[selected_idx];
        
        if (preProg.origCone.empty()) {
            lv_textarea_set_text(ta_cone, selected_idx <= 1 ? "08" : "5");
            lv_textarea_set_text(ta_candle, "");
            lv_textarea_set_text(ta_soak, "");
        } else {
            lv_textarea_set_text(ta_cone, preProg.origCone.c_str());
            lv_textarea_set_text(ta_candle, preProg.origCandle == 0 ? "" : std::to_string(preProg.origCandle).c_str());
            lv_textarea_set_text(ta_soak, preProg.origSoak == 0 ? "" : std::to_string(preProg.origSoak).c_str());
        }
    } else {
        // Otherwise, it's a custom program or "Add New"
        lv_obj_add_flag(cont_predefined, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cont_custom, LV_OBJ_FLAG_HIDDEN);

        
        if (selected_idx == 4 + appState.customPrograms.size()) {
            // "Add New Custom" selected
            current_custom_idx = appState.customPrograms.size(); // It will be the next index
            FiringProgram newProg;
            newProg.name = "Custom " + std::to_string(current_custom_idx + 1);
            newProg.isCustom = true;
            appState.customPrograms.push_back(newProg);
            
            // Rebuild options and select the newly added program
            build_dropdown_options();
            lv_dropdown_set_selected(dropdown_program, 4 + current_custom_idx);
        } else {
            current_custom_idx = selected_idx - 4;
        }
        
        update_custom_segment_list();
    }
}

static void dropdown_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        update_dynamic_content();
    }
}

static void btn_back_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Discard any volatile changes by reloading from NVM
        persistence.loadCustomPrograms(appState.customPrograms);
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
            appState.status.activeProgramName = appState.predefinedPrograms[selected_idx].name;
        } else {
            // Handle Custom program selection...
            if (current_custom_idx >= 0 && current_custom_idx < appState.customPrograms.size()) {
                 appState.status.activeProgramName = appState.customPrograms[current_custom_idx].name;
            }
        }
        
        persistence.saveCustomPrograms(appState.customPrograms);

        ui_switch_to_main_screen();
    }
}

static void build_predefined_container() {
    cont_predefined = lv_obj_create(cont_fill);
    lv_obj_set_size(cont_predefined, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(cont_predefined, 0, 0); // No border for cleaner look
    lv_obj_set_style_bg_opa(cont_predefined, 0, 0);
    lv_obj_set_style_pad_all(cont_predefined, 0, 0);

    // Cone Temp
    ta_cone = lv_textarea_create(cont_predefined);
    lv_obj_set_size(ta_cone, 60, 35);
    lv_textarea_set_one_line(ta_cone, true);
    lv_obj_align(ta_cone, LV_ALIGN_TOP_LEFT, 15, 5);
    lv_obj_add_event_cb(ta_cone, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_t *lbl_cone = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_cone, "Cone");
    lv_obj_align_to(lbl_cone, ta_cone, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Candle Time
    ta_candle = lv_textarea_create(cont_predefined);
    lv_obj_set_size(ta_candle, 60, 35);
    lv_textarea_set_one_line(ta_candle, true);
    lv_textarea_set_placeholder_text(ta_candle, "min");
    lv_obj_align_to(ta_candle, ta_cone, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_add_event_cb(ta_candle, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_t *lbl_candle = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_candle, "Candle");
    lv_obj_align_to(lbl_candle, ta_candle, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Soak Time
    ta_soak = lv_textarea_create(cont_predefined);
    lv_obj_set_size(ta_soak, 60, 35);
    lv_textarea_set_one_line(ta_soak, true);
    lv_textarea_set_placeholder_text(ta_soak, "min");
    lv_obj_align_to(ta_soak, ta_candle, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_add_event_cb(ta_soak, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_t *lbl_soak = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_soak, "Soak");
    lv_obj_align_to(lbl_soak, ta_soak, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
}

static void btn_add_segment_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (current_custom_idx >= 0) {
            // -1 signals a new segment, don't push it yet
            ui_edit_segment_popup_create(program_config_screen, current_custom_idx, -1, update_custom_segment_list);
        }
    }
}

static void btn_edit_segment_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int seg_idx = (int)(intptr_t)lv_event_get_user_data(e);
        if (current_custom_idx >= 0 && seg_idx >= 0) {
            ui_edit_segment_popup_create(program_config_screen, current_custom_idx, seg_idx, update_custom_segment_list);
        }
    }
}

static void btn_delete_segment_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        int seg_idx = (int)(intptr_t)lv_event_get_user_data(e);
        if (current_custom_idx >= 0 && seg_idx >= 0) {
            auto& segs = appState.customPrograms[current_custom_idx].segments;
            if (seg_idx < segs.size()) {
                segs.erase(segs.begin() + seg_idx);
                update_custom_segment_list();
            }
        }
    }
}

static void update_custom_segment_list() {
    if (current_custom_idx < 0 || current_custom_idx >= appState.customPrograms.size()) return;
    
    lv_obj_clean(list_segments); // Clear existing list items

    const auto& prog = appState.customPrograms[current_custom_idx];
    
    for (size_t i = 0; i < prog.segments.size(); ++i) {
        const auto& seg = prog.segments[i];
        
        // Make the entire row a clickable button
        lv_obj_t * btn_edit = lv_btn_create(list_segments);
        lv_obj_set_style_bg_color(btn_edit, lv_color_hex(0x282b30), 0);
        auto theme = lv_theme_default_get();
        lv_obj_set_width(btn_edit, lv_pct(100));
        lv_obj_set_height(btn_edit, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(btn_edit, btn_edit_segment_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        lv_obj_t * lbl_summary = lv_label_create(btn_edit);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0fC/h to %.0fC hold %dm", seg.rampRate, seg.targetTemperature, seg.soakTime);
        lv_label_set_text(lbl_summary, buf);
        lv_obj_center(lbl_summary);
    }

    // Add Segment Button at the bottom
    lv_obj_t * btn_add = lv_btn_create(list_segments);
    lv_obj_set_size(btn_add, lv_pct(100), 40);
    lv_obj_add_event_cb(btn_add, btn_add_segment_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_add = lv_label_create(btn_add);
    lv_label_set_text(lbl_add, "Add Segment");
    lv_obj_center(lbl_add);
}

static void build_custom_container() {
    cont_custom = lv_obj_create(cont_fill);
    lv_obj_set_size(cont_custom, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(cont_custom, LV_OBJ_FLAG_HIDDEN); // Hidden by default
    lv_obj_set_layout(cont_custom, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_custom, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(cont_custom, 0, 0);
    lv_obj_set_style_border_width(cont_custom, 0, 0);

    // List container first
    list_segments = lv_obj_create(cont_custom);
    lv_obj_set_size(list_segments, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(list_segments, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_segments, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_segments, 0, 0);
    lv_obj_set_style_bg_opa(list_segments, 0, 0);
    lv_obj_set_style_border_width(list_segments, 0, 0);
}

static void screen_delete_cb(lv_event_t * e) {
    program_config_screen = NULL;
}

void ui_program_config_screen_create() {
    program_config_screen = lv_obj_create(NULL);
    lv_obj_add_event_cb(program_config_screen, screen_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_pad_all(program_config_screen, 0, 0);

    screen_layout = lv_obj_create(program_config_screen);
    lv_obj_set_size(screen_layout, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(screen_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen_layout, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(screen_layout, 0, 0);
    lv_obj_set_style_pad_all(screen_layout, 0, 0);
    lv_obj_set_style_bg_opa(screen_layout, 0, 0);

    cont_top = lv_obj_create(screen_layout);
    lv_obj_set_size(cont_top, lv_pct(100), 50);
    lv_obj_set_layout(cont_top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(cont_top, 5, 0);
    lv_obj_set_style_pad_column(cont_top, 10, 0);
    lv_obj_set_style_border_width(cont_top, 0, 0);
    lv_obj_set_style_bg_opa(cont_top, 0, 0);

    cont_fill = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_fill, lv_pct(100));
    lv_obj_set_flex_grow(cont_fill, 1);
    lv_obj_set_style_border_width(cont_fill, 0, 0);
    lv_obj_set_style_pad_all(cont_fill, 0, 0);
    lv_obj_set_style_bg_opa(cont_fill, 0, 0);
    
    // Back Button (X) on left
    btn_back = lv_button_create(cont_top);
    lv_obj_set_size(btn_back, 40, 40);
    // lv_obj_set_style_bg_color(btn_back, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_back);

    // Top dropdown in middle (fills space)
    dropdown_program = lv_dropdown_create(cont_top);
    lv_obj_set_height(dropdown_program, 40);
    lv_obj_set_flex_grow(dropdown_program, 1);
    lv_obj_add_event_cb(dropdown_program, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Save Button (Checkmark) on right
    btn_save = lv_button_create(cont_top);
    lv_obj_set_size(btn_save, 40, 40);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(btn_save, btn_save_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_OK);
    lv_obj_center(lbl_save);

    // Build the dynamic containers
    build_predefined_container();
    build_custom_container();

    // Populate dropdown and select active
    build_dropdown_options();
    lv_dropdown_set_selected(dropdown_program, appState.activeProgramIndex);
    
    // Trigger initial state
    update_dynamic_content();
}

lv_obj_t* ui_program_config_screen_get() {
    return program_config_screen;
}

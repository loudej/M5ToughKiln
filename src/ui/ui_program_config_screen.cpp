#include "../lvgl_includes.h"
#include "ui_program_config_screen.h"
#include "ui.h"
#include "ui_keypad.h"
#include "ui_edit_segment_popup.h"
#include "ui_sizes.h"
#include "../model/app_state.h"
#include "../model/temp_units.h"
#include "../model/profile_generator.h"

static constexpr lv_coord_t PAD_TOP_BAR    = 5;
static constexpr lv_coord_t TOP_BAR_ITEM_H = 40;

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
    for (const auto& name : appState.getCustomProgramNames()) {
        options += "\n" + name;
    }
    options += "\nAdd New Custom";
    lv_dropdown_set_options(dropdown_program, options.c_str());
}

static void update_dynamic_content() {
    uint16_t selected_idx = lv_dropdown_get_selected(dropdown_program);

    if (selected_idx <= 3) {
        current_custom_idx = -1;
        lv_obj_clear_flag(cont_predefined, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cont_custom, LV_OBJ_FLAG_HIDDEN);

        FiringProgram preProg{};
        if (!appState.tryCopyPredefinedProgram(selected_idx, &preProg))
            return;

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
        lv_obj_add_flag(cont_predefined, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cont_custom, LV_OBJ_FLAG_HIDDEN);

        const size_t customCount = appState.getCustomProgramCount();

        if (selected_idx == 4 + customCount) {
            current_custom_idx = static_cast<int>(customCount);
            FiringProgram newProg;
            newProg.name = "Custom " + std::to_string(current_custom_idx + 1);
            newProg.isCustom = true;
            appState.appendCustomProgram(std::move(newProg));

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

static void save_current_program() {
    uint16_t selected_idx = lv_dropdown_get_selected(dropdown_program);

    ProgramSelectionCommitDraft d;
    d.activeIndex = static_cast<int>(selected_idx);

    if (selected_idx <= 3) {
        const char* coneStr = lv_textarea_get_text(ta_cone);
        d.cone   = coneStr ? coneStr : "";
        d.candle = static_cast<int>(atof(lv_textarea_get_text(ta_candle)));
        d.soak   = static_cast<int>(atof(lv_textarea_get_text(ta_soak)));
    }

    appState.commitProgramSelection(d);
}

static void btn_back_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        save_current_program();
        ui_switch_to_main_screen();
    }
}

static void btn_start_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        save_current_program();
        appState.setKilnStateFromProgramConfigStart();
        ui_switch_to_main_screen();
    }
}

static void build_predefined_container() {
    cont_predefined = lv_obj_create(cont_fill);
    lv_obj_set_size(cont_predefined, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(cont_predefined, 0, 0);
    lv_obj_set_style_bg_opa(cont_predefined, 0, 0);
    lv_obj_set_style_pad_all(cont_predefined, UI_PAD_STD, 0);

    static const lv_coord_t col_dsc[] = {UI_GRID_INPUT_W, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_set_layout(cont_predefined, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(cont_predefined, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(cont_predefined, UI_PAD_STD, 0);
    lv_obj_set_style_pad_column(cont_predefined, UI_PAD_STD, 0);

    // Cone Temp
    ta_cone = lv_textarea_create(cont_predefined);
    lv_textarea_set_one_line(ta_cone, true);
    lv_obj_set_height(ta_cone, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(ta_cone, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ta_cone, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    
    lv_obj_t *lbl_cone = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_cone, "Cone");
    lv_obj_set_grid_cell(lbl_cone, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // Candle Time
    ta_candle = lv_textarea_create(cont_predefined);
    lv_textarea_set_one_line(ta_candle, true);
    lv_obj_set_height(ta_candle, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ta_candle, "min");
    lv_obj_add_event_cb(ta_candle, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ta_candle, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 1, 1);
    
    lv_obj_t *lbl_candle = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_candle, "Candle");
    lv_obj_set_grid_cell(lbl_candle, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    // Soak Time
    ta_soak = lv_textarea_create(cont_predefined);
    lv_textarea_set_one_line(ta_soak, true);
    lv_obj_set_height(ta_soak, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ta_soak, "min");
    lv_obj_add_event_cb(ta_soak, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ta_soak, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 2, 1);
    
    lv_obj_t *lbl_soak = lv_label_create(cont_predefined);
    lv_label_set_text(lbl_soak, "Soak");
    lv_obj_set_grid_cell(lbl_soak, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
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
            appState.eraseCustomSegment(static_cast<size_t>(current_custom_idx), static_cast<size_t>(seg_idx));
            update_custom_segment_list();
        }
    }
}

static void update_custom_segment_list() {
    if (current_custom_idx < 0 || static_cast<size_t>(current_custom_idx) >= appState.getCustomProgramCount())
        return;

    lv_obj_clean(list_segments);

    FiringProgram prog{};
    if (!appState.tryCopyCustomProgram(static_cast<size_t>(current_custom_idx), &prog))
        return;

    const TempUnit tu = appState.getTempUnit();

    for (size_t i = 0; i < prog.segments.size(); ++i) {
        const auto& seg = prog.segments[i];

        lv_obj_t * btn_edit = lv_btn_create(list_segments);
        lv_obj_set_style_bg_color(btn_edit, lv_color_hex(0x282b30), 0);
        lv_obj_set_width(btn_edit, lv_pct(100));
        lv_obj_set_height(btn_edit, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(btn_edit, btn_edit_segment_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t * lbl_summary = lv_label_create(btn_edit);
        char buf[64];
        const char* unit = unitSymbol(tu);
        float dispTarget = toDisplayTemp(seg.targetTemperature, tu);
        float dispRate   = toDisplayRate(seg.rampRate, tu);
        if (seg.soakTime > 0) {
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0%s  %.0f\xc2\xb0%s/h  %dm",
                     dispTarget, unit, dispRate, unit, seg.soakTime);
        } else {
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0%s  %.0f\xc2\xb0%s/h",
                     dispTarget, unit, dispRate, unit);
        }
        lv_label_set_text(lbl_summary, buf);
        lv_obj_center(lbl_summary);
    }

    lv_obj_t * btn_add = lv_btn_create(list_segments);
    lv_obj_set_size(btn_add, lv_pct(100), LV_SIZE_CONTENT);
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
    lv_obj_set_style_pad_all(cont_custom, 0, 0);

    // List container first
    list_segments = lv_obj_create(cont_custom);
    lv_obj_set_size(list_segments, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(list_segments, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_segments, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_segments, UI_PAD_STD, 0);
    lv_obj_set_style_pad_row(list_segments, UI_PAD_STD, 0);
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
    lv_obj_set_size(cont_top, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(cont_top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(cont_top, PAD_TOP_BAR, 0);
    lv_obj_set_style_pad_column(cont_top, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(cont_top, 0, 0);
    lv_obj_set_style_bg_opa(cont_top, 0, 0);

    cont_fill = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_fill, lv_pct(100));
    lv_obj_set_flex_grow(cont_fill, 1);
    lv_obj_set_style_border_width(cont_fill, 0, 0);
    lv_obj_set_style_pad_all(cont_fill, 0, 0);
    lv_obj_set_style_bg_opa(cont_fill, 0, 0);
    
    // Back Button (<-) on left
    btn_back = lv_btn_create(cont_top);
    lv_obj_set_size(btn_back, LV_SIZE_CONTENT, TOP_BAR_ITEM_H);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);

    // Top dropdown in middle (fills space)
    dropdown_program = lv_dropdown_create(cont_top);
    lv_obj_set_height(dropdown_program, TOP_BAR_ITEM_H);
    lv_obj_set_flex_grow(dropdown_program, 1);
    lv_obj_add_event_cb(dropdown_program, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Start Button on right
    btn_save = lv_btn_create(cont_top);
    lv_obj_set_size(btn_save, LV_SIZE_CONTENT, TOP_BAR_ITEM_H);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(btn_save, btn_start_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "START");
    lv_obj_center(lbl_save);

    // Build the dynamic containers
    build_predefined_container();
    build_custom_container();

    // Populate dropdown and select active
    build_dropdown_options();
    lv_dropdown_set_selected(dropdown_program, static_cast<uint16_t>(appState.getActiveProgramIndex()));
    
    // Trigger initial state
    update_dynamic_content();
}

lv_obj_t* ui_program_config_screen_get() {
    return program_config_screen;
}

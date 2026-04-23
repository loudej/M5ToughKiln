#include "ui_program_config_screen.h"
#include "ui.h"

static lv_obj_t *program_config_screen;
static lv_obj_t *dropdown_program;
static lv_obj_t *btn_back;

static void btn_back_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_switch_to_main_screen();
    }
}

void ui_program_config_screen_create() {
    if (program_config_screen) {
        // already created
        return;
    }

    program_config_screen = lv_obj_create(NULL);
    
    // Top dropdown
    dropdown_program = lv_dropdown_create(program_config_screen);
    lv_dropdown_set_options(dropdown_program, "Bisque Fire\nGlaze Fire\nCustom 1\nAdd New Custom");
    lv_obj_set_width(dropdown_program, 200);
    lv_obj_align(dropdown_program, LV_ALIGN_TOP_MID, 0, 10);

    // Back button
    btn_back = lv_btn_create(program_config_screen);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);

    // TODO: Dynamic content area population based on dropdown selection
}

lv_obj_t* ui_program_config_screen_get() {
    return program_config_screen;
}

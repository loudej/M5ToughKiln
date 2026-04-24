#include "../lvgl_includes.h"
#include "ui.h"
#include "ui_main_screen.h"
#include "ui_program_config_screen.h"
#include "ui_keypad.h"

void ui_init() {
    // Initialize the global keypad first
    ui_keypad_init();

    // Create the main screen initially
    ui_main_screen_create();
}

void ui_switch_to_main_screen() {
    // Logic to switch screens
    lv_scr_load(ui_main_screen_get());
    if (ui_program_config_screen_get() != NULL) {
        lv_obj_del(ui_program_config_screen_get());
    }
}

void ui_switch_to_program_config_screen() {
    // Logic to switch screens
    ui_program_config_screen_create();
    lv_scr_load(ui_program_config_screen_get());
}

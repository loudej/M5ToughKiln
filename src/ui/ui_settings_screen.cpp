#include "../lvgl_includes.h"
#include "ui_settings_screen.h"
#include "ui.h"
#include "ui_sizes.h"
#include "../model/app_state.h"
#include "../model/temp_units.h"
#include "../service/preferences_persistence.h"

extern PreferencesPersistence persistence;

static constexpr lv_coord_t TOP_BAR_ITEM_H = 40;

static lv_obj_t *settings_screen;
static lv_obj_t *btn_unit_f;
static lv_obj_t *btn_unit_c;

static void refresh_unit_buttons() {
    bool isFahrenheit = appState.tempUnit == TempUnit::FAHRENHEIT;
    if (isFahrenheit) {
        lv_obj_add_state(btn_unit_f, LV_STATE_CHECKED);
        lv_obj_clear_state(btn_unit_c, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(btn_unit_f, LV_STATE_CHECKED);
        lv_obj_add_state(btn_unit_c, LV_STATE_CHECKED);
    }
}

static void on_unit_f(lv_event_t *e) {
    appState.tempUnit = TempUnit::FAHRENHEIT;
    refresh_unit_buttons();
}

static void on_unit_c(lv_event_t *e) {
    appState.tempUnit = TempUnit::CELSIUS;
    refresh_unit_buttons();
}

static void on_back(lv_event_t *e) {
    persistence.saveSettings();
    ui_switch_to_main_screen();
}

static void screen_delete_cb(lv_event_t *e) {
    settings_screen = NULL;
}

void ui_settings_screen_create() {
    settings_screen = lv_obj_create(NULL);
    lv_obj_add_event_cb(settings_screen, screen_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_pad_all(settings_screen, 0, 0);

    lv_obj_t *screen_layout = lv_obj_create(settings_screen);
    lv_obj_set_size(screen_layout, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(screen_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen_layout, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(screen_layout, 0, 0);
    lv_obj_set_style_pad_all(screen_layout, 0, 0);
    lv_obj_set_style_bg_opa(screen_layout, 0, 0);

    // --- Top bar ---
    lv_obj_t *cont_top = lv_obj_create(screen_layout);
    lv_obj_set_size(cont_top, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(cont_top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont_top, 5, 0);
    lv_obj_set_style_pad_column(cont_top, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(cont_top, 0, 0);
    lv_obj_set_style_bg_opa(cont_top, 0, 0);

    lv_obj_t *btn_back = lv_btn_create(cont_top);
    lv_obj_set_size(btn_back, LV_SIZE_CONTENT, TOP_BAR_ITEM_H);
    lv_obj_add_event_cb(btn_back, on_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);

    lv_obj_t *lbl_title = lv_label_create(cont_top);
    lv_obj_set_flex_grow(lbl_title, 1);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_title, "Settings");

    // --- Scrollable fill area ---
    lv_obj_t *cont_fill = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_fill, lv_pct(100));
    lv_obj_set_flex_grow(cont_fill, 1);
    lv_obj_set_layout(cont_fill, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_fill, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont_fill, UI_PAD_STD, 0);
    lv_obj_set_style_pad_row(cont_fill, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(cont_fill, 0, 0);
    lv_obj_set_style_bg_opa(cont_fill, 0, 0);
    lv_obj_set_style_bg_opa(cont_fill, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(cont_fill, LV_OBJ_FLAG_SCROLLABLE);

    // --- Temperature unit row ---
    lv_obj_t *cont_unit = lv_obj_create(cont_fill);
    lv_obj_set_width(cont_unit, lv_pct(100));
    lv_obj_set_height(cont_unit, LV_SIZE_CONTENT);
    lv_obj_set_layout(cont_unit, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_unit, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_unit, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont_unit, 0, 0);
    lv_obj_set_style_border_width(cont_unit, 0, 0);
    lv_obj_set_style_bg_opa(cont_unit, 0, 0);
    lv_obj_set_style_pad_column(cont_unit, UI_PAD_STD, 0);

    lv_obj_t *lbl_unit = lv_label_create(cont_unit);
    lv_label_set_text(lbl_unit, "Temperature Unit");
    lv_obj_set_flex_grow(lbl_unit, 1);

    btn_unit_f = lv_btn_create(cont_unit);
    lv_obj_add_flag(btn_unit_f, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn_unit_f, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_unit_f, on_unit_f, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_f = lv_label_create(btn_unit_f);
    lv_label_set_text(lbl_f, "\xc2\xb0" "F");
    lv_obj_center(lbl_f);

    btn_unit_c = lv_btn_create(cont_unit);
    lv_obj_add_flag(btn_unit_c, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn_unit_c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_unit_c, on_unit_c, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_c = lv_label_create(btn_unit_c);
    lv_label_set_text(lbl_c, "\xc2\xb0" "C");
    lv_obj_center(lbl_c);

    refresh_unit_buttons();
}

lv_obj_t* ui_settings_screen_get() {
    return settings_screen;
}

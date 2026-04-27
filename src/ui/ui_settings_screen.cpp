#include "../lvgl_includes.h"
#include "ui_settings_screen.h"
#include "ui.h"
#include "ui_sizes.h"
#include "ui_text_keyboard.h"
#include "../model/app_state.h"
#include "../model/temp_units.h"
#include "../service/preferences_persistence.h"
#include "../server/kiln_wifi.h"

#include <cstring>
#include <string>
#include <vector>

extern PreferencesPersistence persistence;

static constexpr lv_coord_t TOP_BAR_ITEM_H        = 40;
static constexpr uint32_t   WIFI_SCAN_PERIOD_MS = 7500;

static lv_obj_t *settings_screen;
static lv_timer_t *s_wifi_scan_timer = nullptr;
static bool        s_wifi_scan_pending  = false;
static bool        s_dropdown_was_open  = false;

static lv_obj_t *btn_unit_f;
static lv_obj_t *btn_unit_c;

static lv_obj_t *lbl_wifi_ip;
static lv_obj_t *lbl_wifi_rssi;
static lv_obj_t *dropdown_wifi;
static lv_obj_t *btn_wifi_passphrase;

/// Modal editor (nullptr when closed)
static lv_obj_t *pass_modal       = nullptr;
static lv_obj_t *pass_modal_ta    = nullptr;

static bool s_dropdown_suppress_evt = false;

static std::string               s_selected_ssid;
static std::string               s_wifi_pass;
static std::vector<KilnWifiNetwork> s_scan_results;

static void refresh_wifi_labels();
static void wifi_rebuild_dropdown_options(bool allow_while_open);
static void wifi_scan_timer_cb(lv_timer_t *t);
static void close_pass_modal(bool apply);

static void pass_modal_ta_event(lv_event_t *e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_FOCUSED)
        ui_text_keyboard_set_target(pass_modal_ta);
    else if (c == LV_EVENT_DEFOCUSED)
        ui_text_keyboard_hide();
}

static bool wifi_dropdown_is_open() {
    return dropdown_wifi != nullptr && lv_dropdown_is_open(dropdown_wifi);
}

static void sanitize_ssid_line(std::string& s) {
    for (char& c : s) {
        if (c == '\n' || c == '\r')
            c = ' ';
    }
}

static void refresh_wifi_labels() {
    if (!lbl_wifi_ip || !lbl_wifi_rssi)
        return;

    lv_label_set_text(lbl_wifi_ip, kiln_wifi_ip_status_line().c_str());

    if (kiln_wifi_station_connected())
        lv_label_set_text_fmt(lbl_wifi_rssi, "%d dBm", kiln_wifi_station_rssi_dbm());
    else
        lv_label_set_text(lbl_wifi_rssi, "-");
}

static void wifi_rebuild_dropdown_options(bool allow_while_open) {
    if (!dropdown_wifi)
        return;
    if (!allow_while_open && wifi_dropdown_is_open())
        return;

    std::string opts;

    auto append_unique = [&](const std::string& raw) {
        std::string s = raw;
        sanitize_ssid_line(s);
        if (s.empty())
            return;

        if (!opts.empty())
            opts += '\n';
        opts += s;
    };

    if (!s_selected_ssid.empty())
        append_unique(s_selected_ssid);

    for (const auto& n : s_scan_results) {
        if (!s_selected_ssid.empty() && n.ssid == s_selected_ssid)
            continue;
        append_unique(n.ssid);
    }

    if (opts.empty()) {
        if (!s_selected_ssid.empty())
            opts = s_selected_ssid;
        else
            opts = "(no networks)";
    }

    s_dropdown_suppress_evt = true;
    lv_dropdown_set_options(dropdown_wifi, opts.c_str());
    lv_dropdown_set_selected(dropdown_wifi, 0);
    s_dropdown_suppress_evt = false;
}

static void wifi_trigger_join_if_configured() {
    if (s_selected_ssid.empty())
        return;
    persistence.saveWifiCredentials(s_selected_ssid, s_wifi_pass);
    kiln_wifi_request_join(s_selected_ssid, s_wifi_pass);
}

static void on_dropdown_wifi(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
        return;
    if (s_dropdown_suppress_evt || !dropdown_wifi)
        return;

    char buf[140];
    lv_dropdown_get_selected_str(dropdown_wifi, buf, sizeof buf);
    std::string choice(buf);

    sanitize_ssid_line(choice);

    if (choice.empty() || choice == "(no networks)")
        return;

    if (choice == s_selected_ssid)
        return;

    s_selected_ssid = choice;
    persistence.saveWifiCredentials(s_selected_ssid, s_wifi_pass);
    kiln_wifi_request_join(s_selected_ssid, s_wifi_pass);
    wifi_rebuild_dropdown_options(false);
}

static void pass_modal_ok_cb(lv_event_t *e) {
    (void)e;
    close_pass_modal(true);
}

static void pass_modal_cancel_cb(lv_event_t *e) {
    (void)e;
    close_pass_modal(false);
}

static void modal_bg_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        pass_modal_cancel_cb(e);
}

static void close_pass_modal(bool apply) {
    /* Keyboard keeps a raw pointer to the textarea; clear it before we delete the modal subtree. */
    ui_text_keyboard_clear_target();
    ui_text_keyboard_hide();

    if (!pass_modal)
        return;

    if (apply && pass_modal_ta) {
        const char *t = lv_textarea_get_text(pass_modal_ta);
        s_wifi_pass = t ? t : "";
        persistence.saveWifiCredentials(s_selected_ssid, s_wifi_pass);
        wifi_trigger_join_if_configured();
    }

    lv_obj_del(pass_modal);
    pass_modal    = nullptr;
    pass_modal_ta = nullptr;
}

static void on_passphrase_btn(lv_event_t *e) {
    (void)e;
    if (pass_modal)
        close_pass_modal(false);

    lv_obj_t *modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_60, 0);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modal, modal_bg_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *panel = lv_obj_create(modal);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, UI_PAD_STD);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, UI_PAD_STD, 0);
    lv_obj_set_style_pad_row(panel, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(panel, 0, 0);

    lv_obj_t *row = lv_obj_create(panel);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    pass_modal_ta = lv_textarea_create(row);
    lv_obj_set_flex_grow(pass_modal_ta, 1);
    lv_textarea_set_one_line(pass_modal_ta, true);
    lv_textarea_set_max_length(pass_modal_ta, 64);
    lv_textarea_set_text(pass_modal_ta, s_wifi_pass.c_str());

    lv_obj_t *btn_ok = lv_btn_create(row);
    lv_obj_add_event_cb(btn_ok, pass_modal_ok_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, LV_SYMBOL_OK);
    lv_obj_center(lbl_ok);

    lv_obj_t *btn_cancel = lv_btn_create(row);
    lv_obj_add_event_cb(btn_cancel, pass_modal_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_cancel);

    pass_modal = modal;

    lv_obj_add_event_cb(pass_modal_ta, pass_modal_ta_event, LV_EVENT_ALL, NULL);

    lv_obj_add_state(pass_modal_ta, LV_STATE_FOCUSED);
    ui_text_keyboard_set_target(pass_modal_ta);
}

static void wifi_scan_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!settings_screen || lv_scr_act() != settings_screen)
        return;
    if (wifi_dropdown_is_open()) {
        s_wifi_scan_pending = true;
        return;
    }

    kiln_wifi_scan(s_scan_results);
    wifi_rebuild_dropdown_options(false);
}

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
    (void)e;
    appState.tempUnit = TempUnit::FAHRENHEIT;
    refresh_unit_buttons();
}

static void on_unit_c(lv_event_t *e) {
    (void)e;
    appState.tempUnit = TempUnit::CELSIUS;
    refresh_unit_buttons();
}

static void on_back(lv_event_t *e) {
    (void)e;
    close_pass_modal(false);
    ui_text_keyboard_hide();
    persistence.saveSettings();
    ui_switch_to_main_screen();
}

static void screen_delete_cb(lv_event_t *e) {
    (void)e;
    close_pass_modal(false);
    if (s_wifi_scan_timer) {
        lv_timer_del(s_wifi_scan_timer);
        s_wifi_scan_timer = nullptr;
    }
    settings_screen    = nullptr;
    lbl_wifi_ip        = nullptr;
    lbl_wifi_rssi      = nullptr;
    dropdown_wifi      = nullptr;
    btn_wifi_passphrase = nullptr;
}

void ui_settings_screen_create() {
    s_dropdown_was_open  = false;
    s_wifi_scan_pending  = false;
    s_selected_ssid      = persistence.loadWifiSsid();
    s_wifi_pass          = persistence.loadWifiPass();

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

    lv_obj_t *cont_fill = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_fill, lv_pct(100));
    lv_obj_set_flex_grow(cont_fill, 1);
    lv_obj_set_layout(cont_fill, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_fill, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont_fill, UI_PAD_STD, 0);
    lv_obj_set_style_pad_row(cont_fill, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(cont_fill, 0, 0);
    lv_obj_set_style_bg_opa(cont_fill, 0, 0);
    lv_obj_add_flag(cont_fill, LV_OBJ_FLAG_SCROLLABLE);

    // --- Temperature (above Wi-Fi) ---
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

    // --- Wi-Fi ---
    lv_obj_t *lbl_wifi_hdr = lv_label_create(cont_fill);
    lv_label_set_text(lbl_wifi_hdr, "Wi-Fi");

    auto add_wifi_row = [](lv_obj_t *parent, const char *title, lv_obj_t **value_out) {
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, 0, 0);

        lv_obj_t *t = lv_label_create(row);
        lv_label_set_text(t, title);

        *value_out = lv_label_create(row);
        lv_obj_set_flex_grow(*value_out, 1);
        lv_obj_set_style_text_align(*value_out, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(*value_out, "-");
    };

    add_wifi_row(cont_fill, "IP address", &lbl_wifi_ip);
    add_wifi_row(cont_fill, "Signal", &lbl_wifi_rssi);

    lv_obj_t *net_row = lv_obj_create(cont_fill);
    lv_obj_set_width(net_row, lv_pct(100));
    lv_obj_set_height(net_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(net_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(net_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(net_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(net_row, UI_PAD_STD, 0);
    lv_obj_set_style_border_width(net_row, 0, 0);
    lv_obj_set_style_pad_all(net_row, 0, 0);
    lv_obj_set_style_bg_opa(net_row, 0, 0);

    lv_obj_t *lbl_net = lv_label_create(net_row);
    lv_label_set_text(lbl_net, "Network");
    lv_obj_set_width(lbl_net, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(lbl_net, 0);

    dropdown_wifi = lv_dropdown_create(net_row);
    lv_obj_set_flex_grow(dropdown_wifi, 1);
    lv_dropdown_set_options(dropdown_wifi, "");
    lv_obj_add_event_cb(dropdown_wifi, on_dropdown_wifi, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *passphrase_row = lv_obj_create(cont_fill);
    lv_obj_set_width(passphrase_row, lv_pct(100));
    lv_obj_set_height(passphrase_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(passphrase_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(passphrase_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(passphrase_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(passphrase_row, 0, 0);
    lv_obj_set_style_pad_all(passphrase_row, 0, 0);
    lv_obj_set_style_bg_opa(passphrase_row, 0, 0);

    btn_wifi_passphrase = lv_btn_create(passphrase_row);
    lv_obj_set_width(btn_wifi_passphrase, LV_SIZE_CONTENT);
    lv_obj_set_height(btn_wifi_passphrase, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_wifi_passphrase, on_passphrase_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_pp = lv_label_create(btn_wifi_passphrase);
    lv_label_set_text(lbl_pp, "Password...");
    lv_obj_center(lbl_pp);

    kiln_wifi_scan(s_scan_results);
    wifi_rebuild_dropdown_options(true);

    s_wifi_scan_timer = lv_timer_create(wifi_scan_timer_cb, WIFI_SCAN_PERIOD_MS, NULL);

    refresh_wifi_labels();
}

lv_obj_t *ui_settings_screen_get() {
    return settings_screen;
}

void ui_settings_screen_update_status() {
    if (!settings_screen)
        return;
    if (lv_scr_act() != settings_screen)
        return;

    bool open = wifi_dropdown_is_open();
    if (s_dropdown_was_open && !open && s_wifi_scan_pending) {
        s_wifi_scan_pending = false;
        kiln_wifi_scan(s_scan_results);
        wifi_rebuild_dropdown_options(false);
    }
    s_dropdown_was_open = open;

    refresh_wifi_labels();
}

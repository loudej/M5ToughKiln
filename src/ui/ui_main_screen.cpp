#include "../lvgl_includes.h"
#include "ui_main_screen.h"
#include "ui.h"
#include "ui_sizes.h"
#include "robotomono_fonts.h"
#include "../model/app_state.h"
#include "../model/temp_units.h"
#include "../model/profile_generator.h"
#include "../model/program_selection_snapshot.h"
#include "ui_profile_chart.h"

#include <algorithm>
#include <string>

// 70°F expressed in °C — assumed room temperature for duration estimates
static constexpr float ROOM_TEMP_C = (70.0f - 32.0f) * 5.0f / 9.0f;

// Progress bar height (thermal bar + time progress bar)
static constexpr lv_coord_t BAR_H = 10;

static lv_obj_t *main_screen;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_power;
static lv_obj_t *bar_power;
static lv_obj_t *lbl_program;
static lv_obj_t *lbl_hdr_target;
/// Target row col 1: idle = "Cone …"; running = live setpoint.
static lv_obj_t *lbl_target_left;
static lv_obj_t *bar_thermal;
/// Target row col 3: idle = peak temp; running = schedule peak temp.
static lv_obj_t *lbl_target_peak;
/// Time row col 1: elapsed when running/DONE; empty when idle.
static lv_obj_t *lbl_time_left;
static lv_obj_t *bar_progress;
static lv_obj_t *lbl_time_right;
static lv_obj_t *btn_start;
static lv_obj_t *lbl_btn_start;
static lv_obj_t *btn_config;
static lv_obj_t *btn_go_back;

static const FiringProgram* get_active_program() {
    int idx = appState.activeProgramIndex;
    if (idx >= 0 && idx <= 3 && (size_t)idx < appState.predefinedPrograms.size())
        return &appState.predefinedPrograms[idx];
    int customIdx = idx - 4;
    if (customIdx >= 0 && (size_t)customIdx < appState.customPrograms.size())
        return &appState.customPrograms[customIdx];
    return nullptr;
}

static void update_idle_program_labels() {
    const FiringProgram* prog = get_active_program();
    if (!prog || prog->segments.empty()) {
        lv_label_set_text(lbl_target_left, "--");
        lv_label_set_text(lbl_target_peak, "--");
        lv_label_set_text(lbl_time_left, "");
        lv_label_set_text(lbl_time_right, "--:--");
        return;
    }

    float peakC = 0;
    for (const auto& seg : prog->segments) {
        if (seg.targetTemperature > peakC) peakC = seg.targetTemperature;
    }
    const float peakDisp = toDisplayTemp(peakC, appState.tempUnit);

    std::string coneStr;
    if (!prog->origCone.empty())
        coneStr = prog->origCone;
    else
        coneStr = ProfileGenerator::coneLabelFromPeakTempC(peakC);

    lv_label_set_text_fmt(lbl_target_left, "Cone %s", coneStr.c_str());
    lv_label_set_text_fmt(lbl_target_peak, "%.0f\xc2\xb0%s", peakDisp,
                          unitSymbol(appState.tempUnit));

    float totalMins = ProfileGenerator::estimateTotalMinutes(*prog, ROOM_TEMP_C);
    int   h         = static_cast<int>(totalMins) / 60;
    int   m         = static_cast<int>(totalMins) % 60;
    lv_label_set_text(lbl_time_left, "");
    lv_label_set_text_fmt(lbl_time_right, "%d:%02d", h, m);
}

static void btn_config_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_switch_to_program_config_screen();
    }
}

static void btn_go_back_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    program_selection_swap_with_previous();
    ui_main_screen_update();
}

static void btn_settings_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_switch_to_settings_screen();
    }
}

static void btn_start_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;

    const KilnState s = appState.status.currentState;
    if (s == KilnState::IDLE) {
        appState.status.currentState = KilnState::RAMPING;
    } else {
        appState.status.currentState = KilnState::IDLE;
        appState.status.frozenControllerError.clear();
    }
    ui_main_screen_update();
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
    lv_obj_set_style_pad_all(cont_fill, UI_PAD_STD, 0);
    lv_obj_set_style_bg_opa(cont_fill, 0, 0);

    lv_obj_t *cont_bottom = lv_obj_create(screen_layout);
    lv_obj_set_width(cont_bottom, lv_pct(100));
    lv_obj_set_height(cont_bottom, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(cont_bottom, 0, 0);
    lv_obj_set_style_pad_all(cont_bottom, UI_PAD_STD, 0);
    lv_obj_set_style_bg_opa(cont_bottom, 0, 0);
    lv_obj_set_layout(cont_bottom, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_bottom, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_bottom, UI_PAD_STD, 0);

    static const lv_coord_t col_dsc[] = {
        LV_GRID_CONTENT, // Row-title column
        LV_GRID_CONTENT, // Left value (Power / Target / Time): right-aligned text
        LV_GRID_FR(1),   // Bar column (hidden when idle)
        LV_GRID_CONTENT, // Right value (peak / remaining): right-aligned text
        LV_GRID_TEMPLATE_LAST
    };
    static const lv_coord_t row_dsc[] = {
        LV_GRID_CONTENT, // Row 0: Program
        LV_GRID_CONTENT, // Row 1: Temp
        LV_GRID_CONTENT, // Row 2: Status
        LV_GRID_CONTENT, // Row 3: Power
        LV_GRID_CONTENT, // Row 4: Target
        LV_GRID_CONTENT, // Row 5: Time
        LV_GRID_CONTENT, // Row 6: Schedule vs actual chart (fixed height; scroll cont_fill if needed)
        LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_layout(cont_fill, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(cont_fill, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(cont_fill, UI_PAD_STD, 0);
    lv_obj_set_style_pad_column(cont_fill, UI_PAD_STD, 0);

    // Row 0: Program
    lv_obj_t *lbl_hdr_program = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_program, "Program");
    lv_obj_set_grid_cell(lbl_hdr_program, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t *cont_program_row = lv_obj_create(cont_fill);
    lv_obj_set_width(cont_program_row, lv_pct(100));
    lv_obj_set_height(cont_program_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(cont_program_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_program_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(cont_program_row, UI_PAD_STD, 0);
    lv_obj_set_flex_align(cont_program_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(cont_program_row, 0, 0);
    lv_obj_set_style_pad_all(cont_program_row, 0, 0);
    lv_obj_set_style_bg_opa(cont_program_row, 0, 0);
    lv_obj_set_grid_cell(cont_program_row, LV_GRID_ALIGN_STRETCH, 1, 3, LV_GRID_ALIGN_CENTER, 0, 1);

    btn_config = lv_btn_create(cont_program_row);
    lv_obj_set_flex_grow(btn_config, 1);
    lv_obj_add_event_cb(btn_config, btn_config_event_cb, LV_EVENT_ALL, NULL);

    lbl_program = lv_label_create(btn_config);
    lv_label_set_text(lbl_program, "None");
    lv_obj_center(lbl_program);

    btn_go_back = lv_btn_create(cont_program_row);
    lv_obj_set_flex_grow(btn_go_back, 0);
    lv_obj_set_style_min_width(btn_go_back, 52, 0);
    lv_obj_add_event_cb(btn_go_back, btn_go_back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_go_back = lv_label_create(btn_go_back);
    lv_label_set_text(lbl_go_back, LV_SYMBOL_LOOP);
    lv_obj_center(lbl_go_back);

    // Row 1: Temperature
    lv_obj_t *lbl_hdr_temp = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_temp, "Temp");
    lv_obj_set_grid_cell(lbl_hdr_temp, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    lbl_temp = lv_label_create(cont_fill);
    lv_obj_set_style_text_font(lbl_temp, &robotomono_48, 0);
    lv_obj_set_style_text_align(lbl_temp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_temp, "25.0 C");
    lv_obj_set_grid_cell(lbl_temp, LV_GRID_ALIGN_STRETCH, 1, 3, LV_GRID_ALIGN_CENTER, 1, 1);

    // Row 2: Status
    lv_obj_t *lbl_hdr_status = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_status, "Status");
    lv_obj_set_grid_cell(lbl_hdr_status, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    lbl_status = lv_label_create(cont_fill);
    lv_label_set_text(lbl_status, "IDLE");
    lv_obj_set_grid_cell(lbl_status, LV_GRID_ALIGN_STRETCH, 1, 3, LV_GRID_ALIGN_CENTER, 2, 1);

    // Row 3: Power (idle / always: left triplet column only — aligns with Target/Time left fields)
    lv_obj_t *lbl_hdr_power = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_power, "Power");
    lv_obj_set_grid_cell(lbl_hdr_power, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    lbl_power = lv_label_create(cont_fill);
    lv_obj_set_style_width(lbl_power, LV_SIZE_CONTENT, 0);
    lv_label_set_long_mode(lbl_power, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(lbl_power, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_power, "0%");
    lv_obj_set_grid_cell(lbl_power, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    bar_power = lv_bar_create(cont_fill);
    lv_obj_set_width(bar_power, lv_pct(100));
    lv_obj_set_height(bar_power, BAR_H);
    lv_bar_set_range(bar_power, 0, 100);
    lv_bar_set_value(bar_power, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_power, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_power, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_power, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_power, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
    lv_obj_set_grid_cell(bar_power, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_add_flag(bar_power, LV_OBJ_FLAG_HIDDEN);

    // Row 4: Target — one left label (idle vs running text); bar; one right label (peak when running)
    lbl_hdr_target = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_target, "Target");
    lv_obj_set_grid_cell(lbl_hdr_target, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    lbl_target_left = lv_label_create(cont_fill);
    lv_obj_set_style_text_font(lbl_target_left, &robotomono_14, 0);
    lv_obj_set_style_width(lbl_target_left, LV_SIZE_CONTENT, 0);
    lv_label_set_long_mode(lbl_target_left, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(lbl_target_left, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_target_left, "—");
    lv_obj_set_grid_cell(lbl_target_left, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    bar_thermal = lv_bar_create(cont_fill);
    lv_obj_set_width(bar_thermal, lv_pct(100));
    lv_obj_set_height(bar_thermal, BAR_H);
    lv_bar_set_range(bar_thermal, 0, 100);
    lv_bar_set_value(bar_thermal, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_thermal, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_thermal, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_thermal, 4, LV_PART_INDICATOR);
    lv_obj_set_grid_cell(bar_thermal, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_add_flag(bar_thermal, LV_OBJ_FLAG_HIDDEN);

    lbl_target_peak = lv_label_create(cont_fill);
    lv_obj_set_style_text_font(lbl_target_peak, &robotomono_14, 0);
    lv_obj_set_style_width(lbl_target_peak, LV_SIZE_CONTENT, 0);
    lv_label_set_long_mode(lbl_target_peak, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(lbl_target_peak, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_target_peak, "");
    lv_obj_set_grid_cell(lbl_target_peak, LV_GRID_ALIGN_END, 3, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    // Row 5: Time — one left label; bar; one right label (remaining when running)
    lv_obj_t *lbl_hdr_time = lv_label_create(cont_fill);
    lv_label_set_text(lbl_hdr_time, "Time");
    lv_obj_set_grid_cell(lbl_hdr_time, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 5, 1);

    lbl_time_left = lv_label_create(cont_fill);
    lv_obj_set_style_text_font(lbl_time_left, &robotomono_14, 0);
    lv_obj_set_style_width(lbl_time_left, LV_SIZE_CONTENT, 0);
    lv_label_set_long_mode(lbl_time_left, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(lbl_time_left, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_time_left, "");
    lv_obj_set_grid_cell(lbl_time_left, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 5, 1);

    bar_progress = lv_bar_create(cont_fill);
    lv_obj_set_width(bar_progress, lv_pct(100));
    lv_obj_set_height(bar_progress, BAR_H);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_grid_cell(bar_progress, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_CENTER, 5, 1);
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);

    lbl_time_right = lv_label_create(cont_fill);
    lv_obj_set_style_text_font(lbl_time_right, &robotomono_14, 0);
    lv_obj_set_style_width(lbl_time_right, LV_SIZE_CONTENT, 0);
    lv_label_set_long_mode(lbl_time_right, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(lbl_time_right, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_time_right, "--:--");
    lv_obj_set_grid_cell(lbl_time_right, LV_GRID_ALIGN_END, 3, 1, LV_GRID_ALIGN_CENTER, 5, 1);

    ui_profile_chart_create(cont_fill);

    // Start/Stop Button (fills available width)
    btn_start = lv_btn_create(cont_bottom);
    lv_obj_set_flex_grow(btn_start, 1);
    lv_obj_set_height(btn_start, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_start, btn_start_event_cb, LV_EVENT_ALL, NULL);
    lbl_btn_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_btn_start, "START");
    lv_obj_center(lbl_btn_start);

    // Settings Button (gear icon) — wider touch target vs START
    lv_obj_t *btn_settings = lv_btn_create(cont_bottom);
    lv_obj_set_height(btn_settings, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(btn_settings, 56, 0);
    lv_obj_set_width(btn_settings, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_settings, btn_settings_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, LV_SYMBOL_SETTINGS);
    lv_obj_center(lbl_settings);

    lv_scr_load(main_screen);
    ui_main_screen_update();
}

lv_obj_t* ui_main_screen_get() {
    return main_screen;
}

void ui_main_screen_update() {
    if (!main_screen) return;

    // Update current temperature with degree symbol
    float dispTemp = toDisplayTemp(appState.status.currentTemperature, appState.tempUnit);
    lv_label_set_text_fmt(lbl_temp, "%.1f\xc2\xb0%s", dispTemp, unitSymbol(appState.tempUnit));
    lv_label_set_text_fmt(lbl_program, "%s", appState.status.activeProgramName.c_str());

    lv_label_set_text_fmt(lbl_power, "%.0f%%", appState.status.power * 100.0f);

    // Update state dependent UI
    if (appState.status.currentState == KilnState::IDLE) {
        const KilnSensorRead& sr = appState.status.sensor;
        if (!sr.hardwareInitialized || !sr.communicationOk) {
            lv_label_set_text(lbl_status, "NO SENSOR");
            lv_obj_set_style_text_color(lbl_status, lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else if (sr.deviceReportsFault()) {
            char faultLine[28];
            kilnFormatStatusFaultLine(sr.statusRegister, faultLine, sizeof faultLine);
            lv_label_set_text(lbl_status, faultLine);
            lv_obj_set_style_text_color(lbl_status, lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else {
            lv_label_set_text(lbl_status, "IDLE");
            lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
        }
        update_idle_program_labels();
        lv_obj_add_flag(bar_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar_thermal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_btn_start, "START");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_clear_state(btn_config, LV_STATE_DISABLED);
    } else if (appState.status.currentState == KilnState::DONE) {
        lv_label_set_text(lbl_status, "COMPLETE");
        lv_obj_set_style_text_color(lbl_status, lv_palette_main(LV_PALETTE_GREEN), 0);

        float dispTarget = toDisplayTemp(appState.status.targetTemperature, appState.tempUnit);
        const char* sym = unitSymbol(appState.tempUnit);
        lv_obj_clear_flag(bar_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bar_thermal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(bar_power,
                         static_cast<int32_t>(appState.status.power * 100.f + 0.5f),
                         LV_ANIM_OFF);
        lv_label_set_text_fmt(lbl_target_left, "%.1f\xc2\xb0%s", dispTarget, sym);

        {
            const float floorC =
                std::min(fromDisplayTemp(75.0f, TempUnit::FAHRENHEIT),
                         appState.status.programRunStartTemperatureC);
            const float peakC = appState.status.programPeakTemperatureC;
            const float tgtC  = appState.status.targetTemperature;
            float       span  = peakC - floorC;
            int         pct   = 0;
            if (span > 1e-4f)
                pct = static_cast<int>((tgtC - floorC) / span * 100.f + 0.5f);
            else if (peakC > floorC)
                pct = (tgtC >= peakC) ? 100 : 0;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            lv_bar_set_value(bar_thermal, pct, LV_ANIM_OFF);

            float dPeak = toDisplayTemp(peakC, appState.tempUnit);
            lv_label_set_text_fmt(lbl_target_peak, "%.0f\xc2\xb0%s", dPeak, sym);
            lv_obj_set_style_bg_color(bar_thermal, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
        }

        uint32_t elapsedSec = appState.status.totalTimeElapsed;
        lv_label_set_text_fmt(lbl_time_left, "%u:%02u",
                              elapsedSec / 3600, (elapsedSec % 3600) / 60);
        lv_label_set_text_fmt(lbl_time_right, "%u:%02u", 0u, 0u);
        lv_bar_set_value(bar_progress, 100, LV_ANIM_OFF);

        lv_label_set_text(lbl_btn_start, "DONE");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
        lv_obj_add_state(btn_config, LV_STATE_DISABLED);
    } else if (appState.status.currentState == KilnState::ERROR) {
        if (!appState.status.frozenControllerError.empty()) {
            lv_label_set_text(lbl_status, appState.status.frozenControllerError.c_str());
        } else {
            lv_label_set_text(lbl_status, "ERROR: Unknown");
        }
        lv_obj_set_style_text_color(lbl_status, lv_palette_main(LV_PALETTE_RED), 0);
        update_idle_program_labels();
        lv_obj_add_flag(bar_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar_thermal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_btn_start, "RESET");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_clear_state(btn_config, LV_STATE_DISABLED);
    } else {
        // Running states (Ramping, Soaking, Cooling)
        const KilnSensorRead& sr = appState.status.sensor;
        if (!sr.controlUsable()) {
            if (sr.deviceReportsFault()) {
                char faultLine[28];
                kilnFormatStatusFaultLine(sr.statusRegister, faultLine, sizeof faultLine);
                lv_label_set_text(lbl_status, faultLine);
            } else {
                lv_label_set_text(lbl_status, "SENSOR?");
            }
            lv_obj_set_style_text_color(lbl_status, lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else {
            lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
            if (appState.status.currentState == KilnState::RAMPING) {
                lv_label_set_text(lbl_status, "RAMPING");
            } else if (appState.status.currentState == KilnState::SOAKING) {
                lv_label_set_text(lbl_status, "SOAKING");
            } else if (appState.status.currentState == KilnState::COOLING) {
                lv_label_set_text(lbl_status, "COOLING");
            }
        }

        float dispTarget = toDisplayTemp(appState.status.targetTemperature, appState.tempUnit);
        const char* sym = unitSymbol(appState.tempUnit);
        lv_obj_clear_flag(bar_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bar_thermal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(bar_power,
                         static_cast<int32_t>(appState.status.power * 100.f + 0.5f),
                         LV_ANIM_OFF);
        lv_label_set_text_fmt(lbl_target_left, "%.1f\xc2\xb0%s", dispTarget, sym);

        {
            const float floorC =
                std::min(fromDisplayTemp(75.0f, TempUnit::FAHRENHEIT),
                         appState.status.programRunStartTemperatureC);
            const float peakC = appState.status.programPeakTemperatureC;
            const float tgtC  = appState.status.targetTemperature;
            float       span  = peakC - floorC;
            int         pct   = 0;
            if (span > 1e-4f)
                pct = static_cast<int>((tgtC - floorC) / span * 100.f + 0.5f);
            else if (peakC > floorC)
                pct = (tgtC >= peakC) ? 100 : 0;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            lv_bar_set_value(bar_thermal, pct, LV_ANIM_OFF);

            float dPeak = toDisplayTemp(peakC, appState.tempUnit);
            lv_label_set_text_fmt(lbl_target_peak, "%.0f\xc2\xb0%s", dPeak, sym);

            lv_color_t indCol;
            switch (appState.status.currentState) {
                case KilnState::RAMPING:
                    indCol = lv_palette_main(LV_PALETTE_RED);
                    break;
                case KilnState::COOLING:
                    indCol = lv_palette_main(LV_PALETTE_BLUE);
                    break;
                case KilnState::SOAKING:
                    indCol = lv_palette_main(LV_PALETTE_GREEN);
                    break;
                default:
                    indCol = lv_palette_main(LV_PALETTE_GREY);
                    break;
            }
            lv_obj_set_style_bg_color(bar_thermal, indCol, LV_PART_INDICATOR);
        }

        uint32_t elapsedSec = appState.status.totalTimeElapsed;
        lv_label_set_text_fmt(lbl_time_left, "%u:%02u",
                              elapsedSec / 3600, (elapsedSec % 3600) / 60);

        const FiringProgram* prog = get_active_program();
        float totalSec = prog ? ProfileGenerator::estimateTotalMinutes(*prog, ROOM_TEMP_C) * 60.0f : 0.0f;

        float remSec = (totalSec > (float)elapsedSec) ? totalSec - (float)elapsedSec : 0.0f;
        uint32_t remSecU = (uint32_t)remSec;
        lv_label_set_text_fmt(lbl_time_right, "%u:%02u",
                              remSecU / 3600, (remSecU % 3600) / 60);

        int timePct = (totalSec > 0.0f) ? (int)((float)elapsedSec / totalSec * 100.0f) : 0;
        if (timePct > 100) timePct = 100;
        lv_bar_set_value(bar_progress, timePct, LV_ANIM_OFF);

        lv_label_set_text(lbl_btn_start, "STOP");
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_add_state(btn_config, LV_STATE_DISABLED);
    }

    {
        const KilnState navState = appState.status.currentState;
        const bool progPickEnabled =
            (navState == KilnState::IDLE || navState == KilnState::ERROR);
        const bool prevOk = g_previous_program_selection.valid;
        if (progPickEnabled && prevOk)
            lv_obj_clear_state(btn_go_back, LV_STATE_DISABLED);
        else
            lv_obj_add_state(btn_go_back, LV_STATE_DISABLED);
    }

    ui_profile_chart_update();
}

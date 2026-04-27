#include "../lvgl_includes.h"
#include "ui_edit_segment_popup.h"
#include "ui_keypad.h"
#include "ui_sizes.h"
#include "../model/app_state.h"
#include "../model/temp_units.h"

static constexpr lv_coord_t POPUP_W = 280;

// Struct to pass context to event handlers
struct PopupContext {
    lv_obj_t* popup;
    int prog_idx;
    int seg_idx;
    RefreshListCb refresh_cb;
    lv_obj_t* ta_target;
    lv_obj_t* ta_rate;
    lv_obj_t* ta_soak;
    lv_obj_t* lbl_target;
    lv_obj_t* lbl_rate;
};

static void on_save(lv_event_t *e) {
    PopupContext *ctx = (PopupContext *)lv_event_get_user_data(e);

    // Inputs are in display units; validate and convert to Celsius for storage
    float targetDisplay = atof(lv_textarea_get_text(ctx->ta_target));
    float rateDisplay   = atof(lv_textarea_get_text(ctx->ta_rate));

    const TempUnit tu = appState.getTempUnit();

    float targetC = fromDisplayTemp(targetDisplay, tu);
    float rateC   = fromDisplayRate(rateDisplay, tu);

    bool targetOk = targetC >= 1.0f && targetC <= 1400.0f;
    bool rateOk   = rateC   >= 1.0f && rateC   <= 999.0f;

    lv_obj_set_style_text_color(ctx->lbl_target,
        targetOk ? lv_color_white() : lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_text_color(ctx->lbl_rate,
        rateOk   ? lv_color_white() : lv_palette_main(LV_PALETTE_RED), 0);

    if (!targetOk || !rateOk) return;

    FiringSegment tempSeg;
    tempSeg.targetTemperature = targetC;
    tempSeg.rampRate = rateC;
    const char* soakText = lv_textarea_get_text(ctx->ta_soak);
    tempSeg.soakTime = (soakText && soakText[0] != '\0') ? atoi(soakText) : 0;

    FiringProgram cp{};
    if (!appState.tryCopyCustomProgram(static_cast<size_t>(ctx->prog_idx), &cp))
        return;
    if (ctx->seg_idx == -1) {
        cp.segments.push_back(tempSeg);
    } else {
        if (ctx->seg_idx < 0 || static_cast<size_t>(ctx->seg_idx) >= cp.segments.size())
            return;
        cp.segments[static_cast<size_t>(ctx->seg_idx)] = tempSeg;
    }
    appState.replaceCustomProgram(static_cast<size_t>(ctx->prog_idx), std::move(cp));

    if (ctx->refresh_cb) ctx->refresh_cb();
    lv_obj_del(ctx->popup);
    delete ctx;
}

static void on_delete(lv_event_t *e) {
    PopupContext *ctx = (PopupContext *)lv_event_get_user_data(e);
    
    if (ctx->seg_idx != -1) {
        appState.eraseCustomSegment(static_cast<size_t>(ctx->prog_idx), static_cast<size_t>(ctx->seg_idx));
    }

    if (ctx->refresh_cb) ctx->refresh_cb();
    lv_obj_del(ctx->popup);
    delete ctx;
}

void ui_edit_segment_popup_create(lv_obj_t *parent, int prog_idx, int seg_idx, RefreshListCb refresh_cb) {
    if (prog_idx < 0 || static_cast<size_t>(prog_idx) >= appState.getCustomProgramCount())
        return;

    FiringProgram pg{};
    if (!appState.tryCopyCustomProgram(static_cast<size_t>(prog_idx), &pg))
        return;
    if (seg_idx != -1 &&
        (seg_idx < 0 || static_cast<size_t>(seg_idx) >= pg.segments.size()))
        return;

    const TempUnit dispUnit = appState.getTempUnit();
    
    // Create an overlay to act as a modal backdrop and center the popup
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *popup = lv_obj_create(overlay);
    lv_obj_set_width(popup, POPUP_W);
    lv_obj_set_height(popup, LV_SIZE_CONTENT);
    lv_obj_center(popup);
    lv_obj_set_layout(popup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, UI_PAD_STD, 0);
    lv_obj_set_style_pad_row(popup, UI_PAD_STD, 0);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x1a1d21), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    
    // Allocate context (must be freed on close)
    PopupContext* ctx = new PopupContext();
    ctx->popup = overlay; // Delete the overlay (which contains the popup) on close
    ctx->prog_idx = prog_idx;
    ctx->seg_idx = seg_idx;
    ctx->refresh_cb = refresh_cb;

    // --- Header row: [<- OK] [Segment N] [Remove] ---
    lv_obj_t *cont_header = lv_obj_create(popup);
    lv_obj_set_width(cont_header, lv_pct(100));
    lv_obj_set_height(cont_header, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(cont_header, 0, 0);
    lv_obj_set_style_border_width(cont_header, 0, 0);
    lv_obj_set_style_bg_opa(cont_header, 0, 0);
    lv_obj_set_layout(cont_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // OK / back button (default theme color, left)
    lv_obj_t *btn_ok = lv_btn_create(cont_header);
    lv_obj_set_size(btn_ok, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_ok, on_save, LV_EVENT_CLICKED, ctx);
    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_ok);

    // Title label (fills space between buttons)
    lv_obj_t *lbl_title = lv_label_create(cont_header);
    lv_obj_set_flex_grow(lbl_title, 1);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    char title_buf[24];
    if (seg_idx == -1) {
        snprintf(title_buf, sizeof(title_buf), "New Segment");
    } else {
        snprintf(title_buf, sizeof(title_buf), "Segment %d", seg_idx + 1);
    }
    lv_label_set_text(lbl_title, title_buf);

    // Remove button (red, right)
    lv_obj_t *btn_remove = lv_btn_create(cont_header);
    lv_obj_set_size(btn_remove, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn_remove, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_remove, on_delete, LV_EVENT_CLICKED, ctx);
    lv_obj_t *lbl_remove = lv_label_create(btn_remove);
    lv_label_set_text(lbl_remove, LV_SYMBOL_TRASH);
    lv_obj_center(lbl_remove);

    // --- Inputs grid ---
    char buf[16] = "";

    lv_obj_t *cont_inputs = lv_obj_create(popup);
    lv_obj_set_width(cont_inputs, lv_pct(100));
    lv_obj_set_height(cont_inputs, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(cont_inputs, 0, 0);
    lv_obj_set_style_border_width(cont_inputs, 0, 0);
    lv_obj_set_style_bg_opa(cont_inputs, 0, 0);

    static const lv_coord_t col_dsc[] = {UI_GRID_INPUT_W, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_set_layout(cont_inputs, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(cont_inputs, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(cont_inputs, UI_PAD_STD, 0);
    lv_obj_set_style_pad_column(cont_inputs, UI_PAD_STD, 0);

    // Target Temp
    ctx->ta_target = lv_textarea_create(cont_inputs);
    lv_textarea_set_one_line(ctx->ta_target, true);
    lv_obj_set_height(ctx->ta_target, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ctx->ta_target, unitSymbol(dispUnit));
    if (seg_idx != -1) {
        float dispVal =
            toDisplayTemp(pg.segments[static_cast<size_t>(seg_idx)].targetTemperature, dispUnit);
        snprintf(buf, sizeof(buf), "%.0f", dispVal);
        lv_textarea_set_text(ctx->ta_target, buf);
    } else {
        lv_textarea_set_text(ctx->ta_target, "");
    }
    lv_obj_add_event_cb(ctx->ta_target, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ctx->ta_target, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    ctx->lbl_target = lv_label_create(cont_inputs);
    lv_label_set_text(ctx->lbl_target, "Target Temp");
    lv_obj_set_grid_cell(ctx->lbl_target, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // Ramp Rate
    ctx->ta_rate = lv_textarea_create(cont_inputs);
    lv_textarea_set_one_line(ctx->ta_rate, true);
    lv_obj_set_height(ctx->ta_rate, LV_SIZE_CONTENT);
    char rate_placeholder[8];
    snprintf(rate_placeholder, sizeof(rate_placeholder), "%s/h", unitSymbol(dispUnit));
    lv_textarea_set_placeholder_text(ctx->ta_rate, rate_placeholder);
    if (seg_idx != -1) {
        float dispVal = toDisplayRate(pg.segments[static_cast<size_t>(seg_idx)].rampRate, dispUnit);
        snprintf(buf, sizeof(buf), "%.0f", dispVal);
        lv_textarea_set_text(ctx->ta_rate, buf);
    } else {
        lv_textarea_set_text(ctx->ta_rate, "");
    }
    lv_obj_add_event_cb(ctx->ta_rate, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ctx->ta_rate, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 1, 1);

    ctx->lbl_rate = lv_label_create(cont_inputs);
    lv_label_set_text(ctx->lbl_rate, "Ramp Rate");
    lv_obj_set_grid_cell(ctx->lbl_rate, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    // Soak Time
    ctx->ta_soak = lv_textarea_create(cont_inputs);
    lv_textarea_set_one_line(ctx->ta_soak, true);
    lv_obj_set_height(ctx->ta_soak, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ctx->ta_soak, "min");
    if (seg_idx != -1 && pg.segments[static_cast<size_t>(seg_idx)].soakTime > 0) {
        snprintf(buf, sizeof(buf), "%d", (int)pg.segments[static_cast<size_t>(seg_idx)].soakTime);
        lv_textarea_set_text(ctx->ta_soak, buf);
    } else {
        lv_textarea_set_text(ctx->ta_soak, "");
    }
    lv_obj_add_event_cb(ctx->ta_soak, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ctx->ta_soak, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 2, 1);

    lv_obj_t *lbl_soak = lv_label_create(cont_inputs);
    lv_label_set_text(lbl_soak, "Soak Time");
    lv_obj_set_grid_cell(lbl_soak, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
}

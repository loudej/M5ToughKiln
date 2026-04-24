#include "../lvgl_includes.h"
#include "ui_edit_segment_popup.h"
#include "ui_keypad.h"
#include "../model/app_state.h"

// Struct to pass context to event handlers
struct PopupContext {
    lv_obj_t* popup;
    int prog_idx;
    int seg_idx;
    RefreshListCb refresh_cb;
    lv_obj_t* ta_target;
    lv_obj_t* ta_rate;
    lv_obj_t* ta_soak;
};

static void on_save(lv_event_t *e) {
    PopupContext *ctx = (PopupContext *)lv_event_get_user_data(e);
    
    FiringSegment tempSeg;
    tempSeg.targetTemperature = atof(lv_textarea_get_text(ctx->ta_target));
    tempSeg.rampRate = atof(lv_textarea_get_text(ctx->ta_rate));
    tempSeg.soakTime = atoi(lv_textarea_get_text(ctx->ta_soak));

    auto& segs = appState.customPrograms[ctx->prog_idx].segments;
    
    if (ctx->seg_idx == -1) {
        // Append new
        segs.push_back(tempSeg);
    } else {
        // Update existing
        segs[ctx->seg_idx] = tempSeg;
    }

    // Cleanup and refresh
    if (ctx->refresh_cb) ctx->refresh_cb();
    lv_obj_del(ctx->popup);
    delete ctx;
}

static void on_cancel(lv_event_t *e) {
    PopupContext *ctx = (PopupContext *)lv_event_get_user_data(e);
    lv_obj_del(ctx->popup);
    delete ctx;
}

static void on_delete(lv_event_t *e) {
    PopupContext *ctx = (PopupContext *)lv_event_get_user_data(e);
    
    if (ctx->seg_idx != -1) {
        auto& segs = appState.customPrograms[ctx->prog_idx].segments;
        segs.erase(segs.begin() + ctx->seg_idx);
    }

    if (ctx->refresh_cb) ctx->refresh_cb();
    lv_obj_del(ctx->popup);
    delete ctx;
}

void ui_edit_segment_popup_create(lv_obj_t *parent, int prog_idx, int seg_idx, RefreshListCb refresh_cb) {
    if (prog_idx < 0 || prog_idx >= appState.customPrograms.size()) return;
    
    // Create an overlay to act as a modal backdrop and center the popup
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *popup = lv_obj_create(overlay);
    lv_obj_set_width(popup, 280);
    lv_obj_set_height(popup, LV_SIZE_CONTENT);
    lv_obj_center(popup);
    lv_obj_set_layout(popup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, 15, 0);
    lv_obj_set_style_pad_row(popup, 15, 0);
    
    // Allocate context context (must be freed on close)
    PopupContext* ctx = new PopupContext();
    ctx->popup = overlay; // Delete the overlay (which contains the popup) on close
    ctx->prog_idx = prog_idx;
    ctx->seg_idx = seg_idx;
    ctx->refresh_cb = refresh_cb;

    char buf[16] = "";

    // Content container for inputs using grid
    lv_obj_t *cont_inputs = lv_obj_create(popup);
    lv_obj_set_width(cont_inputs, lv_pct(100));
    lv_obj_set_height(cont_inputs, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(cont_inputs, 0, 0);
    lv_obj_set_style_border_width(cont_inputs, 0, 0);
    lv_obj_set_style_bg_opa(cont_inputs, 0, 0);

    static const lv_coord_t col_dsc[] = {80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_set_layout(cont_inputs, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(cont_inputs, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(cont_inputs, 10, 0);
    lv_obj_set_style_pad_column(cont_inputs, 10, 0);

    // Target Temp
    ctx->ta_target = lv_textarea_create(cont_inputs);
    lv_textarea_set_one_line(ctx->ta_target, true);
    lv_obj_set_height(ctx->ta_target, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ctx->ta_target, "C");
    if (seg_idx != -1) {
        snprintf(buf, sizeof(buf), "%.0f", appState.customPrograms[prog_idx].segments[seg_idx].targetTemperature);
        lv_textarea_set_text(ctx->ta_target, buf);
    } else {
        lv_textarea_set_text(ctx->ta_target, "");
    }
    lv_obj_add_event_cb(ctx->ta_target, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ctx->ta_target, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    lv_obj_t *lbl_target = lv_label_create(cont_inputs);
    lv_label_set_text(lbl_target, "Target Temp");
    lv_obj_set_grid_cell(lbl_target, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // Ramp Rate
    ctx->ta_rate = lv_textarea_create(cont_inputs);
    lv_textarea_set_one_line(ctx->ta_rate, true);
    lv_obj_set_height(ctx->ta_rate, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ctx->ta_rate, "C/h");
    if (seg_idx != -1) {
        snprintf(buf, sizeof(buf), "%.0f", appState.customPrograms[prog_idx].segments[seg_idx].rampRate);
        lv_textarea_set_text(ctx->ta_rate, buf);
    } else {
        lv_textarea_set_text(ctx->ta_rate, "");
    }
    lv_obj_add_event_cb(ctx->ta_rate, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ctx->ta_rate, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 1, 1);

    lv_obj_t *lbl_rate = lv_label_create(cont_inputs);
    lv_label_set_text(lbl_rate, "Ramp Rate");
    lv_obj_set_grid_cell(lbl_rate, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    // Soak Time
    ctx->ta_soak = lv_textarea_create(cont_inputs);
    lv_textarea_set_one_line(ctx->ta_soak, true);
    lv_obj_set_height(ctx->ta_soak, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(ctx->ta_soak, "min");
    if (seg_idx != -1) {
        snprintf(buf, sizeof(buf), "%d", appState.customPrograms[prog_idx].segments[seg_idx].soakTime);
        lv_textarea_set_text(ctx->ta_soak, buf);
    } else {
        lv_textarea_set_text(ctx->ta_soak, "");
    }
    lv_obj_add_event_cb(ctx->ta_soak, ui_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_grid_cell(ctx->ta_soak, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 2, 1);

    lv_obj_t *lbl_soak = lv_label_create(cont_inputs);
    lv_label_set_text(lbl_soak, "Soak Time");
    lv_obj_set_grid_cell(lbl_soak, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);


    // Buttons container using flex row
    lv_obj_t *cont_buttons = lv_obj_create(popup);
    lv_obj_set_width(cont_buttons, lv_pct(100));
    lv_obj_set_height(cont_buttons, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(cont_buttons, 0, 0);
    lv_obj_set_style_border_width(cont_buttons, 0, 0);
    lv_obj_set_style_bg_opa(cont_buttons, 0, 0);
    lv_obj_set_layout(cont_buttons, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_buttons, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Delete (DEL)
    lv_obj_t *btn_delete = lv_btn_create(cont_buttons);
    lv_obj_set_size(btn_delete, 60, 40);
    lv_obj_set_style_bg_color(btn_delete, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_delete, on_delete, LV_EVENT_CLICKED, ctx);
    lv_obj_t *lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, "DEL");
    lv_obj_center(lbl_delete);

    // Cancel (X)
    lv_obj_t *btn_cancel = lv_btn_create(cont_buttons);
    lv_obj_set_size(btn_cancel, 50, 40);
    lv_obj_set_style_bg_color(btn_cancel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(btn_cancel, on_cancel, LV_EVENT_CLICKED, ctx);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_cancel);

    // Save (Checkmark)
    lv_obj_t *btn_save = lv_btn_create(cont_buttons);
    lv_obj_set_size(btn_save, 50, 40);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(btn_save, on_save, LV_EVENT_CLICKED, ctx);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_OK);
    lv_obj_center(lbl_save);
}

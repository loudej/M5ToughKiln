#include "ui_edit_segment_popup.h"

static void on_save(lv_event_t *e) {
    lv_obj_t *popup = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(popup);
}

static void on_cancel(lv_event_t *e) {
    lv_obj_t *popup = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(popup);
}

void ui_edit_segment_popup_create(lv_obj_t *parent, FiringSegment& segment) {
    lv_obj_t *popup = lv_obj_create(parent);
    lv_obj_set_size(popup, 280, 220);
    lv_obj_center(popup);
    
    // Optional: make it somewhat modal visually
    // In LVGL 8/9, true modality is usually done by creating on lv_layer_top()
    // but a centered object on the current screen is often sufficient for simple apps.

    // Target Temp
    lv_obj_t *lbl_target = lv_label_create(popup);
    lv_label_set_text(lbl_target, "Target Temp (C):");
    lv_obj_align(lbl_target, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t *ta_target = lv_textarea_create(popup);
    lv_obj_set_size(ta_target, 100, 35);
    lv_obj_align_to(ta_target, lbl_target, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Ramp Rate
    lv_obj_t *lbl_rate = lv_label_create(popup);
    lv_label_set_text(lbl_rate, "Ramp Rate (C/h):");
    lv_obj_align_to(lbl_rate, lbl_target, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    lv_obj_t *ta_rate = lv_textarea_create(popup);
    lv_obj_set_size(ta_rate, 100, 35);
    lv_obj_align_to(ta_rate, lbl_rate, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Soak Time
    lv_obj_t *lbl_soak = lv_label_create(popup);
    lv_label_set_text(lbl_soak, "Soak Time (min):");
    lv_obj_align_to(lbl_soak, lbl_rate, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    lv_obj_t *ta_soak = lv_textarea_create(popup);
    lv_obj_set_size(ta_soak, 100, 35);
    lv_obj_align_to(ta_soak, lbl_soak, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Buttons
    lv_obj_t *btn_save = lv_btn_create(popup);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(btn_save, on_save, LV_EVENT_CLICKED, popup);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save");

    lv_obj_t *btn_cancel = lv_btn_create(popup);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn_cancel, on_cancel, LV_EVENT_CLICKED, popup);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
}

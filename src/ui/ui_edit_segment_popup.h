#ifndef UI_EDIT_SEGMENT_POPUP_H
#define UI_EDIT_SEGMENT_POPUP_H

#include <lvgl.h>

// Callback type to refresh the parent list when the popup saves
typedef void (*RefreshListCb)(void);

// prog_idx: index in appState.customPrograms
// seg_idx: index of the segment, or -1 if adding a new one
void ui_edit_segment_popup_create(lv_obj_t *parent, int prog_idx, int seg_idx, RefreshListCb refresh_cb);

#endif // UI_EDIT_SEGMENT_POPUP_H

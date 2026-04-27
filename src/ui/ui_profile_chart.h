#ifndef UI_PROFILE_CHART_H
#define UI_PROFILE_CHART_H

#include "../lvgl_includes.h"

/// Adds a full-width chart row (parent must be an LVGL grid). Call once after grid rows are defined.
void ui_profile_chart_create(lv_obj_t* grid_parent);

/// Refresh program curve and actual temperature trace (call from main screen update).
void ui_profile_chart_update();

#endif

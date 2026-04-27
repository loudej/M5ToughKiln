#ifndef UI_PROFILE_CHART_H
#define UI_PROFILE_CHART_H

#include "../lvgl_includes.h"
#include <cstddef>
#include <cstdint>

/// Adds a full-width chart row (parent must be an LVGL grid). Call once after grid rows are defined.
void ui_profile_chart_create(lv_obj_t* grid_parent);

/// Refresh program curve and actual temperature trace (call from main screen update).
void ui_profile_chart_update();

/// Monotonic revision; increments when trace buffers are cleared or decimated (web clients must resync).
uint32_t ui_profile_chart_trace_revision();

/// Number of actual/power samples (same X for both).
size_t ui_profile_chart_trace_points();

/// Sample `i` in [0, count); returns false if out of range. `temp_celsius` from internal °C×10 storage.
bool ui_profile_chart_trace_point(size_t i, int32_t* t_sec, float* temp_celsius, int* power_pct);

#endif

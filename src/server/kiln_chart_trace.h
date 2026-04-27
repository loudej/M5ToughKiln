#ifndef KILN_CHART_TRACE_H
#define KILN_CHART_TRACE_H

#include <Arduino.h>

/// Incremental actual-temp / power traces (mirrors LVGL chart buffers).
/// Query: `?since=M&rev=R` — if `rev` differs or `since` is stale, full snapshot is returned with
/// `"resync":true` so the browser replaces its accumulated series.
void kiln_chart_trace_serialize(String &out, uint32_t client_revision, size_t since_index);

#endif

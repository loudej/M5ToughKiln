#include "ui_profile_chart.h"
#include "../model/app_state.h"
#include "../model/firing_program.h"
#include "../model/profile_generator.h"
#include "../model/kiln_status.h"
#include "draw/lv_draw_triangle.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Actual / power history — fixed-size min/max bucket buffer.
// When full, adjacent pairs are merged and resolution doubles; old data is never thrown away,
// just progressively coarsened uniformly across the whole history.
struct TraceSlot {
    int32_t t_sec;   ///< Bucket start time (seconds since program arm)
    int16_t hi_tc;   ///< Max temperature in bucket (°C × 10)
    int16_t lo_tc;   ///< Min temperature in bucket (°C × 10)
    uint8_t pwr_hi;  ///< Max power% in bucket (0–100)
    uint8_t pwr_lo;  ///< Min power% in bucket (0–100)
};

static constexpr size_t kTraceCapacity = 200;
static TraceSlot g_trace[kTraceCapacity];
static size_t    g_trace_count      = 0;
static int32_t   g_trace_resolution = 1; // seconds per bucket; doubles on each merge

static uint32_t g_trace_revision = 1;
static void     bump_trace_revision() { ++g_trace_revision; }

static void clear_actual_power_trace() {
    bump_trace_revision();
    g_trace_count      = 0;
    g_trace_resolution = 1;
}

namespace {

constexpr float ROOM_TEMP_C = (70.0f - 32.0f) * 5.0f / 9.0f;

/// Y values are stored as °C × 10 (int32) for LVGL chart resolution.
constexpr int32_t kYScale = 10;

constexpr uint32_t kMaxChartPoints = 200;

lv_obj_t*       g_wrap{};
lv_obj_t*       g_chart{};
lv_chart_series_t* g_ser_program{};
lv_chart_series_t* g_ser_actual{};
lv_chart_series_t* g_ser_power{};

/// Secondary Y axis for power (% × 100 int scale → 0–100).
constexpr int32_t kPowerAxisMin = 0;
constexpr int32_t kPowerAxisMax = 100;

/// Last axis ranges applied (match lv_chart_set_axis_range); used to draw the live marker.
int32_t g_axis_xmax_sec = 3600;
int32_t g_axis_y_min_centi = 0;
int32_t g_axis_y_max_centi = 1000;

/// Scheduled program end (from full vertex list, before downsampling) for DONE-state ring marker.
int32_t g_sched_prog_end_sec       = 0;
int32_t g_sched_prog_end_y_centi   = 0;

std::vector<int32_t> g_prog_x;
std::vector<int32_t> g_prog_y;

/// Piecewise schedule: same timing model as ProfileGenerator::estimateTotalMinutes + explicit soak holds.
void build_program_vertices(const FiringProgram& prog, float startTempC,
                            std::vector<int32_t>& out_x_sec, std::vector<int32_t>& out_y_centi_c) {
    out_x_sec.clear();
    out_y_centi_c.clear();
    float tMin = 0.f;
    float prevT = startTempC;

    auto push = [&](float tm, float tempC) {
        out_x_sec.push_back(static_cast<int32_t>(tm * 60.f + 0.5f));
        out_y_centi_c.push_back(static_cast<int32_t>(tempC * static_cast<float>(kYScale)));
    };

    push(tMin, prevT);

    for (const auto& seg : prog.segments) {
        float deltaC = seg.targetTemperature - prevT;
        float rampMin = 0.f;
        if (seg.rampRate > 0.f && deltaC != 0.f)
            rampMin = std::fabs(deltaC) / seg.rampRate * 60.f;
        tMin += rampMin;
        push(tMin, seg.targetTemperature);

        tMin += static_cast<float>(seg.soakTime);
        push(tMin, seg.targetTemperature);
        prevT = seg.targetTemperature;
    }
}

void downsample_if_needed(std::vector<int32_t>& xs, std::vector<int32_t>& ys, size_t max_pts) {
    if (xs.size() <= max_pts || max_pts < 4)
        return;
    std::vector<int32_t> nx, ny;
    nx.reserve(max_pts);
    ny.reserve(max_pts);
    const float step = static_cast<float>(xs.size() - 1) / static_cast<float>(max_pts - 1);
    for (size_t i = 0; i < max_pts; ++i) {
        size_t j = static_cast<size_t>(static_cast<float>(i) * step + 0.5f);
        if (j >= xs.size())
            j = xs.size() - 1;
        nx.push_back(xs[j]);
        ny.push_back(ys[j]);
    }
    xs.swap(nx);
    ys.swap(ny);
}

void clear_series_points(uint32_t point_cnt) {
    for (uint32_t i = 0; i < point_cnt; ++i) {
        lv_chart_set_series_value_by_id2(g_chart, g_ser_program, i, 0, LV_CHART_POINT_NONE);
        lv_chart_set_series_value_by_id2(g_chart, g_ser_actual, i, 0, LV_CHART_POINT_NONE);
        if (g_ser_power)
            lv_chart_set_series_value_by_id2(g_chart, g_ser_power, i, 0, LV_CHART_POINT_NONE);
    }
}

/// Axis ranges + fixed grid division counts (LVGL draws evenly spaced lines over [xmin,xmax] / [ymin,ymax]).
void apply_axes(int32_t xmax_sec, int32_t ymin_centi, int32_t ymax_centi) {
    if (xmax_sec < 1)
        xmax_sec = 1;

    const int margin_y = std::max(50, (ymax_centi - ymin_centi) / 10);
    ymin_centi -= margin_y;
    ymax_centi += margin_y;
    if (ymin_centi >= ymax_centi)
        ymax_centi = ymin_centi + 100;

    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_PRIMARY_X, 0, xmax_sec);
    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, ymin_centi, ymax_centi);
    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_SECONDARY_Y, kPowerAxisMin, kPowerAxisMax);
    lv_chart_set_div_line_count(g_chart, 5, 6);

    g_axis_xmax_sec    = xmax_sec;
    g_axis_y_min_centi = ymin_centi;
    g_axis_y_max_centi = ymax_centi;
}

static void draw_marker_cross(lv_layer_t* layer, int32_t cx, int32_t cy, lv_color_t color, int32_t half = 6) {
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.base.layer = layer;
    line_dsc.color      = color;
    line_dsc.width      = 2;
    line_dsc.opa        = LV_OPA_COVER;
    line_dsc.round_start = 1;
    line_dsc.round_end   = 1;

    line_dsc.p1.x = cx - half;
    line_dsc.p1.y = cy - half;
    line_dsc.p2.x = cx + half;
    line_dsc.p2.y = cy + half;
    lv_draw_line(layer, &line_dsc);

    line_dsc.p1.x = cx - half;
    line_dsc.p1.y = cy + half;
    line_dsc.p2.x = cx + half;
    line_dsc.p2.y = cy - half;
    lv_draw_line(layer, &line_dsc);
}

static void draw_marker_ring(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, int32_t width) {
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.base.layer   = layer;
    arc_dsc.color        = lv_color_white();
    arc_dsc.width        = width;
    arc_dsc.center.x     = cx;
    arc_dsc.center.y     = cy;
    arc_dsc.radius       = static_cast<uint16_t>(radius);
    arc_dsc.opa          = LV_OPA_COVER;
    arc_dsc.start_angle  = 0;
    arc_dsc.end_angle    = 360;
    lv_draw_arc(layer, &arc_dsc);
}

/// Filled area under power % curve (secondary Y 0–100), drawn before grid and temperature lines.
static void chart_power_mountain_on_event(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN)
        return;
    if (!g_chart || g_trace_count == 0)
        return;

    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer)
        return;

    lv_obj_t* chart = lv_event_get_target_obj(e);

    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);

    const int32_t border_w = lv_obj_get_style_border_width(chart, LV_PART_MAIN);
    const int32_t pad_l    = lv_obj_get_style_pad_left(chart, LV_PART_MAIN);
    const int32_t pad_t    = lv_obj_get_style_pad_top(chart, LV_PART_MAIN);
    const int32_t w        = lv_obj_get_content_width(chart);
    const int32_t h        = lv_obj_get_content_height(chart);
    const int32_t x_ofs    = coords.x1 + pad_l + border_w - lv_obj_get_scroll_left(chart);
    const int32_t y_ofs    = coords.y1 + pad_t + border_w - lv_obj_get_scroll_top(chart);

    const int32_t xmax = (g_axis_xmax_sec > 0) ? g_axis_xmax_sec : 1;

    auto map_x = [&](int32_t x_sec) -> int32_t {
        const int32_t xc = LV_CLAMP(0, x_sec, xmax);
        return lv_map(xc, 0, xmax, 0, w) + x_ofs;
    };
    auto map_y_pct = [&](int32_t y_pct) -> int32_t {
        const int32_t yc = LV_CLAMP(kPowerAxisMin, y_pct, kPowerAxisMax);
        const int32_t py = lv_map(yc, kPowerAxisMin, kPowerAxisMax, 0, h);
        return h - py + y_ofs;
    };

    const int32_t y_bottom = map_y_pct(0);

    lv_color_t fill_col = lv_palette_darken(LV_PALETTE_YELLOW, 3);

    auto draw_quad = [&](int32_t xa, int32_t xb, int32_t ya, int32_t yb) {
        lv_draw_triangle_dsc_t td;
        lv_draw_triangle_dsc_init(&td);
        td.base.layer = layer;
        td.color      = fill_col;
        td.opa        = LV_OPA_50;

        td.p[0].x = static_cast<lv_value_precise_t>(xa);
        td.p[0].y = static_cast<lv_value_precise_t>(ya);
        td.p[1].x = static_cast<lv_value_precise_t>(xb);
        td.p[1].y = static_cast<lv_value_precise_t>(yb);
        td.p[2].x = static_cast<lv_value_precise_t>(xb);
        td.p[2].y = static_cast<lv_value_precise_t>(y_bottom);
        lv_draw_triangle(layer, &td);

        td.p[1].y = static_cast<lv_value_precise_t>(y_bottom);
        td.p[2].x = static_cast<lv_value_precise_t>(xa);
        td.p[2].y = static_cast<lv_value_precise_t>(y_bottom);
        lv_draw_triangle(layer, &td);
    };

    if (g_trace_count == 1) {
        const int32_t x0 = map_x(g_trace[0].t_sec);
        const int32_t x1 = map_x(g_trace[0].t_sec + g_trace_resolution);
        const int32_t y0 = map_y_pct(g_trace[0].pwr_hi);
        draw_quad(x0, x1, y0, y0);
        return;
    }

    for (size_t i = 0; i + 1 < g_trace_count; ++i) {
        const int32_t x0 = map_x(g_trace[i].t_sec);
        const int32_t x1 = map_x(g_trace[i + 1].t_sec);
        const int32_t y0 = map_y_pct(g_trace[i].pwr_hi);
        const int32_t y1 = map_y_pct(g_trace[i + 1].pwr_hi);
        draw_quad(x0, x1, y0, y1);
    }
}

static void chart_marker_on_event(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t*             chart = lv_event_get_target_obj(e);

    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        int32_t* s = static_cast<int32_t*>(lv_event_get_param(e));
        *s         = LV_MAX(*s, 16);
        return;
    }

    if (code != LV_EVENT_DRAW_POST_END)
        return;

    const AppState::TelemetryView tv = appState.getTelemetryView();
    const KilnState                 st = tv.status.currentState;
    if (st == KilnState::ERROR)
        return;

    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer)
        return;

    constexpr int32_t kCrossHalf = 6;
    constexpr int32_t kRingR     = 10;
    constexpr int32_t kRingW     = 2;

    const int32_t border_w = lv_obj_get_style_border_width(chart, LV_PART_MAIN);
    const int32_t pad_l    = lv_obj_get_style_pad_left(chart, LV_PART_MAIN);
    const int32_t pad_t    = lv_obj_get_style_pad_top(chart, LV_PART_MAIN);
    const int32_t w        = lv_obj_get_content_width(chart);
    const int32_t h        = lv_obj_get_content_height(chart);

    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);
    const int32_t x_ofs = coords.x1 + pad_l + border_w - lv_obj_get_scroll_left(chart);
    const int32_t y_ofs = coords.y1 + pad_t + border_w - lv_obj_get_scroll_top(chart);

    const int32_t cur_y_centi =
        static_cast<int32_t>(tv.status.currentTemperature * static_cast<float>(kYScale));

    auto map_xy = [&](int32_t x_sec, int32_t y_centi, int32_t* out_x, int32_t* out_y) {
        const int32_t xc = LV_CLAMP(0, x_sec, g_axis_xmax_sec);
        const int32_t yc = LV_CLAMP(g_axis_y_min_centi, y_centi, g_axis_y_max_centi);
        const int32_t px = lv_map(xc, 0, g_axis_xmax_sec, 0, w);
        const int32_t py = h - lv_map(yc, g_axis_y_min_centi, g_axis_y_max_centi, 0, h);
        *out_x             = px + x_ofs;
        *out_y             = py + y_ofs;
    };

    if (st == KilnState::IDLE) {
        int32_t cx0 = 0;
        int32_t cy  = 0;
        map_xy(0, cur_y_centi, &cx0, &cy);
        const int32_t cx = cx0 + kCrossHalf + 2;
        draw_marker_cross(layer, cx, cy, lv_palette_main(LV_PALETTE_YELLOW), kCrossHalf);
        return;
    }

    const int32_t elapsed_x = static_cast<int32_t>(tv.status.totalTimeElapsed);

    int32_t ox = 0;
    int32_t oy = 0;
    int32_t xx = 0;
    int32_t xy = 0;

    if (st == KilnState::DONE) {
        map_xy(g_sched_prog_end_sec, g_sched_prog_end_y_centi, &ox, &oy);
        map_xy(elapsed_x, cur_y_centi, &xx, &xy);
        draw_marker_ring(layer, ox, oy, kRingR, kRingW);
        draw_marker_cross(layer, xx, xy, lv_palette_main(LV_PALETTE_YELLOW), kCrossHalf);
        return;
    }

    const int32_t tgt_centi =
        static_cast<int32_t>(tv.status.targetTemperature * static_cast<float>(kYScale));
    map_xy(elapsed_x, tgt_centi, &ox, &oy);
    map_xy(elapsed_x, cur_y_centi, &xx, &xy);

    draw_marker_ring(layer, ox, oy, kRingR, kRingW);
    draw_marker_cross(layer, xx, xy, lv_palette_main(LV_PALETTE_RED), kCrossHalf);
}

} // namespace

static constexpr lv_coord_t kChartHeight = 176;

void ui_profile_chart_create(lv_obj_t* grid_parent) {
    g_wrap = lv_obj_create(grid_parent);
    lv_obj_set_grid_cell(g_wrap, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_STRETCH, 6, 1);
    lv_obj_set_height(g_wrap, LV_SIZE_CONTENT);
    lv_obj_set_layout(g_wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(g_wrap, 0, 0);
    lv_obj_set_style_border_width(g_wrap, 0, 0);
    lv_obj_set_style_bg_opa(g_wrap, 0, 0);

    g_chart = lv_chart_create(g_wrap);
    lv_obj_set_width(g_chart, lv_pct(100));
    lv_obj_set_height(g_chart, kChartHeight);
    lv_obj_set_style_pad_all(g_chart, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_chart, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_chart, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);

    lv_obj_set_style_width(g_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(g_chart, 0, LV_PART_INDICATOR);

    lv_chart_set_type(g_chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(g_chart, static_cast<uint32_t>(kTraceCapacity));
    lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_CIRCULAR);

    g_ser_program = lv_chart_add_series(g_chart, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_CHART_AXIS_PRIMARY_Y);
    g_ser_actual  = lv_chart_add_series(g_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    g_ser_power =
        lv_chart_add_series(g_chart, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_SECONDARY_Y);

    lv_chart_set_series_color(g_chart, g_ser_program, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_chart_set_series_color(g_chart, g_ser_actual, lv_palette_main(LV_PALETTE_RED));

//    lv_obj_add_event_cb(g_chart, chart_power_mountain_on_event, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(g_chart, chart_marker_on_event, LV_EVENT_DRAW_POST_END, nullptr);
    lv_obj_add_event_cb(g_chart, chart_marker_on_event, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
    lv_obj_refresh_ext_draw_size(g_chart);

    ui_profile_chart_update();
}

void ui_profile_chart_update() {
    if (!g_chart || !g_ser_program || !g_ser_actual || !g_ser_power)
        return;

    static KilnState prev_kiln_state      = KilnState::IDLE;
    static int        idle_tracked_prog_i = -99999;

    static FiringProgram active_prog_snap{};
    const AppState::TelemetryView tv       = appState.getTelemetryView();
    const int                     activeIx = appState.getActiveProgramIndex();
    const KilnState               st       = tv.status.currentState;

    if (st == KilnState::IDLE &&
        (prev_kiln_state == KilnState::RAMPING || prev_kiln_state == KilnState::SOAKING ||
         prev_kiln_state == KilnState::COOLING)) {
        clear_actual_power_trace();
    }

    if (st == KilnState::RAMPING &&
        (prev_kiln_state == KilnState::IDLE || prev_kiln_state == KilnState::ERROR ||
         prev_kiln_state == KilnState::DONE)) {
        clear_actual_power_trace();
    }

    if (st == KilnState::IDLE && idle_tracked_prog_i != -99999 &&
        idle_tracked_prog_i != activeIx) {
        clear_actual_power_trace();
    }

    const bool recording_trace =
        st == KilnState::RAMPING || st == KilnState::SOAKING || st == KilnState::COOLING ||
        st == KilnState::DONE;

    const bool             haveProg = appState.tryCopyActiveProgram(&active_prog_snap);
    const FiringProgram*   prog     = haveProg ? &active_prog_snap : nullptr;

    if (!prog || prog->segments.empty()) {
        g_sched_prog_end_sec     = 0;
        g_sched_prog_end_y_centi = 0;
        if (lv_chart_get_point_count(g_chart) != 32)
            lv_chart_set_point_count(g_chart, 32);
        clear_series_points(lv_chart_get_point_count(g_chart));
        apply_axes(3600, static_cast<int32_t>(ROOM_TEMP_C * kYScale),
                   static_cast<int32_t>(ROOM_TEMP_C * kYScale + 500));
        lv_chart_refresh(g_chart);
        prev_kiln_state      = st;
        idle_tracked_prog_i = activeIx;
        return;
    }

    build_program_vertices(*prog, ROOM_TEMP_C, g_prog_x, g_prog_y);
    g_sched_prog_end_sec =
        g_prog_x.empty() ? 0 : g_prog_x.back();
    g_sched_prog_end_y_centi =
        g_prog_y.empty() ? 0 : g_prog_y.back();
    downsample_if_needed(g_prog_x, g_prog_y, 160);

    const float total_min = ProfileGenerator::estimateTotalMinutes(*prog, ROOM_TEMP_C);
    int32_t     xmax_sec  = static_cast<int32_t>(total_min * 60.f + 0.5f);
    if (xmax_sec < 1)
        xmax_sec = 1;
    if (!g_prog_x.empty())
        xmax_sec = std::max(xmax_sec, g_prog_x.back());
    xmax_sec = std::max(xmax_sec, g_sched_prog_end_sec);

    int32_t ymin_c = static_cast<int32_t>(ROOM_TEMP_C * kYScale);
    int32_t ymax_c = ymin_c;
    for (size_t i = 0; i < g_prog_y.size(); ++i) {
        ymin_c = std::min(ymin_c, g_prog_y[i]);
        ymax_c = std::max(ymax_c, g_prog_y[i]);
    }
    ymin_c = std::min(ymin_c, g_sched_prog_end_y_centi);
    ymax_c = std::max(ymax_c, g_sched_prog_end_y_centi);

    if (recording_trace) {
        const uint32_t elapsed = tv.status.totalTimeElapsed;
        const int32_t  cur_x   = static_cast<int32_t>(elapsed);
        const int16_t  cur_tc  = static_cast<int16_t>(LV_CLAMP(
            -32767, static_cast<int32_t>(tv.status.currentTemperature * static_cast<float>(kYScale)), 32767));
        const uint8_t  p_pct   = static_cast<uint8_t>(
            LV_CLAMP(0, static_cast<int32_t>(tv.status.power * 100.f + 0.5f), 100));

        if (g_trace_count == 0) {
            g_trace[0]    = { cur_x, cur_tc, cur_tc, p_pct, p_pct };
            g_trace_count = 1;
        } else {
            TraceSlot& tail = g_trace[g_trace_count - 1];
            if (cur_x - tail.t_sec < g_trace_resolution) {
                // Still within the current bucket's time window — update min/max in-place.
                if (cur_tc  > tail.hi_tc)  tail.hi_tc  = cur_tc;
                if (cur_tc  < tail.lo_tc)  tail.lo_tc  = cur_tc;
                if (p_pct   > tail.pwr_hi) tail.pwr_hi = p_pct;
                if (p_pct   < tail.pwr_lo) tail.pwr_lo = p_pct;
            } else {
                // Start a new bucket. Merge-halve first if the buffer is full.
                if (g_trace_count == kTraceCapacity) {
                    const size_t half = kTraceCapacity / 2;
                    for (size_t i = 0; i < half; ++i) {
                        const TraceSlot& a = g_trace[2 * i];
                        const TraceSlot& b = g_trace[2 * i + 1];
                        g_trace[i] = {
                            a.t_sec,
                            static_cast<int16_t>(std::max(a.hi_tc,  b.hi_tc)),
                            static_cast<int16_t>(std::min(a.lo_tc,  b.lo_tc)),
                            std::max(a.pwr_hi, b.pwr_hi),
                            std::min(a.pwr_lo, b.pwr_lo)
                        };
                    }
                    g_trace_count      = half;
                    g_trace_resolution *= 2;
                    bump_trace_revision();
                }
                g_trace[g_trace_count++] = { cur_x, cur_tc, cur_tc, p_pct, p_pct };
            }
        }

        for (size_t i = 0; i < g_trace_count; ++i) {
            ymin_c = std::min(ymin_c, static_cast<int32_t>(g_trace[i].lo_tc));
            ymax_c = std::max(ymax_c, static_cast<int32_t>(g_trace[i].hi_tc));
        }
        xmax_sec = std::max(xmax_sec, cur_x + 1);
    } else if (g_trace_count > 0) {
        for (size_t i = 0; i < g_trace_count; ++i) {
            ymin_c = std::min(ymin_c, static_cast<int32_t>(g_trace[i].lo_tc));
            ymax_c = std::max(ymax_c, static_cast<int32_t>(g_trace[i].hi_tc));
        }
        xmax_sec = std::max(xmax_sec, g_trace[g_trace_count - 1].t_sec);
    }

    const int32_t tgt_centi_idle =
        static_cast<int32_t>(tv.status.targetTemperature * static_cast<float>(kYScale));
    ymin_c = std::min(ymin_c, tgt_centi_idle);
    ymax_c = std::max(ymax_c, tgt_centi_idle);

    const uint32_t n_prog  = static_cast<uint32_t>(g_prog_x.size());
    const uint32_t n_trace = static_cast<uint32_t>(g_trace_count);
    uint32_t       need    = std::max({n_prog, n_trace, 32u});
    need                   = std::min(need, kMaxChartPoints);

    if (lv_chart_get_point_count(g_chart) != need)
        lv_chart_set_point_count(g_chart, need);

    clear_series_points(lv_chart_get_point_count(g_chart));

    for (uint32_t i = 0; i < n_prog && i < need; ++i)
        lv_chart_set_series_value_by_id2(g_chart, g_ser_program, i, g_prog_x[i], g_prog_y[i]);

    for (uint32_t i = 0; i < n_trace && i < need; ++i) {
        const int32_t mid_tc = (static_cast<int32_t>(g_trace[i].hi_tc) + static_cast<int32_t>(g_trace[i].lo_tc)) / 2;
        lv_chart_set_series_value_by_id2(g_chart, g_ser_actual, i, g_trace[i].t_sec, mid_tc);
    }

    for (uint32_t i = 0; i < n_trace && i < need && g_ser_power; ++i)
        lv_chart_set_series_value_by_id2(g_chart, g_ser_power, i, g_trace[i].t_sec,
                                          static_cast<int32_t>(g_trace[i].pwr_hi));

    apply_axes(xmax_sec, ymin_c, ymax_c);
    lv_chart_refresh(g_chart);

    prev_kiln_state      = st;
    idle_tracked_prog_i = activeIx;
}

uint32_t ui_profile_chart_trace_revision() {
    return g_trace_revision;
}

size_t ui_profile_chart_trace_points() {
    return g_trace_count;
}

bool ui_profile_chart_trace_point(size_t i, int32_t* t_sec, float* temp_celsius, int* power_pct) {
    if (!t_sec || !temp_celsius || !power_pct || i >= g_trace_count)
        return false;
    constexpr float kDegScale = 10.f;
    *t_sec        = g_trace[i].t_sec;
    *temp_celsius = (static_cast<float>(g_trace[i].hi_tc) + static_cast<float>(g_trace[i].lo_tc))
                    / 2.f / kDegScale;
    *power_pct    = static_cast<int>(g_trace[i].pwr_hi);
    return true;
}

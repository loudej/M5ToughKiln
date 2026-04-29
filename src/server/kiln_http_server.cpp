#include "kiln_http_server.h"
#include "kiln_dashboard_json.h"
#include "kiln_chart_trace.h"
#include "kiln_programs_api.h"
#include "kiln_web_assets.h"
#include "kiln_wifi.h"

#include "../model/app_state.h"
#include "../model/kiln_status.h"

#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <WebServer.h>
#include <Arduino.h>
#include <cstring>

static WebServer server(80);
static bool       s_routes_registered = false;
static bool       s_server_begun      = false;
static bool       s_elegant_ota_begun = false;

static uint32_t s_http_t0_us;
static String   s_http_path;

/// High-frequency dashboard polls: summarized on a timer instead of per request.
static constexpr uint32_t kAggFlushIntervalMs = 10000;
static constexpr size_t   kAggMaxBuckets      = 16;

static uint32_t s_agg_last_flush_ms = 0;

struct HttpAggBucket {
    char     method[8]{};
    char     path[40]{};
    int      status = 0;
    uint32_t count  = 0;
    uint64_t sum_us = 0;
    uint32_t max_us = 0;
    uint64_t sum_bytes = 0;
    uint32_t max_bytes = 0;
};

static HttpAggBucket s_agg[kAggMaxBuckets]{};
static int           s_agg_count = 0;

static const char* http_method_str() {
    switch (server.method()) {
        case HTTP_GET:
            return "GET";
        case HTTP_POST:
            return "POST";
        case HTTP_PUT:
            return "PUT";
        case HTTP_DELETE:
            return "DELETE";
        default:
            return "?";
    }
}

static String path_without_query(const String& uri) {
    const int q = uri.indexOf('?');
    return q >= 0 ? uri.substring(0, q) : uri;
}

static bool is_polled_aggregate_path(const String& pathNoQuery) {
    return pathNoQuery == "/api/status" || pathNoQuery == "/api/chart/trace";
}

static void http_agg_record(const char* method, const String& pathNoQuery, int status, uint32_t us,
                            size_t body_bytes) {
    for (int i = 0; i < s_agg_count; ++i) {
        HttpAggBucket& b = s_agg[i];
        if (b.status == status && strcmp(b.path, pathNoQuery.c_str()) == 0 &&
            strcmp(b.method, method) == 0) {
            ++b.count;
            b.sum_us += us;
            if (us > b.max_us)
                b.max_us = us;
            b.sum_bytes += body_bytes;
            const uint32_t nb = static_cast<uint32_t>(body_bytes);
            if (nb > b.max_bytes)
                b.max_bytes = nb;
            return;
        }
    }
    if (s_agg_count >= (int)kAggMaxBuckets)
        return;
    HttpAggBucket& b = s_agg[s_agg_count++];
    strncpy(b.method, method, sizeof b.method - 1);
    b.method[sizeof b.method - 1] = '\0';
    strncpy(b.path, pathNoQuery.c_str(), sizeof b.path - 1);
    b.path[sizeof b.path - 1] = '\0';
    b.status   = status;
    b.count    = 1;
    b.sum_us   = us;
    b.max_us   = us;
    b.sum_bytes = body_bytes;
    b.max_bytes = static_cast<uint32_t>(body_bytes);
}

static void http_agg_flush() {
    if (s_agg_count == 0)
        return;
    for (int i = 0; i < s_agg_count; ++i) {
        const HttpAggBucket& b = s_agg[i];
        if (b.count == 0)
            continue;
        const double avg_us   = static_cast<double>(b.sum_us) / static_cast<double>(b.count);
        const double avg_ms   = avg_us / 1000.0;
        const double max_ms   = static_cast<double>(b.max_us) / 1000.0;
        const double avg_B    = static_cast<double>(b.sum_bytes) / static_cast<double>(b.count);
        Serial.printf("[HTTP agg %lus] %s %s %d n=%lu avg_ms=%.1f max_ms=%.1f avg_B=%.0f max_B=%lu\n",
                      static_cast<unsigned long>(kAggFlushIntervalMs / 1000), b.method, b.path, b.status,
                      static_cast<unsigned long>(b.count), avg_ms, max_ms, avg_B,
                      static_cast<unsigned long>(b.max_bytes));
    }
    s_agg_count = 0;
    memset(s_agg, 0, sizeof s_agg);
}

static void http_req_begin() {
    s_http_t0_us = micros();
    s_http_path  = server.uri();
}

static void http_log_response(int status, size_t body_bytes) {
    const uint32_t us          = micros() - s_http_t0_us;
    const String   pathNorm    = path_without_query(s_http_path);
    const char*    method      = http_method_str();

    if (is_polled_aggregate_path(pathNorm)) {
        http_agg_record(method, pathNorm, status, us, body_bytes);
        return;
    }

    Serial.printf("[HTTP] %s %s %d %u B %lu us\n", method, s_http_path.c_str(), status,
                  static_cast<unsigned>(body_bytes), static_cast<unsigned long>(us));
}

static void handle_root() {
    http_req_begin();
    server.send(200, "text/html", KILN_WEB_INDEX_HTML);
    http_log_response(200, KILN_WEB_INDEX_HTML_LEN);
}

static void handle_style() {
    http_req_begin();
    server.send(200, "text/css", KILN_WEB_STYLE_CSS);
    http_log_response(200, KILN_WEB_STYLE_CSS_LEN);
}

static void handle_app() {
    http_req_begin();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/javascript", KILN_WEB_APP_JS);
    http_log_response(200, KILN_WEB_APP_JS_LEN);
}

static void handle_api_status() {
    http_req_begin();
    String json;
    kiln_dashboard_serialize_status(json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
    http_log_response(200, json.length());
}

static void handle_api_chart_trace() {
    http_req_begin();
    uint32_t rev =
        server.hasArg("rev") ? (uint32_t)server.arg("rev").toInt() : 0u;
    size_t since = server.hasArg("since") ? (size_t)server.arg("since").toInt() : 0u;

    String json;
    kiln_chart_trace_serialize(json, rev, since);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
    http_log_response(200, json.length());
}

static void handle_api_control_tap() {
    http_req_begin();
    appState.applyStartStopTap();
    String json;
    JsonDocument doc;
    doc["ok"]        = true;
    doc["kilnState"] = kilnStateJsonKey(appState.getStatus().currentState);
    serializeJson(doc, json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
    http_log_response(200, json.length());
}

static void handle_api_programs_get() {
    http_req_begin();
    String json;
    kiln_programs_serialize_json(json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
    http_log_response(200, json.length());
}

static void handle_api_programs_save() {
    http_req_begin();
    String err;
    const String body = server.arg("plain");
    if (!kiln_programs_apply_save_json(body, err)) {
        const int code = err.indexOf("locked") >= 0 ? 403 : 400;
        server.sendHeader("Cache-Control", "no-store");
        server.send(code, "application/json", err);
        http_log_response(code, err.length());
        return;
    }
    const char ok[] = "{\"ok\":true}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", ok);
    http_log_response(200, sizeof(ok) - 1);
}

static void handle_api_programs_swap() {
    http_req_begin();
    if (!kiln_programs_selection_edit_allowed()) {
        const char err[] = "{\"error\":\"program locked while firing\"}";
        server.sendHeader("Cache-Control", "no-store");
        server.send(403, "application/json", err);
        http_log_response(403, sizeof(err) - 1);
        return;
    }
    if (!appState.swapProgramSelectionWithPrevious()) {
        const char err[] = "{\"error\":\"swap unavailable\"}";
        server.sendHeader("Cache-Control", "no-store");
        server.send(403, "application/json", err);
        http_log_response(403, sizeof(err) - 1);
        return;
    }
    const char ok[] = "{\"ok\":true}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", ok);
    http_log_response(200, sizeof(ok) - 1);
}

static void handle_not_found() {
    http_req_begin();
    const char msg[] = "Not found";
    server.send(404, "text/plain", msg);
    http_log_response(404, sizeof(msg) - 1);
}

static void register_routes() {
    if (s_routes_registered)
        return;
    server.on("/", HTTP_GET, handle_root);
    server.on("/style.css", HTTP_GET, handle_style);
    server.on("/app.js", HTTP_GET, handle_app);
    server.on("/api/status", HTTP_GET, handle_api_status);
    server.on("/api/chart/trace", HTTP_GET, handle_api_chart_trace);
    server.on("/api/control/tap", HTTP_POST, handle_api_control_tap);
    server.on("/api/programs", HTTP_GET, handle_api_programs_get);
    server.on("/api/programs/save", HTTP_POST, handle_api_programs_save);
    server.on("/api/programs/swap-previous", HTTP_POST, handle_api_programs_swap);
    server.onNotFound(handle_not_found);
    s_routes_registered = true;
}

void kiln_http_server_poll() {
    if (!kiln_wifi_station_connected())
        return;

    register_routes();

    if (!s_server_begun) {
        server.begin();
        s_server_begun = true;
    }
    if (!s_elegant_ota_begun) {
        ElegantOTA.begin(&server);
        s_elegant_ota_begun = true;
    }

    server.handleClient();
    ElegantOTA.loop();

    const uint32_t now = millis();
    if (s_agg_last_flush_ms == 0)
        s_agg_last_flush_ms = now;
    else if (now - s_agg_last_flush_ms >= kAggFlushIntervalMs) {
        http_agg_flush();
        s_agg_last_flush_ms = now;
    }
}

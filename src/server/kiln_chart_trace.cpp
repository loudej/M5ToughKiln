#include "kiln_chart_trace.h"
#include "../ui/ui_profile_chart.h"

#include <ArduinoJson.h>

void kiln_chart_trace_serialize(String& out, uint32_t client_revision, size_t since_index) {
    const uint32_t rev = ui_profile_chart_trace_revision();
    const size_t   n   = ui_profile_chart_trace_points();

    JsonDocument doc;
    doc["revision"]    = rev;
    doc["totalPoints"] = n;

    const bool stale = (since_index > n);
    const bool full  = stale || (client_revision != rev);

    doc["resync"]      = full;
    doc["incremental"] = !full && since_index > 0;

    const size_t start = full ? 0 : since_index;
    const size_t i0    = (start > n) ? n : start;

    JsonArray ja = doc["actual"].to<JsonArray>();
    JsonArray jp = doc["power"].to<JsonArray>();

    for (size_t i = i0; i < n; ++i) {
        int32_t t_sec;
        float   tc;
        int     pp;
        if (!ui_profile_chart_trace_point(i, &t_sec, &tc, &pp))
            break;
        JsonObject oa = ja.add<JsonObject>();
        oa["t"]       = t_sec;
        oa["c"]       = static_cast<double>(tc);

        JsonObject ob = jp.add<JsonObject>();
        ob["t"]       = t_sec;
        ob["p"]       = pp;
    }

    out.clear();
    serializeJson(doc, out);
}

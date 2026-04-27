#include "kiln_dashboard_json.h"
#include "../model/app_state.h"
#include "../model/firing_program.h"
#include "../model/kiln_status.h"
#include "../model/profile_generator.h"
#include "../model/program_selection_snapshot.h"
#include "../model/temp_units.h"
#include "../hardware/kiln_sensor_read.h"
#include "../ui/ui_profile_chart.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr float   ROOM_TEMP_C = (70.0f - 32.0f) * 5.0f / 9.0f;
constexpr int32_t kYScale     = 10;
constexpr int     kMaxSched   = 200;

void append_idle_targets(JsonObject row, char (&coneBuf)[96], char (&peakBuf)[48], char (&etaBuf)[48],
                         const FiringProgram* prog, TempUnit unit) {
    coneBuf[0] = peakBuf[0] = etaBuf[0] = '\0';

    if (!prog || prog->segments.empty()) {
        strncpy(coneBuf, "--", sizeof coneBuf - 1);
        strncpy(peakBuf, "--", sizeof peakBuf - 1);
        strncpy(etaBuf, "--:--", sizeof etaBuf - 1);
        coneBuf[sizeof coneBuf - 1] = peakBuf[sizeof peakBuf - 1] = etaBuf[sizeof etaBuf - 1] = '\0';
        row["targetLeft"] = coneBuf;
        row["targetPeak"] = peakBuf;
        return;
    }

    float peakC = 0;
    for (const auto& seg : prog->segments) {
        if (seg.targetTemperature > peakC)
            peakC = seg.targetTemperature;
    }
    const float peakDisp = toDisplayTemp(peakC, unit);

    std::string coneStr = !prog->origCone.empty() ? prog->origCone : ProfileGenerator::coneLabelFromPeakTempC(peakC);

    snprintf(coneBuf, sizeof coneBuf, "Cone %s", coneStr.c_str());
    snprintf(peakBuf, sizeof peakBuf, "%.0f °%s", peakDisp, unitSymbol(unit));

    float totalMins = ProfileGenerator::estimateTotalMinutes(*prog, ROOM_TEMP_C);
    int   h         = static_cast<int>(totalMins) / 60;
    int   m         = static_cast<int>(totalMins) % 60;
    snprintf(etaBuf, sizeof etaBuf, "%d:%02d", h, m);

    row["targetLeft"] = coneBuf;
    row["targetPeak"] = peakBuf;
}

void build_program_vertices(const FiringProgram& prog, float startTempC, std::vector<int32_t>& out_x_sec,
                            std::vector<int32_t>& out_y_centi) {
    out_x_sec.clear();
    out_y_centi.clear();
    float tMin  = 0.f;
    float prevT = startTempC;

    auto push = [&](float tm, float tempC) {
        out_x_sec.push_back(static_cast<int32_t>(tm * 60.f + 0.5f));
        out_y_centi.push_back(static_cast<int32_t>(tempC * static_cast<float>(kYScale)));
    };

    push(tMin, prevT);

    for (const auto& seg : prog.segments) {
        float deltaC   = seg.targetTemperature - prevT;
        float rampMin  = 0.f;
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

int thermal_pct(float tgtC, float peakC, float floorC) {
    float span = peakC - floorC;
    int   pct  = 0;
    if (span > 1e-4f)
        pct = static_cast<int>((tgtC - floorC) / span * 100.f + 0.5f);
    else if (peakC > floorC)
        pct = (tgtC >= peakC) ? 100 : 0;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return pct;
}

const char* thermal_tone(KilnState st) {
    switch (st) {
        case KilnState::RAMPING:
            return "red";
        case KilnState::COOLING:
            return "blue";
        case KilnState::SOAKING:
            return "green";
        case KilnState::DONE:
            return "green";
        default:
            return "grey";
    }
}

void fill_status_line(char (&line)[160], const char** tone_out, const KilnStatus& st) {
    *tone_out = "normal";
    const KilnSensorRead& sr = st.sensor;

    switch (st.currentState) {
        case KilnState::IDLE:
            if (!sr.hardwareInitialized || !sr.communicationOk) {
                strncpy(line, "NO SENSOR", sizeof line - 1);
                *tone_out = "warn";
            } else if (sr.deviceReportsFault()) {
                kilnFormatStatusFaultLine(sr.statusRegister, line, sizeof line);
                *tone_out = "warn";
            } else {
                strncpy(line, "IDLE", sizeof line - 1);
            }
            break;
        case KilnState::DONE:
            strncpy(line, "COMPLETE", sizeof line - 1);
            *tone_out = "good";
            break;
        case KilnState::ERROR:
            if (!st.frozenControllerError.empty()) {
                strncpy(line, st.frozenControllerError.c_str(), sizeof line - 1);
                line[sizeof line - 1] = '\0';
            } else {
                strncpy(line, "ERROR: Unknown", sizeof line - 1);
            }
            *tone_out = "bad";
            break;
        default:
            if (!sr.controlUsable()) {
                if (sr.deviceReportsFault()) {
                    kilnFormatStatusFaultLine(sr.statusRegister, line, sizeof line);
                } else {
                    strncpy(line, "SENSOR?", sizeof line - 1);
                }
                *tone_out = "warn";
            } else if (st.currentState == KilnState::RAMPING) {
                strncpy(line, "RAMPING", sizeof line - 1);
            } else if (st.currentState == KilnState::SOAKING) {
                strncpy(line, "SOAKING", sizeof line - 1);
            } else if (st.currentState == KilnState::COOLING) {
                strncpy(line, "COOLING", sizeof line - 1);
            } else {
                strncpy(line, "?", sizeof line - 1);
            }
            break;
    }
    line[sizeof line - 1] = '\0';
}

} // namespace

void kiln_dashboard_serialize_status(String& out) {
    JsonDocument doc;

    const AppState::TelemetryView tv = appState.getTelemetryView();
    FiringProgram                     activeCopy{};
    const bool                      haveActive = appState.tryCopyActiveProgram(&activeCopy);
    const FiringProgram*            activeProg = haveActive ? &activeCopy : nullptr;

    doc["tempUnit"] = unitSymbol(tv.tempUnit);

    char tempBuf[48];
    float dispTemp = toDisplayTemp(tv.status.currentTemperature, tv.tempUnit);
    snprintf(tempBuf, sizeof tempBuf, "%.1f °%s", dispTemp, unitSymbol(tv.tempUnit));
    doc["temperature"] = tempBuf;

    doc["programName"] = tv.status.activeProgramName;

    doc["previousSelectionValid"] = g_previous_program_index.valid;

    char statusBuf[160];
    const char* statusTone = "normal";
    fill_status_line(statusBuf, &statusTone, tv.status);
    doc["statusText"]   = statusBuf;
    doc["statusTone"]   = statusTone;
    doc["kilnState"]    = kilnStateJsonKey(tv.status.currentState);
    doc["powerPercent"] = static_cast<int>(tv.status.power * 100.f + 0.5f);
    doc["traceRevision"] = ui_profile_chart_trace_revision();

    JsonObject bars = doc["bars"].to<JsonObject>();

    const KilnState st = tv.status.currentState;

    bool showPower    = (st != KilnState::IDLE && st != KilnState::ERROR);
    bool showThermal  = showPower || (st == KilnState::DONE);
    bool showProgress = showThermal;

    JsonObject barPower = bars["power"].to<JsonObject>();
    barPower["visible"] = showPower;
    barPower["percent"] = static_cast<int>(tv.status.power * 100.f + 0.5f);

    JsonObject barThermal = bars["thermal"].to<JsonObject>();
    barThermal["visible"] = showThermal;
    barThermal["tone"]    = thermal_tone(st);

    JsonObject barTime = bars["timeProgress"].to<JsonObject>();
    barTime["visible"]    = showProgress;

    char tgtLeft[96];
    char tgtPeak[48];
    char timeLeft[32];
    char timeRight[32];
    char etaIdle[48];

    const float floorC =
        std::min(fromDisplayTemp(75.0f, TempUnit::FAHRENHEIT), tv.status.programRunStartTemperatureC);
    const float peakProgC = tv.status.programPeakTemperatureC;
    const float tgtC      = tv.status.targetTemperature;

    JsonObject rowTarget = doc["target"].to<JsonObject>();
    JsonObject rowTime   = doc["time"].to<JsonObject>();

    if (st == KilnState::IDLE || st == KilnState::ERROR) {
        append_idle_targets(rowTarget, tgtLeft, tgtPeak, etaIdle, activeProg, tv.tempUnit);
        rowTime["timeRight"] = etaIdle;
        rowTime["timeLeft"]  = "";
        barThermal["percent"] = 0;
        barTime["percent"]    = 0;
    } else if (st == KilnState::DONE) {
        float dTarget = toDisplayTemp(tgtC, tv.tempUnit);
        snprintf(tgtLeft, sizeof tgtLeft, "%.1f °%s", dTarget, unitSymbol(tv.tempUnit));
        float dPeak = toDisplayTemp(peakProgC, tv.tempUnit);
        snprintf(tgtPeak, sizeof tgtPeak, "%.0f °%s", dPeak, unitSymbol(tv.tempUnit));

        rowTarget["targetLeft"] = tgtLeft;
        rowTarget["targetPeak"] = tgtPeak;

        uint32_t elapsedSec = tv.status.totalTimeElapsed;
        snprintf(timeLeft, sizeof timeLeft, "%u:%02u", elapsedSec / 3600, (elapsedSec % 3600) / 60);
        snprintf(timeRight, sizeof timeRight, "%u:%02u", 0u, 0u);
        rowTime["timeLeft"]  = timeLeft;
        rowTime["timeRight"] = timeRight;

        barThermal["percent"] = thermal_pct(tgtC, peakProgC, floorC);
        barTime["percent"]    = 100;
    } else {
        float dTarget = toDisplayTemp(tgtC, tv.tempUnit);
        snprintf(tgtLeft, sizeof tgtLeft, "%.1f °%s", dTarget, unitSymbol(tv.tempUnit));
        float dPeak = toDisplayTemp(peakProgC, tv.tempUnit);
        snprintf(tgtPeak, sizeof tgtPeak, "%.0f °%s", dPeak, unitSymbol(tv.tempUnit));
        rowTarget["targetLeft"] = tgtLeft;
        rowTarget["targetPeak"] = tgtPeak;

        uint32_t elapsedSec = tv.status.totalTimeElapsed;
        snprintf(timeLeft, sizeof timeLeft, "%u:%02u", elapsedSec / 3600, (elapsedSec % 3600) / 60);

        const FiringProgram* prog      = activeProg;
        float                totalSecF = prog ? ProfileGenerator::estimateTotalMinutes(*prog, ROOM_TEMP_C) * 60.f : 0.f;
        float remSec                   = (totalSecF > (float)elapsedSec) ? totalSecF - (float)elapsedSec : 0.f;
        uint32_t remSecU               = (uint32_t)remSec;
        snprintf(timeRight, sizeof timeRight, "%u:%02u", remSecU / 3600, (remSecU % 3600) / 60);

        rowTime["timeLeft"]  = timeLeft;
        rowTime["timeRight"] = timeRight;

        barThermal["percent"] = thermal_pct(tgtC, peakProgC, floorC);

        int timePct =
            (totalSecF > 0.f) ? static_cast<int>((float)elapsedSec / totalSecF * 100.f + 0.5f) : 0;
        if (timePct > 100)
            timePct = 100;
        barTime["percent"] = timePct;
    }

    JsonObject chart = doc["chart"].to<JsonObject>();

    std::vector<int32_t> px, py;
    const FiringProgram* progChart = activeProg;

    if (!progChart || progChart->segments.empty()) {
        chart["hasData"] = false;
    } else {
        chart["hasData"] = true;
        build_program_vertices(*progChart, ROOM_TEMP_C, px, py);
        int32_t sched_end_sec = px.empty() ? 0 : px.back();
        float   scheduleEndC =
            py.empty() ? ROOM_TEMP_C : py.back() / static_cast<float>(kYScale);
        downsample_if_needed(px, py, kMaxSched);

        float total_min = ProfileGenerator::estimateTotalMinutes(*progChart, ROOM_TEMP_C);
        int32_t xmax_sec =
            static_cast<int32_t>(total_min * 60.f + 0.5f);
        if (xmax_sec < 1)
            xmax_sec = 1;
        if (!px.empty())
            xmax_sec = std::max(xmax_sec, px.back());
        xmax_sec = std::max(xmax_sec, sched_end_sec);

        int32_t ymin_c = static_cast<int32_t>(ROOM_TEMP_C * kYScale);
        int32_t ymax_c = ymin_c;
        for (size_t i = 0; i < py.size(); ++i) {
            ymin_c = std::min(ymin_c, py[i]);
            ymax_c = std::max(ymax_c, py[i]);
        }

        uint32_t elapsed = tv.status.totalTimeElapsed;
        if (st == KilnState::RAMPING || st == KilnState::SOAKING || st == KilnState::COOLING ||
            st == KilnState::DONE) {
            int32_t cur_y = static_cast<int32_t>(tv.status.currentTemperature * static_cast<float>(kYScale));
            ymin_c        = std::min(ymin_c, cur_y);
            ymax_c        = std::max(ymax_c, cur_y);
            int32_t tgt_y = static_cast<int32_t>(tv.status.targetTemperature * static_cast<float>(kYScale));
            ymin_c        = std::min(ymin_c, tgt_y);
            ymax_c        = std::max(ymax_c, tgt_y);
            xmax_sec      = std::max(xmax_sec, static_cast<int32_t>(elapsed) + 1);
        }

        chart["xMaxSec"] = xmax_sec;
        chart["yMinC"]   = ymin_c / static_cast<float>(kYScale);
        chart["yMaxC"]   = ymax_c / static_cast<float>(kYScale);

        JsonArray schedArr = chart["schedule"].to<JsonArray>();
        for (size_t i = 0; i < px.size(); ++i) {
            JsonObject p = schedArr.add<JsonObject>();
            p["t"]       = px[i];
            p["c"]       = py[i] / static_cast<float>(kYScale);
        }

        chart["elapsedSec"]    = elapsed;
        chart["actualTempC"]   = tv.status.currentTemperature;
        chart["targetTempC"]   = tv.status.targetTemperature;
        chart["scheduleEndSec"] = sched_end_sec;
        chart["scheduleEndC"]   = scheduleEndC;
    }

    out.clear();
    serializeJson(doc, out);
}

#include "kiln_programs_api.h"
#include "../model/app_state.h"
#include "../model/kiln_status.h"
#include "../model/program_selection_snapshot.h"
#include "../model/temp_units.h"

#include <ArduinoJson.h>
#include <cstring>
#include <string>

bool kiln_programs_selection_edit_allowed() {
    const KilnState st = appState.getStatus().currentState;
    return st == KilnState::IDLE || st == KilnState::ERROR;
}

void kiln_programs_serialize_json(String& out) {
    JsonDocument doc;

    doc["activeIndex"]            = appState.getActiveProgramIndex();
    doc["previousSelectionValid"] = g_previous_program_index.valid;
    const TempUnit tu             = appState.getTempUnit();
    doc["tempUnit"]               = unitSymbol(tu);

    JsonArray pre = doc["predefined"].to<JsonArray>();
    for (int i = 0; i < 4; ++i) {
        FiringProgram p{};
        if (!appState.tryCopyPredefinedProgram(static_cast<size_t>(i), &p))
            continue;
        JsonObject o = pre.add<JsonObject>();
        o["slot"]    = i;
        o["name"]    = p.name;
        o["cone"]    = p.origCone;
        o["candle"]  = p.origCandle;
        o["soak"]    = p.origSoak;
    }

    JsonArray cust = doc["custom"].to<JsonArray>();
    const size_t n = appState.getCustomProgramCount();
    for (size_t i = 0; i < n; ++i) {
        FiringProgram p{};
        if (!appState.tryCopyCustomProgram(i, &p))
            continue;
        JsonObject o = cust.add<JsonObject>();
        o["index"] = static_cast<int>(i);
        o["name"]  = p.name;
        JsonArray segs = o["segments"].to<JsonArray>();
        for (const auto& s : p.segments) {
            JsonObject sg = segs.add<JsonObject>();
            sg["target"]  = toDisplayTemp(s.targetTemperature, tu);
            sg["rate"]    = toDisplayRate(s.rampRate, tu);
            sg["soakMin"] = static_cast<uint32_t>(s.soakTime);
        }
    }

    serializeJson(doc, out);
}

bool kiln_programs_apply_save_json(const String& body, String& errJson) {
    if (!kiln_programs_selection_edit_allowed()) {
        errJson = "{\"error\":\"program locked while firing\"}";
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        errJson = "{\"error\":\"bad json\"}";
        return false;
    }

    if (doc["activeIndex"].isNull()) {
        errJson = "{\"error\":\"activeIndex required\"}";
        return false;
    }

    const int ai       = doc["activeIndex"].as<int>();
    const size_t nCust = appState.getCustomProgramCount();
    const int maxIdx   = static_cast<int>(3 + nCust);

    if (ai < 0 || ai > maxIdx) {
        errJson = "{\"error\":\"activeIndex out of range\"}";
        return false;
    }

    ProgramSelectionCommitDraft draft;
    draft.activeIndex = ai;

    if (ai <= 3) {
        std::string cone;
        if (!doc["cone"].isNull()) {
            const char* cs = doc["cone"].as<const char*>();
            if (cs)
                cone = cs;
        }
        const int candle = doc["candle"].is<int>() ? doc["candle"].as<int>()
                           : doc["candle"].is<float>() ? static_cast<int>(doc["candle"].as<float>())
                                                       : 0;
        const int soak = doc["soak"].is<int>() ? doc["soak"].as<int>()
                         : doc["soak"].is<float>() ? static_cast<int>(doc["soak"].as<float>())
                                                   : 0;

        if (cone.empty())
            cone = (ai <= 1) ? "08" : "5";

        draft.cone    = std::move(cone);
        draft.candle  = candle;
        draft.soak    = soak;
    }

    appState.commitProgramSelection(draft);
    return true;
}

#include "kiln_programs_api.h"
#include "../model/app_state.h"
#include "../model/kiln_status.h"
#include "../model/program_selection_snapshot.h"
#include "../model/temp_units.h"
#include "../service/preferences_persistence.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern PreferencesPersistence persistence;

namespace {

constexpr size_t kMaxCustomPrograms     = 16;
constexpr size_t kMaxSegmentsPerProgram = 32;

static bool try_parse_number(JsonVariantConst v, float& out) {
    if (v.isNull())
        return false;
    if (v.is<int>() || v.is<long>() || v.is<float>() || v.is<double>()) {
        out = v.as<float>();
        return true;
    }
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (!s)
            return false;
        char* end = nullptr;
        out       = strtof(s, &end);
        if (end == s)
            return false;
        while (*end && std::isspace(static_cast<unsigned char>(*end)))
            ++end;
        return *end == '\0';
    }
    return false;
}

static void set_range_error(String& errJson, size_t progI, const std::string& progName, size_t segI,
                            const char* field, float provided, float minAllowed, float maxAllowed,
                            const char* unit) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"error\":\"custom[%u] '%s' segment %u field %s provided %.2f %s; allowed %.2f to %.2f %s\"}",
             static_cast<unsigned>(progI + 1), progName.c_str(), static_cast<unsigned>(segI + 1), field, provided,
             unit, minAllowed, maxAllowed, unit);
    errJson = buf;
}

static void set_not_numeric_error(String& errJson, size_t progI, const std::string& progName, size_t segI,
                                  const char* field) {
    char buf[192];
    snprintf(buf, sizeof(buf), "{\"error\":\"custom[%u] '%s' segment %u field %s is not numeric\"}",
             static_cast<unsigned>(progI + 1), progName.c_str(), static_cast<unsigned>(segI + 1), field);
    errJson = buf;
}

/// Validates one segment's display fields against the same limits used by the touch popup
/// (1..1400 °C target, 1..999 °C/h ramp, 0..1440 min soak), and writes the result to `outSeg`.
static bool validate_segment_fields(JsonObjectConst sg, TempUnit tu, size_t progI, const std::string& progName,
                                    size_t segI, FiringSegment& outSeg, String& errJson) {
    float targetDisp = 0.f;
    if (!try_parse_number(sg["target"], targetDisp)) {
        set_not_numeric_error(errJson, progI, progName, segI, "target");
        return false;
    }
    const float targetMinDisp = toDisplayTemp(1.0f, tu);
    const float targetMaxDisp = toDisplayTemp(1400.0f, tu);
    if (targetDisp < targetMinDisp || targetDisp > targetMaxDisp) {
        set_range_error(errJson, progI, progName, segI, "target", targetDisp, targetMinDisp, targetMaxDisp,
                        unitSymbol(tu));
        return false;
    }

    float rateDisp = 0.f;
    if (!try_parse_number(sg["rate"], rateDisp)) {
        set_not_numeric_error(errJson, progI, progName, segI, "rate");
        return false;
    }
    const float rateMinDisp = toDisplayRate(1.0f, tu);
    const float rateMaxDisp = toDisplayRate(999.0f, tu);
    if (rateDisp < rateMinDisp || rateDisp > rateMaxDisp) {
        char unitBuf[8];
        snprintf(unitBuf, sizeof(unitBuf), "%s/h", unitSymbol(tu));
        set_range_error(errJson, progI, progName, segI, "rate", rateDisp, rateMinDisp, rateMaxDisp, unitBuf);
        return false;
    }

    float soakRaw = 0.f;
    if (!sg["soakMin"].isNull() && !try_parse_number(sg["soakMin"], soakRaw)) {
        set_not_numeric_error(errJson, progI, progName, segI, "soakMin");
        return false;
    }
    if (soakRaw < 0.f || soakRaw > 24.f * 60.f) {
        set_range_error(errJson, progI, progName, segI, "soakMin", soakRaw, 0.f, 24.f * 60.f, "min");
        return false;
    }

    outSeg.targetTemperature = fromDisplayTemp(targetDisp, tu);
    outSeg.rampRate          = fromDisplayRate(rateDisp, tu);
    outSeg.soakTime          = static_cast<uint32_t>(std::max(0.f, soakRaw));
    return true;
}

}  // namespace

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

    JsonArray     cust = doc["custom"].to<JsonArray>();
    const size_t  n    = appState.getCustomProgramCount();
    for (size_t i = 0; i < n; ++i) {
        FiringProgram p{};
        if (!appState.tryCopyCustomProgram(i, &p))
            continue;
        JsonObject o   = cust.add<JsonObject>();
        o["index"]     = static_cast<int>(i);
        o["name"]      = p.name;
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

    JsonDocument                   doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        errJson = "{\"error\":\"bad json\"}";
        return false;
    }

    if (doc["activeIndex"].isNull()) {
        errJson = "{\"error\":\"activeIndex required\"}";
        return false;
    }

    const int    ai     = doc["activeIndex"].as<int>();
    const size_t nCust  = appState.getCustomProgramCount();
    const int    maxIdx = static_cast<int>(3 + nCust);
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
                           : doc["candle"].is<float>()
                               ? static_cast<int>(doc["candle"].as<float>())
                               : 0;
        const int soak = doc["soak"].is<int>() ? doc["soak"].as<int>()
                         : doc["soak"].is<float>()
                             ? static_cast<int>(doc["soak"].as<float>())
                             : 0;

        if (cone.empty())
            cone = (ai <= 1) ? "08" : "5";

        draft.cone   = std::move(cone);
        draft.candle = candle;
        draft.soak   = soak;
    }

    appState.commitProgramSelection(draft);
    return true;
}

bool kiln_programs_append_custom_json(String& out, String& errJson) {
    if (!kiln_programs_selection_edit_allowed()) {
        errJson = "{\"error\":\"program locked while firing\"}";
        return false;
    }
    const size_t existing = appState.getCustomProgramCount();
    if (existing >= kMaxCustomPrograms) {
        errJson = "{\"error\":\"too many custom programs\"}";
        return false;
    }
    FiringProgram fp;
    fp.isCustom = true;
    fp.name     = "Custom " + std::to_string(existing + 1);
    appState.appendCustomProgram(fp);
    persistence.saveCustomPrograms();

    JsonDocument doc;
    doc["index"] = static_cast<int>(existing);
    doc["name"]  = fp.name;
    serializeJson(doc, out);
    return true;
}

bool kiln_programs_upsert_segment_json(const String& body, String& errJson) {
    if (!kiln_programs_selection_edit_allowed()) {
        errJson = "{\"error\":\"program locked while firing\"}";
        return false;
    }
    JsonDocument                   doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        errJson = "{\"error\":\"bad json\"}";
        return false;
    }
    if (doc["programIndex"].isNull() || doc["segmentIndex"].isNull()) {
        errJson = "{\"error\":\"programIndex and segmentIndex required\"}";
        return false;
    }
    const int    progIdx  = doc["programIndex"].as<int>();
    const int    segIdx   = doc["segmentIndex"].as<int>();
    const size_t nCust    = appState.getCustomProgramCount();
    if (progIdx < 0 || static_cast<size_t>(progIdx) >= nCust) {
        errJson = "{\"error\":\"programIndex out of range\"}";
        return false;
    }
    FiringProgram fp{};
    if (!appState.tryCopyCustomProgram(static_cast<size_t>(progIdx), &fp)) {
        errJson = "{\"error\":\"failed to load custom program\"}";
        return false;
    }
    if (segIdx != -1 && (segIdx < 0 || static_cast<size_t>(segIdx) >= fp.segments.size())) {
        errJson = "{\"error\":\"segmentIndex out of range\"}";
        return false;
    }

    const TempUnit  tu = appState.getTempUnit();
    FiringSegment   newSeg{};
    const size_t    segIdxForError = (segIdx == -1) ? fp.segments.size() : static_cast<size_t>(segIdx);
    JsonObjectConst sg             = doc.as<JsonObjectConst>();
    if (!validate_segment_fields(sg, tu, static_cast<size_t>(progIdx), fp.name, segIdxForError, newSeg, errJson))
        return false;

    if (segIdx == -1) {
        if (fp.segments.size() >= kMaxSegmentsPerProgram) {
            errJson = "{\"error\":\"too many segments in one program\"}";
            return false;
        }
        fp.segments.push_back(newSeg);
    } else {
        fp.segments[static_cast<size_t>(segIdx)] = newSeg;
    }
    appState.replaceCustomProgram(static_cast<size_t>(progIdx), std::move(fp));
    persistence.saveCustomPrograms();
    return true;
}

bool kiln_programs_delete_segment_json(const String& body, String& errJson) {
    if (!kiln_programs_selection_edit_allowed()) {
        errJson = "{\"error\":\"program locked while firing\"}";
        return false;
    }
    JsonDocument                   doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        errJson = "{\"error\":\"bad json\"}";
        return false;
    }
    if (doc["programIndex"].isNull() || doc["segmentIndex"].isNull()) {
        errJson = "{\"error\":\"programIndex and segmentIndex required\"}";
        return false;
    }
    const int progIdx = doc["programIndex"].as<int>();
    const int segIdx  = doc["segmentIndex"].as<int>();
    if (progIdx < 0 || segIdx < 0) {
        errJson = "{\"error\":\"indices must be non-negative\"}";
        return false;
    }
    if (!appState.eraseCustomSegment(static_cast<size_t>(progIdx), static_cast<size_t>(segIdx))) {
        errJson = "{\"error\":\"failed to erase segment (out of range)\"}";
        return false;
    }
    persistence.saveCustomPrograms();
    return true;
}

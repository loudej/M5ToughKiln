#include "kiln_settings_api.h"

#include "../model/app_state.h"
#include "../model/kiln_pid_gains.h"
#include "../model/temp_units.h"
#include "../service/preferences_persistence.h"

#include <ArduinoJson.h>

extern PreferencesPersistence persistence;

void kiln_settings_serialize_json(String& out) {
    JsonDocument doc;
    const KilnPidGains defs = kilnDefaultPidGains();
    const KilnPidGains ovVals = appState.getPidOvValues();
    const uint8_t       mask = appState.getPidOvMask();

    doc["tempUnit"] = unitSymbol(appState.getTempUnit());

    doc["kpDefault"] = defs.kp;
    doc["kiDefault"] = defs.ki;
    doc["kdDefault"] = defs.kd;

    if ((mask & kPidOvKp) != 0)
        doc["kp"] = ovVals.kp;
    else
        doc["kp"].set(nullptr);

    if ((mask & kPidOvKi) != 0)
        doc["ki"] = ovVals.ki;
    else
        doc["ki"].set(nullptr);

    if ((mask & kPidOvKd) != 0)
        doc["kd"] = ovVals.kd;
    else
        doc["kd"].set(nullptr);

    serializeJson(doc, out);
}

bool kiln_settings_apply_json(const String& body, String& errJson) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        errJson = "{\"error\":\"bad json\"}";
        return false;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();

    bool changed = false;

    JsonVariantConst vTu = root["tempUnit"];
    if (!vTu.isUnbound() && !vTu.isNull()) {
        const char* tu = vTu.as<const char*>();
        if (tu && tu[0] != '\0') {
            if (tu[0] == 'C' || tu[0] == 'c') {
                appState.setTempUnit(TempUnit::CELSIUS);
                changed = true;
            } else if (tu[0] == 'F' || tu[0] == 'f') {
                appState.setTempUnit(TempUnit::FAHRENHEIT);
                changed = true;
            }
        }
    }

    uint8_t      mask = appState.getPidOvMask();
    KilnPidGains ov   = appState.getPidOvValues();

    auto handle = [&](const char* key, uint8_t bit, float& slot) {
        JsonVariantConst v = root[key];
        if (v.isUnbound())
            return;
        if (v.isNull()) {
            mask &= static_cast<uint8_t>(~bit);
            changed = true;
            return;
        }
        mask |= bit;
        slot    = v.as<float>();
        changed = true;
    };

    handle("kp", kPidOvKp, ov.kp);
    handle("ki", kPidOvKi, ov.ki);
    handle("kd", kPidOvKd, ov.kd);

    appState.setPidOvState(mask, ov);

    if (changed)
        persistence.saveSettings();

    (void)errJson;
    return true;
}

#include "preferences_persistence.h"
#include "../model/kiln_pid_gains.h"
#include "../model/app_state.h"
#include "../model/profile_generator.h"
#include "../model/program_selection_snapshot.h"
#include "../model/temp_units.h"
#include <Arduino.h>

namespace {

void persist_pid_nv_block(Preferences& p, uint8_t mask, const KilnPidGains& ov) {
    KilnPidGains defs = kilnDefaultPidGains();
    mask &= (kPidOvKp | kPidOvKi | kPidOvKd);
    p.putUInt("pid_stor_ver", 2u);
    p.putUInt("pid_ov", mask);
    if (mask & kPidOvKp)
        p.putFloat("pid_o_kp", ov.kp);
    else
        p.remove("pid_o_kp");
    if (mask & kPidOvKi)
        p.putFloat("pid_o_ki", ov.ki);
    else
        p.remove("pid_o_ki");
    if (mask & kPidOvKd)
        p.putFloat("pid_o_kd", ov.kd);
    else
        p.remove("pid_o_kd");
    // legacy keys from older builds
    p.remove("pid_kp");
    p.remove("pid_ki");
    p.remove("pid_kd");
}

void load_previous_snapshot(Preferences& prefs) {
    g_previous_program_index.valid = prefs.getUInt("pv_valid", 0) != 0;
    if (!g_previous_program_index.valid)
        return;
    g_previous_program_index.programIndex = prefs.getInt("pv_act", 0);
}

void save_previous_snapshot(Preferences& prefs) {
    prefs.putUInt("pv_valid", g_previous_program_index.valid ? 1u : 0u);
    if (!g_previous_program_index.valid)
        return;

    prefs.putInt("pv_act", g_previous_program_index.programIndex);
}

} // namespace

bool PreferencesPersistence::loadCustomPrograms() {
    prefs.begin(NVS_NAMESPACE, true);

    AppState::LoadedProgramsBundle bundle;
    bundle.customPrograms.clear();

    int count = prefs.getInt("custom_count", 0);
    for (int i = 0; i < count; i++) {
        FiringProgram p;
        p.isCustom = true;

        char key[16];
        sprintf(key, "p%d_name", i);
        p.name = prefs.getString(key, "Custom").c_str();

        sprintf(key, "p%d_seg_cnt", i);
        int segCount = prefs.getInt(key, 0);

        for (int j = 0; j < segCount; j++) {
            FiringSegment seg;
            sprintf(key, "p%d_s%d_tt", i, j);
            seg.targetTemperature = prefs.getFloat(key, 0);

            sprintf(key, "p%d_s%d_rr", i, j);
            seg.rampRate = prefs.getFloat(key, 0);

            sprintf(key, "p%d_s%d_st", i, j);
            seg.soakTime = prefs.getUInt(key, 0);

            p.segments.push_back(seg);
        }
        bundle.customPrograms.push_back(p);
    }

    bundle.activeProgramIndex = prefs.getInt("active_prog", 0);
    if (bundle.activeProgramIndex < 0)
        bundle.activeProgramIndex = 0;

    bundle.predefinedPrograms.resize(4);
    for (int i = 0; i < 4; i++) {
        char key[16];
        sprintf(key, "pre%d_cone", i);
        bundle.predefinedPrograms[i].origCone = prefs.getString(key, i < 2 ? "08" : "5").c_str();
        sprintf(key, "pre%d_candle", i);
        bundle.predefinedPrograms[i].origCandle = prefs.getInt(key, 0);
        sprintf(key, "pre%d_soak", i);
        bundle.predefinedPrograms[i].origSoak = prefs.getInt(key, 0);

        if (i == 0)
            bundle.predefinedPrograms[0] = ProfileGenerator::generateFastBisque(
                bundle.predefinedPrograms[0].origCone, bundle.predefinedPrograms[0].origCandle,
                bundle.predefinedPrograms[0].origSoak);
        else if (i == 1)
            bundle.predefinedPrograms[1] = ProfileGenerator::generateSlowBisque(
                bundle.predefinedPrograms[1].origCone, bundle.predefinedPrograms[1].origCandle,
                bundle.predefinedPrograms[1].origSoak);
        else if (i == 2)
            bundle.predefinedPrograms[2] = ProfileGenerator::generateFastGlaze(
                bundle.predefinedPrograms[2].origCone, bundle.predefinedPrograms[2].origCandle,
                bundle.predefinedPrograms[2].origSoak);
        else if (i == 3)
            bundle.predefinedPrograms[3] = ProfileGenerator::generateSlowGlaze(
                bundle.predefinedPrograms[3].origCone, bundle.predefinedPrograms[3].origCandle,
                bundle.predefinedPrograms[3].origSoak);
    }

    load_previous_snapshot(prefs);
    prefs.end();

    appState.commitLoadedPrograms(std::move(bundle));
    return true;
}

bool PreferencesPersistence::saveCustomPrograms() {
    const AppState::PersistSnapshot snap = appState.snapshotForPersist();

    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();

    prefs.putInt("custom_count", (int)snap.customPrograms.size());
    for (int i = 0; i < (int)snap.customPrograms.size(); i++) {
        const FiringProgram& p = snap.customPrograms[(size_t)i];

        char key[16];
        sprintf(key, "p%d_name", i);
        prefs.putString(key, p.name.c_str());

        sprintf(key, "p%d_seg_cnt", i);
        prefs.putInt(key, (int)p.segments.size());

        for (int j = 0; j < (int)p.segments.size(); j++) {
            const FiringSegment& seg = p.segments[(size_t)j];

            sprintf(key, "p%d_s%d_tt", i, j);
            prefs.putFloat(key, seg.targetTemperature);

            sprintf(key, "p%d_s%d_rr", i, j);
            prefs.putFloat(key, seg.rampRate);

            sprintf(key, "p%d_s%d_st", i, j);
            prefs.putUInt(key, seg.soakTime);
        }
    }

    prefs.putInt("active_prog", snap.activeProgramIndex);

    for (int i = 0; i < 4; i++) {
        char key[16];
        sprintf(key, "pre%d_cone", i);
        prefs.putString(key, snap.predefinedPrograms[(size_t)i].origCone.c_str());
        sprintf(key, "pre%d_candle", i);
        prefs.putInt(key, snap.predefinedPrograms[(size_t)i].origCandle);
        sprintf(key, "pre%d_soak", i);
        prefs.putInt(key, snap.predefinedPrograms[(size_t)i].origSoak);
    }

    save_previous_snapshot(prefs);

    prefs.end();
    return true;
}

bool PreferencesPersistence::loadSettings() {
    prefs.begin(NVS_SETTINGS_NAMESPACE, true);
    const int unit = prefs.getInt("temp_unit", 0);
    appState.setTempUnit((unit == 1) ? TempUnit::CELSIUS : TempUnit::FAHRENHEIT);

    const uint32_t storVer = prefs.getUInt("pid_stor_ver", 0);
    KilnPidGains   defs    = kilnDefaultPidGains();
    uint8_t        mask    = 0;
    KilnPidGains   ov{};

    if (storVer < 2u) {
        if (prefs.isKey("pid_kp")) {
            mask |= kPidOvKp;
            ov.kp = prefs.getFloat("pid_kp", defs.kp);
        }
        if (prefs.isKey("pid_ki")) {
            mask |= kPidOvKi;
            ov.ki = prefs.getFloat("pid_ki", defs.ki);
        }
        if (prefs.isKey("pid_kd")) {
            mask |= kPidOvKd;
            ov.kd = prefs.getFloat("pid_kd", defs.kd);
        }
    } else {
        mask =
            static_cast<uint8_t>(prefs.getUInt("pid_ov", 0) & (kPidOvKp | kPidOvKi | kPidOvKd));
        if (mask & kPidOvKp)
            ov.kp = prefs.getFloat("pid_o_kp", defs.kp);
        if (mask & kPidOvKi)
            ov.ki = prefs.getFloat("pid_o_ki", defs.ki);
        if (mask & kPidOvKd)
            ov.kd = prefs.getFloat("pid_o_kd", defs.kd);
    }

    prefs.end();

    appState.setPidOvState(mask, ov);

    if (storVer < 2u) {
        prefs.begin(NVS_SETTINGS_NAMESPACE, false);
        persist_pid_nv_block(prefs, mask, ov);
        prefs.end();
    }
    return true;
}

bool PreferencesPersistence::saveSettings() {
    const TempUnit       u       = appState.getTempUnit();
    const uint8_t        mask    = appState.getPidOvMask();
    const KilnPidGains ov = appState.getPidOvValues();
    prefs.begin(NVS_SETTINGS_NAMESPACE, false);
    prefs.putInt("temp_unit", u == TempUnit::CELSIUS ? 1 : 0);
    persist_pid_nv_block(prefs, mask, ov);
    prefs.end();
    return true;
}

std::string PreferencesPersistence::loadWifiSsid() {
    prefs.begin(NVS_SETTINGS_NAMESPACE, true);
    String s = prefs.getString("wifi_ssid", "");
    prefs.end();
    return std::string(s.c_str());
}

std::string PreferencesPersistence::loadWifiPass() {
    prefs.begin(NVS_SETTINGS_NAMESPACE, true);
    String s = prefs.getString("wifi_pass", "");
    prefs.end();
    return std::string(s.c_str());
}

void PreferencesPersistence::saveWifiCredentials(const std::string& ssid, const std::string& pass) {
    prefs.begin(NVS_SETTINGS_NAMESPACE, false);
    prefs.putString("wifi_ssid", ssid.c_str());
    prefs.putString("wifi_pass", pass.c_str());
    prefs.end();
}

#include "preferences_persistence.h"
#include "../model/profile_generator.h"
#include "../model/program_selection_snapshot.h"
#include "../model/temp_units.h"
#include <Arduino.h>

namespace {

void load_previous_snapshot(Preferences& prefs) {
    ProgramSelectionSnapshot s;
    s.valid = prefs.getUInt("pv_valid", 0) != 0;
    if (!s.valid) {
        g_previous_program_selection = s;
        return;
    }
    s.activeProgramIndex = prefs.getInt("pv_act", 0);

    for (int i = 0; i < 4; ++i) {
        char key[20];
        sprintf(key, "pv_pre%d_cone", i);
        s.preCone[i] = prefs.getString(key, "").c_str();
        sprintf(key, "pv_pre%d_candle", i);
        s.preCandle[i] = prefs.getInt(key, 0);
        sprintf(key, "pv_pre%d_soak", i);
        s.preSoak[i] = prefs.getInt(key, 0);
    }

    s.hasCustomSlot = prefs.getUInt("pv_has_custom", 0) != 0;
    s.customIndex   = prefs.getInt("pv_custom_idx", 0);

    if (s.hasCustomSlot) {
        FiringProgram& cp = s.customCopy;
        cp.name      = prefs.getString("pv_c_name", "").c_str();
        cp.isCustom  = prefs.getUInt("pv_c_iscustom", 1) != 0;
        cp.origCone  = prefs.getString("pv_c_ocone", "").c_str();
        cp.origCandle = prefs.getInt("pv_c_ocandle", 0);
        cp.origSoak   = prefs.getInt("pv_c_osoak", 0);

        cp.segments.clear();
        int segCount = prefs.getInt("pv_c_seg_cnt", 0);
        char segKey[20];
        for (int j = 0; j < segCount; j++) {
            FiringSegment seg;
            sprintf(segKey, "pv_c_s%d_tt", j);
            seg.targetTemperature = prefs.getFloat(segKey, 0);
            sprintf(segKey, "pv_c_s%d_rr", j);
            seg.rampRate = prefs.getFloat(segKey, 0);
            sprintf(segKey, "pv_c_s%d_st", j);
            seg.soakTime = prefs.getUInt(segKey, 0);
            cp.segments.push_back(seg);
        }
    }

    g_previous_program_selection = s;
}

void save_previous_snapshot(Preferences& prefs) {
    const ProgramSelectionSnapshot& sp = g_previous_program_selection;
    prefs.putUInt("pv_valid", sp.valid ? 1u : 0u);
    if (!sp.valid)
        return;

    prefs.putInt("pv_act", sp.activeProgramIndex);

    for (int i = 0; i < 4; ++i) {
        char key[20];
        sprintf(key, "pv_pre%d_cone", i);
        prefs.putString(key, sp.preCone[i].c_str());
        sprintf(key, "pv_pre%d_candle", i);
        prefs.putInt(key, sp.preCandle[i]);
        sprintf(key, "pv_pre%d_soak", i);
        prefs.putInt(key, sp.preSoak[i]);
    }

    prefs.putUInt("pv_has_custom", sp.hasCustomSlot ? 1u : 0u);
    prefs.putInt("pv_custom_idx", sp.customIndex);

    const FiringProgram& cp = sp.customCopy;
    prefs.putString("pv_c_name", cp.name.c_str());
    prefs.putUInt("pv_c_iscustom", cp.isCustom ? 1u : 0u);
    prefs.putString("pv_c_ocone", cp.origCone.c_str());
    prefs.putInt("pv_c_ocandle", cp.origCandle);
    prefs.putInt("pv_c_osoak", cp.origSoak);
    prefs.putInt("pv_c_seg_cnt", cp.segments.size());

    for (size_t j = 0; j < cp.segments.size(); j++) {
        const FiringSegment& seg = cp.segments[j];
        char key[20];
        sprintf(key, "pv_c_s%u_tt", (unsigned)j);
        prefs.putFloat(key, seg.targetTemperature);
        sprintf(key, "pv_c_s%u_rr", (unsigned)j);
        prefs.putFloat(key, seg.rampRate);
        sprintf(key, "pv_c_s%u_st", (unsigned)j);
        prefs.putUInt(key, seg.soakTime);
    }
}

} // namespace

bool PreferencesPersistence::loadCustomPrograms(std::vector<FiringProgram>& programs) {
    programs.clear();
    prefs.begin(NVS_NAMESPACE, true); // read-only

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
        programs.push_back(p);
    }

    // Load active program and predefined states
    appState.activeProgramIndex = prefs.getInt("active_prog", 0);
    if (appState.activeProgramIndex < 0) appState.activeProgramIndex = 0;
    
    for (int i = 0; i < 4; i++) {
        char key[16];
        sprintf(key, "pre%d_cone", i);
        appState.predefinedPrograms[i].origCone = prefs.getString(key, i < 2 ? "08" : "5").c_str();
        sprintf(key, "pre%d_candle", i);
        appState.predefinedPrograms[i].origCandle = prefs.getInt(key, 0);
        sprintf(key, "pre%d_soak", i);
        appState.predefinedPrograms[i].origSoak = prefs.getInt(key, 0);
        
        // Regenerate the program logic based on the loaded inputs
        if (i == 0) appState.predefinedPrograms[0] = ProfileGenerator::generateFastBisque(appState.predefinedPrograms[0].origCone, appState.predefinedPrograms[0].origCandle, appState.predefinedPrograms[0].origSoak);
        else if (i == 1) appState.predefinedPrograms[1] = ProfileGenerator::generateSlowBisque(appState.predefinedPrograms[1].origCone, appState.predefinedPrograms[1].origCandle, appState.predefinedPrograms[1].origSoak);
        else if (i == 2) appState.predefinedPrograms[2] = ProfileGenerator::generateFastGlaze(appState.predefinedPrograms[2].origCone, appState.predefinedPrograms[2].origCandle, appState.predefinedPrograms[2].origSoak);
        else if (i == 3) appState.predefinedPrograms[3] = ProfileGenerator::generateSlowGlaze(appState.predefinedPrograms[3].origCone, appState.predefinedPrograms[3].origCandle, appState.predefinedPrograms[3].origSoak);
    }

    if (appState.activeProgramIndex <= 3) {
        appState.status.activeProgramName = appState.predefinedPrograms[appState.activeProgramIndex].name;
    } else {
        int customIdx = appState.activeProgramIndex - 4;
        if (customIdx >= 0 && customIdx < appState.customPrograms.size()) {
            appState.status.activeProgramName = appState.customPrograms[customIdx].name;
        } else {
             appState.activeProgramIndex = 0;
             appState.status.activeProgramName = appState.predefinedPrograms[0].name;
        }
    }

    load_previous_snapshot(prefs);

    prefs.end();
    return true;
}

bool PreferencesPersistence::saveCustomPrograms(const std::vector<FiringProgram>& programs) {
    prefs.begin(NVS_NAMESPACE, false); // read-write
    prefs.clear(); // Clear old data

    prefs.putInt("custom_count", programs.size());
    for (int i = 0; i < programs.size(); i++) {
        const FiringProgram& p = programs[i];
        
        char key[16];
        sprintf(key, "p%d_name", i);
        prefs.putString(key, p.name.c_str());

        sprintf(key, "p%d_seg_cnt", i);
        prefs.putInt(key, p.segments.size());

        for (int j = 0; j < p.segments.size(); j++) {
            const FiringSegment& seg = p.segments[j];
            
            sprintf(key, "p%d_s%d_tt", i, j);
            prefs.putFloat(key, seg.targetTemperature);
            
            sprintf(key, "p%d_s%d_rr", i, j);
            prefs.putFloat(key, seg.rampRate);
            
            sprintf(key, "p%d_s%d_st", i, j);
            prefs.putUInt(key, seg.soakTime);
        }
    }

    // Save active program and predefined states
    prefs.putInt("active_prog", appState.activeProgramIndex);
    
    for (int i = 0; i < 4; i++) {
        char key[16];
        sprintf(key, "pre%d_cone", i);
        prefs.putString(key, appState.predefinedPrograms[i].origCone.c_str());
        sprintf(key, "pre%d_candle", i);
        prefs.putInt(key, appState.predefinedPrograms[i].origCandle);
        sprintf(key, "pre%d_soak", i);
        prefs.putInt(key, appState.predefinedPrograms[i].origSoak);
    }

    save_previous_snapshot(prefs);

    prefs.end();
    return true;
}

bool PreferencesPersistence::loadSettings() {
    prefs.begin(NVS_SETTINGS_NAMESPACE, true);
    int unit = prefs.getInt("temp_unit", 0); // 0 = Fahrenheit (default), 1 = Celsius
    appState.tempUnit = (unit == 1) ? TempUnit::CELSIUS : TempUnit::FAHRENHEIT;
    prefs.end();
    return true;
}

bool PreferencesPersistence::saveSettings() {
    prefs.begin(NVS_SETTINGS_NAMESPACE, false);
    prefs.putInt("temp_unit", appState.tempUnit == TempUnit::CELSIUS ? 1 : 0);
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

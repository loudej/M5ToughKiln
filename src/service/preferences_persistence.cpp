#include "preferences_persistence.h"
#include "../model/profile_generator.h"
#include <Arduino.h>

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

    prefs.end();
    return true;
}

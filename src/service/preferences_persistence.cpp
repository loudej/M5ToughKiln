#include "preferences_persistence.h"
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

    prefs.end();
    return true;
}

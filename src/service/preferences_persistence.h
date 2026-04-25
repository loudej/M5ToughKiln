#ifndef PREFERENCES_PERSISTENCE_H
#define PREFERENCES_PERSISTENCE_H

#include "persistence_service.h"
#include <Preferences.h>

class PreferencesPersistence : public IPersistenceService {
private:
    Preferences prefs;
    const char* NVS_NAMESPACE          = "kiln_app";
    const char* NVS_SETTINGS_NAMESPACE = "kiln_cfg";

public:
    bool loadCustomPrograms(std::vector<FiringProgram>& programs) override;
    bool saveCustomPrograms(const std::vector<FiringProgram>& programs) override;

    bool loadSettings();
    bool saveSettings();
};

#endif // PREFERENCES_PERSISTENCE_H

#ifndef PREFERENCES_PERSISTENCE_H
#define PREFERENCES_PERSISTENCE_H

#include "persistence_service.h"
#include <Preferences.h>
#include <string>

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

    std::string loadWifiSsid();
    std::string loadWifiPass();
    void        saveWifiCredentials(const std::string& ssid, const std::string& pass);
};

#endif // PREFERENCES_PERSISTENCE_H

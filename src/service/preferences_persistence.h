#ifndef PREFERENCES_PERSISTENCE_H
#define PREFERENCES_PERSISTENCE_H

#include "persistence_service.h"
#include <Preferences.h>

class PreferencesPersistence : public IPersistenceService {
private:
    Preferences prefs;
    const char* NVS_NAMESPACE = "kiln_app";

public:
    bool loadCustomPrograms(std::vector<FiringProgram>& programs) override;
    bool saveCustomPrograms(const std::vector<FiringProgram>& programs) override;
};

#endif // PREFERENCES_PERSISTENCE_H

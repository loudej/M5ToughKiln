#ifndef PERSISTENCE_SERVICE_H
#define PERSISTENCE_SERVICE_H

#include "../model/firing_program.h"

// Interface for a service that can save and load the application state.
class IPersistenceService {
public:
    virtual ~IPersistenceService() = default;

    /// Loads custom + predefined programs and active index into `appState` (single commit).
    virtual bool loadCustomPrograms() = 0;

    /// Persists current `appState` program data to non-volatile memory.
    virtual bool saveCustomPrograms() = 0;
};

#endif // PERSISTENCE_SERVICE_H

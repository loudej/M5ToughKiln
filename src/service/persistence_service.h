#ifndef PERSISTENCE_SERVICE_H
#define PERSISTENCE_SERVICE_H

#include "../model/app_state.h"

// Interface for a service that can save and load the application state.
class IPersistenceService {
public:
    virtual ~IPersistenceService() = default;

    // Load the custom firing programs from non-volatile memory.
    virtual bool loadCustomPrograms(std::vector<FiringProgram>& programs) = 0;

    // Save the custom firing programs to non-volatile memory.
    virtual bool saveCustomPrograms(const std::vector<FiringProgram>& programs) = 0;
};

#endif // PERSISTENCE_SERVICE_H

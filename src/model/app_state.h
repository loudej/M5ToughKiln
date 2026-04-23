#ifndef APP_STATE_H
#define APP_STATE_H

#include "kiln_status.h"
#include "firing_program.h"
#include <vector>

struct AppState {
    KilnStatus status;
    std::vector<FiringProgram> predefinedPrograms;
    std::vector<FiringProgram> customPrograms;
    int activeProgramIndex = -1;
};

// Global application state instance
extern AppState appState;

#endif // APP_STATE_H

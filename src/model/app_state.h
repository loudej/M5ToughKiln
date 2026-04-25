#ifndef APP_STATE_H
#define APP_STATE_H

#include "kiln_status.h"
#include "firing_program.h"
#include "temp_units.h"
#include <vector>

struct AppState {
    KilnStatus status;
    std::vector<FiringProgram> predefinedPrograms;
    std::vector<FiringProgram> customPrograms;
    int activeProgramIndex = -1;
    TempUnit tempUnit = TempUnit::FAHRENHEIT;
};

// Global application state instance
extern AppState appState;

void app_state_init();

#endif // APP_STATE_H

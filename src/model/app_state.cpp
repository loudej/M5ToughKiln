#include "app_state.h"
#include "profile_generator.h"

// Define the global application state instance
AppState appState;

void app_state_init() {
    // Generate empty templates for the predefined programs
    appState.predefinedPrograms.push_back(ProfileGenerator::generateFastBisque("04", 0, 0));
    appState.predefinedPrograms.push_back(ProfileGenerator::generateSlowBisque("04", 0, 0));
    appState.predefinedPrograms.push_back(ProfileGenerator::generateFastGlaze("6", 0, 0));
    appState.predefinedPrograms.push_back(ProfileGenerator::generateSlowGlaze("6", 0, 0));
    
    // Set default active
    appState.activeProgramIndex = 0;
    appState.status.activeProgramName = appState.predefinedPrograms[0].name;
}

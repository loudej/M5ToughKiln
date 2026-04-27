#include "app_state.h"
#include "profile_generator.h"

// Define the global application state instance
AppState appState;

FiringProgram* AppState::activeProgram() {
    if (activeProgramIndex < 0) return nullptr;
    if (activeProgramIndex < (int)predefinedPrograms.size()) {
        return &predefinedPrograms[activeProgramIndex];
    }
    const int customIdx = activeProgramIndex - (int)predefinedPrograms.size();
    if (customIdx >= 0 && customIdx < (int)customPrograms.size()) {
        return &customPrograms[customIdx];
    }
    return nullptr;
}

const FiringProgram* AppState::activeProgram() const {
    if (activeProgramIndex < 0) return nullptr;
    if (activeProgramIndex < (int)predefinedPrograms.size()) {
        return &predefinedPrograms[activeProgramIndex];
    }
    const int customIdx = activeProgramIndex - (int)predefinedPrograms.size();
    if (customIdx >= 0 && customIdx < (int)customPrograms.size()) {
        return &customPrograms[customIdx];
    }
    return nullptr;
}

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

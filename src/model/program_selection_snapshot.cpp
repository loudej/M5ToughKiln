#include "program_selection_snapshot.h"
#include "app_state.h"
#include "profile_generator.h"
#include "service/preferences_persistence.h"

extern PreferencesPersistence persistence;

ProgramSelectionSnapshot g_previous_program_selection;

void ProgramSelectionSnapshot::captureFrom(const AppState& app) {
    app.captureIntoSelectionSnapshot(this);
}

void ProgramSelectionSnapshot::applyTo(AppState& app) const {
    app.applySelectionSnapshot(*this);
}

void program_selection_capture_current_as_previous() {
    appState.captureIntoSelectionSnapshot(&g_previous_program_selection);
}

void program_selection_swap_with_previous() {
    if (!g_previous_program_selection.valid)
        return;

    ProgramSelectionSnapshot cur;
    appState.captureIntoSelectionSnapshot(&cur);

    g_previous_program_selection.applyTo(appState);
    g_previous_program_selection = cur;

    persistence.saveCustomPrograms();
}

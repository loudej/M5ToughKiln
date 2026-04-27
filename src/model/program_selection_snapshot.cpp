#include "program_selection_snapshot.h"
#include "app_state.h"
#include "profile_generator.h"
#include "service/preferences_persistence.h"

extern PreferencesPersistence persistence;

ProgramSelectionSnapshot g_previous_program_selection;

static void regeneratePredefinedSlot(AppState& app, int slot) {
    const std::string& cone = app.predefinedPrograms[slot].origCone;
    const int          cand = app.predefinedPrograms[slot].origCandle;
    const int          soak = app.predefinedPrograms[slot].origSoak;
    switch (slot) {
        case 0:
            app.predefinedPrograms[0] =
                ProfileGenerator::generateFastBisque(cone, cand, soak);
            break;
        case 1:
            app.predefinedPrograms[1] =
                ProfileGenerator::generateSlowBisque(cone, cand, soak);
            break;
        case 2:
            app.predefinedPrograms[2] =
                ProfileGenerator::generateFastGlaze(cone, cand, soak);
            break;
        case 3:
            app.predefinedPrograms[3] =
                ProfileGenerator::generateSlowGlaze(cone, cand, soak);
            break;
        default:
            break;
    }
}

void ProgramSelectionSnapshot::captureFrom(const AppState& app) {
    activeProgramIndex = app.activeProgramIndex;
    valid              = true;

    for (int i = 0; i < 4; ++i) {
        preCone[i]   = app.predefinedPrograms[i].origCone;
        preCandle[i] = app.predefinedPrograms[i].origCandle;
        preSoak[i]   = app.predefinedPrograms[i].origSoak;
    }

    hasCustomSlot = false;
    customIndex   = 0;
    customCopy    = {};

    if (activeProgramIndex >= 4) {
        const size_t ci = static_cast<size_t>(activeProgramIndex - 4);
        if (ci < app.customPrograms.size()) {
            hasCustomSlot = true;
            customIndex   = static_cast<int>(ci);
            customCopy    = app.customPrograms[ci];
        }
    }
}

void ProgramSelectionSnapshot::applyTo(AppState& app) const {
    if (!valid)
        return;

    for (int i = 0; i < 4; ++i) {
        app.predefinedPrograms[i].origCone   = preCone[i];
        app.predefinedPrograms[i].origCandle = preCandle[i];
        app.predefinedPrograms[i].origSoak   = preSoak[i];
        regeneratePredefinedSlot(app, i);
    }

    if (hasCustomSlot && customIndex >= 0) {
        const size_t need = static_cast<size_t>(customIndex) + 1;
        while (app.customPrograms.size() < need) {
            app.customPrograms.push_back(FiringProgram{});
        }
        app.customPrograms[static_cast<size_t>(customIndex)] = customCopy;
    }

    app.activeProgramIndex = activeProgramIndex;

    if (activeProgramIndex >= 0 && activeProgramIndex <= 3) {
        app.status.activeProgramName = app.predefinedPrograms[activeProgramIndex].name;
    } else if (activeProgramIndex >= 4) {
        const size_t ci = static_cast<size_t>(activeProgramIndex - 4);
        if (ci < app.customPrograms.size()) {
            app.status.activeProgramName = app.customPrograms[ci].name;
        }
    }
}

void program_selection_capture_current_as_previous() {
    g_previous_program_selection.captureFrom(appState);
}

void program_selection_swap_with_previous() {
    if (!g_previous_program_selection.valid)
        return;

    ProgramSelectionSnapshot cur;
    cur.captureFrom(appState);

    g_previous_program_selection.applyTo(appState);
    g_previous_program_selection = cur;

    persistence.saveCustomPrograms(appState.customPrograms);
}

#ifndef PROGRAM_SELECTION_SNAPSHOT_H
#define PROGRAM_SELECTION_SNAPSHOT_H

#include "app_state.h"
#include "firing_program.h"

/// Full snapshot of which program is active and all parameters needed to restore
/// predefined generator inputs and the selected custom program body.
struct ProgramSelectionSnapshot {
    int  activeProgramIndex = 0;
    bool valid              = false;

    std::string preCone[4];
    int         preCandle[4]{};
    int         preSoak[4]{};

    bool          hasCustomSlot = false;
    int           customIndex   = 0;
    FiringProgram customCopy{};

    void captureFrom(const AppState& app);
    void applyTo(AppState& app) const;
};

extern ProgramSelectionSnapshot g_previous_program_selection;

void program_selection_capture_current_as_previous();
void program_selection_swap_with_previous();

#endif

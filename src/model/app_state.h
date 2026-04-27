#ifndef APP_STATE_H
#define APP_STATE_H

#include "kiln_status.h"
#include "firing_program.h"
#include "temp_units.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include <vector>

struct ProgramSelectionSnapshot;

/// Protects `appState`. Any task may call the public getters/setters on `AppState`;
/// each acquires this mutex only for the duration of the copy. `FiringController::update()`
/// runs on core 1 while holding this mutex and uses private member access (friend).
extern SemaphoreHandle_t g_state_mutex;

class FiringController;

class AppState {
    friend class FiringController;

    KilnStatus                status_{};
    std::vector<FiringProgram> predefinedPrograms_{};
    std::vector<FiringProgram> customPrograms_{};
    int          activeProgramIndex_{0};
    TempUnit     tempUnit_{TempUnit::FAHRENHEIT};

    FiringProgram*       mutableActiveProgram();
    const FiringProgram* mutableActiveProgram() const;

    void syncActiveProgramNameWithIndex();
    void regeneratePredefinedSlotLocked(int slot);

public:
    /// Single copy for UI / HTTP hot paths (one lock).
    struct TelemetryView {
        KilnStatus status;
        TempUnit   tempUnit{};
    };

    /// NVS load applies these in one critical section.
    struct LoadedProgramsBundle {
        std::vector<FiringProgram> customPrograms;
        std::vector<FiringProgram> predefinedPrograms;
        int                        activeProgramIndex = 0;
    };

    /// NVS save reads these under one lock.
    struct PersistSnapshot {
        std::vector<FiringProgram> customPrograms;
        std::vector<FiringProgram> predefinedPrograms;
        int                        activeProgramIndex = 0;
    };

    KilnStatus getStatus() const;
    TempUnit   getTempUnit() const;
    int        getActiveProgramIndex() const;

    TelemetryView getTelemetryView() const;

    bool tryCopyActiveProgram(FiringProgram* out) const;
    bool tryCopyPredefinedProgram(size_t idx, FiringProgram* out) const;
    bool tryCopyCustomProgram(size_t idx, FiringProgram* out) const;

    std::vector<std::string> getCustomProgramNames() const;
    size_t                   getCustomProgramCount() const;

    void setTempUnit(TempUnit u);
    void setActiveProgramIndex(int idx);
    void setActiveProgramName(std::string name);

    /// Main screen START / STOP / RESET tap (same semantics as previous inline logic).
    void applyStartStopTap();

    void setPredefinedProgram(size_t idx, FiringProgram prog);
    void appendCustomProgram(FiringProgram prog);
    /// Replaces one custom slot; returns false if index out of range.
    bool replaceCustomProgram(size_t idx, FiringProgram prog);
    void replaceCustomPrograms(std::vector<FiringProgram> programs);
    bool eraseCustomSegment(size_t programIndex, size_t segmentIndex);

    void setKilnStateFromProgramConfigStart();

    void captureIntoSelectionSnapshot(ProgramSelectionSnapshot* dst) const;
    void applySelectionSnapshot(const ProgramSelectionSnapshot& src);

    void commitLoadedPrograms(LoadedProgramsBundle bundle);

    PersistSnapshot snapshotForPersist() const;
};

extern AppState appState;

void app_state_mutex_init();
void app_state_init();

#endif

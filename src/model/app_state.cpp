#include "app_state.h"
#include "program_selection_snapshot.h"
#include "profile_generator.h"

#include <algorithm>
#include <cstring>

SemaphoreHandle_t g_state_mutex = nullptr;

namespace {

inline void lockSt() {
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
}

inline void unlockSt() {
    xSemaphoreGive(g_state_mutex);
}

}  // namespace

AppState appState;

void app_state_mutex_init() {
    if (g_state_mutex)
        return;
    g_state_mutex = xSemaphoreCreateMutex();
    configASSERT(g_state_mutex);
}

void AppState::regeneratePredefinedSlotLocked(int slot) {
    if (slot < 0 || slot >= (int)predefinedPrograms_.size())
        return;

    const std::string& cone = predefinedPrograms_[slot].origCone;
    const int          cand = predefinedPrograms_[slot].origCandle;
    const int          soak = predefinedPrograms_[slot].origSoak;
    switch (slot) {
        case 0:
            predefinedPrograms_[0] = ProfileGenerator::generateFastBisque(cone, cand, soak);
            break;
        case 1:
            predefinedPrograms_[1] = ProfileGenerator::generateSlowBisque(cone, cand, soak);
            break;
        case 2:
            predefinedPrograms_[2] = ProfileGenerator::generateFastGlaze(cone, cand, soak);
            break;
        case 3:
            predefinedPrograms_[3] = ProfileGenerator::generateSlowGlaze(cone, cand, soak);
            break;
        default:
            break;
    }
}

void AppState::syncActiveProgramNameWithIndex() {
    if (activeProgramIndex_ <= 3 && activeProgramIndex_ >= 0 &&
        activeProgramIndex_ < (int)predefinedPrograms_.size()) {
        status_.activeProgramName = predefinedPrograms_[activeProgramIndex_].name;
    } else if (activeProgramIndex_ >= 4) {
        const int ci = activeProgramIndex_ - 4;
        if (ci >= 0 && ci < (int)customPrograms_.size())
            status_.activeProgramName = customPrograms_[(size_t)ci].name;
    }
}

FiringProgram* AppState::mutableActiveProgram() {
    if (activeProgramIndex_ < 0)
        return nullptr;
    if (activeProgramIndex_ < (int)predefinedPrograms_.size()) {
        return &predefinedPrograms_[activeProgramIndex_];
    }
    const int customIdx = activeProgramIndex_ - (int)predefinedPrograms_.size();
    if (customIdx >= 0 && customIdx < (int)customPrograms_.size())
        return &customPrograms_[customIdx];
    return nullptr;
}

const FiringProgram* AppState::mutableActiveProgram() const {
    return const_cast<AppState*>(this)->mutableActiveProgram();
}

void app_state_init() {
    AppState::LoadedProgramsBundle b;
    b.customPrograms.clear();
    b.predefinedPrograms.push_back(ProfileGenerator::generateFastBisque("04", 0, 0));
    b.predefinedPrograms.push_back(ProfileGenerator::generateSlowBisque("04", 0, 0));
    b.predefinedPrograms.push_back(ProfileGenerator::generateFastGlaze("6", 0, 0));
    b.predefinedPrograms.push_back(ProfileGenerator::generateSlowGlaze("6", 0, 0));
    b.activeProgramIndex = 0;
    appState.commitLoadedPrograms(std::move(b));
}

KilnStatus AppState::getStatus() const {
    lockSt();
    KilnStatus s = status_;
    unlockSt();
    return s;
}

TempUnit AppState::getTempUnit() const {
    lockSt();
    TempUnit u = tempUnit_;
    unlockSt();
    return u;
}

int AppState::getActiveProgramIndex() const {
    lockSt();
    int i = activeProgramIndex_;
    unlockSt();
    return i;
}

AppState::TelemetryView AppState::getTelemetryView() const {
    lockSt();
    TelemetryView v;
    v.status   = status_;
    v.tempUnit = tempUnit_;
    unlockSt();
    return v;
}

bool AppState::tryCopyActiveProgram(FiringProgram* out) const {
    if (!out)
        return false;
    lockSt();
    const FiringProgram* p = mutableActiveProgram();
    if (!p) {
        unlockSt();
        return false;
    }
    *out = *p;
    unlockSt();
    return true;
}

bool AppState::tryCopyPredefinedProgram(size_t idx, FiringProgram* out) const {
    if (!out)
        return false;
    lockSt();
    if (idx >= predefinedPrograms_.size()) {
        unlockSt();
        return false;
    }
    *out = predefinedPrograms_[idx];
    unlockSt();
    return true;
}

bool AppState::tryCopyCustomProgram(size_t idx, FiringProgram* out) const {
    if (!out)
        return false;
    lockSt();
    if (idx >= customPrograms_.size()) {
        unlockSt();
        return false;
    }
    *out = customPrograms_[idx];
    unlockSt();
    return true;
}

std::vector<std::string> AppState::getCustomProgramNames() const {
    lockSt();
    std::vector<std::string> names;
    names.reserve(customPrograms_.size());
    for (const auto& p : customPrograms_)
        names.push_back(p.name);
    unlockSt();
    return names;
}

size_t AppState::getCustomProgramCount() const {
    lockSt();
    size_t n = customPrograms_.size();
    unlockSt();
    return n;
}

void AppState::setTempUnit(TempUnit u) {
    lockSt();
    tempUnit_ = u;
    unlockSt();
}

void AppState::setActiveProgramIndex(int idx) {
    lockSt();
    activeProgramIndex_ = idx;
    syncActiveProgramNameWithIndex();
    unlockSt();
}

void AppState::setActiveProgramName(std::string name) {
    lockSt();
    status_.activeProgramName = std::move(name);
    unlockSt();
}

void AppState::applyStartStopTap() {
    lockSt();
    if (status_.currentState == KilnState::IDLE) {
        status_.currentState = KilnState::RAMPING;
    } else {
        status_.currentState = KilnState::IDLE;
        status_.frozenControllerError.clear();
    }
    unlockSt();
}

void AppState::setPredefinedProgram(size_t idx, FiringProgram prog) {
    lockSt();
    if (idx < predefinedPrograms_.size())
        predefinedPrograms_[idx] = std::move(prog);
    unlockSt();
}

void AppState::appendCustomProgram(FiringProgram prog) {
    lockSt();
    customPrograms_.push_back(std::move(prog));
    unlockSt();
}

bool AppState::replaceCustomProgram(size_t idx, FiringProgram prog) {
    lockSt();
    if (idx >= customPrograms_.size()) {
        unlockSt();
        return false;
    }
    customPrograms_[idx] = std::move(prog);
    unlockSt();
    return true;
}

void AppState::replaceCustomPrograms(std::vector<FiringProgram> programs) {
    lockSt();
    customPrograms_ = std::move(programs);
    unlockSt();
}

bool AppState::eraseCustomSegment(size_t programIndex, size_t segmentIndex) {
    lockSt();
    if (programIndex >= customPrograms_.size()) {
        unlockSt();
        return false;
    }
    auto& segs = customPrograms_[programIndex].segments;
    if (segmentIndex >= segs.size()) {
        unlockSt();
        return false;
    }
    segs.erase(segs.begin() + segmentIndex);
    unlockSt();
    return true;
}

void AppState::setKilnStateFromProgramConfigStart() {
    lockSt();
    status_.currentState = KilnState::RAMPING;
    unlockSt();
}

void AppState::captureIntoSelectionSnapshot(ProgramSelectionSnapshot* dst) const {
    if (!dst)
        return;
    lockSt();
    dst->activeProgramIndex = activeProgramIndex_;
    dst->valid              = true;

    for (int i = 0; i < 4; ++i) {
        if (i < (int)predefinedPrograms_.size()) {
            dst->preCone[i]   = predefinedPrograms_[i].origCone;
            dst->preCandle[i] = predefinedPrograms_[i].origCandle;
            dst->preSoak[i]   = predefinedPrograms_[i].origSoak;
        }
    }

    dst->hasCustomSlot = false;
    dst->customIndex   = 0;
    dst->customCopy    = {};

    if (activeProgramIndex_ >= 4) {
        const size_t ci = static_cast<size_t>(activeProgramIndex_ - 4);
        if (ci < customPrograms_.size()) {
            dst->hasCustomSlot = true;
            dst->customIndex   = static_cast<int>(ci);
            dst->customCopy    = customPrograms_[ci];
        }
    }
    unlockSt();
}

void AppState::applySelectionSnapshot(const ProgramSelectionSnapshot& snap) {
    if (!snap.valid)
        return;
    lockSt();
    for (int i = 0; i < 4; ++i) {
        if (i >= (int)predefinedPrograms_.size())
            continue;
        predefinedPrograms_[i].origCone   = snap.preCone[i];
        predefinedPrograms_[i].origCandle = snap.preCandle[i];
        predefinedPrograms_[i].origSoak   = snap.preSoak[i];
        regeneratePredefinedSlotLocked(i);
    }

    if (snap.hasCustomSlot && snap.customIndex >= 0) {
        const size_t need = static_cast<size_t>(snap.customIndex) + 1;
        while (customPrograms_.size() < need)
            customPrograms_.push_back(FiringProgram{});
        customPrograms_[static_cast<size_t>(snap.customIndex)] = snap.customCopy;
    }

    activeProgramIndex_ = snap.activeProgramIndex;
    syncActiveProgramNameWithIndex();
    unlockSt();
}

void AppState::commitLoadedPrograms(LoadedProgramsBundle bundle) {
    lockSt();
    if (bundle.predefinedPrograms.size() == 4)
        predefinedPrograms_ = std::move(bundle.predefinedPrograms);
    customPrograms_   = std::move(bundle.customPrograms);
    activeProgramIndex_ = bundle.activeProgramIndex;
    if (activeProgramIndex_ < 0)
        activeProgramIndex_ = 0;

    if (activeProgramIndex_ <= 3 && activeProgramIndex_ < (int)predefinedPrograms_.size()) {
        status_.activeProgramName = predefinedPrograms_[activeProgramIndex_].name;
    } else {
        int customIdx = activeProgramIndex_ - 4;
        if (customIdx >= 0 && customIdx < (int)customPrograms_.size()) {
            status_.activeProgramName = customPrograms_[customIdx].name;
        } else {
            activeProgramIndex_       = 0;
            status_.activeProgramName = predefinedPrograms_[0].name;
        }
    }
    unlockSt();
}

AppState::PersistSnapshot AppState::snapshotForPersist() const {
    lockSt();
    PersistSnapshot p;
    p.customPrograms     = customPrograms_;
    p.predefinedPrograms = predefinedPrograms_;
    p.activeProgramIndex = activeProgramIndex_;
    unlockSt();
    return p;
}

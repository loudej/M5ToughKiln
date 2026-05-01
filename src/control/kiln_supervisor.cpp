#include "kiln_supervisor.h"
#include "stopwatch.h"

#include "../model/app_state.h"

#include <Arduino.h>
#include <cmath>
#include <string>

namespace {

// Unit scales (multiply literal magnitudes on the left).
constexpr float    k_Fahrenheit  = 5.0f / 9.0f;   // Δ°F → Δ°C, or (T_F − 32) for absolute °F → °C
constexpr float    k_Percent     = 1.0f / 100.0f;
constexpr uint32_t k_Seconds     = 1000UL;
constexpr uint32_t k_Minutes     = 60UL * 1000UL;

constexpr float    kHeatNoRiseMinRiseC          = 10 * k_Fahrenheit;
constexpr uint32_t kHeatNoRiseWindowMs          = 10 * k_Minutes;
constexpr uint8_t  kHeatNoRiseFailWindowsReq   = 2;
constexpr float    kHeatDemandThresh            = 80 * k_Percent;

constexpr float    kRampOvershootTolC         = 27 * k_Fahrenheit;
constexpr uint32_t kRampOvershootDebounceMs   = 5 * k_Minutes;

constexpr float    kRampHardStopC             = 54 * k_Fahrenheit;

constexpr float    kSoakUndertempTolC         = 18 * k_Fahrenheit;
constexpr uint32_t kSoakUndertempDebounceMs   = 5 * k_Minutes;
constexpr float    kSoakHeatDemandThresh      = 70 * k_Percent;

constexpr float    kSoakOvershootTolC         = 18 * k_Fahrenheit;
constexpr uint32_t kSoakOvershootDebounceMs   = 2 * k_Minutes;

constexpr float    kSoakHardStopC             = 45 * k_Fahrenheit;

constexpr float    kCoolUndertempTolC         = 18 * k_Fahrenheit;
constexpr uint32_t kCoolUndertempDebounceMs   = 5 * k_Minutes;

constexpr float    kCoolingRunawayRiseC       = 9 * k_Fahrenheit;
constexpr uint32_t kCoolingRunawayWindowMs    = 3 * k_Minutes;
constexpr float    kCoolingRunawayPowerMax    = 5 * k_Percent;

// Absolute sensor bounds: °F spot values, then °C via (T_F − 32) × k_Fahrenheit.
constexpr float kSensorMin_abs_F = -58.f;
constexpr float kSensorMax_abs_F = 2372.f;
constexpr float kSensorMinC      = (kSensorMin_abs_F - 32.f) * k_Fahrenheit;
constexpr float kSensorMaxC      = (kSensorMax_abs_F - 32.f) * k_Fahrenheit;

constexpr float    kSensorJumpMaxC_per_min  = 180 * k_Fahrenheit;
constexpr uint32_t kSensorInvalidDebounceMs = 5 * k_Seconds;

struct SupervisorState {
    uint32_t heatWindowStartMs = 0;
    float    heatWindowStartTempC = 0.f;
    uint8_t  heatFailedWindows = 0;

    Stopwatch swSensorInvalid{kSensorInvalidDebounceMs};
    Stopwatch swRampHard{0};
    Stopwatch swRampOver{kRampOvershootDebounceMs};
    Stopwatch swSoakUnder{kSoakUndertempDebounceMs};
    Stopwatch swSoakHard{0};
    Stopwatch swSoakOver{kSoakOvershootDebounceMs};
    Stopwatch swCoolUnder{kCoolUndertempDebounceMs};

    uint32_t coolRiseWindowStartMs = 0;
    float    coolRiseWindowStartTempC = 0.f;

    bool     hasLastSensor = false;
    uint32_t lastSensorMs = 0;
    float    lastSensorC = 0.f;
};

SupervisorState s;

static bool is_active_state(KilnState st) {
    return st == KilnState::RAMPING || st == KilnState::SOAKING || st == KilnState::COOLING;
}

static bool validate_program_integrity(const FiringProgram& prog) {
    if (prog.segments.empty()) return false;
    for (const auto& seg : prog.segments) {
        if (!std::isfinite(seg.targetTemperature) || !std::isfinite(seg.rampRate)) return false;
        if (seg.targetTemperature < kSensorMinC || seg.targetTemperature > kSensorMaxC) return false;
        if (seg.rampRate < 0.f || seg.rampRate > 600.f) return false; // conservative sanity limit
        if (seg.soakTime > 24U * 60U) return false;
    }
    return true;
}

} // namespace

void KilnSupervisor::resetRunWindowState() {
    s.heatWindowStartMs = 0;
    s.heatWindowStartTempC = 0.f;
    s.heatFailedWindows = 0;
    s.swSensorInvalid.reset();
    s.swRampHard.reset();
    s.swRampOver.reset();
    s.swSoakUnder.reset();
    s.swSoakHard.reset();
    s.swSoakOver.reset();
    s.swCoolUnder.reset();
    s.coolRiseWindowStartMs = 0;
    s.coolRiseWindowStartTempC = 0.f;
    s.hasLastSensor = false;
}

void KilnSupervisor::service() {
    const uint32_t now = millis();

    const AppState::TelemetryView tv = appState.getTelemetryView();
    const KilnStatus& st = tv.status;

    if (!is_active_state(st.currentState)) {
        resetRunWindowState();
        return;
    }

    FiringProgram ap;
    if (!appState.tryCopyActiveProgram(&ap) || !validate_program_integrity(ap)) {
        appState.latchError("ERROR K-103: Program Integrity Fault");
        return;
    }

    const float tempC = st.currentTemperature;
    const float targetC = st.targetTemperature;
    const float power = st.power;
    const float overC = tempC - targetC;
    const float deficitC = targetC - tempC;

    // K-113: invalid sensor data range/jump-rate
    bool invalidSensor = !std::isfinite(tempC) || tempC < kSensorMinC || tempC > kSensorMaxC;
    if (s.hasLastSensor && now > s.lastSensorMs) {
        const float dtMin = static_cast<float>(now - s.lastSensorMs) / 60000.f;
        if (dtMin > 0.f) {
            const float jumpRate = std::fabs(tempC - s.lastSensorC) / dtMin;
            if (jumpRate > kSensorJumpMaxC_per_min) invalidSensor = true;
        }
    }
    if (s.swSensorInvalid(now, invalidSensor)) {
        appState.latchError("ERROR K-113: Invalid Sensor Data");
        return;
    }
    s.lastSensorC = tempC;
    s.lastSensorMs = now;
    s.hasLastSensor = true;

    if (st.currentState == KilnState::RAMPING) {
        if (power >= kHeatDemandThresh) {
            if (s.heatWindowStartMs == 0) {
                s.heatWindowStartMs = now;
                s.heatWindowStartTempC = tempC;
            } else if (now - s.heatWindowStartMs >= kHeatNoRiseWindowMs) {
                const float riseC = tempC - s.heatWindowStartTempC;
                if (riseC < kHeatNoRiseMinRiseC) {
                    if (s.heatFailedWindows < 255) ++s.heatFailedWindows;
                } else {
                    s.heatFailedWindows = 0;
                }
                s.heatWindowStartMs = now;
                s.heatWindowStartTempC = tempC;
            }
            if (s.heatFailedWindows >= kHeatNoRiseFailWindowsReq) {
                appState.latchError("ERROR K-104: Heat Demand, No Response");
                return;
            }
        } else {
            s.heatWindowStartMs = 0;
            s.heatFailedWindows = 0;
        }
    } else {
        s.heatWindowStartMs = 0;
        s.heatFailedWindows = 0;
    }

    if (s.swRampHard(now,
                      st.currentState == KilnState::RAMPING,
                      overC >= kRampHardStopC)) {
        appState.latchError("ERROR K-106: Ramp Over-Temp Hard Stop");
        return;
    }
    if (s.swRampOver(now,
                     st.currentState == KilnState::RAMPING,
                     overC >= kRampOvershootTolC)) {
        appState.latchError("ERROR K-105: Ramp Overshoot");
        return;
    }

    if (s.swSoakUnder(now,
                      st.currentState == KilnState::SOAKING,
                      deficitC >= kSoakUndertempTolC,
                      power >= kSoakHeatDemandThresh)) {
        appState.latchError("ERROR K-107: Cannot Hold Setpoint");
        return;
    }
    if (s.swSoakHard(now,
                     st.currentState == KilnState::SOAKING,
                     overC >= kSoakHardStopC)) {
        appState.latchError("ERROR K-108: Soak Overshoot");
        return;
    }
    if (s.swSoakOver(now,
                     st.currentState == KilnState::SOAKING,
                     overC >= kSoakOvershootTolC)) {
        appState.latchError("ERROR K-108: Soak Overshoot");
        return;
    }

    if (s.swCoolUnder(now,
                      st.currentState == KilnState::COOLING,
                      deficitC >= kCoolUndertempTolC)) {
        appState.latchError("ERROR K-109: Cooling Below Setpoint");
        return;
    }

    if (st.currentState == KilnState::COOLING) {
        if (power <= kCoolingRunawayPowerMax) {
            if (s.coolRiseWindowStartMs == 0) {
                s.coolRiseWindowStartMs = now;
                s.coolRiseWindowStartTempC = tempC;
            } else if (now - s.coolRiseWindowStartMs >= kCoolingRunawayWindowMs) {
                if ((tempC - s.coolRiseWindowStartTempC) >= kCoolingRunawayRiseC) {
                    appState.latchError("ERROR K-110: Heating Detected During Cooling");
                    return;
                }
                s.coolRiseWindowStartMs = now;
                s.coolRiseWindowStartTempC = tempC;
            }
        } else {
            s.coolRiseWindowStartMs = 0;
        }
    } else {
        s.coolRiseWindowStartMs = 0;
    }
}

#include "kiln_supervisor.h"
#include "stopwatch.h"
#include "temp_rate_window.h"

#include "../model/app_state.h"

#include <Arduino.h>
#include <cmath>
#include <string>

namespace {

// Unit scales (multiply literal magnitudes on the left).
constexpr float k_Fahrenheit =
    5.0f / 9.0f; // Δ°F → Δ°C, or (T_F − 32) for absolute °F → °C
constexpr float k_Percent = 1.0f / 100.0f;
constexpr uint32_t k_Seconds = 1000UL;
constexpr uint32_t k_Minutes = 60UL * 1000UL;

// K-104: heat demand, no temperature rise.
//   Trigger: output ≥ 80%, kiln rises < 60 °F/hr (33.3 °C/hr) for 2 consecutive
//   10-min windows.
constexpr float kHeatDemandThresh = 80 * k_Percent;
constexpr float kHeatNoRiseMinC_per_hr =
    60.0f * k_Fahrenheit; // 60 °F/hr = 33.3 °C/hr
constexpr uint32_t kHeatNoRiseWindowMs = 10 * k_Minutes;
constexpr int kHeatNoRiseFailWindowsReq = 2;

// K-105 / K-106: ramp overshoot and hard stop (absolute over-setpoint bands).
constexpr float kRampOvershootTolC = 27 * k_Fahrenheit; // +27 °F soft
constexpr uint32_t kRampOvershootDebounceMs = 5 * k_Minutes;
constexpr float kRampHardStopC = 54 * k_Fahrenheit; // +54 °F instant hard stop

// K-107 / K-108: soak hold and overshoot (absolute bands).
constexpr float kSoakUndertempTolC = 18 * k_Fahrenheit;
constexpr uint32_t kSoakUndertempDebounceMs = 5 * k_Minutes;
constexpr float kSoakHeatDemandThresh = 70 * k_Percent;
constexpr float kSoakOvershootTolC = 18 * k_Fahrenheit;
constexpr uint32_t kSoakOvershootDebounceMs = 2 * k_Minutes;
constexpr float kSoakHardStopC = 45 * k_Fahrenheit;

// K-109: cooling below setpoint (absolute deficit band).
constexpr float kCoolUndertempTolC = 18 * k_Fahrenheit;
constexpr uint32_t kCoolUndertempDebounceMs = 5 * k_Minutes;

// K-110: unexpected heating during cooling (rate window).
//   Trigger: output ≤ 5%, kiln rises ≥ 180 °F/hr (100 °C/hr) over 3-min window.
constexpr float kCoolingRunawayC_per_hr =
    180.0f * k_Fahrenheit; // 180 °F/hr = 100 °C/hr
constexpr uint32_t kCoolingRunawayWindowMs = 3 * k_Minutes;
constexpr float kCoolingRunawayPowerMax = 5 * k_Percent;

// K-111: cooling too slow (rate window).
//   Trigger: drop rate < 27 °F/hr (15 °C/hr) over 20-min window; suppressed
//   below 212 °F (100 °C).
constexpr float kCoolTooSlowMinC_per_hr =
    27.0f * k_Fahrenheit; // 27 °F/hr = 15 °C/hr
constexpr uint32_t kCoolTooSlowWindowMs = 20 * k_Minutes;
constexpr float kCoolTooSlowSuppressC =
    (212.0f - 32.0f) * k_Fahrenheit; // 100 °C

// K-113: invalid sensor data (absolute bounds + jump-rate).
constexpr float kSensorMin_abs_F = -58.f;
constexpr float kSensorMax_abs_F = 2372.f;
constexpr float kSensorMinC = (kSensorMin_abs_F - 32.f) * k_Fahrenheit;
constexpr float kSensorMaxC = (kSensorMax_abs_F - 32.f) * k_Fahrenheit;

constexpr float kSensorJumpMaxC_per_min = 180 * k_Fahrenheit;
constexpr uint32_t kSensorInvalidDebounceMs = 5 * k_Seconds;

struct SupervisorState {
  TempRateWindow k104HeatRise{
      kHeatNoRiseWindowMs}; ///< K-104: heat demand, no rise.
  TempRateWindow k110CoolRise{
      kCoolingRunawayWindowMs}; ///< K-110: unexpected rise while cooling.
  TempRateWindow k111CoolSlow{
      kCoolTooSlowWindowMs}; ///< K-111: cooling too slow.

  Stopwatch k113Sensor{kSensorInvalidDebounceMs};
  Stopwatch k106RampHard{0};
  Stopwatch k105RampOver{kRampOvershootDebounceMs};
  Stopwatch k107SoakUnder{kSoakUndertempDebounceMs};
  Stopwatch k108SoakHard{0};
  Stopwatch k108SoakOver{kSoakOvershootDebounceMs};
  Stopwatch k109CoolUnder{kCoolUndertempDebounceMs};

  bool hasLastSensor = false;
  uint32_t lastSensorMs = 0;
  float lastSensorC = 0.f;
};

SupervisorState s;

static bool is_active_state(KilnState st) {
  return st == KilnState::RAMPING || st == KilnState::SOAKING ||
         st == KilnState::COOLING;
}

static bool validate_program_integrity(const FiringProgram &prog) {
  if (prog.segments.empty())
    return false;
  for (const auto &seg : prog.segments) {
    if (!std::isfinite(seg.targetTemperature) || !std::isfinite(seg.rampRate))
      return false;
    if (seg.targetTemperature < kSensorMinC ||
        seg.targetTemperature > kSensorMaxC)
      return false;
    if (seg.rampRate < 0.f || seg.rampRate > 600.f)
      return false;
    if (seg.soakTime > 24U * 60U)
      return false;
  }
  return true;
}

} // namespace

void KilnSupervisor::resetRunWindowState() {
  s.k104HeatRise.reset();
  s.k110CoolRise.reset();
  s.k111CoolSlow.reset();
  s.k113Sensor.reset();
  s.k106RampHard.reset();
  s.k105RampOver.reset();
  s.k107SoakUnder.reset();
  s.k108SoakHard.reset();
  s.k108SoakOver.reset();
  s.k109CoolUnder.reset();
  s.hasLastSensor = false;
}

void KilnSupervisor::service() {
  const uint32_t now = millis();

  const AppState::TelemetryView tv = appState.getTelemetryView();
  const KilnStatus &st = tv.status;

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

  // K-113: sensor data out-of-range or implausible jump rate.
  bool invalidSensor =
      !std::isfinite(tempC) || tempC < kSensorMinC || tempC > kSensorMaxC;
  if (s.hasLastSensor && now > s.lastSensorMs) {
    const float dtMin = static_cast<float>(now - s.lastSensorMs) / 60000.f;
    const float jumpRate =
        dtMin > 0.f ? std::fabs(tempC - s.lastSensorC) / dtMin : 0.f;
    if (jumpRate > kSensorJumpMaxC_per_min)
      invalidSensor = true;
  }
  if (s.k113Sensor(now, invalidSensor)) {
    appState.latchError("ERROR K-113: Invalid Sensor Data");
    return;
  }
  s.lastSensorC = tempC;
  s.lastSensorMs = now;
  s.hasLastSensor = true;

  // K-104: heat demand, no temperature rise — two consecutive failed 10-minute
  // windows.
  if (s.k104HeatRise.update(
          now, tempC, [](float r) { return r < kHeatNoRiseMinC_per_hr; },
          st.currentState == KilnState::RAMPING,
          power >= kHeatDemandThresh) >= kHeatNoRiseFailWindowsReq) {
    appState.latchError("ERROR K-104: Heat Demand, No Response");
    return;
  }

  // K-106: ramp over-temp hard stop — instant, no debounce.
  if (s.k106RampHard(now, st.currentState == KilnState::RAMPING,
                     overC >= kRampHardStopC)) {
    appState.latchError("ERROR K-106: Ramp Over-Temp Hard Stop");
    return;
  }

  // K-105: ramp overshoot — 5-minute debounce.
  if (s.k105RampOver(now, st.currentState == KilnState::RAMPING,
                     overC >= kRampOvershootTolC)) {
    appState.latchError("ERROR K-105: Ramp Overshoot");
    return;
  }

  // K-107: cannot hold soak setpoint — 5-minute debounce, high heat demand
  // required.
  if (s.k107SoakUnder(now, st.currentState == KilnState::SOAKING,
                      deficitC >= kSoakUndertempTolC,
                      power >= kSoakHeatDemandThresh)) {
    appState.latchError("ERROR K-107: Cannot Hold Setpoint");
    return;
  }

  // K-108: soak overshoot hard stop — instant.
  if (s.k108SoakHard(now, st.currentState == KilnState::SOAKING,
                     overC >= kSoakHardStopC)) {
    appState.latchError("ERROR K-108: Soak Overshoot");
    return;
  }

  // K-108: soak overshoot soft — 2-minute debounce.
  if (s.k108SoakOver(now, st.currentState == KilnState::SOAKING,
                     overC >= kSoakOvershootTolC)) {
    appState.latchError("ERROR K-108: Soak Overshoot");
    return;
  }

  // K-109: cooling below setpoint — 5-minute debounce.
  if (s.k109CoolUnder(now, st.currentState == KilnState::COOLING,
                      deficitC >= kCoolUndertempTolC)) {
    appState.latchError("ERROR K-109: Cooling Below Setpoint");
    return;
  }

  // K-110: unexpected heating while cooling — rise ≥ 180 °F/hr over 3-min
  // window, output ≤ 5%.
  if (s.k110CoolRise.update(
          now, tempC, [](float r) { return r >= kCoolingRunawayC_per_hr; },
          st.currentState == KilnState::COOLING,
          power <= kCoolingRunawayPowerMax) >= 1) {
    appState.latchError("ERROR K-110: Heating Detected During Cooling");
    return;
  }

  // K-111: cooling too slow — drop < 27 °F/hr over 20-min window; suppressed
  // below 212 °F.
  if (s.k111CoolSlow.update(
          now, tempC, [](float r) { return r > -kCoolTooSlowMinC_per_hr; },
          st.currentState == KilnState::COOLING,
          tempC >= kCoolTooSlowSuppressC) >= 1) {
    appState.latchError("ERROR K-111: Cooling Too Slow");
    return;
  }
}

#ifndef TEMP_RATE_WINDOW_H
#define TEMP_RATE_WINDOW_H

#include <stdint.h>

/// Fixed-width sliding temperature-rate window with consecutive-failure counting.
///
/// Uses a separate `armed_` flag so `startMs_ == 0` is never overloaded as "not yet started"
/// — `now == 0` is a valid millis() value at boot and after the ~49-day rollover.
///
/// Construct with a fixed window duration. Each call to update() either:
///   - resets state and returns 0 if any active flag is false, OR
///   - accumulates until the window elapses, then calls fn(rate_C_per_hr) to determine
///     pass/fail, rolls the window forward, and returns the consecutive-failure count.
///
/// The fn evaluator receives the net temperature change rate in °C/hr over the elapsed
/// window (positive = warmed, negative = cooled), and returns true if that is a failure.
///
/// Example — K-111 cooling too slow (must drop > 15 °C/hr; fault if it doesn't):
///
///   TempRateWindow coolSlowWin{20 * k_Minutes};
///   // in service():
///   const int n = s.coolSlowWin.update(now, tempC,
///       [](float r){ return r > -kCoolTooSlowMinC_per_hr; },   // failure: not dropping fast enough
///       st.currentState == KilnState::COOLING,
///       tempC >= kCoolTooSlowSuppressC);
///   if (n >= 1) { latchError("ERROR K-111: Cooling Too Slow"); return; }
///
/// Example — K-104 heat demand no response (two consecutive failed windows):
///
///   TempRateWindow heatWin{10 * k_Minutes};
///   const int n = s.heatWin.update(now, tempC,
///       [](float r){ return r < kHeatNoRiseMinC_per_hr; },
///       st.currentState == KilnState::RAMPING,
///       power >= kHeatDemandThresh);
///   if (n >= kHeatNoRiseFailWindowsReq) { latchError("ERROR K-104: ..."); return; }
///
class TempRateWindow {
public:
    explicit TempRateWindow(uint32_t windowMs) : windowMs_(windowMs) {}

    void reset() {
        armed_    = false;
        failures_ = 0;
    }

    /// Update the window. `fn` receives rate in °C/hr; variadic `flags...` are ANDed for the
    /// active condition. Returns the current consecutive-failure count (0 when inactive or
    /// window not yet elapsed, ≥1 after each successive failed window).
    template<typename Fn, typename... Flags>
    int update(uint32_t now, float tempC, Fn fn, Flags... flags) {
        if (!allTrue(flags...)) {
            reset();
            return 0;
        }
        if (!armed_) {
            armed_      = true;
            startMs_    = now;
            startTempC_ = tempC;
            return 0;
        }
        if ((now - startMs_) < windowMs_)
            return failures_;

        const float windowHours = windowMs_ / 3600000.f;
        const float rateCperHr  = (tempC - startTempC_) / windowHours;
        startMs_    = now;
        startTempC_ = tempC;

        if (fn(rateCperHr)) {
            if (failures_ < 255) ++failures_;
        } else {
            failures_ = 0;
        }
        return failures_;
    }

private:
    // C++11-compatible variadic AND (fold expression not available until C++17).
    static bool allTrue() { return true; }
    template<typename... Rest>
    static bool allTrue(bool first, Rest... rest) { return first && allTrue(rest...); }

    const uint32_t windowMs_;
    bool           armed_      = false;
    uint32_t       startMs_    = 0;
    float          startTempC_ = 0.f;
    uint8_t        failures_   = 0;
};

#endif // TEMP_RATE_WINDOW_H

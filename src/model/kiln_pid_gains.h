#ifndef KILN_PID_GAINS_H
#define KILN_PID_GAINS_H

#include <cmath>

struct KilnPidGains {
    float kp = 0.5f;
    float ki = 0.015f;
    float kd = 0.4f;
};

inline KilnPidGains kilnDefaultPidGains() {
    return KilnPidGains{};
}

inline float clampFloat(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    if (std::isnan(v) || !std::isfinite(v)) return lo;
    return v;
}

/// Conservative bounds; controller uses output 0–100 %.
inline KilnPidGains kilnClampPidGains(const KilnPidGains& g) {
    KilnPidGains o;
    o.kp = clampFloat(g.kp, 0.001f, 50.f);
    o.ki = clampFloat(g.ki, 0.f, 5.f);
    o.kd = clampFloat(g.kd, 0.f, 200.f);
    return o;
}

constexpr uint8_t kPidOvKp = 1u;
constexpr uint8_t kPidOvKi = 2u;
constexpr uint8_t kPidOvKd = 4u;

/// Per-field overrides: unset bits keep `kilnDefaultPidGains()` for that coefficient.
inline KilnPidGains kilnMergePidGains(uint8_t mask, const KilnPidGains& ov) {
    KilnPidGains d = kilnDefaultPidGains();
    if (mask & kPidOvKp) d.kp = ov.kp;
    if (mask & kPidOvKi) d.ki = ov.ki;
    if (mask & kPidOvKd) d.kd = ov.kd;
    return kilnClampPidGains(d);
}

#endif

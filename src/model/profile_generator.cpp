#include "profile_generator.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>

// ── Cone helpers ────────────────────────────────────────────────────────────

// "04" → -4, "010" → -10, "1" → 1, "6" → 6
int ProfileGenerator::parseConeInt(const std::string& cone) {
    if (cone.empty()) return 6;
    if (cone == "0")  return 0;
    // Orton "0-prefix" cones (low-fire): "01".."010" → -1..-10
    if (cone.size() >= 2 && cone[0] == '0') {
        return -std::atoi(cone.c_str() + 1);
    }
    int n = std::atoi(cone.c_str());
    return (n != 0) ? n : 6; // atoi returns 0 on failure; default to cone 6
}

// Orton chart target temperatures at 108°F/hr, in °F.
float ProfileGenerator::coneTargetF(int c) {
    switch (c) {
        case -10: return 1657;
        case  -9: return 1688;
        case  -8: return 1728;
        case  -7: return 1789;
        case  -6: return 1828;
        case  -5: return 1888;
        case  -4: return 1945;
        case  -3: return 1987;
        case  -2: return 2016;
        case  -1: return 2046;
        case   0: return 2063; // interpolated: midpoint of cone 01 and cone 1
        case   1: return 2079;
        case   2: return 2088;
        case   3: return 2106;
        case   4: return 2124;
        case   5: return 2167;
        case   6: return 2232;
        case   7: return 2262;
        case   8: return 2280;
        case   9: return 2300;
        case  10: return 2345;
        default:  return 2232; // out-of-range: fall back to cone 6
    }
}

// ── Program generators ───────────────────────────────────────────────────────

FiringProgram ProfileGenerator::generateFastBisque(const std::string& cone, int candleMinutes, int soakMinutes) {
    FiringProgram prog;
    prog.name = "Fast Bisque (Cone " + cone + ")";
    prog.isCustom = false;
    prog.origCone   = cone;
    prog.origCandle = candleMinutes;
    prog.origSoak   = soakMinutes;

    float tF = coneTargetF(parseConeInt(cone));
    float tC = fToC(tF);

    prog.segments.push_back({fToC(250),        rateToC(120), (uint32_t)candleMinutes});
    prog.segments.push_back({fToC(1000),       rateToC(300), 0});
    prog.segments.push_back({fToC(1100),       rateToC(150), 0});
    prog.segments.push_back({fToC(tF - 250),   rateToC(180), 0});
    prog.segments.push_back({tC,               rateToC(108), (uint32_t)soakMinutes});
    return prog;
}

FiringProgram ProfileGenerator::generateSlowBisque(const std::string& cone, int candleMinutes, int soakMinutes) {
    FiringProgram prog;
    prog.name = "Slow Bisque (Cone " + cone + ")";
    prog.isCustom = false;
    prog.origCone   = cone;
    prog.origCandle = candleMinutes;
    prog.origSoak   = soakMinutes;

    float tF = coneTargetF(parseConeInt(cone));
    float tC = fToC(tF);

    prog.segments.push_back({fToC(250),        rateToC(80),  (uint32_t)candleMinutes});
    prog.segments.push_back({fToC(1000),       rateToC(200), 0});
    prog.segments.push_back({fToC(1100),       rateToC(100), 0});
    prog.segments.push_back({fToC(tF - 250),   rateToC(180), 0});
    prog.segments.push_back({tC,               rateToC(80),  (uint32_t)soakMinutes});
    return prog;
}

FiringProgram ProfileGenerator::generateFastGlaze(const std::string& cone, int candleMinutes, int soakMinutes) {
    FiringProgram prog;
    prog.name = "Fast Glaze (Cone " + cone + ")";
    prog.isCustom = false;
    prog.origCone   = cone;
    prog.origCandle = candleMinutes;
    prog.origSoak   = soakMinutes;

    float tF = coneTargetF(parseConeInt(cone));
    float tC = fToC(tF);

    prog.segments.push_back({fToC(250),        rateToC(570), (uint32_t)candleMinutes});
    prog.segments.push_back({fToC(tF - 250),   rateToC(570), 0});
    prog.segments.push_back({tC,               rateToC(200), (uint32_t)soakMinutes});
    return prog;
}

FiringProgram ProfileGenerator::generateSlowGlaze(const std::string& cone, int candleMinutes, int soakMinutes) {
    FiringProgram prog;
    prog.name = "Slow Glaze (Cone " + cone + ")";
    prog.isCustom = false;
    prog.origCone   = cone;
    prog.origCandle = candleMinutes;
    prog.origSoak   = soakMinutes;

    float tF = coneTargetF(parseConeInt(cone));
    float tC = fToC(tF);

    prog.segments.push_back({fToC(250),        rateToC(150), (uint32_t)candleMinutes});
    prog.segments.push_back({fToC(tF - 250),   rateToC(400), 0});
    prog.segments.push_back({tC,               rateToC(120), (uint32_t)soakMinutes});
    return prog;
}

std::string ProfileGenerator::coneIntToString(int c) {
    if (c < 0) return "0" + std::to_string(-c); // -4 → "04", -10 → "010"
    return std::to_string(c);                    //  6 → "6"
}

std::string ProfileGenerator::coneLabelFromPeakTempC(float peakTempC) {
    const float peakF = peakTempC * 9.0f / 5.0f + 32.0f;
    int         bestCi  = 6;
    float       bestAbs = 1e12f;
    for (int ci = -10; ci <= 10; ++ci) {
        const float d = std::fabs(coneTargetF(ci) - peakF);
        if (d < bestAbs) {
            bestAbs = d;
            bestCi  = ci;
        }
    }
    return coneIntToString(bestCi);
}

float ProfileGenerator::estimateTotalMinutes(const FiringProgram& prog, float startTempC) {
    float totalMinutes = 0;
    float prevTemp = startTempC;
    for (const auto& seg : prog.segments) {
        float deltaC = seg.targetTemperature - prevTemp;
        // Same physics as FiringController: rampRate °C/h applies to heating and cooling.
        if (seg.rampRate > 0.f && deltaC != 0.f) {
            totalMinutes += (std::fabs(deltaC) / seg.rampRate) * 60.0f;
        }
        totalMinutes += static_cast<float>(seg.soakTime);    // soakTime in minutes
        prevTemp = seg.targetTemperature;
    }
    return totalMinutes;
}

ScheduleArmAlignment ProfileGenerator::alignArmScheduleToMeasured(const FiringProgram& prog,
                                                                  float measuredC,
                                                                  float startTempC) {
    // The program is a piecewise 2D schedule: X = minutes, Y = °C.
    // Each segment has a ramp leg (slope = rampRate °C/h) then a flat soak leg (soakTime minutes).
    //
    // We walk segments from the beginning. For each segment:
    //   - If measuredC is clearly past the segment's target (in the direction of travel),
    //     accumulate the segment's ramp + soak time and continue to the next segment.
    //   - Otherwise measuredC sits on (or before) this ramp leg: interpolate to find the
    //     exact intersection, and return.
    //
    // The returned alignedSetpointC is the Y value at the intersection.
    // The controller sets segmentStartTemp = alignedSetpointC, segmentStartMs = now, so
    // the ramp continues forward from the aligned temperature at the correct rate.

    ScheduleArmAlignment out{};
    if (prog.segments.empty())
        return out;

    constexpr float kBandC   = 2.5f;   // ±°C: "at or past target" tolerance
    constexpr float kEpsTemp = 1e-3f;  // ΔT below this is a flat/degenerate segment
    constexpr float kEpsRate = 1e-9f;  // rampRate below this treated as zero

    float accumMins = 0.f;  // total schedule minutes for all skipped segments so far
    float fromY     = startTempC;

    const int n = static_cast<int>(prog.segments.size());

    for (int i = 0; i < n; ++i) {
        const FiringSegment& seg = prog.segments[static_cast<size_t>(i)];
        const float toY = seg.targetTemperature;
        const float dY  = toY - fromY;

        const bool heating = dY >  kEpsTemp;
        const bool cooling = dY < -kEpsTemp;

        // Duration of this segment's ramp leg (0 for flat or zero-rate segments).
        const float rampMins = (heating || cooling) && seg.rampRate > kEpsRate
                               ? std::fabs(dY) / seg.rampRate * 60.f
                               : 0.f;

        // "Past target" means the kiln has already gone through this segment's target
        // in the direction of travel — both the ramp and the soak are behind us.
        const bool pastTarget = heating ? (measuredC > toY + kBandC)
                              : cooling ? (measuredC < toY - kBandC)
                                        : (std::fabs(measuredC - toY) > kBandC);

        if (pastTarget) {
            accumMins += rampMins + static_cast<float>(seg.soakTime);
            fromY = toY;
            continue;
        }

        // measuredC is on or before this segment's target.
        // Interpolate position u ∈ [0, 1] along the ramp leg.
        // For flat or zero-rate segments u = 0 (aligned at the ramp start).
        float u = 0.f;
        if ((heating || cooling) && rampMins > 0.f) {
            u = (measuredC - fromY) / dY;
            if (u < 0.f) u = 0.f;
            if (u > 1.f) u = 1.f;
        }

        out.ok                          = true;
        out.segmentIndex                = i;
        out.alignedSetpointC            = fromY + u * dY;
        out.scheduleElapsedMinutesTotal = accumMins + u * rampMins;
        out.initialState                = cooling ? KilnState::COOLING : KilnState::RAMPING;
        return out;
    }

    // measuredC is past every segment — program is complete.
    out.ok                          = true;
    out.segmentIndex                = n;  // >= segments.size() signals DONE to the controller
    out.alignedSetpointC            = prog.segments.back().targetTemperature;
    out.scheduleElapsedMinutesTotal = estimateTotalMinutes(prog, startTempC);
    out.initialState                = KilnState::RAMPING;
    return out;
}

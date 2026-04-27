#include "profile_generator.h"
#include <cstdlib>
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

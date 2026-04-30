#ifndef PROFILE_GENERATOR_H
#define PROFILE_GENERATOR_H

#include "firing_program.h"
#include "kiln_status.h"

#include <string>

struct ScheduleArmAlignment {
    bool      ok                          = false;
    int       segmentIndex                = 0;
    /// Setpoint (°C) at the alignment intersection — used directly as segmentStartTemp.
    float     alignedSetpointC            = 21.1f;
    /// Total schedule minutes from program X=0 to the alignment point — for the display clock only.
    float     scheduleElapsedMinutesTotal = 0.f;
    KilnState initialState                = KilnState::RAMPING;
};

class ProfileGenerator {
public:
    // Takes user parameters and returns a populated FiringProgram.
    // Cone is passed as a string (e.g., "04", "6", "10") to handle Orton cone naming conventions.
    
    static FiringProgram generateFastBisque(const std::string& cone, int candleMinutes, int soakMinutes);
    static FiringProgram generateSlowBisque(const std::string& cone, int candleMinutes, int soakMinutes);
    static FiringProgram generateFastGlaze(const std::string& cone, int candleMinutes, int soakMinutes);
    static FiringProgram generateSlowGlaze(const std::string& cone, int candleMinutes, int soakMinutes);

    // Convert an integer cone back to the Orton string notation (e.g., -4 → "04", 6 → "6").
    static std::string coneIntToString(int coneInt);

    /// Nearest standard Orton cone label from segment peak temperature (stored °C); used when `origCone` is empty (e.g. custom programs).
    static std::string coneLabelFromPeakTempC(float peakTempC);

    // Estimate total firing time in minutes given a starting temperature in °C.
    static float estimateTotalMinutes(const FiringProgram& prog, float startTempC);

    /// Find where measuredC sits on the piecewise schedule (X = minutes, Y = °C).
    /// Walks segments in order; skips any segment whose target is clearly behind measuredC
    /// (in the direction of travel); stops at the first ramp leg that contains measuredC.
    /// Returns:
    ///   alignedSetpointC            — the schedule temperature at the intersection point
    ///   scheduleElapsedMinutesTotal — total elapsed minutes to that point (display only)
    ///   initialState                — RAMPING or COOLING (never SOAKING)
    static ScheduleArmAlignment alignArmScheduleToMeasured(const FiringProgram& prog,
                                                            float measuredC,
                                                            float startTempC);

private:
    // Parse an Orton cone string to an integer (e.g., "04" → -4, "6" → 6).
    static int  parseConeInt(const std::string& cone);
    // Look up the peak firing temperature in °F from the Orton chart (108°F/hr column).
    static float coneTargetF(int coneInt);
    // Unit conversion helpers — all segment data is stored internally in Celsius.
    static float fToC(float f)      { return (f - 32.0f) * 5.0f / 9.0f; }
    static float rateToC(float fph) { return fph * 5.0f / 9.0f; }
};

#endif // PROFILE_GENERATOR_H

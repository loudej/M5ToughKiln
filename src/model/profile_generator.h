#ifndef PROFILE_GENERATOR_H
#define PROFILE_GENERATOR_H

#include "firing_program.h"
#include <string>

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

    // Estimate total firing time in minutes given a starting temperature in °C.
    static float estimateTotalMinutes(const FiringProgram& prog, float startTempC);

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

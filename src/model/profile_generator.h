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

private:
    // Helper function to resolve an Orton cone string to a target temperature in Celsius.
    static float getTempForCone(const std::string& cone);
};

#endif // PROFILE_GENERATOR_H

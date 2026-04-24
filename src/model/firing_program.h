#ifndef FIRING_PROGRAM_H
#define FIRING_PROGRAM_H

#include <vector>
#include <string>

struct FiringSegment {
    float targetTemperature;
    float rampRate; // Degrees per hour
    uint32_t soakTime; // Minutes
};

class FiringProgram {
public:
    std::string name;
    std::vector<FiringSegment> segments;
    bool isCustom;

    // Original inputs for predefined programs to restore UI state
    std::string origCone;
    int origCandle;
    int origSoak;
};

#endif // FIRING_PROGRAM_H

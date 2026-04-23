#include "profile_generator.h"

// Placeholder for a real lookup table. 
// Currently just returns a dummy value so the structure works.
float ProfileGenerator::getTempForCone(const std::string& cone) {
    if (cone == "04") return 1060.0f;
    if (cone == "6") return 1222.0f;
    return 1000.0f; // Default fallback
}

FiringProgram ProfileGenerator::generateFastBisque(const std::string& cone, float candleHours, float soakMinutes) {
    FiringProgram prog;
    prog.name = "Fast Bisque (Cone " + cone + ")";
    prog.isCustom = false;
    
    float peakTemp = getTempForCone(cone);

    // Placeholder Logic:
    // 1. Candle (optional)
    if (candleHours > 0) {
        prog.segments.push_back({100.0f, 100.0f, (uint32_t)(candleHours * 60.0f)}); 
    }
    
    // 2. Fast ramp to peak
    prog.segments.push_back({peakTemp, 500.0f, (uint32_t)soakMinutes});

    return prog;
}

FiringProgram ProfileGenerator::generateSlowBisque(const std::string& cone, float candleHours, float soakMinutes) {
    FiringProgram prog;
    prog.name = "Slow Bisque (Cone " + cone + ")";
    prog.isCustom = false;
    
    float peakTemp = getTempForCone(cone);

    // Placeholder Logic:
    if (candleHours > 0) {
        prog.segments.push_back({100.0f, 50.0f, (uint32_t)(candleHours * 60.0f)}); 
    }
    
    // 1. Slow initial ramp (water smoking)
    prog.segments.push_back({250.0f, 80.0f, 0});
    // 2. Moderate ramp through quartz inversion
    prog.segments.push_back({600.0f, 150.0f, 0});
    // 3. Faster ramp to peak
    prog.segments.push_back({peakTemp, 300.0f, (uint32_t)soakMinutes});

    return prog;
}

FiringProgram ProfileGenerator::generateFastGlaze(const std::string& cone, float candleHours, float soakMinutes) {
    FiringProgram prog;
    prog.name = "Fast Glaze (Cone " + cone + ")";
    prog.isCustom = false;
    
    float peakTemp = getTempForCone(cone);

    if (candleHours > 0) {
         prog.segments.push_back({100.0f, 150.0f, (uint32_t)(candleHours * 60.0f)});
    }
    
    // Fast ramp to peak
    prog.segments.push_back({peakTemp, 600.0f, (uint32_t)soakMinutes});

    return prog;
}

FiringProgram ProfileGenerator::generateSlowGlaze(const std::string& cone, float candleHours, float soakMinutes) {
    FiringProgram prog;
    prog.name = "Slow Glaze (Cone " + cone + ")";
    prog.isCustom = false;
    
    float peakTemp = getTempForCone(cone);

    if (candleHours > 0) {
         prog.segments.push_back({100.0f, 100.0f, (uint32_t)(candleHours * 60.0f)});
    }
    
    // Slower ramp for larger pieces or tricky glazes
    prog.segments.push_back({peakTemp, 250.0f, (uint32_t)soakMinutes});

    return prog;
}

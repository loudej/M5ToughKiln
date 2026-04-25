#ifndef TEMP_UNITS_H
#define TEMP_UNITS_H

enum class TempUnit { FAHRENHEIT, CELSIUS };

// Temperature point conversion (C <-> display unit)
inline float toDisplayTemp(float celsius, TempUnit unit) {
    return unit == TempUnit::FAHRENHEIT ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}
inline float fromDisplayTemp(float display, TempUnit unit) {
    return unit == TempUnit::FAHRENHEIT ? (display - 32.0f) * 5.0f / 9.0f : display;
}

// Rate conversion: scale only, no offset (degrees per hour)
inline float toDisplayRate(float c_per_h, TempUnit unit) {
    return unit == TempUnit::FAHRENHEIT ? c_per_h * 9.0f / 5.0f : c_per_h;
}
inline float fromDisplayRate(float display, TempUnit unit) {
    return unit == TempUnit::FAHRENHEIT ? display * 5.0f / 9.0f : display;
}

inline const char* unitSymbol(TempUnit unit) {
    return unit == TempUnit::FAHRENHEIT ? "F" : "C";
}

#endif // TEMP_UNITS_H

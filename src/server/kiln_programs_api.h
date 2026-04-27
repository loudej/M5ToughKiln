#ifndef KILN_PROGRAMS_API_H
#define KILN_PROGRAMS_API_H

#include <Arduino.h>

/// IDLE or ERROR — same gates as program row on the touchscreen.
bool kiln_programs_selection_edit_allowed();

/// Full program selection state for the web UI (mirrors device memory).
void kiln_programs_serialize_json(String& out);

/// Apply web program selection + predefined cone/candle/soak. Returns false and sets `errJson` on error.
bool kiln_programs_apply_save_json(const String& body, String& errJson);

#endif

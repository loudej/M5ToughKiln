#ifndef KILN_PROGRAMS_API_H
#define KILN_PROGRAMS_API_H

#include <Arduino.h>

/// IDLE or ERROR — same gates as the program row on the touchscreen.
bool kiln_programs_selection_edit_allowed();

/// GET /api/programs — full program list + per-slot details (display units).
void kiln_programs_serialize_json(String& out);

/// POST /api/programs/save — JSON `{ activeIndex, cone?, candle?, soak? }`. Selects the active program;
/// when `activeIndex` is 0..3, also updates that predefined slot's cone/candle/soak (matches touch UI).
bool kiln_programs_apply_save_json(const String& body, String& errJson);

/// POST /api/programs/custom/append — appends a new "Custom N" program (no segments).
/// On success, writes `{ "index": N, "name": "..." }` to `out`.
bool kiln_programs_append_custom_json(String& out, String& errJson);

/// POST /api/programs/custom/segment — upsert one segment in one custom program.
/// Body: `{ programIndex, segmentIndex (-1 for new), target, rate, soakMin? }` (display units).
bool kiln_programs_upsert_segment_json(const String& body, String& errJson);

/// POST /api/programs/custom/segment/delete — remove one segment from a custom program.
/// Body: `{ programIndex, segmentIndex }`.
bool kiln_programs_delete_segment_json(const String& body, String& errJson);

#endif

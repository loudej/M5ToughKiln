#ifndef KILN_DASHBOARD_JSON_H
#define KILN_DASHBOARD_JSON_H

#include <Arduino.h>

/// Serializes main-screen-equivalent status for the read-only web dashboard (UTF-8 JSON).
void kiln_dashboard_serialize_status(String &out);

#endif

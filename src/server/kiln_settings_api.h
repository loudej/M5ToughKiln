#pragma once

#include <Arduino.h>

void kiln_settings_serialize_json(String& out);
bool kiln_settings_apply_json(const String& body, String& errJson);

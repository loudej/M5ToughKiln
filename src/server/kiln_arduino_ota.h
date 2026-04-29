#ifndef KILN_ARDUINO_OTA_H
#define KILN_ARDUINO_OTA_H

/// Drive Network OTA (ArduinoOTA) when Wi-Fi is connected. Required for PlatformIO
/// `upload_protocol = espota` (PIO `espota` client) — separate from ElegantOTA’s HTTP UI.
void kiln_arduino_ota_service();

#endif

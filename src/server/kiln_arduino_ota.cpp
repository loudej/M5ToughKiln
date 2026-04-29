#include "kiln_arduino_ota.h"
#include "kiln_wifi.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <ArduinoOTA.h>

void kiln_arduino_ota_service() {
    static bool s_ota_began = false;
    if (!kiln_wifi_station_connected()) {
        s_ota_began = false;
        return;
    }

    if (!s_ota_began) {
        // host for optional `upload_port = m5-kiln.local` (mDNS; not always resolved on Windows)
        ArduinoOTA.setHostname("m5-kiln");
        // Optional: setPassword and match the OTA env: upload_flags = --auth=...
        ArduinoOTA
            .onStart([]() {
                Serial.println("[Network OTA] start");
            })
            .onEnd([]() {
                Serial.println("\n[Network OTA] end, rebooting");
            })
            .onProgress([](unsigned int p, unsigned int t) {
                if (p == 0U || p == t || (p & 0x1FFFFU) == 0U) {
                    Serial.printf(" [Network OTA] %u / %u\r", p, t);
                }
            })
            .onError([](ota_error_t e) {
                const char* msg = "other";
                if (e == OTA_AUTH_ERROR) {
                    msg = "auth";
                } else if (e == OTA_BEGIN_ERROR) {
                    msg = "begin";
                } else if (e == OTA_CONNECT_ERROR) {
                    msg = "connect";
                } else if (e == OTA_RECEIVE_ERROR) {
                    msg = "receive";
                } else if (e == OTA_END_ERROR) {
                    msg = "end";
                }
                Serial.printf("\n[Network OTA] error: %d (%s)\n", static_cast<int>(e), msg);
            });
        ArduinoOTA.begin();
        s_ota_began = true;
    }
    ArduinoOTA.handle();
}

#else

void kiln_arduino_ota_service() {
}

#endif

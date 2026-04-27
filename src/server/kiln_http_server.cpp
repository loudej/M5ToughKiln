#include "kiln_http_server.h"
#include "kiln_wifi.h"

#include <WebServer.h>
#include <Arduino.h>

static WebServer server(80);
static bool       s_routes_registered = false;
static bool       s_server_begun    = false;

static const char INDEX_HTML[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1\"><title>Kiln</title></head>"
    "<body style=\"font-family:system-ui,sans-serif;margin:2rem;\">"
    "<h1>Kiln controller</h1>"
    "<p>This page is served from the M5Stack.</p>"
    "</body></html>";

static void register_routes() {
    if (s_routes_registered)
        return;
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", INDEX_HTML); });
    s_routes_registered = true;
}

void kiln_http_server_poll() {
    if (!kiln_wifi_station_connected())
        return;

    register_routes();

    if (!s_server_begun) {
        server.begin();
        s_server_begun = true;
    }

    server.handleClient();
}

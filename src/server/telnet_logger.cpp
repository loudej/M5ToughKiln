#include "telnet_logger.h"
#include "kiln_wifi.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <string>

// ── Configuration ────────────────────────────────────────────────────────────

static constexpr uint8_t  kMaxClients   = 4;
static constexpr uint8_t  kHistorySize  = 30;

// ── Ring buffer (last kHistorySize log entries) ───────────────────────────────

static std::string s_history[kHistorySize];
static uint8_t     s_history_write = 0;   // next slot to overwrite
static uint8_t     s_history_count = 0;   // entries stored (≤ kHistorySize)

static void history_push(const char* text) {
    s_history[s_history_write] = text;
    s_history_write = (s_history_write + 1) % kHistorySize;
    if (s_history_count < kHistorySize) ++s_history_count;
}

/// Replay history to a single client, oldest first, then print the separator.
static void history_replay(WiFiClient& client) {
    // Oldest entry sits kHistoryCount slots behind the write pointer.
    uint8_t start = (s_history_write - s_history_count + kHistorySize) % kHistorySize;
    for (uint8_t i = 0; i < s_history_count; ++i) {
        const std::string& line = s_history[(start + i) % kHistorySize];
        client.write(reinterpret_cast<const uint8_t*>(line.c_str()), line.size());
    }
    client.print("---\n");
}

// ── State ────────────────────────────────────────────────────────────────────

static uint16_t    s_port           = 23;
static WiFiServer  s_server(23);
static WiFiClient  s_clients[kMaxClients];
static bool        s_server_started = false;

// ── Internal helpers ─────────────────────────────────────────────────────────

static void broadcast(const char* text, size_t len) {
    for (auto& c : s_clients) {
        if (c && c.connected()) {
            c.write(reinterpret_cast<const uint8_t*>(text), len);
        }
    }
}

/// M5.Log callback — receives the fully-formatted log string (no ANSI colour
/// codes because we call setEnableColor(callback, false) during setup).
static void log_callback(esp_log_level_t /*level*/, bool /*is_color*/, const char* text) {
    history_push(text);
    broadcast(text, strlen(text));
}

static void start_server() {
    s_server = WiFiServer(s_port);
    s_server.begin();
    s_server_started = true;
    M5.Log.printf("[Telnet] log server listening on %s:%d\n",
                  WiFi.localIP().toString().c_str(), s_port);
}

static void stop_server() {
    for (auto& c : s_clients) {
        if (c) c.stop();
    }
    s_server.stop();
    s_server_started = false;
}

// ── Public API ───────────────────────────────────────────────────────────────

void telnet_logger_setup(uint16_t port) {
    s_port = port;

    // Enable the callback log target at full verbosity, no ANSI colour codes.
    M5.Log.setLogLevel(m5::log_target_callback, ESP_LOG_VERBOSE);
    M5.Log.setEnableColor(m5::log_target_callback, false);
    M5.Log.setCallback(log_callback);
}

void telnet_logger_service() {
    if (!kiln_wifi_station_connected()) {
        if (s_server_started)
            stop_server();
        return;
    }

    if (!s_server_started) {
        start_server();
    }

    // Accept a pending connection if a slot is free.
    WiFiClient incoming = s_server.available();
    if (incoming) {
        bool placed = false;
        for (auto& slot : s_clients) {
            if (!slot || !slot.connected()) {
                slot = std::move(incoming);
                history_replay(slot);
                placed = true;
                break;
            }
        }
        if (!placed) {
            incoming.print("[Telnet] Max clients reached.\n");
            incoming.stop();
        }
    }
}

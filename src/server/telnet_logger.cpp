#include "telnet_logger.h"
#include "kiln_wifi.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include <string>

// ── Configuration ────────────────────────────────────────────────────────────

static constexpr uint8_t  kMaxClients     = 4;
static constexpr uint8_t  kHistorySize    = 30;

// Bounded FIFO of pending log lines drained by telnet_logger_service() each loop.
// Sized to absorb a brief HTTP burst without losing lines but small enough that
// memory stays bounded if everything is paused.
static constexpr size_t   kQueueSlots     = 64;
static constexpr size_t   kMaxLineLen     = 240;

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
    uint8_t start = (s_history_write - s_history_count + kHistorySize) % kHistorySize;
    for (uint8_t i = 0; i < s_history_count; ++i) {
        const std::string& line = s_history[(start + i) % kHistorySize];
        client.write(reinterpret_cast<const uint8_t*>(line.c_str()), line.size());
    }
    client.print("---\n");
}

// ── Pending-line queue (producer = M5.Log callback; consumer = service loop) ──
//
// The callback runs on whatever task called M5.Log.printf — main loop, HTTP
// handler, controller task on core 1, etc. We must never block here. The mutex
// take uses a 0-tick timeout: under contention we drop the line rather than
// stall the producer. The queue itself is also bounded; oldest enqueued lines
// are preserved and new ones are dropped when full.

struct QueueSlot {
    char     data[kMaxLineLen];
    uint16_t len;
};

static QueueSlot         s_queue[kQueueSlots];
static size_t            s_q_head      = 0;   ///< next write index
static size_t            s_q_tail      = 0;   ///< next read index
static size_t            s_q_count     = 0;   ///< slots in use
static uint32_t          s_dropped     = 0;   ///< lines dropped since last report
static SemaphoreHandle_t s_q_mutex     = nullptr;

static void queue_push_line(const char* text) {
    if (!s_q_mutex) return;
    if (xSemaphoreTake(s_q_mutex, 0) != pdTRUE) {
        ++s_dropped;
        return;
    }
    if (s_q_count >= kQueueSlots) {
        ++s_dropped;
        xSemaphoreGive(s_q_mutex);
        return;
    }
    QueueSlot& slot = s_queue[s_q_head];
    size_t len      = strlen(text);
    if (len >= kMaxLineLen) len = kMaxLineLen - 1;
    memcpy(slot.data, text, len);
    slot.data[len] = '\0';
    slot.len       = static_cast<uint16_t>(len);
    s_q_head       = (s_q_head + 1) % kQueueSlots;
    ++s_q_count;
    xSemaphoreGive(s_q_mutex);
}

/// Pull the next pending line into `out`. Returns false when the queue is empty.
static bool queue_pop_line(QueueSlot& out) {
    if (!s_q_mutex) return false;
    if (xSemaphoreTake(s_q_mutex, pdMS_TO_TICKS(2)) != pdTRUE) return false;
    if (s_q_count == 0) {
        xSemaphoreGive(s_q_mutex);
        return false;
    }
    out      = s_queue[s_q_tail];
    s_q_tail = (s_q_tail + 1) % kQueueSlots;
    --s_q_count;
    xSemaphoreGive(s_q_mutex);
    return true;
}

// ── State ────────────────────────────────────────────────────────────────────

static uint16_t    s_port           = 23;
static WiFiServer  s_server(23);
static WiFiClient  s_clients[kMaxClients];
static bool        s_server_started = false;

// ── Internal helpers ─────────────────────────────────────────────────────────

/// Non-blocking broadcast: skips clients whose TCP send buffer can't accept the
/// line in full and forcibly drops them. We never call WiFiClient::write() with
/// a request larger than what's already free, which would otherwise block on
/// LWIP's send timeout (~10 s) and stall the entire main loop.
static void broadcast_line(const char* data, size_t len) {
    for (auto& c : s_clients) {
        if (!c || !c.connected()) continue;
        const int avail = c.availableForWrite();
        if (avail < 0 || static_cast<size_t>(avail) < len) {
            c.stop();
            continue;
        }
        c.write(reinterpret_cast<const uint8_t*>(data), len);
    }
}

/// M5.Log callback — receives the fully-formatted log string (no ANSI colour
/// codes because we call setEnableColor(callback, false) during setup). MUST NOT
/// BLOCK: just records the line in history and drops it into the FIFO.
static void log_callback(esp_log_level_t /*level*/, bool /*is_color*/, const char* text) {
    history_push(text);
    queue_push_line(text);
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

    if (!s_q_mutex) s_q_mutex = xSemaphoreCreateMutex();

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

    // Drain the FIFO. Bounded per call so we never spend too long here even
    // under a heavy log burst — leftovers are picked up next loop iteration.
    constexpr size_t kMaxDrainPerService = 16;
    QueueSlot slot;
    for (size_t i = 0; i < kMaxDrainPerService; ++i) {
        if (!queue_pop_line(slot)) break;
        broadcast_line(slot.data, slot.len);
    }

    // Surface drops occasionally so silent loss is visible in the log itself.
    static uint32_t s_last_reported_drops = 0;
    if (s_dropped != s_last_reported_drops) {
        const uint32_t newDrops = s_dropped - s_last_reported_drops;
        s_last_reported_drops   = s_dropped;
        // Re-enters log_callback → enqueues itself → broadcast next iteration.
        // Safe because we are not holding s_q_mutex here.
        M5.Log.printf("[Telnet] dropped %u log line(s) (queue contention or full)\n",
                      static_cast<unsigned>(newDrops));
    }
}

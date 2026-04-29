#include "kiln_wifi.h"
#include "../service/preferences_persistence.h"

#include <WiFi.h>
#include <Arduino.h>
#include <algorithm>
#include <unordered_map>

#ifndef WIFI_SCAN_RUNNING
#define WIFI_SCAN_RUNNING (-2)
#endif
#ifndef WIFI_SCAN_FAILED
#define WIFI_SCAN_FAILED (-1)
#endif

extern PreferencesPersistence persistence;

enum class ConnPhase { Idle, Disconnecting, Connecting };

enum class WifiAsyncScanAwait { Idle, WaitingResult };

static ConnPhase     s_phase = ConnPhase::Idle;
static std::string   s_join_ssid;
static std::string   s_join_pass;
static uint32_t      s_phase_ms = 0;

static WifiAsyncScanAwait          s_wifi_async_await       = WifiAsyncScanAwait::Idle;
static bool                        s_wifi_async_results_ready = false;
static std::vector<KilnWifiNetwork> s_wifi_async_scratch;

static void dedupe_and_sort(std::vector<KilnWifiNetwork>& nets) {
    std::unordered_map<std::string, int> best;
    for (const auto& n : nets) {
        if (n.ssid.empty())
            continue;
        auto it = best.find(n.ssid);
        if (it == best.end() || n.rssi > it->second)
            best[n.ssid] = n.rssi;
    }
    nets.clear();
    nets.reserve(best.size());
    for (const auto& p : best) {
        KilnWifiNetwork row;
        row.ssid = p.first;
        row.rssi = p.second;
        nets.push_back(std::move(row));
    }
    std::sort(nets.begin(), nets.end(),
              [](const KilnWifiNetwork& a, const KilnWifiNetwork& b) { return a.rssi > b.rssi; });
}

void kiln_wifi_station_begin_from_nvs() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    std::string ssid = persistence.loadWifiSsid();
    std::string pass = persistence.loadWifiPass();
    if (!ssid.empty())
        kiln_wifi_request_join(ssid, pass);
}

static void ensure_sta_mode() {
    if (WiFi.getMode() != WIFI_STA)
        WiFi.mode(WIFI_STA);
}

/** Poll async scan completion; fills `s_wifi_async_scratch` + `ready` flag when done. */
static void kiln_wifi_drive_async_scan_collect() {
    if (s_wifi_async_await != WifiAsyncScanAwait::WaitingResult)
        return;

    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING)
        return;

    /* Finished — success (n≥0), failed (<0 except running), or n==0. */
    s_wifi_async_await = WifiAsyncScanAwait::Idle;

    s_wifi_async_scratch.clear();

    if (n < 0) {
        WiFi.scanDelete();
        return;
    }

    s_wifi_async_scratch.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        KilnWifiNetwork row;
        row.ssid = WiFi.SSID(i).c_str();
        row.rssi = WiFi.RSSI(i);
        if (row.ssid.empty())
            continue;
        s_wifi_async_scratch.push_back(std::move(row));
    }

    WiFi.scanDelete();
    dedupe_and_sort(s_wifi_async_scratch);
    s_wifi_async_results_ready = true;
}

void kiln_wifi_scan(std::vector<KilnWifiNetwork>& out) {
    out.clear();
    ensure_sta_mode();
    int n = WiFi.scanNetworks(false, false, false, 300);
    if (n <= 0) {
        WiFi.scanDelete();
        return;
    }

    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        KilnWifiNetwork row;
        row.ssid = WiFi.SSID(i).c_str();
        row.rssi = WiFi.RSSI(i);
        if (row.ssid.empty())
            continue;
        out.push_back(std::move(row));
    }

    WiFi.scanDelete();
    dedupe_and_sort(out);
}

void kiln_wifi_scan_request_async() {
    ensure_sta_mode();
    /* Supersede undelivered results from a previous scan (e.g. fast timer retick). */
    if (s_wifi_async_results_ready) {
        s_wifi_async_scratch.clear();
        s_wifi_async_results_ready = false;
    }

    /* Do not overlap scans — Arduino returns WIFI_SCAN_RUNNING while in flight. */
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
        return;

    WiFi.scanDelete();

    WiFi.scanNetworks(/*async*/true, /*show_hidden*/false, /*passive*/false,
                      /*channel_time_ms_per_chan*/300);
    s_wifi_async_await = WifiAsyncScanAwait::WaitingResult;
}

bool kiln_wifi_take_completed_async_scan(std::vector<KilnWifiNetwork>& out) {
    if (!s_wifi_async_results_ready)
        return false;
    out.swap(s_wifi_async_scratch);
    s_wifi_async_results_ready = false;
    return true;
}

void kiln_wifi_request_join(const std::string& ssid, const std::string& pass) {
    if (ssid.empty())
        return;
    s_join_ssid = ssid;
    s_join_pass = pass;
    s_phase     = ConnPhase::Disconnecting;
    s_phase_ms  = millis();
    WiFi.disconnect(false);
}

void kiln_wifi_service() {
    kiln_wifi_drive_async_scan_collect();

    switch (s_phase) {
        case ConnPhase::Disconnecting: {
            wl_status_t st = WiFi.status();
            if (st == WL_DISCONNECTED || millis() - s_phase_ms > 900) {
                WiFi.mode(WIFI_STA);
                WiFi.setAutoReconnect(true);
                WiFi.begin(s_join_ssid.c_str(), s_join_pass.c_str());
                s_phase    = ConnPhase::Connecting;
                s_phase_ms = millis();
            }
            break;
        }
        case ConnPhase::Connecting:
            if (WiFi.status() == WL_CONNECTED) {
                s_phase = ConnPhase::Idle;
                persistence.saveWifiCredentials(s_join_ssid, s_join_pass);
            } else if (millis() - s_phase_ms > 45000)
                s_phase = ConnPhase::Idle;
            break;
        default:
            break;
    }
}

std::string kiln_wifi_ip_status_line() {
    if (s_phase == ConnPhase::Disconnecting)
        return "disconnecting";
    if (s_phase == ConnPhase::Connecting)
        return "connecting...";
    if (WiFi.status() == WL_CONNECTED)
        return WiFi.localIP().toString().c_str();
    return "-";
}

bool kiln_wifi_station_connect_blocking(const std::string& ssid, const std::string& pass,
                                        uint32_t timeout_ms) {
    if (ssid.empty())
        return false;

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), pass.c_str());

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms)
            return false;
        delay(200);
    }
    return true;
}

bool kiln_wifi_station_connected() {
    return WiFi.status() == WL_CONNECTED;
}

std::string kiln_wifi_station_ip() {
    if (!kiln_wifi_station_connected())
        return {};
    return WiFi.localIP().toString().c_str();
}

int kiln_wifi_station_rssi_dbm() {
    if (!kiln_wifi_station_connected())
        return 0;
    return WiFi.RSSI();
}

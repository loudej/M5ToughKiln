#include "kiln_wifi.h"
#include "../service/preferences_persistence.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <Arduino.h>
#include <algorithm>
#include <cstring>
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
    // disconnect(wifioff=false, eraseap=true): keep the radio on, but wipe the cached
    // SSID/BSSID from NVS so ESP-IDF picks fresh next association instead of reusing
    // the previous BSSID (which may now be the weakest mesh node).
    WiFi.disconnect(false, true);
}

void kiln_wifi_service() {
    kiln_wifi_drive_async_scan_collect();

    switch (s_phase) {
        case ConnPhase::Disconnecting: {
            wl_status_t st = WiFi.status();
            if (st == WL_DISCONNECTED || millis() - s_phase_ms > 900) {
                WiFi.mode(WIFI_STA);
                WiFi.setAutoReconnect(true);

                // Blocking scan to find the strongest BSSID for s_join_ssid. Adds a few seconds
                // to a join, which is acceptable here since this only runs at boot or on an
                // explicit reconnect — never while the kiln is firing.
                uint8_t best_bssid[6] = {0};
                bool    found_bssid   = false;
                int     best_rssi     = -127;
                const int n = WiFi.scanNetworks(false, false, false, 300);
                for (int i = 0; i < n; ++i) {
                    if (s_join_ssid == WiFi.SSID(i).c_str() && WiFi.RSSI(i) > best_rssi) {
                        best_rssi   = WiFi.RSSI(i);
                        memcpy(best_bssid, WiFi.BSSID(i), 6);
                        found_bssid = true;
                    }
                }
                WiFi.scanDelete();

                if (found_bssid) {
                    M5.Log.printf("[Wi-Fi] joining %s on BSSID "
                                  "%02x:%02x:%02x:%02x:%02x:%02x rssi=%d\n",
                                  s_join_ssid.c_str(),
                                  best_bssid[0], best_bssid[1], best_bssid[2],
                                  best_bssid[3], best_bssid[4], best_bssid[5],
                                  best_rssi);
                    WiFi.begin(s_join_ssid.c_str(), s_join_pass.c_str(), 0, best_bssid);
                } else {
                    M5.Log.printf("[Wi-Fi] joining %s — no scan result, fallback to any BSSID\n",
                                  s_join_ssid.c_str());
                    WiFi.begin(s_join_ssid.c_str(), s_join_pass.c_str());
                }

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

    // Periodic roaming: every 2 minutes, scan asynchronously for a stronger node on the
    // same SSID. If one is at least 8 dBm better than the current link, kick a reconnect
    // (which will run the Fix 2 BSSID-targeted scan in the Disconnecting handler). The
    // 8 dBm hysteresis prevents thrashing between two nodes with similar signal.
    static uint32_t s_roam_check_ms     = 0;
    static bool     s_roam_scan_pending = false;

    if (s_phase == ConnPhase::Idle && WiFi.status() == WL_CONNECTED) {
        const uint32_t now = millis();
        if (now - s_roam_check_ms >= 120000) {
            s_roam_check_ms     = now;
            kiln_wifi_scan_request_async();
            s_roam_scan_pending = true;
        }
    }

    if (s_roam_scan_pending) {
        std::vector<KilnWifiNetwork> scratch;
        if (kiln_wifi_take_completed_async_scan(scratch)) {
            s_roam_scan_pending     = false;
            const int   current_rssi = WiFi.RSSI();
            const std::string cur_ssid = WiFi.SSID().c_str();
            int best_rssi = current_rssi;
            for (const auto& net : scratch) {
                if (net.ssid == cur_ssid && net.rssi > best_rssi)
                    best_rssi = net.rssi;
            }
            if (best_rssi > current_rssi + 8) {
                M5.Log.printf("[Wi-Fi] roaming: current rssi=%d, best=%d on %s — reconnecting\n",
                              current_rssi, best_rssi, cur_ssid.c_str());
                kiln_wifi_request_join(s_join_ssid, s_join_pass);
            }
        }
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

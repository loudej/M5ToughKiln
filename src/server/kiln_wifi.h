#ifndef KILN_WIFI_H
#define KILN_WIFI_H

#include <cstdint>
#include <string>
#include <vector>

struct KilnWifiNetwork {
    std::string ssid;
    int         rssi;
};

/// Start station mode and join using credentials from NVS (non-blocking).
void kiln_wifi_station_begin_from_nvs();

/// Same AP may appear multiple times; keep strongest RSSI. Sorted strongest first.
void kiln_wifi_scan(std::vector<KilnWifiNetwork>& out);

/// Non-blocking: disconnect then join. Saved to NVS when association succeeds.
void kiln_wifi_request_join(const std::string& ssid, const std::string& pass);

/// Drive connection state machine (call from main loop).
void kiln_wifi_service();

/// IP row: "disconnecting" / "connecting..." / IPv4 / em dash when idle
std::string kiln_wifi_ip_status_line();

bool kiln_wifi_station_connect_blocking(const std::string& ssid, const std::string& pass,
                                        uint32_t timeout_ms);

bool kiln_wifi_station_connected();

std::string kiln_wifi_station_ip();

/// dBm when associated, or 0 if not connected
int kiln_wifi_station_rssi_dbm();

#endif

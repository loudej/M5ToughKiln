#ifndef KILN_HTTP_SERVER_H
#define KILN_HTTP_SERVER_H

/// When Wi-Fi is connected, serve the web dashboard, JSON status, and control tap.
/// High-traffic JSON routes are log-aggregated every 10s; other requests log per request.
/// GET /            — main page
/// GET /style.css, /app.js
/// GET /api/status — JSON (see kiln_dashboard_json)
/// GET /api/chart/trace — incremental firing traces (temperature + power % vs time)
/// POST /api/control/tap — same as main-screen START/STOP/DONE/RESET (applyStartStopTap)
/// GET /api/programs — program list + predefined cone/candle/soak + custom segments (display units)
/// POST /api/programs/save — JSON { activeIndex, cone?, candle?, soak? } — applies like device config screen
/// POST /api/programs/swap-previous — swap with stored previous selection (same as loop button)
/// GET/POST /update   — OTA firmware upload (ElegantOTA) when Wi-Fi is up

void kiln_http_server_poll();

#endif

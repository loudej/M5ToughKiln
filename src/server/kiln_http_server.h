#ifndef KILN_HTTP_SERVER_H
#define KILN_HTTP_SERVER_H

/// When Wi-Fi is connected, serve a read-only web dashboard and JSON status.
/// High-traffic JSON routes are log-aggregated every 10s; other requests log per request.
/// GET /            — main page
/// GET /style.css, /app.js
/// GET /api/status — JSON (see kiln_dashboard_json)
/// GET /api/chart/trace — incremental firing traces (temperature + power % vs time)

void kiln_http_server_poll();

#endif

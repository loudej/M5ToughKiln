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
/// POST /api/programs/save — JSON { activeIndex, cone?, candle?, soak? } — selects active program; updates the
/// predefined slot's cone/candle/soak when activeIndex is 0..3. Custom segment edits use the granular routes below.
/// POST /api/programs/custom/append — appends a new "Custom N" program (no segments). Returns { index, name }.
/// POST /api/programs/custom/segment — JSON { programIndex, segmentIndex (-1 = new), target, rate, soakMin? } —
/// upserts one segment in one custom program (display units; same limits as touch popup).
/// POST /api/programs/custom/segment/delete — JSON { programIndex, segmentIndex } — removes one segment.
/// POST /api/programs/swap-previous — swap with stored previous selection (same as loop button)
/// GET/POST /update   — OTA firmware upload (ElegantOTA) when Wi-Fi is up
/// POST /api/settings/save — JSON `{ tempUnit?, kp?, ki?, kd? }` (Wi-Fi unchanged; use device UI).
/// GET /settings — web UI for those settings only.
/// GET /api/settings — JSON same fields as saved.

void kiln_http_server_poll();

#endif

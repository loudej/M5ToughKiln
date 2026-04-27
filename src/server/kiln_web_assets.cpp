#include "kiln_web_assets.h"

/** Embedded static dashboard (avoid LittleFS partition + upload workflow on ESP32). */

const char KILN_WEB_INDEX_HTML[] = R"--HTML--(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>Kiln</title>
  <link rel="stylesheet" href="/style.css"/>
</head>
<body>
  <main class="panel">
    <header class="hdr"><h1>Kiln</h1><span class="unit" id="hdrUnit"></span></header>

    <table class="status-table">
      <tr>
        <th scope="row">Program</th>
        <td colspan="3" class="strong" id="programName"></td>
      </tr>
      <tr>
        <th scope="row">Temp</th>
        <td colspan="3" class="hero" id="temperature"></td>
      </tr>
      <tr>
        <th scope="row">Status</th>
        <td colspan="3" id="statusLine"></td>
      </tr>
      <tr id="powerSection">
        <th scope="row">Power</th>
        <td class="pct mono val-left" id="powerPct"></td>
        <td class="bar-cell"><div class="barwrap"><div class="bar bg"><div id="fillPower" class="fill pw"></div></div></div></td>
        <td class="val-right"></td>
      </tr>
      <tr id="targetSection">
        <th scope="row">Target</th>
        <td class="pct mono val-left" id="targetLeft"></td>
        <td class="bar-cell"><div class="barwrap"><div class="bar bg"><div id="fillThermal" class="fill th"></div></div></div></td>
        <td class="pct mono val-right" id="targetPeak"></td>
      </tr>
      <tr id="timeSection">
        <th scope="row">Time</th>
        <td class="pct mono val-left" id="timeLeft"></td>
        <td class="bar-cell"><div class="barwrap"><div class="bar bg"><div id="fillTime" class="fill tm"></div></div></div></td>
        <td class="pct mono val-right" id="timeRight"></td>
      </tr>
    </table>

    <section class="chart-wrap">
      <div class="chart-label">Schedule</div>
      <svg id="chartSvg" class="chart-svg" viewBox="0 0 400 180" preserveAspectRatio="xMidYMid meet"></svg>
    </section>

    <p class="fine">Read-only dashboard (polls controller state).</p>
  </main>
  <script defer src="/app.js"></script>
</body>
</html>
)--HTML--";

const char KILN_WEB_STYLE_CSS[] = R"--CSS--(
:root {
  --bg:#121418;
  --panel:#1a1d23;
  --muted:#94a3b8;
  --txt:#f1f5f9;
  --bar:#273043;
}
* { box-sizing:border-box; }
body {
  margin:0;
  font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  background:var(--bg);
  color:var(--txt);
  padding:clamp(12px,3vw,28px);
  font-size:15px;
}
.panel {
  max-width:520px;
  margin:0 auto;
  background:var(--panel);
  border-radius:12px;
  padding:16px 18px 20px;
  border:1px solid #2a3240;
}
.hdr { display:flex;justify-content:space-between;align-items:center;margin-bottom:12px; }
.hdr h1 { margin:0;font-size:1.25rem;font-weight:650;letter-spacing:.02em; }
.unit { color:var(--muted);font-size:.9rem; }

.status-table {
  width:100%;
  border-collapse:collapse;
  margin-bottom:14px;
}
.status-table th {
  width:1%;
  white-space:nowrap;
  padding-right:12px;
  text-align:left;
  color:var(--muted);
  font-size:.82rem;
  text-transform:uppercase;
  letter-spacing:.06em;
  font-weight:normal;
  vertical-align:middle;
}
.status-table td {
  padding:4px 0;
  vertical-align:middle;
}
.status-table .bar-cell {
  width:100%;
  padding:0 8px;
}
.status-table .val-left,
.status-table .val-right {
  text-align:right;
  white-space:nowrap;
  padding:0 8px;
}
.status-table .val-left {
  width:1%;
}
.status-table .pct {
  font-variant-numeric:tabular-nums;
}
.status-table td[colspan="3"] {
  word-break:break-word;
}

.hero {
  font-size:clamp(2rem,12vw,3rem);
  font-weight:650;
  font-variant-numeric:tabular-nums;
  text-align:right;
}
.strong { font-weight:550; font-size:1rem; }

.chart-wrap { margin-bottom:14px; }
.chart-label {
  color:var(--muted);
  font-size:.82rem;
  text-transform:uppercase;
  letter-spacing:.06em;
  margin-bottom:8px;
}
.chart-svg {
  width:100%;
  height:auto;
  aspect-ratio:400 / 180;
  max-height:min(220px,50vh);
  border-radius:6px;
  border:1px solid #374151;
  background:#101218;
}

.mono { font-family:ui-monospace,Menlo,Consolas,monospace; font-size:.88rem; }

.barwrap {
  height:14px;
}
.bar {
  height:100%;
  border-radius:5px;
  overflow:hidden;
}
.bar.bg {
  background:var(--bar);
}
.fill {
  height:100%;
  width:0%;
  border-radius:5px;
  transition:width .25s ease;
}
.fill.pw { background:#eab308; }
.fill.th.red { background:#ef4444; }
.fill.th.green { background:#22c55e; }
.fill.th.blue { background:#3b82f6; }
.fill.th.grey { background:#64748b; }
.fill.tm { background:#64748f; }

.hidden-row { visibility:hidden;height:0;margin:0;padding:0;overflow:hidden;}

.fine {
  margin:14px 0 0;
  font-size:.75rem;
  color:#64748b;
  text-align:center;
}

.warn { color:#fb923c; }
.good { color:#4ade80; }
.bad { color:#f87171; }
.norm { color:var(--txt); }
)--CSS--";

const char KILN_WEB_APP_JS[] = R"--JS--(
/** Maps kiln_dashboard_json `statusTone` (normal|warn|good|bad) to CSS classes (.norm, .warn, …). */
const toneClass = { warn:'warn', good:'good', bad:'bad', normal:'norm' };

let lastStatus = null;
let traceActual = [];
let tracePower = [];
let traceClientRev = 0;
let traceSince = 0;

let statusPollBusy = false;
let tracePollBusy = false;
let traceChain = Promise.resolve();

function applyTraceJson(tj) {
  if (tj.resync) {
    traceActual = Array.isArray(tj.actual) ? tj.actual.slice() : [];
    tracePower = Array.isArray(tj.power) ? tj.power.slice() : [];
  } else {
    const da = tj.actual || [];
    const dp = tj.power || [];
    for (let i = 0; i < da.length; i++) traceActual.push(da[i]);
    for (let i = 0; i < dp.length; i++) tracePower.push(dp[i]);
  }
  traceSince = typeof tj.totalPoints === 'number' ? tj.totalPoints : traceActual.length;
  traceClientRev = typeof tj.revision === 'number' ? tj.revision : traceClientRev;
}

function pullTraceDelta() {
  const step = traceChain.then(async () => {
    const r = await fetch(
      '/api/chart/trace?since=' + traceSince + '&rev=' + traceClientRev,
      { cache: 'no-store' }
    );
    applyTraceJson(await r.json());
  });
  traceChain = step.catch(function () {});
  return step;
}

function renderDashboardChart() {
  if (lastStatus) {
    renderChart(lastStatus.chart, lastStatus, traceActual, tracePower);
  }
}

function setBar(fillId, pct, visible) {
  const fill = document.getElementById(fillId);
  if (!fill) return;
  fill.style.width = Math.max(0, Math.min(100, pct)) + '%';
  const barw = fill.closest('.barwrap');
  if (barw) barw.style.display = visible ? '' : 'none';
}

function setThermalTone(elFill, tone) {
  elFill.classList.remove('red','green','blue','grey');
  const t = tone || 'grey';
  elFill.classList.add(t === 'grey' ? 'grey' : t);
}

function renderChart(chart, statusJ, trActual, trPower) {
  const svg = document.getElementById('chartSvg');
  if (!svg || !chart || !chart.hasData) {
    if (svg) svg.innerHTML = '';
    const cw = svg && svg.closest('.chart-wrap');
    if (cw) cw.style.display = 'none';
    return;
  }
  const cw = svg.closest('.chart-wrap');
  if (cw) cw.style.display = '';

  const xMax = chart.xMaxSec || 1;
  const yMin = chart.yMinC;
  const yMax = chart.yMaxC;
  const span = Math.max(yMax - yMin, 1);
  const W = 400, H = 180;
  function sx(t) { return (t / xMax) * W; }
  function syTemp(c) { return H - ((c - yMin) / span) * H; }
  function syPwr(p) {
    const x = Math.max(0, Math.min(100, Number(p)));
    return H - (x / 100) * H;
  }

  let ds = '';
  const sch = chart.schedule || [];
  for (let i = 0; i < sch.length; i++) {
    const pt = sch[i];
    const x = sx(pt.t), y = syTemp(pt.c);
    ds += (i === 0 ? 'M' : 'L') + x.toFixed(1) + ',' + y.toFixed(1) + ' ';
  }

  let actPts = Array.isArray(trActual) ? trActual.slice() : [];
  let pwrPts = Array.isArray(trPower) ? trPower.slice() : [];

  const el = chart.elapsedSec != null ? chart.elapsedSec : 0;
  const actualC = chart.actualTempC;
  if (actualC != null && Number.isFinite(actualC)) {
    if (actPts.length && actPts[actPts.length - 1].t === el) {
      actPts[actPts.length - 1] = { t: el, c: actualC };
    } else if (actPts.length === 0 || actPts[actPts.length - 1].t < el) {
      actPts.push({ t: el, c: actualC });
    }
  }

  const pp = statusJ && statusJ.powerPercent != null ? Number(statusJ.powerPercent) : null;
  if (pp != null && Number.isFinite(pp)) {
    if (pwrPts.length && pwrPts[pwrPts.length - 1].t === el) {
      pwrPts[pwrPts.length - 1] = { t: el, p: pp };
    } else if (pwrPts.length === 0 || pwrPts[pwrPts.length - 1].t < el) {
      pwrPts.push({ t: el, p: pp });
    }
  }

  let da = '';
  for (let i = 0; i < actPts.length; i++) {
    const pt = actPts[i];
    const x = sx(pt.t), y = syTemp(pt.c);
    da += (i === 0 ? 'M' : 'L') + x.toFixed(1) + ',' + y.toFixed(1) + ' ';
  }

  let dp = '';
  for (let i = 0; i < pwrPts.length; i++) {
    const pt = pwrPts[i];
    const x = sx(pt.t), y = syPwr(pt.p);
    dp += (i === 0 ? 'M' : 'L') + x.toFixed(1) + ',' + y.toFixed(1) + ' ';
  }

  const tx = sx(el);
  const ty = syTemp(chart.targetTempC || 0);
  const ax = sx(el);
  const ay = syTemp(chart.actualTempC || 0);

  svg.setAttribute('viewBox', '0 0 ' + W + ' ' + H);
  let inner = '<path fill="none" stroke="#94a3b8" stroke-width="2" d="' + ds.trim() + '" />';
  if (da.trim()) {
    inner += '<path fill="none" stroke="#ef4444" stroke-width="2" d="' + da.trim() + '" />';
  }
  if (dp.trim()) {
    inner += '<path fill="none" stroke="#eab308" stroke-width="1.8" opacity="0.85" d="' + dp.trim() + '" />';
  }
  inner +=
    '<circle cx="' + tx + '" cy="' + ty + '" r="9" fill="none" stroke="#cbd5e1" stroke-width="2"/>' +
    '<path stroke="#ef4444" stroke-width="2" d="M' +
    (ax - 6) +
    ',' +
    ay +
    ' L' +
    (ax + 6) +
    ',' +
    ay +
    ' M' +
    ax +
    ',' +
    (ay - 6) +
    ' L' +
    ax +
    ',' +
    (ay + 6) +
    '" />';
  svg.innerHTML = inner;
}

async function pollTrace() {
  if (tracePollBusy) return;
  tracePollBusy = true;
  try {
    await pullTraceDelta();
    renderDashboardChart();
  } catch (e) {
    console.warn(e);
  } finally {
    tracePollBusy = false;
  }
}

async function poll() {
  if (statusPollBusy) return;
  statusPollBusy = true;
  try {
    const r = await fetch('/api/status', { cache: 'no-store' });
    const j = await r.json();
    lastStatus = j;

    document.getElementById('hdrUnit').textContent = j.tempUnit ? ('°' + j.tempUnit) : '';
    document.getElementById('programName').textContent = j.programName || '';
    document.getElementById('temperature').textContent = j.temperature || '';

    const st = document.getElementById('statusLine');
    st.textContent = j.statusText || '';
    st.className = toneClass[j.statusTone] || 'norm';

    document.getElementById('powerPct').textContent =
      (j.powerPercent != null ? j.powerPercent : 0) + '%';

    const b = j.bars || {};
    const bp = b.power || {};
    const bt = b.thermal || {};
    const btm = b.timeProgress || {};

    setBar('fillPower', bp.percent !== undefined ? bp.percent : 0, bp.visible);
    const ft = document.getElementById('fillThermal');
    setBar('fillThermal', bt.percent !== undefined ? bt.percent : 0, bt.visible);
    if (ft && bt.visible) setThermalTone(ft, bt.tone);

    setBar('fillTime', btm.percent !== undefined ? btm.percent : 0, btm.visible);

    const tg = j.target || {};
    document.getElementById('targetLeft').textContent = tg.targetLeft || '';
    document.getElementById('targetPeak').textContent = tg.targetPeak || '';

    const tm = j.time || {};
    document.getElementById('timeLeft').textContent = tm.timeLeft || '';
    document.getElementById('timeRight').textContent = tm.timeRight || '';

    const revMismatch =
      typeof j.traceRevision === 'number' && j.traceRevision !== traceClientRev;
    if (revMismatch) {
      traceActual = [];
      tracePower = [];
      traceSince = 0;
      await pullTraceDelta();
    }

    renderDashboardChart();
  } catch (e) {
    console.warn(e);
  } finally {
    statusPollBusy = false;
  }
}

poll();
pollTrace();
setInterval(poll, 500);
setInterval(pollTrace, 750);
)--JS--";

const size_t KILN_WEB_INDEX_HTML_LEN = sizeof(KILN_WEB_INDEX_HTML) - 1;
const size_t KILN_WEB_STYLE_CSS_LEN  = sizeof(KILN_WEB_STYLE_CSS) - 1;
const size_t KILN_WEB_APP_JS_LEN     = sizeof(KILN_WEB_APP_JS) - 1;

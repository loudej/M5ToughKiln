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
    <header class="hdr">
    <h1>Kiln</h1>
    <div class="hdr-side"><a href="/settings" class="hdr-link">Settings</a></div>
  </header>

    <table class="status-table">
      <tr>
        <th scope="row">Program</th>
        <td colspan="3">
          <div class="program-row-btn">
            <button type="button" id="btnProgOpen" class="prog-open-btn"></button>
            <button type="button" id="btnProgSwap" class="prog-swap-btn" title="Swap with previous program slot"><svg class="prog-swap-ic" viewBox="0 0 24 24" aria-hidden="true" xmlns="http://www.w3.org/2000/svg"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M17 1l4 4-4 4"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M3 11V9a4 4 0 014-4h14"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M7 23l-4-4 4-4"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M21 13v2a4 4 0 01-4 4H3"/></svg></button>
          </div>
        </td>
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

    <div class="ctrl-row">
      <button type="button" id="ctrlTap" class="ctrl-btn ctrl-start">START</button>
    </div>
  </main>

  <div id="progBackdrop" class="prog-backdrop hidden"></div>
  <div id="progPanel" class="prog-panel hidden" role="dialog" aria-modal="true">
    <div class="prog-panel-inner">
      <button type="button" id="progClose" class="prog-close-btn" aria-label="Close">&times;</button>
      <h2 class="prog-panel-title">Programs</h2>

      <label class="prog-field"><span class="prog-lbl">Program</span>
        <select id="progSelect"></select>
      </label>

      <div id="progPredefinedWrap">
        <label class="prog-field"><span class="prog-lbl">Cone</span>
          <input id="progCone" type="text" autocomplete="off" maxlength="12"/>
        </label>
        <label class="prog-field"><span class="prog-lbl">Candle (min)</span>
          <input id="progCandle" type="number" step="1"/>
        </label>
        <label class="prog-field"><span class="prog-lbl">Soak (min)</span>
          <input id="progSoak" type="number" step="1"/>
        </label>
      </div>

      <div id="progCustomWrap" class="hidden">
        <div class="prog-muted">Segments (read-only)</div>
        <div id="progCustomSegments" class="prog-seg-list"></div>
      </div>

      <button type="button" id="progApply" class="prog-apply-btn">Apply</button>
    </div>
  </div>
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
.hdr-side { display:flex; align-items:center; gap:10px; }
.hdr-link {
  font-size:.85rem; color:#94a3b8;
  text-decoration:none;
}
.hdr-link:hover { color:#f1f5f9; }

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

.ctrl-row {
  margin-top:16px;
}
.ctrl-btn {
  width:100%;
  border:none;
  border-radius:10px;
  padding:14px 18px;
  font-size:1.05rem;
  font-weight:650;
  letter-spacing:.04em;
  cursor:pointer;
  color:#f8fafc;
  font-family:inherit;
}
.ctrl-btn:disabled {
  opacity:0.55;
  cursor:not-allowed;
}
.ctrl-start { background:#16a34a; }
.ctrl-stop { background:#dc2626; }
.ctrl-done { background:#475569; }
.ctrl-reset { background:#b91c1c; }

.hidden { display:none !important; }

.program-row-btn {
  display:flex;
  gap:8px;
  align-items:stretch;
  width:100%;
}
.prog-open-btn {
  flex:1;
  text-align:left;
  border:1px solid #3b4a5c;
  border-radius:8px;
  background:#0f1318;
  color:var(--txt);
  font-size:1rem;
  font-weight:550;
  padding:8px 10px;
  cursor:pointer;
  font-family:inherit;
  min-width:0;
}
.prog-swap-btn {
  flex:0 0 48px;
  border:1px solid #3b4a5c;
  border-radius:8px;
  background:#0f1318;
  color:var(--txt);
  cursor:pointer;
  font-family:inherit;
  padding:0;
  display:flex;
  align-items:center;
  justify-content:center;
}
.prog-swap-ic {
  width:22px;
  height:22px;
  display:block;
}
.prog-open-btn:disabled {
  opacity:0.45;
  cursor:not-allowed;
}

.prog-swap-btn:disabled {
  opacity:0.45;
  cursor:not-allowed;
}

.prog-backdrop {
  position:fixed;
  inset:0;
  background:rgba(0,0,0,0.55);
  z-index:100;
}
.prog-panel {
  position:fixed;
  z-index:101;
  left:50%;
  top:50%;
  transform:translate(-50%,-50%);
  width:min(480px, 100% - 32px);
  max-height:min(90vh, 640px);
  overflow:auto;
  background:var(--panel);
  border:1px solid #374151;
  border-radius:12px;
  box-shadow:0 16px 40px rgba(0,0,0,0.45);
}
.prog-panel-inner {
  padding:18px;
  position:relative;
}
.prog-close-btn {
  position:absolute;
  top:10px;
  right:12px;
  border:none;
  background:none;
  color:var(--muted);
  font-size:1.6rem;
  line-height:1;
  cursor:pointer;
  padding:4px 8px;
}
.prog-panel-title {
  margin:0 0 14px;
  font-size:1.15rem;
  font-weight:650;
}
.prog-field {
  display:flex;
  flex-direction:column;
  gap:4px;
  margin-bottom:12px;
}
.prog-field .prog-lbl {
  font-size:.78rem;
  color:var(--muted);
  text-transform:uppercase;
  letter-spacing:.05em;
}
.prog-field input,
.prog-field select {
  border:1px solid #374151;
  border-radius:6px;
  background:#101218;
  color:var(--txt);
  padding:8px 10px;
  font-size:.95rem;
  font-family:inherit;
}
.prog-muted {
  font-size:.82rem;
  color:var(--muted);
  margin-bottom:8px;
}
.prog-seg-list {
  display:flex;
  flex-direction:column;
  gap:6px;
}
.prog-seg-row {
  font-size:.88rem;
  padding:8px 10px;
  border-radius:6px;
  background:#101218;
  border:1px solid #2d3748;
}
.prog-apply-btn {
  width:100%;
  margin-top:14px;
  padding:12px;
  border:none;
  border-radius:8px;
  background:#2563eb;
  color:#fff;
  font-weight:650;
  cursor:pointer;
  font-size:1rem;
}
.prog-apply-btn:hover { filter:brightness(1.06); }
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
let tapBusy = false;
let programsData = null;

function programSelectionLocked(j) {
  const ks = (j && j.kilnState) ? j.kilnState : 'idle';
  return ks !== 'idle' && ks !== 'error';
}

function updateProgramRow(j) {
  const o = document.getElementById('btnProgOpen');
  const sw = document.getElementById('btnProgSwap');
  if (o) o.textContent = j.programName || '';
  const ks = (j && j.kilnState) ? j.kilnState : 'idle';
  const locked = ks !== 'idle' && ks !== 'error';
  if (o) o.disabled = locked;
  if (sw) sw.disabled = locked || !j.previousSelectionValid || ks !== 'idle';
}

async function fetchProgramsJson() {
  const r = await fetch('/api/programs', { cache: 'no-store' });
  programsData = await r.json();
  return programsData;
}

function fillProgSelect() {
  const sel = document.getElementById('progSelect');
  if (!sel || !programsData) return;
  sel.innerHTML = '';
  const pre = programsData.predefined || [];
  for (let i = 0; i < pre.length; i++) {
    const p = pre[i];
    const opt = document.createElement('option');
    opt.value = String(p.slot);
    opt.textContent = p.name;
    sel.appendChild(opt);
  }
  const cust = programsData.custom || [];
  for (let i = 0; i < cust.length; i++) {
    const p = cust[i];
    const opt = document.createElement('option');
    opt.value = String(4 + p.index);
    opt.textContent = p.name;
    sel.appendChild(opt);
  }
}

function fmtSegNum(x) {
  if (typeof x !== 'number' || !isFinite(x)) return '--';
  return String(Math.round(x * 10) / 10);
}

function renderCustomSegments(customIdx) {
  const box = document.getElementById('progCustomSegments');
  const tu = (programsData && programsData.tempUnit) ? programsData.tempUnit : '';
  box.innerHTML = '';
  const custList = programsData ? programsData.custom : [];
  let cust = null;
  for (let i = 0; i < custList.length; i++) {
    if (custList[i].index === customIdx) {
      cust = custList[i];
      break;
    }
  }
  if (!cust || !cust.segments || !cust.segments.length) {
    box.textContent = '(no segments)';
    return;
  }
  for (let i = 0; i < cust.segments.length; i++) {
    const s = cust.segments[i];
    const row = document.createElement('div');
    row.className = 'prog-seg-row mono';
    row.textContent =
      '#' +
      (i + 1) +
      '  ' +
      fmtSegNum(s.target) +
      '\xb0' +
      tu +
      '  \xb7  ' +
      fmtSegNum(s.rate) +
      '\xb0' +
      tu +
      '/h  \xb7  soak ' +
      s.soakMin +
      ' min';
    box.appendChild(row);
  }
}

function syncFieldsFromProgSelect() {
  const sel = document.getElementById('progSelect');
  const preWrap = document.getElementById('progPredefinedWrap');
  const custWrap = document.getElementById('progCustomWrap');
  if (!sel || !programsData) return;
  const idx = parseInt(sel.value, 10);
  if (idx <= 3) {
    preWrap.classList.remove('hidden');
    custWrap.classList.add('hidden');
    const pre = programsData.predefined[idx];
    const coneEl = document.getElementById('progCone');
    const cdEl = document.getElementById('progCandle');
    const skEl = document.getElementById('progSoak');
    if (coneEl) coneEl.value = pre && pre.cone != null ? String(pre.cone) : '';
    if (cdEl) cdEl.value = pre && pre.candle ? String(pre.candle) : '';
    if (skEl) skEl.value = pre && pre.soak ? String(pre.soak) : '';
  } else {
    preWrap.classList.add('hidden');
    custWrap.classList.remove('hidden');
    renderCustomSegments(idx - 4);
  }
}

async function openProgPanel() {
  if (lastStatus && programSelectionLocked(lastStatus)) return;
  await fetchProgramsJson();
  fillProgSelect();
  const sel = document.getElementById('progSelect');
  const ai = programsData.activeIndex;
  sel.value = String(ai);
  syncFieldsFromProgSelect();
  document.getElementById('progBackdrop').classList.remove('hidden');
  document.getElementById('progPanel').classList.remove('hidden');
}

function closeProgPanel() {
  const b = document.getElementById('progBackdrop');
  const p = document.getElementById('progPanel');
  if (b) b.classList.add('hidden');
  if (p) p.classList.add('hidden');
}

async function applyProgPanel() {
  const sel = document.getElementById('progSelect');
  const ai = parseInt(sel.value, 10);
  const payload = { activeIndex: ai };
  if (ai <= 3) {
    payload.cone = document.getElementById('progCone').value.trim();
    const cRaw = document.getElementById('progCandle').value.trim();
    const sRaw = document.getElementById('progSoak').value.trim();
    payload.candle = cRaw === '' ? 0 : parseInt(cRaw, 10);
    payload.soak = sRaw === '' ? 0 : parseInt(sRaw, 10);
  }
  const r = await fetch('/api/programs/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  if (!r.ok) {
    console.warn('programs save failed');
    return;
  }
  closeProgPanel();
  await poll();
}

async function swapProgPrevious() {
  const sw = document.getElementById('btnProgSwap');
  if (!sw || sw.disabled) return;
  if (lastStatus && programSelectionLocked(lastStatus)) return;
  await fetch('/api/programs/swap-previous', { method: 'POST', cache: 'no-store' });
  await poll();
}

function ctrlLabelForKilnState(ks) {
  if (!ks) return 'START';
  switch (ks) {
    case 'idle': return 'START';
    case 'ramping':
    case 'soaking':
    case 'cooling': return 'STOP';
    case 'done': return 'DONE';
    case 'error': return 'RESET';
    default: return 'START';
  }
}

function ctrlClassForKilnState(ks) {
  if (!ks || ks === 'idle') return 'ctrl-start';
  if (ks === 'ramping' || ks === 'soaking' || ks === 'cooling') return 'ctrl-stop';
  if (ks === 'done') return 'ctrl-done';
  if (ks === 'error') return 'ctrl-reset';
  return 'ctrl-start';
}

function updateCtrlButton(j) {
  const btn = document.getElementById('ctrlTap');
  if (!btn) return;
  const ks = (j && j.kilnState) ? j.kilnState : 'idle';
  btn.textContent = ctrlLabelForKilnState(ks);
  btn.className = 'ctrl-btn ' + ctrlClassForKilnState(ks);
}

async function onCtrlTap() {
  if (tapBusy) return;
  tapBusy = true;
  const btn = document.getElementById('ctrlTap');
  if (btn) btn.disabled = true;
  try {
    const r = await fetch('/api/control/tap', { method: 'POST', cache: 'no-store' });
    if (r.ok) {
      const j = await r.json();
      if (j && j.kilnState) {
        if (!lastStatus) lastStatus = {};
        lastStatus.kilnState = j.kilnState;
        updateCtrlButton(lastStatus);
      }
    }
    await poll();
  } catch (e) {
    console.warn(e);
  } finally {
    tapBusy = false;
    if (btn) btn.disabled = false;
  }
}

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

    updateCtrlButton(j);

    updateProgramRow(j);

    if (programSelectionLocked(j)) closeProgPanel();

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

(function () {
  const btn = document.getElementById('ctrlTap');
  if (btn) btn.addEventListener('click', onCtrlTap);
})();

(function () {
  const openBtn = document.getElementById('btnProgOpen');
  if (openBtn) openBtn.addEventListener('click', openProgPanel);
  const swapBtn = document.getElementById('btnProgSwap');
  if (swapBtn) swapBtn.addEventListener('click', swapProgPrevious);
  const bd = document.getElementById('progBackdrop');
  if (bd) bd.addEventListener('click', closeProgPanel);
  const cls = document.getElementById('progClose');
  if (cls) cls.addEventListener('click', closeProgPanel);
  const ap = document.getElementById('progApply');
  if (ap) ap.addEventListener('click', applyProgPanel);
  const sel = document.getElementById('progSelect');
  if (sel) sel.addEventListener('change', syncFieldsFromProgSelect);
})();
)--JS--";

const char KILN_WEB_SETTINGS_HTML[] = R"--SETS--(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>Kiln Settings</title>
  <link rel="stylesheet" href="/style.css"/>
  <style>
    .sett-hdr { width:100%; display:flex; align-items:center; gap:12px; margin-bottom:16px; }
    .sett-back {
      flex:0 0 auto; border:1px solid #3b4a5c; border-radius:8px;
      background:#0f1318; color:#f1f5f9; text-decoration:none; padding:8px 14px;
      font-size:.95rem; font-family:inherit; cursor:pointer;
    }
    .sett-hdr h2 { flex:1; margin:0; font-size:1.22rem; text-align:center; font-weight:650; letter-spacing:.02em; }
    .sett-section { margin-top:8px; }
    .sett-grid {
      display:grid;
      grid-template-columns:auto 1fr;
      gap:10px 14px;
      align-items:center;
    }
    .sett-grid .sett-units-wrap { justify-self:end; width:100%; max-width:320px;}
    .sett-grid input.pid-inp {
      width:100%; max-width:320px; box-sizing:border-box; justify-self:end;
      border:1px solid #374151; border-radius:6px; background:#101218; color:#f1f5f9;
      padding:8px 10px; font-size:.95rem; font-family:inherit;
    }
    .sett-grid .prog-lbl { margin:0; }
    .sett-units button {
      flex:1; padding:10px 12px; border:1px solid #374151; border-radius:8px;
      background:#101218; color:#f1f5f9; cursor:pointer; font:inherit;
    }
    .sett-units button.on {
      border-color:#3b82f6;
      background:#1e293b;
    }
    .sett-units { display:flex; gap:8px; }
    .sett-muted { grid-column:1/-1;font-size:.8rem;color:#64748b;margin:4px 0 10px;line-height:1.35; }
  </style>
</head>
<body>
  <main class="panel">
    <header class="sett-hdr">
      <a class="sett-back" href="/" id="settBack">&larr; Back</a>
      <h2>Settings</h2>
    </header>

    <div class="sett-section">
      <p class="sett-muted">Leaving empty uses the firmware defaults (shown faded in each box).</p>
      <div class="sett-grid">
        <span class="prog-lbl">Temperature unit</span>
        <div class="sett-units-wrap">
          <div class="sett-units">
            <button type="button" id="btnUf">&deg; F</button>
            <button type="button" id="btnUc">&deg; C</button>
          </div>
        </div>

        <span class="prog-lbl">Kp</span>
        <input id="fldKp" class="pid-inp" type="text" inputmode="decimal" maxlength="24" autocomplete="off"/>

        <span class="prog-lbl">Ki</span>
        <input id="fldKi" class="pid-inp" type="text" inputmode="decimal" maxlength="24" autocomplete="off"/>

        <span class="prog-lbl">Kd</span>
        <input id="fldKd" class="pid-inp" type="text" inputmode="decimal" maxlength="24" autocomplete="off"/>
      </div>
    </div>
  </main>
  <script>
(function () {
  function unitFromApi(j) {
    var s = String(j.tempUnit || 'F').toUpperCase();
    return (s.charAt(0) === 'C') ? 'C' : 'F';
  }

  function jsonPidField(el) {
    var t = el.value.trim();
    if (!t.length) return null;
    var v = parseFloat(t.replace(',', '.'));
    return (Number.isFinite(v) ? v : null);
  }

  async function pull() {
    var r = await fetch('/api/settings', { cache: 'no-store' });
    var j = await r.json();
    document.getElementById('fldKp').value = (j.kp != null) ? String(j.kp) : '';
    document.getElementById('fldKi').value = (j.ki != null) ? String(j.ki) : '';
    document.getElementById('fldKd').value = (j.kd != null) ? String(j.kd) : '';
    document.getElementById('fldKp').placeholder =
      (j.kpDefault != null) ? String(j.kpDefault) : '';
    document.getElementById('fldKi').placeholder =
      (j.kiDefault != null) ? String(j.kiDefault) : '';
    document.getElementById('fldKd').placeholder =
      (j.kdDefault != null) ? String(j.kdDefault) : '';
    var u = unitFromApi(j);
    document.getElementById('btnUf').classList.toggle('on', u !== 'C');
    document.getElementById('btnUc').classList.toggle('on', u === 'C');
  }

  function selUnit(which) {
    document.getElementById('btnUf').classList.toggle('on', which === 'F');
    document.getElementById('btnUc').classList.toggle('on', which === 'C');
  }

  async function saveThenHome(ev) {
    if (ev) ev.preventDefault();
    var u = document.getElementById('btnUc').classList.contains('on') ? 'C' : 'F';
    var bodyObj = {
      tempUnit: u,
      kp: jsonPidField(document.getElementById('fldKp')),
      ki: jsonPidField(document.getElementById('fldKi')),
      kd: jsonPidField(document.getElementById('fldKd'))
    };
    var r = await fetch('/api/settings/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(bodyObj),
    });
    if (!r.ok) {
      window.alert('Save failed (' + r.status + ').');
      return;
    }
    window.location.href = '/';
  }

  document.getElementById('settBack').addEventListener('click', function (e) {
    saveThenHome(e).catch(console.warn);
  });
  document.getElementById('btnUf').addEventListener('click', function () { selUnit('F'); });
  document.getElementById('btnUc').addEventListener('click', function () { selUnit('C'); });
  pull().catch(console.warn);
})();
  </script>
</body></html>
)--SETS--";

const size_t KILN_WEB_INDEX_HTML_LEN   = sizeof(KILN_WEB_INDEX_HTML) - 1;
const size_t KILN_WEB_STYLE_CSS_LEN    = sizeof(KILN_WEB_STYLE_CSS) - 1;
const size_t KILN_WEB_APP_JS_LEN       = sizeof(KILN_WEB_APP_JS) - 1;
const size_t KILN_WEB_SETTINGS_HTML_LEN = sizeof(KILN_WEB_SETTINGS_HTML) - 1;

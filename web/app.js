"use strict";
// gauge-cam web interface (no framework, served gzipped by the device).

const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => [...document.querySelectorAll(sel)];
let cfg = null;
let status = null;

// ---------- helpers ----------

async function api(path, options = {}) {
  const res = await fetch(path, options);
  const text = await res.text();
  let body;
  try { body = text ? JSON.parse(text) : {}; } catch { body = { error: text }; }
  if (!res.ok) throw new Error(body.error || res.statusText);
  return body;
}

const post = (path, body) => api(path, {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: body === undefined ? "" : JSON.stringify(body),
});

let toastTimer = 0;
function toast(message, isError = false) {
  const t = $("#toast");
  t.textContent = message;
  t.className = isError ? "err" : "";
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.hidden = true; }, isError ? 6000 : 3000);
}

// The narrow no-break space French puts between thousands is missing from
// Gauge Mono, and Chrome would then draw the whole number in a fallback font.
const num = (v, decimals = 1) => (v === null || v === undefined || Number.isNaN(v))
  ? "–" : Number(v).toLocaleString(lang, { maximumFractionDigits: decimals }).replace(/\u202f/g, "\u00a0");

// Large figures: a narrower thousands gap than the monospaced space.
const bigNum = (v, decimals = 1) => num(v, decimals).replace(/\u00a0/g, '<span class="gap"></span>');

const dateTime = (epoch) => new Date(epoch * 1000).toLocaleString(lang,
  { dateStyle: "short", timeStyle: "medium" });

function duration(s) {
  if (s < 0 || s === undefined) return "–";
  if (s < 90) return t("unit.s", { n: Math.round(s) });
  if (s < 5400) return t("unit.min", { n: Math.round(s / 60) });
  if (s < 172800) return t("unit.h", { n: Math.round(s / 3600) });
  return t("unit.d", { n: Math.round(s / 86400) });
}

async function busy(button, task) {
  button.disabled = true;
  try { return await task(); } catch (e) { toast(deviceError(e.message), true); } finally { button.disabled = false; }
}

// Translates the device error messages (kept in English in the firmware).
const ERRORS = [
  [/implausible jump from ([-\d.]+) to ([-\d.]+) (\S*), checking again \((\d+)\/\d+\)/, "err.jump",
    (m) => ({ from: `${num(Number(m[1]))} ${m[3]}`, to: `${num(Number(m[2]))} ${m[3]}`, n: m[4] })],
  [/needle hub not found/, "err.hub"],
  [/needle contrast too low/, "err.contrast"],
  [/not in dial order/, "err.order"],
  [/dial not configured|set at least the start/, "err.notConfigured"],
  [/search band too narrow/, "err.band"],
  [/no photo yet/, "err.noPhoto"],
  [/type the MQTT password again/, "err.mqttPass"],
  [/firmware update in progress/, "err.updating"],
  [/tank table/, "err.table"],
  [/camera/, "err.camera"],
];
function deviceError(message) {
  if (!message) return "";
  for (const [re, key, params] of ERRORS) {
    const m = message.match(re);
    if (m) return t(key, params ? params(m) : { error: message });
  }
  return message;
}

// ---------- overlay drawing ----------

const COLORS = { start: "#2ecc71", end: "#e056fd", point: "#f9ca24", extra: "#4fd1c5", pivot: "#ff3b30",
  roi: "#f9ca24", band: "#3498db", needle: "#2ecc71" };

function at(cx, cy, angle, r) {
  const a = angle * Math.PI / 180;
  return [cx + r * Math.sin(a), cy - r * Math.cos(a)];
}

function drawOverlay(canvas, img, scene) {
  if (!img.naturalWidth) return;
  canvas.width = img.naturalWidth;
  canvas.height = img.naturalHeight;
  const ctx = canvas.getContext("2d");
  const lw = Math.max(1.5, canvas.width / 450);
  ctx.lineWidth = lw;
  ctx.font = `${Math.round(lw * 7)}px system-ui, sans-serif`;
  ctx.textBaseline = "middle";

  if (scene.roi && scene.roi[2] > 0 && scene.roi[3] > 0) {
    ctx.setLineDash([lw * 4, lw * 3]);
    ctx.strokeStyle = COLORS.roi;
    ctx.strokeRect(...scene.roi);
    ctx.setLineDash([]);
    if (scene.handles) {
      const [x, y, w, h] = scene.roi, s = lw * 5;
      ctx.fillStyle = COLORS.roi;
      ctx.strokeStyle = "#000";
      for (const [cx, cy] of [[x, y], [x + w, y], [x, y + h], [x + w, y + h]]) {
        ctx.fillRect(cx - s, cy - s, 2 * s, 2 * s);
        ctx.strokeRect(cx - s, cy - s, 2 * s, 2 * s);
      }
    }
  }
  const d = scene.detail;
  if (d) {
    ctx.strokeStyle = COLORS.band;
    for (const r of [d.r_start, d.r_end]) {
      ctx.beginPath(); ctx.arc(d.cx, d.cy, r, 0, 2 * Math.PI); ctx.stroke();
    }
    if (d.hub_radius) {
      ctx.strokeStyle = COLORS.pivot;
      ctx.beginPath(); ctx.arc(d.cx, d.cy, d.hub_radius, 0, 2 * Math.PI); ctx.stroke();
    }
    for (const [off, color] of [[0, COLORS.start], [d.scale_span, COLORS.end]]) {
      const angle = d.scale_start + d.scale_direction * off;
      ctx.strokeStyle = color;
      ctx.beginPath(); ctx.moveTo(d.cx, d.cy); ctx.lineTo(...at(d.cx, d.cy, angle, d.r_end)); ctx.stroke();
    }
    if (d.needle_angle !== undefined && d.needle_angle !== null) {
      ctx.strokeStyle = COLORS.needle;
      ctx.lineWidth = lw * 2.5;
      ctx.beginPath(); ctx.moveTo(d.cx, d.cy); ctx.lineTo(...at(d.cx, d.cy, d.needle_angle, d.r_end)); ctx.stroke();
      ctx.lineWidth = lw;
    }
  }
  const pivot = d ? [d.cx, d.cy] : scene.pivot;
  if (pivot) {
    const s = lw * 6;
    ctx.strokeStyle = COLORS.pivot;
    ctx.beginPath();
    ctx.moveTo(pivot[0] - s, pivot[1]); ctx.lineTo(pivot[0] + s, pivot[1]);
    ctx.moveTo(pivot[0], pivot[1] - s); ctx.lineTo(pivot[0], pivot[1] + s);
    ctx.stroke();
  }
  for (const p of scene.points || []) {
    const color = COLORS[p.kind] || COLORS.point;
    ctx.fillStyle = color;
    ctx.strokeStyle = "#000";
    ctx.beginPath(); ctx.arc(p.x, p.y, lw * 3, 0, 2 * Math.PI); ctx.fill(); ctx.stroke();
    const label = Number.isFinite(p.v) ? String(p.v) : "?";
    ctx.lineWidth = lw * 2.5;
    ctx.strokeText(label, p.x + lw * 5, p.y);
    ctx.fillText(label, p.x + lw * 5, p.y);
    ctx.lineWidth = lw;
  }
}

function configScene(c) {
  const pts = c.points || [];
  return {
    roi: c.roi,
    pivot: c.pivot_x >= 0 ? [c.pivot_x, c.pivot_y] : null,
    points: pts.map((p, i) => ({ x: p[0], y: p[1], v: p[2],
      kind: i === 0 ? "start" : i === pts.length - 1 ? "end" : "point" })),
  };
}

// ---------- photo orientation ----------

const FRAME = { QVGA: [320, 240], VGA: [640, 480], SVGA: [800, 600] };
const SHOT_KEYS = ["led_intensity", "led_ms", "rotation", "vflip", "hmirror", "framesize",
  "cam_brightness", "cam_contrast", "exposure_auto", "exposure", "gain"];
const ORIENT_KEYS = ["rotation", "vflip", "hmirror", "framesize"];

const pick = (o, keys) => Object.fromEntries(keys.map((k) => [k, o[k]]));
const sameOrient = (a, b) => ORIENT_KEYS.every((k) => a[k] === b[k]);

function photoSize(o) {
  const [w, h] = FRAME[o.framesize] || FRAME.VGA;
  return o.rotation % 180 ? [h, w] : [w, h];
}

// Maps a pixel of a photo taken with orientation `a` to the same spot of a
// photo taken with `b`. The firmware applies the sensor flips, then rotates
// clockwise: (x, y) of a w x h photo goes to (h - 1 - y, x) for 90 degrees.
function mapPoint(x, y, a, b) {
  let [w, h] = photoSize(a);
  for (let r = a.rotation; r > 0; r -= 90) [x, y, w, h] = [y, w - 1 - x, h, w];
  if (a.hmirror) x = w - 1 - x;
  if (a.vflip) y = h - 1 - y;
  const [bw, bh] = FRAME[b.framesize] || FRAME.VGA;
  x = (x + 0.5) * bw / w - 0.5;
  y = (y + 0.5) * bh / h - 0.5;
  [w, h] = [bw, bh];
  if (b.hmirror) x = w - 1 - x;
  if (b.vflip) y = h - 1 - y;
  for (let r = b.rotation; r > 0; r -= 90) [x, y, w, h] = [h - 1 - y, x, h, w];
  return [Math.round(x * 10) / 10, Math.round(y * 10) / 10];
}

function mapRect(r, a, b) {
  const [x0, y0] = mapPoint(r[0], r[1], a, b);
  const [x1, y1] = mapPoint(r[0] + r[2] - 1, r[1] + r[3] - 1, a, b);
  return [Math.round(Math.min(x0, x1)), Math.round(Math.min(y0, y1)),
    Math.round(Math.abs(x1 - x0)) + 1, Math.round(Math.abs(y1 - y0)) + 1];
}

// ---------- tabs ----------

function showTab(name) {
  $$("#tabs button").forEach((x) => x.classList.toggle("on", x.dataset.tab === name));
  $$(".tab").forEach((s) => s.classList.toggle("on", s.id === `tab-${name}`));
  if (name === "dial") dial.enter();
  else if (dial.bubble) dial.closeBubble(false);
  if (name === "settings" && $("#sub-system").classList.contains("on")) loadLog();
  scrollTo(0, 0);
}
$$("#tabs button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));

function showSub(name) {
  $$("#snav button").forEach((x) => x.classList.toggle("on", x.dataset.sub === name));
  $$(".sub").forEach((s) => s.classList.toggle("on", s.id === `sub-${name}`));
  if (name === "system") loadLog();
}
$$("#snav button").forEach((b) => b.addEventListener("click", () => showSub(b.dataset.sub)));

// ---------- reading tab ----------

let photoStamp = 0;
function photoUrl() { return `/api/photo.jpg?t=${photoStamp}`; }

const known = (v) => v !== null && v !== undefined && !Number.isNaN(v);
const clock = (date) => date.toLocaleTimeString(lang, { hour: "2-digit", minute: "2-digit" });

// Instrument arc of the home page: 240 degrees, red reserve at the start,
// filled to `percent` (null: empty track).
function drawGauge(svg, percent) {
  const c = 150, stroke = 18, r = c - stroke, start = -120, sweep = 240;
  const p = (a, radius) => at(c, c, a, radius).map((x) => x.toFixed(1)).join(" ");
  const arc = (a0, a1) => `M${p(a0, r)}A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${p(a1, r)}`;
  const path = (d, cls) => `<path d="${d}" class="${cls}" stroke-width="${stroke}" fill="none" stroke-linecap="round"/>`;
  let ticks = "";
  for (let i = 0; i <= 10; i++) {
    const a = start + sweep * i / 10;
    ticks += `<line x1="${at(c, c, a, r - stroke * 0.9)[0].toFixed(1)}" y1="${at(c, c, a, r - stroke * 0.9)[1].toFixed(1)}"`
      + ` x2="${at(c, c, a, r - stroke * (i % 5 ? 1.3 : 1.6))[0].toFixed(1)}" y2="${at(c, c, a, r - stroke * (i % 5 ? 1.3 : 1.6))[1].toFixed(1)}"/>`;
  }
  const fill = known(percent) ? Math.max(0.5, Math.min(100, percent)) : 0;
  svg.innerHTML = path(arc(start, start + sweep), "g-track")
    + (fill ? path(arc(start, start + sweep * fill / 100), "g-val") : "")
    + path(arc(start, start + sweep * 0.12), "g-red")
    + `<g class="g-ticks">${ticks}</g>`;
}

// Fill level of the gauge: the tank's when it is described, else the
// position of the level on the dial scale.
function scalePercent(level) {
  const v = (cfg?.points || []).map((x) => x[2]);
  if (!known(level) || v.length < 2) return null;
  const lo = Math.min(...v), hi = Math.max(...v);
  return hi > lo ? 100 * (level - lo) / (hi - lo) : null;
}

function setLed(id, state) { $(id).className = `led ${state}`; }

function renderLeds() {
  const s = status;
  setLed("#led-wifi", s ? (s.wifi.connected ? "ok" : s.wifi.ap ? "warn" : "err") : "err");
  setLed("#led-mqtt", s ? (s.mqtt.connected ? "ok" : s.mqtt.configured ? "err" : "") : "");
  const camError = s?.attempt?.error && /camera/.test(s.attempt.error);
  setLed("#led-cam", s ? (camError ? "err" : s.photo ? "ok" : "") : "");
  $("#led-wifi").title = s ? (s.wifi.connected ? `${s.wifi.ssid} (${s.wifi.rssi} dBm)` : t(s.wifi.ap ? "conn.ap" : "s.disconnected")) : t("conn.offline");
  $("#led-mqtt").title = t(!s ? "conn.offline" : s.mqtt.connected ? "conn.ok" : s.mqtt.configured ? "conn.down" : "conn.none");
}

function renderStatus() {
  const s = status;
  $("#title").textContent = s.name;
  document.title = s.name;
  renderLeds();

  const g = s.good || {};
  const tank = cfg ? tankModel(cfg, cfg.tank_table || []) : { ready: false };
  const litres = known(g.volume_l);
  const level = known(g.level) ? `${num(g.level)} ${g.unit}` : "–";
  drawGauge($("#r-gauge"), known(g.percent) ? g.percent : scalePercent(g.level));
  if (litres) {
    $("#r-big").innerHTML = `${bigNum(g.volume_l, 0)}<small> L</small>`;
    $("#r-sub").textContent = tank.capacity > 0 ? t("r.ofCapacity", { cap: num(tank.capacity, 0) }) : "";
  } else if (known(g.level)) {
    $("#r-big").innerHTML = `${bigNum(g.level)}<small> ${String(g.unit).replace(/</g, "&lt;")}</small>`;
    $("#r-sub").textContent = t("r.noTank");
  } else {
    $("#r-big").textContent = "–";
    $("#r-sub").textContent = t(s.dial_ready ? "r.never" : "r.setDial");
  }
  $("#r-percent").textContent = known(g.percent) ? `${num(g.percent)} %` : "–";
  $("#r-second-label").textContent = t(litres ? "r.level" : "r.volume");
  $("#r-second").textContent = litres ? level : "–";
  $("#r-next").textContent = s.next_read_s >= 0 ? clock(new Date(Date.now() + s.next_read_s * 1000)) : "–";

  const a = s.attempt || {};
  const line = $("#r-status");
  const state = $("#r-state");
  if (!s.dial_ready) {
    line.textContent = t("r.notReady");
    line.className = "status warn";
    state.textContent = t("r.state.notReady");
    state.className = "warn";
  } else if (a.ok) {
    line.textContent = "";
    state.textContent = t("r.state.ok");
    state.className = "ok";
  } else if (a.error) {
    line.textContent = deviceError(a.error);
    line.className = a.suspect ? "status warn" : "status err";
    state.textContent = t(a.suspect ? "r.state.suspect" : "r.state.failed");
    state.className = a.suspect ? "warn" : "err";
  } else {
    line.textContent = "";
    state.textContent = "";
  }
  $("#r-last").textContent = g.age_s >= 0
    ? (g.at ? clock(new Date(g.at * 1000)) : t("r.failureAge", { age: duration(g.age_s) })) : "–";
  $("#r-contrast").textContent = known(a.detail?.contrast) ? num(a.detail.contrast, 0) : "–";
  $("#r-refused").textContent = String(failures.list.length);
  $("#r-version").textContent = s.version;

  if (s.last_failure !== failures.id) loadFailures(s.last_failure);
  renderFailures();

  if (s.photo) {
    const stamp = s.photo.taken_ms;
    if (stamp !== photoStamp) {
      photoStamp = stamp;
      $("#r-photo").src = photoUrl();
    } else {
      drawReading();
    }
  }
}

function drawReading() {
  if (!cfg || !status) return;
  const scene = configScene(cfg);
  scene.detail = status.good?.detail;
  drawOverlay($("#r-overlay"), $("#r-photo"), scene);
}
$("#r-photo").addEventListener("load", drawReading);

// Refused readings kept by the device, with their photo.
const failures = { id: undefined, list: [], fetchedAt: 0 };

async function loadFailures(id) {
  failures.id = id;
  try {
    failures.list = id ? await api("/api/failures") : [];
    failures.fetchedAt = Date.now();
  } catch {
    failures.id = undefined;  // try again at the next refresh
    return;
  }
  const box = $("#r-failure-list");
  box.innerHTML = "";
  for (const f of failures.list) {
    const card = document.createElement("div");
    card.className = "card";
    card.innerHTML = `<div class="viewer"><div class="stage"><img alt=""><canvas></canvas></div></div><p></p><small></small>`;
    const img = card.querySelector("img");
    if (f.photo) {
      img.addEventListener("load", () => {
        const scene = cfg ? configScene(cfg) : {};
        scene.detail = f.detail;
        drawOverlay(card.querySelector("canvas"), img, scene);
      });
      img.src = `/api/failure.jpg?id=${f.id}`;
    } else {
      card.querySelector(".viewer").remove();
    }
    box.append(card);
  }
  renderFailures();
}

function renderFailures() {
  $("#r-failures").hidden = failures.list.length === 0;
  const elapsed = (Date.now() - failures.fetchedAt) / 1000;
  $$("#r-failure-list .card").forEach((card, i) => {
    const f = failures.list[i];
    card.querySelector("p").textContent = deviceError(f.error);
    const parts = [f.at ? dateTime(f.at) : t("r.failureAge", { age: duration(f.age_s + elapsed) })];
    if (f.level !== null && f.level !== undefined) parts.push(t("r.failureRead", { value: num(f.level), unit: f.unit }));
    if (!f.photo) parts.push(t("r.noPhoto"));
    card.querySelector("small").textContent = parts.join(" · ");
  });
}

async function refresh() {
  try {
    status = await api("/api/status");
    renderStatus();
    renderSystem();
    renderMenu();
  } catch (e) {
    status = null;
    renderLeds();
  }
}

$("#r-read").addEventListener("click", (e) => busy(e.target, async () => {
  const r = await post("/api/read");
  if (!r.ok) toast(deviceError(r.error), !r.suspect);
  await refresh();
}));

// ---------- dial tab ----------

// Graduations printed on common gauges, in click order. "free": the user
// types every value.
const MODELS = {
  oil: { values: [0, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150], unit: "cm" },
  pct: { values: [0, 25, 50, 75, 100], unit: "%" },
  free: { values: [], unit: null },
};
const STEPS = ["photo", "frame", "grad", "check"];

// First guess of the needle hub: the dark disc that contrasts most with its
// surroundings in the middle of the frame (the dial face is light around
// it, a dark bezel is not). The firmware refines it in hub mode.
function findHub(img, frame) {
  const w = img.naturalWidth, h = img.naturalHeight;
  const canvas = document.createElement("canvas");
  canvas.width = w;
  canvas.height = h;
  const g = canvas.getContext("2d", { willReadFrequently: true });
  g.drawImage(img, 0, 0);
  const px = g.getImageData(0, 0, w, h).data;
  const W = w + 1, sum = new Float64Array(W * (h + 1));
  for (let y = 0; y < h; y++) {
    let row = 0;
    for (let x = 0; x < w; x++) {
      const i = 4 * (y * w + x);
      row += 0.299 * px[i] + 0.587 * px[i + 1] + 0.114 * px[i + 2];
      sum[(y + 1) * W + x + 1] = sum[y * W + x + 1] + row;
    }
  }
  // Mean brightness of the square of half-side r around (x, y), clipped to the photo.
  const mean = (x, y, r) => {
    const x0 = Math.max(0, x - r), x1 = Math.min(w, x + r + 1), y0 = Math.max(0, y - r), y1 = Math.min(h, y + r + 1);
    return (sum[y1 * W + x1] - sum[y0 * W + x1] - sum[y1 * W + x0] + sum[y0 * W + x0]) / ((x1 - x0) * (y1 - y0));
  };
  const [fx, fy, fw, fh] = frame;
  const k = Math.max(2, Math.round(Math.min(fw, fh) * 0.035));
  const x0 = Math.max(0, Math.round(fx + fw * 0.2)), x1 = Math.min(w - 1, Math.round(fx + fw * 0.8));
  const y0 = Math.max(0, Math.round(fy + fh * 0.2)), y1 = Math.min(h - 1, Math.round(fy + fh * 0.8));
  let best = -Infinity, at = null;
  for (let y = y0; y <= y1; y += 2) {
    for (let x = x0; x <= x1; x += 2) {
      const score = mean(x, y, 3 * k) - mean(x, y, k);
      if (score > best) { best = score; at = [x, y]; }
    }
  }
  return at;
}

const dial = {
  step: "photo",
  draft: null,
  detail: null,     // last test reading on the draft, null when stale
  testError: "",
  testLevel: null,
  drag: null,
  bubble: null,     // { p, isNew } while the value bubble is open
  loaded: false,
  dirty: false,
  orient: null,     // capture settings of the displayed photo and of the draft coordinates
  capturing: false,
  again: false,

  img: () => $("#d-photo-img"),
  canvas: () => $("#d-overlay"),

  enter() {
    if (!this.loaded && cfg) this.reset();
    if (!this.img().src && status?.photo) this.loadPhoto();
  },

  reset() {
    const saved = (cfg.points || []).map((a) => ({ x: a[0], y: a[1], v: a[2] }));
    const values = saved.map((p) => p.v);
    // The model whose printed values the saved points start with.
    let model = saved.length ? "free" : "oil", count = 0;
    for (const name of ["oil", "pct"]) {
      const list = MODELS[name].values;
      let n = 0;
      while (n < list.length && values.includes(list[n])) n++;
      if ((n >= 3 || n === list.length) && n > count) { model = name; count = n; }
    }
    const placed = new Set(MODELS[model].values.slice(0, count));
    const pts = saved.map((p) => {
      const seq = placed.has(p.v);
      placed.delete(p.v);
      return { ...p, extra: !seq };
    });
    // Template points first, in click order, so that undo removes the extras first.
    const order = MODELS[model].values;
    pts.sort((a, b) => (a.extra - b.extra) || (a.extra ? 0 : order.indexOf(a.v) - order.indexOf(b.v)));
    this.draft = {
      roi: cfg.roi && cfg.roi[2] > 0 ? [...cfg.roi] : null,
      pivot: cfg.pivot_x >= 0 ? [cfg.pivot_x, cfg.pivot_y] : null,
      pivotAuto: false,
      model,
      pts,
      history: [...pts],
    };
    $("#d-model").value = model;
    $("#d-mode").value = cfg.pivot_mode;
    $("#d-unit").value = cfg.unit;
    this.orient = pick(cfg, SHOT_KEYS);
    fill($("#d-shot"), this.orient);
    showLed();
    this.loaded = true;
    this.dirty = false;
    this.detail = null;
    this.testError = "";
    this.closeBubble(false);
    const d = this.draft;
    this.show(!status?.photo ? "photo" : !d.roi && !d.pivot ? "frame" : saved.length < 2 ? "grad" : "check", false);
  },

  // ----- state -----

  // The frame on screen. Without a saved one the whole photo is read: the
  // default frame stays inside the photo so that its corners can be grabbed.
  frame() {
    const img = this.img(), w = img.naturalWidth, h = img.naturalHeight, m = Math.round(Math.min(w, h) * 0.05);
    return this.draft.roi || [m, m, w - 2 * m, h - 2 * m];
  },

  template() { return MODELS[this.draft.model].values; },
  seqCount() { return this.draft.pts.filter((p) => !p.extra).length; },
  nextValue() { return this.template()[this.seqCount()]; },

  isDone(step) {
    const d = this.draft;
    if (!d) return false;
    switch (step) {
      case "photo": return Boolean(this.img().naturalWidth);
      case "frame": return Boolean(d.pivot);
      case "grad": return d.pts.length >= 2 && this.nextValue() === undefined;
      default: return !this.dirty && Boolean(status?.dial_ready);
    }
  },

  changed() {
    this.dirty = true;
    this.detail = null;
    this.testError = "";
    this.testLevel = null;
    this.render();
  },

  // ----- steps -----

  show(step, run = true) {
    this.closeBubble(false);
    this.step = step;
    this.render();
    if (step === "check" && run && !this.detail && !this.testError) {
      try { this.payload(); } catch { return; }
      busy($("#d-test"), () => this.test());
    }
  },

  render() {
    if (!this.draft) return;
    const d = this.draft;
    $$("#d-steps button").forEach((b) => {
      b.classList.toggle("on", b.dataset.step === this.step);
      b.classList.toggle("done", this.isDone(b.dataset.step));
    });
    $$(".dstep").forEach((s) => s.classList.toggle("on", s.dataset.step === this.step));
    $("#d-viewer").dataset.step = this.step;
    $("#d-dirty").hidden = !this.dirty;

    // framing
    const line = $("#d-pivot-line");
    line.textContent = t(!d.pivot ? "d.pivot.none" : d.pivotAuto ? "d.pivot.found" : "d.pivot.manual");
    line.classList.toggle("off", !d.pivot);

    // graduations
    const free = d.model === "free";
    $("#d-unit-box").hidden = !free;
    const next = this.nextValue();
    const title = $("#d-seq-title");
    if (free) {
      title.textContent = t("d.seq.free");
      $("#d-seq-help").textContent = t("d.seq.freeHelp");
    } else if (next !== undefined) {
      title.innerHTML = `${t("d.seq.next")} <span class="big-n">${num(next)}</span>`;
      $("#d-seq-help").textContent = t("d.seq.nextHelp");
    } else {
      title.textContent = t("d.seq.done", { n: this.template().length });
      $("#d-seq-help").textContent = t("d.seq.doneHelp");
    }
    const chips = $("#d-chips");
    chips.innerHTML = "";
    for (const p of [...d.pts].sort((a, b) => a.v - b.v)) {
      const c = document.createElement("span");
      c.className = p.extra ? "chip extra" : "chip";
      c.textContent = Number.isFinite(p.v) ? num(p.v) : "?";
      chips.append(c);
    }
    $("#d-undo").disabled = !d.history.length;
    $("#d-to-check").disabled = d.pts.length < 2;

    this.renderCheck();
    this.layout();
    this.draw();
  },

  renderCheck() {
    const title = $("#d-check-title"), help = $("#d-check-help"), kv = $("#d-check-kv");
    const unit = this.unit();
    kv.innerHTML = "";
    const ok = Boolean(this.detail) && !this.testError;
    if (ok) {
      title.innerHTML = t("d.check.reads", { value: `<span class="amb">${num(this.testLevel)}&nbsp;${unit}</span>` });
      help.textContent = t("d.check.look");
      for (const [k, v] of [["d.check.contrast", num(this.detail.contrast, 0)], ["d.check.angle", `${num(this.detail.needle_angle)}°`]]) {
        kv.insertAdjacentHTML("beforeend", "<dt></dt><dd></dd>");
        kv.lastElementChild.previousElementSibling.textContent = t(k);
        kv.lastElementChild.textContent = v;
      }
    } else if (this.testError) {
      title.textContent = t("d.check.failed");
      help.textContent = this.testError;
    } else {
      title.textContent = t("d.check.title");
      help.textContent = t("d.check.help");
    }
    const tested = ok || Boolean(this.testError);
    $("#d-check-no").textContent = t(ok ? "d.check.no" : "d.check.fix");
    $("#d-test").textContent = t(tested ? "d.check.again" : "d.check.read");
    $("#d-save").textContent = t(ok ? "d.check.yes" : "d.check.save");
    $("#d-save").classList.toggle("pri", ok || !tested);
    $("#d-save").hidden = !this.dirty;  // nothing to save: the device already has it
    $("#d-test").classList.toggle("pri", false);
  },

  unit() {
    return MODELS[this.draft.model].unit || $("#d-unit").value.trim() || "cm";
  },

  // ----- photo -----

  loadPhoto() { this.img().src = `/api/photo.jpg?t=${Date.now()}`; },

  photoLoaded() {
    const d = this.draft;
    if (d && !d.pivot) this.autoPivot();
    this.render();
  },

  readShot() {
    const v = read($("#d-shot"));
    v.rotation = Number(v.rotation);
    return v;
  },

  // Moves the frame, centre and points to a photo taken with `to`.
  reorient(to) {
    const from = this.orient;
    this.orient = to;
    if (sameOrient(from, to)) return;
    const d = this.draft;
    if (d.roi) d.roi = mapRect(d.roi, from, to);
    if (d.pivot) d.pivot = mapPoint(d.pivot[0], d.pivot[1], from, to);
    for (const p of d.pts) [p.x, p.y] = mapPoint(p.x, p.y, from, to);
  },

  // New photo with the capture settings of the panel, saved or not.
  async takePhoto() {
    if (!this.draft) return;
    if (this.capturing) { this.again = true; return; }
    this.capturing = true;
    $("#d-shot").classList.add("busy");
    $("#d-photo").disabled = true;
    try {
      do {
        this.again = false;
        const shot = this.readShot();
        await post("/api/capture", shot);
        this.reorient(shot);
        this.changed();
        this.loadPhoto();
      } while (this.again);
    } catch (e) {
      // The photo on screen keeps its orientation: so do the fields.
      fill($("#d-shot"), pick(this.orient, ORIENT_KEYS));
      toast(t("d.shotFailed", { error: e.message }), true);
    } finally {
      this.capturing = false;
      $("#d-shot").classList.remove("busy");
      $("#d-photo").disabled = false;
    }
    refresh();
  },

  autoPivot() {
    const img = this.img();
    if (!img.naturalWidth) return;
    const hub = findHub(img, this.frame());
    if (!hub) return;
    this.draft.pivot = hub;
    this.draft.pivotAuto = true;
  },

  // ----- drawing -----

  scene() {
    const d = this.draft;
    const byValue = d.pts.filter((p) => Number.isFinite(p.v)).sort((a, b) => a.v - b.v);
    const lo = byValue[0], hi = byValue[byValue.length - 1];
    const points = d.pts.map((p) => ({ ...p,
      kind: p === lo ? "start" : p === hi && byValue.length > 1 && this.nextValue() === undefined ? "end"
        : p.extra ? "extra" : "point" }));
    return { roi: this.img().naturalWidth ? this.frame() : null, handles: this.step === "frame",
      pivot: d.pivot, points, detail: this.detail };
  },

  draw() { drawOverlay(this.canvas(), this.img(), this.scene()); },

  // Graduations and check show the inside of the frame, enlarged to the
  // width of the column: the graduations are far enough apart for a finger.
  layout() {
    const img = this.img(), viewer = $("#d-viewer"), stage = viewer.firstElementChild, c = this.canvas();
    const crop = img.naturalWidth && (this.step === "grad" || this.step === "check");
    viewer.classList.toggle("crop", Boolean(crop));
    if (!crop) {
      for (const el of [viewer, stage, img, c]) el.removeAttribute("style");
      return;
    }
    const [fx, fy, fw, fh] = this.frame();
    const avail = viewer.parentElement.clientWidth;
    const s = Math.min(avail / fw, innerHeight * 0.75 / fh, 4);
    const px = (v) => `${Math.round(v * s)}px`;
    Object.assign(viewer.style, { width: px(fw), height: px(fh) });
    Object.assign(stage.style, { left: px(-fx), top: px(-fy) });
    img.style.width = px(img.naturalWidth);
    Object.assign(c.style, { width: px(img.naturalWidth), height: px(img.naturalHeight) });
  },

  // ----- pointer -----

  imagePoint(e) {
    const c = this.canvas();
    const r = c.getBoundingClientRect();
    return [Math.round((e.clientX - r.left) * c.width / r.width),
      Math.round((e.clientY - r.top) * c.height / r.height)];
  },

  // Grab distance in photo pixels: about 14 screen pixels.
  reach() {
    const c = this.canvas();
    return 14 * c.width / c.getBoundingClientRect().width;
  },

  onDown(e) {
    if (!this.img().naturalWidth || !this.draft) return;
    if (this.bubble) { this.closeBubble(false); return; }
    e.preventDefault();
    const [x, y] = this.imagePoint(e);
    const near = (px, py) => Math.hypot(px - x, py - y) <= this.reach();
    const d = this.draft;
    let drag = null;
    if (this.step === "frame") {
      const [fx, fy, fw, fh] = this.frame();
      const corners = [[fx, fy], [fx + fw, fy], [fx, fy + fh], [fx + fw, fy + fh]];
      const i = corners.findIndex(([cx, cy]) => near(cx, cy));
      if (i >= 0) drag = { kind: "corner", anchor: corners[3 - i] };
      else if (x >= fx && x <= fx + fw && y >= fy && y <= fy + fh) drag = { kind: "inside", from: [x, y], roi: [fx, fy, fw, fh] };
      else drag = { kind: "corner", anchor: [x, y], fresh: true };
    } else if (this.step === "grad") {
      const p = [...d.pts].reverse().find((q) => near(q.x, q.y));
      drag = p ? { kind: "point", p, from: [x, y] } : { kind: "add" };
    } else {
      return;
    }
    drag.start = [x, y];
    drag.moved = false;
    this.drag = drag;
    this.canvas().setPointerCapture(e.pointerId);
  },

  onMove(e) {
    const g = this.drag;
    if (!g) return;
    const [x, y] = this.imagePoint(e);
    if (!g.moved && Math.hypot(x - g.start[0], y - g.start[1]) < this.reach() / 3) return;
    g.moved = true;
    const d = this.draft;
    const img = this.img();
    const clampX = (v) => Math.max(0, Math.min(img.naturalWidth, v));
    const clampY = (v) => Math.max(0, Math.min(img.naturalHeight, v));
    if (g.kind === "corner") {
      const [ax, ay] = g.anchor, bx = clampX(x), by = clampY(y);
      d.roi = [Math.min(ax, bx), Math.min(ay, by), Math.abs(bx - ax), Math.abs(by - ay)];
    } else if (g.kind === "inside") {
      const [rx, ry, rw, rh] = g.roi;
      d.roi = [Math.max(0, Math.min(img.naturalWidth - rw, rx + x - g.from[0])),
        Math.max(0, Math.min(img.naturalHeight - rh, ry + y - g.from[1])), rw, rh];
    } else if (g.kind === "point") {
      g.p.x = clampX(x);
      g.p.y = clampY(y);
    }
    this.draw();
  },

  onUp() {
    const g = this.drag;
    if (!g) return;
    this.drag = null;
    const d = this.draft;
    const [x, y] = g.start;
    if (g.kind === "corner" || g.kind === "inside") {
      if (!g.moved && g.kind === "inside") {
        d.pivot = [x, y];          // a click in the frame places the centre
        d.pivotAuto = false;
      } else if (g.moved) {
        if (d.roi[2] < 20 || d.roi[3] < 20) d.roi = g.fresh ? null : g.roi || null;
        const [fx, fy, fw, fh] = this.frame();
        const inside = d.pivot && d.pivot[0] >= fx && d.pivot[0] <= fx + fw && d.pivot[1] >= fy && d.pivot[1] <= fy + fh;
        if (!d.pivot || d.pivotAuto || !inside) this.autoPivot();
      } else {
        return;
      }
      this.changed();
    } else if (g.kind === "point") {
      if (g.moved) this.changed();
      else this.openBubble(g.p, false);
    } else if (g.kind === "add") {
      const next = this.nextValue();
      const p = { x, y, v: next === undefined ? NaN : next, extra: next === undefined };
      d.pts.push(p);
      d.history.push(p);
      this.changed();
      if (p.extra) this.openBubble(p, true);
    }
  },

  // ----- value bubble -----

  openBubble(p, isNew) {
    const c = this.canvas();
    const box = $("#d-bubble");
    this.bubble = { p, isNew };
    box.style.left = `${100 * p.x / c.width}%`;
    box.style.top = `${100 * p.y / c.height}%`;
    // Towards the needle centre: the dial face is free, the scale is not.
    const [px, py] = this.draft.pivot || [c.width / 2, c.height / 2];
    box.classList.toggle("below", p.y < py);
    box.classList.toggle("left", p.x > px);
    const input = $("#d-bubble-value");
    input.value = Number.isFinite(p.v) ? String(p.v).replace(".", lang === "fr" ? "," : ".") : "";
    $("#d-bubble-del").hidden = isNew || !p.extra;
    box.hidden = false;
    input.focus();
    input.select();
  },

  closeBubble(keep) {
    const b = this.bubble;
    $("#d-bubble").hidden = true;
    this.bubble = null;
    if (b && !keep && b.isNew) this.remove(b.p);
  },

  remove(p) {
    const d = this.draft;
    d.pts = d.pts.filter((q) => q !== p);
    d.history = d.history.filter((q) => q !== p);
    this.changed();
  },

  submitBubble() {
    const b = this.bubble;
    if (!b) return;
    const v = Number($("#d-bubble-value").value.trim().replace(",", "."));
    if ($("#d-bubble-value").value.trim() === "" || !Number.isFinite(v)) { $("#d-bubble-value").focus(); return; }
    b.p.v = v;
    this.closeBubble(true);
    this.changed();
  },

  undo() {
    const p = this.draft.history.pop();
    if (p) this.remove(p);
  },

  setModel(model) {
    const d = this.draft;
    if (model === d.model) return;
    if (d.pts.length && !confirm(t("d.model.confirm"))) { $("#d-model").value = d.model; return; }
    d.model = model;
    d.pts = [];
    d.history = [];
    this.changed();
  },

  // ----- test and save -----

  // Builds the configuration to test or save, or throws a readable error.
  payload() {
    const d = this.draft;
    if (!d.pivot) throw new Error(t("d.err.pivot"));
    if (d.pts.length < 2) throw new Error(t("d.err.ends"));
    if (d.pts.some((p) => !Number.isFinite(p.v))) throw new Error(t("d.err.values"));
    const sorted = [...d.pts].sort((a, b) => a.v - b.v);
    for (let i = 1; i < sorted.length; i++) {
      if (sorted[i].v === sorted[i - 1].v) throw new Error(t("d.err.same", { v: num(sorted[i].v) }));
    }
    const roi = d.roi ? d.roi.map(Math.round) : [0, 0, 0, 0];
    return {
      ...this.readShot(),
      ...pick(this.orient, ORIENT_KEYS),
      roi,
      pivot_mode: $("#d-mode").value,
      pivot_x: d.pivot[0],
      pivot_y: d.pivot[1],
      points: sorted.map((p) => [p.x, p.y, p.v]),
      unit: this.unit(),
    };
  },

  async test() {
    let body;
    try { body = this.payload(); } catch (e) { this.testError = e.message; this.renderCheck(); return; }
    const r = await post("/api/test", body);
    this.detail = r.detail || null;
    this.testLevel = r.level;
    this.testError = r.ok ? "" : deviceError(r.error) || t("d.check.failed");
    this.render();
  },

  async save() {
    this.reorient(this.readShot());
    const body = this.payload();
    await post("/api/config", body);
    cfg = await api("/api/config");
    this.dirty = false;
    toast(t("d.saved"));
    await refresh();
    this.render();
  },
};

$("#d-photo-img").addEventListener("load", () => dial.photoLoaded());
const overlay = $("#d-overlay");
overlay.addEventListener("pointerdown", (e) => dial.onDown(e));
overlay.addEventListener("pointermove", (e) => dial.onMove(e));
overlay.addEventListener("pointerup", () => dial.onUp());
overlay.addEventListener("pointercancel", () => { dial.drag = null; dial.draw(); });
addEventListener("resize", () => { if (dial.draft) dial.layout(); });
$$("#d-steps button").forEach((b) => b.addEventListener("click", () => dial.show(b.dataset.step)));
$$("#tab-dial [data-go]").forEach((b) => b.addEventListener("click", () => dial.show(b.dataset.go)));
$("#d-bubble").addEventListener("submit", (e) => { e.preventDefault(); dial.submitBubble(); });
$("#d-bubble").addEventListener("keydown", (e) => { if (e.key === "Escape") { dial.closeBubble(false); dial.render(); } });
$("#d-bubble-del").addEventListener("click", () => { const b = dial.bubble; dial.closeBubble(true); if (b) dial.remove(b.p); });
$("#d-undo").addEventListener("click", () => dial.undo());
$("#d-model").addEventListener("change", (e) => dial.setModel(e.target.value));
$("#d-unit").addEventListener("input", () => dial.changed());
$("#d-photo").addEventListener("click", () => dial.takePhoto());
$("#d-shot").addEventListener("change", () => dial.takePhoto());
$("#d-shot").addEventListener("input", showLed);
$("#d-shot").addEventListener("submit", (e) => e.preventDefault());
function showLed() {
  const form = $("#d-shot");
  const v = Number(form.led_intensity.value);
  $("#d-led-value").textContent = v ? `${Math.round(v / 2.55)} %` : t("d.off");
  $("#d-exposure-value").textContent = form.exposure.value;
  $("#d-gain-value").textContent = form.gain.value;
  $$("#d-shot .manual-exposure").forEach((el) => el.classList.toggle("off", form.exposure_auto.checked));
}
$("#d-test").addEventListener("click", (e) => busy(e.target, () => dial.test()));
$("#d-save").addEventListener("click", (e) => busy(e.target, () => dial.save()));
$("#d-mode").addEventListener("change", () => dial.changed());

// ---------- forms (tank, camera, network) ----------

function fill(form, values) {
  for (const el of form.elements) {
    if (!el.name || !(el.name in values)) continue;
    if (el.type === "checkbox") el.checked = Boolean(values[el.name]);
    else el.value = values[el.name];
  }
}

function read(form) {
  const out = {};
  for (const el of form.elements) {
    if (!el.name) continue;
    if (el.type === "checkbox") out[el.name] = el.checked;
    else if (el.type === "number" || el.type === "range") out[el.name] = el.value === "" ? 0 : Number(el.value);
    else out[el.name] = el.value;
  }
  return out;
}

function renderForms() {
  fill($("#f-tank"), { ...cfg, tank_table_text: (cfg.tank_table || []).map((r) => r.join(" ")).join("\n") });
  fill($("#f-camera"), { ...cfg, interval_min: Math.round(cfg.interval_s / 60) });
  const net = $("#f-network");
  fill(net, { ...cfg, wifi_pass: "", mqtt_pass: "", ui_password: "", ui_password_clear: false,
    ap_password: "", ap_password_clear: false });
  net.wifi_pass.placeholder = cfg.wifi_pass_set ? t("n.unchanged") : "";
  net.mqtt_pass.placeholder = cfg.mqtt_pass_set ? t("n.unchanged") : "";
  net.ui_password.placeholder = cfg.ui_password_set ? t("n.unchanged") : t("n.nonePw");
  net.ap_password.placeholder = cfg.ap_password_set ? t("n.unchanged") : t("n.nonePw");
  renderCapacity();
  renderMenu();
}

// Level -> litres rows typed one per line: "40 520", "40;520", "40,5 520".
function parseTable(text) {
  const rows = [];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line || line.startsWith("#")) continue;
    let cells = line.split(/[\s;]+/);
    if (cells.length === 1) cells = line.split(",");
    const row = cells.map((c) => Number(c.replace(",", ".")));
    const prev = rows[rows.length - 1];
    if (row.length !== 2 || row.some((x) => !Number.isFinite(x)) || (prev && (row[0] <= prev[0] || row[1] < prev[1]))) {
      return { error: i + 1 };
    }
    rows.push(row);
  }
  return { rows };
}

// Same formulas as gauge::Tank in the firmware.
function tankModel(v, table) {
  const L = v.tank_length_cm, W = v.tank_width_cm, H = v.tank_height_cm, D = v.tank_diameter_cm;
  const shape = v.tank_shape;
  const ready = shape === "box" ? L > 0 && W > 0 : shape === "hcyl" ? L > 0 && D > 0
    : shape === "vcyl" ? D > 0 : table.length >= 2;
  const full = shape === "hcyl" ? D : shape === "table" ? (table.length ? table[table.length - 1][0] : 0) : H;
  const volume = (level) => {
    if (full > 0) level = Math.min(level, full);
    level = Math.max(level, shape === "table" ? table[0][0] : 0);
    if (shape === "box") return level * L * W / 1000;
    if (shape === "vcyl") return level * Math.PI * D * D / 4 / 1000;
    if (shape === "hcyl") {
      const r = D / 2;
      return (r * r * Math.acos((r - level) / r) - (r - level) * Math.sqrt(Math.max(0, 2 * r * level - level * level))) * L / 1000;
    }
    for (let i = 1; i < table.length; i++) {
      const [a, b] = [table[i - 1], table[i]];
      if (level <= b[0]) return a[1] + (b[1] - a[1]) * (level - a[0]) / (b[0] - a[0]);
    }
    return table[table.length - 1][1];
  };
  return { ready, full, volume, capacity: ready && full > 0 ? volume(full) : 0 };
}

function renderCapacity() {
  const form = $("#f-tank");
  const v = read(form);
  $$("#t-shapes button").forEach((b) => b.classList.toggle("on", b.dataset.shape === v.tank_shape));
  form.querySelectorAll("[data-shapes]").forEach((el) => {
    el.hidden = !el.dataset.shapes.split(" ").includes(v.tank_shape);
  });
  $("#t-shape-help").textContent = t(`t.shape.${v.tank_shape}.help`);
  const parsed = parseTable(v.tank_table_text);
  const line = $("#t-capacity");
  const value = $("#t-capacity-value");
  value.textContent = "–";
  if (v.tank_shape === "table" && parsed.error) {
    line.textContent = t("t.table.invalid", { n: parsed.error });
    return;
  }
  const tank = tankModel(v, parsed.rows || []);
  if (!tank.ready) {
    line.textContent = t("t.incomplete");
  } else if (tank.capacity > 0) {
    value.innerHTML = `${bigNum(tank.capacity, 0)}<small> L</small>`;
    line.textContent = v.tank_shape === "box" ? t("t.perCm", { perCm: num(tank.volume(1), 2) })
      : t("t.half", { level: num(tank.full / 2), unit: cfg?.unit || "cm", v: num(tank.volume(tank.full / 2), 0) });
  } else {
    line.textContent = t("t.perCm", { perCm: num(tank.volume(1), 2) });
  }
}
$("#f-tank").addEventListener("input", renderCapacity);
$$("#t-shapes button").forEach((b) => b.addEventListener("click", () => {
  $("#f-tank").tank_shape.value = b.dataset.shape;
  renderCapacity();
}));

// One-line summaries in the settings menu.
function renderMenu() {
  if (!cfg) return;
  const tank = tankModel(cfg, cfg.tank_table || []);
  $("#sn-tank").textContent = tank.ready ? (tank.capacity > 0 ? `${num(tank.capacity, 0)} L` : t(`t.shape.${cfg.tank_shape}`)) : t("sn.todo");
  $("#sn-camera").textContent = t("unit.min", { n: Math.round(cfg.interval_s / 60) });
  $("#sn-network").textContent = status ? t(status.mqtt.connected ? "sn.connected" : status.mqtt.configured ? "sn.down" : "sn.todo") : "";
  $("#sn-system").textContent = status ? status.version : "";
}

async function saveForm(e, transform = (v) => v) {
  e.preventDefault();
  const button = e.target.querySelector(".btn.pri");
  await busy(button, async () => {
    const r = await post("/api/config", transform(read(e.target)));
    cfg = await api("/api/config");
    renderForms();
    toast(t(r.restart ? "savedRestart" : "saved"));
    refresh();  // name, LEDs and home figures follow the new settings
  });
}

$("#f-tank").addEventListener("submit", (e) => saveForm(e, (v) => {
  const parsed = parseTable(v.tank_table_text);
  delete v.tank_table_text;
  if (parsed.error && v.tank_shape === "table") throw new Error(t("t.table.invalid", { n: parsed.error }));
  if (parsed.rows) v.tank_table = parsed.rows;
  return v;
}));
$("#f-camera").addEventListener("submit", (e) => saveForm(e, (v) => {
  v.interval_s = Math.max(1, v.interval_min) * 60;
  delete v.interval_min;
  return v;
}));
$("#f-network").addEventListener("submit", (e) => saveForm(e, (v) => {
  for (const key of ["wifi_pass", "mqtt_pass", "ui_password", "ap_password"]) if (!v[key]) delete v[key];
  return v;
}));
$("#n-scan").addEventListener("click", (e) => busy(e.target, async () => {
  const list = await api("/api/wifi/scan");
  const dl = $("#ssids");
  dl.innerHTML = "";
  for (const n of list.sort((a, b) => b.rssi - a.rssi)) {
    const o = document.createElement("option");
    o.value = n.ssid;
    o.label = `${n.rssi} dBm${n.open ? `, ${t("n.open")}` : ""}`;
    dl.append(o);
  }
  toast(t("n.found", { n: list.length }));
}));

// ---------- system tab ----------

// Notes the firmware leaves before restarting itself (kept in English).
const NOTES = [
  [/^the camera does not start/, "note.camera"],
  [/^the camera stopped delivering/, "note.frames"],
  [/^no Wi-Fi for 1 h/, "note.wifi"],
  [/^MQTT server unreachable/, "note.mqtt"],
  [/^the new firmware cannot join Wi-Fi/, "note.rollback"],
  [/^asked from the interface/, "note.asked"],
  [/^update to (\S+) installed/, "note.updated"],
];
function restartNote(note) {
  for (const [re, key] of NOTES) {
    const m = note.match(re);
    if (m) return t(key, { v: m[1] });
  }
  return note;
}

function renderSystem() {
  const s = status;
  const kb = t("s.kb");
  const reason = t(`reset.${s.reset_reason}`);
  const rows = [
    [t("s.version"), s.version], [t("s.id"), s.id], [t("s.uptime"), duration(s.uptime_s)],
    [t("s.reset"), s.restart_note ? t("s.resetWithNote", { reason, note: restartNote(s.restart_note) }) : reason],
    [t("s.clock"), s.time ? dateTime(s.time) : t("s.noClock")],
    [t("s.wifi"), s.wifi.connected ? `${s.wifi.ssid} (${s.wifi.rssi} dBm), ${s.wifi.ip}` : t("s.disconnected")],
    [t("s.ap"), s.wifi.ap ? t("s.apOn", { name: s.wifi.hostname, ip: s.wifi.ap_ip }) : t("s.closed")],
    [t("s.mqtt"), t(s.mqtt.connected ? "s.connected" : s.mqtt.configured ? "s.disconnected" : "s.notSet")],
    [t("s.photo"), s.photo ? `${s.photo.width} × ${s.photo.height}, ${num(s.photo.bytes / 1024, 0)} ${kb}` : t("s.none")],
    [t("s.memory"), `${num(s.heap_free / 1024, 0)} ${kb} (${t("s.min")} ${num(s.heap_min / 1024, 0)} ${kb}) + ${num(s.psram_free / 1024, 0)} ${kb} PSRAM`],
  ];
  $("#s-info").innerHTML = rows.map(([k, v]) => `<dt>${k}</dt><dd>${String(v).replace(/</g, "&lt;")}</dd>`).join("");
  renderUpdate();
}

let installing = null;  // version being installed: reload once it runs

function renderUpdate() {
  const u = status.update;
  if (!u) return;
  if (installing && status.version !== installing.from) {
    installing = null;
    toast(t("s.updated"));
    setTimeout(() => location.reload(), 1500);
  }
  const line = $("#s-release");
  const age = duration(u.checked_s);
  let text;
  let kind = "";
  if (u.busy && u.progress !== undefined) text = t("s.installing", { v: u.latest, p: u.progress });
  else if (u.busy) text = t("s.checking");
  else if (u.install_error) { text = t("s.installFailed", { error: u.install_error }); kind = "err"; }
  else if (u.error) { text = t("s.checkFailed", { error: u.error }); kind = "err"; }
  else if (u.checked_s < 0) text = t("s.neverChecked");
  else if (u.available) { text = t("s.available", { v: u.latest, age }); kind = "warn"; }
  else if (!u.latest) text = t("s.noRelease", { age });
  else text = t("s.upToDate", { v: u.latest, age });
  if (u.pending_verify) text += ` ${t("s.pendingVerify")}`;
  line.textContent = text;
  line.className = `status ${kind}`;
  const install = $("#s-install");
  install.hidden = !u.available || u.busy;
  install.textContent = t("s.install", { v: u.latest });
  const bar = $("#s-ota-progress");
  bar.hidden = !(u.busy && u.progress !== undefined);
  bar.value = u.progress || 0;
}

$("#s-check").addEventListener("click", (e) => busy(e.target, async () => {
  await post("/api/update/check");
  for (const wait of [3000, 4000, 6000]) {
    await new Promise((r) => setTimeout(r, wait));
    await refresh();
    if (status.update && !status.update.busy) break;
  }
}));

$("#s-install").addEventListener("click", (e) => busy(e.target, async () => {
  const v = status.update.latest;
  if (!confirm(t("s.installConfirm", { v }))) return;
  await post("/api/update/install");
  installing = { from: status.version };
  toast(t("s.installStarted", { v }));
  await refresh();
}));

function logStamp(line) {
  if (line.at) return dateTime(line.at);
  const s = line.t;
  const pad = (n) => String(n).padStart(2, "0");
  return `+${Math.floor(s / 3600)}:${pad(Math.floor(s / 60) % 60)}:${pad(s % 60)}`;
}

async function loadLog() {
  try {
    const lines = await api("/api/log");
    const pre = $("#s-log");
    pre.textContent = lines.map((l) => `${logStamp(l)}  ${l.msg}`).join("\n") || t("s.logEmpty");
    pre.scrollTop = pre.scrollHeight;
  } catch (e) {
    toast(e.message, true);
  }
}
$("#s-log-refresh").addEventListener("click", (e) => busy(e.target, loadLog));

$("#s-update").addEventListener("click", (e) => {
  const file = $("#s-file").files[0];
  if (!file) return toast(t("s.chooseFile"), true);
  const button = e.target;
  const bar = $("#s-progress");
  const xhr = new XMLHttpRequest();
  const form = new FormData();
  form.append("firmware", file);
  button.disabled = true;
  bar.hidden = false;
  xhr.upload.onprogress = (ev) => { if (ev.lengthComputable) bar.value = 100 * ev.loaded / ev.total; };
  xhr.onload = () => {
    button.disabled = false;
    if (xhr.status === 200) {
      toast(t("s.updated"));
      setTimeout(() => location.reload(), 15000);
    } else {
      toast(t("s.updateFailed", { error: xhr.responseText }), true);
    }
  };
  xhr.onerror = () => { button.disabled = false; toast(t("s.sendFailed"), true); };
  xhr.open("POST", "/api/update");
  xhr.send(form);
});

$("#s-export").addEventListener("click", async () => {
  const data = await api("/api/config");
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `${status?.id || "gauge-cam"}.json`;
  a.click();
  URL.revokeObjectURL(a.href);
});

$("#s-import").addEventListener("change", async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  try {
    const data = JSON.parse(await file.text());
    for (const k of Object.keys(data)) if (k.endsWith("_set")) delete data[k];
    const r = await post("/api/config", data);
    cfg = await api("/api/config");
    renderForms();
    dial.loaded = false;
    toast(t(r.restart ? "s.importedRestart" : "s.imported"));
  } catch (err) {
    toast(err.message, true);
  }
  e.target.value = "";
});

$("#s-restart").addEventListener("click", (e) => busy(e.target, async () => {
  await post("/api/restart");
  toast(t("s.restarting"));
  setTimeout(() => location.reload(), 10000);
}));

// ---------- language ----------

function rerender() {
  applyI18n();
  dial.render();
  showLed();
  if (cfg) renderForms();
  loadLog();
  if (status) { renderStatus(); renderSystem(); }
}

const langSelect = $("#lang");
for (const code of LANGS) langSelect.add(new Option(I18N[code]["lang.name"], code));
langSelect.value = lang;
langSelect.addEventListener("change", () => { setLang(langSelect.value); rerender(); });

// ---------- start ----------

applyI18n();
(async () => {
  try {
    cfg = await api("/api/config");
    renderForms();
  } catch (e) {
    toast(e.message, true);
  }
  await refresh();
  setInterval(refresh, 5000);
})();

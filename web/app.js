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

const num = (v, decimals = 1) => (v === null || v === undefined || Number.isNaN(v))
  ? "–" : Number(v).toLocaleString(lang, { maximumFractionDigits: decimals });

function duration(s) {
  if (s < 0 || s === undefined) return "–";
  if (s < 90) return t("unit.s", { n: Math.round(s) });
  if (s < 5400) return t("unit.min", { n: Math.round(s / 60) });
  if (s < 172800) return t("unit.h", { n: Math.round(s / 3600) });
  return t("unit.d", { n: Math.round(s / 86400) });
}

async function busy(button, task) {
  button.disabled = true;
  try { return await task(); } catch (e) { toast(e.message, true); } finally { button.disabled = false; }
}

// Translates the device error messages (kept in English in the firmware).
const ERRORS = [
  [/needle hub not found/, "err.hub"],
  [/needle contrast too low/, "err.contrast"],
  [/not in dial order/, "err.order"],
  [/dial not configured|set at least the start/, "err.notConfigured"],
  [/search band too narrow/, "err.band"],
  [/no photo yet/, "err.noPhoto"],
  [/camera/, "err.camera"],
];
function deviceError(message) {
  if (!message) return "";
  for (const [re, key] of ERRORS) if (re.test(message)) return t(key, { error: message });
  return message;
}

// ---------- overlay drawing ----------

const COLORS = { start: "#2ecc71", end: "#e056fd", point: "#f9ca24", pivot: "#ff3b30",
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
  "cam_brightness", "cam_contrast"];
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

$$("#tabs button").forEach((b) => b.addEventListener("click", () => {
  $$("#tabs button").forEach((x) => x.classList.toggle("active", x === b));
  $$(".tab").forEach((t) => t.classList.toggle("active", t.id === `tab-${b.dataset.tab}`));
  if (b.dataset.tab === "dial") dial.enter();
}));

// ---------- reading tab ----------

let photoStamp = 0;
function photoUrl() { return `/api/photo.jpg?t=${photoStamp}`; }

function renderStatus() {
  const s = status;
  $("#title").textContent = s.name;
  document.title = s.name;
  const conn = $("#conn");
  conn.textContent = t(s.wifi.ap && !s.wifi.connected ? "conn.ap" :
    s.mqtt.connected ? "conn.ok" : s.mqtt.configured ? "conn.down" : "conn.none");
  conn.className = "pill " + (s.mqtt.connected ? "ok" : s.mqtt.configured ? "err" : "");

  const g = s.good || {};
  $("#r-level").textContent = g.level === null || g.level === undefined ? "–" : `${num(g.level)} ${g.unit}`;
  $("#r-volume").textContent = g.volume_l === null || g.volume_l === undefined ? "–" : `${num(g.volume_l, 0)} L`;
  $("#r-percent").textContent = g.percent === null || g.percent === undefined ? "–" : `${num(g.percent)} %`;

  const a = s.attempt || {};
  const line = $("#r-status");
  if (!s.dial_ready) {
    line.textContent = t("r.notReady");
    line.className = "status warn";
  } else if (a.ok) {
    line.textContent = t("r.ok", { contrast: num(a.detail?.contrast, 0) });
    line.className = "status ok";
  } else if (a.error) {
    line.textContent = deviceError(a.error);
    line.className = "status err";
  } else {
    line.textContent = "";
  }
  const last = g.age_s >= 0 ? t("r.last", { age: duration(g.age_s) }) : t("r.never");
  $("#r-times").textContent = `${last} · ${t("r.next", { in: duration(s.next_read_s) })}`;

  if (s.photo) {
    const stamp = Math.round(Date.now() / 1000 - s.photo.age_s);
    if (Math.abs(stamp - photoStamp) > 2) {
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

async function refresh() {
  try {
    status = await api("/api/status");
    renderStatus();
    renderSystem();
  } catch (e) {
    $("#conn").textContent = t("conn.offline");
    $("#conn").className = "pill err";
  }
}

$("#r-read").addEventListener("click", (e) => busy(e.target, async () => {
  const r = await post("/api/read");
  if (!r.ok) toast(deviceError(r.error), true);
  await refresh();
}));

// ---------- dial tab ----------

const dial = {
  tool: "roi",
  draft: null,
  detail: null,
  drag: null,
  moving: null,
  loaded: false,
  orient: null,  // capture settings of the displayed photo and of the draft coordinates
  capturing: false,
  again: false,

  enter() {
    if (!this.loaded && cfg) this.reset();
    if (!$("#d-photo-img").src && status?.photo) this.loadPhoto();
  },

  reset() {
    const p = cfg.points || [];
    const pt = (a) => ({ x: a[0], y: a[1], v: a[2] });
    this.draft = {
      roi: cfg.roi && cfg.roi[2] > 0 ? [...cfg.roi] : null,
      pivot: cfg.pivot_x >= 0 ? [cfg.pivot_x, cfg.pivot_y] : null,
      start: p.length >= 2 ? pt(p[0]) : null,
      end: p.length >= 2 ? pt(p[p.length - 1]) : null,
      mids: p.slice(1, -1).map(pt),
    };
    $("#d-mode").value = cfg.pivot_mode;
    $("#d-unit").value = cfg.unit;
    this.orient = pick(cfg, SHOT_KEYS);
    fill($("#d-shot"), this.orient);
    $("#d-shot-box").open = !(p.length >= 2);  // folded once the dial is set up
    showLed();
    this.loaded = true;
    this.setTool(this.draft.roi ? (this.draft.pivot ? "point" : "pivot") : "roi");
    this.renderTable();
  },

  loadPhoto() { $("#d-photo-img").src = `/api/photo.jpg?t=${Date.now()}`; },

  readShot() {
    const v = read($("#d-shot"));
    v.rotation = Number(v.rotation);
    return v;
  },

  // Moves the outline, centre and points to a photo taken with `to`.
  reorient(to) {
    const from = this.orient;
    this.orient = to;
    if (sameOrient(from, to)) return;
    const d = this.draft;
    const move = (p) => { if (p) [p.x, p.y] = mapPoint(p.x, p.y, from, to); };
    if (d.roi) d.roi = mapRect(d.roi, from, to);
    if (d.pivot) d.pivot = mapPoint(d.pivot[0], d.pivot[1], from, to);
    move(d.start);
    move(d.end);
    d.mids.forEach(move);
    this.renderTable();
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
        this.detail = null;  // the last test was on the previous photo
        $("#d-result").textContent = "";
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

  setTool(tool, moving = null) {
    this.tool = tool;
    this.moving = moving;
    $$("#d-tools button").forEach((b) => b.classList.toggle("active", b.dataset.tool === tool));
    this.hintKey = `d.hint.${tool}`;
    $("#d-hint").textContent = t(this.hintKey);
  },

  scene() {
    const d = this.draft;
    const points = [];
    if (d.start) points.push({ ...d.start, kind: "start" });
    for (const m of d.mids) points.push({ ...m, kind: "point" });
    if (d.end) points.push({ ...d.end, kind: "end" });
    return { roi: d.roi, pivot: d.pivot, points, detail: this.detail };
  },

  draw() { drawOverlay($("#d-overlay"), $("#d-photo-img"), this.scene()); },

  changed() {
    this.detail = null;
    $("#d-result").textContent = "";
    this.renderTable();
    this.draw();
  },

  renderTable() {
    const d = this.draft;
    const body = $("#d-points");
    body.innerHTML = "";
    const row = (p, kind, label, index) => {
      const tr = document.createElement("tr");
      tr.innerHTML = `<td><span class="dot" style="background:${COLORS[kind]}"></span> ${label}</td>
        <td><input type="number" step="any"></td><td></td>`;
      tr.title = p ? `x ${Math.round(p.x)}, y ${Math.round(p.y)}` : t("d.toPlace");
      const input = tr.querySelector("input");
      if (p) input.value = Number.isFinite(p.v) ? p.v : "";
      else { input.disabled = true; input.placeholder = t("d.toPlace"); }
      input.addEventListener("input", () => { p.v = input.value === "" ? NaN : Number(input.value); this.detail = null; this.draw(); });
      const cell = tr.lastElementChild;
      if (p) {
        const move = document.createElement("button");
        move.textContent = t("d.move");
        move.addEventListener("click", () => this.setTool("move", p));
        cell.append(move);
      }
      if (kind === "point") {
        const del = document.createElement("button");
        del.textContent = "×";
        del.title = t("d.delete");
        del.addEventListener("click", () => { d.mids.splice(index, 1); this.changed(); });
        cell.append(del);
      }
      body.append(tr);
      return input;
    };
    const inputs = [row(d.start, "start", t("d.row.start"))];
    d.mids.forEach((m, i) => inputs.push(row(m, "point", t("d.row.point", { n: i + 1 }), i)));
    inputs.push(row(d.end, "end", t("d.row.end")));
    return inputs;
  },

  focusValue(p) {
    const inputs = this.renderTable();
    const d = this.draft;
    const order = [d.start, ...d.mids, d.end];
    const i = order.indexOf(p);
    if (i >= 0 && inputs[i]) { inputs[i].focus(); inputs[i].select(); }
  },

  imagePoint(e) {
    const c = $("#d-overlay");
    const r = c.getBoundingClientRect();
    return [Math.round((e.clientX - r.left) * c.width / r.width),
      Math.round((e.clientY - r.top) * c.height / r.height)];
  },

  onDown(e) {
    if (!$("#d-photo-img").naturalWidth) return;
    e.preventDefault();  // keeps the focus in the value field
    const [x, y] = this.imagePoint(e);
    const d = this.draft;
    switch (this.tool) {
      case "roi":
        this.drag = [x, y];
        d.roi = [x, y, 0, 0];
        e.target.setPointerCapture(e.pointerId);
        return;
      case "pivot":
        d.pivot = [x, y];
        this.changed();
        this.setTool(d.start ? "point" : "start");
        return;
      case "start":
      case "end": {
        const fallback = this.tool === "start" ? 0 : 150;
        d[this.tool] = { x, y, v: d[this.tool] ? d[this.tool].v : fallback };
        const placed = d[this.tool];
        this.changed();
        this.setTool(this.tool === "start" && !d.end ? "end" : "point");
        this.focusValue(placed);
        return;
      }
      case "point": {
        const p = { x, y, v: NaN };
        d.mids.push(p);
        this.changed();
        this.focusValue(p);
        return;
      }
      case "move":
        Object.assign(this.moving, { x, y });
        this.changed();
        this.setTool("point");
    }
  },

  onMove(e) {
    if (!this.drag) return;
    const [x, y] = this.imagePoint(e);
    const [x0, y0] = this.drag;
    this.draft.roi = [Math.min(x, x0), Math.min(y, y0), Math.abs(x - x0), Math.abs(y - y0)];
    this.draw();
  },

  onUp() {
    if (!this.drag) return;
    this.drag = null;
    const roi = this.draft.roi;
    if (roi[2] < 20 || roi[3] < 20) this.draft.roi = null;
    this.changed();
    if (this.draft.roi) this.setTool(this.draft.pivot ? "point" : "pivot");
  },

  // Builds the configuration to test or save, or throws a readable error.
  payload() {
    const d = this.draft;
    if (!d.pivot) throw new Error(t("d.err.pivot"));
    if (!d.start || !d.end) throw new Error(t("d.err.ends"));
    const all = [d.start, ...d.mids, d.end];
    if (all.some((p) => !Number.isFinite(p.v))) throw new Error(t("d.err.values"));
    const rising = d.end.v > d.start.v;
    const lo = Math.min(d.start.v, d.end.v), hi = Math.max(d.start.v, d.end.v);
    if (d.start.v === d.end.v) throw new Error(t("d.err.same"));
    if (d.mids.some((p) => p.v <= lo || p.v >= hi)) throw new Error(t("d.err.range", { lo, hi }));
    const mids = [...d.mids].sort((a, b) => rising ? a.v - b.v : b.v - a.v);
    const points = [d.start, ...mids, d.end].map((p) => [p.x, p.y, p.v]);
    return {
      ...this.readShot(),
      ...pick(this.orient, ORIENT_KEYS),
      roi: d.roi || [0, 0, 0, 0],
      pivot_mode: $("#d-mode").value,
      pivot_x: d.pivot[0],
      pivot_y: d.pivot[1],
      points,
      unit: $("#d-unit").value.trim() || "cm",
    };
  },

  async test() {
    const body = this.payload();
    const r = await post("/api/test", body);
    this.detail = r.detail || null;
    const line = $("#d-result");
    if (r.ok) {
      line.textContent = t("d.result", { value: num(r.level), unit: r.unit,
        angle: num(r.detail.needle_angle), contrast: num(r.detail.contrast, 0) });
      line.className = "status ok";
    } else {
      line.textContent = deviceError(r.error);
      line.className = "status err";
    }
    this.draw();
  },

  async save() {
    this.reorient(this.readShot());
    const body = this.payload();
    await post("/api/config", body);
    cfg = await api("/api/config");
    toast(t("d.saved"));
    await this.test();
  },
};

$("#d-photo-img").addEventListener("load", () => dial.draw());
const overlay = $("#d-overlay");
overlay.addEventListener("pointerdown", (e) => dial.onDown(e));
overlay.addEventListener("pointermove", (e) => dial.onMove(e));
overlay.addEventListener("pointerup", () => dial.onUp());
$$("#d-tools button").forEach((b) => b.addEventListener("click", () => dial.setTool(b.dataset.tool)));
$("#d-photo").addEventListener("click", () => dial.takePhoto());
$("#d-shot").addEventListener("change", () => dial.takePhoto());
$("#d-shot").addEventListener("input", showLed);
$("#d-shot").addEventListener("submit", (e) => e.preventDefault());
function showLed() {
  const v = Number($("#d-shot").led_intensity.value);
  $("#d-led-value").textContent = v ? `${Math.round(v / 2.55)} %` : t("d.off");
}
$("#d-test").addEventListener("click", (e) => busy(e.target, () => dial.test()));
$("#d-save").addEventListener("click", (e) => busy(e.target, () => dial.save()));
$("#d-mode").addEventListener("change", () => dial.changed());
$("#d-zoom").addEventListener("change", (e) => {
  const z = e.target.value;
  const viewer = $("#d-viewer");
  const img = $("#d-photo-img");
  viewer.classList.toggle("zoomed", z !== "fit");
  const w = z === "fit" ? "" : `${img.naturalWidth * Number(z)}px`;
  img.style.width = w;
  $("#d-overlay").style.width = w;
  $("#d-overlay").style.height = z === "fit" ? "" : `${img.naturalHeight * Number(z)}px`;
});

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
  fill($("#f-tank"), cfg);
  fill($("#f-camera"), { ...cfg, interval_min: Math.round(cfg.interval_s / 60) });
  const net = $("#f-network");
  fill(net, { ...cfg, wifi_pass: "", mqtt_pass: "", ui_password: "", ui_password_clear: false });
  net.wifi_pass.placeholder = cfg.wifi_pass_set ? t("n.unchanged") : "";
  net.mqtt_pass.placeholder = cfg.mqtt_pass_set ? t("n.unchanged") : "";
  net.ui_password.placeholder = cfg.ui_password_set ? t("n.unchanged") : t("n.nonePw");
  renderCapacity();
}

function renderCapacity() {
  const v = read($("#f-tank"));
  const cap = v.tank_length_cm * v.tank_width_cm * v.tank_height_cm / 1000;
  const perCm = v.tank_length_cm * v.tank_width_cm / 1000;
  $("#t-capacity").textContent = cap > 0
    ? t("t.capacity", { cap: num(cap, 0), perCm: num(perCm, 2) })
    : perCm > 0 ? t("t.perCm", { perCm: num(perCm, 2) }) : "";
}
$("#f-tank").addEventListener("input", renderCapacity);

async function saveForm(e, transform = (v) => v) {
  e.preventDefault();
  const button = e.target.querySelector("button.primary");
  await busy(button, async () => {
    const r = await post("/api/config", transform(read(e.target)));
    cfg = await api("/api/config");
    renderForms();
    toast(t(r.restart ? "savedRestart" : "saved"));
  });
}

$("#f-tank").addEventListener("submit", (e) => saveForm(e));
$("#f-camera").addEventListener("submit", (e) => saveForm(e, (v) => {
  v.interval_s = Math.max(1, v.interval_min) * 60;
  delete v.interval_min;
  return v;
}));
$("#f-network").addEventListener("submit", (e) => saveForm(e, (v) => {
  for (const key of ["wifi_pass", "mqtt_pass", "ui_password"]) if (!v[key]) delete v[key];
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

function renderSystem() {
  const s = status;
  const kb = t("s.kb");
  const rows = [
    [t("s.version"), s.version], [t("s.id"), s.id], [t("s.uptime"), duration(s.uptime_s)],
    [t("s.wifi"), s.wifi.connected ? `${s.wifi.ssid} (${s.wifi.rssi} dBm), ${s.wifi.ip}` : t("s.disconnected")],
    [t("s.ap"), s.wifi.ap ? t("s.apOn", { name: s.wifi.hostname, ip: s.wifi.ap_ip }) : t("s.closed")],
    [t("s.mqtt"), t(s.mqtt.connected ? "s.connected" : s.mqtt.configured ? "s.disconnected" : "s.notSet")],
    [t("s.photo"), s.photo ? `${s.photo.width} × ${s.photo.height}, ${num(s.photo.bytes / 1024, 0)} ${kb}` : t("s.none")],
    [t("s.memory"), `${num(s.heap_free / 1024, 0)} ${kb} + ${num(s.psram_free / 1024, 0)} ${kb} PSRAM`],
  ];
  $("#s-info").innerHTML = rows.map(([k, v]) => `<dt>${k}</dt><dd>${String(v).replace(/</g, "&lt;")}</dd>`).join("");
}

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
  if (dial.hintKey) $("#d-hint").textContent = t(dial.hintKey);
  if (dial.draft) dial.renderTable();
  showLed();
  if (cfg) renderForms();
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

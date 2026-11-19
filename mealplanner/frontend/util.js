const S = {
  uid: localStorage.getItem('mp_uid') || (() => { const v = 'u' + Math.random().toString(36).slice(2, 12); localStorage.setItem('mp_uid', v); return v; })(),
  profile: null, targets: null, plan: null, tab: 'home', day: Math.min(6, (new Date().getDay() + 6) % 7),
  grocery: null, stores: null, recipes: [], recFilter: 'all', recQuery: '', busy: false,
  checked: new Set(JSON.parse(localStorage.getItem('mp_checked') || '[]')), units: localStorage.getItem('mp_units') || 'metric',
  draft: null, wizStep: 0,
};

async function api(method, path, body) {
  const r = await fetch('/api' + path, { method, headers: { 'Content-Type': 'application/json' }, body: body ? JSON.stringify(body) : undefined });
  let j = {}; try { j = await r.json(); } catch (e) {}
  if (!r.ok) throw new Error(j.error || 'Request failed (' + r.status + ')');
  return j;
}

const $ = (s, el = document) => el.querySelector(s);
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const money = n => '$' + (Math.round((n || 0) * 100) / 100).toFixed(2);
// Grocery totals are a range: ~85% of the Instacart price up to the Instacart price (see INSTACART_LOW_FACTOR in planner.hpp).
const moneyRange = (lo, hi) => (lo == null || Math.round(lo) === Math.round(hi)) ? money(hi) : '$' + Math.round(lo) + '–$' + Math.round(hi);
const cap = s => s ? s[0].toUpperCase() + s.slice(1) : '';
const DAY_LONG = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];

const EMOJI = [[/oat/i, '🥣'], [/parfait|yogurt/i, '🍓'], [/toast/i, '🥑'], [/scramble|hash/i, '🍳'], [/burrito|wrap|quesadilla/i, '🌯'],
  [/salad/i, '🥗'], [/pasta|bolognese|noodle|primavera/i, '🍝'], [/soup|stew|chili/i, '🍲'], [/curry/i, '🍛'], [/salmon|tuna/i, '🐟'],
  [/chicken/i, '🍗'], [/rice|bowl/i, '🍚'], [/potato|bake/i, '🥔'], [/stir/i, '🥘']];
const foodEmoji = name => (EMOJI.find(([re]) => re.test(name)) || [0, '🍽️'])[1];

function toast(msg) {
  const old = $('.toast'); if (old) old.remove();
  const t = document.createElement('div'); t.className = 'toast'; t.textContent = msg;
  $('#app').appendChild(t); setTimeout(() => t.remove(), 2600);
}

function openSheet(html) {
  closeSheet(true);
  $('#overlay').innerHTML = `<div class="scrim" data-act="closeSheet"></div><div class="sheet"><div class="grab"></div>${html}</div>`;
  requestAnimationFrame(() => { $('.scrim').classList.add('on'); $('.sheet').classList.add('on'); });
}
function closeSheet(instant) {
  const o = $('#overlay'); if (!o.firstChild) return;
  if (instant) { o.innerHTML = ''; return; }
  $('.scrim').classList.remove('on'); $('.sheet').classList.remove('on'); setTimeout(() => o.innerHTML = '', 220);
}

function gaugeSvg(value, goal) {
  const N = 11, cx = 150, cy = 150, r = 118, start = 205, end = -25, ratio = goal > 0 ? Math.min(1, value / goal) : 0;
  const filled = Math.round(ratio * N - 1e-6 + (ratio > 0 ? 0.5 : 0));
  let out = '';

  const inset = 7, rIn = r - 14 + inset, rOut = r + 14 - inset, edge = (inset / r) * 180 / Math.PI + 1.8;
  const pt = (rad, a) => [(cx + rad * Math.cos(a * Math.PI / 180)).toFixed(1), (cy - rad * Math.sin(a * Math.PI / 180)).toFixed(1)];
  for (let i = 0; i < N; i++) {
    const a0 = start + (end - start) * (i / N) - edge, a1 = start + (end - start) * ((i + 1) / N) + edge;
    const [ox0, oy0] = pt(rOut, a0), [ox1, oy1] = pt(rOut, a1), [ix1, iy1] = pt(rIn, a1), [ix0, iy0] = pt(rIn, a0);
    const col = i < filled ? '#f4a47c' : '#e6e1d9';
    out += `<path d="M${ox0} ${oy0} A${rOut} ${rOut} 0 0 1 ${ox1} ${oy1} L${ix1} ${iy1} A${rIn} ${rIn} 0 0 0 ${ix0} ${iy0} Z" fill="${col}" stroke="${col}" stroke-width="${inset * 2}" stroke-linejoin="round"/>`;
  }
  return `<svg viewBox="0 0 300 190" role="img" aria-label="${Math.round(value)} of ${Math.round(goal)} calories">${out}</svg>`;
}

const macroBar = (label, v, goal) => `<div class="macro"><span class="muted small">${label}</span><div><b>${Math.round(v)}g</b> <span class="muted small">/ ${Math.round(goal)}g</span></div><div class="bar"><i style="width:${Math.min(100, goal ? v / goal * 100 : 0)}%"></i></div></div>`;

const kgToLb = kg => kg * 2.20462, lbToKg = lb => lb / 2.20462;
const cmToIn = cm => cm / 2.54;

function persistChecked() { localStorage.setItem('mp_checked', JSON.stringify([...S.checked])); }

function setBusy(on, label) {
  S.busy = on;
  const b = $('#busy'); if (b) b.remove();
  if (on) { const d = document.createElement('div'); d.id = 'busy'; d.className = 'toast'; d.innerHTML = `<span class="spin"></span> ${esc(label || 'Working…')}`; $('#app').appendChild(d); }
}

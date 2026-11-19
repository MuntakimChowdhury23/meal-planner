const ALLERGENS = ['Peanuts', 'Tree nuts', 'Dairy', 'Eggs', 'Gluten', 'Soy', 'Fish', 'Shellfish', 'Sesame'];
const CUISINES = ['American', 'Mexican', 'Italian', 'Asian', 'Indian', 'Mediterranean'];
const DIETS = [['omnivore', 'Anything'], ['pescatarian', 'Pescatarian'], ['vegetarian', 'Vegetarian'], ['vegan', 'Vegan']];
const ACTIVITY = [['sedentary', 'Mostly sitting'], ['light', 'Light (1–3 workouts/wk)'], ['moderate', 'Moderate (3–5/wk)'], ['active', 'Active (6–7/wk)'], ['very_active', 'Very active / physical job']];

function defaultDraft() {
  return { name: '', sex: 'female', age: 30, height_cm: 168, weight_kg: 75, activity: 'light', loss_kg_per_week: 0.5, diet: 'omnivore',
    cuisines: [], dislikes: [], allergies: [], people: 1, weekly_budget: 0, radius_km: 10, zip: '', lat: 40.7484, lon: -73.9857 };
}

function viewSplash() {
  return `<div class="hero"><div class="art">🥗</div>
    <span class="tag" style="top:6%;left:8%">170 kcal</span><span class="tag" style="top:14%;right:8%">110 kcal</span><span class="tag" style="top:26%;left:38%">90 kcal</span>
    <h1>Eat well.<br>Spend less.</h1><p>Weekly meal plans that fit your calories, skip your allergens and shop at the cheapest nearby store.</p>
    <div class="dots"><i></i><i></i><i class="on"></i></div><button class="btn" data-act="startWizard">Get Started</button></div>`;
}

const chipSet = (items, sel, act, extra = '') => `<div class="chips">${items.map(i => `<button class="chip ${sel.map(s => s.toLowerCase()).includes(i.toLowerCase()) ? 'on' : ''}" data-act="${act}" data-v="${esc(i)}" ${extra}>${esc(i)}</button>`).join('')}</div>`;

function viewWizard(editing) {
  const d = S.draft, imp = S.units === 'imperial', step = S.wizStep;
  const bar = `<div class="row" style="gap:6px;margin-bottom:16px">${[0, 1, 2].map(i => `<div style="flex:1;height:5px;border-radius:9px;background:${i <= step ? 'var(--peach)' : 'var(--track)'}"></div>`).join('')}</div>`;
  let body = '';
  if (step === 0) {
    const w = imp ? Math.round(kgToLb(d.weight_kg)) : Math.round(d.weight_kg), h = imp ? Math.round(cmToIn(d.height_cm)) : Math.round(d.height_cm);
    body = `<div class="h2">About you</div>
      <div class="field"><label>Units</label><div class="seg"><button class="${!imp ? 'on' : ''}" data-act="units" data-u="metric">kg / cm</button><button class="${imp ? 'on' : ''}" data-act="units" data-u="imperial">lb / in</button></div></div>
      <div class="field"><label>Sex (for calorie formula)</label><div class="seg">${[['female', 'Female'], ['male', 'Male'], ['other', 'Other']].map(s => `<button class="${d.sex === s[0] ? 'on' : ''}" data-act="setSex" data-v="${s[0]}">${s[1]}</button>`).join('')}</div></div>
      <div class="grid2"><div class="field"><label>Age</label><input class="inp" id="w_age" type="number" inputmode="numeric" value="${d.age}"></div>
      <div class="field"><label>Height (${imp ? 'in' : 'cm'})</label><input class="inp" id="w_h" type="number" inputmode="decimal" value="${h}"></div></div>
      <div class="field"><label>Weight (${imp ? 'lb' : 'kg'})</label><input class="inp" id="w_w" type="number" inputmode="decimal" value="${w}"></div>
      <div class="field"><label>Activity level</label><select class="inp" id="w_act">${ACTIVITY.map(a => `<option value="${a[0]}" ${d.activity === a[0] ? 'selected' : ''}>${a[1]}</option>`).join('')}</select></div>
      <div class="field"><label>Weight-loss goal · <span class="accent" id="rateLbl">${rateLabel(d.loss_kg_per_week)}</span></label><input id="w_rate" type="range" min="0" max="1" step="0.25" value="${Math.max(0, d.loss_kg_per_week)}"></div>
      <div id="calcPreview"></div>`;
  } else if (step === 1) {
    body = `<div class="h2">Food preferences</div>
      <div class="field"><label>Diet</label>${chipSet(DIETS.map(x => x[1]), [DIETS.find(x => x[0] === d.diet)[1]], 'setDiet')}</div>
      <div class="field"><label>Favourite cuisines</label>${chipSet(CUISINES, d.cuisines, 'togCuisine')}</div>
      <div class="field"><label>Foods you dislike</label><div class="chips" style="margin-bottom:8px">${d.dislikes.map(x => `<button class="chip x on" data-act="rmDislike" data-v="${esc(x)}">${esc(x)}</button>`).join('')}</div>
        <div class="row"><input class="inp" id="w_dis" placeholder="e.g. tofu, cabbage" autocomplete="off"><button class="btn sm peach" data-act="addDislike">Add</button></div></div>`;
  } else {
    body = `<div class="h2">Allergies</div><p class="muted small" style="margin-top:-6px">Recipes containing these are removed completely — from plans, swaps and the recipe list.</p>
      <div class="field">${chipSet(ALLERGENS, d.allergies, 'togAllergy')}</div>
      <div class="field"><label>Other allergy (ingredient name)</label><div class="chips" style="margin-bottom:8px">${d.allergies.filter(a => !ALLERGENS.map(x => x.toLowerCase()).includes(a.toLowerCase())).map(x => `<button class="chip x on" data-act="rmAllergy" data-v="${esc(x)}">${esc(x)}</button>`).join('')}</div>
        <div class="row"><input class="inp" id="w_alg" placeholder="e.g. garlic" autocomplete="off"><button class="btn sm peach" data-act="addAllergy">Add</button></div></div>
      <div class="grid2"><div class="field"><label>People eating</label><input class="inp" id="w_ppl" type="number" min="1" max="8" inputmode="numeric" value="${d.people}"></div>
      <div class="field"><label>Weekly budget ($, optional)</label><input class="inp" id="w_bud" type="number" min="0" inputmode="decimal" value="${d.weekly_budget || ''}" placeholder="none"></div></div>
      <div class="field"><label>Your ZIP code (required, used to find nearby stores and prices)</label><input class="inp" id="w_zip" required inputmode="numeric" maxlength="5" pattern="[0-9]{5}" placeholder="e.g. 27858" value="${esc(d.zip || '')}"></div>
      <div class="field"><label>Search stores within · <span class="accent" id="radLbl">${d.radius_km} km</span></label><input id="w_rad" type="range" min="2" max="40" step="1" value="${d.radius_km}"></div>
      <button class="btn ghost sm" data-act="useLocation" style="margin-bottom:10px">📍 Use my location</button>
      <p class="muted small">Plans are estimates to support healthy eating and are not medical advice. If you have a medical condition or severe allergy, check recipes with your clinician and read every label.</p>`;
  }
  const last = step === 2;
  return `<div class="topbar"><button class="iconbtn" data-act="wizBack" aria-label="Back">${step === 0 && !editing ? '' : '←'}</button><h1>${editing ? 'Your profile' : 'Set up'}</h1><span class="muted small">${step + 1}/3</span></div>${bar}${body}
    <div style="margin-top:18px"><button class="btn peach" data-act="${last ? 'saveProfile' : 'wizNext'}">${last ? (editing ? 'Save & rebuild plan' : 'Create my plan') : 'Continue'}</button></div>`;
}

function rateLabel(r) { return r <= 0 ? 'Maintain weight' : S.units === 'imperial' ? `Lose ${(r * 2.20462).toFixed(1)} lb / week` : `Lose ${r} kg / week`; }

function readStep() {
  const d = S.draft, v = id => { const e = document.getElementById(id); return e ? e.value : null; };
  if (S.wizStep === 0) {
    const imp = S.units === 'imperial';
    d.age = +v('w_age') || d.age; d.activity = v('w_act') || d.activity; d.loss_kg_per_week = +v('w_rate');
    const h = +v('w_h'), w = +v('w_w'); d.height_cm = imp ? h * 2.54 : h; d.weight_kg = imp ? lbToKg(w) : w;
  } else if (S.wizStep === 2) {
    d.people = Math.max(1, Math.min(8, +v('w_ppl') || 1)); d.weekly_budget = +v('w_bud') || 0; d.radius_km = +v('w_rad') || 10; d.zip = (v('w_zip') || '').trim();
  }
}

let calcTimer;
function previewCalc() {
  clearTimeout(calcTimer);
  calcTimer = setTimeout(async () => {
    const box = $('#calcPreview'); if (!box) return;
    try {
      readStep(); const t = await api('POST', '/calc', S.draft);
      box.innerHTML = `<div class="card" style="background:var(--peach-soft)"><div class="muted small">Your daily targets</div><div class="row between"><div><b style="font-size:26px">${t.kcal}</b> kcal</div><div class="small">P ${t.protein}g · C ${t.carbs}g · F ${t.fat}g</div></div>
        <div class="muted small">Maintenance ≈ ${t.tdee} kcal (BMR ${t.bmr})</div>${t.warnings.map(w => `<div class="small" style="color:var(--warn);margin-top:6px">⚠ ${esc(w)}</div>`).join('')}</div>`;
    } catch (e) {}
  }, 250);
}

function viewProfileTab() {
  S.draft = S.draft || JSON.parse(JSON.stringify(S.profile));
  return viewWizard(true);
}

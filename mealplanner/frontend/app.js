const NAV = [['home', '🏠', 'Home'], ['plan', '📖', 'Plan'], ['scan', '🛒', 'Groceries'], ['recipes', '⭐', 'Recipes'], ['profile', '⚙️', 'Settings']];

function renderNav() {
  const n = $('#nav'); n.hidden = !S.profile;
  n.innerHTML = NAV.map(([k, ic, lb]) => `<button class="${k === 'scan' ? 'center' : ''} ${(S.tab === k || (k === 'scan' && S.tab === 'cart')) ? 'on' : ''}" data-act="tab" data-t="${k === 'scan' ? 'cart' : k}" aria-label="${lb}">${ic}</button>`).join('');
}

let lastTab = null;
function render() {
  const sc = $('#screen'), top = lastTab === S.tab ? sc.scrollTop : 0; lastTab = S.tab;
  if (!S.profile && S.tab !== 'wizard') { sc.innerHTML = ''; $('#overlay').innerHTML = viewSplash(); renderNav(); return; }
  let html = '';
  if (S.tab === 'wizard') html = viewWizard(!!S.profile);
  else if (S.tab === 'home') html = viewHome();
  else if (S.tab === 'plan') html = viewPlan();
  else if (S.tab === 'cart') html = viewCart();
  else if (S.tab === 'recipes') html = viewRecipes();
  else if (S.tab === 'profile') html = viewProfileTab();
  sc.innerHTML = html; sc.scrollTop = top; renderNav();
  if (S.tab === 'wizard' || S.tab === 'profile') { previewCalc(); wireWizardInputs(); }
  if (S.tab === 'recipes') { const q = $('#recQ'); if (q) q.oninput = debounce(e => { S.recQuery = e.target.value; loadRecipes(true); }, 250); }
}

function wireWizardInputs() {
  const rate = $('#w_rate'); if (rate) rate.oninput = () => { $('#rateLbl').textContent = rateLabel(+rate.value); previewCalc(); };
  const rad = $('#w_rad'); if (rad) rad.oninput = () => { $('#radLbl').textContent = rad.value + ' km'; };
  ['w_age', 'w_h', 'w_w', 'w_act'].forEach(id => { const e = document.getElementById(id); if (e) e.oninput = previewCalc; });
}

const debounce = (fn, ms) => { let t; return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; };

async function loadRecipes(keepFocus) {
  const q = new URLSearchParams({ user: S.uid }); if (S.recQuery) q.set('q', S.recQuery);
  if (['breakfast', 'lunch', 'dinner'].includes(S.recFilter)) q.set('meal', S.recFilter);
  if (S.recFilter === 'vegan') q.set('diet', 'vegan');
  const r = await api('GET', '/recipes?' + q); S.recipes = r.recipes;
  if (S.tab === 'recipes') { render(); if (keepFocus) { const i = $('#recQ'); if (i) { i.focus(); i.setSelectionRange(i.value.length, i.value.length); } } }
}

async function loadCart() {
  if (!S.plan) return;
  try {
    const [g, st] = await Promise.all([api('GET', `/plan/${S.plan.id}/grocery${S.storePick ? '?store=' + S.storePick : ''}`), api('GET', `/plan/${S.plan.id}/stores`)]);
    S.grocery = g; S.stores = st; if (S.tab === 'cart') render();
  } catch (e) { toast(e.message); }
}

async function go(tab) {
  S.tab = tab; closeSheet(true);
  if (tab === 'profile') S.draft = JSON.parse(JSON.stringify(S.profile)), S.wizStep = 0;
  render(); $('#screen').scrollTop = 0;
  if (tab === 'recipes') loadRecipes(); if (tab === 'cart') loadCart();
}

async function generate(keep) {
  if (S.busy) return; setBusy(true, 'Optimizing meals and prices…');
  try {
    S.plan = await api('POST', '/plan/generate', { user_id: S.uid, plan_id: keep && S.plan ? S.plan.id : undefined });
    S.grocery = S.stores = null; S.storePick = 0;
    setBusy(false); toast(keep ? 'Plan regenerated' : 'Your week is ready'); render();
  } catch (e) { setBusy(false); toast(e.message); }
}

async function refreshPlan(p) { S.plan = p; S.grocery = S.stores = null; render(); }

const ACTIONS = {
  tab: el => go(el.dataset.t),
  day: el => { S.day = +el.dataset.i; render(); },
  generate: el => generate(!!el.dataset.keep),
  closeSheet: () => closeSheet(),
  recipe: el => showRecipe(el.dataset.id, el.dataset.slot),
  swap: el => showSwap(+el.dataset.slot),
  doSwap: async el => {
    try { closeSheet(true); await refreshPlan(await api('POST', `/plan/${S.plan.id}/swap`, { slot: +el.dataset.slot, recipe_id: +el.dataset.id })); toast('Meal swapped & kept'); } catch (e) { toast(e.message); }
  },
  lock: async el => { try { await refreshPlan(await api('POST', `/plan/${S.plan.id}/lock`, { slot: +el.dataset.slot, locked: el.dataset.locked === '1' })); } catch (e) { toast(e.message); } },
  log: async el => { try { await refreshPlan(await api('POST', `/plan/${S.plan.id}/log`, { slot: +el.dataset.slot, eaten: el.dataset.eaten === '1' })); } catch (e) { toast(e.message); } },
  review: () => showReview(),
  confirm: async () => { try { closeSheet(true); await refreshPlan(await api('POST', `/plan/${S.plan.id}/confirm`, {})); toast('Plan confirmed ✓'); } catch (e) { toast(e.message); } },
  recFilter: el => { S.recFilter = el.dataset.f; loadRecipes(); },
  pickStore: el => { S.storePick = +el.dataset.id; loadCart(); },
  tick: el => { const id = +el.dataset.id; S.checked.has(id) ? S.checked.delete(id) : S.checked.add(id); persistChecked(); render(); },
  priceEdit: el => showPriceEdit(el),
  submitPrice: async el => {
    const price = +$('#pe_price').value, pack = +$('#pe_pack').value;
    try {
      await api('POST', '/prices', { store_id: S.grocery.store_id, ingredient_id: +el.dataset.id, price, pack_g: pack, user_id: S.uid });
      closeSheet(true); toast('Thanks! Price updated'); loadCart(); if (S.plan) { const p = await api('GET', '/plan/' + S.plan.id); S.plan = p; }
    } catch (e) { toast(e.message); }
  },

  startWizard: () => { S.draft = defaultDraft(); S.wizStep = 0; S.tab = 'wizard'; $('#overlay').innerHTML = ''; render(); },
  wizNext: () => { readStep(); if (!validateStep()) return; S.wizStep++; render(); $('#screen').scrollTop = 0; },
  wizBack: () => { if (S.wizStep > 0) { readStep(); S.wizStep--; render(); } else if (S.profile) go('home'); },
  units: el => { readStep(); S.units = el.dataset.u; localStorage.setItem('mp_units', S.units); render(); },
  setSex: el => { readStep(); S.draft.sex = el.dataset.v; render(); },
  setDiet: el => { S.draft.diet = DIETS.find(x => x[1] === el.dataset.v)[0]; render(); },
  togCuisine: el => { toggle(S.draft.cuisines, el.dataset.v); render(); },
  togAllergy: el => { toggle(S.draft.allergies, el.dataset.v); readStepSafe(); render(); },
  addDislike: () => { const i = $('#w_dis'), v = i.value.trim(); if (v) { S.draft.dislikes.push(v); render(); } },
  rmDislike: el => { S.draft.dislikes = S.draft.dislikes.filter(x => x !== el.dataset.v); render(); },
  addAllergy: () => { readStep(); const i = $('#w_alg'), v = i.value.trim(); if (v) { S.draft.allergies.push(v); render(); } },
  rmAllergy: el => { readStep(); S.draft.allergies = S.draft.allergies.filter(x => x !== el.dataset.v); render(); },
  useLocation: () => {
    if (!navigator.geolocation) return toast('Location not available');
    readStep(); navigator.geolocation.getCurrentPosition(p => { S.draft.lat = p.coords.latitude; S.draft.lon = p.coords.longitude; toast('Location set'); }, () => toast('Could not get location'), { timeout: 8000 });
  },
  saveProfile: async () => {
    readStep(); if (!validateStep()) return;
    try {
      setBusy(true, 'Saving…');
      const r = await api('PUT', '/profile/' + S.uid, S.draft);
      S.profile = r.profile; S.targets = r.targets; S.draft = null; setBusy(false);
      S.tab = 'home'; await generate(false);
    } catch (e) { setBusy(false); toast(e.message); }
  },
};

const toggle = (arr, v) => { const i = arr.findIndex(x => x.toLowerCase() === v.toLowerCase()); i >= 0 ? arr.splice(i, 1) : arr.push(v); };
function readStepSafe() { try { readStep(); } catch (e) {} }

function validateStep() {
  const d = S.draft;
  if (S.wizStep === 0) {
    if (d.age < 14 || d.age > 100) return toast('Please enter an age between 14 and 100'), false;
    if (d.height_cm < 120 || d.height_cm > 230) return toast('Please check your height'), false;
    if (d.weight_kg < 35 || d.weight_kg > 250) return toast('Please check your weight'), false;
  }
  if (S.wizStep === 2 && !/^\d{5}$/.test(d.zip || '')) return toast('Please enter your 5-digit ZIP code'), false;
  return true;
}

document.addEventListener('click', e => {
  const el = e.target.closest('[data-act]'); if (!el) return;
  if (el.classList.contains('check') || el.closest('.check')) e.stopPropagation();
  const inner = e.target.closest('button[data-act]');
  const target = inner || el;
  const fn = ACTIONS[target.dataset.act]; if (fn) fn(target);
});

(async function init() {
  try {
    const r = await api('GET', '/profile/' + S.uid); S.profile = r.profile; S.targets = r.targets;
    try { S.plan = await api('GET', '/plan/' + S.uid + '/latest'); } catch (e) {}
  } catch (e) {}
  render();
})();

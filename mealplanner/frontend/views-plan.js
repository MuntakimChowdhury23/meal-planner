function emptyPlan() {
  return `<div class="empty"><div class="em">🥗</div><h2>No plan yet</h2><p>Build a 7-day plan that fits your calories, allergies and budget.</p>
  <button class="btn peach" data-act="generate">Generate my week</button></div>`;
}

function mealCard(m, showCheck) {
  const n = m.nutrition;
  return `<div class="card meal-card" data-act="recipe" data-id="${m.recipe_id}" data-slot="${m.slot}">
    <div class="meal"><div class="thumb">${foodEmoji(m.name)}</div>
      <div class="info"><div class="muted small">${cap(m.meal)} · ${m.minutes} min</div><div class="name">${esc(m.name)}</div>
        <div class="muted small">${m.portions} portion${m.portions === 1 ? '' : 's'}</div></div>
      <div class="kcal"><b>${n.kcal}</b><span class="accent small">${m.pct_of_goal}% of goal</span></div></div>
    <div class="row between"><div class="mm"><span>Protein<b>${n.protein}g</b></span><span>Carbs<b>${n.carbs}g</b></span><span>Fat<b>${n.fat}g</b></span></div>
      ${showCheck ? `<button class="check ${m.eaten ? 'on' : ''}" data-act="log" data-slot="${m.slot}" data-eaten="${m.eaten ? 0 : 1}" aria-label="Mark eaten">✓</button>` : ''}</div></div>`;
}

function viewHome() {
  const p = S.plan; if (!p) return `<div class="topbar"><h1>Dashboard</h1></div>${emptyPlan()}`;
  const d = p.days[S.day], t = p.targets, n = d.nutrition;
  const days = p.days.map((x, i) => `<button class="day ${i === S.day ? 'on' : ''}" data-act="day" data-i="${i}">${x.name}</button>`).join('');
  const status = p.status === 'confirmed' ? '<span class="pill good">✓ Plan confirmed</span>' : '<span class="pill">Draft — needs your review</span>';
  return `<div class="topbar"><button class="iconbtn" data-act="tab" data-t="plan" aria-label="Open plan">📅</button><h1>${DAY_LONG[S.day]}</h1><button class="iconbtn" data-act="tab" data-t="profile" aria-label="Profile">👤</button></div>
  <div class="days">${days}</div>
  <div class="card"><div class="gauge">${gaugeSvg(n.kcal, t.kcal)}<div class="center"><div class="muted small">⚡ planned</div><div class="big">${n.kcal} kcal</div><div class="accent small">Goal ${t.kcal} kcal</div></div></div>
    <div class="muted small" style="text-align:center;margin:-6px 0 10px">Logged so far: <b>${d.eaten.kcal} kcal</b></div>
    <div class="macros">${macroBar('Protein', n.protein, t.protein)}${macroBar('Carbs', n.carbs, t.carbs)}${macroBar('Fat', n.fat, t.fat)}</div></div>
  <div class="card row between" data-act="tab" data-t="plan"><div><div class="muted small">${status}</div><div style="margin-top:6px"><b>${moneyRange(p.estimated_cost_low, p.estimated_cost)}</b> <span class="muted small">at ${esc(p.best_store || '—')}</span></div></div>
    <span class="pill good">Saves ~${money(p.estimated_savings)}</span></div>
  ${d.meals.map(m => mealCard(m, true)).join('')}
  <button class="btn ghost" data-act="tab" data-t="plan">See the full week →</button>`;
}

function viewPlan() {
  const p = S.plan; if (!p) return `<div class="topbar"><h1>Meal plan</h1></div>${emptyPlan()}`;
  const w = p.week_nutrition, t = p.targets;
  const days = p.days.map(d => `<div class="h2" style="margin-bottom:6px">${DAY_LONG[d.day]} <span class="muted small" style="font-weight:500">${d.nutrition.kcal} kcal · ${d.nutrition.protein}g protein</span></div>
    <div class="card" style="padding:6px 16px">${d.meals.map(m => `<div class="item"><div class="thumb" style="width:44px;height:44px;font-size:22px" data-act="recipe" data-id="${m.recipe_id}" data-slot="${m.slot}">${foodEmoji(m.name)}</div>
      <div class="grow" data-act="recipe" data-id="${m.recipe_id}" data-slot="${m.slot}"><div class="muted small">${cap(m.meal)} · ${m.nutrition.kcal} kcal</div><div class="nm"><b>${esc(m.name)}</b></div></div>
      <button class="iconbtn" style="width:38px;height:38px;font-size:15px;box-shadow:none;background:${m.locked ? 'var(--peach-soft)' : '#f4f0ea'}" data-act="lock" data-slot="${m.slot}" data-locked="${m.locked ? 0 : 1}" aria-label="Keep this meal when regenerating">${m.locked ? '🔒' : '🔓'}</button>
      <button class="btn sm ghost" style="box-shadow:none;background:#f4f0ea" data-act="swap" data-slot="${m.slot}">Swap</button></div>`).join('')}</div>`).join('');
  return `<div class="topbar"><h1>Your week</h1><span class="pill ${p.status === 'confirmed' ? 'good' : ''}">${p.status === 'confirmed' ? '✓ Confirmed' : 'Draft'}</span></div>
  <div class="card"><div class="row between"><div><div class="muted small">Estimated groceries</div><div class="big" style="font-size:28px;font-weight:800">${moneyRange(p.estimated_cost_low, p.estimated_cost)}</div><div class="muted small">at ${esc(p.best_store || '—')}</div></div>
    <div style="text-align:right"><span class="pill good">~${money(p.estimated_savings)} saved</span><div class="muted small" style="margin-top:6px">Avg ${w.kcal} kcal/day<br>goal ${t.kcal}</div></div></div></div>
  <div class="btns" style="margin-bottom:6px"><button class="btn ghost" data-act="generate" data-keep="1">↻ Regenerate</button><button class="btn peach" data-act="review">${p.status === 'confirmed' ? 'Review again' : 'Review & confirm'}</button></div>
  <p class="muted small" style="margin:8px 4px 0">🔒 Locked meals stay put when you regenerate. Swapping a meal locks it.</p>
  ${days}`;
}

async function showRecipe(id, slot) {
  try {
    const r = await api('GET', '/recipes/' + id);
    const sl = slot !== undefined && slot !== '' && S.plan ? S.plan.days[Math.floor(slot / 3)].meals[slot % 3] : null;
    const k = sl ? sl.portions : 1;
    openSheet(`<div style="text-align:center"><div class="thumb" style="width:96px;height:96px;font-size:50px;margin:0 auto 8px">${foodEmoji(r.name)}</div>
      <div class="h2" style="margin:0">${esc(r.name)}</div><div class="muted small">${esc(r.cuisine)} · ${r.minutes} min · ${r.vegan ? '🌱 vegan' : r.vegetarian ? 'vegetarian' : 'contains meat or fish'}</div></div>
      <div class="card" style="margin-top:14px"><div class="macros" style="margin:0;text-align:center"><div><b>${Math.round(r.per_serving.kcal * k)}</b><div class="muted small">kcal</div></div><div><b>${Math.round(r.per_serving.protein * k)}g</b><div class="muted small">protein</div></div><div><b>${Math.round(r.per_serving.carbs * k)}g</b><div class="muted small">carbs</div></div></div>
      <div class="muted small" style="text-align:center;margin-top:8px">${sl ? `${k} portion${k === 1 ? '' : 's'} in your plan · fat ${Math.round(r.per_serving.fat * k)}g` : 'per serving · fat ' + r.per_serving.fat + 'g'}</div></div>
      <div class="h2">Ingredients <span class="muted small" style="font-weight:500">per ${sl ? 'your portion' : 'serving'}</span></div>
      <div class="card" style="padding:4px 16px">${r.ingredients.map(i => `<div class="item"><div class="grow nm">${esc(i.name)}</div><b>${Math.round(i.grams_per_serving * k)} g</b></div>`).join('')}</div>
      <div class="h2">Method</div><div class="card">${esc(r.instructions)}</div>
      ${r.allergens.length ? `<div class="muted small" style="margin-bottom:12px">Contains: ${r.allergens.map(a => esc(a.replace('_', ' '))).join(', ')}</div>` : ''}
      ${sl ? `<button class="btn peach" data-act="swap" data-slot="${slot}">Swap this meal</button>` : ''}`);
  } catch (e) { toast(e.message); }
}

async function showSwap(slot) {
  setBusy(true, 'Finding cheaper options…');
  try {
    const a = await api('GET', `/plan/${S.plan.id}/alternatives/${slot}`);
    const cur = S.plan.days[Math.floor(slot / 3)].meals[slot % 3];
    setBusy(false);
    openSheet(`<div class="h2" style="margin-top:0">Swap ${esc(cur.name)}</div><p class="muted small" style="margin-top:-6px">Only meals that avoid your allergens and dislikes. Cost change is for the whole week's shopping.</p>
      ${a.alternatives.length ? a.alternatives.map(x => `<div class="card" data-act="doSwap" data-slot="${slot}" data-id="${x.id}"><div class="meal"><div class="thumb">${foodEmoji(x.name)}</div>
        <div class="info"><div class="name">${esc(x.name)}</div><div class="muted small">${x.nutrition.kcal} kcal · ${x.nutrition.protein}g protein · ${x.minutes} min</div></div>
        <span class="pill ${x.week_cost_change < -0.005 ? 'good' : x.week_cost_change > 0.005 ? 'bad' : ''}">${x.week_cost_change > 0 ? '+' : x.week_cost_change < 0 ? '−' : ''}${money(Math.abs(x.week_cost_change))}</span></div></div>`).join('') : '<div class="empty">No other meals fit your filters.</div>'}`);
  } catch (e) { setBusy(false); toast(e.message); }
}

function showReview() {
  const p = S.plan, t = p.targets, w = p.week_nutrition;
  const al = (S.profile.allergies || []).join(', ');
  const checks = [
    ['Allergens', al ? `Every meal avoids: ${esc(al)}` : 'No allergies listed'],
    ['Calories', `Avg ${w.kcal} kcal/day vs goal ${t.kcal}`],
    ['Protein', `Avg ${w.protein} g/day vs target ${t.protein} g`],
    ['Groceries', `${moneyRange(p.estimated_cost_low, p.estimated_cost)} at ${esc(p.best_store || '—')}` + (S.profile.weekly_budget > 0 ? (p.within_budget ? ' · within budget' : ' · over your budget') : '')],
  ];
  openSheet(`<div class="h2" style="margin-top:0">Review your plan</div>
    <div class="card" style="padding:4px 16px">${checks.map(c => `<div class="item"><span class="pill good">✓</span><div class="grow"><b>${c[0]}</b><div class="muted small">${c[1]}</div></div></div>`).join('')}</div>
    <p class="muted small">Prices are estimates from imported and community-reported data. Nutrition values come from your ingredient database. This isn't medical advice.</p>
    <div class="btns"><button class="btn ghost" data-act="closeSheet">Keep editing</button><button class="btn peach" data-act="confirm">Approve plan</button></div>`);
}

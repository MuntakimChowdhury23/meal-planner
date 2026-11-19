const CATS = [['all', '🍽️', 'All'], ['breakfast', '🥣', 'Breakfast'], ['lunch', '🥗', 'Lunch'], ['dinner', '🍝', 'Dinner'], ['vegan', '🌱', 'Vegan']];

function viewRecipes() {
  const cats = CATS.map(c => `<div class="cat ${S.recFilter === c[0] ? 'on' : ''}" data-act="recFilter" data-f="${c[0]}"><div class="ico">${c[1]}</div>${c[2]}</div>`).join('');
  const list = S.recipes.map(r => `<div class="rcard" data-act="recipe" data-id="${r.id}">
    <div class="hd"><div><b style="font-size:16px">${esc(r.name)}</b><div class="muted small">⏱ ${r.minutes} min · ${esc(r.cuisine)}</div></div>${r.vegan ? '<span class="pill good">🌱</span>' : ''}</div>
    <div class="plate">${foodEmoji(r.name)}</div>
    <div class="ft"><span class="pill">${cap(r.meal)}</span><span class="muted small">P ${r.per_serving.protein}g · C ${r.per_serving.carbs}g · F ${r.per_serving.fat}g</span><b>${r.per_serving.kcal} kcal</b></div></div>`).join('');
  return `<div class="topbar"><h1>Recipes</h1><span class="muted small">${S.recipes.length} safe for you</span></div>
  <div class="search"><span>🔍</span><input id="recQ" placeholder="Search recipes or cuisines" value="${esc(S.recQuery)}" autocomplete="off"></div>
  <div class="cats">${cats}</div>
  ${list || '<div class="empty"><div class="em">🔎</div>No recipes match. Try a different filter.</div>'}`;
}

function viewCart() {
  const p = S.plan; if (!p) return `<div class="topbar"><h1>Groceries</h1></div>${emptyPlan()}`;
  const g = S.grocery, st = S.stores;
  if (!g || !st) return `<div class="topbar"><h1>Groceries</h1></div><div class="empty"><span class="spin"></span></div>`;
  const stores = st.stores.map(s => `<div class="card store ${s.store_id === g.store_id ? 'best' : ''}" data-act="pickStore" data-id="${s.store_id}" style="margin-bottom:10px">
    <div class="thumb" style="width:46px;height:46px;font-size:22px">🏪</div>
    <div class="grow" style="flex:1"><b>${esc(s.name)}</b> ${s.best ? '<span class="pill good">Best value</span>' : ''}<div class="muted small">${s.distance_km} km${s.missing_items ? ` · <span style="color:var(--warn)">${s.missing_items} item${s.missing_items > 1 ? 's' : ''} not stocked</span>` : ' · stocks everything'}</div></div>
    <div style="text-align:right"><b style="font-size:18px">${moneyRange(s.total_low, s.total)}</b>${s.extra_vs_best > 0 ? `<div class="muted small">+${money(s.extra_vs_best)}</div>` : ''}</div></div>`).join('');
  const cats = g.categories.map(c => `<div class="cat-h">${esc(c.category)}</div>${c.items.map(i => {
    const key = i.ingredient_id, on = S.checked.has(key);
    return `<div class="item ${on ? 'done' : ''}"><button class="check ${on ? 'on' : ''}" data-act="tick" data-id="${key}" aria-label="Got it">✓</button>
      <div class="grow"><div class="nm"><b>${esc(i.name)}</b></div><div class="muted small">${i.packs} × ${Math.round(i.pack_g)} g pack · need ${i.grams} g${i.used_in_recipes > 1 ? ` · <span class="accent">shared by ${i.used_in_recipes} recipes</span>` : ''}${i.community ? ' · 👥 community price' : ''}${i.missing ? ' · <span style="color:var(--warn)">not stocked here (estimate)</span>' : ''}</div></div>
      <div style="text-align:right"><b>${money(i.cost)}</b><div><button class="muted small" style="text-decoration:underline" data-act="priceEdit" data-id="${key}" data-name="${esc(i.name)}" data-pack="${Math.round(i.pack_g)}" data-price="${i.unit_price}">update price</button></div></div></div>`;
  }).join('')}`).join('');
  return `<div class="topbar"><h1>Groceries</h1><span class="pill">${p.status === 'confirmed' ? '✓ confirmed' : 'draft'}</span></div>
  <div class="card" style="background:var(--dark);color:#fff"><div class="muted small" style="color:#bbb">Shop at one store · ${esc(g.store)}</div><div style="font-size:34px;font-weight:800;margin:4px 0">${moneyRange(g.total_low, g.total)}</div>
    <div class="muted small" style="color:#bbb;margin-bottom:6px">Likely in-store cost up to the Instacart price (Instacart marks items up)</div>
    <div class="small" style="color:#ddd">Food you'll eat this week: ${money(g.cost_of_food_used)} · leftover pantry value: ${money(g.leftover_value)}</div></div>
  <div class="h2">Compare nearby stores</div>${stores}
  <div class="h2">Your list <span class="muted small" style="font-weight:500">combined across all 21 meals</span></div>
  <div class="card" style="padding:4px 16px 10px">${cats}</div>
  <p class="muted small">Totals count whole packages, so recipes that share ingredients lower the bill. Missing items are priced from other stores plus 25% and need a second trip.</p>`;
}

function showPriceEdit(el) {
  const d = el.dataset;
  openSheet(`<div class="h2" style="margin-top:0">Update price: ${esc(d.name)}</div>
    <p class="muted small" style="margin-top:-6px">at ${esc(S.grocery.store)} · helps neighbours plan cheaper. Please only report prices you've seen.</p>
    <div class="grid2"><div class="field"><label>Price ($)</label><input class="inp" id="pe_price" type="number" step="0.01" min="0" inputmode="decimal" value="${esc(d.price)}"></div>
    <div class="field"><label>Package size (g)</label><input class="inp" id="pe_pack" type="number" step="1" min="1" inputmode="numeric" value="${esc(d.pack)}"></div></div>
    <button class="btn peach" data-act="submitPrice" data-id="${esc(d.id)}">Submit price</button>`);
}

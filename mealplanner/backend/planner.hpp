#pragma once
#include <random>
#include "data.hpp"
#include "json.hpp"

static const char* DAY_NAMES[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
static const char* MEALS[3] = {"breakfast", "lunch", "dinner"};
static const double MEAL_SHARE[3] = {0.25, 0.35, 0.40};

struct Profile {
    std::string name = "", sex = "other", activity = "light", diet = "omnivore", zip = "";
    int age = 30; double heightCm = 170, weightKg = 75;
    double lossKgPerWeek = 0.5;
    std::vector<std::string> cuisines, dislikes, allergies;
    double lat = 40.7484, lon = -73.9857, radiusKm = 15, weeklyBudget = 0;
    int people = 1;
};

struct Targets { double bmr = 0, tdee = 0, kcal = 0, protein = 0, carbs = 0, fat = 0; std::vector<std::string> warnings; };

inline Profile profileFrom(const Json& j) {
    Profile p;
    p.name = j.at("name").str();
    p.zip = j.at("zip").str();
    p.sex = lower(j.at("sex").str("other"));
    p.activity = lower(j.at("activity").str("light"));
    p.diet = lower(j.at("diet").str("omnivore"));
    p.age = j.has("age") ? j.at("age").integer() : 30;
    p.heightCm = j.has("height_cm") ? j.at("height_cm").num() : 170;
    p.weightKg = j.has("weight_kg") ? j.at("weight_kg").num() : 75;
    p.lossKgPerWeek = j.has("loss_kg_per_week") ? j.at("loss_kg_per_week").num() : 0.5;
    p.cuisines = j.at("cuisines").strList();
    p.dislikes = j.at("dislikes").strList();
    p.allergies = j.at("allergies").strList();
    if (j.has("lat")) p.lat = j.at("lat").num();
    if (j.has("lon")) p.lon = j.at("lon").num();
    if (j.has("radius_km")) p.radiusKm = std::max(1.0, j.at("radius_km").num());
    if (j.has("weekly_budget")) p.weeklyBudget = j.at("weekly_budget").num();
    if (j.has("people")) p.people = std::max(1, std::min(8, j.at("people").integer(1)));
    return p;
}

inline Json profileJson(const Profile& p) {
    Json j = Json::object();
    j["name"] = p.name; j["zip"] = p.zip; j["sex"] = p.sex; j["activity"] = p.activity; j["diet"] = p.diet;
    j["age"] = p.age; j["height_cm"] = p.heightCm; j["weight_kg"] = p.weightKg;
    j["loss_kg_per_week"] = p.lossKgPerWeek;
    auto arr = [](const std::vector<std::string>& v) { Json a = Json::array(); for (auto& s : v) a.push(s); return a; };
    j["cuisines"] = arr(p.cuisines); j["dislikes"] = arr(p.dislikes); j["allergies"] = arr(p.allergies);
    j["lat"] = p.lat; j["lon"] = p.lon; j["radius_km"] = p.radiusKm; j["weekly_budget"] = p.weeklyBudget; j["people"] = p.people;
    return j;
}

inline Targets calcTargets(const Profile& p) {
    Targets t;
    double base = 10 * p.weightKg + 6.25 * p.heightCm - 5 * p.age;
    t.bmr = p.sex == "male" ? base + 5 : p.sex == "female" ? base - 161 : base - 78;
    double f = 1.375;
    if (p.activity == "sedentary") f = 1.2; else if (p.activity == "moderate") f = 1.55;
    else if (p.activity == "active") f = 1.725; else if (p.activity == "very_active") f = 1.9;
    t.tdee = t.bmr * f;
    double rate = std::max(-1.0, std::min(1.0, p.lossKgPerWeek));
    double adj = std::max(-1000.0, std::min(1000.0, rate * 7700.0 / 7.0));
    double floorK = p.sex == "male" ? 1500 : p.sex == "female" ? 1200 : 1350;
    t.kcal = t.tdee - adj;
    if (t.kcal < floorK) { t.kcal = floorK; t.warnings.push_back("Calories raised to a safe minimum of " + std::to_string((int)floorK) + " kcal; your chosen loss rate was too aggressive."); }
    if (p.age < 18) t.warnings.push_back("Weight-loss targets are not appropriate under 18. Please talk to a doctor or dietitian.");
    if (p.weightKg > 0 && rate > 0.01 * p.weightKg) t.warnings.push_back("A loss rate above 1% of body weight per week is hard to sustain.");
    t.kcal = std::round(t.kcal);
    t.protein = std::round(std::min(1.6 * p.weightKg, 0.30 * t.kcal / 4.0));
    t.fat = std::round(0.27 * t.kcal / 9.0);
    t.carbs = std::round((t.kcal - t.protein * 4 - t.fat * 9) / 4.0);
    t.bmr = std::round(t.bmr); t.tdee = std::round(t.tdee);
    return t;
}

inline Json targetsJson(const Targets& t) {
    Json j = Json::object();
    j["bmr"] = t.bmr; j["tdee"] = t.tdee; j["kcal"] = t.kcal; j["protein"] = t.protein; j["carbs"] = t.carbs; j["fat"] = t.fat;
    Json w = Json::array(); for (auto& s : t.warnings) w.push(s); j["warnings"] = w;
    return j;
}

struct Filter {
    std::set<std::string> tags;
    std::vector<std::string> words;
    std::string diet = "omnivore";
};

inline Filter makeFilter(const Profile& p) {
    Filter f; f.diet = p.diet;
    for (auto& a : p.allergies) {
        std::string t = normalizeAllergen(a);
        if (!t.empty()) f.tags.insert(t); else f.words.push_back(lower(a));
        if (t == "fish" && lower(a) == "seafood") f.tags.insert("shellfish");
    }
    for (auto& d : p.dislikes) f.words.push_back(lower(d));
    return f;
}

inline bool recipeAllowed(const Database& db, const Recipe& r, const Filter& f) {
    for (auto& a : r.allergens) if (f.tags.count(a)) return false;
    if (f.diet == "vegan" && !r.vegan) return false;
    if (f.diet == "vegetarian" && !r.vegetarian) return false;
    if (f.diet == "pescatarian" && !r.pescatarian) return false;
    std::string rn = lower(r.name);
    for (auto& w : f.words) {
        if (w.empty()) continue;
        if (rn.find(w) != std::string::npos) return false;
        for (auto& it : r.items) {
            if (lower(db.ingredients.at(it.ing).name).find(w) != std::string::npos) return false;
        }
    }
    return true;
}

struct Slot { int day = 0, meal = 0, recipe = 0; double portions = 1; bool locked = false, eaten = false; };
struct Plan {
    std::string id, userId, status = "draft";
    std::vector<Slot> slots;
    unsigned seed = 1; double baselineCost = 0; long created = 0;
};

inline double portionsFor(const Recipe& r, int meal, const Targets& t) {
    double target = t.kcal * MEAL_SHARE[meal];
    double k = r.perServing.kcal > 1 ? target / r.perServing.kcal : 1;
    return std::max(0.5, std::min(2.0, std::round(k * 4) / 4.0));
}

using Basket = std::map<int, double>;

inline Basket basketOf(const Database& db, const Plan& pl, int people) {
    Basket b;
    for (auto& s : pl.slots) {
        const Recipe& r = db.recipes.at(s.recipe);
        for (auto& it : r.items) b[it.ing] += it.grams / r.servings * s.portions * people;
    }
    return b;
}

// Instacart marks prices up relative to the shelf, so the "real" in-store cost is estimated to sit between
// INSTACART_LOW_FACTOR x the Instacart price and the Instacart price itself. Totals, ranking, budget and savings
// all use the Instacart price (the high end); the low end is reported alongside for display. Community reports are
// already shelf prices, so they have no markup to remove.
static const double INSTACART_LOW_FACTOR = 0.85;

struct LineCost { int ing; double grams, packs, packG, cost, unitPrice; bool missing, community; int reports; double low = 0; };

static const double MISSING_PENALTY = 5.0;
struct StoreQuote { int store; double distanceKm, total = 0, low = 0, rank = 0; int missing = 0, communityItems = 0; std::vector<LineCost> lines; };

inline StoreQuote quoteStore(const Database& db, const Store& st, const Basket& b, double distKm) {
    StoreQuote q; q.store = st.id; q.distanceKm = distKm;
    for (auto& kv : b) {
        LineCost l{kv.first, kv.second, 0, 0, 0, 0, false, false, 0, 0};
        Price p;
        if (db.effective(st.id, kv.first, p)) {
            l.packG = p.packG; l.packs = std::ceil(kv.second / p.packG - 1e-9); l.unitPrice = p.price;
            l.cost = l.packs * p.price; l.community = p.community; l.reports = p.reports;
            if (p.community) q.communityItems++;
        } else {

            double best = 1e18, pack = 0, price = 0;
            for (auto& s2 : db.stores) {
                Price pp;
                if (s2.first != st.id && db.effective(s2.first, kv.first, pp) && pp.price < best) { best = pp.price; pack = pp.packG; price = pp.price; }
            }
            l.missing = true; q.missing++;
            if (pack > 0) { l.packG = pack; l.packs = std::ceil(kv.second / pack - 1e-9); l.unitPrice = price * 1.25; l.cost = l.packs * l.unitPrice; }
        }
        l.low = l.community ? l.cost : l.cost * INSTACART_LOW_FACTOR;
        q.total += l.cost; q.low += l.low; q.lines.push_back(l);
    }
    q.rank = q.total + MISSING_PENALTY * q.missing;
    return q;
}

inline std::vector<StoreQuote> quoteAll(const Database& db, const Profile& p, const Basket& b) {
    std::vector<std::pair<double, const Store*>> near;
    // Only stores we have prices for. (A store with no scraped/reported prices would be quoted entirely from
    // other stores' prices, which says nothing about that store.) If nothing has prices, fall back to all stores.
    bool anyPriced = false;
    for (auto& kv : db.stores) if (db.hasPrices(kv.first)) anyPriced = true;
    for (auto& kv : db.stores) if (!anyPriced || db.hasPrices(kv.first)) near.push_back({haversineKm(p.lat, p.lon, kv.second.lat, kv.second.lon), &kv.second});
    std::sort(near.begin(), near.end(), [](auto& a, auto& c) { return a.first < c.first; });
    std::vector<StoreQuote> out;
    for (auto& n : near) if (n.first <= p.radiusKm) out.push_back(quoteStore(db, *n.second, b, n.first));
    if (out.empty() && !near.empty()) out.push_back(quoteStore(db, *near[0].second, b, near[0].first));
    std::sort(out.begin(), out.end(), [](auto& a, auto& c) { return a.rank < c.rank; });
    return out;
}

inline double cheapestTotal(const Database& db, const Profile& p, const Basket& b) {
    double best = 1e18;
    bool anyPriced = false;
    for (auto& kv : db.stores) if (db.hasPrices(kv.first)) anyPriced = true;
    for (auto& kv : db.stores) {
        if (anyPriced && !db.hasPrices(kv.first)) continue;
        if (haversineKm(p.lat, p.lon, kv.second.lat, kv.second.lon) > p.radiusKm) continue;
        best = std::min(best, quoteStore(db, kv.second, b, 0).rank);
    }
    if (best > 1e17) for (auto& kv : db.stores) if (!anyPriced || db.hasPrices(kv.first)) best = std::min(best, quoteStore(db, kv.second, b, 0).rank);
    return best;
}

struct Planner {
    const Database& db; const Profile& p; const Targets& t; Filter filter;
    std::vector<int> cand[3];
    std::map<int, double> jitter;

    Planner(const Database& d, const Profile& pr, const Targets& tg, unsigned seed) : db(d), p(pr), t(tg), filter(makeFilter(pr)) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> u(0.0, 1.2);
        for (auto& kv : db.recipes) {
            const Recipe& r = kv.second;
            jitter[r.id] = u(rng);
            if (!recipeAllowed(db, r, filter)) continue;
            for (int m = 0; m < 3; m++) if (r.meal == MEALS[m]) cand[m].push_back(r.id);
        }
    }

    bool feasible(std::string& why) const {
        for (int m = 0; m < 3; m++) if (cand[m].empty()) { why = std::string("No ") + MEALS[m] + " recipes match your allergies, diet and dislikes. Import more recipes or relax a filter."; return false; }
        return true;
    }

    bool prefers(const Recipe& r) const {
        for (auto& c : p.cuisines) if (lower(c) == lower(r.cuisine)) return true;
        return false;
    }

    double objective(const Plan& pl) const {
        double cost = cheapestTotal(db, p, basketOf(db, pl, p.people));
        double pen = 0;
        std::map<int, int> cnt;
        for (auto& s : pl.slots) cnt[s.recipe]++;
        for (auto& kv : cnt) { if (kv.second > 3) pen += 50 * (kv.second - 3); else if (kv.second == 3) pen += 1.0; }
        for (size_t i = 0; i < pl.slots.size(); i++) {
            const Slot& s = pl.slots[i];
            const Recipe& r = db.recipes.at(s.recipe);
            pen += jitter.at(r.id) * 0.5;
            if (!p.cuisines.empty() && prefers(r)) pen -= 0.6;
            if (i >= 3 && pl.slots[i - 3].recipe == s.recipe) pen += 0.8;
        }

        for (int d = 0; d < 7; d++) {
            double prot = 0, kc = 0;
            for (int m = 0; m < 3; m++) { const Slot& s = pl.slots[d * 3 + m]; const Nutrition& n = db.recipes.at(s.recipe).perServing; prot += n.protein * s.portions; kc += n.kcal * s.portions; }
            if (prot < 0.8 * t.protein) pen += (0.8 * t.protein - prot) * 0.05;
            double over = kc - 1.03 * t.kcal, under = 0.92 * t.kcal - kc;
            if (over > 0) pen += over * 0.02;
            if (under > 0) pen += under * 0.01;
        }
        return cost + pen;
    }

    void fillSlot(Slot& s, int recipeId) const {
        s.recipe = recipeId;
        s.portions = portionsFor(db.recipes.at(recipeId), s.meal, t);
    }

    Plan generate(const Plan* keep, unsigned seed) const {
        std::mt19937 rng(seed);
        Plan best; double bestObj = 1e18;
        for (int restart = 0; restart < 5; restart++) {
            Plan pl; pl.seed = seed;
            pl.slots.resize(21);
            for (int i = 0; i < 21; i++) {
                Slot& s = pl.slots[i]; s.day = i / 3; s.meal = i % 3;
                if (keep && keep->slots.size() == 21 && keep->slots[i].locked) { s = keep->slots[i]; continue; }
                const auto& c = cand[s.meal];
                fillSlot(s, c[rng() % c.size()]);
            }
            double cur = objective(pl), temp = 2.0;
            Plan localBest = pl; double localObj = cur;
            for (int it = 0; it < 4000; it++) {
                int i = rng() % 21;
                if (pl.slots[i].locked) continue;
                const auto& c = cand[pl.slots[i].meal];
                Slot old = pl.slots[i];
                fillSlot(pl.slots[i], c[rng() % c.size()]);
                double nxt = objective(pl);
                std::uniform_real_distribution<double> u(0, 1);
                if (nxt <= cur || u(rng) < std::exp((cur - nxt) / temp)) {
                    cur = nxt;
                    if (cur < localObj) { localObj = cur; localBest = pl; }
                } else pl.slots[i] = old;
                temp = std::max(0.02, temp * 0.9988);
            }
            if (localObj < bestObj) { bestObj = localObj; best = localBest; }
        }

        double sum = 0; int n = 0;
        for (int k = 0; k < 120; k++) {
            Plan r = best;
            for (int i = 0; i < 21; i++) if (!r.slots[i].locked) { const auto& c = cand[r.slots[i].meal]; fillSlot(r.slots[i], c[rng() % c.size()]); }
            sum += cheapestTotal(db, p, basketOf(db, r, p.people)); n++;
        }
        best.baselineCost = n ? sum / n : 0;
        return best;
    }
};

inline Json nutritionJson(const Nutrition& n) {
    Json j = Json::object();
    j["kcal"] = std::round(n.kcal); j["protein"] = std::round(n.protein); j["carbs"] = std::round(n.carbs); j["fat"] = std::round(n.fat);
    return j;
}

inline Json recipeJson(const Database& db, const Recipe& r, bool detail) {
    Json j = Json::object();
    j["id"] = r.id; j["name"] = r.name; j["meal"] = r.meal; j["cuisine"] = r.cuisine; j["minutes"] = r.minutes;
    j["servings"] = r.servings; j["per_serving"] = nutritionJson(r.perServing);
    j["vegan"] = r.vegan; j["vegetarian"] = r.vegetarian;
    Json al = Json::array(); for (auto& a : r.allergens) al.push(a); j["allergens"] = al;
    if (detail) {
        j["instructions"] = r.instructions;
        Json items = Json::array();
        for (auto& it : r.items) {
            Json x = Json::object(); const Ingredient& g = db.ingredients.at(it.ing);
            x["ingredient_id"] = it.ing; x["name"] = g.name; x["grams_total"] = std::round(it.grams); x["grams_per_serving"] = std::round(it.grams / r.servings);
            items.push(x);
        }
        j["ingredients"] = items;
    }
    return j;
}

inline Json planJson(const Database& db, const Plan& pl, const Profile& p, const Targets& t) {
    Json j = Json::object();
    j["id"] = pl.id; j["status"] = pl.status; j["created"] = pl.created;
    Json days = Json::array();
    Nutrition week;
    for (int d = 0; d < 7; d++) {
        Json day = Json::object(); day["day"] = d; day["name"] = DAY_NAMES[d];
        Json meals = Json::array(); Nutrition tot; Nutrition eaten;
        for (int m = 0; m < 3; m++) {
            const Slot& s = pl.slots[d * 3 + m];
            const Recipe& r = db.recipes.at(s.recipe);
            Nutrition n = r.perServing.scaled(s.portions);
            tot += n; if (s.eaten) eaten += n;
            Json x = Json::object();
            x["slot"] = d * 3 + m; x["meal"] = MEALS[m]; x["recipe_id"] = r.id; x["name"] = r.name; x["cuisine"] = r.cuisine;
            x["minutes"] = r.minutes; x["portions"] = s.portions; x["locked"] = s.locked; x["eaten"] = s.eaten;
            x["nutrition"] = nutritionJson(n); x["pct_of_goal"] = t.kcal > 0 ? std::round(n.kcal / t.kcal * 100) : 0;
            meals.push(x);
        }
        week += tot;
        day["meals"] = meals; day["nutrition"] = nutritionJson(tot); day["eaten"] = nutritionJson(eaten);
        days.push(day);
    }
    j["days"] = days; j["week_nutrition"] = nutritionJson(week.scaled(1.0 / 7)); j["targets"] = targetsJson(t);
    Basket b = basketOf(db, pl, p.people);
    auto quotes = quoteAll(db, p, b);
    if (!quotes.empty()) {
        j["best_store"] = db.stores.at(quotes[0].store).name;
        j["estimated_cost"] = std::round(quotes[0].total * 100) / 100;
        j["estimated_cost_low"] = std::round(quotes[0].low * 100) / 100;
        j["baseline_cost"] = std::round(pl.baselineCost * 100) / 100;
        j["estimated_savings"] = std::round(std::max(0.0, pl.baselineCost - quotes[0].total) * 100) / 100;
        j["within_budget"] = p.weeklyBudget <= 0 || quotes[0].total <= p.weeklyBudget;
    }
    return j;
}

inline Json groceryJson(const Database& db, const Plan& pl, const Profile& p, int storeId) {
    Basket b = basketOf(db, pl, p.people);
    auto quotes = quoteAll(db, p, b);
    Json j = Json::object();
    if (quotes.empty()) return j;
    const StoreQuote* q = &quotes[0];
    if (storeId > 0) for (auto& x : quotes) if (x.store == storeId) q = &x;
    j["store"] = db.stores.at(q->store).name; j["store_id"] = q->store; j["total"] = std::round(q->total * 100) / 100; j["total_low"] = std::round(q->low * 100) / 100;
    j["missing_items"] = q->missing;

    std::map<int, std::set<int>> used;
    for (auto& s : pl.slots) for (auto& it : db.recipes.at(s.recipe).items) used[it.ing].insert(s.recipe);
    std::map<std::string, Json> cats; double usedCost = 0;
    for (auto& l : q->lines) {
        const Ingredient& g = db.ingredients.at(l.ing);
        Json x = Json::object();
        x["ingredient_id"] = l.ing; x["name"] = g.name; x["grams"] = std::round(l.grams); x["packs"] = l.packs; x["pack_g"] = l.packG;
        x["unit_price"] = std::round(l.unitPrice * 100) / 100; x["cost"] = std::round(l.cost * 100) / 100; x["cost_low"] = std::round(l.low * 100) / 100;
        x["missing"] = l.missing; x["community"] = l.community; x["reports"] = l.reports; x["used_in_recipes"] = (int)used[l.ing].size();
        if (l.packG > 0) usedCost += l.cost * std::min(1.0, l.grams / (l.packs * l.packG));
        if (!cats.count(g.category)) cats[g.category] = Json::array();
        cats[g.category].push(x);
    }
    Json arr = Json::array();
    for (auto& kv : cats) { Json c = Json::object(); c["category"] = kv.first; c["items"] = kv.second; arr.push(c); }
    j["categories"] = arr;
    j["cost_of_food_used"] = std::round(usedCost * 100) / 100;
    j["leftover_value"] = std::round((q->total - usedCost) * 100) / 100;
    return j;
}

inline Json storesJson(const Database& db, const Plan& pl, const Profile& p) {
    Basket b = basketOf(db, pl, p.people);
    auto quotes = quoteAll(db, p, b);
    Json arr = Json::array();
    double bestTotal = quotes.empty() ? 0 : quotes[0].total;
    for (size_t i = 0; i < quotes.size(); i++) {
        auto& q = quotes[i]; const Store& s = db.stores.at(q.store);
        Json x = Json::object();
        x["store_id"] = s.id; x["name"] = s.name; x["address"] = s.address; x["distance_km"] = std::round(q.distanceKm * 10) / 10;
        x["total"] = std::round(q.total * 100) / 100; x["total_low"] = std::round(q.low * 100) / 100; x["missing_items"] = q.missing; x["community_items"] = q.communityItems;
        x["extra_vs_best"] = std::round(std::max(0.0, q.total - bestTotal) * 100) / 100; x["best"] = (i == 0);
        arr.push(x);
    }
    Json j = Json::object(); j["stores"] = arr; return j;
}

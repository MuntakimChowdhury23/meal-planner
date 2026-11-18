#pragma once
#include <random>
#include "data.hpp"

static const char* DAY_NAMES[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
static const char* MEALS[3] = {"breakfast", "lunch", "dinner"};
static const double MEAL_SHARE[3] = {0.25, 0.35, 0.40};

struct Profile {
    std::string name, sex = "other", activity = "light", diet = "omnivore";
    int age = 30; double heightCm = 170, weightKg = 75;
    double lossKgPerWeek = 0.5;
    std::vector<std::string> cuisines, dislikes, allergies;
    bool hasLocation = false; double lat = 0, lon = 0, radiusKm = 15;
    double weeklyBudget = 0;
    int people = 1;
};

struct Targets { double bmr = 0, tdee = 0, kcal = 0, protein = 0, carbs = 0, fat = 0; std::vector<std::string> warnings; };

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

struct Filter {
    std::set<std::string> tags;
    std::vector<std::string> words;
    std::string diet = "omnivore";
};

// Returns the allergen tag used in ingredients.csv, or "" if the text isn't a known allergen
// (unknown entries are matched against ingredient and recipe names instead).
inline std::string normalizeAllergen(const std::string& raw) {
    static const std::map<std::string, std::string> syn = {
        {"peanut", "peanut"}, {"peanuts", "peanut"}, {"groundnut", "peanut"},
        {"tree nut", "tree_nut"}, {"tree_nut", "tree_nut"}, {"tree nuts", "tree_nut"}, {"nuts", "tree_nut"}, {"almond", "tree_nut"}, {"walnut", "tree_nut"}, {"cashew", "tree_nut"}, {"nut", "tree_nut"},
        {"dairy", "dairy"}, {"milk", "dairy"}, {"lactose", "dairy"}, {"cheese", "dairy"},
        {"egg", "egg"}, {"eggs", "egg"},
        {"gluten", "gluten"}, {"wheat", "gluten"}, {"celiac", "gluten"},
        {"soy", "soy"}, {"soya", "soy"}, {"soybean", "soy"},
        {"fish", "fish"}, {"seafood", "fish"}, {"shellfish", "shellfish"}, {"shrimp", "shellfish"}, {"sesame", "sesame"}};
    auto it = syn.find(lower(raw));
    return it == syn.end() ? "" : it->second;
}

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
        for (auto& it : r.items)
            if (lower(db.ingredients.at(it.ing).name).find(w) != std::string::npos) return false;
    }
    return true;
}

struct Slot { int day = 0, meal = 0, recipe = 0; double portions = 1; };
struct Plan { std::vector<Slot> slots; };

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

struct LineCost { int ing; double grams, packs, packG, cost, unitPrice; bool missing; };

static const double MISSING_PENALTY = 5.0;
struct StoreQuote { int store; double distanceKm, total = 0, rank = 0; int missing = 0; std::vector<LineCost> lines; };

inline StoreQuote quoteStore(const Database& db, const Store& st, const Basket& b, double distKm) {
    StoreQuote q; q.store = st.id; q.distanceKm = distKm;
    for (auto& kv : b) {
        LineCost l{kv.first, kv.second, 0, 0, 0, 0, false};
        Price p;
        if (db.priceFor(st.id, kv.first, p)) {
            l.packG = p.packG; l.packs = std::ceil(kv.second / p.packG - 1e-9); l.unitPrice = p.price;
            l.cost = l.packs * p.price;
        } else {
            // Not sold here: estimate from the cheapest other store, plus a 25% markup.
            double best = 1e18, pack = 0, price = 0;
            for (auto& s2 : db.stores) {
                Price pp;
                if (s2.first != st.id && db.priceFor(s2.first, kv.first, pp) && pp.price < best) { best = pp.price; pack = pp.packG; price = pp.price; }
            }
            l.missing = true; q.missing++;
            if (pack > 0) { l.packG = pack; l.packs = std::ceil(kv.second / pack - 1e-9); l.unitPrice = price * 1.25; l.cost = l.packs * l.unitPrice; }
        }
        q.total += l.cost; q.lines.push_back(l);
    }
    q.rank = q.total + MISSING_PENALTY * q.missing;
    return q;
}

// Stores within the user's radius (or all stores if no location was given), cheapest first.
inline std::vector<StoreQuote> quoteAll(const Database& db, const Profile& p, const Basket& b) {
    std::vector<std::pair<double, const Store*>> near;
    for (auto& kv : db.stores)
        near.push_back({p.hasLocation ? haversineKm(p.lat, p.lon, kv.second.lat, kv.second.lon) : 0.0, &kv.second});
    std::sort(near.begin(), near.end(), [](auto& a, auto& c) { return a.first < c.first; });
    std::vector<StoreQuote> out;
    for (auto& n : near) if (!p.hasLocation || n.first <= p.radiusKm) out.push_back(quoteStore(db, *n.second, b, n.first));
    if (out.empty() && !near.empty()) out.push_back(quoteStore(db, *near[0].second, b, near[0].first));
    std::sort(out.begin(), out.end(), [](auto& a, auto& c) { return a.rank < c.rank; });
    return out;
}

inline double cheapestTotal(const Database& db, const Profile& p, const Basket& b) {
    auto q = quoteAll(db, p, b);
    return q.empty() ? 0 : q[0].rank;
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
        for (int m = 0; m < 3; m++) if (cand[m].empty()) { why = std::string("No ") + MEALS[m] + " recipes match your allergies, diet and dislikes. Add more recipes to the CSVs or relax a filter."; return false; }
        return true;
    }

    bool prefers(const Recipe& r) const {
        for (auto& c : p.cuisines) if (lower(c) == lower(r.cuisine)) return true;
        return false;
    }

    // Lower is better: grocery cost plus penalties for repetition and missing nutrition targets.
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

    Plan generate(unsigned seed) const {
        std::mt19937 rng(seed);
        Plan best; double bestObj = 1e18;
        for (int restart = 0; restart < 5; restart++) {
            Plan pl; pl.slots.resize(21);
            for (int i = 0; i < 21; i++) {
                Slot& s = pl.slots[i]; s.day = i / 3; s.meal = i % 3;
                const auto& c = cand[s.meal];
                fillSlot(s, c[rng() % c.size()]);
            }
            double cur = objective(pl), temp = 2.0;
            Plan localBest = pl; double localObj = cur;
            for (int it = 0; it < 4000; it++) {
                int i = rng() % 21;
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
        return best;
    }
};

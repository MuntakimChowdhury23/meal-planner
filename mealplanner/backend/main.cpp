#include <mutex>
#include <random>
#include "http.hpp"
#include "json.hpp"
#include "planner.hpp"

static Database db;
static std::mutex mu;
static std::map<std::string, Profile> profiles;
static std::map<std::string, Plan> plans;
static std::string dataDir = "data";

static Response ok(const Json& j) { return {200, "application/json", j.dump()}; }
static Response fail(int code, const std::string& msg) { Json j = Json::object(); j["error"] = msg; return {code, "application/json", j.dump()}; }

static void saveState() {
    Json root = Json::object(), pj = Json::object(), pl = Json::object();
    for (auto& kv : profiles) pj[kv.first] = profileJson(kv.second);
    for (auto& kv : plans) {
        const Plan& p = kv.second;
        Json x = Json::object(); x["user"] = p.userId; x["status"] = p.status; x["seed"] = (double)p.seed; x["baseline"] = p.baselineCost; x["created"] = (double)p.created;
        Json slots = Json::array();
        for (auto& s : p.slots) { Json y = Json::object(); y["r"] = s.recipe; y["p"] = s.portions; y["l"] = s.locked; y["e"] = s.eaten; slots.push(y); }
        x["slots"] = slots; pl[kv.first] = x;
    }
    root["profiles"] = pj; root["plans"] = pl;
    std::ofstream(dataDir + "/state.json") << root.dump();
}

static void loadState() {
    std::ifstream f(dataDir + "/state.json");
    if (!f) return;
    std::string txt((std::istreambuf_iterator<char>(f)), {});
    try {
        Json root = Json::parse(txt);
        for (auto& kv : root.at("profiles").o) profiles[kv.first] = profileFrom(kv.second);
        for (auto& kv : root.at("plans").o) {
            Plan p; p.id = kv.first; p.userId = kv.second.at("user").str(); p.status = kv.second.at("status").str("draft");
            p.seed = (unsigned)kv.second.at("seed").num(1); p.baselineCost = kv.second.at("baseline").num(); p.created = (long)kv.second.at("created").num();
            int i = 0;
            bool valid = true;
            for (auto& s : kv.second.at("slots").a) {
                Slot sl; sl.day = i / 3; sl.meal = i % 3; sl.recipe = s.at("r").integer(); sl.portions = s.at("p").num(1);
                sl.locked = s.at("l").boolean(); sl.eaten = s.at("e").boolean();
                if (!db.recipes.count(sl.recipe)) valid = false;
                p.slots.push_back(sl); i++;
            }
            if (valid && p.slots.size() == 21) plans[p.id] = p;
        }
    } catch (...) { std::cerr << "warning: could not read state.json, starting fresh\n"; }
}

static std::string newId(const char* prefix) {
    static std::mt19937_64 rng(std::random_device{}());
    char buf[32]; snprintf(buf, sizeof buf, "%s%012llx", prefix, (unsigned long long)(rng() & 0xFFFFFFFFFFFFULL));
    return buf;
}

static Json parseBody(const Request& r) {
    if (r.body.empty()) return Json::object();
    return Json::parse(r.body);
}

static Plan* findPlan(const std::string& id) { auto it = plans.find(id); return it == plans.end() ? nullptr : &it->second; }

struct Ctx { Plan* plan; Profile prof; Targets tg; };
static bool context(const std::string& planId, Ctx& c, Response& err) {
    c.plan = findPlan(planId);
    if (!c.plan) { err = fail(404, "plan not found"); return false; }
    auto it = profiles.find(c.plan->userId);
    if (it == profiles.end()) { err = fail(404, "profile not found"); return false; }
    c.prof = it->second; c.tg = calcTargets(c.prof);
    return true;
}

int main(int argc, char** argv) {
    int port = 8080; std::string web = "frontend";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
        else if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--web" && i + 1 < argc) web = argv[++i];
    }
    if (!db.load(dataDir)) { std::cerr << "Could not load recipes from '" << dataDir << "'. Run tools/make_seed.py or add your CSVs.\n"; return 1; }
    loadState();
    std::cout << "Loaded " << db.recipes.size() << " recipes, " << db.ingredients.size() << " ingredients, " << db.stores.size() << " stores, "
              << db.basePrices.size() << " prices, " << db.submissions.size() << " community reports\n";

    HttpServer s;
    s.staticDir(web);

    s.route("GET", "/api/health", [](const Request&) { Json j = Json::object(); j["ok"] = true; j["recipes"] = db.recipes.size(); return ok(j); });

    s.route("POST", "/api/calc", [](const Request& r) { return ok(targetsJson(calcTargets(profileFrom(parseBody(r))))); });

    s.route("PUT", "/api/profile/:id", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        Profile p = profileFrom(parseBody(r));
        if (p.age < 10 || p.age > 110 || p.heightCm < 100 || p.heightCm > 250 || p.weightKg < 30 || p.weightKg > 300) return fail(400, "Please check height, weight and age values.");
        if ((p.zip.size() != 5 || p.zip.find_first_not_of("0123456789") != std::string::npos)) return fail(400, "Please enter your 5-digit ZIP code.");
        profiles[r.params[0]] = p; saveState();
        Json j = Json::object(); j["profile"] = profileJson(p); j["targets"] = targetsJson(calcTargets(p));
        Json rec = Json::array(); Filter f = makeFilter(p);
        for (auto& a : p.allergies) { std::string t = normalizeAllergen(a); Json x = Json::object(); x["input"] = a; x["matched"] = t.empty() ? "ingredient name" : t; rec.push(x); }
        j["allergy_matches"] = rec;
        return ok(j);
    });
    s.route("GET", "/api/profile/:id", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        auto it = profiles.find(r.params[0]);
        if (it == profiles.end()) return fail(404, "no profile");
        Json j = Json::object(); j["profile"] = profileJson(it->second); j["targets"] = targetsJson(calcTargets(it->second));
        return ok(j);
    });

    s.route("GET", "/api/recipes", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        std::string meal = lower(r.query.count("meal") ? r.query.at("meal") : ""), q = lower(r.query.count("q") ? r.query.at("q") : "");
        std::string diet = lower(r.query.count("diet") ? r.query.at("diet") : "");
        Profile p; bool haveUser = false;
        if (r.query.count("user") && profiles.count(r.query.at("user"))) { p = profiles[r.query.at("user")]; haveUser = true; }
        Filter f = makeFilter(p);
        Json arr = Json::array();
        for (auto& kv : db.recipes) {
            const Recipe& c = kv.second;
            if (!meal.empty() && meal != "all" && c.meal != meal) continue;
            if (!q.empty() && lower(c.name).find(q) == std::string::npos && lower(c.cuisine).find(q) == std::string::npos) continue;
            if (diet == "vegan" && !c.vegan) continue;
            if (diet == "vegetarian" && !c.vegetarian) continue;
            if (haveUser && !recipeAllowed(db, c, f)) continue;
            arr.push(recipeJson(db, c, false));
        }
        Json j = Json::object(); j["recipes"] = arr; return ok(j);
    });
    s.route("GET", "/api/recipes/:id", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        auto it = db.recipes.find(atoi(r.params[0].c_str()));
        if (it == db.recipes.end()) return fail(404, "recipe not found");
        return ok(recipeJson(db, it->second, true));
    });
    s.route("GET", "/api/ingredients", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        std::string q = lower(r.query.count("q") ? r.query.at("q") : "");
        Json arr = Json::array();
        for (auto& kv : db.ingredients) {
            if (!q.empty() && lower(kv.second.name).find(q) == std::string::npos) continue;
            Json x = Json::object(); x["id"] = kv.first; x["name"] = kv.second.name; x["category"] = kv.second.category; arr.push(x);
        }
        Json j = Json::object(); j["ingredients"] = arr; return ok(j);
    });
    s.route("GET", "/api/stores", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        double lat = r.query.count("lat") ? atof(r.query.at("lat").c_str()) : 40.7484, lon = r.query.count("lon") ? atof(r.query.at("lon").c_str()) : -73.9857;
        std::vector<std::pair<double, const Store*>> v;
        for (auto& kv : db.stores) v.push_back({haversineKm(lat, lon, kv.second.lat, kv.second.lon), &kv.second});
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
        Json arr = Json::array();
        for (auto& x : v) { Json o = Json::object(); o["id"] = x.second->id; o["name"] = x.second->name; o["address"] = x.second->address; o["distance_km"] = std::round(x.first * 10) / 10; arr.push(o); }
        Json j = Json::object(); j["stores"] = arr; return ok(j);
    });

    s.route("POST", "/api/plan/generate", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        Json b = parseBody(r);
        std::string uid = b.at("user_id").str();
        auto it = profiles.find(uid);
        if (it == profiles.end()) return fail(400, "Create a profile first.");
        Targets t = calcTargets(it->second);
        unsigned seed = (unsigned)(std::random_device{}());
        Planner pl(db, it->second, t, seed);
        std::string why;
        if (!pl.feasible(why)) return fail(400, why);
        Plan* old = b.has("plan_id") ? findPlan(b.at("plan_id").str()) : nullptr;
        Plan np = pl.generate(old, seed);
        np.userId = uid; np.created = Database::now();
        np.id = old ? old->id : newId("p");
        np.status = "draft";
        plans[np.id] = np; saveState();
        return ok(planJson(db, plans[np.id], it->second, t));
    });
    s.route("GET", "/api/plan/:id", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        return ok(planJson(db, *c.plan, c.prof, c.tg));
    });
    s.route("GET", "/api/plan/:id/latest", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        const Plan* best = nullptr;
        for (auto& kv : plans) if (kv.second.userId == r.params[0] && (!best || kv.second.created >= best->created)) best = &kv.second;
        if (!best) return fail(404, "no plan");
        Ctx c; Response e;
        if (!context(best->id, c, e)) return e;
        return ok(planJson(db, *c.plan, c.prof, c.tg));
    });

    s.route("GET", "/api/plan/:id/alternatives/:slot", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        int slot = atoi(r.params[1].c_str());
        if (slot < 0 || slot > 20) return fail(400, "bad slot");
        Planner pl(db, c.prof, c.tg, c.plan->seed);
        double base = pl.objective(*c.plan);
        double baseCost = cheapestTotal(db, c.prof, basketOf(db, *c.plan, c.prof.people));
        struct Alt { int id; double obj, cost; };
        std::vector<Alt> alts;
        for (int id : pl.cand[slot % 3]) {
            if (id == c.plan->slots[slot].recipe) continue;
            Plan tmp = *c.plan; pl.fillSlot(tmp.slots[slot], id);
            alts.push_back({id, pl.objective(tmp), cheapestTotal(db, c.prof, basketOf(db, tmp, c.prof.people))});
        }
        std::sort(alts.begin(), alts.end(), [](auto& a, auto& b) { return a.obj < b.obj; });
        Json arr = Json::array();
        for (size_t i = 0; i < alts.size() && i < 8; i++) {
            const Recipe& rc = db.recipes.at(alts[i].id);
            Json x = recipeJson(db, rc, false);
            x["portions"] = portionsFor(rc, slot % 3, c.tg);
            x["nutrition"] = nutritionJson(rc.perServing.scaled(portionsFor(rc, slot % 3, c.tg)));
            x["week_cost_change"] = std::round((alts[i].cost - baseCost) * 100) / 100;
            arr.push(x);
        }
        (void)base;
        Json j = Json::object(); j["alternatives"] = arr; return ok(j);
    });
    s.route("POST", "/api/plan/:id/swap", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        Json b = parseBody(r);
        int slot = b.at("slot").integer(-1);
        if (slot < 0 || slot > 20) return fail(400, "bad slot");
        Planner pl(db, c.prof, c.tg, c.plan->seed);
        int rid = b.at("recipe_id").integer(0);
        if (rid == 0) {
            double bestObj = 1e18;
            for (int id : pl.cand[slot % 3]) {
                if (id == c.plan->slots[slot].recipe) continue;
                Plan tmp = *c.plan; pl.fillSlot(tmp.slots[slot], id);
                double o = pl.objective(tmp);
                if (o < bestObj) { bestObj = o; rid = id; }
            }
        }
        if (rid == 0) return fail(400, "No alternative recipe available for this slot.");
        auto& cl = pl.cand[slot % 3];
        if (std::find(cl.begin(), cl.end(), rid) == cl.end()) return fail(400, "That recipe doesn't fit this meal or your allergy/diet settings.");
        pl.fillSlot(c.plan->slots[slot], rid);
        c.plan->slots[slot].locked = true; c.plan->slots[slot].eaten = false;
        c.plan->status = "draft"; saveState();
        return ok(planJson(db, *c.plan, c.prof, c.tg));
    });
    s.route("POST", "/api/plan/:id/lock", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        Json b = parseBody(r); int slot = b.at("slot").integer(-1);
        if (slot < 0 || slot > 20) return fail(400, "bad slot");
        c.plan->slots[slot].locked = b.at("locked").boolean(true); saveState();
        return ok(planJson(db, *c.plan, c.prof, c.tg));
    });
    s.route("POST", "/api/plan/:id/log", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        Json b = parseBody(r); int slot = b.at("slot").integer(-1);
        if (slot < 0 || slot > 20) return fail(400, "bad slot");
        c.plan->slots[slot].eaten = b.at("eaten").boolean(true); saveState();
        return ok(planJson(db, *c.plan, c.prof, c.tg));
    });
    s.route("POST", "/api/plan/:id/confirm", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        c.plan->status = "confirmed"; saveState();
        return ok(planJson(db, *c.plan, c.prof, c.tg));
    });

    s.route("GET", "/api/plan/:id/grocery", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        return ok(groceryJson(db, *c.plan, c.prof, r.query.count("store") ? atoi(r.query.at("store").c_str()) : 0));
    });
    s.route("GET", "/api/plan/:id/stores", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu); Ctx c; Response e;
        if (!context(r.params[0], c, e)) return e;
        return ok(storesJson(db, *c.plan, c.prof));
    });

    s.route("POST", "/api/prices", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        Json b = parseBody(r);
        int st = b.at("store_id").integer(), ing = b.at("ingredient_id").integer();
        double price = b.at("price").num(), pack = b.at("pack_g").num(0);
        if (!db.stores.count(st) || !db.ingredients.count(ing)) return fail(400, "Unknown store or ingredient.");
        if (price <= 0 || price > 500) return fail(400, "Price must be between 0 and 500.");
        Price cur;
        if (pack <= 0) { if (!db.effective(st, ing, cur)) return fail(400, "Please include the package size in grams."); pack = cur.packG; }
        double per100 = price / pack * 100.0, ref = db.medianPer100g(ing);
        if (ref > 0 && (per100 > ref * 4 || per100 < ref * 0.25)) return fail(400, "That price looks far from typical for this item. Please double-check the price and package size.");
        db.appendSubmission({st, ing, price, pack, b.at("user_id").str("anon"), Database::now()});
        Json j = Json::object(); j["ok"] = true; Price e2; if (db.effective(st, ing, e2)) { j["effective_price"] = e2.price; j["reports"] = e2.reports; }
        return ok(j);
    });
    s.route("GET", "/api/prices", [](const Request& r) {
        std::lock_guard<std::mutex> g(mu);
        int ing = r.query.count("ingredient_id") ? atoi(r.query.at("ingredient_id").c_str()) : 0;
        Json arr = Json::array();
        for (auto& st : db.stores) {
            Price p; if (!db.effective(st.first, ing, p)) continue;
            Json x = Json::object(); x["store_id"] = st.first; x["store"] = st.second.name; x["price"] = p.price; x["pack_g"] = p.packG;
            x["community"] = p.community; x["reports"] = p.reports; arr.push(x);
        }
        Json j = Json::object(); j["prices"] = arr; return ok(j);
    });

    s.route("POST", "/api/reload", [](const Request&) {
        std::lock_guard<std::mutex> g(mu);
        bool good = db.load(dataDir);
        Json j = Json::object(); j["ok"] = good; j["recipes"] = db.recipes.size(); j["ingredients"] = db.ingredients.size(); j["stores"] = db.stores.size();
        return good ? ok(j) : fail(400, "reload failed");
    });

    return s.listenOn(port) ? 0 : 1;
}

// Plate & Save: a local meal planner.
// Reads the CSV files in the data folder, asks the user for their details, and calculates a weekly plan
// and grocery list. Nothing is sent over a network.
#include <cstdio>
#include <random>
#include "data.hpp"
#include "planner.hpp"

static std::string dataDir = "data";

static std::string profilePath() { return dataDir + "/profile.txt"; }
static std::string planPath() { return dataDir + "/plan.csv"; }

// ---------- profile storage (plain text, one key=value per line) ----------

static std::string joinList(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) s += (i ? "," : "") + v[i];
    return s;
}
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::vector<std::string> parseList(const std::string& s) {
    std::vector<std::string> out;
    for (auto& x : splitStr(s, ',')) { std::string t = trim(x); if (!t.empty()) out.push_back(t); }
    return out;
}

static void saveProfile(const Profile& p) {
    std::ofstream f(profilePath());
    f << "name=" << p.name << "\nsex=" << p.sex << "\nage=" << p.age << "\nheight_cm=" << p.heightCm << "\nweight_kg=" << p.weightKg
      << "\nactivity=" << p.activity << "\nloss_kg_per_week=" << p.lossKgPerWeek << "\ndiet=" << p.diet
      << "\ncuisines=" << joinList(p.cuisines) << "\ndislikes=" << joinList(p.dislikes) << "\nallergies=" << joinList(p.allergies)
      << "\npeople=" << p.people << "\nweekly_budget=" << p.weeklyBudget << "\nhas_location=" << (p.hasLocation ? 1 : 0)
      << "\nlat=" << p.lat << "\nlon=" << p.lon << "\nradius_km=" << p.radiusKm << "\n";
}

static bool loadProfile(Profile& p) {
    std::ifstream f(profilePath());
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "name") p.name = v; else if (k == "sex") p.sex = v; else if (k == "age") p.age = atoi(v.c_str());
        else if (k == "height_cm") p.heightCm = atof(v.c_str()); else if (k == "weight_kg") p.weightKg = atof(v.c_str());
        else if (k == "activity") p.activity = v; else if (k == "loss_kg_per_week") p.lossKgPerWeek = atof(v.c_str());
        else if (k == "diet") p.diet = v; else if (k == "cuisines") p.cuisines = parseList(v);
        else if (k == "dislikes") p.dislikes = parseList(v); else if (k == "allergies") p.allergies = parseList(v);
        else if (k == "people") p.people = std::max(1, std::min(8, atoi(v.c_str())));
        else if (k == "weekly_budget") p.weeklyBudget = atof(v.c_str());
        else if (k == "has_location") p.hasLocation = atoi(v.c_str()) != 0;
        else if (k == "lat") p.lat = atof(v.c_str()); else if (k == "lon") p.lon = atof(v.c_str());
        else if (k == "radius_km") p.radiusKm = std::max(1.0, atof(v.c_str()));
    }
    return true;
}

// ---------- plan storage ----------

static void savePlan(const Plan& pl) {
    std::ofstream f(planPath());
    f << "slot,recipe_id,portions\n";
    for (size_t i = 0; i < pl.slots.size(); i++) f << i << "," << pl.slots[i].recipe << "," << pl.slots[i].portions << "\n";
}

static bool loadPlan(const Database& db, Plan& pl) {
    auto rows = readCsv(planPath());
    if (rows.size() != 21) return false;
    pl.slots.resize(21);
    for (auto& r : rows) {
        if (r.size() < 3) return false;
        int i = atoi(r[0].c_str());
        if (i < 0 || i > 20 || !db.recipes.count(atoi(r[1].c_str()))) return false;
        pl.slots[i] = {i / 3, i % 3, atoi(r[1].c_str()), atof(r[2].c_str())};
    }
    return true;
}

// ---------- prompts ----------

static std::string ask(const std::string& q, const std::string& def = "") {
    std::cout << q;
    if (!def.empty()) std::cout << " [" << def << "]";
    std::cout << ": " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return def;
    line = trim(line);
    return line.empty() ? def : line;
}
static double askNum(const std::string& q, double def, double lo, double hi) {
    while (true) {
        char buf[32]; snprintf(buf, sizeof buf, "%g", def);
        std::string a = ask(q, buf);
        char* end; double v = strtod(a.c_str(), &end);
        if (*end == 0 && v >= lo && v <= hi) return v;
        std::cout << "  Please enter a number between " << lo << " and " << hi << ".\n";
    }
}
static std::string askChoice(const std::string& q, const std::vector<std::string>& opts, const std::string& def) {
    std::string label = q + " (" + joinList(opts) + ")";
    while (true) {
        std::string a = lower(ask(label, def));
        for (auto& o : opts) if (a == o) return o;
        std::cout << "  Please choose one of: " << joinList(opts) << "\n";
    }
}

// ---------- output ----------

static void printTargets(const Targets& t) {
    printf("Daily target: %.0f kcal  |  protein %.0f g, carbs %.0f g, fat %.0f g   (BMR %.0f, maintenance %.0f)\n", t.kcal, t.protein, t.carbs, t.fat, t.bmr, t.tdee);
    for (auto& w : t.warnings) printf("  ! %s\n", w.c_str());
}

static Nutrition slotNutrition(const Database& db, const Slot& s) { return db.recipes.at(s.recipe).perServing.scaled(s.portions); }

static void printPlan(const Database& db, const Plan& pl, const Profile& p, const Targets& t) {
    printTargets(t);
    Nutrition week;
    for (int d = 0; d < 7; d++) {
        printf("\n%s\n", DAY_NAMES[d]);
        Nutrition tot;
        for (int m = 0; m < 3; m++) {
            const Slot& s = pl.slots[d * 3 + m];
            const Recipe& r = db.recipes.at(s.recipe);
            Nutrition n = slotNutrition(db, s); tot += n;
            printf("  %-9s %-38s x%-4g %4.0f kcal  %2d min  [id %d]\n", MEALS[m], r.name.c_str(), s.portions, n.kcal, r.minutes, r.id);
        }
        week += tot;
        printf("  %-9s %-38s      %4.0f kcal  protein %.0f g, carbs %.0f g, fat %.0f g\n", "total", "", tot.kcal, tot.protein, tot.carbs, tot.fat);
    }
    printf("\nWeekly average: %.0f kcal/day\n", week.kcal / 7);
    auto quotes = quoteAll(db, p, basketOf(db, pl, p.people));
    if (!quotes.empty()) {
        printf("Estimated groceries for %d %s: $%.2f at %s\n", p.people, p.people == 1 ? "person" : "people", quotes[0].total, db.stores.at(quotes[0].store).name.c_str());
        if (p.weeklyBudget > 0) printf("Budget $%.2f: %s\n", p.weeklyBudget, quotes[0].total <= p.weeklyBudget ? "within budget" : "over budget");
    }
    printf("\nRun './mealplanner grocery' for the shopping list, or './mealplanner recipe <id>' for instructions.\n");
}

// ---------- commands ----------

static int cmdSetup() {
    Profile p; bool had = loadProfile(p);
    std::cout << "Profile setup" << (had ? " (press Enter to keep the current value)" : "") << "\n\n";
    p.name = ask("Name", p.name);
    p.sex = askChoice("Sex", {"female", "male", "other"}, p.sex);
    p.age = (int)askNum("Age", p.age, 10, 110);
    p.heightCm = askNum("Height (cm)", p.heightCm, 100, 250);
    p.weightKg = askNum("Weight (kg)", p.weightKg, 30, 300);
    p.activity = askChoice("Activity level", {"sedentary", "light", "moderate", "active", "very_active"}, p.activity);
    p.lossKgPerWeek = askNum("Weight to lose per week in kg (0 to maintain)", p.lossKgPerWeek, 0, 1);
    p.diet = askChoice("Diet", {"omnivore", "pescatarian", "vegetarian", "vegan"}, p.diet);
    p.allergies = parseList(ask("Allergies, comma separated (e.g. peanuts, dairy, gluten, shellfish)", joinList(p.allergies)));
    p.dislikes = parseList(ask("Foods you dislike, comma separated", joinList(p.dislikes)));
    p.cuisines = parseList(ask("Preferred cuisines, comma separated (e.g. Italian, Mexican, Asian)", joinList(p.cuisines)));
    p.people = (int)askNum("People you're cooking for", p.people, 1, 8);
    p.weeklyBudget = askNum("Weekly grocery budget in $ (0 for none)", p.weeklyBudget, 0, 5000);
    std::string loc = lower(ask("Use store distance? Enter your latitude,longitude or 'no'", p.hasLocation ? std::to_string(p.lat) + "," + std::to_string(p.lon) : "no"));
    if (loc == "no" || loc == "n") p.hasLocation = false;
    else {
        auto parts = parseList(loc);
        char *e1 = nullptr, *e2 = nullptr;
        double la = parts.size() == 2 ? strtod(parts[0].c_str(), &e1) : 0, lo = parts.size() == 2 ? strtod(parts[1].c_str(), &e2) : 0;
        if (parts.size() == 2 && !*e1 && !*e2) {
            p.hasLocation = true; p.lat = la; p.lon = lo;
            p.radiusKm = askNum("Maximum distance to a store (km)", p.radiusKm, 1, 500);
        } else { p.hasLocation = false; std::cout << "  Couldn't read that location, so all stores will be considered.\n"; }
    }
    saveProfile(p);

    Database db;
    if (db.load(dataDir)) {
        Filter f = makeFilter(p);
        for (auto& a : p.allergies)
            if (normalizeAllergen(a).empty()) printf("  Note: '%s' isn't a standard allergen, so recipes with it in the name or ingredients are excluded.\n", a.c_str());
        int n = 0; for (auto& kv : db.recipes) if (recipeAllowed(db, kv.second, f)) n++;
        printf("\n%d of %d recipes match your diet, allergies and dislikes.\n", n, (int)db.recipes.size());
    }
    printf("Saved to %s\n", profilePath().c_str());
    printTargets(calcTargets(p));
    std::cout << "\nNext: ./mealplanner plan\n";
    return 0;
}

static bool loadAll(Database& db, Profile& p) {
    if (!db.load(dataDir)) { std::cerr << "Could not load recipes from '" << dataDir << "'. Run tools/make_seed.py or add your CSVs.\n"; return false; }
    if (!loadProfile(p)) { std::cerr << "No profile yet. Run './mealplanner setup' first.\n"; return false; }
    return true;
}

static int cmdPlan() {
    Database db; Profile p;
    if (!loadAll(db, p)) return 1;
    Targets t = calcTargets(p);
    unsigned seed = std::random_device{}();
    Planner pl(db, p, t, seed);
    std::string why;
    if (!pl.feasible(why)) { std::cerr << why << "\n"; return 1; }
    Plan plan = pl.generate(seed);
    savePlan(plan);
    printPlan(db, plan, p, t);
    return 0;
}

static int cmdShow() {
    Database db; Profile p;
    if (!loadAll(db, p)) return 1;
    Plan plan;
    if (!loadPlan(db, plan)) { std::cerr << "No saved plan. Run './mealplanner plan' first.\n"; return 1; }
    printPlan(db, plan, p, calcTargets(p));
    return 0;
}

static int cmdGrocery() {
    Database db; Profile p;
    if (!loadAll(db, p)) return 1;
    Plan plan;
    if (!loadPlan(db, plan)) { std::cerr << "No saved plan. Run './mealplanner plan' first.\n"; return 1; }
    auto quotes = quoteAll(db, p, basketOf(db, plan, p.people));
    if (quotes.empty()) { std::cerr << "No stores found in data/stores.csv.\n"; return 1; }

    printf("Store comparison\n");
    for (size_t i = 0; i < quotes.size(); i++) {
        const Store& s = db.stores.at(quotes[i].store);
        printf("  %s %-42s $%7.2f", i == 0 ? "*" : " ", s.name.c_str(), quotes[i].total);
        if (p.hasLocation) printf("  %.1f km", quotes[i].distanceKm);
        if (quotes[i].missing) printf("  (%d items not sold here, estimated)", quotes[i].missing);
        printf("\n");
    }
    const StoreQuote& q = quotes[0];
    const Store& best = db.stores.at(q.store);
    printf("\nShopping list for %s (%s)\n", best.name.c_str(), best.address.c_str());

    std::map<std::string, std::vector<const LineCost*>> cats;
    for (auto& l : q.lines) cats[db.ingredients.at(l.ing).category].push_back(&l);
    double used = 0;
    for (auto& kv : cats) {
        printf("\n%s\n", kv.first.c_str());
        for (auto* l : kv.second) {
            printf("  %-28s need %5.0f g   buy %2.0f x %4.0f g   $%6.2f%s\n", db.ingredients.at(l->ing).name.c_str(), l->grams, l->packs, l->packG, l->cost, l->missing ? "  (est.)" : "");
            if (l->packG > 0) used += l->cost * std::min(1.0, l->grams / (l->packs * l->packG));
        }
    }
    printf("\nTotal: $%.2f   (food actually used: $%.2f, leftover: $%.2f)\n", q.total, used, q.total - used);
    if (p.weeklyBudget > 0) printf("Budget $%.2f: %s\n", p.weeklyBudget, q.total <= p.weeklyBudget ? "within budget" : "over budget");
    return 0;
}

static int cmdRecipe(const std::string& idArg) {
    Database db;
    if (!db.load(dataDir)) { std::cerr << "Could not load recipes from '" << dataDir << "'.\n"; return 1; }
    auto it = db.recipes.find(atoi(idArg.c_str()));
    if (it == db.recipes.end()) { std::cerr << "Recipe not found.\n"; return 1; }
    const Recipe& r = it->second;
    printf("%s  (%s, %s, %d min, serves %d)\n", r.name.c_str(), r.meal.c_str(), r.cuisine.c_str(), r.minutes, r.servings);
    printf("Per serving: %.0f kcal, protein %.0f g, carbs %.0f g, fat %.0f g\n", r.perServing.kcal, r.perServing.protein, r.perServing.carbs, r.perServing.fat);
    printf("\nIngredients (whole recipe):\n");
    for (auto& i : r.items) printf("  %5.0f g  %s\n", i.grams, db.ingredients.at(i.ing).name.c_str());
    printf("\n%s\n", r.instructions.c_str());
    return 0;
}

static void usage() {
    std::cout << "Plate & Save: local meal planner\n\n"
                 "Usage: mealplanner [--data DIR] <command>\n\n"
                 "  setup        enter or update your details, diet, allergies and preferences\n"
                 "  plan         generate a new 7-day meal plan\n"
                 "  show         show the last plan again\n"
                 "  grocery      shopping list and store price comparison for the last plan\n"
                 "  recipe <id>  ingredients and instructions for a recipe\n";
}

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) dataDir = argv[++i]; else args.push_back(a);
    }
    if (args.empty()) { usage(); return 0; }
    const std::string& c = args[0];
    if (c == "setup") return cmdSetup();
    if (c == "plan") return cmdPlan();
    if (c == "show") return cmdShow();
    if (c == "grocery") return cmdGrocery();
    if (c == "recipe" && args.size() > 1) return cmdRecipe(args[1]);
    usage();
    return 1;
}

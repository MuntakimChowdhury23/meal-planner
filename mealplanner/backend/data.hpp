#pragma once
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using Row = std::vector<std::string>;

inline std::vector<Row> readCsv(const std::string& path) {
    std::ifstream f(path);
    std::vector<Row> rows;
    if (!f) return rows;
    std::string text((std::istreambuf_iterator<char>(f)), {});
    Row row; std::string cell; bool q = false;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (q) {
            if (c == '"') { if (i + 1 < text.size() && text[i + 1] == '"') { cell += '"'; i++; } else q = false; }
            else cell += c;
        } else if (c == '"') q = true;
        else if (c == ',') { row.push_back(cell); cell.clear(); }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') i++;
            row.push_back(cell); cell.clear();
            if (!(row.size() == 1 && row[0].empty())) rows.push_back(row);
            row.clear();
        } else cell += c;
    }
    if (!cell.empty() || !row.empty()) { row.push_back(cell); rows.push_back(row); }
    if (!rows.empty()) rows.erase(rows.begin());
    return rows;
}

inline std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string r = "\"";
    for (char c : s) { if (c == '"') r += '"'; r += c; }
    return r + "\"";
}

inline std::vector<std::string> splitStr(const std::string& s, char d) {
    std::vector<std::string> r; std::string cur;
    for (char c : s) { if (c == d) { if (!cur.empty()) r.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) r.push_back(cur);
    return r;
}

inline std::string lower(std::string s) { for (auto& c : s) c = tolower((unsigned char)c); return s; }

// Maps a user-facing allergen label (what the UI offers: Dairy, Eggs, Gluten, Tree nuts, ...) onto the tag used in
// ingredients.csv (milk, egg, wheat, tree nuts, ...). Returns "" when the label isn't a known allergen, in which
// case callers fall back to matching it against ingredient/recipe names.
inline std::string normalizeAllergen(const std::string& raw) {
    std::string s;
    for (char c : lower(raw)) s += (c == '-' || c == '_') ? ' ' : c;
    size_t b = s.find_first_not_of(' '), e = s.find_last_not_of(' ');
    if (b == std::string::npos) return "";
    s = s.substr(b, e - b + 1);
    static const std::map<std::string, std::string> M = {
        {"dairy", "milk"}, {"milk", "milk"}, {"lactose", "milk"},
        {"egg", "egg"}, {"eggs", "egg"},
        {"gluten", "wheat"}, {"wheat", "wheat"},
        {"soy", "soy"}, {"soya", "soy"}, {"soybean", "soy"}, {"soybeans", "soy"},
        {"fish", "fish"}, {"seafood", "fish"},
        {"shellfish", "shellfish"}, {"crustacean", "shellfish"}, {"crustaceans", "shellfish"},
        {"sesame", "sesame"},
        {"peanut", "peanut"}, {"peanuts", "peanut"},
        {"tree nut", "tree nuts"}, {"tree nuts", "tree nuts"}, {"nuts", "tree nuts"}, {"nut", "tree nuts"},
    };
    auto it = M.find(s);
    return it == M.end() ? "" : it->second;
}

struct Nutrition {
    double kcal = 0, protein = 0, carbs = 0, fat = 0;
    Nutrition& operator+=(const Nutrition& o) { kcal += o.kcal; protein += o.protein; carbs += o.carbs; fat += o.fat; return *this; }
    Nutrition scaled(double k) const { return {kcal * k, protein * k, carbs * k, fat * k}; }
};

struct Ingredient {
    int id = 0; std::string name, category;
    double kcal = 0, protein = 0, carbs = 0, fat = 0;
    std::set<std::string> allergens;
};
struct RecipeItem { int ing; double grams; };
struct Recipe {
    int id = 0; std::string name, meal, cuisine, instructions;
    int minutes = 0, servings = 1;
    std::vector<RecipeItem> items;
    Nutrition perServing;
    std::set<std::string> allergens;
    bool vegan = true, vegetarian = true, pescatarian = true;
};
struct Store { int id = 0; std::string name, address; double lat = 0, lon = 0; };
struct Price { double price = 0, packG = 0; int reports = 0; bool community = false; long updated = 0; };
struct Submission { int store, ing; double price, packG; std::string user; long ts; };

struct Database {
    std::map<int, Ingredient> ingredients;
    std::map<int, Recipe> recipes;
    std::map<int, Store> stores;
    std::map<std::pair<int, int>, Price> basePrices;
    std::vector<Submission> submissions;
    std::string dir;

    static long now() { return (long)time(nullptr); }

    bool load(const std::string& d) {
        dir = d;
        ingredients.clear(); recipes.clear(); stores.clear(); basePrices.clear();
        for (auto& r : readCsv(dir + "/ingredients.csv")) {
            if (r.size() < 8) continue;
            Ingredient i; i.id = atoi(r[0].c_str()); i.name = r[1]; i.category = lower(r[2]);
            i.kcal = atof(r[3].c_str()); i.protein = atof(r[4].c_str()); i.carbs = atof(r[5].c_str()); i.fat = atof(r[6].c_str());
            // Cells hold one or more tags separated by ';' or ',' (e.g. "Wheat,Milk"); "None" means no allergens.
            std::string cell = r[7];
            for (auto& ch : cell) if (ch == ',') ch = ';';
            for (auto& a : splitStr(cell, ';')) {
                size_t b = a.find_first_not_of(' '), e = a.find_last_not_of(' ');
                std::string t = lower(b == std::string::npos ? "" : a.substr(b, e - b + 1));
                if (!t.empty() && t != "none") i.allergens.insert(t);
            }
            ingredients[i.id] = i;
        }
        for (auto& r : readCsv(dir + "/stores.csv")) {
            if (r.size() < 5) continue;
            Store s; s.id = atoi(r[0].c_str()); s.name = r[1]; s.lat = atof(r[2].c_str()); s.lon = atof(r[3].c_str()); s.address = r[4];
            stores[s.id] = s;
        }
        for (auto& r : readCsv(dir + "/prices.csv")) {
            if (r.size() < 4) continue;
            Price p; p.price = atof(r[2].c_str()); p.packG = atof(r[3].c_str());
            if (p.price > 0 && p.packG > 0) basePrices[{atoi(r[0].c_str()), atoi(r[1].c_str())}] = p;
        }
        for (auto& r : readCsv(dir + "/recipes.csv")) {
            if (r.size() < 7) continue;
            Recipe c; c.id = atoi(r[0].c_str()); c.name = r[1]; c.meal = lower(r[2]); c.cuisine = r[3];
            c.minutes = atoi(r[4].c_str()); c.servings = std::max(1, atoi(r[5].c_str())); c.instructions = r[6];
            recipes[c.id] = c;
        }
        for (auto& r : readCsv(dir + "/recipe_ingredients.csv")) {
            if (r.size() < 3) continue;
            auto it = recipes.find(atoi(r[0].c_str()));
            if (it == recipes.end() || !ingredients.count(atoi(r[1].c_str()))) continue;
            it->second.items.push_back({atoi(r[1].c_str()), atof(r[2].c_str())});
        }
        for (auto& kv : recipes) derive(kv.second);
        loadSubmissions();
        return !recipes.empty();
    }

    void derive(Recipe& r) {
        Nutrition total;
        r.allergens.clear(); r.vegan = r.vegetarian = r.pescatarian = true;
        for (auto& it : r.items) {
            const Ingredient& g = ingredients.at(it.ing);
            double k = it.grams / 100.0;
            total += Nutrition{g.kcal * k, g.protein * k, g.carbs * k, g.fat * k};
            for (auto& a : g.allergens) r.allergens.insert(a);
            if (g.category == "meat") r.vegan = r.vegetarian = r.pescatarian = false;
            if (g.category == "fish") r.vegan = r.vegetarian = false;
            if (g.category == "dairy" || g.category == "egg" || g.category == "animal") r.vegan = false;
        }
        r.perServing = total.scaled(1.0 / r.servings);
    }

    void loadSubmissions() {
        submissions.clear();
        for (auto& r : readCsv(dir + "/community_prices.csv")) {
            if (r.size() < 6) continue;
            submissions.push_back({atoi(r[0].c_str()), atoi(r[1].c_str()), atof(r[2].c_str()), atof(r[3].c_str()), r[4], atol(r[5].c_str())});
        }
    }
    void appendSubmission(const Submission& s) {
        submissions.push_back(s);
        std::string path = dir + "/community_prices.csv";
        bool fresh = !std::ifstream(path).good();
        std::ofstream f(path, std::ios::app);
        if (fresh) f << "store_id,ingredient_id,price,pack_g,user,timestamp\n";
        f << s.store << "," << s.ing << "," << s.price << "," << s.packG << "," << csvEscape(s.user) << "," << s.ts << "\n";
    }

    bool effective(int store, int ing, Price& out) const {
        auto b = basePrices.find({store, ing});
        std::vector<const Submission*> recent;
        long cutoff = now() - 45L * 86400;
        for (auto& s : submissions) if (s.store == store && s.ing == ing && s.ts >= cutoff) recent.push_back(&s);
        if (!recent.empty()) {
            std::sort(recent.begin(), recent.end(), [](auto* a, auto* c) { return a->ts > c->ts; });
            if (recent.size() > 5) recent.resize(5);
            std::vector<double> pp;
            for (auto* s : recent) pp.push_back(s->price / (s->packG > 0 ? s->packG : 1) * 100.0);
            std::sort(pp.begin(), pp.end());
            double med = pp[pp.size() / 2];
            double pack = recent[0]->packG > 0 ? recent[0]->packG : (b != basePrices.end() ? b->second.packG : 0);
            if (pack <= 0) return false;
            out.packG = pack; out.price = std::round(med * pack / 100.0 * 100) / 100; out.reports = (int)recent.size();
            out.community = true; out.updated = recent[0]->ts;
            return true;
        }
        if (b == basePrices.end()) return false;
        out = b->second; return true;
    }

    // True if we have any price (catalog or community-reported) for this store.
    bool hasPrices(int store) const {
        for (auto& kv : basePrices) if (kv.first.first == store) return true;
        for (auto& s : submissions) if (s.store == store) return true;
        return false;
    }

    double medianPer100g(int ing) const {
        std::vector<double> v;
        for (auto& kv : basePrices) if (kv.first.second == ing) v.push_back(kv.second.price / kv.second.packG * 100.0);
        if (v.empty()) return 0;
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    }
};

inline double haversineKm(double la1, double lo1, double la2, double lo2) {
    const double R = 6371.0, d2r = M_PI / 180.0;
    double dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
    double a = std::sin(dla / 2) * std::sin(dla / 2) + std::cos(la1 * d2r) * std::cos(la2 * d2r) * std::sin(dlo / 2) * std::sin(dlo / 2);
    return 2 * R * std::asin(std::sqrt(a));
}

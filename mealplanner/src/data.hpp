#pragma once
#include <algorithm>
#include <cmath>
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

inline std::vector<std::string> splitStr(const std::string& s, char d) {
    std::vector<std::string> r; std::string cur;
    for (char c : s) { if (c == d) { if (!cur.empty()) r.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) r.push_back(cur);
    return r;
}

inline std::string lower(std::string s) { for (auto& c : s) c = tolower((unsigned char)c); return s; }

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
struct Price { double price = 0, packG = 0; };

struct Database {
    std::map<int, Ingredient> ingredients;
    std::map<int, Recipe> recipes;
    std::map<int, Store> stores;
    std::map<std::pair<int, int>, Price> basePrices;
    std::string dir;

    bool load(const std::string& d) {
        dir = d;
        ingredients.clear(); recipes.clear(); stores.clear(); basePrices.clear();
        for (auto& r : readCsv(dir + "/ingredients.csv")) {
            if (r.size() < 8) continue;
            Ingredient i; i.id = atoi(r[0].c_str()); i.name = r[1]; i.category = lower(r[2]);
            i.kcal = atof(r[3].c_str()); i.protein = atof(r[4].c_str()); i.carbs = atof(r[5].c_str()); i.fat = atof(r[6].c_str());
            for (auto& a : splitStr(r[7], ';')) i.allergens.insert(lower(a));
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

    bool priceFor(int store, int ing, Price& out) const {
        auto b = basePrices.find({store, ing});
        if (b == basePrices.end()) return false;
        out = b->second; return true;
    }
};

inline double haversineKm(double la1, double lo1, double la2, double lo2) {
    const double R = 6371.0, d2r = M_PI / 180.0;
    double dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
    double a = std::sin(dla / 2) * std::sin(dla / 2) + std::cos(la1 * d2r) * std::cos(la2 * d2r) * std::sin(dlo / 2) * std::sin(dlo / 2);
    return 2 * R * std::asin(std::sqrt(a));
}

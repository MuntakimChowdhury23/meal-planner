# Plate & Save: Meal Planner

A local command-line meal planner. You enter your details and preferences, and it builds a 7-day plan (breakfast, lunch and dinner) from a set of CSV files, then works out a shopping list and which store is cheapest.

Everything runs on your machine. The only data it uses is the CSV files in `data/`, and it makes no network connections or API calls.

## How it works

1. **`setup`** asks for your age, sex, height, weight, activity level, weight goal, diet, allergies, dislikes, preferred cuisines, household size, budget and (optionally) your location, and saves them to `data/profile.txt`.
2. **`plan`** calculates your daily calorie and macro targets (Mifflin–St Jeor), filters the recipes in the CSVs by your diet, allergies and dislikes, and picks 21 meals using a simulated-annealing search. It balances nutrition targets, grocery cost, variety and cuisine preferences, and scales portions to fit each meal's share of your calories. The plan is saved to `data/plan.csv`.
3. **`grocery`** totals the ingredients across the plan, rounds up to pack sizes from `prices.csv`, and compares the cost at each store, listing the cheapest.

## Quick start

Requirements: a C++17 compiler (g++ or clang++). Python 3 is only needed if you want to regenerate the sample data.

```bash
git clone <your-repo-url>
cd mealplanner
./run.sh setup      # builds, then asks for your details
./run.sh plan       # generate a weekly plan
./run.sh grocery    # shopping list and store comparison
```

Or build it yourself:

```bash
g++ -std=c++17 -O2 src/main.cpp -o mealplanner
# or: cmake -B build && cmake --build build
./mealplanner setup
```

Run it from the project root, or pass `--data path/to/data` to point at another folder.

## Commands

| Command | What it does |
|---|---|
| `setup` | Enter or update your profile (press Enter to keep a current value) |
| `plan` | Generate a new 7-day meal plan |
| `show` | Show the last plan again |
| `grocery` | Shopping list and store price comparison for the last plan |
| `recipe <id>` | Ingredients, nutrition and instructions for a recipe (ids are shown in the plan) |

Allergies recognized as standard tags: peanuts, tree nuts, dairy, eggs, gluten, soy, fish, shellfish and sesame. Anything else you list (allergies or dislikes) excludes any recipe with that word in its name or ingredients.

## Data

All data lives in CSV files in `data/`:

| File | Columns |
|---|---|
| `ingredients.csv` | `id, name, category, kcal_100g, protein_100g, carbs_100g, fat_100g, allergens` |
| `recipes.csv` | `id, name, meal, cuisine, minutes, servings, instructions` |
| `recipe_ingredients.csv` | `recipe_id, ingredient_id, grams` |
| `stores.csv` | `id, name, lat, lon, address` |
| `prices.csv` | `store_id, ingredient_id, price, pack_g` |

The bundled data is a starter set (60 recipes, 75 ingredients, 6 stores). **Prices are illustrative, not live**, and the stores are in Greenville, NC. To use your own recipes, ingredients, stores or prices, edit the CSVs, keeping the same columns. `meal` must be `Breakfast`, `Lunch` or `Dinner`, and multiple allergens in one ingredient are separated with `;`.

To regenerate a fully synthetic sample set, delete the CSVs and run `python3 tools/make_seed.py`.

## Project structure

```
mealplanner/
├── src/
│   ├── main.cpp       # Commands, prompts, profile and plan files
│   ├── planner.hpp    # Targets, filtering, optimizer, grocery pricing
│   └── data.hpp       # CSV loading
├── data/              # CSV database
├── tools/make_seed.py # Generates sample data
├── CMakeLists.txt
└── run.sh
```

## Notes

- Your profile and plan are plain files in `data/` (`profile.txt`, `plan.csv`). Delete them to start fresh.
- If you enter a location, only stores within your chosen distance are compared; otherwise all stores in `stores.csv` are.
- Calorie and nutrition figures are general estimates, not medical or dietary advice. Check the allergen data in your CSVs before relying on it.

## License

Add a license of your choice (e.g. MIT) before publishing.

# Meal Planner

A personalized weekly meal planning application that calculates nutritional targets, generates meal plans based on dietary constraints, and compares grocery prices across nearby stores to find the most cost effective one.

## Core Functionality & Calculations

- **Nutritional Calculations:** Uses the Mifflin-St Jeor equation to calculate Basal Metabolic Rate (BMR), then applies an activity multiplier to estimate Total Daily Energy Expenditure (TDEE). Adjusts calorie and macronutrient targets based on weight goals.
- **Weekly Meal Generation:** Generates a 7-day meal plan from a 60 recipe library, filtering by dietary preferences, allergies, disliked ingredients, and cuisine preferences. Dynamically scales portions to align with daily nutritional targets.
- **Grocery Optimization:** Aggregates weekly ingredients, calculates required quantities, and rounds them to available package sizes.
- **Store Price Comparison:** Compares grocery basket costs across nearby stores, ranking results by total price and distance using the Haversine formula.
- **Price Data Collection:** Uses Playwright to retrieve Instacart product prices by intercepting GraphQL responses. Matches products to ingredients, normalizes package weights, and updates the local price dataset.
- **Community Price Reports:** Applies median-based pricing from community submissions within a 45-day window to account for price corrections.

## Technology Stack & Project Structure

| Technology | Implementation |
|---|---|
| C++17 | Backend HTTP server, REST API, nutrition calculations, meal generation, grocery aggregation, and store comparisons |
| JavaScript | Frontend application logic, API communication, and interactive features |
| HTML / CSS | Frontend structure and styling |
| Python 3 | Instacart price scraper, data generation, and scraper testing |
| Playwright | Browser automation and GraphQL response interception |
| CSV | Local storage for recipes, ingredients, stores, and pricing data |

### Project Layout

```text
backend/     C++ server, planner, data layer, JSON and HTTP helpers
frontend/    HTML, CSS, and JavaScript single-page application
data/        CSV datasets for recipes, ingredients, stores, and prices
tools/       Python scraper, data generation, and tests
run.sh       Builds and launches the application
scrape.sh    Refreshes grocery prices and reloads the application
```

## How to Run

**Requirements:** C++ compiler supporting C++17 and Python 3.

Clone the repository and run:

```bash
./run.sh
```

The application builds and starts the backend at:

`http://localhost:8080`

To use a different port:

```bash
./run.sh 3000
```

**Required after first login: scrape Instacart prices**(takes 20-30 mins to gather info and update it)

```bash
pip install playwright
playwright install chromium
python3 tools/test_scraper.py
./scrape.sh
```

The application uses local CSV files for data and persistence. 
Initial setup requires populating the datasets with an AI agent or downloaded csv:
# ingredients.csv (csv downloadable from USDA, No AI needed for this)
id,name,category,kcal_100g,protein_100g,carbs_100g,fat_100g,allergens
1,Chicken Breast,Protein,165,31.0,0.0,3.6,None

# recipes.csv
id,name,meal,cuisine,minutes,servings,instructions


# recipe_ingredients.csv
recipe_id,ingredient_id,grams

# stores.csv
id,name,lat,lon,address
these are preset to id 1 = Publix, id 5 = Food Lion, and id 2 = Harris Teeter for now

# prices.csv(from instacart, No AI needed)
store_id,ingredient_id,price,pack_g,source,product,scraped_at
1,1,6.44,907,estimate,
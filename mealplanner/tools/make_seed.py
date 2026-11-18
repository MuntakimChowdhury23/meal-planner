"""Generates SAMPLE seed CSVs into ../data. Prices/stores are illustrative, not real.
Replace the CSVs with your own data (same column layout) to use your own recipes and prices."""
import csv, hashlib, os

OUT = os.path.join(os.path.dirname(__file__), "..", "data")
os.makedirs(OUT, exist_ok=True)

ING = [
 (1,"Rolled oats","grain",389,17,66,7,"gluten",1000,3.2),
 (2,"Brown rice","grain",365,7,77,3,"",900,2.8),
 (3,"Whole wheat bread","bakery",247,13,41,3,"gluten",680,2.9),
 (4,"Whole wheat pasta","grain",348,14,72,3,"gluten",454,1.9),
 (5,"Quinoa","grain",368,14,64,6,"",450,5.5),
 (6,"Flour tortillas","bakery",310,8,52,8,"gluten",340,3.0),
 (7,"Eggs","egg",143,13,1,10,"egg",600,3.9),
 (8,"Milk","dairy",61,3,5,3,"dairy",1890,3.2),
 (9,"Greek yogurt","dairy",59,10,4,0,"dairy",907,5.2),
 (10,"Cheddar cheese","dairy",403,25,1,33,"dairy",227,3.3),
 (11,"Chicken breast","meat",165,31,0,4,"",900,6.5),
 (12,"Ground turkey","meat",150,19,0,8,"",454,5.2),
 (13,"Salmon fillet","fish",208,20,0,13,"fish",340,8.9),
 (14,"Canned tuna","fish",116,26,0,1,"fish",142,1.3),
 (15,"Black beans (dry)","legume",341,21,63,1,"",454,1.8),
 (16,"Lentils (dry)","legume",353,25,60,1,"",454,1.7),
 (17,"Chickpeas (dry)","legume",364,19,61,6,"",454,1.9),
 (18,"Firm tofu","legume",76,8,2,5,"soy",397,2.5),
 (19,"Peanut butter","nut",588,25,20,50,"peanut",454,3.1),
 (20,"Almonds","nut",579,21,22,50,"tree_nut",227,5.0),
 (21,"Bananas","produce",89,1,23,0,"",900,1.6),
 (22,"Apples","produce",52,0,14,0,"",900,2.9),
 (23,"Spinach","produce",23,3,4,0,"",227,3.0),
 (24,"Broccoli","produce",34,3,7,0,"",450,2.4),
 (25,"Carrots","produce",41,1,10,0,"",907,1.8),
 (26,"Onions","produce",40,1,9,0,"",900,1.9),
 (27,"Garlic","produce",149,6,33,0,"",90,0.9),
 (28,"Tomatoes","produce",18,1,4,0,"",450,2.2),
 (29,"Bell peppers","produce",31,1,6,0,"",300,2.6),
 (30,"Potatoes","produce",77,2,17,0,"",2270,3.8),
 (31,"Sweet potatoes","produce",86,2,20,0,"",900,2.4),
 (32,"Cucumber","produce",15,1,4,0,"",400,1.1),
 (33,"Avocado","produce",160,2,9,15,"",300,2.4),
 (34,"Frozen mixed berries","produce",57,1,14,0,"",340,3.2),
 (35,"Olive oil","oil",884,0,0,100,"",500,7.5),
 (36,"Canned diced tomatoes","pantry",21,1,5,0,"",794,1.9),
 (37,"Soy sauce","pantry",53,8,5,0,"soy;gluten",296,2.6),
 (38,"Honey","animal",304,0,82,0,"",340,4.9),
 (39,"Cabbage","produce",25,1,6,0,"",900,1.9),
 (40,"Zucchini","produce",17,1,3,0,"",400,1.6),
 (41,"Frozen corn","produce",86,3,19,1,"",454,1.6),
 (42,"Coconut milk","pantry",197,2,3,21,"",400,2.1),
 (43,"Curry powder","pantry",325,14,58,14,"",85,2.2),
]

STORES = [
 (1,"FreshMart Market",40.7580,-73.9855,"Midtown (sample)",1.12,[]),
 (2,"ValueGrocer",40.7440,-73.9900,"Flatiron (sample)",0.92,[13]),
 (3,"BulkBarn Foods",40.7300,-74.0000,"West Village (sample)",0.85,[13,33,18,5]),
 (4,"Corner Co Express",40.7500,-73.9800,"Murray Hill (sample)",1.22,[5,15,16,17,18,34,41,42,43,12,13]),
]

def I(n):
    for r in ING:
        if n.lower() in r[1].lower(): return r[0]
    raise KeyError(n)

R = []
def rec(name, meal, cuisine, mins, serv, ings, steps):
    R.append((name, meal, cuisine, mins, serv, {I(k): v for k, v in ings.items()}, steps))

rec("Overnight Oats with Banana","breakfast","American",5,2,{"oats":80,"milk":250,"banana":120,"honey":15},"Stir oats, milk and honey in a jar. Chill overnight. Top with sliced banana.")
rec("Peanut Butter Banana Toast","breakfast","American",5,2,{"bread":120,"peanut":32,"banana":120},"Toast the bread. Spread peanut butter and top with sliced banana.")
rec("Veggie Scramble","breakfast","American",10,2,{"eggs":200,"spinach":60,"tomatoes":100,"onions":40,"olive oil":10,"bread":60},"Saute onion in oil, add tomato and spinach, pour in beaten eggs and stir until set. Serve with toast.")
rec("Berry Yogurt Parfait","breakfast","American",5,2,{"greek":300,"berries":150,"oats":40,"honey":15,"almonds":20},"Layer yogurt, berries, oats and chopped almonds. Drizzle with honey.")
rec("Black Bean Breakfast Burrito","breakfast","Mexican",20,2,{"tortillas":130,"black beans":80,"eggs":100,"bell":80,"onions":30,"cheddar":30},"Simmer beans until tender (or use pre-cooked). Scramble eggs with pepper and onion, fill tortillas with eggs, beans and cheese, and roll.")
rec("Savory Oatmeal with Egg","breakfast","American",12,2,{"oats":80,"eggs":100,"spinach":40,"cheddar":20,"olive oil":5},"Cook oats in water with a pinch of salt. Fry eggs in oil, wilt spinach, and serve over oats with cheese.")
rec("Tofu Scramble","breakfast","American",15,2,{"tofu":300,"spinach":60,"bell":80,"onions":40,"olive oil":10,"bread":60},"Crumble tofu into a hot oiled pan with onion and pepper. Season well, fold in spinach, and serve with toast.")
rec("Apple Cinnamon Oats","breakfast","American",10,2,{"oats":80,"apples":180,"milk":250,"almonds":20},"Simmer oats with milk and diced apple, season with cinnamon, and top with chopped almonds.")
rec("Avocado Egg Toast","breakfast","American",10,2,{"bread":120,"avocado":100,"eggs":100,"tomatoes":60},"Toast bread, mash avocado on top, add a fried egg and sliced tomato.")
rec("Sweet Potato Hash","breakfast","American",25,2,{"sweet potatoes":300,"onions":40,"bell":80,"eggs":100,"olive oil":10},"Dice and pan-fry sweet potato in oil until soft, add onion and pepper, top with fried eggs.")

rec("Chickpea Salad Bowl","lunch","Mediterranean",20,2,{"chickpeas":120,"cucumber":150,"tomatoes":150,"onions":30,"olive oil":15,"spinach":40},"Cook chickpeas until tender (or use pre-cooked). Toss with chopped vegetables, spinach and olive oil.")
rec("Tuna Pasta Salad","lunch","Mediterranean",20,2,{"pasta":160,"tuna":142,"tomatoes":100,"cucumber":100,"olive oil":15},"Boil and cool pasta. Toss with flaked tuna, chopped vegetables and olive oil.")
rec("Lentil Vegetable Soup","lunch","Mediterranean",35,3,{"lentils":160,"carrots":120,"onions":80,"garlic":10,"diced tomatoes":200,"olive oil":10},"Saute onion, carrot and garlic in oil. Add lentils, tomatoes and water. Simmer 25 minutes.")
rec("Chicken Quinoa Bowl","lunch","American",30,2,{"chicken":250,"quinoa":120,"broccoli":200,"olive oil":10},"Cook quinoa. Pan-sear sliced chicken in oil, steam broccoli, and assemble bowls.")
rec("Black Bean Rice Bowl","lunch","Mexican",35,2,{"brown rice":140,"black beans":120,"corn":100,"bell":100,"avocado":80},"Cook rice and beans separately. Warm the corn and peppers. Serve together topped with avocado.")
rec("Turkey Veggie Wrap","lunch","American",20,2,{"tortillas":130,"ground turkey":200,"spinach":60,"tomatoes":100,"cheddar":30},"Brown turkey with seasoning. Fill tortillas with turkey, spinach, tomato and cheese.")
rec("Egg Fried Rice","lunch","Asian",20,2,{"brown rice":160,"eggs":100,"carrots":80,"broccoli":100,"soy sauce":20,"onions":40,"olive oil":10},"Stir-fry onion, carrot and broccoli. Add cooked rice, push aside and scramble eggs. Season with soy sauce.")
rec("Spinach Chickpea Curry","lunch","Indian",30,2,{"chickpeas":120,"coconut":200,"diced tomatoes":200,"onions":80,"garlic":10,"curry":10,"spinach":80},"Saute onion and garlic, add curry powder, tomatoes, coconut milk and chickpeas. Simmer, then stir in spinach.")
rec("Tofu Veggie Stir-fry","lunch","Asian",25,2,{"tofu":300,"broccoli":200,"bell":100,"carrots":80,"brown rice":140,"soy sauce":25,"garlic":8},"Cook rice. Pan-fry cubed tofu until golden, add vegetables and garlic, finish with soy sauce.")
rec("Cheesy Potato Broccoli Bake","lunch","American",40,3,{"potatoes":400,"broccoli":200,"cheddar":60,"milk":100},"Slice potatoes, layer with broccoli, pour over milk, top with cheese and bake at 200C for 30 minutes.")

rec("Baked Salmon with Sweet Potato","dinner","American",35,2,{"salmon":300,"sweet potatoes":300,"broccoli":200,"olive oil":15},"Roast sweet potato wedges at 220C. Add salmon and broccoli tossed in oil for the last 12 minutes.")
rec("Chicken Veggie Stir-fry","dinner","Asian",25,2,{"chicken":300,"brown rice":160,"broccoli":200,"carrots":100,"bell":100,"soy sauce":25,"garlic":8},"Cook rice. Stir-fry sliced chicken, then vegetables and garlic. Finish with soy sauce and serve over rice.")
rec("Turkey Black Bean Chili","dinner","Mexican",45,3,{"ground turkey":300,"black beans":120,"diced tomatoes":400,"onions":100,"bell":100,"garlic":8},"Brown turkey with onion and pepper. Add beans, tomatoes, garlic and spices. Simmer 30 minutes.")
rec("Lentil Bolognese","dinner","Italian",40,3,{"pasta":180,"lentils":140,"diced tomatoes":400,"carrots":100,"onions":80,"garlic":10},"Simmer lentils, carrot, onion, garlic and tomatoes until thick. Serve over boiled pasta.")
rec("Chicken Fajita Bowl","dinner","Mexican",30,2,{"chicken":300,"bell":200,"onions":100,"brown rice":160,"avocado":100},"Sear sliced chicken with pepper and onion and fajita spices. Serve over rice with avocado.")
rec("Coconut Chickpea Curry with Rice","dinner","Indian",35,3,{"chickpeas":160,"coconut":200,"diced tomatoes":200,"spinach":100,"brown rice":160,"onions":80,"curry":12,"garlic":10},"Make a curry base with onion, garlic and curry powder. Add tomatoes, coconut milk, chickpeas and spinach. Serve with rice.")
rec("Tuna Stuffed Potatoes","dinner","American",50,2,{"potatoes":500,"tuna":142,"greek":100,"cheddar":40,"broccoli":150},"Bake potatoes until soft. Mix tuna with yogurt, stuff potatoes, top with cheese, and serve with steamed broccoli.")
rec("Peanut Tofu Noodles","dinner","Asian",25,2,{"pasta":160,"tofu":300,"peanut":40,"soy sauce":25,"cabbage":200,"carrots":80,"garlic":8},"Boil noodles. Pan-fry tofu, add shredded cabbage and carrot. Toss with peanut-soy-garlic sauce.")
rec("Pasta Primavera","dinner","Italian",25,2,{"pasta":180,"zucchini":200,"tomatoes":200,"garlic":8,"olive oil":20,"cheddar":30},"Boil pasta. Saute zucchini, tomato and garlic in oil, toss with pasta and finish with grated cheese.")
rec("Black Bean Quesadillas","dinner","Mexican",25,2,{"tortillas":190,"black beans":100,"cheddar":80,"bell":100,"onions":40},"Fill tortillas with beans, pepper, onion and cheese. Toast in a dry pan until crisp and melted.")
rec("Mediterranean Chicken and Quinoa","dinner","Mediterranean",35,2,{"chicken":300,"quinoa":120,"cucumber":150,"tomatoes":150,"olive oil":15},"Pan-sear seasoned chicken. Cook quinoa. Serve with a cucumber-tomato salad dressed in oil.")
rec("Cabbage Lentil Stew","dinner","American",40,3,{"cabbage":300,"lentils":120,"potatoes":300,"carrots":100,"onions":80,"diced tomatoes":200},"Simmer everything with water and seasoning for 30 minutes until the lentils and potatoes are tender.")

def w(name, header, rows):
    with open(os.path.join(OUT, name), "w", newline="") as f:
        cw = csv.writer(f); cw.writerow(header); cw.writerows(rows)

w("ingredients.csv", ["id","name","category","kcal_100g","protein_100g","carbs_100g","fat_100g","allergens"],
  [r[:8] for r in ING])
w("stores.csv", ["id","name","lat","lon","address"], [s[:5] for s in STORES])

def noise(store, ing):
    h = hashlib.md5(f"{store}-{ing}".encode()).digest()[0] / 255.0
    return 0.93 + 0.14 * h
prices = []
for s in STORES:
    for r in ING:
        if r[0] in s[6]: continue
        p = round(r[9] * s[5] * noise(s[0], r[0]), 2)
        prices.append([s[0], r[0], p, r[8]])
w("prices.csv", ["store_id","ingredient_id","price","pack_g"], prices)

w("recipes.csv", ["id","name","meal","cuisine","minutes","servings","instructions"],
  [[i+1, r[0], r[1], r[2], r[3], r[4], r[6]] for i, r in enumerate(R)])
w("recipe_ingredients.csv", ["recipe_id","ingredient_id","grams"],
  [[i+1, k, v] for i, r in enumerate(R) for k, v in r[5].items()])
print(f"{len(ING)} ingredients, {len(STORES)} stores, {len(prices)} prices, {len(R)} recipes")

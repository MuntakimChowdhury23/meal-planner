"""Offline checks for the Instacart scraper's parsing/matching (no network needed)."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scrape_instacart import parse_grams, extract_products, pick_best, load_ingredients

assert parse_grams("16 oz") == 454
assert parse_grams("4 x 4 oz") == 454
assert parse_grams("2 lb") == 907
assert parse_grams("per lb") == 454
assert parse_grams("32 fl oz") == 946
assert parse_grams("1 gal") is None
assert parse_grams("12 ct", "Eggs") == 600
assert parse_grams("12 ct", "Spaghetti Pasta") is None
assert parse_grams("1.5 kg") == 1500

payload = {"data": {"items": [
    {"name": "Perdue Chicken Breast Boneless", "size": "2.5 lb",
     "price": {"viewSection": {"priceString": "$12.49"}}, "availability": {"available": True}},
    {"name": "Chicken Breast Nuggets", "size": "12 oz",
     "price": {"viewSection": {"priceString": "$5.00"}}},
    {"name": "Chicken Breast Tenders", "size": "1 lb",
     "price": {"viewSection": {"priceString": "$4.00"}}, "availability": {"available": False}},
    {"name": "Dog Food", "size": "10 lb", "price": {"viewSection": {"priceString": "$1.00"}}},
]}}
items = extract_products(payload)
assert len(items) == 3, items  # out-of-stock dropped
ing = {"base": "Chicken Breast"}
item, g = pick_best(items, ing)
assert item["name"].startswith("Chicken Breast Nuggets") or "Perdue" in item["name"]
assert pick_best(items, {"base": "Salmon Fillet"}) is None
q = {i["name"]: i["query"] for i in load_ingredients()}
assert q["Chickpeas (Canned)"] == "Chickpeas Canned" and q["Ground Beef (85/15)"] == "Ground Beef 85/15"
print("all scraper checks passed")

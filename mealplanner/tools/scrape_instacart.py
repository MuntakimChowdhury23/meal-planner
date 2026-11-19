#!/usr/bin/env python3
"""Personal-use Instacart price scraper for the meal planner.

For every (store, ingredient) pair it searches the store's Instacart storefront, picks the
best-matching in-stock product, converts its pack size to grams and merges the result into
data/prices.csv (same columns the C++ backend already reads: store_id,ingredient_id,price,pack_g).

Usage:
    python3 tools/scrape_instacart.py                    # all stores that have a slug
    python3 tools/scrape_instacart.py --stores 1,5       # only stores.csv ids 1 and 5
    python3 tools/scrape_instacart.py --limit 5          # first 5 ingredients (quick test)
    python3 tools/scrape_instacart.py --dry-run          # don't touch prices.csv
    python3 tools/scrape_instacart.py --debug            # save raw GraphQL JSON to tools/debug_json/
    python3 tools/scrape_instacart.py --slug 4=lidl      # supply/override a slug

No Instacart account is needed. The ZIP code comes from the profile you saved in the app
(data/state.json), so prices are for stores near you.

Then reload the running app:  curl -X POST http://localhost:8080/api/reload
"""
import argparse
import csv
import json
import os
import re
import shutil
import sys
import time
from urllib.parse import quote_plus

from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(HERE, "..", "data")
PROFILE_DIR = os.path.join(HERE, ".instacart_profile")  # remembers the ZIP between runs
DEBUG_DIR = os.path.join(HERE, "debug_json")


# stores.csv id -> Instacart storefront slug (instacart.com/store/<slug>/...).
# None = not on Instacart / unknown; skipped unless you pass --slug ID=slug.
STORE_SLUGS = {
    1: "publix",          # Publix
    2: "harris-teeter",   # Harris Teeter
    3: None,              # Trader Joe's (not on Instacart)
    4: None,              # Lidl (set a slug here or via --slug 4=... if it shows up for your ZIP)
    5: "food-lion",       # Food Lion
    6: None,              # Walmart (not on Instacart)
}

# Products priced per piece with no weight in the listing, so we assume a weight (grams each).
EACH_GRAMS = {"eggs": 50, "lemon": 100, "burger buns": 57, "pita bread": 60, "naan bread": 90}

UNIT_G = {
    "g": 1.0, "gram": 1.0, "grams": 1.0, "ml": 1.0,
    "kg": 1000.0, "l": 1000.0, "liter": 1000.0, "litre": 1000.0,
    "fl oz": 29.5735, "floz": 29.5735,
    "oz": 28.3495, "ounce": 28.3495, "ounces": 28.3495,
    "lb": 453.592, "lbs": 453.592, "pound": 453.592, "pounds": 453.592,
}
UNIT_RE = r"(fl\.?\s*oz|oz|ounces?|lbs?|pounds?|grams?|kg|g|ml|liters?|litres?|l)"


# ----------------------------------------------------------------------------- data loading

def load_ingredients():
    out = []
    with open(os.path.join(DATA_DIR, "ingredients.csv"), newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            name = row["name"]
            # "Chickpeas (Canned)" -> search "Chickpeas Canned"; "Ground Beef (85/15)" -> "Ground Beef 85/15"
            query = re.sub(r"[()]", " ", name)
            query = re.sub(r"\s+", " ", query).strip()
            base = re.sub(r"\(.*?\)", "", name).strip()  # used for relevance matching
            out.append({"id": int(row["id"]), "name": name, "query": query, "base": base})
    return out


def load_store_names():
    names = {}
    with open(os.path.join(DATA_DIR, "stores.csv"), newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            names[int(row["id"])] = row["name"]
    return names


def load_existing_prices(path):
    prices = {}
    if os.path.exists(path):
        with open(path, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                prices[(int(row["store_id"]), int(row["ingredient_id"]))] = (
                    float(row["price"]), float(row["pack_g"]))
    return prices


# ----------------------------------------------------------------------------- parsing

def parse_grams(text, ingredient_name=""):
    """'16 oz' -> 454, '4 x 4 oz' -> 454, 'per lb' -> 454, '12 ct' (eggs) -> 600. None if unknown."""
    if not text:
        return None
    s = str(text).lower().strip()

    multi = re.search(r"(\d+)\s*(?:x|×|pk|pack)\s*(\d+(?:\.\d+)?)\s*" + UNIT_RE + r"\b", s)
    if multi:
        count, val, unit = float(multi.group(1)), float(multi.group(2)), multi.group(3)
        val *= count
    else:
        m = re.search(r"(\d+(?:\.\d+)?)\s*" + UNIT_RE + r"\b", s)
        if m:
            val, unit = float(m.group(1)), m.group(2)
        elif re.search(r"\b(per|/)\s*(lb|pound)\b|\bper lb\b|\blb\b", s):
            val, unit = 1.0, "lb"
        else:
            ct = re.search(r"(\d+)\s*(?:ct|count|pk|pack)\b", s)
            each = EACH_GRAMS.get(ingredient_name.lower())
            if ct and each:
                return round(int(ct.group(1)) * each)
            return None

    unit = re.sub(r"[.\s]", "", unit) if "fl" in unit else unit
    unit = {"floz": "floz"}.get(unit, unit)
    factor = UNIT_G.get(unit)
    return round(val * factor) if factor else None


def parse_price(value):
    if value is None:
        return None
    m = re.search(r"(\d+(?:,\d{3})*\.\d{2})", str(value))
    return float(m.group(1).replace(",", "")) if m else None


def extract_products(payload):
    """Walk Instacart GraphQL JSON looking for product-ish nodes (name + price [+ size/availability])."""
    found = []

    def price_from(node):
        p = node.get("price")
        if isinstance(p, dict):
            vs = p.get("viewSection") or {}
            for cand in (vs.get("priceString"), p.get("priceString"), vs.get("priceValueString")):
                v = parse_price(cand)
                if v:
                    return v
        elif p is not None:
            return parse_price(p)
        vs = node.get("viewSection")
        if isinstance(vs, dict):
            return parse_price(vs.get("priceString"))
        return None

    def size_from(node):
        for key in ("size", "packageSize", "sizeString"):
            if isinstance(node.get(key), str) and node[key]:
                return node[key]
        vs = node.get("viewSection")
        if isinstance(vs, dict):
            for key in ("itemString", "sizeString"):
                if isinstance(vs.get(key), str):
                    return vs[key]
        return ""

    def walk(node):
        if isinstance(node, dict):
            name = node.get("name")
            if isinstance(name, str) and ("price" in node or "viewSection" in node):
                price = price_from(node)
                avail = node.get("availability")
                in_stock = not (isinstance(avail, dict) and avail.get("available") is False)
                if price and in_stock:
                    found.append({"name": name, "size": size_from(node), "price": price})
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(payload)
    seen, uniq = set(), []
    for p in found:  # nested duplicates are common
        key = (p["name"], p["size"], p["price"])
        if key not in seen:
            seen.add(key)
            uniq.append(p)
    return uniq


# ----------------------------------------------------------------------------- matching

def _stem(tok):
    return tok[:-2] if tok.endswith("es") and len(tok) > 4 else tok[:-1] if tok.endswith("s") and len(tok) > 3 else tok


def _tokens(s):
    return {_stem(t) for t in re.findall(r"[a-z]+", s.lower()) if t not in {"fresh", "and", "of", "the"}}


def relevance(product_name, ingredient_base):
    want = _tokens(ingredient_base)
    return len(want & _tokens(product_name)) / len(want) if want else 0.0


def pick_best(items, ing, min_score=0.67, consider=12):
    """Among the first `consider` results (Instacart relevance order) that match the ingredient
    name and have a parseable pack size, take the cheapest per gram."""
    cands = []
    for it in items[:consider]:
        if relevance(it["name"], ing["base"]) < min_score:
            continue
        g = parse_grams(it["size"], ing["base"]) or parse_grams(it["name"], ing["base"])
        if g and g > 0:
            cands.append((it["price"] / g, it, g))
    if not cands:
        return None
    _, it, g = min(cands, key=lambda c: c[0])
    return it, g


# ----------------------------------------------------------------------------- scraping

def load_user_zip():
    """ZIP of the app user, from the profile saved in data/state.json (set in the app's setup form)."""
    try:
        with open(os.path.join(DATA_DIR, "state.json"), encoding="utf-8") as f:
            profiles = json.load(f).get("profiles", {})
    except (OSError, ValueError):
        profiles = {}
    for prof in profiles.values():
        z = str(prof.get("zip", "")).strip()
        if re.fullmatch(r"\d{5}", z):
            return z
    sys.exit("No ZIP code found. Open the app, add your ZIP code in your profile, save it, then rerun.")


def set_zip(page, zip_code):
    """Best-effort: type the ZIP into Instacart's location box. Returns True if we managed to submit it."""
    selectors = [
        'input[placeholder*="ZIP" i]', 'input[aria-label*="ZIP" i]', 'input[placeholder*="address" i]',
        'input[name*="zip" i]', 'input[id*="zip" i]',
    ]
    for sel in selectors:
        try:
            box = page.locator(sel).first
            box.wait_for(state="visible", timeout=4000)
            box.fill(zip_code)
            box.press("Enter")
            page.wait_for_timeout(3000)
            return True
        except Exception:
            continue
    return False


def zip_marker_matches(zip_code):
    try:
        with open(os.path.join(PROFILE_DIR, "zip.txt")) as f:
            return f.read().strip() == zip_code
    except OSError:
        return False


def remember_zip(zip_code):
    os.makedirs(PROFILE_DIR, exist_ok=True)
    with open(os.path.join(PROFILE_DIR, "zip.txt"), "w") as f:
        f.write(zip_code)


def scrape(args):
    ingredients = load_ingredients()
    if args.limit:
        ingredients = ingredients[: args.limit]
    names = load_store_names()

    slugs = dict(STORE_SLUGS)
    for pair in args.slug or []:
        sid, slug = pair.split("=", 1)
        slugs[int(sid)] = slug
    wanted = [int(x) for x in args.stores.split(",")] if args.stores else sorted(names)
    stores = [(sid, names.get(sid, str(sid)), slugs.get(sid)) for sid in wanted]
    for sid, name, slug in stores:
        if not slug:
            print(f"[skip] store {sid} {name}: no Instacart slug (use --slug {sid}=<slug>)")
    stores = [s for s in stores if s[2]]
    if not stores:
        sys.exit("No stores to scrape.")

    if args.debug:
        os.makedirs(DEBUG_DIR, exist_ok=True)

    results = {}
    zip_code = load_user_zip()
    print(f"Using your ZIP code: {zip_code}")
    needs_zip = not zip_marker_matches(zip_code)  # first run, or you asked for a different ZIP

    with sync_playwright() as p:
        # Persistent profile: the ZIP and store selection survive between runs.
        ctx = p.chromium.launch_persistent_context(
            PROFILE_DIR, headless=args.headless and not needs_zip,
            viewport={"width": 1280, "height": 800})
        page = ctx.pages[0] if ctx.pages else ctx.new_page()
        captured = []
        raw_idx = [0]

        def on_response(resp):
            if "graphql" not in resp.url or resp.status != 200:
                return
            try:
                data = resp.json()
            except Exception:
                return
            if args.debug:
                raw_idx[0] += 1
                with open(os.path.join(DEBUG_DIR, f"{raw_idx[0]:04d}.json"), "w") as f:
                    json.dump(data, f)
            captured.extend(extract_products(data))

        page.on("response", on_response)

        if needs_zip:
            page.goto("https://www.instacart.com", wait_until="domcontentloaded")
            if set_zip(page, zip_code):
                print(f"Entered ZIP {zip_code}.")
            else:
                print("Couldn't find the ZIP box automatically.")
            input(f"Check the browser shows ZIP {zip_code} (fix it by hand if not; no login needed), "
                  "then press Enter here... ")
            remember_zip(zip_code)

        for sid, sname, slug in stores:
            print(f"\n--- {sname} (id {sid}, /{slug}) ---")
            for ing in ingredients:
                captured.clear()
                url = f"https://www.instacart.com/store/{slug}/s?k={quote_plus(ing['query'])}"
                try:
                    page.goto(url, wait_until="domcontentloaded")
                    page.wait_for_timeout(int(args.wait * 1000))
                except Exception as e:
                    print(f"  [ERR ] {ing['name']}: {e}")
                    continue

                hit = pick_best(list(captured), ing)
                if hit:
                    item, grams = hit
                    results[(sid, ing["id"])] = (item["price"], grams)
                    print(f"  [OK  ] {ing['name']:28} -> {item['name'][:45]} "
                          f"(${item['price']:.2f} / {grams}g)")
                else:
                    print(f"  [SKIP] {ing['name']:28} ({len(captured)} results, none matched / parseable)")
                time.sleep(args.delay)
        ctx.close()

    return results


def write_prices(results, dry_run):
    path = os.path.join(DATA_DIR, "prices.csv")
    merged = load_existing_prices(path)
    merged.update(results)  # scraped rows replace sample rows; everything else is kept
    print(f"\nScraped {len(results)} prices; prices.csv will hold {len(merged)} rows.")
    if dry_run:
        print("--dry-run: nothing written.")
        return
    if os.path.exists(path):
        shutil.copyfile(path, path + ".bak")
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["store_id", "ingredient_id", "price", "pack_g"])
        for (sid, iid), (price, grams) in sorted(merged.items()):
            w.writerow([sid, iid, f"{price:.2f}", int(grams)])
    print(f"Wrote {path} (previous copy at prices.csv.bak). Reload the app: "
          "curl -X POST http://localhost:8080/api/reload")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--stores", help="comma-separated stores.csv ids (default: all with a slug)")
    ap.add_argument("--slug", action="append", help="ID=slug override, repeatable")
    ap.add_argument("--limit", type=int, help="only the first N ingredients")
    ap.add_argument("--headless", action="store_true", help="hide the browser (after the first run)")
    ap.add_argument("--wait", type=float, default=3.0, help="seconds to let results load (default 3)")
    ap.add_argument("--delay", type=float, default=1.5, help="pause between searches (default 1.5)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--debug", action="store_true", help="dump raw GraphQL responses to tools/debug_json/")
    write_args = ap.parse_args()
    results = scrape(write_args)
    write_prices(results, write_args.dry_run)


if __name__ == "__main__":
    main()

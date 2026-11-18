#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
[ -f data/recipes.csv ] || python3 tools/make_seed.py
g++ -std=c++17 -O2 src/main.cpp -o mealplanner
./mealplanner "$@"

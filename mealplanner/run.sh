#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
PORT="${1:-8080}"
[ -f data/recipes.csv ] || python3 tools/make_seed.py
g++ -std=c++17 -O2 -pthread backend/main.cpp -o mealplanner
exec ./mealplanner --port "$PORT"

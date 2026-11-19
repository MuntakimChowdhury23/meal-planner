#!/usr/bin/env bash
# Refresh data/prices.csv from Instacart, then reload the running app (if any).
set -e
cd "$(dirname "$0")"
python3 tools/scrape_instacart.py "$@"
curl -s -X POST "http://localhost:${PORT:-8080}/api/reload" >/dev/null 2>&1 && echo "App reloaded." || echo "(App not running; start it with ./run.sh)"

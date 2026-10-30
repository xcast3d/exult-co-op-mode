#!/bin/sh
# Serves web/out on this machine only: http://127.0.0.1:8642/exult.html
cd "$(dirname "$0")/out" && exec python3 -m http.server 8642 --bind 127.0.0.1

#!/bin/sh
# Serves the simulator locally: http://localhost:8080/
cd "$(dirname "$0")/web"
echo "Datapad simulator on http://localhost:8080/  (Ctrl+C to stop)"
exec python3 -m http.server 8080

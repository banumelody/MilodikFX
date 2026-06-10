#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FRONTEND_DIR="$REPO_ROOT/frontend"
CONFIG="${1:-Debug}"
BUILD=0
NO_SERVER=0

# simple arg parsing
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build) BUILD=1; shift ;;
    --no-server) NO_SERVER=1; shift ;;
    --config) CONFIG="$2"; shift 2 ;;
    *) shift ;;
  esac
done

EXE_PATH="$REPO_ROOT/build/MilodikFX_artefacts/$CONFIG/MilodikFX.exe"

if [[ $BUILD -eq 1 ]]; then
  echo "[run-local-e2e] Building frontend..."
  (cd "$FRONTEND_DIR" && npm ci && npm run build)
  echo "[run-local-e2e] Building native..."
  cmake -S "$REPO_ROOT" -B "$REPO_ROOT/build" -G "Unix Makefiles" || true
  cmake --build "$REPO_ROOT/build" --config $CONFIG --parallel
fi

if [[ $NO_SERVER -eq 1 ]]; then
  echo "Skipping server start per --no-server"
  exit 0
fi

if [[ ! -f "$EXE_PATH" ]]; then
  echo "Executable not found: $EXE_PATH" >&2
  exit 1
fi

echo "Starting MilodikFX exe: $EXE_PATH"
"$EXE_PATH" &
PID=$!

# wait for readiness
READY=0
for i in $(seq 1 60); do
  if curl -sSf -I http://localhost:3000/index.html >/dev/null 2>&1; then
    READY=1; break
  fi
  sleep 1
done

if [[ $READY -eq 0 ]]; then
  echo "Server not ready after timeout" >&2
  kill $PID || true
  exit 2
fi

# smoke checks
if ! curl -sSf http://localhost:3000/api/levels >/dev/null; then
  kill $PID || true
  echo "GET /api/levels failed" >&2
  exit 3
fi

if ! curl -sSf http://localhost:3000/api/parameters/master-volume >/dev/null; then
  kill $PID || true
  echo "GET /api/parameters/master-volume failed" >&2
  exit 3
fi

if ! curl -sSf -X PUT -H "Content-Type: application/json" -d '{"value":0.5}' http://localhost:3000/api/parameters/master-volume >/dev/null; then
  kill $PID || true
  echo "PUT /api/parameters/master-volume failed" >&2
  exit 3
fi

kill $PID || true
echo "Smoke checks passed"
exit 0

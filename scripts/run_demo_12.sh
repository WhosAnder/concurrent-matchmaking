#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p logs results bin

make all >/dev/null

{
  echo "==== DEMO 12 JUGADORES $(date '+%Y-%m-%d %H:%M:%S') ===="
  echo "[UNSYNC] ./bin/unsync_matchmaking 12 2"
  ./bin/unsync_matchmaking 12 2
  echo
  echo "[SYNC] ./bin/sync_matchmaking 12 1"
  ./bin/sync_matchmaking 12 1
} | tee logs/demo_12.log

{
  echo "==== RESUMEN DEMO 12 ===="
  grep -E "\[RACE\]|\[ERROR\]|\[WARN\]|\[GLOBAL\]|\[OK\]|RESUMEN|\[UNSYNC\]|\[SYNC\]" logs/demo_12.log || true
} > results/demo_12_results.txt

echo "[DONE] Logs: logs/demo_12.log"
echo "[DONE] Resultados: results/demo_12_results.txt"

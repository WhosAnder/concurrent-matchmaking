#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p logs results bin

make unsync >/dev/null

{
  echo "==== UNSYNC RUN $(date '+%Y-%m-%d %H:%M:%S') ===="
  echo "Comando: ./bin/unsync_matchmaking 40 4"
  ./bin/unsync_matchmaking 40 4
} | tee logs/unsync.log

{
  echo "==== RESUMEN UNSYNC ===="
  grep -E "\[RACE\]|\[ERROR\]|\[WARN\]|\[GLOBAL\]|\[OK\]|RESUMEN" logs/unsync.log || true
} > results/unsync_results.txt

echo "[DONE] Logs: logs/unsync.log"
echo "[DONE] Resultados: results/unsync_results.txt"

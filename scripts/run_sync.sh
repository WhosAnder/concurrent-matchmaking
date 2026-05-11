#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p logs results bin

make sync >/dev/null

{
  echo "==== SYNC RUN $(date '+%Y-%m-%d %H:%M:%S') ===="
  echo "Comando: ./bin/sync_matchmaking 40 1"
  ./bin/sync_matchmaking 40 1
} | tee logs/sync.log

{
  echo "==== RESUMEN SYNC ===="
  grep -E "\[RACE\]|\[ERROR\]|\[WARN\]|\[GLOBAL\]|\[OK\]|RESUMEN" logs/sync.log || true
} > results/sync_results.txt

echo "[DONE] Logs: logs/sync.log"
echo "[DONE] Resultados: results/sync_results.txt"

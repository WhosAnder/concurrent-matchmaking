#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p logs results bin

make all >/dev/null

{
  echo "==== STRESS RUN $(date '+%Y-%m-%d %H:%M:%S') ===="
  echo "[UNSYNC] ./bin/unsync_matchmaking 40 8"
  ./bin/unsync_matchmaking 40 8
  echo
  echo "[SYNC] ./bin/sync_matchmaking 40 1"
  ./bin/sync_matchmaking 40 1
  echo
  echo "[SYNC-150] ./bin/sync_matchmaking 40 1 (4 lotes)"
  for i in 1 2 3 4; do
    echo "--- lote $i/4 ---"
    ./bin/sync_matchmaking 40 1
  done
} | tee logs/stress.log

{
  echo "==== RESUMEN STRESS ===="
  echo "Meta: 150 jugadores efectivos en sync (4x40=160, >=150)."
  grep -E "\[RACE\]|\[ERROR\]|\[WARN\]|\[GLOBAL\]|\[OK\]|RESUMEN|lote" logs/stress.log || true
} > results/stress_results.txt

echo "[DONE] Logs: logs/stress.log"
echo "[DONE] Resultados: results/stress_results.txt"

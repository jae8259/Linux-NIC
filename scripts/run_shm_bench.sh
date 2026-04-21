#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SOCK_PATH="${SOCK_PATH:-/tmp/fastpath_shm.sock}"
SIZE="${SIZE:-64}"
ITERS="${ITERS:-200000}"
CASE_ID="${CASE_ID:-shm_${SIZE}}"
OUT_PATH="${OUT_PATH:-$ROOT_DIR/results/raw/${CASE_ID}.json}"

mkdir -p "$ROOT_DIR/results/raw"
rm -f "$SOCK_PATH"

SERVER_PID=""
cleanup() {
  set +e
  if [[ -n "${SERVER_PID}" ]]; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
    wait "${SERVER_PID}" >/dev/null 2>&1 || true
  fi
  rm -f "$SOCK_PATH" >/dev/null 2>&1 || true
}
trap cleanup EXIT

"$ROOT_DIR/build/shm/shm_bench" --mode server --sock "$SOCK_PATH" &
SERVER_PID=$!

for _ in $(seq 1 50); do
  if [[ -S "$SOCK_PATH" ]]; then
    break
  fi
  sleep 0.02
done

if [[ ! -S "$SOCK_PATH" ]]; then
  echo "ERROR: shm server socket did not appear: $SOCK_PATH" >&2
  exit 1
fi

"$ROOT_DIR/build/shm/shm_bench" --mode client --sock "$SOCK_PATH" \
  --size "$SIZE" --iters "$ITERS" \
  --out "$OUT_PATH"

cat "$OUT_PATH"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SOCK_PATH="${SOCK_PATH:-/tmp/fastpath_shm.sock}"
SIZE="${SIZE:-64}"
ITERS="${ITERS:-200000}"

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
sleep 0.1

"$ROOT_DIR/build/shm/shm_bench" --mode client --sock "$SOCK_PATH" \
  --size "$SIZE" --iters "$ITERS" \
  --out "$ROOT_DIR/results/raw/shm_${SIZE}.json"

cat "$ROOT_DIR/results/raw/shm_${SIZE}.json"

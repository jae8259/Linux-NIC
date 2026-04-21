#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

NS1="${NS1:-ns1}"
NS2="${NS2:-ns2}"
IP2="${IP2:-10.200.1.2}"
PORT="${PORT:-7777}"
SIZE="${SIZE:-64}"
ITERS="${ITERS:-100000}"
TIMEOUT_MS="${TIMEOUT_MS:-2000}"
CASE_ID="${CASE_ID:-veth_udp_${SIZE}}"
OUT_PATH="${OUT_PATH:-$ROOT_DIR/results/raw/${CASE_ID}.json}"

"$ROOT_DIR/scripts/netns_down.sh" >/dev/null 2>&1 || true
"$ROOT_DIR/scripts/netns_up.sh"

ECHO_PID=""
cleanup() {
  set +e
  if [[ -n "${ECHO_PID}" ]]; then
    kill "${ECHO_PID}" >/dev/null 2>&1 || true
    wait "${ECHO_PID}" >/dev/null 2>&1 || true
  fi
  "$ROOT_DIR/scripts/netns_down.sh" >/dev/null 2>&1 || true
}
trap cleanup EXIT

mkdir -p "$ROOT_DIR/results/raw"

ip netns exec "$NS2" "$ROOT_DIR/build/base/sock_echo" --bind "0.0.0.0" --port "$PORT" &
ECHO_PID=$!
sleep 0.2

ip netns exec "$NS1" "$ROOT_DIR/build/base/sock_bench" \
  --peer "$IP2" --port "$PORT" --size "$SIZE" --iters "$ITERS" \
  --timeout-ms "$TIMEOUT_MS" \
  --out "$OUT_PATH"

cat "$OUT_PATH"

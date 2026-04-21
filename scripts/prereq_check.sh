#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "SKIP: Linux required (uname -s=$(uname -s))." >&2
  exit 0
fi

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "MISSING: $1" >&2
    exit 1
  fi
}

need_cmd ip
need_cmd tc
need_cmd python3

echo "OK: basic prerequisites found."


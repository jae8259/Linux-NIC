#!/usr/bin/env bash
set -euo pipefail

NS1="${NS1:-ns1}"
NS2="${NS2:-ns2}"

ip netns del "$NS1" 2>/dev/null || true
ip netns del "$NS2" 2>/dev/null || true

echo "OK: netns down ($NS1, $NS2)"


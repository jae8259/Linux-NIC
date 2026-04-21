#!/usr/bin/env bash
set -euo pipefail

NS1="${NS1:-ns1}"
NS2="${NS2:-ns2}"
VETH1="${VETH1:-veth-ns1}"
VETH2="${VETH2:-veth-ns2}"
IP1="${IP1:-10.200.1.1/24}"
IP2="${IP2:-10.200.1.2/24}"

ip netns add "$NS1" 2>/dev/null || true
ip netns add "$NS2" 2>/dev/null || true

if ip link show "$VETH1" >/dev/null 2>&1 || ip link show "$VETH2" >/dev/null 2>&1; then
  echo "ERROR: veth already exists ($VETH1/$VETH2). Run netns_down.sh first." >&2
  exit 1
fi

ip link add "$VETH1" type veth peer name "$VETH2"
ip link set "$VETH1" netns "$NS1"
ip link set "$VETH2" netns "$NS2"

ip -n "$NS1" addr add "$IP1" dev "$VETH1"
ip -n "$NS2" addr add "$IP2" dev "$VETH2"

ip -n "$NS1" link set lo up
ip -n "$NS2" link set lo up
ip -n "$NS1" link set "$VETH1" up
ip -n "$NS2" link set "$VETH2" up

echo "OK: netns up ($NS1<$VETH1> <-> $NS2<$VETH2>)"


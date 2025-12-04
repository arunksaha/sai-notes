#!/usr/bin/env bash

# Run verification steps from README section 5.
# Usage: verify.sh -n <port_count> (default: 4)

set -euo pipefail
set -x

PORT_COUNT=4
VLAN_ID=73
HLINE="----------------------------------------"

usage() {
  echo "Usage: $0 [-n port_count]" >&2
  exit 1
}

while getopts ":n:" opt; do
  case "$opt" in
    n)
      PORT_COUNT="$OPTARG"
      ;;
    *)
      usage
      ;;
  esac
done

if ! [[ "$PORT_COUNT" =~ ^[0-9]+$ ]] || [ "$PORT_COUNT" -lt 1 ]; then
  echo "Port count must be a positive integer" >&2
  exit 1
fi

VLAN_PORTS=$((PORT_COUNT > 1 ? PORT_COUNT - 1 : 1))

echo "5.1 Verify VLAN membership"

bridge vlan show
echo

echo "$HLINE"
echo

echo "5.2.1 VLAN-internal pings (if at least 2 VLAN members)"

if [ "$VLAN_PORTS" -ge 2 ]; then
  sudo ip netns exec h0 ping -c 2 10.${VLAN_ID}.0.2
  echo

  if [ "$VLAN_PORTS" -ge 3 ]; then
    sudo ip netns exec h1 ping -c 2 10.${VLAN_ID}.0.3
    echo
  fi
else
  echo "Skipping VLAN-internal pings (need at least 2 VLAN hosts)"
  echo
fi

echo "$HLINE"
echo

echo "5.2.2 Cross-VLAN ping (requires a non-VLAN port)"

if [ "$PORT_COUNT" -gt "$VLAN_PORTS" ]; then
  last_host=$((PORT_COUNT - 1))
  dest_ip="10.${VLAN_ID}.0.$((last_host + 1))"
  sudo ip netns exec h0 ping -c 2 "$dest_ip" || true
  echo
else
  echo "Skipping cross-VLAN ping (no non-VLAN host)"
  echo
fi

echo "$HLINE"
echo

echo "5.3 UDP verification with socat (requires 2 VLAN hosts)"

if [ "$VLAN_PORTS" -ge 2 ]; then
  sudo ip netns exec h0 socat -v udp-recv:5000 - &
  listener_pid=$!
  sleep 1
  echo

  echo "hello from h1" | sudo ip netns exec h1 socat -v - udp-sendto:10.${VLAN_ID}.0.1:5000
  sleep 1
  echo

  kill "$listener_pid" 2>/dev/null || true
  echo
else
  echo "Skipping UDP verification (need at least 2 VLAN hosts)"
  echo
fi

echo "$HLINE"
echo

echo "5.4 Inspect MAC learning"

bridge fdb show | grep " master br0" || true
echo

echo "$HLINE"
echo

echo "5.5 Optional: packet sniffing (not run automatically)"
echo "Try: sudo ip netns exec h0 tcpdump -i veth0p -nne"
echo

echo "Verification steps completed."
echo

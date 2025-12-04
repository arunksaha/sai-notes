#!/usr/bin/env bash

# Cleanup bridge, veth interfaces, and namespaces created by the VLAN demo.
# Usage: cleanup.sh -n <port_count>  (e.g., cleanup.sh -n 4)

set -u

PORT_COUNT=4

usage() {
  echo "Usage: $0 -n <port_count>" >&2
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

if ! [[ "$PORT_COUNT" =~ ^[0-9]+$ ]] || [ "$PORT_COUNT" -lt 0 ]; then
  echo "Port count must be a non-negative integer" >&2
  exit 1
fi

# Remove bridge first so member interfaces are released.
sudo ip link del br0 2>/dev/null || true

for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip link del "veth$i" 2>/dev/null || true
  sudo ip netns del "h$i" 2>/dev/null || true
done

echo "Cleanup complete for $PORT_COUNT port(s)."

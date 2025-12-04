#!/usr/bin/env bash

# Configure VLAN-aware bridge lab environment as described in README section 4.
# Usage: configure.sh -n <port_count>  (default: 4)

set -euo pipefail

PORT_COUNT=4
VLAN_ID=73

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

# 4.1 Create network namespaces
printf "Creating %d network namespaces (h0..h%d)\n" "$PORT_COUNT" "$((PORT_COUNT - 1))"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip netns add "h$i"
done

# 4.2 Create veth pairs
printf "Creating %d veth pairs (vethX <-> vethXp)\n" "$PORT_COUNT"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip link add "veth$i" type veth peer name "veth${i}p"
done

# 4.3 Move host interfaces into namespaces
printf "Moving vethXp interfaces into namespaces\n"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip link set "veth${i}p" netns "h$i"
done

# 4.4 Bring up loopback interfaces
printf "Bringing up loopback interfaces inside namespaces\n"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip netns exec "h$i" ip link set lo up
done

# 4.5 Assign IP addresses to virtual host veth*p interfaces
printf "Assigning IPs in 10.%d.0.0/24 to vethXp inside namespaces\n" "$VLAN_ID"
for ((i=0; i<PORT_COUNT; i++)); do
  host_ip="10.${VLAN_ID}.0.$((i + 1))"
  sudo ip netns exec "h$i" ip addr add "${host_ip}/24" dev "veth${i}p"
done

# 4.6 Bring host interfaces up
printf "Bringing up vethXp interfaces inside namespaces\n"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip netns exec "h$i" ip link set "veth${i}p" up
done
echo

# Log namespace, interface, IP, and MAC for each host
printf "Logging namespace, interface, IP, MAC for hosts\n"
for ((i=0; i<PORT_COUNT; i++)); do
  ns="h$i"
  iface="veth${i}p"
  ipaddr="10.${VLAN_ID}.0.$((i + 1))"
  mac=$(sudo ip netns exec "$ns" ip -o link show "$iface" | awk '{for(i=1;i<=NF;i++) if($i=="link/ether") {print $(i+1); exit}}')
  echo "$ns,$iface,$ipaddr,$mac"
done
echo

# 4.7 Create Linux bridge
sudo ip link add br0 type bridge

# 4.8 Enable VLAN filtering
sudo ip link set br0 type bridge vlan_filtering 1

# 4.9 Attach switch-side veth interfaces
printf "Attaching vethX interfaces to br0\n"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip link set "veth$i" master br0
done

# 4.10 Bring up ports and bridge
printf "Bringing up vethX interfaces and br0\n"
for ((i=0; i<PORT_COUNT; i++)); do
  sudo ip link set "veth$i" up
done

sudo ip link set br0 up

# 4.11 Configure VLAN on access ports (all but last port, if more than one)
VLAN_PORTS=$((PORT_COUNT > 1 ? PORT_COUNT - 1 : 1))
printf "Configuring VLAN %d on %d access port(s) (veth0..veth%d)\n" "$VLAN_ID" "$VLAN_PORTS" "$((VLAN_PORTS - 1))"
for ((i=0; i<VLAN_PORTS; i++)); do
  sudo bridge vlan add dev "veth$i" vid "$VLAN_ID" pvid untagged
done

echo "Configured $PORT_COUNT port(s) with VLAN ${VLAN_ID} on first $VLAN_PORTS port(s)."

# VLAN-Aware Virtual Switch Using Linux Bridge

This document builds a VLAN-aware virtual switch on Linux using:

- Linux network namespaces  
- `veth` virtual Ethernet interfaces  
- A Linux bridge (`br0`)  
- VLAN filtering (`vlan_filtering=1`)  
- VLAN 73 configured on three ports  
- Packet-level verification using `ping` and `socat`

---

# 🧠 1. Architecture Overview

We simulate a switch with 4 ports using a Linux bridge. Each port is connected to a Linux network namespace (host). VLAN 73 spans the first three ports.

```
                     Linux Bridge (br0)
                 vlan_filtering = 1  (VLAN-aware)
              ---------------------------------------
            veth0      veth1      veth2      veth3
              |          |          |          |
            (h0)       (h1)       (h2)       (h3)
            VLAN73    VLAN73    VLAN73      no VLAN
            10.73.0.1 10.73.0.2 10.73.0.3  10.73.0.4
```

### Why network namespaces?
Namespaces give us multiple *virtual machines* inside one Linux box.  
Each host sees only its own interfaces and IP stack.

### Why veth pairs?
A veth pair is a **virtual cable**: frames entering one side come out the other.

### Why Linux bridge?
The Linux bridge acts as a real L2 switch:

- Learns source MACs per VLAN  
- Floods unknown-destination packets  
- Performs unicast forwarding when DMAC is known  
- Does per-VLAN isolation  

---

# 🧠 2. VLAN-aware bridge

By default, a Linux bridge behaves like a simple dumb switch:
- All ports in one broadcast domain  
- No VLAN awareness  
- MAC table is global, not per-VLAN  
- Unknown unicast floods across all ports  

When we run:

```bash
sudo ip link set br0 type bridge vlan_filtering 1
```

We activate **IEEE 802.1Q VLAN behavior**, meaning:

### ✔ 1. VLAN tagging is recognized  
The bridge now parses VLAN tags in Ethernet frames.

### ✔ 2. MAC learning becomes *per VLAN*
FDB entries become:

```
(MAC, VLAN) → port
```

This is crucial: the same MAC can legally appear in two VLANs.

### ✔ 3. Unknown flooding becomes VLAN-scoped
Packets in VLAN 73 flood **only to ports in VLAN 73**.  
veth3 (not in VLAN 73) sees **no frames**.

### ✔ 4. Ports gain a PVID (Port VLAN ID)
If a frame **arrives untagged**:  
It is internally assigned to the port’s *PVID* (default VLAN).

### ✔ 5. Outbound tagging rules apply  
- Access ports → send untagged  
- Trunk ports → send tagged  
(We use access ports in this doc.)

This mirrors real hardware switch behavior.

---

# 🔧 3. Cleanup (Optional)

```bash
sudo ip link del br0 2>/dev/null
sudo ip link del veth0 2>/dev/null
sudo ip link del veth1 2>/dev/null
sudo ip link del veth2 2>/dev/null
sudo ip link del veth3 2>/dev/null

sudo ip netns del h0 2>/dev/null
sudo ip netns del h1 2>/dev/null
sudo ip netns del h2 2>/dev/null
sudo ip netns del h3 2>/dev/null
```

This ensures no leftover interfaces interfere.

---

# 🔧 4. Create veth Pairs

Each veth pair is a virtual Ethernet cable.

```bash
sudo ip link add veth0 type veth peer name veth0p
sudo ip link add veth1 type veth peer name veth1p
sudo ip link add veth2 type veth peer name veth2p
sudo ip link add veth3 type veth peer name veth3p
```

- `veth0` connects to bridge (switch port 0)  
- `veth0p` goes inside namespace `h0`  

---

# 🔧 5. Create Network Namespaces

```bash
sudo ip netns add h0
sudo ip netns add h1
sudo ip netns add h2
sudo ip netns add h3
```

---

# 🔧 6. Move Host Interfaces into Namespaces

```bash
sudo ip link set veth0p netns h0
sudo ip link set veth1p netns h1
sudo ip link set veth2p netns h2
sudo ip link set veth3p netns h3
```

After this, veth0p–veth3p disappear from root namespace.

---

# 🔧 7. Bring Up Loopback Interfaces

```bash
sudo ip netns exec h0 ip link set lo up
sudo ip netns exec h1 ip link set lo up
sudo ip netns exec h2 ip link set lo up
sudo ip netns exec h3 ip link set lo up
```

---

# 🔧 8. Assign IP Addresses

```bash
sudo ip netns exec h0 ip addr add 10.73.0.1/24 dev veth0p
sudo ip netns exec h1 ip addr add 10.73.0.2/24 dev veth1p
sudo ip netns exec h2 ip addr add 10.73.0.3/24 dev veth2p
sudo ip netns exec h3 ip addr add 10.73.0.4/24 dev veth3p
```

---

# 🔧 9. Bring Host Interfaces UP

```bash
sudo ip netns exec h0 ip link set veth0p up
sudo ip netns exec h1 ip link set veth1p up
sudo ip netns exec h2 ip link set veth2p up
sudo ip netns exec h3 ip link set veth3p up
```

---

# 🔧 10. Create Linux Bridge

```bash
sudo ip link add br0 type bridge
```

---

# 🔧 11. Enable VLAN Filtering (the key step)

```bash
sudo ip link set br0 type bridge vlan_filtering 1
```

This step transforms `br0` into a *true VLAN-switch*.

---

# 🔧 12. Attach Switch-Side veth Interfaces

```bash
sudo ip link set veth0 master br0
sudo ip link set veth1 master br0
sudo ip link set veth2 master br0
sudo ip link set veth3 master br0
```

---

# 🔧 13. Bring Up Ports and Bridge

```bash
sudo ip link set veth0 up
sudo ip link set veth1 up
sudo ip link set veth2 up
sudo ip link set veth3 up
sudo ip link set br0 up
```

---

# 🔧  14. Configure VLAN 73 on Access Ports

```bash
sudo bridge vlan add dev veth0 vid 73 pvid untagged
sudo bridge vlan add dev veth1 vid 73 pvid untagged
sudo bridge vlan add dev veth2 vid 73 pvid untagged
```

### Why these flags?

- **pvid** – frames arriving *untagged* are assigned VLAN 73  
- **untagged** – frames transmitted leave without 802.1Q tags  
- This makes veth0–2 pure access ports (like switchports on an enterprise switch).

### Why veth3 is left out?
Because we want a namespace **not in VLAN 73** to verify VLAN isolation behavior.

---

# 🔍 15. Verify VLAN Membership

```bash
bridge vlan show
```

Expected:

```
veth0   73 PVID Egress Untagged
veth1   73 PVID Egress Untagged
veth2   73 PVID Egress Untagged
veth3   <empty>
```

This tells us:

- veth0–2 belong to VLAN 73  
- veth3 belongs to no VLAN → isolated  

---

# 🧪 16. Test Connectivity

## 16.1 VLAN-internal pings (should succeed)

```bash
sudo ip netns exec h0 ping -c 2 10.73.0.2
sudo ip netns exec h1 ping -c 2 10.73.0.3
```

### Why this works?
- ARP from h0 floods only within VLAN 73.  
- h1/h2 reply; MACs are learned; ICMP succeeds.

---

## 16.2 Cross-VLAN ping (should fail)

```bash
sudo ip netns exec h0 ping -c 2 10.73.0.4
```

### Why does h0 → h3 fail?

1. ARP request from h0 is placed in **VLAN 73**.
2. Bridge floods ARP request only to VLAN 73 ports.
3. veth3 is *not* in VLAN 73.
4. h3 **never receives an ARP request**.
5. h0 cannot resolve 10.73.0.4 → ping fails.

This demonstrates proper VLAN isolation.

---

# 🧪 17. UDP Verification Using socat

### Listener on h0

```bash
sudo ip netns exec h0 socat -v udp-recv:5000 -
```

### Sender on h1

```bash
echo "hello from h1" | sudo ip netns exec h1 socat -v - udp-sendto:10.73.0.1:5000
```

Expected:

- h0 prints: `hello from h1`
- Bridge logs show proper flooding/unicast inside VLAN 73  

---

# 🔍 18. Inspect MAC Learning

After some traffic:

```bash
bridge fdb show
```

Sample output:

```
aa:bb:cc:dd:ee:01 dev veth0 vlan 73 master br0
aa:bb:cc:dd:ee:02 dev veth1 vlan 73 master br0
aa:bb:cc:dd:ee:03 dev veth2 vlan 73 master br0
```

### Important insight:
FDB entries are now **(MAC, VLAN)** pairs.

This prevents:
- Cross-VLAN MAC communication

---

# 🔍 19. Optional: Packet Sniffing

```bash
sudo ip netns exec h0 tcpdump -i veth0p -nne
sudo ip netns exec h1 tcpdump -i veth1p -nne
sudo ip netns exec h2 tcpdump -i veth2p -nne
```

Observe:

- ARP broadcasts  
- Unicast replies  
- VLAN-scoped flooding  

---

# 🎉 20. Summary

This guide constructs a simplistic VLAN-aware L2 switch right out of Linux:

- VLAN-aware forwarding  
- Access port behavior  
- PVID behavior  
- Per-VLAN MAC learning  
- VLAN-scoped forwarding
- ICMP + UDP functional verification  
- A non-VLAN member port to test isolation  

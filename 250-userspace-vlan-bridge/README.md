# Userspace VLAN Switch  
This is Part 2 of the Experimental SAI Project.

This project implements a functional Layer 2 switch entirely in user space without depending on the Linux bridge or hardware ASICs.  It focuses on forwarding logic, switch state, VLAN membership, MAC learning, and observability.

## Goal and Motivation

The primary goal of this phase is to build a fully operational datapath capable of receiving 
real Ethernet frames on TAP interfaces and forwarding them according to VLAN and MAC learning 
rules.  By doing everything in user space, the project highlights exactly how a switch behaves internally while avoiding opaque kernel or hardware interactions.

## Topology Overview

The switch attaches to N TAP interfaces. Each TAP behaves like a physical port, and frames received on one port are processed entirely in user space before being written out to the selected egress ports.

```
             +-----------------------+
             |    Userspace Switch   |
             |                       |
             |  +-----------------+  |
   tap0 ---> |  |                 |  | ---> tap1
             |  |  Dataplane:     |  |
             |  |    - VLAN check |  |
             |  |    - MAC learn  |  |
   tap2 ---> |  |    - L2 fwd     |  | ---> tap3
             |  |                 |  |
             |  +-----------------+  |
             +-----------------------+

   Each tap interface acts as a physical port of the virtual switch.

```

## Design of the `userspace_switch` executable

### 1. Dataplane (`switch_dataplane.cpp`)

The `main()` function runs the dataplane in its own thread.
There it attaches to the TAP interfaces and then runs an frame processing event loop.

The dataplane processes each frame in a clear sequence.

1. Packet Receive  
2. Parse Header  
3. VLAN Membership Check  
4. MAC Learning  
5. Forwarding Decision  
6. Write to Egress Ports  

### 2. Switch State (`switch_state.cpp`, `switch_state.h`)

This module contains the data structures to repressent a switch, including
MAC table, VLAN membership, port properties.

## Directory Structure

```
.
├── build.sh
│       Build helper script that configures and compiles the entire project
│       using CMake. Produces the userspace_switch executable.
│
├── CMakeLists.txt
│       Top level CMake project definition. Declares targets, sources, and
│       build requirements for the switch.
│
├── src
│   ├── CMakeLists.txt
│   │       Component level CMake rules for the switch executable.
│   │
│   ├── switch_config.h
│   │       Defines defaults such as number of ports, buffer sizes, VLAN IDs,
│   │       and other compile time configuration constants.
│   │
│   ├── switch_dataplane.cpp
│   │       Implements ingress, learning, forwarding, and egress logic.
│   │       This file contains the full user space switching pipeline.
|   |       Initializes TAP ports, prepares switch state, and drives the main event loop.
│   │
│   ├── switch_main.cpp
│   │       Entry point of the userspace_switch program.
│   │
│   ├── switch_state.cpp
│   │       Implements the switch's dynamic state including MAC table, VLAN
│   │       membership, and lookup primitives used by the dataplane.
│   │
│   └── switch_state.h
│           Header declaring the SwitchState class and related structures.
│
└── tools
    ├── install_deps.sh
    │       Installs required system dependencies such as CMake and TAP
    │       utilities. Helps set up the environment on a fresh system.
    │
    └── setup.sh
            Creates TAP interfaces for the switch. 
```

## Running the Userspace Switch

### 1. Install Dependencies

```
tools/install_deps.sh
```

### 2. Build the userspace switch

```
$ ./build.sh
```
If the build is successful, then the executable would be available
at the following path.
```
build/src/userspace_switch
```

### 3. Create TAP Interfaces

[Optional] Cleanup all the previously created TAP interfaces.
```
$ sudo bash tools/setup.sh clean
```

Create 4 TAP interfaces.
```
$ sudo bash tools/setup.sh 
```

It creates a file `hostinfo.csv` with the virtual host details.

### 4. Run the switch

```
$ build/src/userspace_switch
```

## Verification

In a terminal, run `tcpdump` as: `sudo tcpdump -i veth1`

In a different terminal, run the following `ping` command to ping from h0 to h1.
```
$ sudo ip netns exec h0 ping -c 2 10.73.0.2
```

### Sample output

The following is a sample virtual host information after the setup.
```
$ cat hostinfo.csv 
h0,veth0p,10.73.0.1,be:d3:45:41:83:e4
h1,veth1p,10.73.0.2,06:f3:fc:ce:ac:eb
h2,veth2p,10.73.0.3,b2:b1:73:e0:e0:e7
h3,veth3p,10.73.0.4,12:8b:a9:f8:23:a3

```

Sample `ping` output.

```
$ sudo ip netns exec h0 ping -c 2 10.73.0.2
PING 10.73.0.2 (10.73.0.2) 56(84) bytes of data.
64 bytes from 10.73.0.2: icmp_seq=1 ttl=64 time=0.819 ms
64 bytes from 10.73.0.2: icmp_seq=2 ttl=64 time=0.459 ms

--- 10.73.0.2 ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 1001ms
rtt min/avg/max/mdev = 0.459/0.639/0.819/0.180 ms
```

Sample `tcpdump` output.

```
$ sudo tcpdump -i veth1
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on veth1, link-type EN10MB (Ethernet), snapshot length 262144 bytes
16:04:09.057467 ARP, Request who-has 10.73.0.2 tell 10.73.0.1, length 28
16:04:09.057510 ARP, Reply 10.73.0.2 is-at 06:f3:fc:ce:ac:eb (oui Unknown), length 28
16:04:09.057750 IP 10.73.0.1 > 10.73.0.2: ICMP echo request, id 28499, seq 1, length 64
16:04:09.057795 IP 10.73.0.2 > 10.73.0.1: ICMP echo reply, id 28499, seq 1, length 64
16:04:10.058527 IP 10.73.0.1 > 10.73.0.2: ICMP echo request, id 28499, seq 2, length 64
16:04:10.058591 IP 10.73.0.2 > 10.73.0.1: ICMP echo reply, id 28499, seq 2, length 64

```

Sample output from `userspace_switch`.
```
[Rx] port = 0, dmac = ff:ff:ff:ff:ff:ff, smac = be:d3:45:41:83:e4, ethtype = 0x0806
 +LEARN vlan = 1, mac = be:d3:45:41:83:e4 at port = 0
  [Tx] port = 1, dmac = ff:ff:ff:ff:ff:ff, smac = be:d3:45:41:83:e4, ethtype = 0x0806
  [Tx] port = 2, dmac = ff:ff:ff:ff:ff:ff, smac = be:d3:45:41:83:e4, ethtype = 0x0806
  [Tx] port = 3, dmac = ff:ff:ff:ff:ff:ff, smac = be:d3:45:41:83:e4, ethtype = 0x0806
[Rx] port = 1, dmac = be:d3:45:41:83:e4, smac = 06:f3:fc:ce:ac:eb, ethtype = 0x0806
  [Tx] port = 0, dmac = be:d3:45:41:83:e4, smac = 06:f3:fc:ce:ac:eb, ethtype = 0x0806
```

The IP addresses and MAC addresses in the `tcpdump` and `switch` output
match squarely with the host setup captured in `hostinfo.csv` above.


## FAQ

### What is the following error?

```
$ sudo ip netns exec h0 ping -c 1 10.73.0.2
Cannot open network namespace "h0": No such file or directory
```

This error means that namespace `h0` does not exist.
Please run the setup script mentioned above `tools/setup.sh`.


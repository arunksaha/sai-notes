# SAI Notes: VLAN Switch Journey
Experimental notes and code for building a simple L2 switch stack in three progressive steps, from a Linux bridge baseline to a userspace dataplane with a minimalist SAI layer and management thread.

## Project Steps
- **150-linux-vlan-bridge**: Linux bridge and veth lab to learn basics of VLAN tagging and forwarding ([README](150-linux-vlan-bridge/README.md)).
- **250-userspace-vlan-bridge**: Moves the dataplane into userspace; raw sockets handle ingress/egress and MAC learning ([README](250-userspace-vlan-bridge/README.md)).
- **350-userspace-vlan-bridge-with-sai**: Adds a minimalist SAI shim plus a management-plane thread that consumes FDB notifications ([README](350-userspace-vlan-bridge-with-sai/README.md)).

## How to Use
- Clone the repo, pick a step, and follow its README for setup, build, and run instructions.
- Each directory is self-contained; tools/setup scripts create the test topology and generated headers where needed.

## Repo Layout
- `150-linux-vlan-bridge/` — Linux bridge lab.
- `250-userspace-vlan-bridge/` — Userspace VLAN bridge.
- `350-userspace-vlan-bridge-with-sai/` — Userspace bridge with SAI shim and mgmt-plane.
- `LICENSE` — Apache 2.0 license.

## Contributing / Issues
- File issues or open PRs if you spot bugs or want to extend the examples.
- This is experimental code; expect sharp edges.

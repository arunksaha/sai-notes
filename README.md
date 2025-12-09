# Understanding SAI by Building a Minimal VLAN Switch
This repo/doc captures notes and code for building a simple L2 switch stack
in three progressive steps, from a Linux bridge baseline to
a userspace dataplane with a minimalist SAI layer and management plane.

## Project Steps
- **150-linux-vlan-bridge**: Linux bridge and veth lab to learn basics of VLAN tagging and forwarding ([README](150-linux-vlan-bridge/README.md)).
- **250-userspace-vlan-bridge**: Moves the dataplane into userspace; raw sockets handle ingress/egress and MAC learning ([README](250-userspace-vlan-bridge/README.md)).
- **350-userspace-vlan-bridge-with-sai**: Adds a minimalist SAI shim plus a management-plane thread that uses SAI API to configure VLAN and consume FDB notifications ([README](350-userspace-vlan-bridge-with-sai/README.md)).

## Comparison to switchsim

I previously built a (Metro) Ethernet switch simulator at
https://github.com/arunksaha/switchsim.
This new effort differs from switchsim in several important ways:
 - switchsim is far more feature-rich, while this project is intentionally minimalist.
 - switchsim includes a full CLI, whereas this project does not.
 - switchsim processes *simulated* packets, while this prototype handles *real* packets on Linux interfaces.
## How to Use
- Clone the repo, pick a step, and follow its README for setup, build, and run instructions.
- Each directory is self-contained; tools/setup scripts create the test topology and generated headers where needed.

## Repo Layout
- `150-linux-vlan-bridge/` — Linux bridge lab
- `250-userspace-vlan-bridge/` — Userspace VLAN bridge with dataplane
- `350-userspace-vlan-bridge-with-sai/` — Userspace VLAN bridge dataplane, management-plane, and SAI.

## Contributing / Issues / Feedback
- File issues or open PRs if you spot bugs or want to extend the examples.
- This is experimental code; expect sharp edges.
- Any feedback is welcome.

# Session Context

This file preserves the current project direction so work can continue on another machine.

## Active direction

- Main active spec: `PROJECT_SPEC.md`
- Older benchmark/background spec: `BENCH_PROJ.md`
- Approved design spec: `docs/superpowers/specs/2026-04-21-latency-fastpath-design.md`

## Project goals

- Primary focus: performance engineering
- Primary metric: same-host message latency
- Secondary metrics: CPU/message and syscall pressure
- Prefer improvements that can help existing socket-based applications
- Keep the scope small for phase 1

## Runtime model

- Primary supported runtime: vanilla Ubuntu VM
- Higher-fidelity rerun target: bare-metal Ubuntu Linux
- Docker: dev/build convenience only
- macOS: workstation only, not benchmark runtime

## Phase-1 scope

- `veth` + socket baseline
- shared-memory ring benchmark
- matrix runner
- minimal helper scripts with documentation
- selective observability later (`perf stat` first)

## Explicitly later

- AF_XDP remains experimental / feature-gated
- Kubernetes/Cilium/netkit is a future extension
- RXE/SIW/QEMU is a future extension

## Current implementation status

- Repo structure cleaned up
- Specs promoted and aligned
- Helper docs added in `docs/HELPERS.md`
- Nix flake added: `flake.nix` and `flake.lock`
- `.envrc` uses the flake for direnv
- Baseline reliability improvements already made:
  - `sock_bench` has a receive timeout
  - runners accept `CASE_ID` / `OUT_PATH`
  - `run_shm_bench.sh` waits for socket readiness
  - matrix runner prints subprocess stdout/stderr

## Recommended next step on Ubuntu

Run and verify:

1. `nix develop` or install equivalent packages
2. `make all`
3. `make test`
4. `sudo env PATH=$PATH make bench-veth`
5. `make bench-shm`
6. `sudo env PATH=$PATH make matrix`

Then, if needed:

- tighten `veth` cleanup and diagnostics further
- tighten `shm` exit/cleanup behavior further
- add `perf stat` integration for selected cases

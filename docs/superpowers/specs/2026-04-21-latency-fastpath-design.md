# Same-Host Latency Fast-Path Lab Design

Date: 2026-04-21
Status: Draft approved for spec write-up

## 1. Objective

Build a small, repeatable same-host latency lab for Linux namespace/container-style communication, optimized for **performance engineering** rather than broad platform coverage.

The lab should answer:

- how much latency the default same-host namespace/socket path adds,
- whether socket-compatible alternatives can reduce that cost,
- what upper-bound improvement is possible when both endpoints cooperate,
- and which results are robust enough to matter on a real system.

## 2. Project priorities

Priority order:

1. **Same-host message latency** as the main optimization target.
2. **Applicability to existing socket-based applications** where possible.
3. **CPU/message and syscall pressure** as secondary metrics.
4. **Throughput** as a guardrail, not the main success metric.

This means the project should prefer measurements that explain localhost request/response behavior over large-scale throughput tuning.

## 3. Runtime model

### Primary supported runtime

The main supported runtime is a **standard Ubuntu VM**.

Examples:
- Ubuntu VM on macOS
- Ubuntu VM on another desktop/laptop

### Higher-fidelity runtime

**Bare-metal Ubuntu Linux** is a later validation mode for cleaner latency numbers.

### Docker role

Docker is a **development/build convenience only**.
It is not the primary benchmark runtime for phase 1 because this project depends on direct Linux kernel features such as namespaces, perf, and potentially BPF/XDP.

### Vanilla requirement

Phase 1 must work on a **vanilla Ubuntu VM** with:
- normal distro kernel,
- normal distro packages,
- no custom kernel patches,
- no special NIC requirements.

Advanced paths may be feature-gated and reported as `SKIP` when unsupported.

## 4. Phase-1 scope

### In scope

- namespace-based same-host testbed
- `veth` + socket baseline
- shared-memory ring benchmark
- AF_XDP included in the design as an experimental socket-adjacent path
- result capture with latency-first metrics
- lightweight observability:
  - `perf stat` required where available
  - `perf record` optional for selected cases
  - `trace-cmd`/ftrace optional for selected cases

### Out of scope for phase 1

- Kubernetes/kind/Cilium/netkit execution
- RXE/SIW/QEMU control path execution
- large-scale throughput tuning
- multi-host networking
- broad helper-script sprawl

### Future extensions to document now

Keep these ideas documented for later phases:
- Kubernetes/kind/Cilium/netkit profile
- richer tracing and flamegraph workflow
- RXE/SIW/QEMU control path
- container runtime integration
- broader throughput and concurrency sweeps

## 5. Recommended benchmark identity

The project should be a **custom microbenchmark lab first**, with app-level tools used later for validation.

### Primary benchmark identity

- `sock_echo` / `sock_bench` for socket baseline
- `shm_bench` for cooperative same-host upper bound
- later `xsk_bench` for AF_XDP

### Secondary validation tools

- `iperf3`
- `netperf` (later, optional)
- optional Cilium-based workloads in a future phase

### Why

Custom benches let the lab control and compare:
- message size
- batching
- wakeup/notification policy
- queue placement
- CPU pinning
- integrity checks

That makes them a better fit for performance engineering than starting from generic application benchmarks.

## 6. Architecture

### 6.1 Main components

#### Topology

A minimal namespace topology:
- namespace `c1`
- namespace `c2`
- one `veth` pair
- fixed IP addresses
- deterministic cleanup

#### Bench binaries

- `sock_echo` / `sock_bench` for baseline request/response latency
- `shm_bench` for cooperative shared-memory transport
- later `xsk_bench` for AF_XDP path

#### Runner

A single case runner should:
1. read a case definition,
2. prepare the environment,
3. start the server side,
4. warm up,
5. execute the benchmark,
6. collect results and optional observation artifacts,
7. validate integrity,
8. clean up fully.

#### Observation helpers

Minimal helpers only:
- `perf stat` wrapper
- optional `perf record` wrapper
- optional tracing wrapper for selected cases

#### Results store

The results layout should preserve:
- raw artifacts,
- normalized result summaries,
- enough metadata to rerun or understand a case later.

### 6.2 Data-path roles

#### `veth` socket baseline

Purpose:
- establish the default same-host namespace/container-like path.

Role in the lab:
- reference latency and CPU cost.

#### `shm_ring`

Purpose:
- show the upper bound when both endpoints cooperate.

Role in the lab:
- establish what same-host latency can look like when the packet/socket path is bypassed.

#### `af_xdp_skb`

Purpose:
- explore a socket-adjacent fast path that still relates to real Linux packet processing.

Role in the lab:
- experimental path included in the design but not required for initial phase-1 success.

## 7. Helper philosophy

Helpers must be minimal and well documented.

### Rules

Every helper must have:
- one clear purpose,
- one documented call pattern,
- one place in the runner or workflow where it is used.

### Acceptable helper categories

- topology helper
- run helper
- observation helper
- collection helper

### Not acceptable

- many thin wrappers around one-off commands
- helpers never used by the main runner
- undocumented helper flags or side effects

### Required helper documentation

Each helper should document:
- what it does,
- when it is called,
- required privileges,
- expected inputs,
- expected outputs,
- one example invocation.

## 8. Measurement philosophy

### Primary metrics

- RTT latency, especially small-message latency
- p50 / p90 / p99, with emphasis on tails where feasible

### Secondary metrics

- CPU/message
- syscall pressure
- context switches / migrations where useful

### Guardrail metrics

- throughput or message rate only to ensure a path is not obviously unusable

### Observation policy

- `perf stat` should be attached to at least selected baseline cases
- `perf record` should be used only when needed for diagnosis
- tracing should be selective, not default-on for every case

This keeps the project focused on fast iteration while still allowing explanation of wins and losses.

## 9. Success criteria

### Hard success for phase 1

On a vanilla Ubuntu VM:
- the repo builds cleanly,
- the namespace baseline runs end-to-end,
- the shared-memory benchmark runs end-to-end,
- the matrix runner produces result artifacts,
- unsupported advanced paths report `SKIP` instead of failing ambiguously,
- cleanup leaves no stale namespaces or benchmark processes.

### Soft success for phase 1

- `shm_ring` beats the `veth` socket baseline for small-message RTT,
- baseline results are stable enough to compare across repeated runs,
- `perf stat` can be collected for at least selected cases,
- output artifacts are understandable without reading source code.

### AF_XDP status in phase 1

AF_XDP is part of the design, but not a requirement for initial project completion.
It may remain:
- scaffolded,
- feature-gated,
- or marked experimental,
until the vanilla baseline is solid.

## 10. Naming and document organization

The repo should treat the newer performance-engineering-oriented spec as the main spec.

Planned naming:
- `project_2.md` becomes `PROJECT_SPEC.md`
- older `project_spec.md` becomes `BENCH_PROJ.md`

This rename is intended to make the active direction explicit:
- the newer document is the main phase-1 spec,
- the older document remains useful as supporting benchmark/background material.

## 11. Initial implementation implications

When implementation planning begins, the initial order should be:

1. make the namespace baseline fully reliable on a vanilla Ubuntu VM,
2. make the shared-memory path equally reliable,
3. make result collection and helper documentation clear,
4. only then promote AF_XDP work,
5. keep future extensions documented but out of the critical path.

This preserves the project’s core promise: a small but serious same-host latency lab that is useful on a real system.

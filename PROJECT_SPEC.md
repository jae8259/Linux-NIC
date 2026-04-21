# Localhost Container Fast-Path Lab

## Status

This is the **main active spec** for the project.

- Main goal: **performance engineering for same-host message latency**
- Main supported runtime: **vanilla Ubuntu VM**
- Higher-fidelity rerun target: **bare-metal Ubuntu Linux**
- Docker role: **dev/build convenience only**

The older benchmark-heavy spec has been preserved as `BENCH_PROJ.md`.
The approved design rationale is in:

- `docs/superpowers/specs/2026-04-21-latency-fastpath-design.md`

---

## 1. Project goal

Build a small, repeatable same-host latency lab that helps answer:

1. How expensive is the default namespace/container-like localhost socket path?
2. Can a socket-adjacent Linux fast path reduce that cost for existing apps?
3. What is the upper-bound latency if both endpoints cooperate?
4. Which results are robust enough to matter on a real system later?

This project is primarily about **same-host request/response latency**, not large-scale throughput tuning.

---

## 2. Scope and priorities

### Priority order

1. **Same-host message latency**
2. **Applicability to existing socket-based applications**
3. **CPU/message and syscall pressure**
4. **Throughput as a guardrail only**

### Phase-1 in scope

- Linux namespace-based topology
- `veth` + socket baseline
- shared-memory ring benchmark
- matrix runner and reproducible result artifacts
- lightweight observability:
  - `perf stat` first
  - `perf record` selectively
  - `trace-cmd`/ftrace only for chosen cases
- AF_XDP represented in the project direction, but **not required for phase-1 completion**

### Phase-1 out of scope

- Kubernetes/kind/Cilium/netkit execution
- RXE/SIW/QEMU control-path execution
- broad throughput tuning
- multi-host networking
- helper-script sprawl

---

## 3. Runtime model

### Main supported runtime

A **standard Ubuntu VM** is the primary supported runtime.

Examples:
- Ubuntu VM on macOS
- Ubuntu VM on another Linux/Windows desktop host

### Higher-fidelity mode

A **bare-metal Ubuntu Linux machine** is the preferred rerun target for cleaner latency numbers.

### Docker policy

Docker is allowed for:
- building the toolchain
- reproducible local development

Docker is **not** the primary benchmark runtime for phase 1, because the lab depends on Linux kernel features such as:
- network namespaces
- perf
- tracing
- possibly BPF/XDP later

### Vanilla requirement

Phase 1 must work on a **vanilla Ubuntu VM** with:
- stock distro kernel
- stock distro packages
- no special NIC requirement
- no custom kernel patches

Advanced features may be marked `SKIP` if unsupported.

---

## 4. Benchmark identity

The project should be a **custom microbenchmark lab first**, with app-level tools used later only as validation.

### Primary benchmark binaries

- `base/sock_echo.c`
- `base/sock_bench.c`
- `shm/shm_bench.c`
- later: `afxdp/xsk_bench.c`

### Secondary validation tools for later phases

- `iperf3`
- `netperf`
- optional Cilium/netkit workloads

### Why

Custom benches let us control exactly what matters for latency engineering:
- message size
- batching
- wakeup policy
- integrity checks
- CPU placement
- rerun behavior

---

## 5. Test paths

### A. `veth_socket`

Two namespaces connected by `veth`, running the socket baseline.

Purpose:
- establish the default container-like localhost path
- provide the main phase-1 reference result

### B. `shm_ring`

Two cooperative endpoints exchanging messages through shared memory.

Purpose:
- provide the upper-bound same-host latency reference
- show what is possible when the socket/packet path is bypassed

### C. `af_xdp_skb` (phase-1 experimental)

AF_XDP in generic/copy-oriented mode where possible.

Purpose:
- explore a Linux fast path that is closer to existing packet processing
- remain feature-gated until the baseline is solid

### D. Future extensions

Document now, implement later:
- `cilium_netkit`
- `rxe_siw_vm_control`
- richer tracing and flamegraphs
- container runtime integration

---

## 6. Hard success criteria for phase 1

On a vanilla Ubuntu VM:

- the repo builds cleanly
- the namespace baseline runs end-to-end
- the shared-memory benchmark runs end-to-end
- the matrix runner writes result artifacts
- unsupported advanced paths are reported as `SKIP`
- cleanup leaves no stale namespaces or benchmark processes

## 7. Soft success criteria for phase 1

- `shm_ring` beats the socket baseline for small-message RTT
- repeated runs are stable enough to compare
- `perf stat` can be collected for selected cases
- results are understandable without reading source code first

---

## 8. Helper philosophy

Helpers must be **minimal, documented, and actually used**.

### Every helper must have

- one clear purpose
- one documented call pattern
- one known caller in the workflow

### Acceptable helper categories

- topology helper
- run helper
- observation helper
- collection helper

### Avoid

- wrappers that only hide a single simple shell command with no added value
- helpers that are not used by the runner or documented workflow
- large piles of shell scripts with overlapping responsibilities

### Required documentation per helper

Document:
- what it does
- when it is used
- required privileges
- inputs
- outputs
- one example invocation

---

## 9. Observation policy

Keep observation useful, not overwhelming.

### Default

- benchmark result artifact
- environment/case metadata

### Required first observation feature

- `perf stat` on selected baseline cases

### Optional selective observation

- `perf record`
- `trace-cmd` / ftrace

Observation exists to explain wins and losses, not to dominate the initial workflow.

---

## 10. Repository direction

Phase 1 should prioritize the following order:

1. reliable namespace/socket baseline
2. reliable shared-memory benchmark
3. clear result collection
4. helper documentation and cleanup discipline
5. selective `perf stat` integration
6. AF_XDP scaffolding and feature gating
7. future extensions documented but out of the critical path

---

## 11. Immediate implementation plan anchor

The current implementation effort should focus on:

- making the `veth` baseline solid on a vanilla Ubuntu VM
- making `shm_bench` solid on a vanilla Ubuntu VM
- documenting helper usage well
- keeping the scope small
- leaving AF_XDP and netkit as non-blocking future paths

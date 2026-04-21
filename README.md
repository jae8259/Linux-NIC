# Linux NIC Study

This repo is now centered on a **same-host latency fast-path lab** for Linux namespace/container-style communication.

This repo contains a runnable localhost fast-path lab focused on **same-host latency**.

- Main active spec: `PROJECT_SPEC.md`
- Older benchmark/background spec: `BENCH_PROJ.md`
- Approved design doc: `docs/superpowers/specs/2026-04-21-latency-fastpath-design.md`

Phase-1 implemented paths:
- `veth` socket baseline
- shared-memory ring baseline

Later/experimental paths:
- AF_XDP
- RXE/SIW control path

## Quick start (Ubuntu VM or Ubuntu host)

- Build: `make all`
- Shared-memory RTT: `make bench-shm`
- veth+netns UDP RTT (needs root): `make bench-veth`
- Matrix runner (skips when unsupported): `make matrix`

Results are written to `results/raw/*.json`.

See also:
- `docs/ENVIRONMENT.md`
- `results/README.md`

## Dev container (optional)

- `docker compose -f docker/compose.yaml run --rm dev`

## Mac + Ubuntu workflow

Mac can build the dev container, but the actual lab should run in a Linux kernel environment. Recommended workflow:

- Preferred: use an Ubuntu VM on macOS (Multipass/Lima/UTM/etc.), clone this repo inside the VM, and run the lab there.
- Higher-fidelity later: rerun on bare-metal Ubuntu Linux if you want cleaner latency numbers.
- See `docs/ENVIRONMENT.md` for notes.

## Notes on `ARTICLE.md`

In the `ARTICLE.md`, a list of urls will be given. You read them and summarize them.

```md
# <Short Title>
Link: <url>
Status: <TODO|WIP|READ>
Summary: A brief summary
(Optioal) ARCHIVED: Refer to a file you made summarization
```

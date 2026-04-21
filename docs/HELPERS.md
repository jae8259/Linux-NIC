# Helper Scripts

This project keeps helper scripts intentionally small.

Each helper below has one purpose, one main call pattern, and one clear place in the workflow.

## `scripts/prereq_check.sh`

- Purpose: verify the minimum local commands needed for the phase-1 baseline
- Used by: `make prereq`
- Privilege: none
- Inputs: none
- Outputs: text status to stdout/stderr
- Example:
  - `./scripts/prereq_check.sh`

## `scripts/netns_up.sh`

- Purpose: create the two-namespace `veth` topology used by the socket baseline
- Used by: `scripts/run_veth_bench.sh`
- Privilege: root
- Inputs:
  - `NS1`, `NS2`
  - `VETH1`, `VETH2`
  - `IP1`, `IP2`
- Outputs:
  - creates namespaces and links
  - prints a one-line success message
- Example:
  - `sudo NS1=ns1 NS2=ns2 ./scripts/netns_up.sh`

## `scripts/netns_down.sh`

- Purpose: remove the two benchmark namespaces
- Used by: `scripts/run_veth_bench.sh`
- Privilege: root
- Inputs:
  - `NS1`, `NS2`
- Outputs:
  - deletes namespaces if present
  - prints a one-line success message
- Example:
  - `sudo ./scripts/netns_down.sh`

## `scripts/run_veth_bench.sh`

- Purpose: run the phase-1 socket baseline end-to-end
- Used by:
  - `make bench-veth`
  - `tests/run_matrix.py`
- Privilege: root
- Inputs:
  - `SIZE`
  - `ITERS`
  - `PORT`
  - `TIMEOUT_MS`
  - `CASE_ID`
  - `OUT_PATH`
- Outputs:
  - result JSON file in `results/raw/`
  - benchmark JSON echoed to stdout
- Example:
  - `sudo SIZE=64 ITERS=50000 ./scripts/run_veth_bench.sh`

## `scripts/run_shm_bench.sh`

- Purpose: run the shared-memory latency benchmark end-to-end
- Used by:
  - `make bench-shm`
  - `tests/run_matrix.py`
- Privilege: none on Linux
- Inputs:
  - `SOCK_PATH`
  - `SIZE`
  - `ITERS`
  - `CASE_ID`
  - `OUT_PATH`
- Outputs:
  - result JSON file in `results/raw/`
  - benchmark JSON echoed to stdout
- Example:
  - `SIZE=64 ITERS=200000 ./scripts/run_shm_bench.sh`

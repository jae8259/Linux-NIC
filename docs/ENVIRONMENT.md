# Environment

## Supported run environments

- **Primary supported runtime:** Ubuntu VM with network namespaces enabled.
- **Higher-fidelity runtime:** bare-metal Ubuntu Linux.
- **Build:** Linux (native) or the provided dev container.

macOS cannot run the netns/BPF parts directly; use an Ubuntu VM (Multipass/Lima/UTM/etc.) or an Ubuntu desktop and run the lab there.

## Dev container

From the repo root:

- `docker compose -f docker/compose.yaml run --rm dev`

This mounts the repo at `/work` and builds binaries with `make all`.

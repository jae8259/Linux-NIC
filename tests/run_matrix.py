#!/usr/bin/env python3

import json
import os
import subprocess
import sys
from pathlib import Path


def parse_minimal_yaml_cases(text: str):
    cases = []
    cur = None
    in_cases = False
    for raw in text.splitlines():
        line = raw.rstrip()
        if not line or line.lstrip().startswith("#"):
            continue
        if line.strip() == "cases:":
            in_cases = True
            continue
        if not in_cases:
            continue
        if line.lstrip().startswith("- "):
            if cur:
                cases.append(cur)
            cur = {}
            rest = line.lstrip()[2:].strip()
            if rest:
                k, v = rest.split(":", 1)
                cur[k.strip()] = v.strip()
            continue
        if cur is None:
            continue
        if ":" in line:
            k, v = line.strip().split(":", 1)
            cur[k.strip()] = v.strip()
    if cur:
        cases.append(cur)

    def coerce(case):
        out = dict(case)
        for key in ("size", "iters"):
            if key in out:
                out[key] = int(out[key])
        return out

    return [coerce(c) for c in cases]


def run(cmd, *, env=None, check=True):
    print("+", " ".join(cmd), flush=True)
    proc = subprocess.run(cmd, env=env, check=check, text=True, capture_output=True)
    if proc.stdout:
        print(proc.stdout, end="")
    if proc.stderr:
        print(proc.stderr, end="", file=sys.stderr)
    return proc


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} tests/matrix.yaml", file=sys.stderr)
        return 2

    repo = Path(__file__).resolve().parents[1]
    matrix_path = Path(sys.argv[1]).resolve()
    cases = parse_minimal_yaml_cases(matrix_path.read_text())

    results_dir = repo / "results" / "raw"
    results_dir.mkdir(parents=True, exist_ok=True)

    is_linux = sys.platform.startswith("linux")
    is_root = os.geteuid() == 0 if is_linux else False

    ok = 0
    skip = 0
    fail = 0

    for case in cases:
        cid = case["id"]
        path = case["path"]
        size = int(case.get("size", 64))
        iters = int(case.get("iters", 100000))
        out_path = results_dir / f"{cid}.json"

        try:
            if path == "veth_udp":
                if not is_linux:
                    print(f"SKIP {cid}: Linux required")
                    skip += 1
                    continue
                if not is_root:
                    print(f"SKIP {cid}: root required (netns)")
                    skip += 1
                    continue
                env = os.environ.copy()
                env["SIZE"] = str(size)
                env["ITERS"] = str(iters)
                env["CASE_ID"] = cid
                env["OUT_PATH"] = str(out_path)
                run(["bash", str(repo / "scripts" / "run_veth_bench.sh")], env=env)
                ok += 1
            elif path == "shm":
                if not is_linux:
                    print(f"SKIP {cid}: Linux required (memfd/eventfd)")
                    skip += 1
                    continue
                env = os.environ.copy()
                env["SIZE"] = str(size)
                env["ITERS"] = str(iters)
                env["CASE_ID"] = cid
                env["OUT_PATH"] = str(out_path)
                run(["bash", str(repo / "scripts" / "run_shm_bench.sh")], env=env)
                ok += 1
            else:
                print(f"SKIP {cid}: unknown path={path}")
                skip += 1
                continue

            if out_path.exists():
                payload = json.loads(out_path.read_text())
                if "rtt_ns" not in payload:
                    raise RuntimeError("missing rtt_ns in result")
            else:
                raise RuntimeError(f"missing result file {out_path}")

        except Exception as e:
            print(f"FAIL {cid}: {e}")
            fail += 1

    print(json.dumps({"ok": ok, "skip": skip, "fail": fail}))
    return 0 if fail == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Rebuild the RedBunny training data whenever source files change.

Run this in a terminal while editing the manual, examples, library or tests:

    python3 TRAIN_IA_MODEL/watch.py

No third-party packages are required.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
WATCHED = (
    ROOT / "README.md",
    ROOT / "docs",
    ROOT / "examples",
    ROOT / "lib",
    ROOT / "tests",
    HERE / "seed.jsonl",
    HERE / "prompts",
    HERE / "build.py",
)


def snapshot() -> dict[str, tuple[int, int]]:
    state: dict[str, tuple[int, int]] = {}
    for item in WATCHED:
        paths = [item] if item.is_file() else item.rglob("*") if item.exists() else []
        for path in paths:
            if not path.is_file() or "__pycache__" in path.parts:
                continue
            stat = path.stat()
            state[str(path)] = (stat.st_mtime_ns, stat.st_size)
    return state


def build() -> int:
    return subprocess.call([sys.executable, str(HERE / "build.py")], cwd=ROOT)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interval", type=float, default=1.5, help="poll interval in seconds")
    args = parser.parse_args()

    print(f"Watching RedBunny sources. Data output: {HERE / 'data'}", flush=True)
    print("Press Ctrl-C to stop.", flush=True)
    build()
    state = snapshot()
    try:
        while True:
            time.sleep(args.interval)
            new_state = snapshot()
            if new_state != state:
                changed = sorted(set(new_state) ^ set(state))
                changed += sorted(
                    path for path in set(new_state) & set(state) if new_state[path] != state[path]
                )
                print("\nChange detected:", flush=True)
                for path in changed[:10]:
                    print(f"  {Path(path).relative_to(ROOT)}", flush=True)
                time.sleep(0.3)
                build()
                state = snapshot()
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0


if __name__ == "__main__":
    sys.exit(main())

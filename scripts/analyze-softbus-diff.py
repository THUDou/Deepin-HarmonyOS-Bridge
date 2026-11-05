#!/usr/bin/env python3
"""Summarize a KLink-vs-OpenHarmony dsoftbus tree diff for rebase planning.

Usage:
  python scripts/analyze-softbus-diff.py third_party/klink/dsoftbus third_party/dsoftbus-6.0
"""

from __future__ import annotations

import argparse
import collections
import os
import subprocess
import sys


def diff_lines(left: str, right: str) -> list[str]:
    process = subprocess.run(
        ["git", "diff", "--no-index", "--no-prefix", "--name-status", left, right],
        capture_output=True,
        text=True,
        check=False,
    )
    if process.returncode not in (0, 1):
        raise RuntimeError(process.stderr.strip())
    return [line for line in process.stdout.splitlines() if line.strip()]


def relative(path: str, base: str) -> str:
    return os.path.relpath(path, base).replace("\\", "/")


def top_level(path: str) -> str:
    parts = path.split("/")
    return parts[0] if len(parts) > 1 else "(root)"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("left", help="KLink dsoftbus tree")
    parser.add_argument("right", help="OpenHarmony dsoftbus tree")
    parser.add_argument("--hints", type=int, default=80)
    args = parser.parse_args()

    lines = diff_lines(args.left, args.right)
    statuses: collections.Counter[str] = collections.Counter()
    left_top: collections.Counter[str] = collections.Counter()
    right_top: collections.Counter[str] = collections.Counter()
    host_hints: list[str] = []
    klink_only: list[str] = []
    oh_only: list[str] = []
    klink_cmake = 0

    for line in lines:
        fields = line.split("\t")
        status = fields[0]
        if "/.git/" in fields[1].replace("\\", "/"):
            continue
        if status[0] in ("R", "C") and len(fields) >= 3:
            if "/.git/" in fields[2].replace("\\", "/"):
                continue
        statuses[status[0]] += 1
        if status[0] in ("R", "C") and len(fields) >= 3:
            old = relative(fields[1], args.left)
            new = relative(fields[2], args.right)
            left_top[top_level(old)] += 1
            right_top[top_level(new)] += 1
            combined = (old + " -> " + new).lower()
            if any(token in combined for token in ("linux", "host", "cmake", "ifname", "netdev", "binder")):
                host_hints.append(f"{status} {old} -> {new}")
            continue
        if status[0] == "A":
            path = relative(fields[1], args.right)
            right_top[top_level(path)] += 1
            oh_only.append(path)
        else:
            path = relative(fields[1], args.left)
            left_top[top_level(path)] += 1
            if status[0] == "D":
                klink_only.append(path)
                if path.endswith("CMakeLists.txt"):
                    klink_cmake += 1
        lower = path.lower()
        if any(token in lower for token in ("linux", "host", "cmake", "ifname", "netdev", "binder")):
            host_hints.append(f"{status} {path}")

    print(f"total_diff_entries={sum(statuses.values())}")
    print("status_counts=" + ", ".join(f"{key}={statuses[key]}" for key in sorted(statuses)))
    print()
    print("left_top_changes:")
    for name, count in left_top.most_common(25):
        print(f"  {count:5d}  {name}")
    print()
    print("right_top_changes:")
    for name, count in right_top.most_common(25):
        print(f"  {count:5d}  {name}")
    print()
    print(f"klink_only_cmake_files={klink_cmake}")
    print(f"klink_only_files={len(klink_only)}")
    print("klink_only_examples:")
    for path in [p for p in klink_only if not p.endswith("CMakeLists.txt")][:20]:
        print("  " + path)
    print(f"oh_only_files={len(oh_only)}")
    print("oh_only_examples:")
    for path in oh_only[:20]:
        print("  " + path)
    print()
    print(f"host_rebase_hints={len(host_hints)} (first {args.hints})")
    for hint in host_hints[: args.hints]:
        print("  " + hint)
    return 0


if __name__ == "__main__":
    sys.exit(main())

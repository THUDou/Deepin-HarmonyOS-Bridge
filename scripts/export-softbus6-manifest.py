#!/usr/bin/env python3
"""Inventory OpenHarmony 6.0 dsoftbus sources and compare KLink 4.x paths.

This is a P0 aid, not a build system. KLink's CMakeLists are 4.x source lists;
before generating 6.x CMake targets we need to know exactly which paths no
longer exist and which 6.x GN targets can replace them.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

SOURCE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}
GN_SOURCE_RE = re.compile(r"sources\s*=\s*\[(.*?)\]", re.S)
QUOTED_RE = re.compile(r'"([^"]+)"')
CMAKE_PATH_RE = re.compile(r'"([^"]+\.(?:c|cc|cpp|cxx|h|hpp))"')


GN_ROOT_VARS = {
    "$dsoftbus_root_path": "",
    "${dsoftbus_root_path}": "",
    "$dsoftbus_core_path": "core",
    "${dsoftbus_core_path}": "core",
    "$dsoftbus_sdk_path": "sdk",
    "${dsoftbus_sdk_path}": "sdk",
    "$dsoftbus_dfx_path": "dfx",
    "${dsoftbus_dfx_path}": "dfx",
    "$softbus_adapter_common": "adapter/common",
    "${softbus_adapter_common}": "adapter/common",
}


def expand_gn_path(gn: Path, raw: str) -> Path | None:
    rel = raw
    for var, value in GN_ROOT_VARS.items():
        rel = rel.replace(var, value)
    rel = rel.lstrip("/")
    if rel.startswith("//"):
        rel = rel[2:]
    # Paths expanded from a root variable are root-relative; bare paths are
    # relative to the BUILD.gn directory (GN's default for source lists).
    was_root_var = any(v in raw for v in GN_ROOT_VARS if v != "$dsoftbus_root_path")
    if was_root_var:
        return Path(rel)
    if raw.startswith("$dsoftbus_root_path") or raw.startswith("${dsoftbus_root_path}"):
        return Path(rel)
    return None


def gn_sources(root: Path) -> tuple[Counter, dict[str, list[str]]]:
    by_dir: dict[str, list[str]] = defaultdict(list)
    for gn in sorted(root.rglob("BUILD.gn")):
        text = gn.read_text(encoding="utf-8", errors="replace")
        for block in GN_SOURCE_RE.findall(text):
            for rel in QUOTED_RE.findall(block):
                if Path(rel).suffix not in SOURCE_EXTS:
                    continue
                root_rel = expand_gn_path(gn, rel)
                src = (root / root_rel).resolve() if root_rel else (gn.parent / rel).resolve()
                try:
                    rel_to_root = src.relative_to(root.resolve()).as_posix()
                except ValueError:
                    continue
                if src.is_file():
                    parts = rel_to_root.split("/")
                    key = "/".join(parts[:2]) if len(parts) > 1 else parts[0]
                    by_dir[key].append(rel_to_root)
    flat = Counter()
    for paths in by_dir.values():
        for p in paths:
            flat[p] += 1
    return flat, by_dir


def klink_missing(klink_root: Path, oh6_root: Path) -> list[str]:
    missing: set[str] = set()
    for cmake in sorted(klink_root.rglob("CMakeLists.txt")):
        text = cmake.read_text(encoding="utf-8", errors="replace")
        for rel in CMAKE_PATH_RE.findall(text):
            candidate = (cmake.parent / rel).resolve()
            try:
                rel_to_klink = candidate.relative_to(klink_root.resolve()).as_posix()
            except ValueError:
                rel_to_klink = rel
            if not (oh6_root / rel_to_klink).is_file():
                missing.add(rel_to_klink)
    return sorted(missing)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--oh6", default="third_party/dsoftbus-6.0")
    ap.add_argument("--klink", default="third_party/klink/dsoftbus")
    ap.add_argument("--out", default="build/softbus6-manifest.json")
    args = ap.parse_args()

    oh6 = Path(args.oh6)
    klink = Path(args.klink)
    flat, by_dir = gn_sources(oh6)
    missing = klink_missing(klink, oh6)
    data = {
        "oh6_root": str(oh6),
        "klink_root": str(klink),
        "gn_source_count": len(flat),
        "gn_sources_by_top_dir": {k: len(v) for k, v in sorted(by_dir.items())},
        "klink_cmake_paths_missing_in_oh6": len(missing),
        "missing_examples": missing[:80],
    }
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({k: data[k] for k in (
        "gn_source_count",
        "gn_sources_by_top_dir",
        "klink_cmake_paths_missing_in_oh6",
    )}, indent=2, ensure_ascii=False))
    print(f"manifest: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

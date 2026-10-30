#!/usr/bin/env python3
"""Keep the HarmonyOS/OpenHarmony HAP on the standard, vendor-neutral layer.

The HAP is the portable layer (the "APK"): it may use standard @ohos.* and
@kit.* APIs, but must not import Huawei-only / vendor-only SDKs. Vendor
capabilities (super device, Kaihong dsoftbus, Huawei Kits) belong to optional
profiles or Pro modules, not to the core HAP.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OHOS = ROOT / "ohos"

# Vendor-only namespaces that would make the core HAP non-portable.
FORBIDDEN = (
    "@hms.",
    "@huawei.",
    "@hw.",
    "@agconnect",
    "@hmscore",
)

IMPORT_RE = re.compile(r"""^\s*import\s+(?:[^'"]+\s+from\s+)?['"]([^'"]+)['"]""", re.M)


def main() -> int:
    if not OHOS.is_dir():
        print(f"missing {OHOS}", file=sys.stderr)
        return 1

    sources = sorted(OHOS.rglob("*.ets")) + sorted(OHOS.rglob("*.ts"))
    if not sources:
        print("no ArkTS/TS sources found", file=sys.stderr)
        return 1

    violations: list[tuple[Path, int, str]] = []
    kit_imports = 0
    for path in sources:
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in IMPORT_RE.finditer(text):
            module = match.group(1)
            if module.startswith("@kit."):
                kit_imports += 1
            if module.startswith(FORBIDDEN):
                line = text.count("\n", 0, match.start()) + 1
                violations.append((path, line, module))

    print(f"scanned {len(sources)} ArkTS/TS files")
    print(f"standard @kit.* imports: {kit_imports}")
    if violations:
        print("vendor-only imports are not allowed in the core HAP:", file=sys.stderr)
        for path, line, module in violations:
            print(f"  {path.relative_to(ROOT)}:{line}: {module}", file=sys.stderr)
        return 1

    print("OK: core HAP uses no vendor-only import namespace")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

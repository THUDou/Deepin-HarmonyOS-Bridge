#!/usr/bin/env python3
"""Validate the HarmonyOS project skeleton without DevEco Studio.

This catches the most common "it will not even load" mistakes: malformed JSON,
missing pages/icons, and missing permissions.
"""

from __future__ import annotations

import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OHOS = os.path.join(ROOT, "ohos")

REQUIRED = [
    "AppScope/app.json5",
    "AppScope/resources/base/element/string.json",
    "AppScope/resources/base/media/app_icon.png",
    "build-profile.json5",
    "hvigorfile.ts",
    "hvigor/hvigor-config.json5",
    "oh-package.json5",
    "entry/build-profile.json5",
    "entry/hvigorfile.ts",
    "entry/oh-package.json5",
    "entry/src/main/module.json5",
    "entry/src/main/ets/entryability/EntryAbility.ets",
    "entry/src/main/ets/pages/Index.ets",
    "entry/src/main/ets/core/Protocol.ets",
    "entry/src/main/ets/core/BridgeClient.ets",
    "entry/src/main/ets/core/Discovery.ets",
    "entry/src/main/resources/base/element/string.json",
    "entry/src/main/resources/base/element/color.json",
    "entry/src/main/resources/base/profile/main_pages.json",
    "entry/src/main/resources/base/media/app_icon.png",
]


def main() -> int:
    errors: list[str] = []
    for relative in REQUIRED:
        path = os.path.join(OHOS, relative)
        if not os.path.isfile(path):
            errors.append(f"missing: {relative}")

    json_files = []
    ignored_dirs = {"build", ".hvigor", "oh_modules", ".idea", ".preview"}
    for base, dirs, files in os.walk(OHOS):
        dirs[:] = [name for name in dirs if name not in ignored_dirs]
        for name in files:
            if name.endswith((".json", ".json5")):
                json_files.append(os.path.join(base, name))
    for path in json_files:
        try:
            with open(path, "r", encoding="utf-8") as handle:
                json.load(handle)
        except Exception as error:  # noqa: BLE001 - report the exact file
            errors.append(f"invalid JSON: {os.path.relpath(path, ROOT)}: {error}")

    icon = os.path.join(OHOS, "entry", "src", "main", "resources", "base", "media", "app_icon.png")
    if os.path.isfile(icon):
        with open(icon, "rb") as handle:
            if handle.read(8) != b"\x89PNG\r\n\x1a\n":
                errors.append("app_icon.png is not a PNG")

    hvigor_config = os.path.join(OHOS, "hvigor", "hvigor-config.json5")
    oh_package = os.path.join(OHOS, "oh-package.json5")
    if os.path.isfile(hvigor_config) and os.path.isfile(oh_package):
        with open(hvigor_config, "r", encoding="utf-8") as handle:
            hvigor_model = json.load(handle).get("modelVersion")
        with open(oh_package, "r", encoding="utf-8") as handle:
            package_model = json.load(handle).get("modelVersion")
        if hvigor_model != package_model:
            errors.append(
                f"modelVersion mismatch: hvigor={hvigor_model!r}, "
                f"oh-package={package_model!r}"
            )

    module = os.path.join(OHOS, "entry", "src", "main", "module.json5")
    if os.path.isfile(module):
        with open(module, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        permissions = {
            item.get("name") for item in data.get("module", {}).get("requestPermissions", [])
        }
        if "ohos.permission.INTERNET" not in permissions:
            errors.append("module.json5 is missing ohos.permission.INTERNET")

    if errors:
        for error in errors:
            print("ERROR:", error)
        return 1
    hap = os.path.join(
        OHOS, "entry", "build", "default", "outputs", "default",
        "entry-default-unsigned.hap",
    )
    if os.path.isfile(hap):
        print(f"HAP artifact: {os.path.relpath(hap, ROOT)} ({os.path.getsize(hap)} bytes)")
    else:
        print("HAP artifact: not built (run hvigorw assembleApp)")

    print(f"OHOS skeleton OK ({len(json_files)} JSON files, {len(REQUIRED)} required files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

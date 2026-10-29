#!/usr/bin/env python3
"""Validate the Deepin D-Bus contract without a D-Bus daemon or DTK SDK."""
from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XML = ROOT / "deepin" / "org.dhb.Bridge.xml"
IFACE = "org.dhb.Bridge"

REQUIRED_METHODS = {
    "GetStatus",
    "ListDevices",
    "GetReceiveDir",
    "SetReceiveDir",
    "Pair",
    "SendFile",
    "CancelTransfer",
}
REQUIRED_SIGNALS = {
    "DeviceFound",
    "TransferProgress",
    "TransferFinished",
    "StatusChanged",
}


def main() -> int:
    try:
        tree = ET.parse(XML)
    except (OSError, ET.ParseError) as error:
        print(f"ERROR: cannot parse {XML}: {error}")
        return 1

    root = tree.getroot()
    if root.tag != "node":
        print(f"ERROR: root element is {root.tag!r}, expected 'node'")
        return 1

    ifaces = [node for node in root.findall("interface") if node.get("name") == IFACE]
    if len(ifaces) != 1:
        print(f"ERROR: expected exactly one {IFACE} interface, got {len(ifaces)}")
        return 1
    iface = ifaces[0]

    methods = {node.get("name"): node for node in iface.findall("method")}
    signals = {node.get("name"): node for node in iface.findall("signal")}
    errors: list[str] = []

    for name in sorted(REQUIRED_METHODS - methods.keys()):
        errors.append(f"missing method: {name}")
    for name in sorted(REQUIRED_SIGNALS - signals.keys()):
        errors.append(f"missing signal: {name}")

    pair = methods.get("Pair")
    if pair is not None:
        args = [(arg.get("name"), arg.get("type"), arg.get("direction"))
                for arg in pair.findall("arg")]
        if ("psk", "s", "in") not in args:
            errors.append("Pair must take psk as a string input")
        if ("port", "q", "in") not in args:
            errors.append("Pair must take port as uint16 input")

    send = methods.get("SendFile")
    if send is not None:
        args = [(arg.get("name"), arg.get("type"), arg.get("direction"))
                for arg in send.findall("arg")]
        if ("path", "s", "in") not in args:
            errors.append("SendFile must take a path string input")

    if errors:
        for error in errors:
            print("ERROR:", error)
        return 1

    print(
        f"Deepin D-Bus contract OK: {len(methods)} methods, "
        f"{len(signals)} signals ({IFACE})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

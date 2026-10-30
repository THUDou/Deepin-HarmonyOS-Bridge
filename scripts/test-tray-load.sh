#!/usr/bin/env bash
# Build and run a QPluginLoader ABI check for the DTK6 tray plugin.
set -eu
ROOT="${1:-/root/dhb}"
CXX="${CXX:-g++}"

"$CXX" -std=c++17 -fPIC \
  -I/usr/include/dde-dock \
  "$ROOT/deepin/dhb-tray/load-check.cpp" \
  $(pkg-config --cflags --libs Qt6Core Qt6Widgets) \
  -o /tmp/dhb-tray-load-check

QT_QPA_PLATFORM=offscreen /tmp/dhb-tray-load-check \
  "$ROOT/build/deepin/dhb-tray/dhb-tray.so"

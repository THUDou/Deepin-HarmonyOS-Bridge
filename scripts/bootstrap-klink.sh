#!/usr/bin/env bash
# Clone and build Klink for local development. This script intentionally does
# not install system packages or run sudo; review the printed commands.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${KLINK_DIR:-$ROOT/third_party/klink}"

if [[ ! -d "$TARGET/.git" ]]; then
  echo "cloning Klink into $TARGET"
  mkdir -p "$(dirname "$TARGET")"
  git clone --depth 1 https://gitee.com/openkylin/klink.git "$TARGET"
else
  echo "using existing $TARGET"
fi

cat <<'DEPS'
Klink build dependencies (Debian/Deepin names):
  sudo apt install build-essential cmake pkg-config libssl-dev uuid-dev
  # optional Qt client:
  sudo apt install qtbase5-dev qttools5-dev libavformat-dev libswscale-dev \
                   libavcodec-dev libavutil-dev libffmpegthumbnailer-dev

Klink README says it is compatible with HarmonyOS soft bus 4.x. KaihongOS 5.x
compatibility is UNVERIFIED; see docs/06-klink-dsoftbus-notes.md.
DEPS

cd "$TARGET"
if [[ -f third_party/openssl/openssl_install.sh ]]; then
  echo
  echo "OpenSSL vendoring step (needs sudo, run manually):"
  echo "  sudo bash ./third_party/openssl/openssl_install.sh ./third_party/openssl amd"
  echo "  # or 'arm' on ARM64"
fi

echo
echo "Building Klink (no install):"
cmake -S . -B build -Dhost_system=ON
cmake --build build -j"$(nproc 2>/dev/null || echo 4)"

cat <<'NEXT'

Build finished (not installed).
To install and run:
  cd third_party/klink/build && sudo make install
  sudo vi /opt/dsoftbus/config/ifname.conf   # set your Wi-Fi/Ethernet interface
  sudo /opt/dsoftbus/bin/connectivity_softbus_server
  sudo /opt/dsoftbus/bin/connectivity_devicemgr_client

Then follow the M0 probe checklist in docs/06-klink-dsoftbus-notes.md.
NEXT

#!/usr/bin/env bash
# Build the KLink 4.x host stack from a clean upstream tree + the DHB lab patch.
#
# The build must live on a Linux filesystem: KLink's lninclude.sh creates
# symlinks, which do not work on a Windows bind mount (DrvFs).
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${KLINK_SRC:-$ROOT/third_party/klink}"
WORK="${KLINK_BUILD_DIR:-/tmp/klink-lab}"
PREFIX="${KLINK_PREFIX:-/opt/dsoftbus}"
PATCH="$ROOT/patches/klink-4.x-linux-lab.patch"
JOBS="${JOBS:-$(nproc)}"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "error: run this on Linux (container or host), not $(uname -s)" >&2
  exit 1
fi
if [[ ! -d "$SRC/.git" ]]; then
  echo "error: missing KLink git clone at $SRC" >&2
  echo "run scripts/fetch-softbus-sources.sh first" >&2
  exit 1
fi
if [[ ! -f "$PATCH" ]]; then
  echo "error: missing patch $PATCH" >&2
  exit 1
fi

echo "== export clean KLink $SRC -> $WORK"
rm -rf "$WORK"
mkdir -p "$WORK"
git -C "$SRC" archive HEAD | tar -x -C "$WORK"

echo "== apply $PATCH"
patch -d "$WORK" -p1 < "$PATCH"

# KLink ships lninclude.sh with CRLF; a Linux shell cannot parse it.
sed -i 's/\r$//' "$WORK/lninclude.sh"
chmod +x "$WORK/lninclude.sh"

echo "== configure (host_system=ON, no Qt client)"
cmake -S "$WORK" -B "$WORK/build" \
  -Dhost_system=ON -DAPPSERVER=OFF -DCMAKE_BUILD_TYPE=Release

echo "== build with $JOBS jobs"
cmake --build "$WORK/build" -j"$JOBS"

echo "== install to $PREFIX"
install -d "$PREFIX/bin" "$PREFIX/lib" "$PREFIX/config"
cp -a "$WORK/output/bin/." "$PREFIX/bin/"
cp -a "$WORK/output/lib/." "$PREFIX/lib/"
cp -a "$WORK/config/." "$PREFIX/config/"
printf 'eth0' > "$PREFIX/config/ifname.conf"

echo
echo "KLink host build complete:"
ls -l "$PREFIX/bin/connectivity_softbus_server" "$PREFIX/bin/connectivity_devicemgr_cmd"
echo "Next: start the server with scripts/klink-lab-server.sh"

#!/usr/bin/env bash
# End-to-end loopback demo: serve a file from one DHB process to another over
# real TCP, then verify the received bytes. Linux only.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${DHB_BIN:-$ROOT/build/dhb}"
PORT="${PORT:-47821}"
PSK="${PSK:-00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff}"

if [[ ! -x "$BIN" ]]; then
  echo "missing $BIN; run scripts/build-linux.sh first" >&2
  exit 1
fi

TMP="$(mktemp -d)"
SERVER_PID=""
cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

mkdir -p "$TMP/recv"
"$BIN" serve --port "$PORT" --psk "$PSK" --dir "$TMP/recv" >"$TMP/server.log" 2>&1 &
SERVER_PID=$!
sleep 0.7

echo "== server =="
cat "$TMP/server.log"

echo "== send =="
"$BIN" send --host 127.0.0.1 --port "$PORT" --psk "$PSK" "$ROOT/README.md"

sleep 0.5
if ! cmp -s "$ROOT/README.md" "$TMP/recv/README.md"; then
  echo "FAILED: received file differs" >&2
  echo "== server log ==" >&2
  cat "$TMP/server.log" >&2
  exit 1
fi

echo
echo "PASS: README.md transferred and verified byte-for-byte"
echo "recv: $TMP/recv/README.md"

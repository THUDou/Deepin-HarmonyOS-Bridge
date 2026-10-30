#!/usr/bin/env bash
# DHB D-Bus + TCP end-to-end smoke test. Run inside `dbus-run-session`.
set -u

ROOT="${1:-/root/dhb}"
DHB="$ROOT/build/dhb"
DAEMON="$ROOT/build/deepin/dhb-daemon/dhb-daemon"
PSK="00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
PORT=47831

[ -x "$DHB" ] || { echo "dhb not found: $DHB" >&2; exit 1; }
[ -x "$DAEMON" ] || { echo "daemon not found: $DAEMON" >&2; exit 1; }

rm -rf /tmp/dhb-serve /tmp/dhb-recv
mkdir -p /tmp/dhb-serve /tmp/dhb-recv
printf 'dhb dbus pair payload\n' >/tmp/dhb-send.txt

"$DHB" serve --port "$PORT" --dir /tmp/dhb-serve --psk "$PSK" \
  >/tmp/dhb-serve.log 2>&1 &
server_pid=$!
"$DAEMON" --recv-dir /tmp/dhb-recv >/tmp/dhb-daemon.log 2>&1 &
daemon_pid=$!
cleanup() {
  kill "$daemon_pid" "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
  if gdbus introspect --session --dest org.dhb.Bridge \
      --object-path /org/dhb/Bridge >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done
sleep 0.2

echo "--- Pair ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.Pair \
  "127.0.0.1" "$PORT" "$PSK"

echo "--- GetStatus ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.GetStatus

echo "--- ListDevices ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.ListDevices

echo "--- SendFile ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.SendFile \
  "/tmp/dhb-send.txt"

sleep 0.5
echo "--- received ---"
ls -l /tmp/dhb-serve
cat /tmp/dhb-serve/dhb-send.txt 2>/dev/null || true
echo "--- server log ---"
cat /tmp/dhb-serve.log
echo "--- daemon log ---"
cat /tmp/dhb-daemon.log

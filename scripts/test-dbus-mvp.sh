#!/usr/bin/env bash
# DHB D-Bus MVP smoke test. Run inside `dbus-run-session`.
set -u

ROOT="${1:-/root/dhb}"
DAEMON="$ROOT/build/deepin/dhb-daemon/dhb-daemon"
if [ ! -x "$DAEMON" ]; then
  echo "daemon not found: $DAEMON" >&2
  exit 1
fi

"$DAEMON" --recv-dir /tmp/dhb-recv >/tmp/dhb-daemon.log 2>&1 &
pid=$!
cleanup() { kill "$pid" 2>/dev/null || true; }
trap cleanup EXIT

for _ in $(seq 1 50); do
  if gdbus introspect --session --dest org.dhb.Bridge \
      --object-path /org/dhb/Bridge >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

echo "--- daemon log ---"
cat /tmp/dhb-daemon.log

echo "--- introspect ---"
gdbus introspect --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge | sed -n '1,60p'

echo "--- GetStatus ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.GetStatus

echo "--- ListDevices ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.ListDevices

echo "--- SetReceiveDir ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.SetReceiveDir "/tmp/dhb-recv2"

echo "--- GetReceiveDir ---"
gdbus call --session --dest org.dhb.Bridge \
  --object-path /org/dhb/Bridge --method org.dhb.Bridge.GetReceiveDir

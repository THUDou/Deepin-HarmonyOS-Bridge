#!/bin/bash
# Inspect the KLink lab server logs/processes without PowerShell quoting issues.
set -u
ps -eo pid,comm,args | grep -F connectivity_softbus | grep -v grep || true
echo '--- ifname ---'
grep 'network interface name' /tmp/klink-server.log | head -2 || true
echo '--- tail ---'
tail -20 /tmp/klink-server.log 2>/dev/null || true
echo '--- sockets ---'
if command -v ss >/dev/null 2>&1; then
  ss -lunp | head -20
elif command -v netstat >/dev/null 2>&1; then
  netstat -lunp | head -20
else
  echo 'no ss/netstat'
fi

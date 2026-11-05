#!/bin/bash
# Drive the KLink device-manager CLI once: discover, list, exit.
set -u
export LD_LIBRARY_PATH=/opt/dsoftbus/lib/base:/opt/dsoftbus/lib/dsoftbus:/opt/dsoftbus/lib/devicemgr:/opt/dsoftbus/lib/third_party
cd /opt/dsoftbus
{
  sleep 8
  printf '1\n'
  sleep 60
  printf '1\n'
  sleep 10
  printf '9\n'
} | timeout 180 ./bin/connectivity_devicemgr_cmd >/tmp/klink-client.log 2>&1
echo "CLIENT_RC:$?"

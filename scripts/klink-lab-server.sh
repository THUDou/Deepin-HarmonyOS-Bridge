#!/bin/bash
# Start the KLink 4.x softbus server inside the Linux lab container.
set -eu
export LD_LIBRARY_PATH=/opt/dsoftbus/lib/base:/opt/dsoftbus/lib/dsoftbus:/opt/dsoftbus/lib/devicemgr:/opt/dsoftbus/lib/third_party
cd /opt/dsoftbus
nohup ./bin/connectivity_softbus_server >>/tmp/klink-server.log 2>&1 &
echo $! >/tmp/klink-server.pid

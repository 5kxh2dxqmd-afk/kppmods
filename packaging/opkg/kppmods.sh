#!/bin/sh
# Name: KPP Mods
# Author: KPP Mods
# kppmods package launcher
set -eu
mkdir -p /var/run/kppmods
# Let the scriptlet launcher return before activation restarts KPP.
setsid /bin/sh -c 'sleep 2; exec /bin/sh /var/local/kindle-opkg/run /var/run/eink-sdk/usr/bin/kppmods-start' \
    </dev/null >/var/run/kppmods/start.log 2>&1 &

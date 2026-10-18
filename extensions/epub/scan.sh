#!/bin/sh
set -eu
# A framework restart stops the scanner. Ensure its existing job is running
# before requesting discovery, including devices still in setup.
if ! /sbin/initctl status scanner | grep -q 'start/running'; then
    /sbin/initctl start scanner
fi
attempt=0
until lipc-get-prop com.lab126.scanner fullScanStatus >/dev/null 2>&1; do
    [ "$attempt" -lt 30 ] || { echo 'Scanner unavailable' >&2; exit 1; }
    sleep 1
    attempt=$((attempt + 1))
done
lipc-set-prop com.lab126.scanner doFullScan 1

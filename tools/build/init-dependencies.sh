#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
jobs=${KPPMODS_SUBMODULE_JOBS:-4}
git -C "$source_dir" submodule update --init --depth 1 --jobs "$jobs" -- \
    vendor/boost vendor/openlipc vendor/utfcpp vendor/hermes-dec vendor/react-kindle
boost_dir="$source_dir/vendor/boost"
git -C "$boost_dir" submodule update --init --depth 1 tools/boostdep
(
    cd "$boost_dir"
    for library in json describe asio process; do
        python3 tools/boostdep/depinst/depinst.py --update --exclude test \
            --git_args "--depth 1 --jobs $jobs" "$library"
    done
)
echo "Upstream submodules initialized"

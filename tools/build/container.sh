#!/bin/sh
# Run the standard build tools in Linux; all paths passed to the command are container paths.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
set -- kppmods-build "$@"
if [ -n "${KPPMODS_SDK_ROOT:-}" ]; then
    set -- -v "$KPPMODS_SDK_ROOT:/inputs/sdk:ro" "$@"
fi
if [ -n "${KPPMODS_PATCHELF_DIR:-}" ]; then
    set -- -v "$KPPMODS_PATCHELF_DIR:/inputs/patchelf:ro" "$@"
fi
exec docker run --rm --platform linux/amd64 \
    -v "$source_dir:/src" \
    -v kppmods-node-modules:/src/node_modules \
    -v "${KPPMODS_TOOLCHAIN_ROOT:?Set KPPMODS_TOOLCHAIN_ROOT}:/inputs/toolchain:ro" \
    -e KPPMODS_TOOLCHAIN_ROOT=/inputs/toolchain "$@"

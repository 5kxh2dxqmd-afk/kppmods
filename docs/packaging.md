# Package an extension

Use the [release build](building.md) for the selected package set. This guide is
for staging one extension, supplying extra native payloads, or building an IPK
outside that release set. Run commands from the repository root.

## Stage the payload

Install host npm dependencies first. The output must be a new directory:

```sh
python3 tools/packaging/package.py launcher build/staged/launcher
```

For source outside this repository:

```sh
python3 tools/packaging/package.py my-mod build/staged/my-mod \
  --source /path/to/my-mod
```

Staging copies assets and the manifest, compiles UI entries and local imports,
and runs the package's optional staging hook. Native source, `node_modules`, Git
metadata and `stage.py` are excluded from the output.

Some bundled packages need additional inputs:

| Package        | Staging input                                                                                         |
| -------------- | ----------------------------------------------------------------------------------------------------- |
| `ui-host`      | `--toolkit build/kindlehf/ui/toolkit.js`; or build it with `npm run build:toolkit -- /tmp/toolkit.js` |
| `status-items` | `rsvg-convert` on PATH, or `--rsvg-convert /path/to/rsvg-convert`                                     |
| `epub`         | `--epub-runtime /path/to/package-native`; see below                                                   |

Native extension libraries are installed by CMake. To assemble one complete
native extension, build and install its component:

```sh
cmake --build --preset kindlehf --target kppmods-extension-statusbar
cmake --install build/kindlehf --prefix /tmp/kppmods --component extension-statusbar
```

The payload is `/tmp/kppmods/usr/lib/kppmods/extensions/org.kppmods.statusbar`.
Use `extension-fake-registration` for Fake Registration. Core installation
provides the shared third-party licenses. Set an extension's version in its
manifest; staging and release builds preserve it unchanged.

## Build one IPK

Run the IPK builder inside a Linux environment with the cross-toolchain and SDK
host tools available. It needs the same ELF policy as the release build. For hard-float devices:

```sh
export OPKG_ARCH=kindlehf
export OPKG_READELF=/path/to/toolchain/bin/arm-kindlehf-linux-gnueabihf-readelf
export OPKG_STRIP=/path/to/toolchain/bin/arm-kindlehf-linux-gnueabihf-strip
export OPKG_PATCHELF=/path/to/eink-sdk/host/bin/patchelf
export OPKG_FIRMWARE_INTERPRETERS=/lib/ld-linux-armhf.so.3
export OPKG_FIRMWARE_LIBRARIES='libdl.so.2 libpthread.so.0 librt.so.1 libm.so.6 libc.so.6 libstdc++.so.6 libgcc_s.so.1'
export OPKG_FIRMWARE_VERSION_LIMITS='libstdc++.so.6:GLIBCXX_3.4.28:CXXABI_1.3.12'
export OPKG_FIRMWARE_VERSION_TAGS='libstdc++.so.6:CXXABI_ARM_1.3.3 libstdc++.so.6:CXXABI_TM_1'
python3 tools/packaging/ipk.py extension build/staged/launcher /tmp/ipks \
  --sdk build/kindlehf/sdk --version 2.0.0-r23
```

`build/kindlehf/sdk` must have been populated by a release build. It contains the pinned
packager and the ELF strip-order patch. For another target, use that target's
architecture, compiler tools and complete `elf_policy` from `release.json`.

Extension IPKs take their version from the staged manifest; `--version` controls
core packages. Additional opkg dependencies use repeated `--depends NAME` flags.
[Deploy the IPK](runtime.md#install-packages) together with any missing dependencies.

## Package-owned build and install steps

An optional `packaging/stage.py` exports `stage(context)`. The context contains
`source`, `output`, `repository` and parsed build `options`. Use it to copy or
generate payloads; the generic packager handles UI compilation.

Optional `packaging/postinst`, `prerm` and `postrm` files contain shell hook bodies.
They receive `package` (installed path), `id` and `reload` variables. Without custom
hooks, installation and removal request a runtime reload. Upgrade removal skips
postrm activation so the new package owns the reload.

`packaging/*.conf` files install as Upstart definitions. Use these for persistent
services; foreground commands launched by the broker are terminated when their
command lifetime ends. Packaging metadata is consumed by the IPK builder and is
not installed in the extension's payload directory.

## EPUB runtime

EPUB is excluded from the default release. Build its native payload with
`kindle-epub/scripts/build-kpp-runtime.sh` in the separate Kindle EPUB project.
Use a rootfs with the target's ARM calling convention, and set `JAVA_HOME` or
`EPUB_JDK_INCLUDE_DIR` for JNI headers. The runtime needs foreground
`kindle-epub-import --serve` support.

```sh
python3 tools/packaging/package.py epub build/staged/epub \
  --epub-runtime /path/to/package-native --architecture kindlepw2
```

Use `--architecture kindlehf` for hard float (also the default). Staging validates
the ELF calling convention and includes the C++ ABI variants present in the runtime.
When packaging the IPK, add `libxml2.so.2 libz.so.1` to the target's
`OPKG_FIRMWARE_LIBRARIES`; keep its interpreter and C++ version limits.

EPUB uses Java/WebReader where KPP has no embedded reader. Its optional KPP preload
is selected only when the firmware exports the reader's extension parser.

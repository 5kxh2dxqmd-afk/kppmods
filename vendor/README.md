# Upstream dependencies

Upstream source lives in pinned Git submodules. `.gitmodules` records public HTTPS URLs; the parent repository's gitlinks select exact revisions.

| Path | Upstream | Pinned source |
| --- | --- | --- |
| `boost/` | [Boost](https://github.com/boostorg/boost) | `boost-1.92.0` |
| `utfcpp/` | [UTF8-CPP](https://github.com/nemtrif/utfcpp) | `v4.1.1` |
| `openlipc/` | [OpenLIPC](https://github.com/arkq/openlipc) | `91645e7` |
| `hermes-dec/` | [hermes-dec](https://github.com/P1sec/hermes-dec) | `0.1.7` |
| `react-kindle/` | [React Kindle](https://github.com/katadelos/react-kindle) | `158ba72` |

Initialize dependencies after cloning or changing their pins:

```sh
tools/build/init-dependencies.sh
```

The script initializes the parent submodules, then runs Boost's dependency installer for JSON, Describe, Asio and Process. It fetches the corresponding headers and sources, excluding dependencies used only by tests. `KPPMODS_SUBMODULE_JOBS` controls Git parallelism (default 4). The native build performs no downloads. Browser-only development can initialize `vendor/react-kindle` directly with `git submodule update --init vendor/react-kindle`.

Update an upstream by checking out its desired revision and staging its submodule path in the parent repository. Run the initialization script after updating Boost so its nested dependencies match the release. Keep submodule checkouts unmodified; project adjustments belong outside them.

## Native builds

Boost's modular include directories supply the header dependency closure. `src/common/json.cpp` compiles JSON separately for each Kindle string ABI. Process sources come from `boost/libs/process/src` and use the C++ standard filesystem. UTF8-CPP supplies command-output sanitization.

`cmake/patches/openlipc.patch` preserves the Kindle hash-array callback and dynamic-allocation declarations. CMake applies it to a header copy in the build directory, leaving `openlipc/include/openlipc.h` unchanged. No proprietary LIPC library is redistributed.

The core package includes Boost, UTF8-CPP, OpenLIPC and hermes-dec licenses from their submodules. Status-bar and fake-registration packages include the OpenLIPC license.

## Hermes tooling

Development tools import hermes-dec from `hermes-dec/src`; its Python package is not installed on the device. `tools/hermes/generate-tables.py` derives the checked-in native opcode and header metadata in `src/runtime/hermes/tables.inc`. The manager uses that metadata to discover Metro modules directly. The pinned parser contains the packed Microsoft layout declarations required by Python 3.14 and the unchanged version 95 opcodes for bytecode version 96.

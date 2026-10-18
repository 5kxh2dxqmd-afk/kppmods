# Runtime internals

Read this when changing the host, activation logic or firmware adapters. Extension
authors can usually work through the [UI API](extension-api.md) and
[manifest](manifest.md) without modifying these components.

## Source map

| Change                                              | Start here                                                        |
| --------------------------------------------------- | ----------------------------------------------------------------- |
| Management requests, settings, process activation   | `src/runtime/`; `broker.cpp`, `service.cpp`, `configuration.cpp`  |
| CLI commands                                        | `src/runtime/main.cpp`                                            |
| Injected host, JS evaluation, native plugin loading | `src/host/`                                                       |
| Shared records and JSON conversion                  | `src/common/`                                                     |
| Extension lifecycle, contributions, shared services | `ui/host/`                                                        |
| Stock tabs, menus and Settings integration          | `ui/adapters/`                                                    |
| Extension-facing React hooks                        | `ui/sdk/`                                                         |
| A bundled feature                                   | Its directory under `extensions/`                                 |
| Native plugin ABI                                   | `include/kppmods/kppmods.h`                                       |
| Build selection and packaging                       | `CMakePresets.json`, `cmake/`, `release.json`, `tools/packaging/` |
| Boot integration                                    | `packaging/opkg/`                                                 |
| Firmware discovery                                  | `src/runtime/hermes/`, `tools/hermes/`                            |

Internal headers stay beside their implementations. CMake targets own native
build dependencies; `include/kppmods` contains the extension interface.
The runtime uses C++20 and statically links its C++ runtime. Injected libraries
use C++17 and the firmware's shared runtime, with both string ABI variants.
Dependency pins and header adjustments are in [vendor documentation](../vendor/README.md).

## Activation

opkg installs payloads in `/mnt/us/.eink-sdk`. Runtime links expose them through
`/var/run/eink-sdk`. KPP Mods stores settings and generated configuration in
`/mnt/us/.eink-sdk/var/lib/kppmods`. Uploaded development bundles share that
persistent directory. Locks, native diagnostics, app logs and live-update messages
use `/var/run/kppmods`; the Upstart overlay uses `/var/run/kppmods-upstart`.

The broker owns package discovery, enabled state, settings and one typed runtime
configuration. CLI and UI management requests use that same owner. Activation
validates dependencies, discovers firmware modules and publishes a configuration
snapshot before restarting affected stock jobs. Requests share the broker lock;
job restarts run asynchronously so status and command requests can still be served.

Activation records pending jobs before publishing its configuration. Jobs remain
pending after a failure or runtime restart; the next activation retries them, even
when the desired configuration is unchanged. `kppmod status` exposes
`pending_jobs`. `kppmod configuration` returns the current desired configuration;
this is not evidence that every affected process has restarted. Restart comparison
uses each process's resolved libraries, scripts and options. KPP also consumes the
package summaries and app entries exposed by the synchronous UI APIs; changes to
those fields require a UI restart. Native-only schemas and descriptions do not.

`kppmods-start` prepares a RAM-backed overlay of stock Upstart definitions.
Activation writes preload and environment settings there. Normal job starts then
load the selected extensions. Native libraries are replaced by restarting their
processes, not by unloading them from a live process.

The `ui-host` package supplies `host.js` and `toolkit.js`. These load before
extension entries. Host adapters own stock component interception and dispatch
contributions from extensions. Native plugins use the C interface for logging and
exclusive hook claims. Extensions share process authority; this is not a sandbox.

## Local IPC

`kppmod` handles both CLI commands and service mode. Without `--config`, it is a
client of the running service. `scan BUNDLE` is the exception: it runs discovery locally without
the broker. `--config CONFIG_JSON` starts the service.

The broker registers LIPC service `org.kppmods`. The `request` hash-array property
accepts a hash with a `json` string containing `{operation, body}` and returns a
hash with the JSON result. Failures return `{error: message}`. A `changed` event
prompts clients to fetch `state`.

Operations include `health`, `state`, `management`, `managementResult`, `exec`,
`command`, `action`, `settings`, `status`, `events`, `development` and
`developmentResult`. See [`broker.cpp`](../src/runtime/broker.cpp) for dispatch and
the corresponding implementation files for request fields.

```sh
kppmod --request health < /dev/null
printf '%s' '{"command":"list"}' | kppmod --request management
```

The UI uses a marked call through the stock Networking native module and receives
`kppmods:response` and `kppmods:changed` events. This transport does not open a
network socket. It is intended for trusted local extensions and tools.

## Firmware discovery

Activation scans `/app/KPPMainApp/js/KPPMainApp.js.hbc` using the native Hermes
scanner. It follows Metro registrations and matches semantic signatures. Missing
or ambiguous React/React Native matches stop activation. Optional adapters check
resolved export shapes before patching stock components.

Inspect a device bundle without changing runtime state:

```sh
kppmod scan /app/KPPMainApp/js/KPPMainApp.js.hbc
```

For offline investigation on the development machine:

```sh
python3 tools/hermes/resolve-modules.py /tmp/KPPMainApp.js.hbc \
  --output /tmp/modules.json
python3 tools/hermes/inspect-gates.py /tmp/KPPMainApp.js.hbc \
  --disassembler /path/to/hbc-disassembler \
  --device scribe3 --firmware 5.20.1 --output /tmp/kpp-gates.json
```

The gate inspector inventories registration references, capability checks and
experiment candidates. It also accepts disassembler text. Use repeated `--symbol`
arguments to select names, and `--include-bodies` to include bytecode. Supply device
and firmware labels from the bundle's source device, including
`/etc/prettyversion.txt`. IDs belong to that bundle; a string match does not prove
that a gate controls a feature. Check callers and conditions before writing an adapter.

Keep extracted firmware and investigation output outside the repository. The
Python tools are development tools; on-device discovery needs no Python and has
no persistent module cache or firmware-specific map.

After changing hermes-dec or `tools/hermes/signatures.py`, regenerate the metadata:

```sh
python3 tools/hermes/generate-tables.py
```

This writes `src/runtime/hermes/tables.inc`. Unknown bytecode versions require a
metadata update before activation can proceed.

## Local checks

```sh
npm ci
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/pre-commit install
```

Commit hooks check first-party JS, Python, C/C++, Markdown, JSON and YAML. Vendored
source and generated Hermes tables are excluded. Run `npm run lint` for JavaScript
or `.venv/bin/pre-commit run --all-files` for the full set of checks. For a native
or firmware change, build the relevant target and verify activation and the affected
feature on a device or emulator; the browser cannot exercise native hooks.

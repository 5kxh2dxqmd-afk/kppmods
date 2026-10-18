# Edit an extension on a Kindle

Use this workflow after the package is [installed and enabled](runtime.md).
It replaces UI code in the running KPP process and keeps persisted settings.
You need SSH access, host npm dependencies and the installed runtime service.

The examples use an SSH config entry named `kindle` that connects as root. For an
emulator, replace `--host kindle` with its connection options on **every** command:

```sh
--host root@127.0.0.1 --port 2494 --identity /path/to/device-key
```

Use the port assigned to your emulator. `--known-hosts FILE` selects a host-key
file. The tool obtains the persistent data directory from the running runtime.
Uploaded bundles live in its `development/` subdirectory; update requests and
acknowledgements use `/var/run/kppmods/development/`. The session and uploaded
bundles survive reboot. Launch the **KPP Mods** scriptlet to resume the runtime,
then continue with `push` or `watch`; another `start` is not needed.

## Start a session

From the repository root:

```sh
python3 tools/device/dev-device.py start --host kindle
python3 tools/device/dev-device.py watch extensions/launcher --host kindle
```

The installed runtime owns session configuration and restarts KPP once, using
the same activation path as package changes. The watcher then builds and uploads each edit, disposes the previous
extension and activates the replacement. Wait for the `applied in ... ms` message;
it confirms JavaScript acknowledgement, not completion of an e-ink refresh.

Component state resets on every update. Settings written through the extension
API remain. Other enabled packages and native preloads stay active.

For one update without a watcher:

```sh
python3 tools/device/dev-device.py push extensions/launcher --host kindle
```

## End the session

Ctrl-C stops the watcher, but leaves development mode running on the device.
Restore the installed packages with:

```sh
python3 tools/device/dev-device.py stop --host kindle
```

This restarts KPP. Development uploads are temporary; [build and install an IPK](building.md)
to retain the change. Stop the session before installing, removing or reloading
packages.

## What can be replaced?

The package must already be enabled and have one UI entry.

| Live update                                                | Build, install and restart                                                                    |
| ---------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| JSX, local imports, assets, title, options and app entries | Native libraries, native action declarations, dependencies, settings schemas, host or toolkit |

This is whole-extension replacement. React component state is not preserved.
Updating a shared JavaScript service does not reactivate its consumers; update
those packages too or restart the session.

## When an update fails

- **Build error:** fix the compiler error; the running extension remains active.
- **Activation error:** the host attempts to restore the previous extension. Its component state resets.
- **Render error:** inspect the `ui:error` event and fix the component; the next accepted update can remount it.
- **No acknowledgement:** check `kppmod status` and `/var/log/messages` as described in [runtime diagnostics](runtime.md#diagnose-a-failure).

Dispose timers and subscriptions through `extension.onDispose` or the cleanup
returned by `activate`. The host cannot undo arbitrary side effects from your code.

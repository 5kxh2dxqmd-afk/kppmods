# Browser workbench

The workbench runs extension JSX with the React Kindle toolkit and simulated
device services. Use it for layout, settings and action handling before testing
on a Kindle. No firmware image or SDK is needed.

## Run it

From the repository root, with Node 22.18+ on 22.x or Node 24.11+:

```sh
git submodule update --init vendor/react-kindle
npm ci
npm run dev
```

Open **http://127.0.0.1:4173**. The default session includes the launcher, extension
manager, Quick Settings, status items and system tools. The component gallery
runs the real toolkit source.

For a new package, follow [Write a UI extension](../docs/extensions.md).

## Select packages and a layout

```sh
npm run dev -- --package launcher --package quick-settings --profile kt6
```

| Option                       | Purpose                                                                      |
| ---------------------------- | ---------------------------------------------------------------------------- |
| `--package ID`               | Select a package; repeat for multiple packages                               |
| `--packages-dir PATH`        | Read package directories from another location                               |
| `--profile pw5\|kt6\|scribe` | Initial simulated layout; default `pw5`                                      |
| `--port PORT`                | HTTP port; default `4173`                                                    |
| `--toolkit PATH`             | Use another React Kindle checkout; also settable through `REACT_KINDLE_PATH` |

Required packages must exist in the same packages directory. The loader follows
`needs` order and rejects missing dependencies and cycles. The pinned toolkit in
`vendor/react-kindle` is bundled from source without its own npm install.

The sidebar changes layout, scale and toolkit firmware presentation. Layout
profiles approximate device proportions. The firmware selector changes toolkit
styling, not the set of simulated native capabilities.

## Edit and debug

Save an entry, import, asset or manifest to rebuild and reload the page. Component
state resets; saved settings and the selected surface persist. A build error
appears over the preview and clears on a successful rebuild. Restart the server
after changing the preview server itself.

The inspector records activation, setting writes, actions, navigation and errors.
Expand an event to inspect its data. For native actions, choose success, failure
or timeout to exercise UI handling: the browser displays the command but never
executes it. JavaScript actions run normally.

`useSetting` persists in browser local storage. **Reset saved settings** restores
manifest defaults. These settings never reach a device, and the simulated broker
does not perform the native broker's validation.

Package image imports and `extension.asset()` use the local asset server. The
bundled `status-items` package substitutes its source SVG for the PNG generated
by device packaging. Other generated assets must be supplied by their build step.

## What the browser does not test

- Stock Metro adapters, native patches, root commands and real LIPC behavior.
- E-ink refresh, window placement, keyboard behavior and native accessibility.
- Exact fonts, native text measurement, pagination or Pango rendering.
- EPUB/native reader rendering.

The **Capabilities** view describes this simulation, not firmware compatibility.
Unsupported managers and direct stock module access fail explicitly. Right-click
simulates long press; scrolling and some specialized controls use browser
approximations. Use [live device development](../docs/development.md) to check the
actual renderer and firmware integration.

## Workbench source

| File          | Responsibility                                                   |
| ------------- | ---------------------------------------------------------------- |
| `server.mjs`  | Dependency loading, bundles, file watching and asset serving     |
| `native.jsx`  | React Native Web bridge and native control simulations           |
| `runtime.js`  | Shared extension contexts, simulated services and local settings |
| `app.jsx`     | Device shell, surfaces and inspector                             |
| `gallery.jsx` | Toolkit examples                                                 |

Generated bundles stay in memory. Workbench files are not included in device packages.

The `capabilities` URL parameter selects simulated stock capabilities. Use
`?capabilities=` to exercise extensions without stock Settings components; omit
it for the default Settings capabilities. Applying enabled-state changes disposes
extensions and reloads the preview. The selection lasts for the browser session.

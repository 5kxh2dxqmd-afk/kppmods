# Write a UI extension

Build a **Reading tools** tab with a button that changes and saves its text size.
You need the [browser workbench](../README.md#start-developing) dependencies installed.
Run all commands from the repository root.

## Create the package

```sh
mkdir -p extensions/reading-tools
```

Create `extensions/reading-tools/manifest.json`:

```json
{
  "schema_version": 2,
  "id": "reading-tools",
  "version": "1.0.0",
  "title": "Reading tools",
  "needs": ["ui-host"],
  "ui": ["main.jsx"],
  "settings": { "fontSize": 20 }
}
```

`ui-host` supplies React and the shared [React Kindle toolkit](../vendor/react-kindle).
The host calls `activate` from each file in `ui` when the package loads.

Create `extensions/reading-tools/main.jsx`:

```jsx
import { KindleProvider, Button, Text } from 'react-kindle';
import { useExtension, useSetting } from '@kppmods/react';

function ReadingTools() {
  const extension = useExtension();
  const [fontSize, setFontSize] = useSetting('fontSize', 20);

  return (
    <KindleProvider firmware={extension.firmware} applicationId="com.lab126.KPPMainApp">
      <Text size={fontSize}>Reading tools</Text>
      <Button onPress={() => setFontSize(fontSize + 1)}>Larger text</Button>
    </KindleProvider>
  );
}

export function activate(extension) {
  extension.tabs.add({ id: 'tools', title: 'Reading tools', component: ReadingTools });
}
```

Use toolkit or React Native components here. Browser elements such as `div` and
`button` will not run in Kindle's renderer.

## Try it in the browser

Stop any running workbench, then select the package:

```sh
npm run dev -- --package reading-tools
```

Open **http://127.0.0.1:4173** and select **Reading tools** in the footer. Press
**Larger text**, then reload the page: the size should be retained. The browser
stores this setting locally; device settings are separate.

Edit the title or component and save. The workbench rebuilds and reloads the page.
Component state resets on reload; values written with `useSetting` persist.
Use the inspector to see setting writes and activation errors.

The host removes registered tabs when the extension stops. For resources you
create yourself, return a cleanup function from `activate` or register one with
`extension.onDispose`:

```js
export function activate(extension) {
  const timer = setInterval(() => extension.log('Still running'), 10000);
  return () => clearInterval(timer);
}
```

## Compile it for Kindle

This step needs Python 3 as well as the JavaScript dependencies. It does not need
a cross-compiler for this UI-only package.

```sh
python3 tools/packaging/package.py reading-tools build/staged/reading-tools
```

The output directory must not already exist. The staged package contains
`main.js` and a manifest pointing to it. Local imports are bundled; React and the
toolkit are supplied by the device host. Asset imports remain package files.

To include it in a release, add `reading-tools` to the `extensions` array in
[`release.json`](../release.json), then follow [Build packages](building.md).
Install the core, `ui-host` and your extension using the [deployment guide](runtime.md).
Once installed and enabled, use [live development](development.md) for further edits.

## Where to go next

- [Extension API](extension-api.md): add Settings rows, actions or Quick Settings controls.
- [Manifest reference](manifest.md): declare actions, dependencies and native code.
- [Workbench options](../preview/README.md): load several packages or an external source directory.

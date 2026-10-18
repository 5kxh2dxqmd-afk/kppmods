#!/usr/bin/env node
// Compile a normal React module into the entry script expected by the native host.
// The wrapper binds the SDK to this package; imports of React and the toolkit
// resolve to the host's shared instances, never copies from node_modules.
const fs = require('fs');
const path = require('path');
const esbuild = require('esbuild');
const { root, sharedRuntime, compileForHermes } = require('./shared-runtime.cjs');

const [entry, packageId, output, sourceRoot] = process.argv.slice(2);
if (!entry || !packageId || !output) {
  throw new Error(
    'Usage: node tools/build/build-extension.cjs ENTRY PACKAGE_ID OUTPUT [PACKAGE_ROOT]'
  );
}
const packageRoot = fs.realpathSync(
  sourceRoot ? path.resolve(sourceRoot) : path.dirname(path.resolve(entry))
);
const sharedModules = {
  react: 'KPPMods.stock("react")',
  'react-native': '__kppmods_api__.RN',
  'react-kindle': '__kppmods_api__.toolkit',
  '@kppmods/toolkit': '__kppmods_api__.toolkit'
};

async function build() {
  const result = await esbuild.build({
    entryPoints: [entry],
    bundle: true,
    write: false,
    format: 'iife',
    globalName: 'extensionModule',
    jsx: 'automatic',
    target: 'es2015',
    minify: false,
    plugins: [
      sharedRuntime(sharedModules),
      {
        name: 'kindle-shared-runtime',
        setup(build) {
          build.onResolve({ filter: /^@kppmods\/react$/ }, () => ({
            path: path.join(root, 'ui/sdk/react.js')
          }));
          build.onResolve({ filter: /\.(svg|png|jpe?g|gif)$/ }, (args) => {
            const absolutePath = fs.realpathSync(
              path.resolve(args.resolveDir, args.path)
            );
            const relativePath = path.relative(packageRoot, absolutePath);
            if (
              relativePath === '..' ||
              relativePath.startsWith('..' + path.sep) ||
              path.isAbsolute(relativePath)
            ) {
              throw new Error(
                'Extension assets must belong to the package: ' + args.path
              );
            }
            if (!fs.statSync(absolutePath).isFile())
              throw new Error('Missing asset: ' + args.path);
            return { path: relativePath, namespace: 'kindle-asset' };
          });
          build.onLoad({ filter: /.*/, namespace: 'kindle-asset' }, (args) => ({
            contents:
              'module.exports = __kppmods_api__.asset(' +
              JSON.stringify(args.path) +
              ');',
            loader: 'js'
          }));
        }
      }
    ]
  });
  // The older supported Hermes runtimes consume ES5 source at this injection
  // point. This affects generated output only; extension sources stay modern JS.
  const compiled = compileForHermes(result.outputFiles[0].text);
  fs.writeFileSync(
    output,
    'KPPMods.define(' +
      JSON.stringify(packageId) +
      ', function (__kppmods_api__) {\n' +
      compiled +
      '\n' +
      'if (typeof extensionModule.activate !== "function") {\n' +
      '  throw new Error("Extension must export activate(extension)");\n' +
      '}\nreturn extensionModule.activate(__kppmods_api__);\n});\n'
  );
}
build().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});

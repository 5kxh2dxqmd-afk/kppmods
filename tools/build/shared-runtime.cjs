const path = require('node:path');
const fs = require('node:fs');
const babel = require('@babel/core');
const root = path.resolve(__dirname, '../..');

function toolkitSource(
  checkout = process.env.REACT_KINDLE_PATH || path.join(root, 'vendor/react-kindle')
) {
  const source = path.resolve(checkout, 'packages/react-kindle/src');
  if (!fs.existsSync(path.join(source, 'index.js')))
    throw new Error(
      'Initialize vendor/react-kindle with git submodule update --init, or select a checkout with --toolkit or REACT_KINDLE_PATH'
    );
  return source;
}

// Shared imports must use Kindle's runtime, never a second bundled React instance.
function sharedRuntime(modules) {
  return {
    name: 'kindle-shared-runtime',
    setup(build) {
      build.onResolve(
        { filter: /^(react|react-native|react-kindle|@kppmods\/toolkit)$/ },
        (args) => {
          if (!Object.hasOwn(modules, args.path))
            throw new Error('Unshared runtime dependency: ' + args.path);
          return { path: args.path, namespace: 'kindle-runtime' };
        }
      );
      build.onLoad({ filter: /.*/, namespace: 'kindle-runtime' }, (args) => ({
        contents: 'module.exports = ' + modules[args.path] + ';',
        loader: 'js'
      }));
      build.onResolve({ filter: /^react\/jsx-runtime$/ }, () => ({
        path: path.join(root, 'ui/sdk/jsx-runtime.js')
      }));
    }
  };
}
function compileForHermes(source) {
  return babel.transformSync(source, {
    babelrc: false,
    configFile: false,
    presets: [
      [
        require.resolve('@babel/preset-env'),
        { targets: { ie: '11' }, modules: false, useBuiltIns: false }
      ]
    ],
    comments: true,
    compact: false
  }).code;
}
module.exports = { root, toolkitSource, sharedRuntime, compileForHermes };

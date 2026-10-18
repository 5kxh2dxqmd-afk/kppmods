#!/usr/bin/env node
const fs = require('node:fs');
const path = require('node:path');
const esbuild = require('esbuild');
const { root, compileForHermes } = require('./shared-runtime.cjs');
const output = process.argv[2];
if (!output) throw new Error('Usage: node tools/build/build-host.cjs OUTPUT');
const result = esbuild.buildSync({
  stdin: {
    contents: 'require("./index")(kppGlobal);',
    resolveDir: path.join(root, 'ui/host')
  },
  bundle: true,
  write: false,
  format: 'iife',
  target: 'es2015',
  loader: { '.js': 'jsx' }
});
fs.writeFileSync(
  output,
  '(function (kppGlobal) {\n' +
    compileForHermes(result.outputFiles[0].text) +
    '\n})(this);\n'
);

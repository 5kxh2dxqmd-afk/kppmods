#!/usr/bin/env node
// A loopback-only development server. Bundles stay in memory, assets stay local.
import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';
import { parseArgs } from 'node:util';
import * as esbuild from 'esbuild';

const require = createRequire(import.meta.url);
const { toolkitSource } = require('../tools/build/shared-runtime.cjs');
const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.dirname(directory);
const { values } = parseArgs({
  options: {
    package: { type: 'string', multiple: true },
    'packages-dir': { type: 'string', default: path.join(root, 'extensions') },
    toolkit: { type: 'string' },
    port: { type: 'string', default: '4173' },
    profile: { type: 'string', default: 'pw5' },
    help: { type: 'boolean' }
  }
});
if (values.help) {
  console.log(
    'npm run dev -- [--package launcher] [--packages-dir PATH] [--toolkit PATH] [--profile pw5|kt6|scribe] [--port 4173]'
  );
  process.exit(0);
}
const toolkit = toolkitSource(values.toolkit);
if (!['pw5', 'kt6', 'scribe'].includes(values.profile))
  throw Error('Unknown profile: ' + values.profile);
const packageDirectory = fs.realpathSync(values['packages-dir']);
const clients = new Set();
let packages = [],
  files = new Map(),
  errors = ['Building preview…'],
  revision = 0;

function readPackages() {
  const available = new Map();
  for (const name of fs.readdirSync(packageDirectory)) {
    const manifest = path.join(packageDirectory, name, 'manifest.json');
    if (fs.existsSync(manifest)) {
      const item = JSON.parse(fs.readFileSync(manifest, 'utf8'));
      if (available.has(item.id)) throw Error('Duplicate package: ' + item.id);
      available.set(item.id, {
        ...item,
        directory: fs.realpathSync(path.dirname(manifest)),
        manifest
      });
    }
  }
  const ordered = [],
    visited = new Set(),
    visiting = new Set();
  function visit(id) {
    if (visited.has(id)) return;
    if (visiting.has(id)) throw Error('Dependency cycle at ' + id);
    const item = available.get(id);
    if (!item) throw Error('Missing package dependency: ' + id);
    visiting.add(id);
    (item.needs || []).forEach(visit);
    visiting.delete(id);
    visited.add(id);
    ordered.push(item);
  }
  (
    values.package || [
      'launcher',
      'mod-manager',
      'quick-settings',
      'status-items',
      'system-tools'
    ]
  ).forEach(visit);
  return ordered;
}
function ownerOf(filename) {
  return packages.find((item) => {
    const relative = path.relative(item.directory, filename);
    return (
      relative &&
      !relative.startsWith('..' + path.sep) &&
      relative !== '..' &&
      !path.isAbsolute(relative)
    );
  });
}
const context = await esbuild.context({
  stdin: {
    contents:
      'import {start} from "./app.jsx"; import packages from "preview:packages"; start(packages);',
    resolveDir: directory,
    loader: 'jsx'
  },
  bundle: true,
  write: false,
  outdir: '/preview',
  sourcemap: 'inline',
  format: 'esm',
  jsx: 'automatic',
  loader: { '.js': 'jsx' },
  nodePaths: [path.join(root, 'node_modules')],
  define: {
    'process.env.NODE_ENV': '"development"',
    __PREVIEW_PROFILE__: JSON.stringify(values.profile)
  },
  plugins: [
    {
      name: 'kindle-preview',
      setup(build) {
        build.onResolve({ filter: /^preview:packages$/ }, () => ({
          path: 'packages',
          namespace: 'preview'
        }));
        build.onLoad({ filter: /.*/, namespace: 'preview' }, () => {
          packages = readPackages();
          let source = '';
          const entries = [];
          packages.forEach((item, index) => {
            if ((item.ui || []).length > 1)
              throw Error('An extension supports at most one UI entry');
            let activation = 'null';
            if (item.ui?.length) {
              const entry = item.ui[0];
              const name = `entry${index}`;
              source += `import {activate as ${name}} from ${JSON.stringify(path.join(item.directory, entry))};\n`;
              activation = name;
            }
            const { directory: ignored, manifest: ignoredManifest, ...manifest } = item;
            entries.push(
              `{manifest:${JSON.stringify(manifest)},activate:${activation}}`
            );
          });
          return {
            contents: source + `export default [${entries.join(',')}];`,
            resolveDir: directory,
            watchFiles: packages.map((item) => item.manifest),
            watchDirs: [packageDirectory]
          };
        });
        build.onResolve(
          { filter: /^(react|react\/jsx-runtime|react-dom)$/ },
          (args) => ({ path: require.resolve(args.path) })
        );
        build.onResolve({ filter: /^preview:contracts$/ }, () => ({
          path: path.join(toolkit, 'compatibility.js')
        }));
        build.onResolve({ filter: /^react-native$/ }, () => ({
          path: path.join(directory, 'native.jsx')
        }));
        build.onResolve({ filter: /^(react-kindle|@kppmods\/toolkit)$/ }, () => ({
          path: path.join(toolkit, 'index.js')
        }));
        build.onResolve({ filter: /^@kppmods\/react$/ }, (args) => {
          const owner = ownerOf(args.importer);
          if (!owner) throw Error('Extension hooks must be imported from a package');
          return { path: owner.id, namespace: 'extension-sdk' };
        });
        build.onLoad({ filter: /.*/, namespace: 'extension-sdk' }, () => {
          const filename = path.join(root, 'ui/sdk/react.js');
          return {
            contents: fs.readFileSync(filename, 'utf8'),
            resolveDir: path.dirname(filename),
            watchFiles: [filename]
          };
        });
        build.onResolve(
          { filter: /^\.\/context\.js$/, namespace: 'extension-sdk' },
          (args) => ({ path: args.importer, namespace: 'extension-context' })
        );
        build.onLoad({ filter: /.*/, namespace: 'extension-context' }, (args) => ({
          contents: `import {getExtension} from ${JSON.stringify(path.join(directory, 'runtime.js'))};
            export function useExtension() { return getExtension(${JSON.stringify(args.path)}); }`,
          resolveDir: directory
        }));
        build.onResolve({ filter: /\.(svg|png|jpe?g|gif)$/ }, (args) => {
          const absolute = fs.realpathSync(path.resolve(args.resolveDir, args.path));
          const owner = ownerOf(absolute);
          if (!owner)
            throw Error('Asset must belong to a loaded package: ' + args.path);
          return { path: absolute, namespace: 'package-asset', pluginData: owner };
        });
        build.onLoad({ filter: /.*/, namespace: 'package-asset' }, (args) => ({
          contents: `export default ${JSON.stringify('/assets/' + args.pluginData.id + '/' + path.relative(args.pluginData.directory, args.path).split(path.sep).map(encodeURIComponent).join('/'))};`,
          watchFiles: [args.path]
        }));
        build.onEnd((result) => {
          errors = result.errors.map((error) => error.text);
          if (!errors.length) {
            files = new Map(
              result.outputFiles.map((file) => [
                path.basename(file.path),
                file.contents
              ])
            );
            revision++;
          }
          for (const client of clients)
            client.write(`data: ${JSON.stringify({ revision, errors })}\n\n`);
        });
      }
    }
  ]
});
await context.watch();
const mime = {
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.html': 'text/html',
  '.woff2': 'font/woff2'
};
const server = http.createServer((request, response) => {
  const url = new URL(request.url, 'http://localhost');
  response.setHeader('Cache-Control', 'no-store');
  if (request.method !== 'GET') {
    response.writeHead(405).end();
    return;
  }
  if (url.pathname === '/events') {
    response.writeHead(200, {
      'Content-Type': 'text/event-stream',
      Connection: 'keep-alive'
    });
    response.write(`data: ${JSON.stringify({ revision, errors })}\n\n`);
    clients.add(response);
    request.on('close', () => clients.delete(response));
    return;
  }
  if (url.pathname.startsWith('/assets/')) {
    try {
      const [, , id, ...segments] = decodeURIComponent(url.pathname).split('/');
      const item = packages.find((item) => item.id === id);
      if (!item) throw Error('Unknown package');
      // The installed GTK asset is generated from this SVG by package.py.
      const asset =
        id === 'status-items' && segments.join('/') === 'assets/apps.png'
          ? 'assets/apps.svg'
          : segments.join('/');
      const filename = fs.realpathSync(path.resolve(item.directory, asset));
      const relative = path.relative(fs.realpathSync(item.directory), filename);
      if (!relative || relative.startsWith('..') || path.isAbsolute(relative))
        throw Error('Invalid asset path');
      response.writeHead(200, {
        'Content-Type': mime[path.extname(filename)] || 'application/octet-stream'
      });
      response.end(fs.readFileSync(filename));
    } catch {
      response.writeHead(404).end('Asset not found');
    }
    return;
  }
  if (url.pathname === '/') {
    response.writeHead(200, { 'Content-Type': 'text/html' });
    response.end(fs.readFileSync(path.join(directory, 'index.html')));
    return;
  }
  const name = url.pathname.slice(1);
  if (files.has(name)) {
    response.writeHead(200, {
      'Content-Type': name.endsWith('.css') ? 'text/css' : 'text/javascript'
    });
    response.end(files.get(name));
  } else response.writeHead(503).end(errors.join('\n') || 'Not found');
});
server.listen(Number(values.port), '127.0.0.1', () =>
  console.log(`KPPMods workbench: http://127.0.0.1:${values.port} (local simulation)`)
);
async function stop() {
  for (const client of clients) client.end();
  server.close();
  await context.dispose();
  process.exit();
}
process.on('SIGINT', stop);
process.on('SIGTERM', stop);

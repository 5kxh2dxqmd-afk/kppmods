// Own each extension's registrations, settings writes, subscriptions and disposal.
module.exports = function createExtensionContext(extensionPackage, host) {
  const {
    configuration,
    React,
    ReactNative,
    extensionHost,
    broker,
    registry,
    executeAction,
    useRegistry,
    resolveIconSource,
    loadStockModule,
    publishedServices,
    revisionForPackage,
    reportExtensionError
  } = host;
  const { request: requestBroker, subscribe: subscribeToBroker } = broker;
  const {
    subscribe: subscribeToEvent,
    emit: emitLocalEvent,
    list: listContributions
  } = registry;
  const scope = require('./scope')((error) =>
    reportExtensionError(extensionPackage.id, error)
  );
  const trackDisposer = scope.track;
  const initialSettings = Object.assign(
    {},
    extensionPackage.settings || {},
    (configuration.settings || {})[extensionPackage.id] || {}
  );
  const extensionSettings = require('./settings')(
    extensionPackage.id,
    initialSettings,
    extensionPackage.settingsSchema || {},
    {
      React,
      broker,
      registry,
      scope
    }
  );
  async function manage(command, argument) {
    let result = await requestBroker('management', {
      command,
      argument
    });
    if (result.operation !== undefined) {
      const id = result.operation;
      do {
        await new Promise((resolve) => setTimeout(resolve, 250));
        result = await requestBroker('managementResult', { id });
      } while (!result.done);
      return result.value;
    }
    return result;
  }
  function qualifyIdentifier(id) {
    if (!/^[a-zA-Z0-9_-][a-zA-Z0-9_.-]{0,126}$/.test(id))
      throw new Error('Invalid local identifier: ' + id);
    return extensionPackage.id + ':' + id;
  }
  function assetUri(path) {
    if (
      typeof path !== 'string' ||
      path[0] === '/' ||
      path.split('/').indexOf('..') >= 0
    )
      throw new Error('Asset path must stay within package');
    return 'file://' + extensionPackage.root + '/' + path;
  }
  const api = {
    id: extensionPackage.id,
    root: extensionPackage.root,
    options: extensionPackage.options || {},
    firmware: configuration.firmware,
    React,
    RN: ReactNative,
    toolkit: extensionHost.toolkit,
    asset: assetUri,
    packages: () =>
      (configuration.packages || []).map((item) => ({
        id: item.id,
        version: item.version,
        title: item.title || item.id
      })),
    state: () => requestBroker('state'),
    onDispose: trackDisposer,
    log: (text) => {
      console.log('[KPPMods:' + extensionPackage.id + '] ' + text);
    },
    events: {
      on: (name, listener) => {
        const off = subscribeToEvent(name, listener),
          stop = name.indexOf('bridge:') === 0 ? subscribeToBroker() : () => {};
        return trackDisposer(() => {
          off();
          stop();
        });
      },
      emit: (name, data) => {
        emitLocalEvent(name, data);
        return requestBroker('events', {
          package: extensionPackage.id,
          name,
          data
        });
      }
    },
    apps: () => {
      const apps = [];
      (configuration.packages || []).forEach((owner) => {
        (owner.apps || []).forEach((app) => {
          apps.push(
            Object.assign({}, app, {
              package: owner.id,
              icon: app.icon ? 'file://' + owner.root + '/' + app.icon : undefined
            })
          );
        });
      });
      return apps;
    },
    navigate: (route) =>
      ReactNative.NativeModules.NavigationModule.navigateTo(route, {}),
    execute: (action, args) => executeAction(extensionPackage.id, action, args),
    exec: (command, options) => {
      if (!scope.active) return Promise.reject(new Error('Extension disposed'));
      const request = Object.assign({}, options || {}, {
        package: extensionPackage.id
      });
      if (Array.isArray(command)) request.argv = command;
      else request.command = command;
      let job, checkResult, release;
      let cancelled = false,
        checking = false,
        again = false,
        settled = false;
      const result = new Promise((resolve, reject) => {
        const unsubscribe = subscribeToEvent('broker:state', () => checkResult());
        const stop = subscribeToBroker();
        function finish(error, value) {
          if (settled) return;
          settled = true;
          release();
          if (error) reject(error);
          else resolve(value);
        }
        release = trackDisposer(() => {
          unsubscribe();
          stop();
          if (!settled) {
            cancelled = true;
            if (job)
              requestBroker('command', { id: job, cancel: true }).catch((error) =>
                reportExtensionError(extensionPackage.id, error)
              );
            finish(new Error('Extension disposed'));
          }
        });
        checkResult = () => {
          if (!job || settled) return;
          if (checking) {
            again = true;
            return;
          }
          checking = true;
          again = false;
          requestBroker('command', { id: job, cancel: cancelled }).then(
            (value) => {
              checking = false;
              if (value.done) finish(null, value);
              else if (again) checkResult();
            },
            (error) => finish(error)
          );
        };
        requestBroker('exec', request).then(
          (started) => {
            job = started.id;
            if (settled && cancelled)
              requestBroker('command', { id: job, cancel: true }).catch((error) =>
                reportExtensionError(extensionPackage.id, error)
              );
            else checkResult();
          },
          (error) => finish(error)
        );
      });
      result.cancel = () => {
        cancelled = true;
        checkResult();
      };
      return result;
    },
    management: {
      list: () => manage('list'),
      apply: (disabled) => manage('apply', JSON.stringify(disabled)),
      apps: () => manage('apps'),
      launchApp: (script) => manage('launch-app', script)
    },
    settings: extensionSettings.api,
    useRegistry,
    list: listContributions,
    Icon: (props) => {
      if (!extensionHost.toolkit) throw new Error('Shared toolkit unavailable');
      return React.createElement(extensionHost.toolkit.Image, {
        source: resolveIconSource(props.name),
        accessibilityLabel: props.label || '',
        style: props.style
      });
    },
    module: loadStockModule,
    capabilities: () => Object.keys(configuration.modules || {}),
    services: {
      publish: (name, version, service) => {
        const id = qualifyIdentifier(name);
        if (publishedServices[id]) throw new Error('Duplicate service ' + id);
        publishedServices[id] = {
          version,
          value: service
        };
        return trackDisposer(() => {
          delete publishedServices[id];
        });
      },
      get: (id, version) => {
        const service = publishedServices[id];
        if (!service || service.version !== version)
          throw new Error('Service unavailable: ' + id + ' v' + version);
        return service.value;
      }
    }
  };
  Object.assign(
    api,
    require('./contributions')(extensionPackage.id, {
      registry,
      scope,
      qualifyIdentifier,
      assetUri,
      revisionForPackage
    })
  );
  api.refreshSettings = extensionSettings.refresh;
  api.stop = scope.stop;
  return api;
};

// Serialize setting patches and publish only broker-confirmed values.
module.exports = function createSettings(
  packageId,
  settings,
  schema,
  { React, broker, registry, scope }
) {
  let writeQueue = Promise.resolve();
  const api = {
    snapshot: () => Object.assign({}, settings),
    get: (key, defaultValue) =>
      settings[key] === undefined ? defaultValue : settings[key],
    set: (key, value) => {
      const pending = writeQueue
        .then(() => {
          const values = { [key]: value };
          require('./validate-settings')(schema, values);
          return broker.request('settings', { package: packageId, values });
        })
        .then((confirmed) => {
          refresh(confirmed);
          return confirmed;
        });
      writeQueue = pending.catch(() => {});
      return pending;
    },
    subscribe: (listener) => {
      const off = registry.subscribe('settings:' + packageId, listener),
        stop = broker.subscribe();
      return scope.track(() => {
        off();
        stop();
      });
    }
  };
  api.use = (key, defaultValue) => {
    const [settingValue, setSettingValue] = React.useState(api.get(key, defaultValue));
    React.useEffect(
      () =>
        api.subscribe((change) => {
          if (change.key === key) setSettingValue(change.value);
        }),
      [key]
    );
    return [settingValue, (value) => api.set(key, value)];
  };
  function refresh(incoming) {
    Object.keys(incoming).forEach((key) => {
      if (JSON.stringify(settings[key]) !== JSON.stringify(incoming[key])) {
        settings[key] = incoming[key];
        registry.emit('settings:' + packageId, {
          key,
          value: incoming[key]
        });
      }
    });
  }
  return { api, refresh };
};

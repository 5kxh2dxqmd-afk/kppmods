// Override only the JavaScript predicates consumed by KPP's interface.
export function activate(extension) {
  const checks = [
    ['IgniteUtilsDataProvider', 'isDeviceRegistered'],
    ['LibraryBookDataProvider', 'IsDeviceRegistered'],
    ['SettingsApplicationModule', 'hasActiveProfileToken']
  ];
  const patched = [];
  checks.forEach(([name, method]) => {
    const target = extension.RN.NativeModules[name];
    if (!target || typeof target[method] !== 'function') return;
    const original = target[method];
    const registered = () => true;
    target[method] = registered;
    if (target[method] !== registered)
      throw new Error('Cannot override ' + name + '.' + method);
    extension.onDispose(() => {
      if (target[method] === registered) target[method] = original;
    });
    patched.push(name + '.' + method);
  });
  if (!patched.length) throw new Error('No supported KPP registration checks found');
  extension.log('Interface registration enabled: ' + patched.join(', '));
}

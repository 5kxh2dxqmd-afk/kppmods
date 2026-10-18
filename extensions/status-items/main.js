export function activate(extension) {
  extension.icons.add({ id: 'apps', path: 'assets/apps.png' });
  for (const item of extension.options.items || []) {
    extension.status.add(item);
  }
}

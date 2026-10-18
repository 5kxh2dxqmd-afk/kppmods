export function activate(extension) {
  for (const icon of extension.options.icons || []) {
    extension.icons.add(icon);
  }
}

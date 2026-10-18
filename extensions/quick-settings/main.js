export function activate(extension) {
  for (const button of extension.options.buttons || []) {
    extension.quickSettings.add({
      id: button.id,
      title: button.title,
      icon: button.icon,
      action: button.action,
      args: button.args,
      close: button.close,
      order: button.order
    });
  }
}

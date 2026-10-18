export function activate(extension) {
  if (!extension.capabilities().includes('settingsRoot')) {
    extension.log('KPP Settings is unavailable on this firmware');
    return;
  }
  extension.settingsPages.hide('legal');
}

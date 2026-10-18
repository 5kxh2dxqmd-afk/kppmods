// Extend the stock three-dot menu using its rows, scrolling and close handler.
module.exports = function installMenu(host) {
  const { loadStockModule, listContributions, patchStockMethod, ReactNative } = host;
  const prototype = loadStockModule(
    'chromeContextMenu',
    (exports) =>
      exports.default &&
      exports.default.prototype &&
      typeof exports.default.prototype.renderItems === 'function'
  ).default.prototype;
  patchStockMethod(
    'menuItems',
    prototype,
    'renderItems',
    (original) =>
      function () {
        const props = this.props;
        const items = listContributions('menuItems').map((item) => ({
          id: item.id,
          label: item.title,
          isEnabled: true,
          group: 'kppmods',
          onClick: () =>
            ReactNative.NativeModules.NavigationModule.navigateTo(item.uri, {}),
          isAppSwitchAction: false
        }));
        this.props = Object.assign({}, props, {
          contextMenuConfig: props.contextMenuConfig.concat(items)
        });
        try {
          return original.apply(this, arguments);
        } finally {
          this.props = props;
        }
      }
  );
};

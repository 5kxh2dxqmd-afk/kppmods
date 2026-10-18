// Preserve stock Home lifecycle and navigation; replace only its content renderer.
module.exports = function installHome(host) {
  const {
    loadStockModule,
    patchStockMethod,
    useRegistry,
    React,
    ReactNative,
    ExtensionErrorBoundary
  } = host;
  const home = loadStockModule(
    'views',
    (moduleExports) => moduleExports.KPPViews && moduleExports.KPPViews.KPP_HOME
  ).KPPViews.KPP_HOME;
  const prototype = home.component && home.component.prototype;
  if (!prototype || typeof prototype.renderHomeFeed !== 'function')
    throw new Error('Stock Home route has no content renderer');
  function CustomHome({ renderStock, homeProps }) {
    const items = useRegistry('home');
    if (!items.length) return renderStock();
    return React.createElement(
      ReactNative.View,
      {
        style: {
          flex: 1
        }
      },
      items.map((item) =>
        React.createElement(
          ExtensionErrorBoundary,
          {
            key: item.id + ':' + item.revision,
            owner: item.owner
          },
          React.createElement(item.component, homeProps)
        )
      )
    );
  }
  patchStockMethod(
    'home',
    prototype,
    'renderHomeFeed',
    (original) =>
      function (...args) {
        return React.createElement(CustomHome, {
          renderStock: () => original.apply(this, args),
          homeProps: this.props
        });
      }
  );
};

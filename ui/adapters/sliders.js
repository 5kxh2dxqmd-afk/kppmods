// Reuse the stock slider rendering with extension labels, bounds and settings.
// The warmth component is a rendering template; warmth hardware is not required.
module.exports = function installSliders(host) {
  const {
    loadStockModule,
    React,
    createActionHandler,
    patchStockMethod,
    useRegistry,
    ExtensionErrorBoundary
  } = host;
  const prototype = loadStockModule(
    'sliderRow',
    (moduleExports) =>
      moduleExports.default &&
      moduleExports.default.prototype &&
      moduleExports.default.prototype.render
  ).default.prototype;
  const StockWarmthSlider = loadStockModule(
    'warmthView',
    (moduleExports) =>
      moduleExports.default && typeof moduleExports.default.type === 'function'
  ).default.type;
  function Slider(props) {
    const item = props.item,
      [sliderValue, setSliderValue] = React.useState(
        item.get ? item.get() : item.value || 0
      );
    React.useEffect(() => {
      if (!item.subscribe) return;
      function refresh() {
        setSliderValue(item.get ? item.get() : item.value || 0);
      }
      const dispose = item.subscribe(refresh);
      refresh();
      return typeof dispose === 'function' ? dispose : undefined;
    }, [item]);
    const tree = StockWarmthSlider({
      is2xScalingSupported: props.is2xScalingSupported,
      warmthScheduleInfo: {
        enabled: false,
        mode: 'manual',
        startTime: 0,
        endTime: 0
      },
      showAlertDialog: () => {},
      caflInfo: {
        value: sliderValue,
        valueMin: item.min || 0,
        valueMax: item.max === undefined ? 100 : item.max
      },
      onSliderChange: (value) => {
        value = Math.max(
          item.min || 0,
          Math.min(item.max === undefined ? 100 : item.max, Math.round(value))
        );
        setSliderValue(value);
        if (item.set) item.set(value);
        if (item.action)
          createActionHandler(item)({
            value
          });
      },
      onSliderAdjusting: () => {},
      onScheduleClick: () => {}
    });
    function relabel(node, title) {
      if (typeof node === 'string') return title ? item.title : node;
      if (!React.isValidElement(node)) return node;
      const props = {};
      if (node.props.testID)
        props.testID = node.props.testID.replace(/warmth|volume/g, item.id);
      if (node.props.accessibilityLabel) props.accessibilityLabel = item.title;
      return React.cloneElement(
        node,
        props,
        React.Children.map(node.props.children, (child) => relabel(child, title))
      );
    }
    const rows = React.Children.toArray(tree.props.children);
    if (rows[0])
      rows[0] = React.cloneElement(
        rows[0],
        {},
        relabel(React.Children.toArray(rows[0].props.children)[0], true)
      );
    if (rows[1]) rows[1] = relabel(rows[1]);
    return React.cloneElement(
      tree,
      {
        testID: 'kppmods.' + item.id
      },
      rows
    );
  }
  function ExtensionSliders(props) {
    return React.createElement(
      React.Fragment,
      {},
      useRegistry('sliders').map((item) =>
        React.createElement(
          ExtensionErrorBoundary,
          {
            key: item.id + ':' + item.revision,
            owner: item.owner
          },
          React.createElement(Slider, { item, is2xScalingSupported: props.scaling })
        )
      )
    );
  }
  patchStockMethod(
    'sliders',
    prototype,
    'render',
    (original) =>
      function () {
        const tree = original.apply(this, arguments);
        return React.cloneElement(
          tree,
          {},
          React.Children.toArray(tree.props.children).concat(
            React.createElement(ExtensionSliders, {
              key: 'kppmods-sliders',
              scaling: this.props.is2xScalingSupported
            })
          )
        );
      }
  );
};

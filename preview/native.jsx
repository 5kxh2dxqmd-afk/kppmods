// Browser implementations of the native contracts consumed by react-kindle.
// These are simulated managers, not captured firmware capabilities.
import React, {
  forwardRef,
  useEffect,
  useImperativeHandle,
  useRef,
  useState
} from 'react';
import * as Web from 'react-native-web';
import QR from 'qrcode';
import { createPortal } from 'react-dom';
import { featureContracts } from 'preview:contracts';
export * from 'react-native-web';

const listeners = new Set();
let viewport = { width: 632, height: 790, scale: 1, fontScale: 1 };
let deviceHandler = () => {};
export function setDeviceHandler(handler) {
  deviceHandler = handler;
}
export function configureViewport(size) {
  viewport = { ...viewport, ...size };
  listeners.forEach((listener) => listener({ window: viewport, screen: viewport }));
}
export const Dimensions = {
  get: () => viewport,
  addEventListener: (name, listener) => {
    listeners.add(listener);
    return { remove: () => listeners.delete(listener) };
  }
};
export function assetURL(source) {
  const uri = typeof source === 'string' ? source : source?.uri;
  return uri?.replace(/^file:\/\/\/packages\//, '/assets/');
}
export const NativeModules = {
  NavigationModule: { navigateTo: (uri) => deviceHandler('navigate', uri) },
  ReactKindleChrome: {
    configure: (source, configuration) => {
      deviceHandler('chrome', JSON.parse(configuration));
      return '';
    },
    takeEvents: () => JSON.stringify(chromeEvents.splice(0)),
    getViewport: () => JSON.stringify(viewport)
  }
};
const chromeEvents = [];
export function clickChrome(id) {
  chromeEvents.push({ id });
}

export function nativeStyle(styles) {
  const flat = { ...Web.StyleSheet.flatten(styles) };
  Object.keys(flat).forEach((key) => {
    if (flat[key] === undefined) delete flat[key];
  });
  if (flat.fontFamily) flat.fontFamily = browserFont(flat.fontFamily);
  for (const [name, targets] of Object.entries({
    paddingHorizontal: ['paddingLeft', 'paddingRight'],
    paddingVertical: ['paddingTop', 'paddingBottom'],
    marginHorizontal: ['marginLeft', 'marginRight'],
    marginVertical: ['marginTop', 'marginBottom']
  })) {
    if (flat[name] !== undefined) {
      targets.forEach((target) => {
        if (flat[target] === undefined) flat[target] = flat[name];
      });
      delete flat[name];
    }
  }
  if (Array.isArray(flat.transform))
    flat.transform = flat.transform
      .map((item) =>
        Object.entries(item)
          .map(
            ([key, value]) =>
              `${key}(${typeof value === 'number' && key.startsWith('translate') ? value + 'px' : value})`
          )
          .join(' ')
      )
      .join(' ');
  return {
    boxSizing: 'border-box',
    borderStyle: 'solid',
    borderWidth: 0,
    minWidth: 0,
    ...flat
  };
}
const event = (value = {}) => ({ nativeEvent: value });
function useNativeRef(ref, element, additional = {}) {
  useImperativeHandle(ref, () => ({
    focus: () => element.current?.focus(),
    blur: () => element.current?.blur(),
    scrollTo: (options) =>
      element.current?.scrollTo({
        left: options.x || 0,
        top: options.y || 0,
        behavior: options.animated ? 'smooth' : 'instant'
      }),
    setNativeProps: (props) => {
      if (!element.current) return;
      if (props.text !== undefined) element.current.value = props.text;
      if (props.value !== undefined) element.current.value = props.value;
      if (props.style) Object.assign(element.current.style, nativeStyle(props.style));
    },
    ...additional
  }));
}
function accessibility(props) {
  return { 'aria-label': props.accessibilityLabel, 'data-testid': props.testID };
}
const NativeButton = forwardRef(function NativeButton(props, ref) {
  const { type = 0, selected, active, disabled, children } = props;
  const node = useRef(null);
  useNativeRef(ref, node);
  const clicked = () => {
    if (!disabled) {
      props.onClicked?.(event());
    }
  };
  const style = nativeStyle(props.style);
  const toggle = type === 1;
  const select = type === 2 || type === 3;
  return (
    <div
      ref={node}
      {...accessibility(props)}
      role={props.accessibilityRole || 'button'}
      aria-disabled={!!disabled}
      aria-checked={toggle || select ? !!selected : undefined}
      aria-selected={type === 4 ? !!selected : undefined}
      tabIndex={disabled ? -1 : 0}
      className={`native-button ${disabled ? 'disabled' : ''} ${type === 4 && selected ? 'selected' : ''}`}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        color: active ? '#fff' : '#111',
        backgroundColor: active ? '#111' : '#fff',
        ...(toggle || select ? { width: toggle ? 68 : 36, height: 36 } : {}),
        ...style
      }}
      onPointerDown={(e) => {
        e.stopPropagation();
        if (!disabled) props.onPressIn?.(event());
      }}
      onPointerCancel={() => props.onPressOut?.(event())}
      onPointerLeave={(e) => {
        if (e.buttons) props.onPressOut?.(event());
      }}
      onClick={(e) => {
        e.stopPropagation();
        clicked();
      }}
      onContextMenu={(e) => {
        if (props.onLongPress && !disabled) {
          e.preventDefault();
          props.onLongPress(event());
        }
      }}
      onKeyDown={(e) => {
        e.stopPropagation();
        if ((e.key === 'Enter' || e.key === ' ') && !disabled) {
          e.preventDefault();
          props.onPressIn?.(event());
          clicked();
        }
      }}
    >
      {toggle ? (
        <span className={'native-switch ' + (selected ? 'on' : '')}>
          <span />
        </span>
      ) : select ? (
        <span className={'native-check ' + (type === 3 ? 'radio' : '')}>
          {selected ? (type === 3 ? '●' : '✓') : ''}
        </span>
      ) : (
        children
      )}
    </div>
  );
});
const NativeInput = forwardRef(function NativeInput(props, ref) {
  const node = useRef(null);
  useNativeRef(ref, node);
  const Tag = props.multiline ? 'textarea' : 'input';
  return (
    <Tag
      ref={node}
      {...accessibility(props)}
      type={props.secureTextEntry ? 'password' : 'text'}
      defaultValue={props.text}
      readOnly={props.editable === false}
      placeholder={props.placeholder}
      style={{
        background: 'transparent',
        outline: 'none',
        color: '#111',
        resize: 'none',
        ...nativeStyle(props.style)
      }}
      onChange={(e) => props.onMessage?.(event({ text: e.target.value }))}
      onFocus={() => props.onFocus?.(event())}
      onBlur={() => props.onBlur?.(event())}
    />
  );
});
const MultilineInput = forwardRef((props, ref) => (
  <NativeInput {...props} multiline ref={ref} />
));
const NativeSlider = forwardRef(function NativeSlider(props, ref) {
  const node = useRef(null);
  useNativeRef(ref, node);
  return (
    <div className="native-slider" style={nativeStyle(props.style)}>
      <input
        ref={node}
        type="range"
        {...accessibility(props)}
        min={props.minimumValue}
        max={props.maximumValue}
        defaultValue={props.value}
        step="1"
        disabled={props.disabled}
        onChange={(e) => props.onChange?.(event({ value: Number(e.target.value) }))}
      />
      {props.type === 1 && <output>{props.value}</output>}
    </div>
  );
});
const NativeScroll = forwardRef(function NativeScroll(props, ref) {
  const node = useRef(null),
    last = useRef({ x: 0, y: 0 });
  useNativeRef(ref, node);
  return (
    <div
      ref={node}
      {...accessibility(props)}
      tabIndex={0}
      style={{ flex: 1, ...nativeStyle(props.style), overflow: 'auto' }}
      onKeyDown={(e) =>
        props.onKeyPress?.(
          event({
            key:
              e.key === 'PageUp'
                ? 'Page_Up'
                : e.key === 'PageDown'
                  ? 'Page_Down'
                  : e.key
          })
        )
      }
      onScroll={(e) => {
        const next = { x: e.currentTarget.scrollLeft, y: e.currentTarget.scrollTop };
        props.onScroll?.(
          event({
            contentOffset: next,
            velocity: { x: next.x - last.current.x, y: next.y - last.current.y }
          })
        );
        last.current = next;
      }}
    >
      {props.children}
    </div>
  );
});
function NativeWindow(props) {
  if (props.hide) return null;
  const modal = props.windowType === 'modal' || props.windowType === 'dialog';
  const content = (
    <div
      role={modal ? 'dialog' : undefined}
      aria-modal={modal || undefined}
      aria-label={props.title}
      className="native-window"
      style={{
        display: 'flex',
        flexDirection: 'column',
        ...nativeStyle(props.style),
        ...(modal
          ? {
              position: 'relative',
              top: 'auto',
              left: 'auto',
              maxWidth: '95%',
              maxHeight: '95%',
              overflow: 'auto'
            }
          : {})
      }}
    >
      {props.children}
    </div>
  );
  return modal
    ? createPortal(
        <div
          className={'native-lightbox ' + (props.windowType === 'modal' ? 'sheet' : '')}
          onClick={(e) => {
            if (e.target === e.currentTarget)
              props.onMessage?.(event({ text: 'INITIATE_SHUTDOWN' }));
          }}
        >
          {content}
        </div>,
        document.querySelector('.screen')
      )
    : content;
}
function NativeImage(props) {
  return (
    <img
      {...accessibility(props)}
      alt={props.accessibilityLabel || ''}
      src={assetURL(props.source)}
      style={{
        objectFit:
          props.resizeMode === 'cover'
            ? 'cover'
            : props.resizeMode === 'stretch'
              ? 'fill'
              : 'contain',
        filter:
          props.invertModeForDarkColorScheme === 'invert' ? 'invert(1)' : undefined,
        ...nativeStyle(props.style)
      }}
    />
  );
}
function NativeQRCode(props) {
  const [src, setSrc] = useState('');
  const handler = useRef(props.onChange);
  handler.current = props.onChange;
  useEffect(() => {
    let mounted = true;
    QR.toDataURL(props.value, { width: props.size, margin: props.margin })
      .then((uri) => {
        if (mounted) {
          setSrc(uri);
          handler.current?.(event({ value: props.size }));
        }
      })
      .catch(() => {
        if (mounted) handler.current?.(event({ value: -1 }));
      });
    return () => {
      mounted = false;
    };
  }, [props.value, props.size, props.margin]);
  return src ? <img alt="QR code" src={src} style={nativeStyle(props.style)} /> : null;
}
// Pango markup is not HTML. Interpret its text/style subset without injecting HTML.
function markupNodes(node, key) {
  if (node.nodeType === 3) return node.textContent;
  const tag = node.nodeName;
  const description = node.getAttribute?.('font_desc') || '';
  const size = description.match(/(\d+(?:\.\d+)?)px$/)?.[1];
  const style = {
    fontWeight: tag === 'b' ? 'bold' : undefined,
    fontStyle: tag === 'i' ? 'italic' : undefined,
    textDecoration: tag === 'u' ? 'underline' : undefined,
    fontSize: size ? Number(size) : undefined,
    color: node.getAttribute?.('foreground') || undefined
  };
  return (
    <span key={key} style={style}>
      {Array.from(node.childNodes).map(markupNodes)}
    </span>
  );
}
function NativeMarkup(props) {
  const parsed = new DOMParser().parseFromString(
    `<markup>${props.markup}</markup>`,
    'application/xml'
  );
  return (
    <div style={nativeStyle(props.style)}>
      {parsed.querySelector('parsererror')
        ? 'Invalid Pango markup'
        : markupNodes(parsed.documentElement)}
    </div>
  );
}
const hosts = {
  KPPUnifiedButton: NativeButton,
  KPPUIViewWrapper: (props) => (
    <Web.View style={{ alignSelf: 'stretch', flexShrink: 1 }}>
      {props.children}
    </Web.View>
  ),
  KPPSimpleNotebook: (props) => (
    <Web.View style={props.style}>
      {React.Children.toArray(props.children).filter(
        (child) => child.props.name === props.currPage
      )}
    </Web.View>
  ),
  KPPTextInput: NativeInput,
  KPPMultilineTextInput: MultilineInput,
  KPPUnifiedSlider: NativeSlider,
  KPPSlider: NativeSlider,
  KPPProgressBar: (props) => (
    <progress max="100" value={props.progress} style={nativeStyle(props.style)} />
  ),
  KPPScrollBar: (props) => (
    <div className="native-scrollbar">
      <button onClick={() => props.onScroll?.(event({ velocity: { x: -1, y: -1 } }))}>
        ‹
      </button>
      <span>
        {props.currentItemIndex + 1} / {props.totalItems}
      </span>
      <button onClick={() => props.onScroll?.(event({ velocity: { x: 1, y: 1 } }))}>
        ›
      </button>
    </div>
  ),
  KPPSpinner: (props) => (
    <div
      className="native-spinner"
      role="progressbar"
      aria-label="Loading"
      style={{ width: props.width, height: props.height }}
    />
  ),
  KPPBar: (props) => (
    <div style={{ ...nativeStyle(props.style), background: props.color }} />
  ),
  KPPPill: (props) => (
    <button
      className="native-pill"
      aria-label={props.accessibilityLabel}
      style={nativeStyle(props.style)}
      onClick={() =>
        (props.gestureDirectionHint === 'up' ? props.onSwipeUp : props.onSwipeDown)?.(
          event()
        )
      }
    >
      <span />
    </button>
  ),
  KPPImage: NativeImage,
  KPPMarkupText: NativeMarkup,
  KPPQRCode: NativeQRCode,
  KPPStaticHTMLView: (props) => (
    <iframe
      title="Static document (browser rendering)"
      sandbox=""
      src={assetURL(props.source)}
      style={nativeStyle(props.style)}
    />
  ),
  KPPTextMeasurementWrapper: (props) => (
    <Web.View style={{ width: props.width, height: props.height }}>
      {props.children}
    </Web.View>
  ),
  KPPPaginatedScrollView: NativeScroll,
  KPPControlledScrollView: NativeScroll,
  KPPWindow: NativeWindow,
  KPPOverlayView: (props) =>
    createPortal(
      <Web.View
        {...props}
        style={[
          { position: 'absolute', top: 40, bottom: 54, left: 0, right: 0, zIndex: 8 },
          props.style
        ]}
      />,
      document.querySelector('.screen')
    )
};
export function requireNativeComponent(name) {
  if (!hosts[name]) throw Error(`The browser preview has no adapter for ${name}`);
  return hosts[name];
}
export const findNodeHandle = (ref) => ref;
export const UIManager = {
  getViewManagerConfig: (name) => {
    if (!hosts[name]) return null;
    const contracts = Object.values(featureContracts).filter(
      (contract) => contract.manager === name
    );
    return {
      NativeProps: Object.fromEntries(
        contracts
          .flatMap((contract) => contract.props || [])
          .map((prop) => [prop, 'mixed'])
      ),
      directEventTypes: Object.fromEntries(
        contracts
          .flatMap((contract) => contract.events || [])
          .map((name) => [name, { registrationName: name }])
      ),
      Commands: Object.fromEntries(
        contracts
          .filter((contract) => contract.command)
          .map((contract) => [contract.command, contract.command])
      )
    };
  },
  getConstants: () => ({}),
  dispatchViewManagerCommand: (ref, command, args) => {
    if (typeof ref?.[command] !== 'function')
      throw Error(`Unsupported preview command: ${command}`);
    ref[command](...args);
  }
};
function browserFont(family) {
  if (family === 'Amazon Ember') return 'Arial, sans-serif';
  if (family === 'Bookerly' || family === 'Bookerly Display') return 'Georgia, serif';
  if (family === 'Kindle Symbol') return 'Arial, sans-serif';
  return family;
}
export const Text = forwardRef(function Text(props, ref) {
  const style = Web.StyleSheet.flatten(props.style);
  const children =
    style?.fontFamily === 'Kindle Symbol' && typeof props.children === 'string'
      ? props.children
          .replaceAll('\uf865', '★')
          .replaceAll('\uf844', '☆')
          .replaceAll('\uf846', '◐')
      : props.children;
  return (
    <Web.Text
      {...props}
      style={[
        props.style,
        style?.fontFamily && { fontFamily: browserFont(style.fontFamily) }
      ]}
      ref={ref}
    >
      {children}
    </Web.Text>
  );
});

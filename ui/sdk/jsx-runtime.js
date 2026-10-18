// Automatic JSX uses the device's React.createElement. It must not import the
// development machine's react/jsx-runtime, which would bring a second runtime.
import React from 'react';

export const Fragment = React.Fragment;
export function jsx(component, props, key) {
  return React.createElement(component, key === undefined ? props : { ...props, key });
}
export const jsxs = jsx;

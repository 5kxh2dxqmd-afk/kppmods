// This module is bound to one extension by build-extension.cjs. React itself is
// supplied by Kindle; no second React runtime or renderer is bundled.
import { useCallback, useEffect } from 'react';
import { useExtension } from './context.js';

export { useExtension };

export function useSetting(key, initialValue) {
  return useExtension().settings.use(key, initialValue);
}

export function useApps() {
  return useExtension().apps();
}

export function usePackages() {
  return useExtension().packages();
}

export function useRegistry(kind) {
  return useExtension().useRegistry(kind);
}

export function useAction(name) {
  const extension = useExtension();
  return useCallback((args) => extension.execute(name, args), [extension, name]);
}

export function useEvent(name, listener) {
  const extension = useExtension();
  useEffect(() => extension.events.on(name, listener), [extension, name, listener]);
}

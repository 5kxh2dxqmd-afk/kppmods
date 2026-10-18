// LIPC carries update metadata; the native adapter reads source from its local file.
module.exports = function startDevelopment({
  extensionHost,
  configuration,
  requestBroker,
  reportExtensionError
}) {
  let revision = '';
  const delay = Math.max(100, configuration.development.pollMs || 200);
  function poll() {
    requestBroker('development', { after: revision })
      .then((update) =>
        update.revision && update.revision !== revision
          ? requestBroker('readFile', { path: update.sourceFile }).then(({ source }) =>
              Object.assign({}, update, { source })
            )
          : update
      )
      .then((update) => {
        if (!update.revision || update.revision === revision) return;
        revision = update.revision;
        const started = Date.now();
        let result;
        try {
          extensionHost.replaceSource(
            update.package,
            update.source,
            update.metadata || {}
          );
          result = { revision, package: update.package, accepted: true };
        } catch (error) {
          result = {
            revision,
            package: update.package,
            accepted: false,
            error: String(error.message || error)
          };
          reportExtensionError(update.package, error);
        }
        result.applyMs = Date.now() - started;
        return requestBroker('developmentResult', result);
      })
      .catch((error) => console.warn('[KPPMods development] ' + error.message))
      .then(() => setTimeout(poll, delay));
  }
  poll();
};

// Own native request correlation and event-driven broker state synchronization.
module.exports = function createBrokerClient(RN, onState) {
  let nextRequest = 0,
    subscribers = 0,
    timer = null,
    inFlight = false,
    dirty = false;
  let cursor = 0,
    streamId = null;
  const pending = new Map();
  function request(operation, body = {}) {
    return new Promise((resolve, reject) => {
      const id = ++nextRequest;
      const timeout = setTimeout(() => {
        pending.delete(id);
        reject(new Error('LIPC request timed out: ' + operation));
      }, 12000);
      pending.set(id, (value) => {
        clearTimeout(timeout);
        if (value.error) reject(new Error(value.error));
        else resolve(value);
      });
      try {
        RN.NativeModules.Networking.sendRequest('kppmods', id, operation, body);
      } catch (error) {
        pending.delete(id);
        clearTimeout(timeout);
        reject(error);
      }
    });
  }
  function poll() {
    clearTimeout(timer);
    timer = null;
    if (!subscribers || inFlight) return;
    dirty = false;
    inFlight = true;
    request('state', { since: cursor })
      .then((state) => {
        if ((streamId !== null && state.streamId !== streamId) || state.seq < cursor)
          return request('state');
        return state;
      })
      .then((state) => {
        streamId = state.streamId;
        cursor = state.seq;
        onState(state);
      })
      .catch(() => {
        if (subscribers) timer = setTimeout(poll, 5000);
      })
      .then(() => {
        inFlight = false;
        if (dirty && subscribers && timer === null) poll();
      });
  }
  RN.DeviceEventEmitter.addListener('kppmods:response', ([id, value]) => {
    const complete = pending.get(id);
    if (complete) {
      pending.delete(id);
      complete(value);
    }
  });
  RN.DeviceEventEmitter.addListener('kppmods:changed', () => {
    dirty = true;
    if (!inFlight) poll();
  });
  return {
    request,
    subscribe() {
      subscribers++;
      if (!inFlight && timer === null) poll();
      let active = true;
      return () => {
        if (!active) return;
        active = false;
        if (!--subscribers) {
          clearTimeout(timer);
          timer = null;
        }
      };
    }
  };
};

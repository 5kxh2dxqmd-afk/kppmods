// Early cleanup removes its registration; extension disposal runs the remainder.
module.exports = function createScope(reportError) {
  const disposers = new Set();
  let active = true;
  return {
    get active() {
      return active;
    },
    track(dispose) {
      if (!active) {
        dispose();
        return () => {};
      }
      function stop() {
        if (!disposers.delete(stop)) return;
        dispose();
      }
      disposers.add(stop);
      return stop;
    },
    stop() {
      active = false;
      Array.from(disposers)
        .reverse()
        .forEach((dispose) => {
          try {
            dispose();
          } catch (error) {
            reportError(error);
          }
        });
    }
  };
};

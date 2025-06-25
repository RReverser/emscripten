addToLibrary({
  sync_tunnel: (value) => Asyncify.handleSleep((wakeUp) => {
    setTimeout(wakeUp, 1, value + 1);
  }),
  sync_tunnel_bool: (value) => Asyncify.handleSleep((wakeUp) => {
    setTimeout(wakeUp, 1, !value);
  })
});

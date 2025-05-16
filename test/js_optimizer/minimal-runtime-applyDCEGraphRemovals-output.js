var name;
var wasmImports = {
  save1: 1,
  number: 33,
  save2: 2
};

// exports gotten directly in the minimal runtime style
WebAssembly.instantiate(Module["wasm"], imports).then(output => {
  wasmExports = output.instance.exports;
  _expD1 = wasmExports['expD1'];
  _expD2 = wasmExports['expD2'];
  _expD3 = wasmExports['expD3'];
  ;
  initRuntime(wasmExports);
  ready();
});

// add uses for some of them, leave *4 as non-roots
_expD1;
Module['_expD2'];
wasmExports['_expD3'];

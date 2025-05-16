// stores
_asan_js_store(HEAP8, x, 1);
_asan_js_store(HEAP16, x, 2);
_asan_js_store(HEAP32, x, 3);
_asan_js_store(HEAPU8, x, 4);
_asan_js_store(HEAPU16, x, 5);
_asan_js_store(HEAPU32, x, 6);
_asan_js_store(HEAPF32, x, 7);
_asan_js_store(HEAPF64, x, 8);
_asan_js_store(HEAP64, x, 9n);
_asan_js_store(HEAPU64, x, 10n);

// loads
a1 = _asan_js_load(HEAP8, x);
a2 = _asan_js_load(HEAP16, x);
a3 = _asan_js_load(HEAP32, x);
a4 = _asan_js_load(HEAPU8, x);
a5 = _asan_js_load(HEAPU16, x);
a6 = _asan_js_load(HEAPU32, x);
a7 = _asan_js_load(HEAPF32, x);
a8 = _asan_js_load(HEAPF64, x);
a9 = _asan_js_load(HEAP64, x);
a10 = _asan_js_load(HEAPU64, x);

// store return value
foo = _asan_js_store(HEAPU8, 1337, 42);

// nesting
_asan_js_load(HEAP16, bar(_asan_js_load(HEAPF64, 5)));
_asan_js_store(HEAPF32, x, _asan_js_load(HEAP32, y));

// Ignore the special asan functions themselves. that is, any JS memory access
// will turn into a function call to _asan_js_load_1 etc., which then does
// the memory access for it. It either calls into wasm to get the proper
// asan-instrumented operation, or before the wasm is ready to be called into,
// we must do the access in JS, unsafely. We should not instrument a heap
// access in these functions, as then we'd get infinite recursion - this is
// where we do actually need to still do a HEAP8[..] etc. operation without
// any ASan instrumentation.
function _asan_js_load(ptr) {
  return HEAP8[ptr];
}

// but do handle everything else
function somethingElse() {
  return _asan_js_load(HEAP8, ptr);
}

// ignore a.X
HEAP8.length;
_asan_js_load(HEAP8, length);

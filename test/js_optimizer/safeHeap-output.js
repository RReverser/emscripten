// stores
SAFE_HEAP_STORE(HEAP8, x, 1);
SAFE_HEAP_STORE(HEAP16, x, 2);
SAFE_HEAP_STORE(HEAP32, x, 3);
SAFE_HEAP_STORE(HEAPU8, x, 4);
SAFE_HEAP_STORE(HEAPU16, x, 5);
SAFE_HEAP_STORE(HEAPU32, x, 6);
SAFE_HEAP_STORE(HEAPF32, x, 7);
SAFE_HEAP_STORE(HEAPF64, x, 8);
SAFE_HEAP_STORE(HEAP64, x, 9n);
SAFE_HEAP_STORE(HEAPU64, x, 10n);

// loads
a1 = SAFE_HEAP_LOAD(HEAP8, x);
a2 = SAFE_HEAP_LOAD(HEAP16, x);
a3 = SAFE_HEAP_LOAD(HEAP32, x);
a4 = SAFE_HEAP_LOAD(HEAPU8, x);
a5 = SAFE_HEAP_LOAD(HEAPU16, x);
a6 = SAFE_HEAP_LOAD(HEAPU32, x);
a7 = SAFE_HEAP_LOAD(HEAPF32, x);
a8 = SAFE_HEAP_LOAD(HEAPF64, x);
a9 = SAFE_HEAP_LOAD(HEAP64, x);
a10 = SAFE_HEAP_LOAD(HEAPU64, x);

// store return value
foo = SAFE_HEAP_STORE(HEAPU8, 1337, 42);

// nesting
SAFE_HEAP_LOAD(HEAP16, bar(SAFE_HEAP_LOAD(HEAPF64, 5)));
SAFE_HEAP_STORE(HEAPF32, x, SAFE_HEAP_LOAD(HEAP32, y));

// Ignore the special functions themselves. that is, any JS memory access
// will turn into a function call to _asan_js_load_1 etc., which then does
// the memory access for it. It either calls into wasm to get the proper
// asan-instrumented operation, or before the wasm is ready to be called into,
// we must do the access in JS, unsafely. We should not instrument a heap
// access in these functions, as then we'd get infinite recursion - this is
// where we do actually need to still do a HEAP8[..] etc. operation without
// any ASan instrumentation.
function SAFE_HEAP_FOO(ptr) {
  return HEAP8[ptr];
}

// but do handle everything else
function somethingElse() {
  return SAFE_HEAP_LOAD(HEAP8, ptr);
}

// ignore a.X
HEAP8.length;
SAFE_HEAP_LOAD(HEAP8, length);

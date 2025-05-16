a = HEAP8[x]; // HEAP8
HEAP8[x] = a;
a = HEAPU8[x]; // HEAPU8
HEAPU8[x] = a;
a = LE_HEAP_LOAD_I16(x * 2); // HEAP16
LE_HEAP_STORE_I16(x * 2, a);
a = LE_HEAP_LOAD_U16(x * 2); // HEAPU16
LE_HEAP_STORE_U16(x * 2, a);
a = LE_HEAP_LOAD_I32(x * 4); // HEAPI32
LE_HEAP_STORE_I32(x * 4, a);
a = LE_HEAP_LOAD_U32(x * 4); // HEAPU32
LE_HEAP_STORE_U32(x * 4, a);
a = LE_HEAP_LOAD_F32(x * 4); // HEAPF32
LE_HEAP_STORE_F32(x * 4, a);
a = LE_HEAP_LOAD_F64(x * 8); // HEAPF64
LE_HEAP_STORE_F64(x * 8, a);
HEAP[x]; // should not be changed
HeAp[x];
LE_ATOMICS_ADD(heap, offset, value);
LE_ATOMICS_AND(heap, offset, value);
LE_ATOMICS_COMPAREEXCHANGE(heap, offset, expected, replacement);
LE_ATOMICS_EXCHANGE(heap, offset, value);
LE_ATOMICS_LOAD(heap, offset);
LE_ATOMICS_OR(heap, offset, value);
LE_ATOMICS_SUB(heap, offset, value);
LE_ATOMICS_WAIT(heap, offset, value, timeout);
LE_ATOMICS_WAITASYNC(heap, offset, value, timeout);
LE_ATOMICS_XOR(heap, offset, value);

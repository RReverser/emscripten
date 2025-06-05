#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define jstype(X)                                                              \
  _Generic((X),                                                                \
    bool: "u8",                                                                \
    char: "i8",                                                                \
    void*: "*",                                                                \
    int8_t: "i8",                                                              \
    uint8_t: "u8",                                                             \
    int16_t: "i16",                                                            \
    uint16_t: "u16",                                                           \
    int32_t: "i32",                                                            \
    uint32_t: "u32",                                                           \
    int64_t: "i64",                                                            \
    uint64_t: "u64",                                                           \
    long: "*", /* 32- or 64-bit integer depending on a platform, use the       \
                  dynamically-sized '*' type */                                \
    unsigned long: "*", /* Same as above. TODO: should we have separate type   \
                           for signed vs unsigned? */                          \
    default: /* catch-all for arbitrary pointers and user-defined types */     \
                                                                               \
        __builtin_classify_type((X)) == __builtin_classify_type((void*)0)      \
      ? "*" /* an array or a pointer, unfortunately no way to distinguish */   \
      : "" /* a custom user type */)

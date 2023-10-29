/*
 * Copyright 2012 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#if __cplusplus < 201103L
#error Including <emscripten/wire.h> requires building with -std=c++11 or newer!
#endif

// A value moving between JavaScript and C++ has three representations:
// - The original JS value: a String
// - The native on-the-wire value: a stack-allocated char*, say
// - The C++ value: std::string
//
// We'll call the on-the-wire type WireType.

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#define EMSCRIPTEN_ALWAYS_INLINE __attribute__((always_inline))

#ifndef EMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES
#define EMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES 1
#endif

namespace emscripten {

#if EMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES
constexpr bool has_unbound_type_names = true;
#else
constexpr bool has_unbound_type_names = false;
#endif

template<typename ElementType> struct memory_view {
  memory_view() = delete;
  explicit memory_view(size_t size, const ElementType* data)
    : size(size), data(data) {}

  const size_t size; // in elements, not bytes
  const void* const data;
};

namespace internal {

typedef const void* TYPEID;

extern "C" {

bool _embind_is_registered_type(
    TYPEID type);

void _embind_register_void(
    TYPEID voidType,
    const char* name);

void _embind_register_bool(
    TYPEID boolType,
    const char* name,
    bool trueValue,
    bool falseValue);

void _embind_register_integer(
    TYPEID integerType,
    const char* name,
    size_t size,
    int32_t minRange,
    uint32_t maxRange);

void _embind_register_bigint(
    TYPEID integerType,
    const char* name,
    size_t size,
    int64_t minRange,
    uint64_t maxRange);

void _embind_register_float(
    TYPEID floatType,
    const char* name,
    size_t size);

void _embind_register_memory_view(
    TYPEID memoryViewType,
    unsigned typedArrayIndex,
    const char* name);

void _embind_register_std_string(
    TYPEID stringType,
    const char* name);

void _embind_register_std_wstring(
    TYPEID stringType,
    size_t charSize,
    const char* name);

} // extern "C"

// We don't need the full std::type_info implementation.  We
// just need a unique identifier per type and polymorphic type
// identification.

template<typename T>
struct CanonicalizedID {
    static char c;
    static constexpr TYPEID get() {
        return &c;
    }
};

template<typename T>
char CanonicalizedID<T>::c;

template<typename T>
struct Canonicalized {
    typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;
};

template<typename T>
struct register_js {
    register_js(TYPEID id) {}
};

template<typename T>
struct LightTypeID {
    static TYPEID get() {
        TYPEID id = get_without_registration();
        register_js_once_per_worker(id);
        return id;
    }

private:
    static constexpr TYPEID get_without_registration() {
        if (has_unbound_type_names) {
#if __has_feature(cxx_rtti)
            return &typeid(T);
#else
            static_assert(!has_unbound_type_names,
                "Unbound type names are illegal with RTTI disabled. "
                "Either add -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 to or remove -fno-rtti "
                "from the compiler arguments");
#endif
        }

        typedef typename Canonicalized<T>::type C;
        return CanonicalizedID<C>::get();
    }

    static void register_js_once_per_worker(TYPEID id) {
        thread_local bool registered;
        // slightly cheaper than inline bool init by avoiding extra guard variable
        if (!registered) {
            // It's possible this thread is reusing same Web Worker,
            // in which case the type may already be registered in JS,
            // or it might've been registered dynamically by EMSCRIPTEN_BINDINGS.
            if (!_embind_is_registered_type(id)) {
                typedef typename Canonicalized<T>::type C;
                typename register_js<C>::register_js reg(id);
            }
            registered = true;
        }
    }
};

// This methods deals with up/downcasting between classes via RTTI when available.
// Since it gets dynamic TYPEID, we can't use automatic type registration here -
// the assumption is that it's been done by EMSCRIPTEN_BINDINGS() block, otherwise
// it will fail at runtime.
template<typename T>
TYPEID getLightTypeID(const T& value) {
#if __has_feature(cxx_rtti)
    if (has_unbound_type_names) {
        return &typeid(value);
    }
#endif
    return LightTypeID<T>::get();
}

// The second typename is an unused stub so it's possible to
// specialize groups of classes via SFINAE.
template<typename T, typename = void>
struct TypeID {
    static TYPEID get() {
        return LightTypeID<T>::get();
    }
};

template<typename T>
struct TypeID<std::unique_ptr<T>> {
    static TYPEID get() {
        return TypeID<T>::get();
    }
};

template<typename T>
struct TypeID<T*> {
    static_assert(!std::is_pointer<T*>::value, "Implicitly binding raw pointers is illegal.  Specify allow_raw_pointer<arg<?>>");
};

template<typename T>
struct AllowedRawPointer {
};

template<typename T>
struct TypeID<AllowedRawPointer<T>> {
    static TYPEID get() {
        return LightTypeID<T*>::get();
    }
};

// ExecutePolicies<>

template<typename... Policies>
struct ExecutePolicies;

template<>
struct ExecutePolicies<> {
    template<typename T, int Index>
    struct With {
        typedef T type;
    };
};

template<typename Policy, typename... Remaining>
struct ExecutePolicies<Policy, Remaining...> {
    template<typename T, int Index>
    struct With {
        typedef typename Policy::template Transform<
            typename ExecutePolicies<Remaining...>::template With<T, Index>::type,
            Index
        >::type type;
    };
};

// TypeList<>

template<typename...>
struct TypeList {};

// Cons :: T, TypeList<types...> -> Cons<T, types...>

template<typename First, typename TypeList>
struct Cons;

template<typename First, typename... Rest>
struct Cons<First, TypeList<Rest...>> {
    typedef TypeList<First, Rest...> type;
};

// Apply :: T, TypeList<types...> -> T<types...>

template<template<typename...> class Output, typename TypeList>
struct Apply;

template<template<typename...> class Output, typename... Types>
struct Apply<Output, TypeList<Types...>> {
    typedef Output<Types...> type;
};

// MapWithIndex_

template<template<size_t, typename> class Mapper, size_t CurrentIndex, typename... Args>
struct MapWithIndex_;

template<template<size_t, typename> class Mapper, size_t CurrentIndex, typename First, typename... Rest>
struct MapWithIndex_<Mapper, CurrentIndex, First, Rest...> {
    typedef typename Cons<
        typename Mapper<CurrentIndex, First>::type,
        typename MapWithIndex_<Mapper, CurrentIndex + 1, Rest...>::type
        >::type type;
};

template<template<size_t, typename> class Mapper, size_t CurrentIndex>
struct MapWithIndex_<Mapper, CurrentIndex> {
    typedef TypeList<> type;
};

template<template<typename...> class Output, template<size_t, typename> class Mapper, typename... Args>
struct MapWithIndex {
    typedef typename internal::Apply<
        Output,
        typename MapWithIndex_<Mapper, 0, Args...>::type
    >::type type;
};


template<typename ArgList>
struct ArgArrayGetter;

template<typename... Args>
struct ArgArrayGetter<TypeList<Args...>> {
    static const TYPEID* get() {
        thread_local const TYPEID types[] = { TypeID<Args>::get()... };
        return types;
    }
};

// WithPolicies<...>::ArgTypeList<...>

template<typename... Policies>
struct WithPolicies {
    template<size_t Index, typename T>
    struct MapWithPolicies {
        typedef typename ExecutePolicies<Policies...>::template With<T, Index>::type type;
    };

    template<typename... Args>
    struct ArgTypeList {
        unsigned getCount() const {
            return sizeof...(Args);
        }

        const TYPEID* getTypes() const {
            return ArgArrayGetter<
                typename MapWithIndex<TypeList, MapWithPolicies, Args...>::type
            >::get();
        }
    };
};

// BindingType<T>

// The second typename is an unused stub so it's possible to
// specialize groups of classes via SFINAE.
template<typename T, typename = void>
struct BindingType;

template<typename T>
void register_num_type(TYPEID id, const char* name) {
  using namespace internal;
  if constexpr (std::is_floating_point<T>::value) {
    _embind_register_float(id, name, sizeof(T));
  } else {
    static_assert(std::is_integral<T>::value, "Not a numeric type");
    if constexpr (sizeof(T) < 8) {
      _embind_register_integer(id,
                                name,
                                sizeof(T),
                                std::numeric_limits<T>::min(),
                                std::numeric_limits<T>::max());
    } else {
      _embind_register_bigint(id,
                              name,
                              sizeof(T),
                              std::numeric_limits<T>::min(),
                              std::numeric_limits<T>::max());
    }
  }
}

// matches typeMapping in embind.js
enum TypedArrayIndex {
  Int8Array,
  Uint8Array,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  // Only available if WASM_BIGINT
  Int64Array,
  Uint64Array,
};

template<typename T>
constexpr TypedArrayIndex getTypedArrayIndex() {
  if constexpr (std::is_floating_point<T>::value) {
    switch (sizeof(T)) {
      case 4:
        return Float32Array;
      case 8:
        return Float64Array;
    }
  } else {
    static_assert(std::is_integral<T>::value, "Not a numeric type");
    switch (sizeof(T)) {
      case 1:
        return std::is_signed<T>::value ? Int8Array : Uint8Array;
      case 2:
        return std::is_signed<T>::value ? Int16Array : Uint16Array;
      case 4:
        return std::is_signed<T>::value ? Int32Array : Uint32Array;
      case 8:
        return std::is_signed<T>::value ? Int64Array : Uint64Array;
    }
    // if constexpr reaches here, compiler will complain about no return value
    // if we accidentally pass unsupported type
  }
}

// Note: the non-word-sized WireType in BindingType<memory_view<...>> only works
// because I happen to know that clang will pass aggregates as pointers to stack
// elements and we never support converting JavaScript typed arrays back into
// memory_view.  (That is, fromWireType is not implemented
// on the C++ side, nor is toWireType implemented in
// JavaScript.)
#define EMBIND_DEFINE_NUM_TYPE(T)                                              \
  template<> struct register_js<T> {                                           \
    register_js(TYPEID id) { register_num_type<T>(id, #T); }                   \
  };                                                                           \
  template<> struct BindingType<T> {                                           \
    typedef T WireType;                                                        \
    constexpr static WireType toWireType(const T& v) { return v; }             \
    constexpr static T fromWireType(WireType v) { return v; }                  \
  };                                                                           \
  template<> struct register_js<memory_view<T>> {                              \
    register_js(TYPEID id) {                                                   \
      _embind_register_memory_view(                                            \
        id, getTypedArrayIndex<T>(), "emscripten::memory_view<" #T ">");       \
    }                                                                          \
  };                                                                           \
  template<> struct BindingType<memory_view<T>> {                              \
    typedef memory_view<T> WireType;                                           \
    static WireType toWireType(const memory_view<T>& mv) { return mv; }        \
  };

EMBIND_DEFINE_NUM_TYPE(char);
EMBIND_DEFINE_NUM_TYPE(signed char);
EMBIND_DEFINE_NUM_TYPE(unsigned char);
EMBIND_DEFINE_NUM_TYPE(signed short);
EMBIND_DEFINE_NUM_TYPE(unsigned short);
EMBIND_DEFINE_NUM_TYPE(signed int);
EMBIND_DEFINE_NUM_TYPE(unsigned int);
EMBIND_DEFINE_NUM_TYPE(signed long);
EMBIND_DEFINE_NUM_TYPE(unsigned long);
EMBIND_DEFINE_NUM_TYPE(signed long long);
EMBIND_DEFINE_NUM_TYPE(unsigned long long);
EMBIND_DEFINE_NUM_TYPE(float);
EMBIND_DEFINE_NUM_TYPE(double);

#undef EMBIND_DEFINE_NUM_TYPE

template<>
struct register_js<void> {
    register_js(TYPEID id) {
        _embind_register_void(id, "void");
    }
};

template<>
struct BindingType<void> {
    typedef void WireType;
};

template<>
struct register_js<bool> {
    register_js(TYPEID id) {
        static_assert(sizeof(bool) == 1);
        _embind_register_bool(id, "bool", true, false);
    }
};

template<>
struct BindingType<bool> {
    typedef bool WireType;
    static WireType toWireType(bool b) {
        return b;
    }
    static bool fromWireType(WireType wt) {
        return wt;
    }
};

template<typename String>
constexpr const char *getStringTypeName() = delete;

template<> constexpr const char *getStringTypeName<std::string>() {
    return "std::string";
}

template<> constexpr const char *getStringTypeName<std::basic_string<unsigned char>>() {
    return "std::basic_string<unsigned char>";
}

template<> constexpr const char *getStringTypeName<std::wstring>() {
    return "std::wstring";
}

template<> constexpr const char *getStringTypeName<std::u16string>() {
    return "std::u16string";
}

template<> constexpr const char *getStringTypeName<std::u32string>() {
    return "std::u32string";
}

template<typename T>
struct register_js<std::basic_string<T>> {
    register_js(TYPEID id) {
        using String = std::basic_string<T>;
        if constexpr (sizeof(T) == 1) {
            _embind_register_std_string(id, getStringTypeName<String>());
        } else {
            _embind_register_std_wstring(id, sizeof(T), getStringTypeName<String>());
        }
    }
};

template<typename T>
struct BindingType<std::basic_string<T>> {
    using String = std::basic_string<T>;
    static_assert(std::is_trivially_copyable<T>::value, "basic_string elements are memcpy'd");
    typedef struct {
        size_t length;
        T data[1]; // trailing data
    }* WireType;
    static WireType toWireType(const String& v) {
        WireType wt = (WireType)malloc(sizeof(size_t) + v.length() * sizeof(T));
        wt->length = v.length();
        memcpy(wt->data, v.data(), v.length() * sizeof(T));
        return wt;
    }
    static String fromWireType(WireType v) {
        return String(v->data, v->length);
    }
};

template<typename T>
struct BindingType<const T> : public BindingType<T> {
};

template<typename T>
struct BindingType<T&> : public BindingType<T> {
};

template<typename T>
struct BindingType<const T&> : public BindingType<T> {
};

template<typename T>
struct BindingType<T&&> {
    typedef typename BindingType<T>::WireType WireType;
    static WireType toWireType(const T& v) {
        return BindingType<T>::toWireType(v);
    }
    static T fromWireType(WireType wt) {
        return BindingType<T>::fromWireType(wt);
    }
};

template<typename T>
struct BindingType<T*> {
    typedef T* WireType;
    static WireType toWireType(T* p) {
        return p;
    }
    static T* fromWireType(WireType wt) {
        return wt;
    }
};

template<typename T>
struct GenericBindingType {
    typedef typename std::remove_reference<T>::type ActualT;
    typedef ActualT* WireType;

    static WireType toWireType(const T& v) {
        return new T(v);
    }

    static WireType toWireType(T&& v) {
        return new T(std::forward<T>(v));
    }

    static ActualT& fromWireType(WireType p) {
        return *p;
    }
};

template<typename T>
struct GenericBindingType<std::unique_ptr<T>> {
    typedef typename BindingType<T*>::WireType WireType;

    static WireType toWireType(std::unique_ptr<T> p) {
        return BindingType<T*>::toWireType(p.release());
    }

    static std::unique_ptr<T> fromWireType(WireType wt) {
        return std::unique_ptr<T>(BindingType<T*>::fromWireType(wt));
    }
};

template<typename Enum>
struct EnumBindingType {
    typedef Enum WireType;

    static WireType toWireType(Enum v) {
        return v;
    }
    static Enum fromWireType(WireType v) {
        return v;
    }
};

// catch-all generic binding
template<typename T, typename>
struct BindingType : std::conditional<
    std::is_enum<T>::value,
    EnumBindingType<T>,
    GenericBindingType<T> >::type
{};

template<typename T>
auto toWireType(T&& v) -> typename BindingType<T>::WireType {
    return BindingType<T>::toWireType(std::forward<T>(v));
}

template<typename T>
constexpr bool typeSupportsMemoryView() {
    return (std::is_floating_point<T>::value &&
                (sizeof(T) == 4 || sizeof(T) == 8)) ||
            (std::is_integral<T>::value &&
                (sizeof(T) == 1 || sizeof(T) == 2 ||
                 sizeof(T) == 4 || sizeof(T) == 8));
}

} // namespace internal

// Note that 'data' is marked const just so it can accept both
// const and nonconst pointers.  It is certainly possible for
// JavaScript to modify the C heap through the typed array given,
// as it merely aliases the C heap.
template<typename T>
inline memory_view<T> typed_memory_view(size_t size, const T* data) {
  static_assert(internal::typeSupportsMemoryView<T>(),
                "type of typed_memory_view is invalid");
  return memory_view<T>(size, data);
}

} // namespace emscripten

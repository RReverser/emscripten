/*
 * Copyright 2012 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#if __cplusplus < 201103L
#error Including <emscripten/val.h> requires building with -std=c++11 or newer!
#endif

#include <cassert>
#include <array>
#include <climits>
#include <emscripten/wire.h>
#include <cstdint> // uintptr_t
#include <vector>
#include <type_traits>
#if __cplusplus >= 202002L
#include <coroutine>
#include <variant>
#endif


namespace emscripten {

class val;

typedef struct _EM_VAL* EM_VAL;

namespace internal {

template<typename WrapperType>
val wrapped_extend(const std::string&, const val&);

enum class EM_METHOD_CALLER_KIND {
  FUNCTION = 0,
  CONSTRUCTOR = 1,
  METHOD = 2,
  CAST = 3,
};

// Implemented in JavaScript.  Don't call these directly.
extern "C" {

void _emval_register_symbol(const char*);

enum {
  _EMVAL_UNDEFINED = 1,
  _EMVAL_NULL = 2,
  _EMVAL_TRUE = 3,
  _EMVAL_FALSE = 4
};

typedef struct _EM_DESTRUCTORS* EM_DESTRUCTORS;

void _emval_incref(EM_VAL value);
void _emval_decref(EM_VAL value);

void _emval_run_destructors(EM_DESTRUCTORS handle);

EM_VAL _emval_new_array(void);
EM_VAL _emval_new_array_from_memory_view(EM_VAL mv);
EM_VAL _emval_new_object(void);
EM_VAL _emval_new_cstring(const char*);
EM_VAL _emval_new_u8string(const char*);
EM_VAL _emval_new_u16string(const char16_t*);

EM_VAL _emval_get_global(const char* name);
EM_VAL _emval_get_module_property(const char* name);
EM_VAL _emval_get_property(EM_VAL object, EM_VAL key);
void _emval_set_property(EM_VAL object, EM_VAL key, EM_VAL value);

bool _emval_equals(EM_VAL first, EM_VAL second);
bool _emval_strictly_equals(EM_VAL first, EM_VAL second);
bool _emval_greater_than(EM_VAL first, EM_VAL second);
bool _emval_less_than(EM_VAL first, EM_VAL second);
bool _emval_not(EM_VAL object);

// DO NOT call this more than once per signature. It will
// leak generated function objects!
void* _emval_get_method_caller(
    const char *sig,
    unsigned argCount, // including return value
    const TYPEID argTypes[],
    EM_METHOD_CALLER_KIND kind);
EM_VAL _emval_typeof(EM_VAL value);
bool _emval_instanceof(EM_VAL object, EM_VAL constructor);
bool _emval_is_number(EM_VAL object);
bool _emval_is_string(EM_VAL object);
bool _emval_in(EM_VAL item, EM_VAL object);
bool _emval_delete(EM_VAL object, EM_VAL property);
[[noreturn]] bool _emval_throw(EM_VAL object);
EM_VAL _emval_await(EM_VAL promise);
EM_VAL _emval_iter_begin(EM_VAL iterable);
EM_VAL _emval_iter_next(EM_VAL iterator);

#if __cplusplus >= 202002L
void _emval_coro_suspend(EM_VAL promise, void* coro_ptr);
EM_VAL _emval_coro_make_promise(EM_VAL *resolve, EM_VAL *reject);
#endif

} // extern "C"

template<const char* address>
struct symbol_registrar {
  symbol_registrar() {
    internal::_emval_register_symbol(address);
  }
};

struct DestructorsRunner {
public:
  EM_DESTRUCTORS destructors;

  DestructorsRunner() : destructors(nullptr) {}
  ~DestructorsRunner() {
    if (destructors) {
      _emval_run_destructors(destructors);
    }
  }

  DestructorsRunner(const DestructorsRunner&) = delete;
  void operator=(const DestructorsRunner&) = delete;
};

template<EM_METHOD_CALLER_KIND Kind, typename Ret, typename... Args>
struct Signature {
  using MethodCallerType = typename BindingType<Ret>::WireType (*) (EM_DESTRUCTORS *destructorsRef, typename BindingType<Args>::WireType...);

  template<typename... Policies>
  static Ret invoke(Args&&... args) {
    using namespace internal;

    static constexpr typename WithPolicies<Policies...>::template ArgTypeList<Ret, Args...> argsSig;
    thread_local auto method_caller = (MethodCallerType)_emval_get_method_caller(getSignature((MethodCallerType)nullptr), argsSig.getCount(), argsSig.getTypes(), Kind);

    DestructorsRunner rd;
    if constexpr (std::is_same_v<Ret, void>) {
      method_caller(&rd.destructors, toWireType(std::forward<Args>(args))...);
    } else {
      return BindingType<Ret>::fromWireType(method_caller(&rd.destructors, toWireType(std::forward<Args>(args))...));
    }
  }
};

} // end namespace internal

#define EMSCRIPTEN_SYMBOL(name)                                         \
static const char name##_symbol[] = #name;                          \
static const ::emscripten::internal::symbol_registrar<name##_symbol> name##_registrar

class val {
public:
  // missing operators:
  // * ~ - + ++ --
  // * * / %
  // * + -
  // * << >> >>>
  // * & ^ | && || ?:
  //
  // exposing void, comma, and conditional is unnecessary
  // same with: = += -= *= /= %= <<= >>= >>>= &= ^= |=

  static val array() {
    return val(internal::_emval_new_array());
  }

  template<typename Iter>
  static val array(Iter begin, Iter end) {
#if __cplusplus >= 202002L
    if constexpr (std::contiguous_iterator<Iter> &&
                  internal::typeSupportsMemoryView<
                    typename std::iterator_traits<Iter>::value_type>()) {
      val view{ typed_memory_view(std::distance(begin, end), std::to_address(begin)) };
      return val(internal::_emval_new_array_from_memory_view(view.as_handle()));
    }
    // For numeric arrays, following codes are unreachable and the compiler
    // will do 'dead code elimination'.
    // Others fallback old way.
#endif
    val new_array = array();
    for (auto it = begin; it != end; ++it) {
      new_array.call<void>("push", *it);
    }
    return new_array;
  }

  template<typename T>
  static val array(const std::vector<T>& vec) {
    if constexpr (internal::typeSupportsMemoryView<T>()) {
        // for numeric types, pass memory view and copy in JS side one-off
        val view{ typed_memory_view(vec.size(), vec.data()) };
        return val(internal::_emval_new_array_from_memory_view(view.as_handle()));
    } else {
        return array(vec.begin(), vec.end());
    }
  }

  static val object() {
    return val(internal::_emval_new_object());
  }

  static val u8string(const char* s) {
    return val(internal::_emval_new_u8string(s));
  }

  static val u16string(const char16_t* s) {
    return val(internal::_emval_new_u16string(s));
  }

  static val undefined() {
    return val(EM_VAL(internal::_EMVAL_UNDEFINED));
  }

  static val null() {
    return val(EM_VAL(internal::_EMVAL_NULL));
  }

  static val take_ownership(EM_VAL e) {
    return val(e);
  }

  static val global(const char* name = 0) {
    return val(internal::_emval_get_global(name));
  }

  static val module_property(const char* name) {
    return val(internal::_emval_get_module_property(name));
  }

  template<typename T>
  explicit val(T&& value) {
    using namespace internal;

    new (this) val(Signature<EM_METHOD_CALLER_KIND::CAST, val, T>::template
      invoke<>(std::forward<T>(value)));
  }

  val() : val(EM_VAL(internal::_EMVAL_UNDEFINED)) {}

  explicit val(const char* v)
      : val(internal::_emval_new_cstring(v))
  {}

  // Note: unlike other constructors, this doesn't use as_handle() because
  // it just moves a value and doesn't need to go via incref/decref.
  // This means it's safe to move values across threads - an error will
  // only arise if you access or free it from the wrong thread later.
  val(val&& v) : handle(v.handle), thread(v.thread) {
    v.handle = 0;
  }

  val(const val& v) : val(v.as_handle()) {
    internal::_emval_incref(handle);
  }

  ~val() {
    if (handle) {
      internal::_emval_decref(as_handle());
      handle = 0;
    }
  }

  EM_VAL as_handle() const {
#ifdef _REENTRANT
    assert(pthread_equal(thread, pthread_self()) && "val accessed from wrong thread");
#endif
    return handle;
  }

  val& operator=(val&& v) & {
    val tmp(std::move(v));
    this->~val();
    new (this) val(std::move(tmp));
    return *this;
  }

  val& operator=(const val& v) & {
    return *this = val(v);
  }

  bool hasOwnProperty(const char* key) const {
    return val::global("Object")["prototype"]["hasOwnProperty"].call<bool>("call", *this, val(key));
  }

  bool isNull() const {
    return as_handle() == EM_VAL(internal::_EMVAL_NULL);
  }

  bool isUndefined() const {
    return as_handle() == EM_VAL(internal::_EMVAL_UNDEFINED);
  }

  bool isTrue() const {
    return as_handle() == EM_VAL(internal::_EMVAL_TRUE);
  }

  bool isFalse() const {
    return as_handle() == EM_VAL(internal::_EMVAL_FALSE);
  }

  bool isNumber() const {
    return internal::_emval_is_number(as_handle());
  }

  bool isString() const {
    return internal::_emval_is_string(as_handle());
  }

  bool isArray() const {
    return instanceof(global("Array"));
  }

  bool equals(const val& v) const {
    return internal::_emval_equals(as_handle(), v.as_handle());
  }

  bool operator==(const val& v) const {
    return equals(v);
  }

  bool operator!=(const val& v) const {
    return !equals(v);
  }

  bool strictlyEquals(const val& v) const {
    return internal::_emval_strictly_equals(as_handle(), v.as_handle());
  }

  bool operator>(const val& v) const {
    return internal::_emval_greater_than(as_handle(), v.as_handle());
  }

  bool operator>=(const val& v) const {
    return (*this > v) || (*this == v);
  }

  bool operator<(const val& v) const {
    return internal::_emval_less_than(as_handle(), v.as_handle());
  }

  bool operator<=(const val& v) const {
    return (*this < v) || (*this == v);
  }

  bool operator!() const {
    return internal::_emval_not(as_handle());
  }

  template<typename T>
  val operator[](const T& key) const {
    return val(internal::_emval_get_property(as_handle(), val_ref(key).as_handle()));
  }

  template<typename K, typename V>
  void set(const K& key, const V& value) {
    internal::_emval_set_property(as_handle(), val_ref(key).as_handle(), val_ref(value).as_handle());
  }

  template<typename T>
  bool delete_(const T& property) const {
    return internal::_emval_delete(as_handle(), val_ref(property).as_handle());
  }

  template<typename... Args>
  val new_(Args&&... args) const {
    using namespace internal;

    return Signature<EM_METHOD_CALLER_KIND::CONSTRUCTOR, val, const val&, Args&&...>::template
      invoke<>(*this, std::forward<Args>(args)...);
  }

  template<typename... Args>
  val operator()(Args&&... args) const {
    using namespace internal;

    return Signature<EM_METHOD_CALLER_KIND::FUNCTION, val, const val&, Args&&...>::template
      invoke<>(*this, std::forward<Args>(args)...);
  }

  template<typename ReturnValue, typename... Args>
  ReturnValue call(const char* name, Args&&... args) const {
    using namespace internal;

    return Signature<EM_METHOD_CALLER_KIND::METHOD, ReturnValue, const val&, const val&, Args&&...>::template
      invoke<>(*this, val(name), std::forward<Args>(args)...);
  }

  template<typename T, typename... Policies>
  T as(Policies...) const {
    using namespace internal;

    return Signature<EM_METHOD_CALLER_KIND::CAST, T, const val&>::template
      invoke<Policies...>(*this);
  }

// Prefer calling val::typeOf() over val::typeof(), since this form works in both C++11 and GNU++11 build modes. "typeof" is a reserved word in GNU++11 extensions.
  val typeOf() const {
    return val(internal::_emval_typeof(as_handle()));
  }

// If code is not being compiled with GNU extensions enabled, typeof() is a valid identifier, so support that as a member function.
#if __is_identifier(typeof)
  [[deprecated("Use typeOf() instead.")]]
  val typeof() const {
    return typeOf();
  }
#endif

  bool instanceof(const val& v) const {
    return internal::_emval_instanceof(as_handle(), v.as_handle());
  }

  bool in(const val& v) const {
    return internal::_emval_in(as_handle(), v.as_handle());
  }

  [[noreturn]] void throw_() const {
    internal::_emval_throw(as_handle());
  }

  val await() const {
    return val(internal::_emval_await(as_handle()));
  }

  struct iterator;

  iterator begin() const;
  // our iterators are sentinel-based range iterators; use nullptr as the end sentinel
  constexpr nullptr_t end() const { return nullptr; }

#if __cplusplus >= 202002L
  class awaiter;
  awaiter operator co_await() const;

  class promise_type;
#endif

private:
  // takes ownership, assumes handle already incref'd and lives on the same thread
  explicit val(EM_VAL handle)
      : handle(handle), thread(pthread_self())
  {}

  template<typename WrapperType>
  friend val internal::wrapped_extend(const std::string& , const val& );

  template<typename T>
  val val_ref(const T& v) const {
    return val(v);
  }

  const val& val_ref(const val& v) const {
    return v;
  }

  pthread_t thread;
  EM_VAL handle;

  friend struct internal::BindingType<val>;
};

struct val::iterator {
  iterator() = delete;
  // Make sure iterator is only moveable, not copyable as it represents a mutable state.
  iterator(iterator&&) = default;
  iterator(const val& v) : iter(internal::_emval_iter_begin(v.as_handle())) {
    this->operator++();
  }
  val&& operator*() { return std::move(cur_value); }
  const val& operator*() const { return cur_value; }
  void operator++() { cur_value = val(internal::_emval_iter_next(iter.as_handle())); }
  bool operator!=(nullptr_t) const { return cur_value.as_handle() != nullptr; }

private:
  val iter;
  val cur_value;
};

inline val::iterator val::begin() const {
  return iterator(*this);
}

#if __cplusplus >= 202002L
// Awaiter defines a set of well-known methods that compiler uses
// to drive the argument of the `co_await` operator (regardless
// of the type of the parent coroutine).
// This one is used for Promises represented by the `val` type.
class val::awaiter {
  // State machine holding awaiter's current state. One of:
  //  - initially created with promise
  //  - waiting with a given coroutine handle
  //  - completed with a result
  std::variant<val, std::coroutine_handle<val::promise_type>, val> state;

  constexpr static std::size_t STATE_PROMISE = 0;
  constexpr static std::size_t STATE_CORO = 1;
  constexpr static std::size_t STATE_RESULT = 2;

public:
  awaiter(const val& promise)
    : state(std::in_place_index<STATE_PROMISE>, promise) {}

  // just in case, ensure nobody moves / copies this type around
  awaiter(awaiter&&) = delete;

  // Promises don't have a synchronously accessible "ready" state.
  bool await_ready() { return false; }

  // On suspend, store the coroutine handle and invoke a helper that will do
  // a rough equivalent of `promise.then(value => this.resume_with(value))`.
  void await_suspend(std::coroutine_handle<val::promise_type> handle) {
    internal::_emval_coro_suspend(std::get<STATE_PROMISE>(state).as_handle(), this);
    state.emplace<STATE_CORO>(handle);
  }

  // When JS invokes `resume_with` with some value, store that value and resume
  // the coroutine.
  void resume_with(val&& result) {
    auto coro = std::move(std::get<STATE_CORO>(state));
    state.emplace<STATE_RESULT>(std::move(result));
    coro.resume();
  }

  // `await_resume` finalizes the awaiter and should return the result
  // of the `co_await ...` expression - in our case, the stored value.
  val await_resume() { return std::move(std::get<STATE_RESULT>(state)); }
};

inline val::awaiter val::operator co_await() const {
  return {*this};
}

// `promise_type` is a well-known subtype with well-known method names
// that compiler uses to drive the coroutine itself
// (`T::promise_type` is used for any coroutine with declared return type `T`).
class val::promise_type {
  val promise, resolve, reject_with_current_exception;

public:
  // Create a `new Promise` and store it alongside the `resolve` and `reject`
  // callbacks that can be used to fulfill it.
  promise_type() {
    EM_VAL resolve_handle;
    EM_VAL reject_handle;
    promise = val(internal::_emval_coro_make_promise(&resolve_handle, &reject_handle));
    resolve = val(resolve_handle);
    reject_with_current_exception = val(reject_handle);
  }

  // Return the stored promise as the actual return value of the coroutine.
  val get_return_object() { return promise; }

  // For similarity with JS async functions, our coroutines are eagerly evaluated.
  auto initial_suspend() noexcept { return std::suspend_never{}; }
  auto final_suspend() noexcept { return std::suspend_never{}; }

  // On an unhandled exception, reject the stored promise instead of throwing
  // it asynchronously where it can't be handled.
  void unhandled_exception() {
    reject_with_current_exception();
  }

  // Resolve the stored promise on `co_return value`.
  template<typename T>
  void return_value(T&& value) {
    resolve(std::forward<T>(value));
  }
};
#endif

// Declare a custom type that can be used in conjunction with
// emscripten::register_type to emit custom TypeScript definitions for val
// types.
#define EMSCRIPTEN_DECLARE_VAL_TYPE(name)                                      \
struct name : public ::emscripten::val {                                       \
  explicit name(val const &other) : val(other) {}                              \
};

namespace internal {

template<typename T>
struct BindingType<T, typename std::enable_if<std::is_base_of<val, T>::value &&
                                              !std::is_const<T>::value>::type> {
  typedef EM_VAL WireType;
  static WireType toWireType(const val& v) {
    EM_VAL handle = v.as_handle();
    _emval_incref(handle);
    return handle;
  }
  static T fromWireType(WireType v) {
    return T(val::take_ownership(v));
  }
};

}

template <typename T, typename... Policies>
std::vector<T> vecFromJSArray(const val& v, Policies... policies) {
  const uint32_t l = v["length"].as<uint32_t>();

  std::vector<T> rv;
  rv.reserve(l);
  for (uint32_t i = 0; i < l; ++i) {
    rv.push_back(v[i].as<T>(std::forward<Policies>(policies)...));
  }

  return rv;
}

template <typename T>
std::vector<T> convertJSArrayToNumberVector(const val& v) {
  const size_t l = v["length"].as<size_t>();

  std::vector<T> rv;
  rv.resize(l);

  // Copy the array into our vector through the use of typed arrays.
  // It will try to convert each element through Number().
  // See https://www.ecma-international.org/ecma-262/6.0/#sec-%typedarray%.prototype.set-array-offset
  // and https://www.ecma-international.org/ecma-262/6.0/#sec-tonumber
  val memoryView{ typed_memory_view(l, rv.data()) };
  memoryView.call<void>("set", v);

  return rv;
}

} // end namespace emscripten

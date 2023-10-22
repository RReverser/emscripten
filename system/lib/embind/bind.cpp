// Copyright 2012 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <emscripten/bind.h>
#ifdef USE_CXA_DEMANGLE
#include <../lib/libcxxabi/include/cxxabi.h>
#endif
#include <algorithm>
#include <climits>
#include <emscripten/emscripten.h>
#include <emscripten/wire.h>
#include <limits>
#include <list>
#include <typeinfo>
#include <vector>

using namespace emscripten;
using namespace internal;

extern "C" {
const char* EMSCRIPTEN_KEEPALIVE __getTypeName(const std::type_info* ti) {
  if (has_unbound_type_names) {
#ifdef USE_CXA_DEMANGLE
    int stat;
    char* demangled = abi::__cxa_demangle(ti->name(), NULL, NULL, &stat);
    if (stat == 0 && demangled) {
      return demangled;
    }

    switch (stat) {
      case -1:
        return strdup("<allocation failure>");
      case -2:
        return strdup("<invalid C++ symbol>");
      case -3:
        return strdup("<invalid argument>");
      default:
        return strdup("<unknown error>");
    }
#else
    return strdup(ti->name());
#endif
  } else {
    char str[80];
    sprintf(str, "%p", reinterpret_cast<const void*>(ti));
    return strdup(str);
  }
}

static InitFunc* init_funcs = nullptr;

EMSCRIPTEN_KEEPALIVE void _embind_initialize_bindings() {
  for (auto* f = init_funcs; f; f = f->next) {
    f->init_func();
  }
}

void _embind_register_bindings(InitFunc* f) {
  f->next = init_funcs;
  init_funcs = f;
}

}

EMSCRIPTEN_BINDINGS(builtin) {
  using namespace emscripten::internal;

  _embind_register_emval(TypeID<val>::get(), "emscripten::val");
}

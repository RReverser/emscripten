/**
 * @license
 * Copyright 2022 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */

addToLibrary({
  // Returns the C function with a specified identifier (for C++, you need to do manual name mangling)
  $getCFunc: (ident) => {
    var func = Module['_' + ident]; // closure exported function
#if ASSERTIONS
    assert(func, 'Cannot call unknown function ' + ident + ', make sure it is exported');
#endif
    return func;
  },

  // C calling interface.
  $ccall__deps: ['$getCFunc', '$writeArrayToMemory', '$stringToUTF8OnStack'],
  $ccall__docs: `
  /**
   * @param {string|null=} returnType
   * @param {Array=} argTypes
   * @param {Arguments|Array=} args
   * @param {Object=} opts
   */`,
  $ccall: (ident, returnType, argTypes, args, opts) => {
    // For fast lookup of conversion functions
    var toC = {
#if MEMORY64
      'pointer': (p) => {{{ to64('p') }}},
#endif
      'string': (str) => {
        var ret = 0;
        if (str !== null && str !== undefined && str !== 0) { // null string
          // at most 4 bytes per UTF-8 code point, +1 for the trailing '\0'
          ret = stringToUTF8OnStack(str);
        }
        return {{{ to64('ret') }}};
      },
      'array': (arr) => {
        var ret = stackAlloc(arr.length);
        writeArrayToMemory(arr, ret);
        return {{{ to64('ret') }}};
      }
    };

    function convertReturnValue(ret) {
      if (returnType === 'string') {
        {{{ from64('ret') }}}
        return UTF8ToString(ret);
      }
#if MEMORY64
      if (returnType === 'pointer') return Number(ret);
#endif
      if (returnType === 'boolean') return Boolean(ret);
      return ret;
    }

    var func = getCFunc(ident);
    var cArgs = [];
    var stack = 0;
#if ASSERTIONS
    assert(returnType !== 'array', 'Return type should not be "array".');
#endif
    if (args) {
      for (var i = 0; i < args.length; i++) {
        var converter = toC[argTypes[i]];
        if (converter) {
          if (stack === 0) stack = stackSave();
          cArgs[i] = converter(args[i]);
        } else {
          cArgs[i] = args[i];
        }
      }
    }
    var ret = func.apply(null, cArgs);
    function onDone(ret) {
      if (stack !== 0) stackRestore(stack);
      return convertReturnValue(ret);
    }
#if ASYNCIFY
    var asyncMode = opts?.async;

    // In regular Asyncify we handle mismatch between the async mode and return
    // value being a Promise in permissive way unless assertions are enabled.
#if ASYNCIFY == 1
    if (asyncMode) {
      ret = Promise.resolve(ret);
    } else if (ret instanceof Promise) {
#if ASSERTIONS
      assert(asyncMode, 'The call to ' + ident + ' is running asynchronously. If this was intended, add the async option to the ccall/cwrap call.');
#endif
      asyncMode = true;
    }
#endif
#endif

    // In JSPI we strictly require the async mode to match the return value being a promise.
#if ASYNCIFY == 2 && ASSERTIONS
    if (asyncMode && !(ret instanceof Promise)) {
      abort('The call to ' + ident + ' is running synchronously. If this was intended, remove the async option from the ccall/cwrap call.');
    }
#endif

    return asyncMode ? ret.then(onDone) : onDone(ret);
  },

  $cwrap__docs: `
  /**
   * @param {string=} returnType
   * @param {Array=} argTypes
   * @param {Object=} opts
   */`,
  $cwrap__deps: ['$getCFunc', '$ccall'],
  $cwrap: (ident, returnType, argTypes, opts) => {
#if !ASSERTIONS
    // When the function takes numbers and returns a number, we can just return
    // the original function
    var numericArgs = !argTypes || argTypes.every((type) => type === 'number' || type === 'boolean');
    var numericRet = returnType !== 'string';
    if (numericRet && numericArgs && !opts) {
      return getCFunc(ident);
    }
#endif
    return function() {
      return ccall(ident, returnType, argTypes, arguments, opts);
    }
  },
});

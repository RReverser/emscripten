"use strict";

var y = 1;
var z = fleefl();
var yy = 1,
  zz = fleefl();
// exported
function g(a) {
  return a + 1;
}
Module['g'] = g;

// used
function h(a) {
  // unused
  return a + 1;
}
print(h(123));

// inner workings
(function () {
  var y = 1;
  var z = fleefl();
  var yy = 1,
    zz = fleefl();
  // exported
  function g(a) {
    return a + 1;
  }
  Module['g'] = g;

  // used
  function hh(a) {
    // unused
    return a + 1;
  }
  print(hh(123));
})();
function glue() {}
glue();
var buffer = new ArrayBuffer(1024);

// unnecessary leftovers that seem to have side effects
"undefined" !== typeof TextDecoder && new TextDecoder("utf8");
new TextDecoder("utf8");
// for comparison, real side effects
new SomethingUnknownWithSideEffects("utf8");
new TextDecoder(Unknown());

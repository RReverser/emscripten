function a(a) {
  b(); // special word in JS, must not be confused by it
}
function b() {}
function c() {
  var a = 10;
  return a;
}
function d(f, g, h) {
  var i, j, k, l;
  i = f;
  j = g;
  k = h;
  l = f;
  return f + g + h + i + j + k + l;
}
function l() {
  a: while (1) {
    b: while (1) {
      if (c()) {
        if (d()) {
          break a;
        } else {
          continue b;
        }
      } else {
        if (d()) {
          break;
        } else {
          continue;
        }
      }
    }
  }
  // Another loop with the same name
  a: while (1) {}
}

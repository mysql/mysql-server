/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include "fiblib.h"
long fib (int n) {
  if (n<=2) return n;
  else {
    long a,b;
    a = _Cilk_spawn fib(n-1);
    b = _Cilk_spawn fib(n-2);
    _Cilk_sync;
    return a+b;
  }
}

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

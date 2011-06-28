#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "fiblib.h"

int main (int argc, char *argv[]) {
  assert(argc==2);
  int n = atoi(argv[1]);
  long fn = fib(n);
  printf("fib(%d)=%ld\n", n, fn);
  return 0;
}

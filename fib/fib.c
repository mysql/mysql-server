/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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

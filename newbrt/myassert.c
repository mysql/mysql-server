/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "myassert.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef TESTER
void my_assert(int a, const char *f, int l) {
    if (!a) { fprintf(stderr, "Assertion failed at %s:%d\n", f, l); abort(); }
}
#endif

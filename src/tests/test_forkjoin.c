/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>
#include <assert.h>

#include <toku_pthread.h>

static void *
f (void *arg) {
    //toku_pthread_exit(arg);  // toku_pthread_exit has a memory leak on linux
    return arg;
}

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    toku_pthread_t t;
    int r = toku_pthread_create(&t, 0, f, 0); assert(r == 0);
    void *ret;
    r = toku_pthread_join(t, &ret); assert(r == 0);
    return 0;
}

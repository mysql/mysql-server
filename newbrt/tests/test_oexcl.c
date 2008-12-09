/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define fname __FILE__ ".tmp"

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    unlink(fname);
    int fd0 = open (fname, O_RDWR|O_CREAT|O_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd0>=0);
    int fd1 = open (fname, O_RDWR|O_CREAT|O_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd1==-1);
    assert(errno==EEXIST);
    return 0;
}

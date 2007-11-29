/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define fname __FILE__ ".tmp"

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    unlink(fname);
    int fd0 = open (fname, O_RDWR|O_CREAT|O_EXCL, 0777);
    assert(fd0>=0);
    int fd1 = open (fname, O_RDWR|O_CREAT|O_EXCL, 0777);
    assert(fd1==-1);
    assert(errno==EEXIST);
    return 0;
}

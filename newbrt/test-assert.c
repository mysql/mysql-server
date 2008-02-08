/* -*- mode: C; c-basic-offset: 4 -*- */
#include "toku_assert.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void catch_abort (int sig __attribute__((__unused__))) {
    exit(1);
}

int main (int argc, const char *argv[]) {
    signal (SIGABRT, catch_abort);
    if (argc!=2) { printf("argcount should be 2.\n");  exit(1); }
    const char *str=argv[1];
    assert(strcmp(str,"ok")==0);
    return 0;
}

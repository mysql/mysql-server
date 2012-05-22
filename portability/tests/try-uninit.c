/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <stdio.h>
#include <stdlib.h>

static void foo(int i) {
    printf("%d\n", i);
}

int main(int argc, char *argv[]) {
    int arg;
    int i;
    for (i = 1; i < argc; i++) {
        arg = atoi(argv[i]);
    }
    foo(arg);
    return 0;
}

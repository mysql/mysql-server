/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <stdio.h>
#include <toku_assert.h>
#include <stdlib.h>
#include <signal.h>


static __attribute__((__noreturn__)) void catch_abort (int sig __attribute__((__unused__))) {
    exit(1);
}

int main(void) {
    signal (SIGABRT, catch_abort);
    int result = 42;
    assert_zero(result);
    return 0;
}

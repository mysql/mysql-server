/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <stdlib.h>

static void *vp;

int main(void) {
    vp = malloc(42);
    return 0;
}

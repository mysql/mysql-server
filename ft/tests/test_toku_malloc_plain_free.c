/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#include <toku_portability.h>
#include "memory.h"
#include "stdlib.h"

#include "test.h"

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    char *XMALLOC_N(5, m);
    m=m;
    toku_free(m);
    return 0;
}

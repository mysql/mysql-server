/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* We are going to test whether create and close properly check their input. */

#include "test.h"

enum { MAX_LOCKS = 1000, MAX_LOCK_MEMORY = MAX_LOCKS * 64 };

static void do_ltm_status(toku_ltm *ltm) {
    LTM_STATUS_S s;
    toku_ltm_get_status(ltm, &s);
    assert(s.status[LTM_LOCKS_LIMIT].value.num == MAX_LOCKS);
    assert(s.status[LTM_LOCKS_CURR].value.num == 0);
    assert(s.status[LTM_LOCK_MEMORY_LIMIT].value.num == MAX_LOCK_MEMORY);
    assert(s.status[LTM_LOCK_MEMORY_CURR].value.num == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);

    int r;

    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, MAX_LOCKS, MAX_LOCK_MEMORY, dbpanic);
    CKERR(r);

    do_ltm_status(ltm);

    toku_ltm_close(ltm);

    return 0;
}

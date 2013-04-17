/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;
CACHETABLE ct;

#define FNAME   __FILE__ ".brt"

static BRT
open_db(u_int32_t flags, char *subdb) {
    BRT t;
    int r;
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_flags(t, flags); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, FNAME, FNAME, subdb, 1, 0, ct, null_txn, (DB*)0);
    assert(r==0);
    return t;
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    int r;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(FNAME);
    BRT one;
    BRT two;
    BRT three;
    char *name1 = "dupsort";
    char *name2 = "dup and dupsort";
    char *name3 = "temp";
    one   = open_db(TOKU_DB_DUPSORT,               name1);
    two   = open_db(TOKU_DB_DUP | TOKU_DB_DUPSORT, name2);
    three = open_db(TOKU_DB_DUPSORT,               name3);
    r = toku_close_brt(one, 0, 0);        assert(r==0);
    r = toku_close_brt(two, 0, 0);        assert(r==0);
    r = toku_brt_remove_subdb(three, name1, 0);
    assert(r==0);
    two   = open_db(TOKU_DB_DUP | TOKU_DB_DUPSORT, name2);
    r = toku_close_brt(two, 0, 0);        assert(r==0);
    r = toku_close_brt(three, 0, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
    return 0;
}


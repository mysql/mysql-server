/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/time.h>
#include "test.h"

#include "ft-flusher.h"
#include "includes.h"
#include "checkpoint.h"

static TOKUTXN const null_txn = NULL;
static DB * const null_db = NULL;

static int
noop_getf(ITEMLEN UU(keylen), bytevec UU(key), ITEMLEN UU(vallen), bytevec UU(val), void *extra, bool UU(lock_only))
{
    int *calledp = extra;
    (*calledp)++;
    return 0;
}

static int
get_one_value(FT_HANDLE t, CACHETABLE UU(ct), void *UU(extra))
{
    int r;
    int called;
    FT_CURSOR cursor;

    r = toku_ft_cursor(t, &cursor, null_txn, false, false);
    CKERR(r);
    called = 0;
    r = toku_ft_cursor_first(cursor, noop_getf, &called);
    CKERR(r);
    assert(called == 1);
    r = toku_ft_cursor_close(cursor);
    CKERR(r);

    return 0;
}

static int
progress(void *extra, float fraction)
{
    float *stop_at = extra;
    if (fraction > *stop_at) {
        return 1;
    } else {
        return 0;
    }
}

static int
do_hot_optimize(FT_HANDLE t, CACHETABLE UU(ct), void *extra)
{
    float *fraction = extra;
    int r = toku_ft_hot_optimize(t, progress, extra);
    if (*fraction < 1.0) {
        CKERR2(r, 1);
    } else {
        CKERR(r);
    }
    return 0;
}

static int
insert_something(FT_HANDLE t, CACHETABLE UU(ct), void *UU(extra))
{
    assert(t);
    int r = 0;
    unsigned int dummy_value = 1 << 31;
    DBT key;
    DBT val;
    toku_fill_dbt(&key, &dummy_value, sizeof(unsigned int));
    toku_fill_dbt(&val, &dummy_value, sizeof(unsigned int));
    r = toku_ft_insert (t, &key, &val, 0);
    CKERR(r);
    return 0;
}

typedef int (*tree_cb)(FT_HANDLE t, CACHETABLE ct, void *extra);

static int
with_open_tree(const char *fname, tree_cb cb, void *cb_extra)
{
    int r, r2;
    FT_HANDLE t;
    CACHETABLE ct;

    r = toku_create_cachetable(&ct, 16*(1<<20), ZERO_LSN, NULL_LOGGER);
    CKERR(r);
    r = toku_open_ft_handle(fname, 
                      0, 
                      &t, 
                      4*(1<<20), 
                      128*(1<<10), 
                      TOKU_DEFAULT_COMPRESSION_METHOD,
                      ct, 
                      null_txn, 
                      toku_builtin_compare_fun
                      );
    CKERR(r);

    r2 = cb(t, ct, cb_extra);
    r = toku_verify_ft(t);
    CKERR(r);
    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    CKERR(r);
    r = toku_close_ft_handle_nolsn(t, 0);
    CKERR(r);
    r = toku_cachetable_close(&ct);
    CKERR(r);

    return r2;
}

#define TMPFTFMT "%s-tmpdata.ft_handle"
static const char *origft_5_0 = "upgrade_test_data.ft.5.0";
static const char *origft_4_2 = "upgrade_test_data.ft.4.2";
static const char *not_flat_4_2 = "upgrade_test_data.ft.4.2.not.flat";

static int
run_test(const char *prog, const char *origft) {
    int r;

    char *fullprog = toku_strdup(__FILE__);
    char *progdir = dirname(fullprog);

    size_t templen = strlen(progdir) + strlen(prog) + strlen(TMPFTFMT) - 1;
    char tempft[templen + 1];
    snprintf(tempft, templen + 1, TMPFTFMT, prog);
    size_t fullorigftlen = strlen(progdir) + strlen(origft) + 1;
    char fullorigft[fullorigftlen + 1];
    snprintf(fullorigft, fullorigftlen + 1, "%s/%s", progdir, origft);
    toku_free(fullprog);
    {
        size_t len = 4 + strlen(fullorigft) + strlen(tempft);
        char buf[len + 1];
        snprintf(buf, len + 1, "cp %s %s", fullorigft, tempft);
        r = system(buf);
        CKERR(r);
    }

    r = with_open_tree(tempft, get_one_value, NULL);
    CKERR(r);
    r = with_open_tree(tempft, insert_something, NULL);
    CKERR(r);
    float fraction = 0.5;
    r = with_open_tree(tempft, do_hot_optimize, &fraction);
    CKERR(r);
    fraction = 1.0;
    r = with_open_tree(tempft, do_hot_optimize, &fraction);
    CKERR(r);
    r = unlink(tempft);
    CKERR(r);

    return r;
}

int
test_main(int argc __attribute__((__unused__)), const char *argv[])
{
    int r;

    r = run_test(argv[0], origft_5_0);
    CKERR(r);
    r = run_test(argv[0], origft_4_2);
    CKERR(r);

    r = run_test(argv[0], not_flat_4_2);
    CKERR(r);

    return r;
}

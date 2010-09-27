/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

// test the LE_CURSOR next function

#include "includes.h"
#include "checkpoint.h"
#include "le-cursor.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static int 
test_brt_cursor_keycompare(DB *UU(db), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

static void
txn_yield(voidfp UU(f), void *UU(fv), void *UU(v)) {
    if (f)
        f(fv);
}

// create a tree and populate it with n rows
static void
create_populate_tree(const char *logdir, const char *fname, int n) {
    if (verbose) fprintf(stderr, "%s %s %s %d\n", __FUNCTION__, logdir, fname, n);
    int error;

    TOKULOGGER logger = NULL;
    error = toku_logger_create(&logger);
    assert(error == 0);
    error = toku_logger_open(logdir, logger);
    assert(error == 0);
    CACHETABLE ct = NULL;
    error = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, logger);
    assert(error == 0);
    toku_logger_set_cachetable(logger, ct);
    error = toku_logger_open_rollback(logger, ct, TRUE);
    assert(error == 0);

    TOKUTXN txn = NULL;
    error = toku_txn_begin_txn(NULL, &txn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    BRT brt = NULL;
    error = toku_open_brt(fname, 1, &brt, 1<<12, ct, txn, test_brt_cursor_keycompare, null_db);
    assert(error == 0);

    error = toku_txn_commit_txn(txn, TRUE, txn_yield, NULL, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    txn = NULL;
    error = toku_txn_begin_txn(NULL, &txn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    // insert keys 0, 1, 2, .. (n-1)
    for (int i = 0; i < n; i++) {
        int k = toku_htonl(i);
        int v = i;
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_insert(brt, &key, &val, txn);
        assert(error == 0);
    }

    error = toku_txn_commit_txn(txn, TRUE, txn_yield, NULL, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    error = toku_close_brt(brt, NULL);
    assert(error == 0);

    error = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);
    assert(error == 0);
    error = toku_logger_close_rollback(logger, FALSE);
    assert(error == 0);
    error = toku_logger_close(&logger);
    assert(error == 0);

    error = toku_cachetable_close(&ct);
    assert(error == 0);
}

// retrieve all of the leaf entries in the the tree and verify the key associated with each one.
// there should be n leaf entires in the tree.
static void 
walk_tree(const char *fname, int n) {
    if (verbose) fprintf(stderr, "%s %s %d\n", __FUNCTION__, fname, n);
    int error;

    CACHETABLE ct = NULL;
    error = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(error == 0);

    BRT brt = NULL;
    error = toku_open_brt(fname, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, null_db);
    assert(error == 0);

    LE_CURSOR cursor = NULL;
    error = le_cursor_create(&cursor, brt, NULL);
    assert(error == 0);

    DBT key;
    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    DBT val;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    int i;
    for (i = 0; ; i++) {
        error = le_cursor_next(cursor, &key, &val);
        if (error != 0) 
            break;
        assert(key.size == sizeof (int));
        int ii;
        memcpy(&ii, key.data, key.size);
        assert((int) toku_htonl(i) == ii);
        
        LEAFENTRY le = (LEAFENTRY) val.data;
        assert(le->attributes == LE_MVCC);
    }
    assert(i == n);

    toku_destroy_dbt(&key);
    toku_destroy_dbt(&val);

    error = le_cursor_close(cursor);
    assert(error == 0);

    error = toku_close_brt(brt, 0);
    assert(error == 0);

    error = toku_cachetable_close(&ct);
    assert(error == 0);
}

static void
init_logdir(const char *logdir) {
    int error;

    char cmd[32+strlen(logdir)];
    sprintf(cmd, "rm -rf %s", logdir);
    error = system(cmd);
    assert(error == 0);

    error = toku_os_mkdir(logdir, 0777);
    assert(error == 0);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    const char *logdir = "dir." __FILE__;
    init_logdir(logdir);
    int error = chdir(logdir);
    assert(error == 0);

    const int n = 1000;
    const char *brtfile =  __FILE__ ".brt";
    create_populate_tree(".", brtfile, n);
    walk_tree(brtfile, n);

    return 0;
}

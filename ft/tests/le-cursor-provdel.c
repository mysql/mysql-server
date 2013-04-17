/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

// test the LE_CURSOR next function with provisionally deleted rows

#include "includes.h"
#include "checkpoint.h"
#include "le-cursor.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static int
get_next_callback(ITEMLEN UU(keylen), bytevec UU(key), ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    DBT *val_dbt = cast_to_typeof(val_dbt) extra;
    if (!lock_only) {
        toku_dbt_set(vallen, val, val_dbt, NULL);
    }
    return 0;
}

static int
le_cursor_get_next(LE_CURSOR cursor, DBT *val) {
    int r = toku_le_cursor_next(cursor, get_next_callback, val);
    return r;
}

static int test_ft_cursor_keycompare(DB *desc __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
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
    error = toku_create_cachetable(&ct, 0, ZERO_LSN, logger);
    assert(error == 0);
    toku_logger_set_cachetable(logger, ct);
    error = toku_logger_open_rollback(logger, ct, TRUE);
    assert(error == 0);

    TOKUTXN txn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &txn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    FT_HANDLE brt = NULL;
    error = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, test_ft_cursor_keycompare);
    assert(error == 0);

    error = toku_txn_commit_txn(txn, TRUE, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    txn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &txn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    // insert keys 0, 1, 2, .. (n-1)
    for (int i = 0; i < n; i++) {
        int k = toku_htonl(i);
        int v = i;
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_ft_insert(brt, &key, &val, txn);
        assert(error == 0);
    }

    error = toku_txn_commit_txn(txn, TRUE, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    error = toku_close_ft_handle_nolsn(brt, NULL);
    assert(error == 0);

    error = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(error == 0);

    error = toku_logger_close_rollback(logger, FALSE);
    assert(error == 0);

    error = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(error == 0);

    error = toku_logger_shutdown(logger);
    CKERR(error);

    error = toku_logger_close(&logger);
    assert(error == 0);

    error = toku_cachetable_close(&ct);
    assert(error == 0);
}

// provionally delete all of the even keys
// the LE_CURSOR should see all of the leaf entries
static void 
test_provdel(const char *logdir, const char *fname, int n) {
    if (verbose) fprintf(stderr, "%s %s %s %d\n", __FUNCTION__, logdir, fname, n);
    int error;

    TOKULOGGER logger = NULL;
    error = toku_logger_create(&logger);
    assert(error == 0);
    error = toku_logger_open(logdir, logger);
    assert(error == 0);
    CACHETABLE ct = NULL;
    error = toku_create_cachetable(&ct, 0, ZERO_LSN, logger);
    assert(error == 0);
    toku_logger_set_cachetable(logger, ct);
    error = toku_logger_open_rollback(logger, ct, FALSE);
    assert(error == 0);

    TOKUTXN txn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &txn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    FT_HANDLE brt = NULL;
    error = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, test_ft_cursor_keycompare);
    assert(error == 0);

    error = toku_txn_commit_txn(txn, TRUE, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    txn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &txn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    // del keys 0, 2, 4, ...
    for (int i = 0; i < n; i += 2) {
        int k = toku_htonl(i);
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        error = toku_ft_delete(brt, &key, txn);
        assert(error == 0);
    }

    TOKUTXN cursortxn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &cursortxn, logger, TXN_SNAPSHOT_NONE);
    assert(error == 0);

    LE_CURSOR cursor = NULL;
    error = toku_le_cursor_create(&cursor, brt, cursortxn);
    assert(error == 0);

    DBT key;
    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    DBT val;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    int i;
    for (i=0; ; i++) {
        error = le_cursor_get_next(cursor, &val);
        if (error != 0) 
            break;
        
        LEAFENTRY le = (LEAFENTRY) val.data;
        assert(le->type == LE_MVCC);
        assert(le->keylen == sizeof (int));
        int ii;
        memcpy(&ii, le->u.mvcc.key_xrs, le->keylen);
        assert((int) toku_htonl(i) == ii);
    }
    assert(i == n);

    toku_destroy_dbt(&key);
    toku_destroy_dbt(&val);

    error = toku_le_cursor_close(cursor);
    assert(error == 0);

    error = toku_txn_commit_txn(cursortxn, TRUE, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(cursortxn);

    error = toku_txn_commit_txn(txn, TRUE, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    error = toku_close_ft_handle_nolsn(brt, NULL);
    assert(error == 0);

    error = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(error == 0);

    error = toku_logger_close_rollback(logger, FALSE);
    assert(error == 0);
    error = toku_logger_close(&logger);
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

    const char *logdir = __SRCFILE__ ".dir";
    init_logdir(logdir);
    int error = chdir(logdir);
    assert(error == 0);

    const int n = 10;
    const char *ftfile =  __SRCFILE__ ".ft_handle";
    create_populate_tree(".", ftfile, n);
    test_provdel(".", ftfile, n);

    return 0;
}

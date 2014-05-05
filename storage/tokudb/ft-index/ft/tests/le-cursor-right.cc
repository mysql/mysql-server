/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."

// test the LE_CURSOR toku_le_cursor_is_key_greater function
// - LE_CURSOR at neg infinity
// - LE_CURSOR at pos infinity
// - LE_CURSOR somewhere else


#include "checkpoint.h"
#include "le-cursor.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static int
get_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen UU(), bytevec val UU(), void *extra, bool lock_only) {
    DBT *CAST_FROM_VOIDP(key_dbt, extra);
    if (!lock_only) {
        toku_dbt_set(keylen, key, key_dbt, NULL);
    }
    return 0;
}

static int
le_cursor_get_next(LE_CURSOR cursor, DBT *val) {
    int r = toku_le_cursor_next(cursor, get_next_callback, val);
    return r;
}

static int 
test_keycompare(DB* UU(desc), const DBT *a, const DBT *b) {
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
    toku_cachetable_create(&ct, 0, ZERO_LSN, logger);
    toku_logger_set_cachetable(logger, ct);
    error = toku_logger_open_rollback(logger, ct, true);
    assert(error == 0);

    TOKUTXN txn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &txn, logger, TXN_SNAPSHOT_NONE, false);
    assert(error == 0);

    FT_HANDLE ft = NULL;
    error = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, test_keycompare);
    assert(error == 0);

    error = toku_txn_commit_txn(txn, true, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    txn = NULL;
    error = toku_txn_begin_txn(NULL, NULL, &txn, logger, TXN_SNAPSHOT_NONE, false);
    assert(error == 0);

    // insert keys 0, 1, 2, .. (n-1)
    for (int i = 0; i < n; i++) {
        int k = toku_htonl(i);
        int v = i;
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
        toku_ft_insert(ft, &key, &val, txn);
    }

    error = toku_txn_commit_txn(txn, true, NULL, NULL);
    assert(error == 0);
    toku_txn_close_txn(txn);

    error = toku_close_ft_handle_nolsn(ft, NULL);
    assert(error == 0);

    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    error = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(error == 0);
    toku_logger_close_rollback(logger);
    assert(error == 0);

    error = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(error == 0);

    toku_logger_shutdown(logger);

    error = toku_logger_close(&logger);
    assert(error == 0);

    toku_cachetable_close(&ct);
}

// test toku_le_cursor_is_key_greater when the LE_CURSOR is positioned at +infinity
static void 
test_pos_infinity(const char *fname, int n) {
    if (verbose) fprintf(stderr, "%s %s %d\n", __FUNCTION__, fname, n);
    int error;

    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    FT_HANDLE ft = NULL;
    error = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_keycompare);
    assert(error == 0);

    // position the cursor at -infinity
    LE_CURSOR cursor = NULL;
    error = toku_le_cursor_create(&cursor, ft, NULL);
    assert(error == 0);

    for (int i = 0; i < 2*n; i++) {
        int k = toku_htonl(i);
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        int right = toku_le_cursor_is_key_greater_or_equal(cursor, &key);
        assert(right == false);
    }
        
    toku_le_cursor_close(cursor);

    error = toku_close_ft_handle_nolsn(ft, 0);
    assert(error == 0);

    toku_cachetable_close(&ct);
}

// test toku_le_cursor_is_key_greater when the LE_CURSOR is positioned at -infinity
static void 
test_neg_infinity(const char *fname, int n) {
    if (verbose) fprintf(stderr, "%s %s %d\n", __FUNCTION__, fname, n);
    int error;

    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    FT_HANDLE ft = NULL;
    error = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_keycompare);
    assert(error == 0);

    // position the LE_CURSOR at +infinity
    LE_CURSOR cursor = NULL;
    error = toku_le_cursor_create(&cursor, ft, NULL);
    assert(error == 0);

    DBT key;
    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    DBT val;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    int i;
    for (i = n-1; ; i--) {
        error = le_cursor_get_next(cursor, &key);
        if (error != 0) 
            break;
        
        assert(key.size == sizeof (int));
        int ii = *(int *)key.data;
        assert((int) toku_htonl(i) == ii);
    }
    assert(i == -1);

    toku_destroy_dbt(&key);
    toku_destroy_dbt(&val);

    for (i = 0; i < 2*n; i++) {
        int k = toku_htonl(i);
        DBT key2;
        toku_fill_dbt(&key2, &k, sizeof k);
        int right = toku_le_cursor_is_key_greater_or_equal(cursor, &key2);
        assert(right == true);
    }

    toku_le_cursor_close(cursor);

    error = toku_close_ft_handle_nolsn(ft, 0);
    assert(error == 0);

    toku_cachetable_close(&ct);
}

// test toku_le_cursor_is_key_greater when the LE_CURSOR is positioned in between -infinity and +infinity
static void 
test_between(const char *fname, int n) {
    if (verbose) fprintf(stderr, "%s %s %d\n", __FUNCTION__, fname, n);
    int error;

    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    FT_HANDLE ft = NULL;
    error = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_keycompare);
    assert(error == 0);

    // position the LE_CURSOR at +infinity
    LE_CURSOR cursor = NULL;
    error = toku_le_cursor_create(&cursor, ft, NULL);
    assert(error == 0);

    DBT key;
    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    DBT val;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    int i;
    for (i = 0; ; i++) {
        // move the LE_CURSOR forward
        error = le_cursor_get_next(cursor, &key);
        if (error != 0) 
            break;
        
        assert(key.size == sizeof (int));
        int ii = *(int *)key.data;
        assert((int) toku_htonl(n-i-1) == ii);

        // test 0 .. i-1 
        for (int j = 0; j <= i; j++) {
            int k = toku_htonl(n-j-1);
            DBT key2;
            toku_fill_dbt(&key2, &k, sizeof k);
            int right = toku_le_cursor_is_key_greater_or_equal(cursor, &key2);
            assert(right == true);
        }

        // test i .. n
        for (int j = i+1; j < n; j++) {
            int k = toku_htonl(n-j-1);
            DBT key2;
            toku_fill_dbt(&key2, &k, sizeof k);
            int right = toku_le_cursor_is_key_greater_or_equal(cursor, &key2);
            assert(right == false);
        }

    }
    assert(i == n);

    toku_destroy_dbt(&key);
    toku_destroy_dbt(&val);

    toku_le_cursor_close(cursor);

    error = toku_close_ft_handle_nolsn(ft, 0);
    assert(error == 0);

    toku_cachetable_close(&ct);
}

static void
init_logdir(const char *logdir) {
    int error;

    toku_os_recursive_delete(logdir);
    error = toku_os_mkdir(logdir, 0777);
    assert(error == 0);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert_zero(r);

    char logdir[TOKU_PATH_MAX+1];
    toku_path_join(logdir, 2, TOKU_TEST_FILENAME, "logdir");
    init_logdir(logdir);
    int error = chdir(logdir);
    assert(error == 0);

    const int n = 10;
    create_populate_tree(".", "ftfile", n);
    test_pos_infinity("ftfile", n);
    test_neg_infinity("ftfile", n);
    test_between("ftfile", n);

    return 0;
}

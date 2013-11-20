/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#include <toku_portability.h>
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include <memory.h>
#include <toku_portability.h>
#include <db.h>

#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// TOKU_TEST_FILENAME is defined in the Makefile

static DB *db;
static DB_TXN* txns[(int)256];
static DB_ENV* dbenv;
static DBC*    cursors[(int)256];

static void
put(bool success, char txn, int _key, int _data) {
    assert(txns[(int)txn]);

    int r;
    DBT key;
    DBT data;
    
    r = db->put(db, txns[(int)txn],
                    dbt_init(&key, &_key, sizeof(int)),
                    dbt_init(&data, &_data, sizeof(int)),
                    0);

    if (success)    CKERR(r);
    else            CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

static void
cget(bool success, bool find, char txn, int _key, int _data, 
     int _key_expect, int _data_expect, uint32_t flags) {
    assert(txns[(int)txn] && cursors[(int)txn]);

    int r;
    DBT key;
    DBT data;
    
    r = cursors[(int)txn]->c_get(cursors[(int)txn],
                                 dbt_init(&key,  &_key,  sizeof(int)),
                                 dbt_init(&data, &_data, sizeof(int)),
                                 flags);
    if (success) {
        if (find) {
            CKERR(r);
            assert(*(int *)key.data  == _key_expect);
            assert(*(int *)data.data == _data_expect);
        }
        else        CKERR2(r,  DB_NOTFOUND);
    }
    else            CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

static void
dbdel (bool success, bool find, char txn, int _key) {
    int r;
    DBT key;

    /* If DB_DELETE_ANY changes to 0, then find is meaningful and 
       has to be fixed in test_dbdel*/
    r = db->del(db, txns[(int)txn], dbt_init(&key,&_key, sizeof(int)), 
                DB_DELETE_ANY);
    if (success) {
        if (find) CKERR(r);
        else      CKERR2( r, DB_NOTFOUND);
    }
    else          CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

static void
init_txn (char name) {
    int r;
    assert(!txns[(int)name]);
    r = dbenv->txn_begin(dbenv, NULL, &txns[(int)name], DB_TXN_NOWAIT);
        CKERR(r);
    assert(txns[(int)name]);
}

static void
init_dbc (char name) {
    int r;

    assert(!cursors[(int)name] && txns[(int)name]);
    r = db->cursor(db, txns[(int)name], &cursors[(int)name], 0);
        CKERR(r);
    assert(cursors[(int)name]);
}

static void
commit_txn (char name) {
    int r;
    assert(txns[(int)name] && !cursors[(int)name]);

    r = txns[(int)name]->commit(txns[(int)name], 0);
        CKERR(r);
    txns[(int)name] = NULL;
}

static void
abort_txn (char name) {
    int r;
    assert(txns[(int)name] && !cursors[(int)name]);

    r = txns[(int)name]->abort(txns[(int)name]);
        CKERR(r);
    txns[(int)name] = NULL;
}

static void
close_dbc (char name) {
    int r;

    assert(cursors[(int)name]);
    r = cursors[(int)name]->c_close(cursors[(int)name]);
        CKERR(r);
    cursors[(int)name] = NULL;
}

static void
early_commit (char name) {
    assert(cursors[(int)name] && txns[(int)name]);
    close_dbc(name);
    commit_txn(name);
}

static void
early_abort (char name) {
    assert(cursors[(int)name] && txns[(int)name]);
    close_dbc(name);
    abort_txn(name);
}

static void
setup_dbs (void) {
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    dbenv   = NULL;
    db      = NULL;
    /* Open/create primary */
    r = db_env_create(&dbenv, 0);
        CKERR(r);
#ifdef TOKUDB
    r = dbenv->set_default_bt_compare(dbenv, int_dbt_cmp);
        CKERR(r);
#endif
    uint32_t env_txn_flags  = DB_INIT_TXN | DB_INIT_LOCK;
    uint32_t env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL;
	r = dbenv->open(dbenv, TOKU_TEST_FILENAME, env_open_flags | env_txn_flags, 0600);
        CKERR(r);
    
    r = db_create(&db, dbenv, 0);
        CKERR(r);
#ifndef TOKUDB
    r = db->set_bt_compare( db, int_dbt_cmp);
    CKERR(r);
#endif

    char a;
    for (a = 'a'; a <= 'z'; a++) init_txn(a);
    init_txn('\0');
    r = db->open(db, txns[(int)'\0'], "foobar.db", NULL, DB_BTREE, DB_CREATE, 0600);
        CKERR(r);
    commit_txn('\0');
    for (a = 'a'; a <= 'z'; a++) init_dbc(a);
}

static void
close_dbs(void) {
    char a;
    for (a = 'a'; a <= 'z'; a++) {
        if (cursors[(int)a]) close_dbc(a);
        if (txns[(int)a])    commit_txn(a);
    }

    int r;
    r = db->close(db, 0);
        CKERR(r);
    db      = NULL;
    r = dbenv->close(dbenv, 0);
        CKERR(r);
    dbenv   = NULL;
}


static __attribute__((__unused__))
void
test_abort (void) {
    /* ********************************************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    early_abort('a');
    cget(true, false, 'b', 1, 1, 0, 0, DB_SET);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, DB_SET);
    cget(true, false, 'b', 1, 1, 0, 0, DB_SET);
    put(false, 'a', 1, 1);
    early_commit('b');
    put(true, 'a', 1, 1);
    cget(true, true, 'a', 1, 1, 1, 1, DB_SET);
    cget(true, false, 'a', 2, 1, 1, 1, DB_SET);
    cget(false, true, 'c', 1, 1, 0, 0, DB_SET);
    early_abort('a');
    cget(true, false, 'c', 1, 1, 0, 0, DB_SET);
    close_dbs();
    /* ********************************************************************** */
}

static void
test_both (uint32_t db_flags) {
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
    cget(true, false, 'a', 2, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
    cget(true, false, 'b', 2, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
#ifdef BLOCKING_ROW_LOCKS_READS_NOT_SHARED
    cget(false, false, 'b', 1, 1, 0, 0, db_flags);
#else
    cget(true, false, 'b', 1, 1, 0, 0, db_flags);
#endif
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 1, 1, 0, 0, db_flags);
#ifdef BLOCKING_ROW_LOCKS_READS_NOT_SHARED
    cget(false, false, 'b', 1, 1, 0, 0, db_flags);
    put(true, 'a', 1, 1);
#else
    cget(true, false, 'b', 1, 1, 0, 0, db_flags);
    put(false, 'a', 1, 1);
#endif
    early_commit('b');
    put(true, 'a', 1, 1);
    cget(true, true, 'a', 1, 1, 1, 1, db_flags);
    cget(true, false, 'a', 2, 1, 0, 0, db_flags);
    cget(false, true, 'c', 1, 1, 0, 0, db_flags);
    early_commit('a');
    cget(true, true, 'c', 1, 1, 1, 1, db_flags);
    close_dbs();
}


static void
test_last (void) {
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 0, 0, 0, 0, DB_LAST);
    put(false, 'b', 2, 1);
    put(true, 'a', 2, 1);
    cget(true, true, 'a', 0, 0, 2, 1, DB_LAST);
    early_commit('a');
    put(true, 'b', 2, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_LAST);
    put(false, 'b', 2, 1);
    put(true, 'b', -1, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_LAST);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    put(true, 'a', 3, 1);
    put(true, 'a', 6, 1);
    cget(true, true, 'a', 0, 0, 6, 1, DB_LAST);
    put(true, 'b', 2, 1);
    put(true, 'b', 4, 1);
    put(false, 'b', 7, 1);
    put(true, 'b', -1, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_LAST);
    put(false, 'b', 1, 0);
    close_dbs();
}

static void
test_first (void) {
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', 0, 0, 0, 0, DB_FIRST);
    put(false, 'b', 2, 1);
    put(true, 'a', 2, 1);
    cget(true, true, 'a', 0, 0, 2, 1, DB_FIRST);
    early_commit('a');
    put(true, 'b', 2, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_FIRST);
    put(true, 'b', 2, 1);
    put(false, 'b', -1, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_FIRST);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    put(true, 'a', 3, 1);
    put(true, 'a', 6, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_FIRST);
    put(true, 'b', 2, 1);
    put(true, 'b', 4, 1);
    put(true, 'b', 7, 1);
    put(false, 'b', -1, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    cget(true, true, 'a', 0, 0, 1, 1, DB_FIRST);
    put(false, 'b', 1, 2);
    close_dbs();
}

static void
test_set_range (uint32_t flag, int i) {
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
    cget(true, false, 'a', i*2, i*1, 0, 0, flag);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
#ifdef BLOCKING_ROW_LOCKS_READS_NOT_SHARED
    cget(false, false, 'b', i*2, i*1, 0, 0, flag);
#else
    cget(true, false, 'b', i*2, i*1, 0, 0, flag);
#endif
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
#ifdef BLOCKING_ROW_LOCKS_READS_NOT_SHARED
    cget(false, false, 'b', i*1, i*1, 0, 0, flag);
#else
    cget(true, false, 'b', i*1, i*1, 0, 0, flag);
#endif
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    cget(true, false, 'a', i*1, i*1, 0, 0, flag);
#ifdef BLOCKING_ROW_LOCKS_READS_NOT_SHARED
    cget(false, false, 'b', i*5, i*5, 0, 0, flag);
    put(true, 'a', i*7, i*6);
    put(true, 'a', i*5, i*5);
#else
    cget(true, false, 'b', i*5, i*5, 0, 0, flag);
    put(false, 'a', i*7, i*6);
    put(false, 'a', i*5, i*5);
#endif
    put(true,  'a', i*4, i*4);
    put(true,  'b', -i*1, i*4);
    put(false,  'b', i*2, i*4);
#ifdef BLOCKING_ROW_LOCKS_READS_NOT_SHARED
    put(true, 'a', i*5, i*4);
#else
    put(false, 'a', i*5, i*4);
#endif
    early_commit('b');
    put(true, 'a', i*7, i*6);
    put(true, 'a', i*5, i*5);
    put(true,  'a', i*4, i*4);
    put(true, 'a', i*5, i*4);
    cget(true, true, 'a', i*1, i*1, i*4, i*4, flag);
    cget(true, true, 'a', i*2, i*1, i*4, i*4, flag);
    cget(false, true, 'c', i*6, i*6, i*7, i*6, flag);
    early_commit('a');
    cget(true, true, 'c', i*6, i*6, i*7, i*6, flag);
    close_dbs();
}

static void
test_next (uint32_t next_type) {
    /* ********************************************************************** */
    setup_dbs();
    put(true,  'a', 2, 1);
    put(true,  'a', 5, 1);
    cget(true, true, 'a', 0, 0, 2, 1, next_type);
    put(false, 'b', 2, 1);
    put(true,  'b', 4, 1);
    put(false, 'b', -1, 1);
    cget(false, true, 'a', 0, 0, 4, 1, next_type);
    early_commit('b');
    cget(true,  true, 'a', 2, 1, 2, 1, DB_SET);
    cget(true,  true, 'a', 0, 0, 4, 1, next_type);
    cget(true,  true, 'a', 0, 0, 5, 1, next_type);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    put(true, 'a', 3, 1);
    put(true, 'a', 6, 1);
    cget(true, true, 'a', 0, 0, 1, 1, next_type);
    cget(true, true, 'a', 0, 0, 3, 1, next_type);
    put(false, 'b', 2, 1);
    put(true,  'b', 4, 1);
    put(true,  'b', 7, 1);
    put(false, 'b', -1, 1);
    close_dbs();
}

static void
test_prev (uint32_t next_type) {
    /* ********************************************************************** */
    setup_dbs();
    put(true,  'a', -2, -1);
    put(true,  'a', -5, -1);
    cget(true, true, 'a', 0, 0, -2, -1, next_type);
    put(false, 'b', -2, -1);
    put(true,  'b', -4, -1);
    put(false, 'b', 1, -1);
    cget(false, true, 'a', 0, 0, -4, -1, next_type);
    early_commit('b');
    cget(true,  true, 'a', -2, -1, -2, -1, DB_SET);
    cget(true,  true, 'a', 0, 0, -4, -1, next_type);
    cget(true,  true, 'a', 0, 0, -5, -1, next_type);
    close_dbs();
    /* ****************************************** */
    setup_dbs();
    put(true, 'a', -1, -1);
    put(true, 'a', -3, -1);
    put(true, 'a', -6, -1);
    cget(true, true, 'a', 0, 0, -1, -1, next_type);
    cget(true, true, 'a', 0, 0, -3, -1, next_type);
    put(false, 'b', -2, -1);
    put(true,  'b', -4, -1);
    put(true,  'b', -7, -1);
    put(false, 'b', 1, -1);
    close_dbs();
}

static void
test_dbdel (void) {
    /* If DB_DELETE_ANY changes to 0, then find is meaningful and 
       has to be fixed in test_dbdel*/
    /* ********************************************************************** */
    setup_dbs();
    put(true, 'c', 1, 1);
    early_commit('c');
    dbdel(true, true, 'a', 1);
    cget(false, true, 'b', 1, 1, 1, 1, DB_SET);
    cget(false, true, 'b', 1, 4, 1, 4, DB_SET);
    cget(false, true, 'b', 1, 0, 1, 4, DB_SET);
    cget(true, false, 'b', 0, 0, 0, 0, DB_SET);
    cget(true, false, 'b', 2, 10, 2, 10, DB_SET);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    dbdel(true, true, 'a', 1);
    cget(false, true, 'b', 1, 1, 1, 1, DB_SET);
    cget(false, true, 'b', 1, 4, 1, 4, DB_SET);
    cget(false, true, 'b', 1, 0, 1, 4, DB_SET);
    cget(true, false, 'b', 0, 0, 0, 0, DB_SET);
    cget(true, false, 'b', 2, 10, 2, 10, DB_SET);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    put(true, 'c', 1, 1);
    early_commit('c');
    cget(true,  true, 'b', 1, 1, 1, 1, DB_SET);
    dbdel(false, true, 'a', 1);
    dbdel(true, true, 'a', 2);
    dbdel(true, true, 'a', 0);
    close_dbs();
}

static void
test_current (void) {
    /* ********************************************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    early_commit('a');
    cget(true,  true, 'b', 1, 1, 1, 1, DB_SET);
    cget(true,  true, 'b', 1, 1, 1, 1, DB_CURRENT);
    close_dbs();
}

struct dbt_pair {
    DBT key;
    DBT val;
};

struct int_pair {
    int key;
    int val;
};

int got_r_h;

static __attribute__((__unused__))
void
ignore (void *ignore __attribute__((__unused__))) {
}
#define TOKU_IGNORE(x) ignore((void*)x)

static void
test (void) {
    /* ********************************************************************** */
    setup_dbs();
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    early_abort('a');
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    early_commit('a');
    close_dbs();
    /* ********************************************************************** */
    setup_dbs();
    put(true, 'a', 1, 1);
    close_dbs();
    /* ********************************************************************** */
    test_both( DB_SET);
    /* ********************************************************************** */
    test_first();
    /* ********************************************************************** */
    test_last();
    /* ********************************************************************** */
    test_set_range( DB_SET_RANGE, 1);
#ifdef DB_SET_RANGE_REVERSE
    test_set_range( DB_SET_RANGE_REVERSE, -1);
#endif
    /* ********************************************************************** */
    test_next(DB_NEXT);
    /* ********************************************************************** */
    test_prev(DB_PREV);
    /* ********************************************************************** */
    test_dbdel();
    /* ********************************************************************** */
    test_current();
    /* ********************************************************************** */
}


int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    if (!IS_TDB) {
	if (verbose) {
	    printf("Warning: " __FILE__" does not work in BDB.\n");
	}
    } else {
	test();
	/*
	  test_abort(0);
	  test_abort(DB_DUP | DB_DUPSORT);
	*/
    }
    return 0;
}

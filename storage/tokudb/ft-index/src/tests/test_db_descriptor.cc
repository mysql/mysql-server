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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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
#define FNAME       "foo.tokudb"
const char *name = NULL;

#define NUM         3
#define MAX_LENGTH  (1<<16)

int order[NUM+1];
uint32_t length[NUM];
uint8_t data[NUM][MAX_LENGTH];
DBT descriptors[NUM];
DB_ENV *env;

enum {NUM_DBS=2};
DB *dbs[NUM_DBS];
DB_TXN *txn = NULL;
DB_TXN *null_txn;
int last_open_descriptor = -1;

int abort_type;
int get_table_lock;
uint64_t num_called = 0;


static void
verify_db_matches(void) {
    DB *db;
    int which;
    for (which = 0; which < NUM_DBS; which++) {
        db = dbs[which];
        if (db) {
            const DBT * dbt = &db->descriptor->dbt;

            if (last_open_descriptor<0) {
                assert(dbt->size == 0 && dbt->data == NULL);
            }
            else {
                assert(last_open_descriptor < NUM);
                assert(dbt->size == descriptors[last_open_descriptor].size);
                assert(!memcmp(dbt->data, descriptors[last_open_descriptor].data, dbt->size));
                assert(dbt->data != descriptors[last_open_descriptor].data);
            }
        }
    }
    
}

static int
verify_int_cmp (DB *dbp, const DBT *a, const DBT *b) {
    num_called++;
    verify_db_matches();
    int r = int_dbt_cmp(dbp, a, b);
    return r;
}

static void
open_db(int descriptor, int which) {
    /* create the dup database file */
    assert(dbs[which]==NULL);
    DB *db;
    int r = db_create(&db, env, 0);
    CKERR(r);
    dbs[which] = db;

    assert(abort_type >=0 && abort_type <= 2);
    if (abort_type==2 && !txn) {
        r = env->txn_begin(env, null_txn, &txn, 0);
            CKERR(r);
        last_open_descriptor = -1; //DB was destroyed at end of last close, did not hang around.
    }
    r = db->open(db, txn, FNAME, name, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    if (descriptor >= 0) {
        assert(descriptor < NUM);
        if (txn) {
            { int chk_r = db->change_descriptor(db, txn, &descriptors[descriptor], 0); CKERR(chk_r); }
        }
        else {
            IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                    { int chk_r = db->change_descriptor(db, txn_desc, &descriptors[descriptor], 0); CKERR(chk_r); }
            });
        }
        last_open_descriptor = descriptor;
    }
    verify_db_matches();
    if (abort_type!=2 && !txn) {
        r = env->txn_begin(env, null_txn, &txn, 0);
            CKERR(r);
    }
    assert(txn);
    if (get_table_lock) {
        r = db->pre_acquire_table_lock(db, txn);
        CKERR(r);
    }
}

static void
delete_db(void) {
    int which;
    for (which = 0; which < NUM_DBS; which++) {
        assert(dbs[which] == NULL);
    }
    int r = env->dbremove(env, NULL, FNAME, name, 0);
    if (abort_type==2) {
        CKERR2(r, ENOENT); //Abort deleted it
    }
    else CKERR(r);
    last_open_descriptor = -1;
}

static void
close_db(int which) {
    assert(dbs[which]!=NULL);
    DB *db = dbs[which];
    dbs[which] = NULL;

    int r;
    if (which==1) {
        r = db->close(db, 0);
        CKERR(r);
        return;
    }
    if (abort_type>0) {
        if (abort_type==2 && dbs[1]) {
            close_db(1);
        }
        r = db->close(db, 0);
        CKERR(r);
        r = txn->abort(txn);
        CKERR(r);
    }
    else {
        r = txn->commit(txn, 0);
        CKERR(r);
        r = db->close(db, 0);
        CKERR(r);
    }
    txn = NULL;
}

static void
setup_data(void) {
    int r = db_env_create(&env, 0);                                           CKERR(r);
    r = env->set_default_bt_compare(env, verify_int_cmp);                     CKERR(r);
    const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK |DB_THREAD |DB_PRIVATE;
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);
    int i;
    for (i=0; i < NUM; i++) {
        length[i] = i * MAX_LENGTH / (NUM-1);
        uint32_t j;
        for (j = 0; j < length[i]; j++) {
            data[i][j] = (uint8_t)(random() & 0xFF);
        }
        memset(&descriptors[i], 0, sizeof(descriptors[i]));
        descriptors[i].size = length[i];
        descriptors[i].data = &data[i][0];
    }
    last_open_descriptor = -1;
    txn = NULL;
}

static void
permute_order(void) {
    int i;
    for (i=0; i < NUM; i++) {
        order[i] = i;
    }
    for (i=0; i < NUM; i++) {
        int which = (random() % (NUM-i)) + i;
        int temp = order[i];
        order[i] = order[which];
        order[which] = temp;
    }
}

static void
test_insert (int n, int which) {
    if (which == -1) {
        for (which = 0; which < NUM_DBS; which++) {
            if (dbs[which]) {
                test_insert(n, which);
            }
        }
        return;
    }
    assert(dbs[which]!=NULL);
    DB *db = dbs[which];
    int i;
    static int last = 0;
    for (i=0; i<n; i++) {
        int k = last++;
        DBT key, val;
        uint64_t called = num_called;
        int r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &i, sizeof i), 0);
        if (i>0) assert(num_called > called);
        CKERR(r);
    }
}

   
static void
runtest(void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    setup_data();
    permute_order();

    int i;
    /* Subsumed by rest of test.
    for (i=0; i < NUM; i++) {
        open_db(-1, 0);
        test_insert(i, 0);
        close_db(0);
        open_db(-1, 0);
        test_insert(i, 0);
        close_db(0);
        delete_db();
    }

    for (i=0; i < NUM; i++) {
        open_db(order[i], 0);
        test_insert(i, 0);
        close_db(0);
        open_db(-1, 0);
        test_insert(i, 0);
        close_db(0);
        open_db(order[i], 0);
        test_insert(i, 0);
        close_db(0);
        delete_db();
    }
    */

    //Upgrade descriptors along the way.  Need version to increase, so do not use 'order[i]'
    for (i=0; i < NUM; i++) {
        open_db(i, 0);
        test_insert(i, 0);
        close_db(0);
        open_db(-1, 0);
        test_insert(i, 0);
        close_db(0);
        open_db(i, 0);
        test_insert(i, 0);
        close_db(0);
    }
    delete_db();

    //Upgrade descriptors along the way. With two handles
    open_db(-1, 1);
    for (i=0; i < NUM; i++) {
        open_db(i, 0);
        test_insert(i, -1);
        close_db(0);
        open_db(-1, 0);
        test_insert(i, -1);
        close_db(0);
        open_db(i, 0);
        test_insert(i, -1);
        close_db(0);
    }
    if (dbs[1]) { 
        close_db(1);
    }
    delete_db();
    
    env->close(env, 0);
}


int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    for (abort_type = 0; abort_type < 3; abort_type++) {
        for (get_table_lock = 0; get_table_lock < 2; get_table_lock++) {
            name = NULL;
            runtest();

            name = "bar";
            runtest();
            
        }
    }

    return 0;
}                                        


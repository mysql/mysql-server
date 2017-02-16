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
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


//
// This test verifies that running evictions on a writer thread
// are ok. We create a dictionary bigger than the cachetable (around 4x greater).
// Then, we spawn a bunch of pthreads that do the following:
//  - scan dictionary forward with bulk fetch
//  - scan dictionary forward slowly
//  - scan dictionary backward with bulk fetch
//  - scan dictionary backward slowly
//  - update existing values in the dictionary with db->put(DB_YESOVERWRITE)
// With the small cachetable, this should produce quite a bit of churn in reading in and evicting nodes.
// If the test runs to completion without crashing, we consider it a success.
//

bool run_test;
int time_of_test;
int num_elements;

struct arg {
    int n;
    DB *db;
    DB_ENV* env;
    bool fast;
    bool fwd;
};

static int
go_fast(DBT const *a, DBT  const *b, void *c) {
    assert(a);
    assert(b);
    assert(c==NULL);
    return TOKUDB_CURSOR_CONTINUE;
}
static int
go_slow(DBT const *a, DBT  const *b, void *c) {
    assert(a);
    assert(b);
    assert(c==NULL);
    return 0;
}

static void *scan_db(void *arg) {
    struct arg *myarg = (struct arg *) arg;
    DB_ENV* env = myarg->env;
    DB* db = myarg->db;
    DB_TXN* txn = NULL;
    while(run_test) {
        int r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT); CKERR(r);
        DBC* cursor = NULL;
        { int chk_r = db->cursor(db, txn, &cursor, 0); CKERR(chk_r); }
        while (r != DB_NOTFOUND) {
            if (myarg->fwd) {
                r = cursor->c_getf_next(cursor, 0, myarg->fast ? go_fast : go_slow, NULL);
            }
            else {
                r = cursor->c_getf_prev(cursor, 0, myarg->fast ? go_fast : go_slow, NULL);
            }
            assert(r==0 || r==DB_NOTFOUND);
        }
        
        { int chk_r = cursor->c_close(cursor); CKERR(chk_r); }
        { int chk_r = txn->commit(txn,0); CKERR(chk_r); }
    }
    return arg;
}

static void *ptquery_db(void *arg) {
    struct arg *myarg = (struct arg *) arg;
    DB_ENV* env = myarg->env;
    DB* db = myarg->db;
    DB_TXN* txn = NULL;
    int n = myarg->n;
    while(run_test) {
        int r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT); CKERR(r);
        int rand_key = random() % n;        
        DBT key;
        DBT val;
        memset(&val, 0, sizeof(val));
        dbt_init(&key, &rand_key, sizeof(rand_key));
        r = db->get(db, txn, &key, &val, 0);
        assert(r != DB_NOTFOUND);
        { int chk_r = txn->commit(txn,0); CKERR(chk_r); }
    }
    return arg;
}

static void *update_db(void *arg) {
    struct arg *myarg = (struct arg *) arg;
    DB_ENV* env = myarg->env;
    DB* db = myarg->db;
    int n = myarg->n;

    DB_TXN* txn = NULL;
    while (run_test) {
        int r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT); CKERR(r);
        for (uint32_t i = 0; i < 1000; i++) {
            int rand_key = random() % n;
            int rand_val = random();
            DBT key, val;
            r = db->put(
                db, 
                txn, 
                dbt_init(&key, &rand_key, sizeof(rand_key)), 
                dbt_init(&val, &rand_val, sizeof(rand_val)), 
                0
                );
            CKERR(r);
        }
        { int chk_r = txn->commit(txn,0); CKERR(chk_r); }
    }
    return arg;
}

static void *test_time(void *arg) {
    assert(arg == NULL);
    usleep(time_of_test*1000*1000);
    if (verbose) printf("should now end test\n");
    run_test = false;
    return arg;
}


static void
test_evictions (void) {
    int n = num_elements;
    if (verbose) printf("test_3645:%d \n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.bulk_fetch.ft_handle";
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r=env->set_default_bt_compare(env, int_dbt_cmp); CKERR(r);
    // set the cache size to 10MB
    r = env->set_cachesize(env, 0, 100000, 1); CKERR(r);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, 10); 
    CKERR(r);



    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->set_readpagesize(db, 1024);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int keys[n];
    for (int i=0; i<n; i++) {
        keys[i] = i;
    }
    
    if (verbose) printf("starting insertion of elements to setup test\n");
    for (int i=0; i<n; i++) {
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    } 

    //
    // the threads that we want:
    //   - one thread constantly updating random values
    //   - one thread doing table scan with bulk fetch
    //   - one thread doing table scan without bulk fetch
    //   - one thread doing random point queries
    //
    run_test = true;
    if (verbose) printf("starting creation of pthreads\n");
    toku_pthread_t mytids[7];
    struct arg myargs[7];
    for (uint32_t i = 0; i < sizeof(myargs)/sizeof(myargs[0]); i++) {
        myargs[i].n = n;
        myargs[i].db = db;
        myargs[i].env = env;
        myargs[i].fast = true;
        myargs[i].fwd = true;
    }

    // make the forward fast scanner
    myargs[0].fast = true;
    myargs[0].fwd = true;
    { int chk_r = toku_pthread_create(&mytids[0], NULL, scan_db, &myargs[0]); CKERR(chk_r); }

    // make the forward slow scanner
    myargs[1].fast = false;
    myargs[1].fwd = true;
    { int chk_r = toku_pthread_create(&mytids[1], NULL, scan_db, &myargs[1]); CKERR(chk_r); }

    // make the backward fast scanner
    myargs[2].fast = true;
    myargs[2].fwd = false;
    { int chk_r = toku_pthread_create(&mytids[2], NULL, scan_db, &myargs[2]); CKERR(chk_r); }

    // make the backward slow scanner
    myargs[3].fast = false;
    myargs[3].fwd = false;
    { int chk_r = toku_pthread_create(&mytids[3], NULL, scan_db, &myargs[3]); CKERR(chk_r); }

    // make the guy that updates the db
    { int chk_r = toku_pthread_create(&mytids[4], NULL, update_db, &myargs[4]); CKERR(chk_r); }

    // make the guy that does point queries
    { int chk_r = toku_pthread_create(&mytids[5], NULL, ptquery_db, &myargs[5]); CKERR(chk_r); }

    // make the guy that sleeps
    { int chk_r = toku_pthread_create(&mytids[6], NULL, test_time, NULL); CKERR(chk_r); }
    
    for (uint32_t i = 0; i < sizeof(myargs)/sizeof(myargs[0]); i++) {
        void *ret;
        r = toku_pthread_join(mytids[i], &ret); assert_zero(r);
    }
    if (verbose) printf("ending test, pthreads have joined\n");


    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static inline void parse_3645_args (int argc, char *const argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
        int resultcode=0;
        if (strcmp(argv[1], "-v")==0) {
            verbose++;
        } 
        else if (strcmp(argv[1], "-q")==0) {
            verbose=0;
        } 
        else if (strcmp(argv[1], "-h")==0) {
        do_usage:
            fprintf(stderr, "Usage:\n%s [-v|-h-q|--num_elements number | --num_seconds number]\n", argv0);
            exit(resultcode);
        } 
        else if (strcmp(argv[1], "--num_elements") == 0) {
            argc--;
            argv++;
            num_elements = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_seconds") == 0) {
            argc--;
            argv++;
            time_of_test = atoi(argv[1]);
        }
        else {
            resultcode=1;
            goto do_usage;
        }
        argc--;
        argv++;
    }
}


int
test_main(int argc, char *const argv[]) {
    // default values
    num_elements = 100000;
    time_of_test = 60;
    parse_3645_args(argc, argv);
    test_evictions();
    return 0;
}

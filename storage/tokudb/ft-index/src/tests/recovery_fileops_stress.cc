/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include <stdlib.h>

static const int NUM_DICTIONARIES = 100;
//static const int NUM_DICTIONARIES = 3;
static const char *table = "tbl";
static const int ROWS_PER_TABLE = 10;

DB_ENV *env;
DB** db_array;
DB* states;
static const int percent_do_op = 20;
static const int percent_do_abort = 25;
static const int start_crashing_iter = 10;
// iterations_per_crash_in_recovery should be an odd number;
static const int iterations_per_crash_in_recovery = 7;
const char *state_db_name="states.db";

#define CREATED 0
#define OPEN    1
#define CLOSED  2
#define DELETED 3

#define COMMIT_TXN 0
#define ABORT_TXN 1

static int commit_or_abort(void) {
    int i = random() % 100;
    int rval = ( i < percent_do_abort ) ? ABORT_TXN : COMMIT_TXN;
    if ( verbose ) {
        if ( rval == ABORT_TXN ) printf("%s :     abort txn\n", __FILE__);
    }
    return rval;
}

static void put_state(int db_num, int state) {
    int r;
    DB_TXN* txn;
    DBT key, val;
    int key_data = db_num;
    int val_data = state;
    r = env->txn_begin(env, NULL, &txn, 0);                                                               CKERR(r);
    r = states->put(states, txn, 
                    dbt_init(&key, &key_data, sizeof(key_data)),
                    dbt_init(&val, &val_data, sizeof(val_data)),
                    0);                                                                                   CKERR(r);
    r = txn->commit(txn, 0);                                                                              CKERR(r);
}

static int get_state(int db_num) {
    int r;
    DBT key, val;

    memset(&val, 0, sizeof(val));
    r = states->get(states, 0,
                    dbt_init(&key, &db_num, sizeof(db_num)),
                    &val, 
                    0);
    CKERR(r);
    int state = *(int*)val.data;
    return state;
}

static int crash_timer;
static void crash_it(void);
static void crash_it_callback_f(void*);
static void set_crash_timer(void) {
    crash_timer = random() % (3 * NUM_DICTIONARIES);
}

static void update_crash_timer(void) {
    if ( --crash_timer == 0 ) {
        // close the states table before we crash
        int r = states->close(states, 0); 
        CKERR(r);
        if ( verbose ) {
            printf("%s : crash\n", __FILE__);
            fflush(stdout);
        }
        crash_it();
    }
}

static void    env_startup(int recovery_flags);
static int64_t generate_val(int64_t key);
static void    insert_n(DB *db, DB_TXN *txn, int firstkey, int n);
static int     verify_identical_dbts(const DBT *dbt1, const DBT *dbt2);
static void    verify_sequential_rows(DB* compare_db, int64_t firstkey, int64_t numkeys);

static DB* do_create(char* name, int* next_state) {
    DB* db = NULL;
    if ( verbose ) printf("%s :   do_create(%s)\n", __FILE__, name);
    int r;
    DB_TXN* txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 
    CKERR(r);
    r = db_create(&db, env, 0);                                                             
    CKERR(r);
    r = db->open(db, txn, name, NULL, DB_BTREE, DB_CREATE, 0666);                          
    CKERR(r);
    insert_n(db, txn, 0, ROWS_PER_TABLE);
    if ( commit_or_abort() == COMMIT_TXN ) {
        r = txn->commit(txn, 0);                                                   
        CKERR(r);
        *next_state = CREATED;
    }
    else {
        r = db->close(db, 0);
        db = NULL;
        CKERR(r);
        r = txn->abort(txn);                                                   
        CKERR(r);
        db = NULL;
    }
    return db;
}

static DB* do_open(char* name, int* next_state) {
    DB* db = NULL;
    DB_TXN* txn;
    if ( verbose ) printf("%s :   do_open(%s)\n", __FILE__, name);
    int r;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 
    CKERR(r);
    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->open(db, txn, name, NULL, DB_UNKNOWN, 0, 0666);
    CKERR(r);
    if ( commit_or_abort() == COMMIT_TXN ) {
        r = txn->commit(txn, 0);                                                   
        CKERR(r);
        *next_state = OPEN;
    }
    else {
        r = db->close(db, 0);
        db = NULL;
        CKERR(r);
        r = txn->abort(txn);                                                   
        CKERR(r);
        db = NULL;
    }
    return db;
}

static void do_close(DB* db, char* name, int* next_state) {
    if ( verbose ) printf("%s :   do_close(%s)\n", __FILE__, name);
    if (!db) printf("db == NULL\n");

    int r = db->close(db, 0); 
    CKERR(r);
    db = NULL;
    *next_state = CLOSED;
}

static void do_delete(char* name, int* next_state) {
    DB_TXN* txn;
    if ( verbose ) printf("%s :   do_delete(%s)\n", __FILE__, name);
    int r;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 
    CKERR(r);
    r = env->dbremove(env, txn, name, NULL, 0);
    CKERR(r);

    if ( commit_or_abort() == COMMIT_TXN ) {
        r = txn->commit(txn, 0);                                                   
        CKERR(r);
        *next_state = DELETED;
    }
    else {
        r = txn->abort(txn);                                                   
        CKERR(r);
    }
}

static int do_random_fileop(int i, int state) {
    DB* db = db_array[i];
    int rval = random() % 100;
//    if ( verbose ) printf("%s : %s : DB '%d', state '%d, rval '%d'\n", __FILE__, __FUNCTION__, i, state, rval);

    int next_state = state;

    char fname[100];
    sprintf(fname, "%s%d.db", table, i);
    
    if ( rval < percent_do_op ) {
        switch ( state ) {
        case CREATED:
            do_close(db, fname, &next_state);
            db_array[i] = db = 0;
            if ( rval < (percent_do_op / 2) ) {
                do_delete(fname, &next_state);
            }
            break;
        case OPEN:
            do_close(db, fname, &next_state);
            db_array[i] = db = 0;
            if ( rval < (percent_do_op / 2) ) {
                do_delete(fname, &next_state);
            }
            break;
        case CLOSED:
            if ( rval < (percent_do_op / 2) ) {
                db = do_open(fname, &next_state);
                db_array[i] = db;
            }
            else {
                do_delete(fname, &next_state);
            }
            break;
        case DELETED:
            db = do_create(fname, &next_state);
            db_array[i] = db;
            break;
        }
    }
    return next_state;
}

static void do_random_fileops(void) 
{
    int i, state, next_state;
    DB_TXN *txn;
    for (i=0;i<NUM_DICTIONARIES;i++) {
        { int chk_r = env->txn_begin(env, NULL, &txn, 0); CKERR(chk_r); }
        state = get_state(i);
        next_state = do_random_fileop(i, state);
        put_state(i, next_state);
        { int chk_r = txn->commit(txn, 0); CKERR(chk_r); }
        update_crash_timer();
    }
}


static void run_test(int iter){
    uint32_t recovery_flags = DB_INIT_LOG | DB_INIT_TXN;
    int r, i;

    XMALLOC_N(NUM_DICTIONARIES, db_array);
    srand(iter);

    if (iter == 0) {
        // create working directory
        toku_os_recursive_delete(TOKU_TEST_FILENAME);
        r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                                               CKERR(r);
    }
    else
        recovery_flags += DB_RECOVER;

    // crash somewhat frequently during recovery
    //   first, wait until after first crash
    if ( iter > start_crashing_iter + 1 ) {
        // every N cycles, crash in recovery
        if ( (iter % iterations_per_crash_in_recovery) == 0 ) {
            // crash at different places in recovery
            if ( iter & 1 ) 
                db_env_set_recover_callback(crash_it_callback_f, NULL);
            else
                db_env_set_recover_callback2(crash_it_callback_f, NULL);
        }
    }

    env_startup(recovery_flags);
    if ( verbose ) printf("%s : environment init\n", __FILE__);

    if (iter == 0) {
        // create a dictionary to store test state
        r = db_create(&states, env, 0);                                                                   CKERR(r);
        r = states->open(states, NULL, state_db_name, NULL, DB_BTREE, DB_CREATE, 0666);                   CKERR(r);
        DB_TXN *states_txn;
        r = env->txn_begin(env, NULL, &states_txn, 0);                                                    CKERR(r);
        for (i=0;i<NUM_DICTIONARIES;i++) {
            put_state(i, DELETED);
        }
        r = states_txn->commit(states_txn, 0);                                                            CKERR(r);
        r = states->close(states, 0);                                                                     CKERR(r);
        if ( verbose ) printf("%s : states.db initialized\n", __FILE__);
    }

    // open the 'states' table
    r = db_create(&states, env, 0);                                                                       CKERR(r);
    r = states->open(states, NULL, state_db_name, NULL, DB_UNKNOWN, 0, 0666);                             CKERR(r);

    if ( verbose ) printf("%s : ===  ITERATION %6d ===\n", __FILE__, iter);

    // verify previous results
    if ( verbose ) printf("%s : verify previous results\n", __FILE__);
    int state = DELETED;
    DB* db;
    char fname[100];
    if ( iter > 0 ) {
        for (i=0;i<NUM_DICTIONARIES;i++) {
            sprintf(fname, "%s%d.db", table, i);
            state = get_state(i);
            switch (state) {
            case CREATED:
            case OPEN:
                // open the table
                r = db_create(&db, env, 0);                                                               CKERR(r);
                r = db->open(db, NULL, fname, NULL, DB_UNKNOWN, 0, 0666);                                 CKERR(r);
                db_array[i] = db;
                verify_sequential_rows(db, 0, ROWS_PER_TABLE);
                // leave table open
                if (verbose) printf("%s :   verified open/created db[%d]\n", __FILE__, i);
                break;
            case CLOSED:
                // open the table
                r = db_create(&db, env, 0);                                                               CKERR(r);
                r = db->open(db, NULL, fname, NULL, DB_UNKNOWN, 0, 0666);                                 CKERR(r);
                verify_sequential_rows(db, 0, ROWS_PER_TABLE);
                // close table
                r = db->close(db, 0);                                                                     CKERR(r);
                db_array[i] = db = NULL;
                if (verbose) printf("%s :   verified closed db[%d]\n", __FILE__, i);
                break;
            case DELETED:
                r = db_create(&db, env, 0);                                                               CKERR(r);
                r = db->open(db, NULL, fname, NULL, DB_UNKNOWN, 0, 0666);
                if ( r == 0 ) assert(1);
                db_array[i] = db = NULL;
                if (verbose) printf("%s :   verified db[%d] removed\n", __FILE__, i);
                break;
            default:
                printf("ERROR : Unknown state '%d'\n", state);
                return;
            }
        }
    }
    if ( verbose ) printf("%s : previous results verified\n", __FILE__);

    // for each of the dictionaries, perform a fileop some percentage of time (set in do_random_fileop).

    // before checkpoint #1
    if ( verbose ) printf("%s : before checkpoint #1\n", __FILE__);
    crash_timer = NUM_DICTIONARIES + 1;  // won't go off
    do_random_fileops();

    // during checkpoint #1
    if ( verbose ) printf("%s : during checkpoint #1\n", __FILE__);
    crash_timer = NUM_DICTIONARIES + 1;  // won't go off

    if ( iter & 1 ) 
        db_env_set_checkpoint_callback((void (*)(void*))do_random_fileops, NULL);
    else
        db_env_set_checkpoint_callback2((void (*)(void*))do_random_fileops, NULL);
    // checkpoint
    r = env->txn_checkpoint(env, 0, 0, 0);                                                                CKERR(r);
    db_env_set_checkpoint_callback(NULL, NULL);
    db_env_set_checkpoint_callback2(NULL, NULL);

    // randomly fail sometime during the next 3 phases
    //  1) before the next checkpoint
    //  2) during the next checkpoint
    //  3) after the next (final) checkpoint

    if ( iter >= start_crashing_iter ) {
        set_crash_timer();
    } 
    else {
        crash_timer = ( 3 * NUM_DICTIONARIES ) + 1;  // won't go off
    }

    // before checkpoint #2
    if ( verbose ) printf("%s : before checkpoint #2\n", __FILE__);
    do_random_fileops();

    // during checkpoint
    if ( verbose ) printf("%s : during checkpoint #2\n", __FILE__);

    if ( iter & 1 ) 
        db_env_set_checkpoint_callback((void (*)(void*))do_random_fileops, NULL);
    else
        db_env_set_checkpoint_callback2((void (*)(void*))do_random_fileops, NULL);
    // checkpoint
    r = env->txn_checkpoint(env, 0, 0, 0);                                                                CKERR(r);
    db_env_set_checkpoint_callback(NULL, NULL);
    db_env_set_checkpoint_callback2(NULL, NULL);

    // after checkpoint
    if ( verbose ) printf("%s : after checkpoint #2\n", __FILE__);
    do_random_fileops();
    
    r = env->txn_checkpoint(env, 0, 0, 0);                                                                CKERR(r);

    for (i=0;i<NUM_DICTIONARIES;i++) {
        db = db_array[i];
        state = get_state(i);
        if ( state == CREATED || state == OPEN ) {
            r = db->close(db, 0);                                                                         CKERR(r);
            db = NULL;
        }
    }

    r = states->close(states, 0);                                                                         CKERR(r);
    r = env->close(env, 0);                                                                               CKERR(r);
    if ( verbose ) printf("%s : done\n", __FILE__);

    toku_free(db_array);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

static int iter_arg = 0;

int test_main(int argc, char *const*argv) {
    do_args(argc, argv);
    run_test(iter_arg);
    return 0;
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] [-i] \n", cmd);
	    exit(resultcode);
	} else if (strcmp(argv[0], "-i")==0) {
            argc--; argv++;
            iter_arg = atoi(argv[0]);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

static void env_startup(int recovery_flags) {
    int r;
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | recovery_flags;
    r = db_env_create(&env, 0);                                     CKERR(r);
    db_env_enable_engine_status(0);  // disable engine status on crash because test is expected to fail
    r=env->set_redzone(env, 0);                                     CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);  CKERR(r);
    //Disable auto-checkpointing.
    r = env->checkpointing_set_period(env, 0);                      CKERR(r);
}

static int64_t generate_val(int64_t key) {
    return key + 314;
}

static void insert_n(DB *db, DB_TXN *txn, int firstkey, int n) {
    int64_t k, v;
    int r, i;
    DBT key, val;

    if (!db) return;

    for (i = 0; i<n; i++) {
	k = firstkey + i;
	v = generate_val(k);
	dbt_init(&key, &k, sizeof(k));
	dbt_init(&val, &v, sizeof(v));
        r = db->put(db, txn, &key, &val, 0);
        CKERR(r);
    }
}

static int verify_identical_dbts(const DBT *dbt1, const DBT *dbt2) {
    int r = 0;
    if (dbt1->size != dbt2->size) r = 1;
    else if (memcmp(dbt1->data, dbt2->data, dbt1->size)!=0) r = 1;
    return r;
}

static void verify_sequential_rows(DB* compare_db, int64_t firstkey, int64_t numkeys) {
    //This does not lock the dbs/grab table locks.
    //This means that you CANNOT CALL THIS while another thread is modifying the db.
    //You CAN call it while a txn is open however.
    int rval = 0;
    DB_TXN *compare_txn;
    int r, r1;

    assert(numkeys >= 1);
    r = env->txn_begin(env, NULL, &compare_txn, DB_READ_UNCOMMITTED);
        CKERR(r);
    DBC *c1;

    r = compare_db->cursor(compare_db, compare_txn, &c1, 0);
        CKERR(r);


    DBT key1, val1;
    DBT key2, val2;

    int64_t k, v;

    dbt_init_realloc(&key1);
    dbt_init_realloc(&val1);

    dbt_init(&key2, &k, sizeof(k));
    dbt_init(&val2, &v, sizeof(v));

//    k = firstkey;
//    v = generate_val(k);
//    r1 = c1->c_get(c1, &key2, &val2, DB_SET);
//    CKERR(r1);

    int64_t i;
    for (i = 0; i<numkeys; i++) {
	k = i + firstkey;
	v = generate_val(k);
        r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
//        printf("k = %" PRIu64 ", v = %" PRIu64 ", key = %" PRIu64 ", val = %" PRIu64 "\n",
//               k, v, *((int64_t *)(key1.data)), *((int64_t *)(val1.data)));
        assert(r1==0);
	rval = verify_identical_dbts(&key1, &key2) |
	    verify_identical_dbts(&val1, &val2);
	assert(rval == 0);
    }
    // now verify that there are no rows after the last expected 
    r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
    assert(r1 == DB_NOTFOUND);

    c1->c_close(c1);
    if (key1.data) toku_free(key1.data);
    if (val1.data) toku_free(val1.data);
    compare_txn->commit(compare_txn, 0);
}

static void UU() crash_it(void) {
    fflush(stdout);
    fflush(stderr);
    int zero = 0;
    int divide_by_zero = 1/zero;
    printf("force use of %d\n", divide_by_zero);
    fflush(stdout);
    fflush(stderr);
}

static void crash_it_callback_f(void *dummy UU()) {
    crash_it();
}

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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "checkpoint_test.h"


static const uint64_t max_cachesize = 256 << 20;
static const int NUM_DICTIONARIES = 1;

static const int OPER_STEPS = 6;

static const int ITERATIONS_PER_CRASH_IN_RECOVERY = 7;

typedef enum __recovery_stress_steps 
{
    PRE_PRE_STEP = 0, 
    PRE_CP_STEP, 
    PRE_POST_STEP, 
    CP_CP_STEP, 
    CP_POST_STEP, 
    POST_POST_STEP
} STEP;
//const int OPER_PER_STEP = 331;
const int OPER_PER_STEP = 43;
#define OPER_PER_ITER ( OPER_STEPS * OPER_PER_STEP )

#define DBG(str)   if (verbose) printf("%s:%25s: %s\n", __FILE__, __FUNCTION__, str)
#define iDBG(iter) if (verbose) printf("%s:%25s: iter = %d\n", __FILE__, __FUNCTION__, iter)

static int firstkey(int iter, int step) { return (iter * OPER_PER_ITER) + (step * OPER_PER_STEP); }

//static toku_pthread_t thread;

static void
drop_dead(void) {
    // deliberate zerodivide or sigsegv
#if 0
    fprintf(stderr, "HAPPY CRASH\n");
#endif
    fflush(stdout);
    fflush(stderr);
    int zero = 0;
    int infinity = 1/zero;
    printf("Survived zerodivide!\n");
    fflush(stdout);
    printf("Infinity = %d\n", infinity);
    fflush(stdout);
    void * intothevoid = NULL;
    (*(int*)intothevoid)++;
    printf("intothevoid = %p, infinity = %d\n", intothevoid, infinity);
    printf("This line should never be printed\n");
    fflush(stdout);
}

static void drop_dead_callback_f(void *dummy UU()) {
    drop_dead();
}

static void verify (DICTIONARY dictionaries, int iter) {
    int i, key;
    DB *db;
//    iDBG(iter);
    for (i=0;i<NUM_DICTIONARIES;i++) {
        db = dictionaries[i].db;
        if (iter == 1) {
            key = firstkey(iter - 1, 0);
            verify_sequential_rows(db, key, OPER_PER_ITER);
        } 
        else if (iter == 2) {
            key = firstkey(iter - 2, 0);
            verify_sequential_rows(db, key, OPER_PER_ITER * 2);
        }
        else if (iter == 3) {
            key = firstkey(iter - 3, 0);
            verify_sequential_rows(db, key, OPER_PER_ITER * 3);
        }
        else if (iter > 3) {
            key = firstkey(iter - 4, 0);
            verify_sequential_rows(db, key, OPER_PER_ITER * 4);
        }
    }
    return;
}

struct iteration_spec {
    DICTIONARY dictionaries;
    int iter;
    STEP step;
    DB_TXN *pre_pre_insert_commit;
    DB_TXN *pre_cp_insert_commit;
    DB_TXN *pre_post_insert_commit;
    DB_TXN *cp_cp_insert_commit;
    DB_TXN *cp_post_insert_commit;
    DB_TXN *post_post_insert_commit;

    DB_TXN *pre_pre_insert_abort;
    DB_TXN *pre_cp_insert_abort;
    DB_TXN *pre_post_insert_abort;
    DB_TXN *cp_cp_insert_abort;
    DB_TXN *cp_post_insert_abort;
    DB_TXN *post_post_insert_abort;

    DB_TXN *pre_insert_incmplt;
    DB_TXN *cp_insert_incmplt;
    DB_TXN *post_insert_incmplt;

    DB_TXN *pre_pre_delete_commit;
    DB_TXN *pre_cp_delete_commit;
    DB_TXN *pre_post_delete_commit;
    DB_TXN *cp_cp_delete_commit;
    DB_TXN *cp_post_delete_commit;
    DB_TXN *post_post_delete_commit;

    DB_TXN *pre_pre_delete_abort;
    DB_TXN *pre_cp_delete_abort;
    DB_TXN *pre_post_delete_abort;
    DB_TXN *cp_cp_delete_abort;
    DB_TXN *cp_post_delete_abort;
    DB_TXN *post_post_delete_abort;

    DB_TXN *pre_delete_incmplt;
    DB_TXN *cp_delete_incmplt;
    DB_TXN *post_delete_incmplt;
};
typedef struct iteration_spec *ITER_SPEC;

static void pre_checkpoint_acts(ITER_SPEC spec) {
    int i;
    DB *db;
    DICTIONARY dictionaries = spec->dictionaries;
    int iter = spec->iter;
    assert(spec->step == PRE_PRE_STEP);
    int key;
    int r;

//    iDBG(iter);
    for (i=0;i<NUM_DICTIONARIES;i++) {
        db = dictionaries[i].db;


        // ---- GOOD INSERTIONS FOR THIS ITERATION ----
        // begin pre, commit pre
        key = firstkey(iter, PRE_PRE_STEP);
        r = env->txn_begin(env, NULL, &spec->pre_pre_insert_commit, 0);               CKERR(r);
        insert_n_fixed(db, NULL, spec->pre_pre_insert_commit, key, OPER_PER_STEP);
        r = spec->pre_pre_insert_commit->commit(spec->pre_pre_insert_commit, 0);      CKERR(r);

        // begin pre, commit cp, post
        key = firstkey(iter, PRE_CP_STEP);
        r = env->txn_begin(env, NULL, &spec->pre_cp_insert_commit, 0);                CKERR(r);
        insert_n_fixed(db, NULL, spec->pre_cp_insert_commit, key, OPER_PER_STEP); 
        key = firstkey(iter, PRE_POST_STEP);
        r = env->txn_begin(env, NULL, &spec->pre_post_insert_commit, 0);              CKERR(r);
        insert_n_fixed(db, NULL, spec->pre_post_insert_commit, key, OPER_PER_STEP);           


        // ---- ABORTED INSERTIONS THAT WOULD OVERWRITE PREVIOUS ITERATION ----
        if ( iter > 0 ) {
            // begin pre, abort pre
            key = firstkey(iter - 1, PRE_PRE_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_pre_insert_abort, 0);            CKERR(r);
            insert_n_broken(db, NULL, spec->pre_pre_insert_abort, key, OPER_PER_STEP);
            r = spec->pre_pre_insert_abort->abort(spec->pre_pre_insert_abort);        CKERR(r);
            // begin pre, abort cp, post
            key = firstkey(iter - 1, PRE_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_cp_insert_abort, 0);             CKERR(r);
            insert_n_broken(db, NULL, spec->pre_cp_insert_abort, key, OPER_PER_STEP);
            key = firstkey(iter - 1, PRE_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_post_insert_abort, 0);           CKERR(r);
            insert_n_broken(db, NULL, spec->pre_post_insert_abort, key, OPER_PER_STEP);
        }

        // ---- INCOMPLETE INSERTIONS THAT WOULD OVERWRITE I-2 ITERATIONS AGO ----
        if ( iter > 1 ) {
            // begin pre, incomplete
            key = firstkey(iter - 2, PRE_PRE_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_insert_incmplt, 0);   CKERR(r);
            insert_n_broken(db, NULL, spec->pre_insert_incmplt, key, OPER_PER_STEP);
        }

        // ---- ABORTED DELETES THAT WOULD DELETE I-3 ITERATIONS AGO ----
        if ( iter > 2 ) {
            // begin pre, abort pre
            key = firstkey(iter - 3, PRE_PRE_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_pre_delete_abort, 0);       CKERR(r);
            delete_n(db, NULL, spec->pre_pre_delete_abort, key, OPER_PER_STEP, 0);
            r = spec->pre_pre_delete_abort->abort(spec->pre_pre_delete_abort);   CKERR(r);
            // begin pre, abort cp, post
            key = firstkey(iter - 3, PRE_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_cp_delete_abort, 0);        CKERR(r);
            insert_n_broken(db, NULL, spec->pre_cp_delete_abort, key, OPER_PER_STEP);
            key = firstkey(iter - 3, PRE_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_post_delete_abort, 0);      CKERR(r);
            insert_n_broken(db, NULL, spec->pre_post_delete_abort, key, OPER_PER_STEP);
        }

        // ---- INCOMPLETE DELETES THAT WOULD DELETE I-4 ITERATIONS AGO ----
        if ( iter > 3 ) {
            // begin pre, incomplete
            key = firstkey(iter - 4, PRE_PRE_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_delete_incmplt, 0);     CKERR(r);
            delete_n(db, NULL, spec->pre_delete_incmplt, key, OPER_PER_STEP, 0);
        }

        // ---- GOOD DELETES THAT REMOVE I-5 ITERATIONS AGO ----
        if ( iter > 4 ) {
            // begin pre, commit pre
            key = firstkey(iter - 5, PRE_PRE_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_pre_delete_commit, 0);               CKERR(r);
            delete_n(db, NULL, spec->pre_pre_delete_commit, key, OPER_PER_STEP, 0);
            r = spec->pre_pre_delete_commit->commit(spec->pre_pre_delete_commit, 0);      CKERR(r);

            // begin pre, commit cp, post
            key = firstkey(iter - 5, PRE_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_cp_delete_commit, 0);                CKERR(r);
            delete_n(db, NULL, spec->pre_cp_delete_commit, key, OPER_PER_STEP, 0); 
            key = firstkey(iter - 5, PRE_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->pre_post_delete_commit, 0);              CKERR(r);
            delete_n(db, NULL, spec->pre_post_delete_commit, key, OPER_PER_STEP, 0);           
        }
    }
    return;
}

static void checkpoint_acts(ITER_SPEC spec) {
    int i, r, key;
    DB *db;
    int iter = spec->iter;
    DICTIONARY dictionaries = spec->dictionaries;
    assert(spec->step == CP_CP_STEP);
//    iDBG(iter);
    for (i=0;i<NUM_DICTIONARIES;i++) {
        db = dictionaries[i].db;

        // ---- GOOD INSERTIONS FOR THIS ITERATION ----
        // begin pre, commit cp
        r = spec->pre_cp_insert_commit->commit(spec->pre_cp_insert_commit, 0);      CKERR(r);
        // begin cp, commit cp
        key = firstkey(iter, CP_CP_STEP);
        r = env->txn_begin(env, NULL, &spec->cp_cp_insert_commit, 0);               CKERR(r);
        insert_n_fixed(db, NULL, spec->cp_cp_insert_commit, key, OPER_PER_STEP);
        r = spec->cp_cp_insert_commit->commit(spec->cp_cp_insert_commit, 0);      CKERR(r);

        // begin cp, commit post
        key = firstkey(iter, CP_POST_STEP);
        r = env->txn_begin(env, NULL, &spec->cp_post_insert_commit, 0);              CKERR(r);
        insert_n_fixed(db, NULL, spec->cp_post_insert_commit, key, OPER_PER_STEP);           

        // ---- ABORTED INSERTIONS THAT WOULD OVERWRITE PREVIOUS ITERATION ----
        if ( iter > 0 ) {
            // begin pre, abort cp
            r = spec->pre_cp_insert_abort->abort(spec->pre_cp_insert_abort);        CKERR(r);
            // begin cp, abort cp
            key = firstkey(iter - 1, CP_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_cp_insert_abort, 0);            CKERR(r);
            insert_n_broken(db, NULL, spec->cp_cp_insert_abort, key, OPER_PER_STEP);
            r = spec->cp_cp_insert_abort->abort(spec->cp_cp_insert_abort);        CKERR(r);
            // begin cp, abort post
            key = firstkey(iter - 1, CP_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_post_insert_abort, 0);           CKERR(r);
            insert_n_broken(db, NULL, spec->cp_post_insert_abort, key, OPER_PER_STEP);
        }

        // ---- INCOMPLETE INSERTIONS THAT WOULD OVERWRITE I-2 ITERATIONS AGO ----
        if ( iter > 1 ) {
            // begin cp, incomplete
            key = firstkey(iter - 2, CP_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_insert_incmplt, 0);   CKERR(r);
            insert_n_broken(db, NULL, spec->cp_insert_incmplt, key, OPER_PER_STEP);
        }

        // ---- ABORTED DELETES THAT WOULD DELETE I-3 ITERATIONS AGO ----
        if ( iter > 2 ) {
            // begin pre, abort cp
            r = spec->pre_cp_delete_abort->abort(spec->pre_cp_delete_abort);   CKERR(r);
            // begin cp, abort cp
            key = firstkey(iter - 3, CP_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_cp_delete_abort, 0);       CKERR(r);
            delete_n(db, NULL, spec->cp_cp_delete_abort, key, OPER_PER_STEP, 0);
            r = spec->cp_cp_delete_abort->abort(spec->cp_cp_delete_abort);   CKERR(r);
            // begin cp, abort post
            key = firstkey(iter - 3, CP_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_post_delete_abort, 0);      CKERR(r);
            insert_n_broken(db, NULL, spec->cp_post_delete_abort, key, OPER_PER_STEP);
        }

        // ---- INCOMPLETE DELETES THAT WOULD DELETE I-4 ITERATIONS AGO ----
        if ( iter > 3 ) {
            // begin pre, incomplete
            key = firstkey(iter - 4, CP_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_delete_incmplt, 0);     CKERR(r);
            delete_n(db, NULL, spec->cp_delete_incmplt, key, OPER_PER_STEP, 0);
        }

        // ---- GOOD DELETES THAT REMOVE I-5 ITERATIONS AGO ----
        if ( iter > 4 ) {
            // begin pre, commit cp
            r = spec->pre_cp_delete_commit->commit(spec->pre_cp_delete_commit, 0);      CKERR(r);
            // begin cp, commit cp
            key = firstkey(iter - 5, CP_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_cp_delete_commit, 0);               CKERR(r);
            delete_n(db, NULL, spec->cp_cp_delete_commit, key, OPER_PER_STEP, 0);
            r = spec->cp_cp_delete_commit->commit(spec->cp_cp_delete_commit, 0);      CKERR(r);

            // begin cp, commit post
            key = firstkey(iter - 5, CP_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->cp_post_delete_commit, 0);              CKERR(r);
            delete_n(db, NULL, spec->cp_post_delete_commit, key, OPER_PER_STEP, 0);           
        }
    }
    return;
}

static void post_checkpoint_acts(ITER_SPEC spec) {
    int i, r, key;
    DB *db;
    int iter = spec->iter;
    DICTIONARY dictionaries = spec->dictionaries;
    assert(spec->step == POST_POST_STEP);
//    iDBG(iter);
    for (i=0;i<NUM_DICTIONARIES;i++) {
        db = dictionaries[i].db;

        // ---- GOOD INSERTIONS FOR THIS ITERATION ----
        // begin pre, commit post
        r = spec->pre_post_insert_commit->commit(spec->pre_post_insert_commit, 0);      CKERR(r);
        // begin cp, commit post
        r = spec->cp_post_insert_commit->commit(spec->cp_post_insert_commit, 0);      CKERR(r);
        // begin post, commit post
        key = firstkey(iter, POST_POST_STEP);
        r = env->txn_begin(env, NULL, &spec->post_post_insert_commit, 0);               CKERR(r);
        insert_n_fixed(db, NULL, spec->post_post_insert_commit, key, OPER_PER_STEP);
        r = spec->post_post_insert_commit->commit(spec->post_post_insert_commit, 0);      CKERR(r);

        // ---- ABORTED INSERTIONS THAT WOULD OVERWRITE PREVIOUS ITERATION ----
        if ( iter > 0 ) {
            // begin pre, abort post
            r = spec->pre_post_insert_abort->abort(spec->pre_post_insert_abort);        CKERR(r);
            // begin cp, abort post
            r = spec->cp_post_insert_abort->abort(spec->cp_post_insert_abort);        CKERR(r);
            // begin post, abort post
            key = firstkey(iter - 1, POST_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->post_post_insert_abort, 0);            CKERR(r);
            insert_n_broken(db, NULL, spec->post_post_insert_abort, key, OPER_PER_STEP);
            r = spec->post_post_insert_abort->abort(spec->post_post_insert_abort);        CKERR(r);
        }

        // ---- INCOMPLETE INSERTIONS THAT WOULD OVERWRITE I-2 ITERATIONS AGO ----
        if ( iter > 1 ) {
            // begin post, incomplete
            key = firstkey(iter - 2, POST_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->post_insert_incmplt, 0);   CKERR(r);
            insert_n_broken(db, NULL, spec->post_insert_incmplt, key, OPER_PER_STEP);
        }

        // ---- ABORTED DELETES THAT WOULD DELETE I-3 ITERATIONS AGO ----
        if ( iter > 2 ) {
            // begin pre, abort post
            r = spec->pre_post_delete_abort->abort(spec->pre_post_delete_abort);   CKERR(r);
            // begin cp, abort post
            r = spec->cp_post_delete_abort->abort(spec->cp_post_delete_abort);   CKERR(r);
            // begin post, abort post
            key = firstkey(iter - 3, POST_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->post_post_delete_abort, 0);       CKERR(r);
            delete_n(db, NULL, spec->post_post_delete_abort, key, OPER_PER_STEP, 0);
            r = spec->post_post_delete_abort->abort(spec->post_post_delete_abort);   CKERR(r);
        }

        // ---- INCOMPLETE DELETES THAT WOULD DELETE I-4 ITERATIONS AGO ----
        if ( iter > 3 ) {
            // begin post, incomplete
            key = firstkey(iter - 4, POST_POST_STEP);
            r = env->txn_begin(env, NULL, &spec->post_delete_incmplt, 0);     CKERR(r);
            delete_n(db, NULL, spec->post_delete_incmplt, key, OPER_PER_STEP, 0);
        }

        // ---- GOOD DELETES THAT REMOVE I-5 ITERATIONS AGO ----
        if ( iter > 4 ) {
            // begin pre, commit post
            r = spec->pre_post_delete_commit->commit(spec->pre_post_delete_commit, 0);      CKERR(r);
            // begin cp, commit post
            r = spec->cp_post_delete_commit->commit(spec->cp_post_delete_commit, 0);      CKERR(r);
            // begin post, commit post
            key = firstkey(iter - 5, CP_CP_STEP);
            r = env->txn_begin(env, NULL, &spec->post_post_delete_commit, 0);               CKERR(r);
            delete_n(db, NULL, spec->post_post_delete_commit, key, OPER_PER_STEP, 0);
            r = spec->post_post_delete_commit->commit(spec->post_post_delete_commit, 0);      CKERR(r);
        }
    }
    return;
}

static void run_test (int iter) {

    uint32_t flags = 0;
    int i, r;

    if (iter == 0)
	dir_create(TOKU_TEST_FILENAME);  // create directory if first time through
    
    // Run with cachesize of 256 bytes per iteration
    // to force lots of disk I/O
    // (each iteration inserts about 4K rows/dictionary, 16 bytes/row, 4 dictionaries = 256K bytes inserted per iteration)
    uint64_t cachebytes = 0; // 0 => use default size
    const int32_t K256 = 256 * 1024;
    cachebytes = K256 * (iter + 1) - (128 * 1024);
    if (cachebytes > max_cachesize)
        cachebytes = 0;
    if (iter & 2) cachebytes = 0;       // use default cachesize half the time

    
    if (verbose) printf("%s: iter = %d\n", __FILE__, iter);

    int recovery_flags = DB_INIT_LOG|DB_INIT_TXN;
    if ( iter != 0 )
        recovery_flags += DB_RECOVER;

    // crash somewhat frequently during recovery
    //   first, wait until after the system is primed
    if ( iter > ITERATIONS_PER_CRASH_IN_RECOVERY + 5 ) {
        // every N cycles, crash in recovery
        if ( (iter % ITERATIONS_PER_CRASH_IN_RECOVERY) == 0 ) {
            // crash at different places in recovery
            if ( iter & 1 ) 
                db_env_set_recover_callback(drop_dead_callback_f, NULL);
            else
                db_env_set_recover_callback2(drop_dead_callback_f, NULL);
        }
    }

    env_startup(TOKU_TEST_FILENAME, cachebytes, recovery_flags);

    // logic below counts on a mapping of 'iter' to dictionary values
    // since crashes in recovery do not modify dictionary values
    //    need to adjust 'iter' to be iter of successful recoveries
    int crashes_in_recovery = (iter / ITERATIONS_PER_CRASH_IN_RECOVERY) - ( ( ITERATIONS_PER_CRASH_IN_RECOVERY + 5 ) / ITERATIONS_PER_CRASH_IN_RECOVERY );
    if ( crashes_in_recovery > 0 ) {
        iter = iter - crashes_in_recovery;
    }

    // create array of dictionaries
    // for each dictionary verify previous iterations and perform new inserts

    DICTIONARY_S dictionaries[NUM_DICTIONARIES];
    for (i = 0; i < NUM_DICTIONARIES; i++) {
	char name[32];
	sprintf(name, "stress_%d", i);
	init_dictionary(&dictionaries[i], flags, name);
	db_startup(&dictionaries[i], NULL);
    }

    // verify previous results
    verify(dictionaries, iter);

    struct iteration_spec spec;
    spec.iter = iter;
    spec.dictionaries = dictionaries;
    spec.step = PRE_PRE_STEP;
    // perform pre-checkpoint actions
    pre_checkpoint_acts(&spec);

    // perform checkpoint acts
    spec.step = CP_CP_STEP;
    if ( iter & 1 ) 
        db_env_set_checkpoint_callback((void (*)(void*))checkpoint_acts, &spec);
    else
        db_env_set_checkpoint_callback2((void (*)(void*))checkpoint_acts, &spec);
    r = env->txn_checkpoint(env, 0, 0, 0);
    CKERR(r);
    db_env_set_checkpoint_callback(NULL, NULL);
    db_env_set_checkpoint_callback2(NULL, NULL);

    // post checkpoint acts
    spec.step = POST_POST_STEP;
    post_checkpoint_acts(&spec);

    // if requesting crash, randomly do other non-committed acts, then "drop_dead"
    if (iter > 0) {
        if (verbose) printf("dying\n");
#if 0
	// separate thread will perform random acts on other dictionaries (not 0)
	r = toku_pthread_create(&thread, 0, random_acts, (void *) dictionaries);
	CKERR(r);
	// this thead will scribble over dictionary 0 before crash to verify that
	// post-checkpoint inserts are not in the database
	DB* db = dictionaries[0].db;
	if (iter & 1)
	    scribble(db, iter);
	else
	    thin_out(db, iter);
#endif
	uint32_t delay = myrandom();
	delay &= 0xFFF;       // select lower 12 bits, shifted up 8 for random number ...
	delay = delay << 8;   // ... uniformly distributed between 0 and 1M ...
	usleep(delay);        // ... to sleep up to one second (1M usec)
	drop_dead();
    }
    
    for (i = 0; i < NUM_DICTIONARIES; i++) {
        db_shutdown(&dictionaries[i]);
    }
    r = env->close(env, 0);
    assert((r == 0) || (r == EINVAL)); // OK to have open transactions prior to close
}


// ------------ infrastructure ----------
static void do_args(int argc, char *const argv[]);

static int iter_arg = 0;

int test_main(int argc, char *const*argv) {
    do_args(argc, argv);
    run_test(iter_arg);
    return 0;
}

static void do_args(int argc, char *const argv[]) {
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
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] [-i] [-C] \n", cmd);
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


/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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


/***


Purpose of this test is to stress the checkpoint logic.  

Multiple dictionaries are used.  Data is inserted, checkpoints are taken,
and this test verifies that all checkpoints are valid.  


Parameters:
 -cC crash or not (not crashing useful for running valgrind)
 -i # iteration number (default is to run 5 iterations)
 -n # number of operations per iteration (default 5001)
 -v verbose
 -q quiet

Each iteration does:
 - Verify that previous two iterations were correctly executed
   - Previous inserts were done correctly
   - There are no rows after last expected
 - Take checkpoint
 - Scribble over database (verifying that changes after checkpoint are not effective)
 - Spawn another thread to perform random acts (inserts/deletes/queries) to
   simulate normal database operations
 - Drop dead

***/

#define NUM_DICTIONARIES 4       // any more than 3 is overkill to exercise linked list logic

static int oper_per_iter = 5001;  // not-very-nice odd number (not a multiple of a power of two)
static int do_log_recover = 0;

static toku_pthread_t thread;

// scribble over database to make sure that changes made after checkpoint are not saved
static void UU()
scribble(DB* db, int iter) {
    int64_t firstkey;     // first key to verify/insert
    int64_t numkeys;      // number of keys to verify/insert

    if (iter > 0){
	if (iter == 1) {
	    firstkey = 0;
	    numkeys = oper_per_iter;
	}
	else {
	    firstkey = (iter - 2) * oper_per_iter;
	    numkeys = oper_per_iter * 2;
	}
    }
    
    // now insert new rows for this iteration
    firstkey = iter * oper_per_iter;
    numkeys = oper_per_iter;

    insert_n_broken(db, NULL, NULL, firstkey, numkeys);
}

// scribble over database to make sure that changes made after checkpoint are not saved
// by deleting three of every four rows
static void UU()
thin_out(DB* db, int iter) {
    int64_t firstkey;     // first key to verify/insert
    int64_t numkeys;      // number of keys to verify/insert

    if (iter > 0){
	if (iter == 1) {
	    firstkey = 0;
	    numkeys = oper_per_iter;
	}
	else {
	    firstkey = (iter - 2) * oper_per_iter;
	    numkeys = oper_per_iter * 2;
	}
    }
    
    int r;
    DBT keydbt;
    int64_t key;
    DB_TXN * txn;

    dbt_init(&keydbt, &key, sizeof(key));
    r = env->txn_begin(env, NULL, &txn, 0);
    CKERR(r);
    r = db->pre_acquire_table_lock(db, txn);
    CKERR(r);
	
    // now delete three of four rows
    firstkey = iter * oper_per_iter;
    numkeys = oper_per_iter;
    
    for (key = firstkey; key < (firstkey + numkeys); key++) {
	if (key & 0x03) {   // leave every fourth key alone
	    r = db->del(db, txn, &keydbt, DB_DELETE_ANY);
	    CKERR(r);
	}
    }
    
    if ( !do_log_recover )
        r = txn->commit(txn, 0);
    CKERR(r);

}



static void
drop_dead(void) {
    // deliberate zerodivide or sigsegv
    fprintf(stderr, "HAPPY CRASH\n");
    fflush(stdout);
    fflush(stderr);
    toku_hard_crash_on_purpose();
    printf("This line should never be printed\n");
    fflush(stdout);
}


static void
verify_and_insert (DB* db, int iter) {

    int64_t firstkey;     // first key to verify/insert
    int64_t numkeys;      // number of keys to verify/insert

    if (iter > 0){
	if (iter == 1) {
	    firstkey = 0;
	    numkeys = oper_per_iter;
	}
	else {
	    firstkey = (iter - 2) * oper_per_iter;
	    numkeys = oper_per_iter * 2;
	}
	verify_sequential_rows(db, firstkey, numkeys);
    }
    
    // now insert new rows for this iteration
    firstkey = iter * oper_per_iter;
    numkeys = oper_per_iter;

    int r;
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);               CKERR(r);
    insert_n_fixed(db, NULL, NULL, firstkey, numkeys);
    r = txn->commit(txn, 0);                              CKERR(r);
}


//  Purpose of this function is to perform a variety of random acts.  
//  This will simulate normal database operations.  The idea is for the 
//  the crash to occur sometimes during an insert, sometimes during a query, etc.
static void *
random_acts(void * d) {
    void * intothevoid = NULL;
    DICTIONARY dictionaries = (DICTIONARY) d;
    if (verbose)
	printf("perform random acts, %s\n", dictionaries[0].filename);
    fflush(stdout);
    int i = 0;
    int64_t k = 0;

    while (1) {    // run until crash
	// main thread is scribbling over dictionary 0
	// this thread will futz with other dictionaries
	for (i = 1; i < NUM_DICTIONARIES; i++) {
	    int j;
	    DB * db = dictionaries[i].db;
	    insert_random(db, NULL, NULL);
	    delete_both_random(db, NULL, NULL, 0);  // delete only if found (performs query)
	    delete_both_random(db, NULL, NULL, DB_DELETE_ANY);  // delete whether or not found (no query)
	    for (j = 0; j < 10; j++) {
		delete_fixed(db, NULL, NULL, k, 0);      // delete only if found to provoke more queries
		k++;
	    }
	}
    }


#if IS_TDB && !TOKU_WINDOWS
    return intothevoid;
#endif
}

uint64_t max_windows_cachesize = 256 << 20;

static void
run_test (int iter, int die) {

    uint32_t flags = 0;

    int i;

    if (iter == 0)
	dir_create(TOKU_TEST_FILENAME);  // create directory if first time through
    
    // Run with cachesize of 256 bytes per iteration
    // to force lots of disk I/O
    // (each iteration inserts about 4K rows/dictionary, 16 bytes/row, 4 dictionaries = 256K bytes inserted per iteration)
    const int32_t K256 = 256 * 1024;
    uint64_t cachebytes = 0;
    cachebytes = K256 * (iter + 1) - (128 * 1024);
    if (cachebytes > max_windows_cachesize)
        cachebytes = 0;
    if (iter & 2) cachebytes = 0;       // use default cachesize half the time

    if (verbose)
	printf("checkpoint_stress: iter = %d, cachesize (bytes) = 0x%08" PRIx64 "\n", iter, cachebytes);

    int recovery_flags = 0;
    if ( do_log_recover ) {
        recovery_flags += DB_INIT_LOG|DB_INIT_TXN;
        if ( iter != 0 )
            recovery_flags += DB_RECOVER;
    }
    env_startup(TOKU_TEST_FILENAME, cachebytes, recovery_flags);

    // create array of dictionaries
    // for each dictionary verify previous iterations and perform new inserts

    DICTIONARY_S dictionaries[NUM_DICTIONARIES];
    for (i = 0; i < NUM_DICTIONARIES; i++) {
	char name[32];
	sprintf(name, "stress_%d", i);
	init_dictionary(&dictionaries[i], flags, name);
	db_startup(&dictionaries[i], NULL);
	DB* db = dictionaries[i].db;
	verify_and_insert(db, iter);
    }

    // take checkpoint (all dictionaries)
    snapshot(NULL, 1);    

    if (die) {
	// separate thread will perform random acts on other dictionaries (not 0)
	int r = toku_pthread_create(&thread, 0, random_acts, (void *) dictionaries);
	CKERR(r);
	// this thead will scribble over dictionary 0 before crash to verify that
	// post-checkpoint inserts are not in the database
	DB* db = dictionaries[0].db;
	if (iter & 1)
	    scribble(db, iter);
	else
	    thin_out(db, iter);
	uint32_t delay = myrandom();
	delay &= 0xFFF;       // select lower 12 bits, shifted up 8 for random number ...
	delay = delay << 8;   // ... uniformly distributed between 0 and 1M ...
	usleep(delay);        // ... to sleep up to one second (1M usec)
	drop_dead();
    }
    else {
	for (i = 0; i < NUM_DICTIONARIES; i++) {
	    db_shutdown(&dictionaries[i]);
	}
	env_shutdown();
    }
}


static void
usage(char *progname) {
    fprintf(stderr, "Usage:\n%s [-c] [-C] [-i N] [-n N] [-l] [-q|-v]\n"
                    "      \n%s [-h]\n", progname, 
                                         progname);
}


int
test_main (int argc, char * const argv[]) {

    // get arguments, set parameters

    int iter = -1;

    int c;
    int crash = 0;
    while ((c = getopt(argc, (char * const *)argv, "cChi:qvn:lX:")) != -1) {
	switch(c) {
        case 'c':
            crash = 1;
            break;
        case 'C':
            crash = 0;
            break;
	case 'i':
	    iter = atoi(optarg);
	    break;
	case 'n':
	    oper_per_iter = atoi(optarg);
	    break;
        case 'l':
            do_log_recover = 1;
            break;
	case 'v':
	    verbose++;
            break;
	case 'q':
	    verbose--;
	    if (verbose<0) verbose=0;
            break;
        case 'X':
            if (strcmp(optarg, "novalgrind") == 0) {
                // provide a way for the shell script runner to pass an
                // arg that suppresses valgrind on this child process
                break;
            }
            // otherwise, fall through to an error
	case 'h':
        case '?':
            usage(argv[0]);
            return 1;
	default:
            assert(false);
            return 1;
	}
    }
    if (argc!=optind) { usage(argv[0]); return 1; }

    // for developing this test and for exercising with valgrind (no crash)
    if (iter <0) {
	if (verbose)
	    printf("No argument, just run five times without crash\n");
	for (iter = 0; iter<5; iter++) {
	    run_test(iter, 0);
	}
    }
    else {
	run_test(iter, crash);
    }

    return 0;

}

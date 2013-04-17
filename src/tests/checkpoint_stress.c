/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
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


#if IS_TDB && !defined(_WIN32) && !defined(_WIN64)
    return intothevoid;
#endif
}

u_int64_t max_windows_cachesize = 256 << 20;

static void
run_test (int iter, int die) {

    u_int32_t flags = DB_DUP|DB_DUPSORT;

    int i;

    if (iter == 0)
	dir_create();  // create directory if first time through
    
    // Run with cachesize of 256 bytes per iteration
    // to force lots of disk I/O
    // (each iteration inserts about 4K rows/dictionary, 16 bytes/row, 4 dictionaries = 256K bytes inserted per iteration)
    const int32_t K256 = 256 * 1024;
    u_int64_t cachebytes = 0;
    cachebytes = K256 * (iter + 1) - (128 * 1024);
    if (cachebytes > max_windows_cachesize)
        cachebytes = 0;
    if (iter & 2) cachebytes = 0;       // use default cachesize half the time

    if (verbose)
	printf("checkpoint_stress: iter = %d, cachesize (bytes) = 0x%08"PRIx64"\n", iter, cachebytes);

    int recovery_flags = 0;
    if ( do_log_recover ) {
        recovery_flags += DB_INIT_LOG;
        if ( iter != 0 )
            recovery_flags += DB_RECOVER;
    }
    env_startup(cachebytes, recovery_flags);

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
	u_int32_t delay = myrandom();
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
test_main (int argc, char *argv[]) {

    // get arguments, set parameters

    int iter = -1;

    int c;
    int crash = 0;
    while ((c = getopt(argc, argv, "cChi:qvn:l:")) != -1) {
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
	case 'h':
        case '?':
            usage(argv[0]);
            return 1;
	default:
            assert(FALSE);
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

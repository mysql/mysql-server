/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <test.h>
#include <db.h>
#include <sys/stat.h>
#include "toku_pthread.h"
#include "checkpoint_test.h"


// Purpose of this test is to verify that truncate performed during checkpoint does not cause crash.
// Method:
//   write two dictionaries, control and test
//   take a checkpoint
//     during the checkpoint (in the callback) trigger a second thread that will truncate test dictionary
//   verify contents of control dictionary
//
// Potential improvements if necessary:
//   make test more strenuous by checkpointing and truncating many dictionaries
//   add random delays to alter timing per iteration
//   verify that test dictionary does not exist on disk


// Only useful for single threaded testing, 
// but can be accessed from checkpoint_callback.
static DICTIONARY test_dictionary = NULL;

static toku_pthread_t thread;

static int iter = 0;

static void
checkpoint_truncate_test(u_int32_t flags, u_int32_t n) {
    void * ignore;

    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, 1, flags); 
	printf("Verify that truncate done during checkpoint does not crash, iter = %d\n", iter);
        fflush(stdout); 
    }
    dir_create();
    env_startup(0, FALSE);

    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    int64_t firstkey = 0;
    int64_t numkeys = n;
    insert_n_fixed(db_test.db, db_control.db, NULL, firstkey, numkeys);
    snapshot(&db_test, TRUE);  // take checkpoint, truncate db_test during checkpoint callback
    verify_sequential_rows(db_control.db, firstkey, numkeys);
    toku_pthread_join(thread, &ignore);
    db_shutdown(&db_control);
    db_shutdown(&db_test);
    env_shutdown();
}


// Purpose is to truncate test db while checkpoint is 
// in progress.
static void *
truncate_thread(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    // maybe insert random delay here if necessary

    if (verbose) {
	printf("truncating %s\n",
	       name);
	fflush(stdout);
    }
    if (iter & 1)
	toku_pthread_yield();  // increase probability of collision by having some different timing
    db_truncate(d->db, NULL);
    return NULL;
} 


static void checkpoint_callback_1(void * extra) {
    int r = toku_pthread_create(&thread, 0, truncate_thread, extra);
    CKERR(r);
}

int
test_main (int argc, char * const argv[]) {
    int limit = 4;
    parse_args(argc, argv);

    db_env_set_checkpoint_callback(checkpoint_callback_1, &test_dictionary);
    for (iter = 0; iter < limit; iter++) {
	checkpoint_truncate_test(0, 16*1024+1);
    }
    db_env_set_checkpoint_callback(NULL, NULL);

    return 0;
}

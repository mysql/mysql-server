/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

#include "checkpoint_test.h"


// Purpose of test is to verify that callbacks are called correctly
// without breaking a simple checkpoint (copied from tests/checkpoint_1.c).

static char * string_1 = "extra1";
static char * string_2 = "extra2";
static int callback_1_count = 0;
static int callback_2_count = 0;

static void checkpoint_callback_1(void * extra) {
    if (verbose) printf("checkpoint callback 1 called with extra = %s\n", *((char**) extra));
    assert(extra == &string_1);
    callback_1_count++;
}

static void checkpoint_callback_2(void * extra) {
    if (verbose) printf("checkpoint callback 2 called with extra = %s\n", *((char**) extra));
    assert(extra == &string_2);
    callback_2_count++;
}

static void
checkpoint_test_1(u_int32_t flags, u_int32_t n, int snap_all) {
    if (verbose>1) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, snap_all, flags); 
        fflush(stdout); 
    }
    dir_create();
    env_startup(0, FALSE);
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
        u_int32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
        snapshot(&db_test, snap_all);
	assert(callback_1_count == run+1);
	assert(callback_2_count == run+1);
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, NULL, NULL);
        db_replace(&db_test, NULL);
        r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
    }
    db_shutdown(&db_test);
    db_shutdown(&db_control);
    env_shutdown();
}

int
test_main (int argc, char *argv[]) {
    parse_args(argc, argv);

    db_env_set_checkpoint_callback(checkpoint_callback_1, &string_1);
    db_env_set_checkpoint_callback2(checkpoint_callback_2, &string_2);

    checkpoint_test_1(0,4096,1);
    return 0;
}

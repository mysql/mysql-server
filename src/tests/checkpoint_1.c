/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

#include "checkpoint_test.h"


// Only useful for single threaded testing, 
// but can be accessed from checkpoint_callback.
static DICTIONARY test_dictionary = NULL;
static int iter = 0;  // horrible technique, but quicker than putting in extra (this is just test code, not product code)


static void
checkpoint_test_1(u_int32_t flags, u_int32_t n, int snap_all) {
    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, snap_all, flags); 
        fflush(stdout); 
    }
    dir_create();
    env_startup();
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
        u_int32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
        snapshot(&db_test, snap_all);
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

static void
checkpoint_test_2(u_int32_t flags, u_int32_t n) {
    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, 1, flags); 
	printf("Verify that inserts done during checkpoint are effective\n");
        fflush(stdout); 
    }
    env_startup();
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
	iter = run;
        u_int32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
	r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
        snapshot(&db_test, TRUE);  // take checkpoint, insert into test db during checkpoint callback
	r = compare_dbs(db_test.db, db_control.db);
	// test and control should be different 
	assert(r!=0);
	// now insert same rows into control and they should be same 
	insert_n_fixed(db_control.db, NULL, NULL, iter * NUM_FIXED_ROWS, NUM_FIXED_ROWS);
	r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
    }
    // now close db_test via checkpoint callback (i.e. during checkpoint)
    iter = -1;  
    snapshot(&db_test, TRUE);
    db_shutdown(&db_control);
    env_shutdown();
}


 

// Purpose is to scribble over test db while checkpoint is 
// in progress.
void checkpoint_callback_1(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    int i;
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (verbose) {
	printf("checkpoint_callback_1 inserting randomly into %s\n",
	       name);
	fflush(stdout);
    }
    for (i=0; i < 1024; i++)
	insert_random(d->db, NULL, NULL);
    
}

void checkpoint_callback_2(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    assert(d==test_dictionary);
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (iter >= 0) {
	if (verbose) {
	    printf("checkpoint_callback_2 inserting fixed rows into %s\n",
		   name);
	    fflush(stdout);
	}
	insert_n_fixed(d->db, NULL, NULL, iter * NUM_FIXED_ROWS, NUM_FIXED_ROWS);
    }
    else {
	DICTIONARY_S db_temp;
	init_dictionary(&db_temp, 0, "temp");
	int i;
	if (verbose) {
	    printf("checkpoint_callback_2 closing %s\n",
		   name);
	    fflush(stdout);
	}
	db_shutdown(d);
	if (verbose) {
	    printf("checkpoint_callback_2 opening and closing unrelated dictionary\n");
	    fflush(stdout);
	}
	db_startup(&db_temp, NULL);
	for (i=0; i<1025; i++)
	    insert_random(db_temp.db, NULL, NULL);
	db_shutdown(&db_temp);	
    }
}


int
test_main (int argc, char *argv[]) {
    parse_args(argc, argv);
    u_int32_t n;
    int snap;

    n = 0;
    for (snap = 0; snap < 2; snap++) {
        checkpoint_test_1(0, n, snap);
        checkpoint_test_1(DB_DUP|DB_DUPSORT, n, snap);
    }
    for (n = 1; n <= 1<<9; n*= 2) {
        for (snap = 0; snap < 2; snap++) {
            checkpoint_test_1(0, n, snap);
            checkpoint_test_1(DB_DUP|DB_DUPSORT, n, snap);
        }
    }

    db_env_set_checkpoint_callback(checkpoint_callback_1, &test_dictionary);
    checkpoint_test_1(0,4096,1);
    db_env_set_checkpoint_callback(checkpoint_callback_2, &test_dictionary);
    checkpoint_test_2(0,4096);
    db_env_set_checkpoint_callback(NULL, NULL);

    return 0;
}

#if 0
 checkpoint_1:
   create two dbs, db_test (observed) and db_control (expected)
   loop n times:
     modify both dbs
     checkpoint db_test
     modify db_test only
     copy db_test file (system(cp)) to db_temp
     compare db_temp with db_control
     continue test using db_temp as db_test instead of db_test
     delete old db_test
 checkpoint_2,3 were subsumed into 1.


TODO: Add callback to toku_checkpoint(), called after ydb lock is released.
      (Note, checkpoint_safe_lock is still held.)

 checkpoint_4:
   Callback can do some inserts, guaranteeing that we are testing that inserts
   are done "during" a checkpoint.

 checkpoint_5:
    Callback does unrelated open, close, and related close.

 checkpoint_6: 
    Callback triggers a thread that will perform same operations as:
     * checkpoint_4
     * checkpoint_5
     * delete (relevant db)
     * delete (irrelevant db)
     * take checkpoint safe lock using YDB api, do something like insert and release it
    but those operations happen during execution of toku_cachetable_end_checkpoint().
    
 checkpoint_7
   take atomic operation lock
   perform some inserts
   on another thread, call toku_checkpoint()
   sleep a few seconds
   perform more inserts
   release atomic operations lock
   wait for checkpoint thread to complete
   verify that checkpointed db has all data inserted


#endif

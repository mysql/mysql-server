/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id: loader-reference-test.c 23787 2010-09-14 17:31:19Z dwells $"

// Verify that the indexer grabs references on the DBs

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>

DB_ENV *env;
enum {NUM_DBS=1};

static int put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_val = dest_val; src_key = src_key; src_val = src_val;
    assert(0);
    return 0;
}

char *src_name="src.db";

static void run_test(void) 
{
    int r;
    r = system("rm -rf " ENVDIR);                                                CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = toku_os_mkdir(ENVDIR "/log", S_IRWXU+S_IRWXG+S_IRWXO);                   CKERR(r);

    r = db_env_create(&env, 0);                                                  CKERR(r);
    r = env->set_lg_dir(env, "log");                                             CKERR(r);
    r = env->set_default_bt_compare(env, int64_dbt_cmp);                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);      CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_INIT_LOG;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
    r = env->checkpointing_set_period(env, 0);                                   CKERR(r);

    DB *src_db = NULL;
    r = db_create(&src_db, env, 0);                                                             CKERR(r);
    r = src_db->open(src_db, NULL, src_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);


    DB *dbs[NUM_DBS];
    for (int i = 0; i < NUM_DBS; i++) {
        r = db_create(&dbs[i], env, 0); CKERR(r);
        char key_name[32]; 
        sprintf(key_name, "key%d", i);
        r = dbs[i]->open(dbs[i], NULL, key_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
        dbs[i]->app_private = (void *) (intptr_t) i;
    }

    DB_TXN *hottxn;
    r = env->txn_begin(env, NULL, &hottxn, 0);                                   CKERR(r);

    DB_INDEXER *indexer;
    r = env->create_indexer(env, hottxn, &indexer, src_db, NUM_DBS, dbs, NULL, 0); CKERR(r);

    r = src_db->close(src_db, 0);                                                
    assert(r == EBUSY); // close the src_db with an active indexer, should not succeed

    for(int i = 0; i < NUM_DBS; i++) {
        r = dbs[i]->close(dbs[i], 0); 
        assert(r == EBUSY); // close a dest_db, should not succeed
    }

    r = indexer->abort(indexer); CKERR(r);

    r = src_db->close(src_db, 0); CKERR(r);

    r = hottxn->commit(hottxn, DB_TXN_SYNC);                                     CKERR(r);

    for(int i = 0;i < NUM_DBS; i++) {
        r = dbs[i]->close(dbs[i], 0);                                            CKERR(r);
    }

    r = env->close(env, 0);                                                      CKERR(r);
}


// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    run_test();
    return 0;
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s\n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

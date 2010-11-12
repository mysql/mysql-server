/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id: loader-reference-test.c 23787 2010-09-14 17:31:19Z dwells $"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>

DB_ENV *env;
enum {NUM_DBS=1};
enum {NUM_KV_PAIRS=3};
struct kv_pair {
    int64_t key;
    int64_t val;
};
struct kv_pair kv_pairs[NUM_KV_PAIRS] = {{1,4},
                                         {2,5},
                                         {3,6}};

static int put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {

    src_db = src_db;
    extra = extra;

    uint32_t which = (uint32_t) (intptr_t) dest_db->app_private;
    
    if (which == NUM_DBS) {
	// primary
	dbt_init(dest_key, src_key->data, src_key->size);
	dbt_init(dest_val, src_val->data, src_val->size);
    } else {
	// secondaries: switch the key and val
	dbt_init(dest_key, src_val->data, src_val->size);
	dbt_init(dest_val, src_key->data, src_key->size);
    }

//    printf("dest_key.data = %d\n", *(int*)dest_key->data);
//    printf("dest_val.data = %d\n", *(int*)dest_val->data);

    return 0;
}

static int poll_print(void *extra, float progress) {
    progress = progress;
    extra = extra;
    if ( verbose ) printf("poll_print %f\n", progress);
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
    src_db->app_private = (void *) NUM_DBS;

    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                      CKERR(r);

    for(int i=0;i<NUM_KV_PAIRS;i++) {
	DBT key, val;
        dbt_init(&key, &kv_pairs[i].key, sizeof(kv_pairs[i].key));
        dbt_init(&val, &kv_pairs[i].val, sizeof(kv_pairs[i].val));
        r = src_db->put(src_db, txn, &key, &val, 0);                             CKERR(r);
    }

    DB *dbs[NUM_DBS];
    for (int i = 0; i < NUM_DBS; i++) {
        r = db_create(&dbs[i], env, 0); CKERR(r);
        char key_name[32]; 
        sprintf(key_name, "key%d", i);
        r = dbs[i]->open(dbs[i], NULL, key_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
        dbs[i]->app_private = (void *) (intptr_t) i;
    }

    DB_TXN *hottxn;
    r = env->txn_begin(env, NULL, &hottxn, 0);                                                               
    CKERR(r);

    DB_INDEXER *indexer;
    r = env->create_indexer(env, hottxn, &indexer, src_db, NUM_DBS, dbs, NULL, 0);
    CKERR(r);
    r = indexer->set_error_callback(indexer, NULL, NULL);
    CKERR(r);
    r = indexer->set_poll_function(indexer, poll_print, NULL);
    CKERR(r);

    // setup putm
    DB *putm_dbs[NUM_DBS+1];
    for (int i = 0; i < NUM_DBS; i++) 
	putm_dbs[i] = dbs[i];
    putm_dbs[NUM_DBS] = src_db;

    DBT putm_keys[NUM_DBS+1], putm_vals[NUM_DBS+1]; 

    uint32_t putm_flags[NUM_DBS+1];
    for (int i = 0; i < NUM_DBS+1; i++)
	putm_flags[i] = 0;

    DBT prikey; int64_t pk;
    dbt_init(&prikey, &pk, sizeof pk);

    DBT prival; int64_t pv;
    dbt_init(&prival, &pv, sizeof pv);

    // putm (8,9)
    pk = 8; pv = 9;
    r = env->put_multiple(env, src_db, txn, &prikey, &prival, NUM_DBS+1, putm_dbs, putm_keys, putm_vals, putm_flags, NULL);
    CKERR(r);

    r = indexer->build(indexer);
    CKERR(r);

    // putm (9, 10)
    pk = 9; pv = 10;
    r = env->put_multiple(env, src_db, txn, &prikey, &prival, NUM_DBS+1, putm_dbs, putm_keys, putm_vals, putm_flags, NULL);
    CKERR(r);

    r = indexer->close(indexer);
    CKERR(r);
    r = hottxn->commit(hottxn, DB_TXN_SYNC);
    CKERR(r);
    
    r = txn->commit(txn, DB_TXN_SYNC);                                           CKERR(r);

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                            CKERR(r);
    }

    r = src_db->close(src_db, 0);                                                CKERR(r);
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

/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"


// Verify that log-suppress recovery is done properly.  (See ticket 2781.)


#include <sys/stat.h>
#include <db.h>
#include "test.h"
#include "ydb-internal.h"


const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
char *namea="a.db";

DB_ENV *env;
DB_TXN *parent;
DB_TXN *child;



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Some loader functions to allow test to work:


// There is no handlerton in this test, so this function is a local replacement
// for the handlerton's generate_row_for_put().
static int put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {

    src_db = src_db;
    extra = extra;

    uint32_t which = *(uint32_t*)dest_db->app_private;

    if ( which == 0 ) {
        if (dest_key->flags==DB_DBT_REALLOC) {
            if (dest_key->data) toku_free(dest_key->data);
            dest_key->flags = 0;
            dest_key->ulen  = 0;
        }
        if (dest_val->flags==DB_DBT_REALLOC) {
            if (dest_val->data) toku_free(dest_val->data);
            dest_val->flags = 0;
            dest_val->ulen  = 0;
        }
        dbt_init(dest_key, src_key->data, src_key->size);
        dbt_init(dest_val, src_val->data, src_val->size);
    }
    else {
        assert(dest_key->flags==DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(unsigned int)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(unsigned int));
            dest_key->ulen = sizeof(unsigned int);
        }
        assert(dest_val->flags==DB_DBT_REALLOC);
        if (dest_val->ulen < sizeof(unsigned int)) {
            dest_val->data = toku_xrealloc(dest_val->data, sizeof(unsigned int));
            dest_val->ulen = sizeof(unsigned int);
        }
        unsigned int *new_key = (unsigned int *)dest_key->data;
        unsigned int *new_val = (unsigned int *)dest_val->data;

        *new_key = 1;
        *new_val = 2;

        dest_key->size = sizeof(unsigned int);
        dest_val->size = sizeof(unsigned int);
        //data is already set above
    }

//    printf("dest_key.data = %d\n", *(int*)dest_key->data);
//    printf("dest_val.data = %d\n", *(int*)dest_val->data);

    return 0;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The test itself:

#define MAX_DBS 1
#define NUM_ROWS 1
#define NUM_DBS 1

// Create loader, insert row(s)
static void 
load(DB **dbs) {
    int r;
    DB_TXN    *ptxn = NULL;
    DB_TXN    *txn = NULL;
    DB_LOADER *loader;
    uint32_t db_flags[MAX_DBS];
    uint32_t dbt_flags[MAX_DBS];
    for(int i=0;i<MAX_DBS;i++) { 
        db_flags[i] = DB_NOOVERWRITE; 
        dbt_flags[i] = 0;
    }
    uint32_t loader_flags = 0;

    // create and initialize loader
    r = env->txn_begin(env, NULL, &ptxn, 0);
    CKERR(r);
    r = env->txn_begin(env, ptxn, &txn, 0);                                                               
    CKERR(r);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    for(int i=1;i<=NUM_ROWS;i++) {
        k = i;
        v = i+1;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
	CKERR(r);
    }

    // close loader
    r = loader->close(loader);
    CKERR(r);

    r = txn->commit(txn, 0);
    CKERR(r);
}


static void
checkpoint_callback(void * UU(extra)){
    printf("Deliberately crash during checkpoint\n"); 
    fflush(stdout);
    int r = env->log_flush(env, NULL); //TODO: USe a real DB_LSN* instead of NULL
    CKERR(r);
    toku_hard_crash_on_purpose();
}

static void
do_x1_shutdown (void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);

    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, 0);                                 CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));

    char name[128];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[MAX_DBS];
    for(int i=0;i<NUM_DBS;i++) {
        idx[i] = i;
        r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
        r = dbs[i]->set_descriptor(dbs[i], 1, &desc);                                       CKERR(r);
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x", i);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
    }

    load(dbs);

    //    r=env->txn_begin(env, 0, &parent, 0);                                      assert(r==0);
    //    r=env->txn_begin(env, &parent, &child, 0);                                 assert(r==0);

    // crash during checkpoint
    db_env_set_checkpoint_callback(checkpoint_callback, NULL);
    r=env->txn_checkpoint(env, 0, 0, 0);                                            CKERR(r);
}

static void
do_x1_recover (BOOL UU(did_commit)) {
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);

    /*****
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO);                       CKERR(r);
    r=db->get(db, tid, dbt_init(&key, "a", 2), dbt_init_malloc(&data), 0);     assert(r==0); 
    r=tid->commit(tid, 0);                                                     assert(r==0);
    toku_free(data.data);
    r=db->close(db, 0);                                                        CKERR(r);
    *********/
    r=env->close(env, 0);                                                      CKERR(r);
}

BOOL do_commit=FALSE, do_recover_committed=FALSE;

static void
x1_parse_args (int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--test") == 0) {
	    do_commit=TRUE;
	} else if (strcmp(argv[0], "--recover") == 0) {
	    do_recover_committed=TRUE;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--commit | --abort | --explicit-abort | --recover-committed | --recover-aborted } \n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
    {
	int n_specified=0;
	if (do_commit)            n_specified++;
	if (do_recover_committed) n_specified++;
	if (n_specified>1) {
	    printf("Specify only one of --commit or --abort or --recover-committed or --recover-aborted\n");
	    resultcode=1;
	    goto do_usage;
	}
    }
}

int
test_main (int argc, char * const argv[])
{
    x1_parse_args(argc, argv);
    if (do_commit) {
	do_x1_shutdown();
    } else if (do_recover_committed) {
	do_x1_recover(TRUE);
    } 
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}

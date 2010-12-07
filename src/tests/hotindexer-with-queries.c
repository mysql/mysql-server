/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "key-val.h"

enum {NUM_INDEXER_INDEXES=1};
static const int NUM_DBS = NUM_INDEXER_INDEXES + 1; // 1 for source DB
static const int NUM_ROWS = 1000000;
int num_rows;
typedef enum {FORWARD = 0, BACKWARD} Direction;
typedef enum {TXN_NONE = 0, TXN_CREATE = 1, TXN_END = 2} TxnWork;

DB_ENV *env;

/*
 *  client scans the primary table (like a range query)
 */

static void * client(void *arg)
{
    DB *src = (DB *)arg;
    if ( verbose ) printf("client start\n");

    int r;
    struct timeval start, now;
    DB_TXN *txn;
    DBT key, val;
    uint32_t k, v;

    dbt_init(&key, &k, sizeof(unsigned int));
    dbt_init(&val, &v, sizeof(unsigned int));
    
    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    
    DBC *cursor;
    r = src->cursor(src, txn, &cursor, 0);  CKERR(r);

    int row = 0;

    gettimeofday(&start,0);
    while ( r != DB_NOTFOUND ) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if ( r != DB_NOTFOUND ) {
            row++;
        }
    }
    gettimeofday(&now, 0);
    if ( verbose ) printf("client : primary table scanned in %d sec, contains %d rows\n",
                          (int)(now.tv_sec - start.tv_sec),
                          row);

    r = cursor->c_close(cursor); CKERR(r);
    r = txn->commit(txn, 0); CKERR(r);
    if ( verbose ) printf("client done\n");

    return NULL;
}

toku_pthread_t *client_thread;
static void client_init(void) 
{
    client_thread = (toku_pthread_t *)toku_malloc(sizeof(toku_pthread_t));
}

static void client_cleanup(void) 
{
    toku_free(client_thread);   client_thread = NULL;
}

static void query_only(DB *src) 
{
    int r;
    void *t0;
    client_init();

    // start thread doing query
    r = toku_pthread_create(client_thread, 0, client, (void *)src);  
    CKERR(r);

    r = toku_pthread_join(*client_thread, &t0);     
    CKERR(r);
    
    client_cleanup();
}

static void test_indexer(DB *src, DB **dbs)
{
    int r;
    DB_TXN    *txn;
    DB_INDEXER *indexer;
    uint32_t db_flags[NUM_DBS];

    if ( verbose ) printf("test_indexer\n");
    for(int i=0;i<NUM_DBS;i++) { 
        db_flags[i] = DB_YESOVERWRITE; 
    }

    client_init();

    // create and initialize indexer
    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);

    if ( verbose ) printf("test_indexer create_indexer\n");
    r = env->create_indexer(env, txn, &indexer, src, NUM_DBS-1, &dbs[1], db_flags, 0);
    CKERR(r);
    r = indexer->set_error_callback(indexer, NULL, NULL);
    CKERR(r);
    r = indexer->set_poll_function(indexer, poll_print, NULL);
    CKERR(r);

    // start thread doing query
    r = toku_pthread_create(client_thread, 0, client, (void *)src);  CKERR(r);

    struct timeval start, now;
    if ( verbose ) {
        printf("test_indexer build\n");
        gettimeofday(&start,0);
    }
    r = indexer->build(indexer);
    CKERR(r);
    if ( verbose ) {
        gettimeofday(&now,0);
        int duration = (int)(now.tv_sec - start.tv_sec);
        if ( duration > 0 )
            printf("test_indexer build : sec = %d\n", duration);
    }

    void *t0;
    r = toku_pthread_join(*client_thread, &t0);     CKERR(r);

    if ( verbose ) printf("test_indexer close\n");
    r = indexer->close(indexer);
    CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);
    CKERR(r);

    client_cleanup();
}


static void run_test(void) 
{
    int r;
    r = system("rm -rf " ENVDIR);                                                CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = toku_os_mkdir(ENVDIR "/log", S_IRWXU+S_IRWXG+S_IRWXO);                   CKERR(r);

    r = db_env_create(&env, 0);                                                  CKERR(r);
    r = env->set_redzone(env, 0);                                                CKERR(r);
    r = env->set_lg_dir(env, "log");                                             CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                           CKERR(r);
    generate_permute_tables();
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);      CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_INIT_LOG;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 0);                                   CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    int ids[MAX_DBS];
    DB *dbs[MAX_DBS];
    for (int i = 0; i < NUM_DBS; i++) {
        ids[i] = i;
        r = db_create(&dbs[i], env, 0); CKERR(r);
        r = dbs[i]->set_descriptor(dbs[i], 1, &desc);                                       CKERR(r);
        dbs[i]->app_private = &ids[i];
        char key_name[32]; 
        sprintf(key_name, "key%d", i);
        r = dbs[i]->open(dbs[i], NULL, key_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
    }

    // generate the src DB (do not use put_multiple)
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                      CKERR(r);
    r = generate_initial_table(dbs[0], txn, num_rows);                           CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);                                           CKERR(r);

    // scan the whole table twice to reduce possible flattening effects
    // -------------------------- //
    query_only(dbs[0]);
    query_only(dbs[0]);
    // -------------------------- //

    // scan the whole table while running the indexer
    // -------------------------- //
    test_indexer(dbs[0], dbs);
    // -------------------------- //

    // scan the whole table again to confirm performance
    // -------------------------- //
    query_only(dbs[0]);
    // -------------------------- //

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                            CKERR(r);
    }
    r = env->close(env, 0);                                                      CKERR(r);

    if ( verbose && (r == 0)) printf("PASS\n");
}

// ------------ infrastructure ----------

#include <sched.h>

static inline void
do_args (int argc, char * const argv[]) {
    const char *progname=argv[0];
    num_rows = NUM_ROWS;
    int num_cpus = 0;
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r")==0) {
            argc--; argv++;
            num_rows = atoi(argv[0]);
        } else if (strcmp(argv[0], "--ncpus") == 0 && argc+1 > 0) {
            argc--; argv++;
            num_cpus = atoi(argv[0]);
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q] [-r rows]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }

    if (num_cpus > 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int i = 0; i < num_cpus; i++)
            CPU_SET(i, &cpuset);
        int r;
        r = sched_setaffinity(toku_os_getpid(), sizeof cpuset, &cpuset);
        assert(r == 0);
        
        cpu_set_t use_cpuset;
        CPU_ZERO(&use_cpuset);
        r = sched_getaffinity(toku_os_getpid(), sizeof use_cpuset, &use_cpuset);
        assert(r == 0);
        assert(memcmp(&cpuset, &use_cpuset, sizeof cpuset) == 0);
    }
}


int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    run_test();
    return 0;
}

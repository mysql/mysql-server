/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "key-val.h"

enum {NUM_INDEXER_INDEXES=1};
static const int NUM_DBS = NUM_INDEXER_INDEXES + 1; // 1 for source DB
static const int NUM_ROWS = 10000;
int num_rows;
typedef enum {FORWARD = 0, BACKWARD} Direction;
typedef enum {TXN_NONE = 0, TXN_CREATE = 1, TXN_END = 2} TxnWork;

DB_ENV *env;

/*
 *  client() is a routine intended to be run in a separate thread from index creation
 *     - it takes a client spec which describes work to be done
 *     - direction : move to ever increasing or decreasing rows
 *     - txnwork : whether a transaction should be created or closed within the client
 *         (allows client transaction to start before or during index creation, 
 *          and to close during or after index creation)
 */

typedef struct client_spec {
    uint32_t num; // number of rows to write
    uint32_t start; // approximate start row 
    int      offset; // offset from stride (= MAX_CLIENTS)
    Direction dir;
    TxnWork txnwork;
    DB_TXN *txn;
    uint32_t max_inserts_per_txn;  // this is for the parent transaction
    DB **dbs;
    int client_number;
    uint32_t *flags;
} client_spec_t, *client_spec;

int client_count = 0;

static void * client(void *arg)
{
    client_spec cs = arg;
    client_count++;
    if ( verbose ) printf("client[%d]\n", cs->client_number);
    assert(cs->client_number < MAX_CLIENTS);
    assert(cs->dir == FORWARD || cs->dir == BACKWARD);

    int r;
    if ( cs->txnwork | TXN_CREATE ) { r = env->txn_begin(env, NULL, &cs->txn, 0);  CKERR(r); }

    DBT key, val;
    DBT dest_keys[NUM_DBS];
    DBT dest_vals[NUM_DBS];
    uint32_t k, v;
    int n = cs->start;

    for(int which=0;which<NUM_DBS;which++) {
        dbt_init(&dest_keys[which], NULL, 0);
        dest_keys[which].flags = DB_DBT_REALLOC;

        dbt_init(&dest_vals[which], NULL, 0);
        dest_vals[which].flags = DB_DBT_REALLOC;
    }

    int rr;
    uint32_t inserts = 0;
    for (uint32_t i = 0; i < cs->num; i++ ) {
        DB_TXN *txn;
        env->txn_begin(env, cs->txn, &txn, 0);
        k = key_to_put(n, cs->offset);
        v = generate_val(k, 0);
        dbt_init(&key, &k, sizeof(k));
        dbt_init(&val, &v, sizeof(v));

        rr = env->put_multiple(env,
                               cs->dbs[0],
                               txn,
                               &key, 
                               &val,
                               NUM_DBS,
                               cs->dbs, // dest dbs
                               dest_keys,
                               dest_vals, 
                               cs->flags);
        if ( rr != 0 ) {
            if ( verbose ) printf("client[%u] : put_multiple returns %d, i=%u, n=%u, key=%u\n", cs->client_number, rr, i, n, k);
            r = txn->abort(txn); CKERR(r);
            break;
        }
        r = txn->commit(txn, 0); CKERR(r);
        // limit the number of inserts per parent transaction to prevent lock escalation
        inserts++;
        if ( inserts >= cs->max_inserts_per_txn ) {
            r = cs->txn->commit(cs->txn, 0); CKERR(r);
            r = env->txn_begin(env, NULL, &cs->txn, 0); CKERR(r);
            inserts = 0;
        }
        n = ( cs->dir == FORWARD ) ? n + 1 : n - 1;
    }

    if ( cs->txnwork | TXN_END )    { r = cs->txn->commit(cs->txn, DB_TXN_SYNC);       CKERR(r); }
    if (verbose) printf("client[%d] done\n", cs->client_number);

    for (int which=0; which<NUM_DBS; which++) {
        toku_free(dest_keys[which].data);
        toku_free(dest_vals[which].data);
    }


    return 0;
}

toku_pthread_t *client_threads;
client_spec_t  *client_specs;

static void clients_init(DB **dbs, uint32_t *flags) 
{
    client_threads = toku_malloc(sizeof(toku_pthread_t) * MAX_CLIENTS);
    client_specs   = toku_malloc(sizeof(client_spec_t) * MAX_CLIENTS);
    
    client_specs[0].client_number = 0;
    client_specs[0].start         = 0;
    client_specs[0].num           = num_rows;
    client_specs[0].offset        = -1;
    client_specs[0].dir           = FORWARD;
    client_specs[0].txnwork       = TXN_CREATE | TXN_END;
    client_specs[0].txn           = NULL;
    client_specs[0].max_inserts_per_txn = 1000;
    client_specs[0].dbs           = dbs;
    client_specs[0].flags         = flags;

    client_specs[1].client_number = 1;
    client_specs[1].start         = 0;
    client_specs[1].num           = num_rows;
    client_specs[1].offset        = 1;
    client_specs[1].dir           = FORWARD;
    client_specs[1].txnwork       = TXN_CREATE | TXN_END;
    client_specs[1].txn           = NULL;
    client_specs[1].max_inserts_per_txn = 100;
    client_specs[1].dbs           = dbs;
    client_specs[1].flags         = flags;

    client_specs[2].client_number = 2;
    client_specs[2].start         = num_rows -1;
    client_specs[2].num           = num_rows;
    client_specs[2].offset        = -2;
    client_specs[2].dir           = BACKWARD;
    client_specs[2].txnwork       = TXN_CREATE | TXN_END;
    client_specs[2].txn           = NULL;
    client_specs[2].max_inserts_per_txn = 1000;
    client_specs[2].dbs           = dbs;
    client_specs[2].flags         = flags;
}

static void clients_cleanup(void) 
{
    toku_free(client_threads);   client_threads = NULL;
    toku_free(client_specs);       client_specs = NULL;
}

// verify results
//  - read the keys in the primary table, then calculate what keys should exist
//     in the other DB.  Read the other table to verify.
static int check_results(DB *src, DB *db) 
{
    int r;
    int fail = 0;

    int clients = client_count;

    int max_rows = ( clients + 1 ) * num_rows;
    unsigned int *db_keys = (unsigned int *) toku_malloc(max_rows * sizeof (unsigned int));

    DBT key, val;
    unsigned int k=0, v=0;
    dbt_init(&key, &k, sizeof(unsigned int));
    dbt_init(&val, &v, sizeof(unsigned int));
    
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    
    DBC *cursor;
    r = src->cursor(src, txn, &cursor, 0);  CKERR(r);

    int which = *(uint32_t*)db->app_private;

    // scan the primary table,
    // calculate the expected keys in 'db'
    int row = 0;
    while ( r != DB_NOTFOUND ) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if ( r != DB_NOTFOUND ) {
            k = *((uint32_t *)(key.data));
            db_keys[row] = twiddle32(k, which);
            row++;
        }
    }
    if ( verbose ) printf("primary table scanned, contains %d rows\n", row);
    int primary_rows = row;
    r = cursor->c_close(cursor); CKERR(r);
    // sort the expected keys 
    qsort(db_keys, primary_rows, sizeof (unsigned int), uint_cmp);

    if ( verbose > 1 ) {
        for(int i=0;i<primary_rows;i++) {
            printf("primary table[%u] = %u\n", i, db_keys[i]);
        }
    }

    // scan the indexer-created DB, comparing keys with expected keys
    //   - there should be exactly 'primary_rows' in the new index
    r = db->cursor(db, txn, &cursor, 0); CKERR(r);
    for (int i=0;i<primary_rows;i++) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT); 
        if ( r == DB_NOTFOUND ) {
            printf("scan of index finds last row is %d\n", i);
        }
        CKERR(r);
        k = *((uint32_t *)(key.data));
        if ( db_keys[i] != k ) {
            if ( verbose ) printf("ERROR expecting key %10u for row %d, found key = %10u\n", db_keys[i],i,k);
            fail = 1;
            goto check_results_error;
        }
    }
    // next cursor op should return DB_NOTFOUND
    r = cursor->c_get(cursor, &key, &val, DB_NEXT);
    assert(r == DB_NOTFOUND);

    // we're done - cleanup and close
check_results_error:
    r = cursor->c_close(cursor); CKERR(r);
    toku_free(db_keys);
    r = txn->commit(txn, 0);  CKERR(r);
    if ( verbose ) {
        if ( fail ) printf("check_results : fail\n");
        else printf("check_results : pass\n");
    }
    return fail;
}

static void test_indexer(DB *src, DB **dbs)
{
    int r;
    DB_TXN    *txn;
    DB_INDEXER *indexer;
    uint32_t db_flags[NUM_DBS];


    if ( verbose ) printf("test_indexer\n");
    for(int i=0;i<NUM_DBS;i++) { 
        db_flags[i] = 0; 
    }
    clients_init(dbs, db_flags);

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

    // start threads doing additional inserts - no lock issues since indexer already created
    r = toku_pthread_create(&client_threads[0], 0, client, (void *)&client_specs[0]);  CKERR(r);
    r = toku_pthread_create(&client_threads[1], 0, client, (void *)&client_specs[1]);  CKERR(r);
//    r = toku_pthread_create(&client_threads[2], 0, client, (void *)&client_specs[2]);  CKERR(r);

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

    void *t0;      r = toku_pthread_join(client_threads[0], &t0);     CKERR(r);
    void *t1;      r = toku_pthread_join(client_threads[1], &t1);     CKERR(r);
//    void *t2;      r = toku_pthread_join(client_threads[2], &t2);     CKERR(r);

    if ( verbose ) printf("test_indexer close\n");
    r = indexer->close(indexer);
    CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);
    CKERR(r);

    clients_cleanup();

    if ( verbose ) printf("check_results\n");
    r = check_results(src, dbs[1]);
    CKERR(r);

    if ( verbose && (r == 0)) printf("PASS\n");
    if ( verbose && (r == 0)) printf("test_indexer done\n");
}


static void run_test(void) 
{
    int r;
    r = system("rm -rf " ENVDIR);                                                CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = toku_os_mkdir(ENVDIR "/log", S_IRWXU+S_IRWXG+S_IRWXO);                   CKERR(r);

    r = db_env_create(&env, 0);                                                  CKERR(r);
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
        dbs[i]->app_private = &ids[i];
        char key_name[32]; 
        sprintf(key_name, "key%d", i);
        r = dbs[i]->open(dbs[i], NULL, key_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            CHK(dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0));
        });
    }

    // generate the src DB (do not use put_multiple)
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                      CKERR(r);
    r = generate_initial_table(dbs[0], txn, num_rows);                           CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);                                           CKERR(r);

    // -------------------------- //
    if (1) test_indexer(dbs[0], dbs);
    // -------------------------- //

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                            CKERR(r);
    }
    r = env->close(env, 0);                                                      CKERR(r);
}

// ------------ infrastructure ----------

static inline void
do_args (int argc, char * const argv[]) {
    const char *progname=argv[0];
    num_rows = NUM_ROWS;
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r")==0) {
            argc--; argv++;
            num_rows = atoi(argv[0]);
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q] [-r rows]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}


int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    run_test();
    return 0;
}


/*
 * Please ignore this code - I don't think I'm going to use it, but I don't want to lose it
 *   I will delete this later - Dave

        if ( rr != 0 ) {  // possible lock deadlock
            if (verbose > 1) {
                printf("client[%u] : put_multiple returns %d, i=%u, n=%u, key=%u\n", cs->client_number, rr, i, n, k);
                if ( verbose > 2 ) print_engine_status(env);
            }
            // abort the transaction, freeing up locks associated with previous put_multiples
            if ( verbose > 1 ) printf("start txn abort\n");
            r = txn->abort(txn); CKERR(r);
            if ( verbose > 1 ) printf("      txn aborted\n");
            sleep(2 + cs->client_number);
            // now retry, waiting until the deadlock resolves itself
            r = env->txn_begin(env, cs->txn, &txn, 0); CKERR(r);
            if ( verbose > 1 ) printf("txn begin\n");
            while ( rr != 0 ) {
                rr = env->put_multiple(env,
                                       cs->dbs[0],
                                       txn,
                                       &key, 
                                       &val,
                                       NUM_DBS,
                                       cs->dbs, // dest dbs
                                       dest_keys,
                                       dest_vals, 
                                       cs->flags,
                                       NULL);
                if ( rr != 0 ) {
                    if ( verbose ) printf("client[%u] : put_multiple returns %d, i=%u, n=%u, key=%u\n", cs->client_number, rr, i, n, k);
                    if ( verbose ) printf("start txn abort\n");
                    r = txn->abort(txn); CKERR(r);
                    if ( verbose ) printf("      txn aborted\n");
                    sleep(2 + cs->client_number);
                    r = env->txn_begin(env, cs->txn, &txn, 0); CKERR(r);
                    if ( verbose ) printf("txn begin\n");
                }
            }
 */

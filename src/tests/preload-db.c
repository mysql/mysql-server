/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#define kv_pair_funcs 1 // pull in kv_pair generators from test.h

#include "test.h"
#include "toku_pthread.h"
#include "toku_atomic.h"
#include <db.h>
#include <sys/stat.h>
#include "ydb-internal.h"

#include "test_kv_gen.h"
/*
 */

DB_ENV *env;
enum {MAX_NAME=128};
enum {ROWS_PER_TRANSACTION=10000};
int NUM_DBS=5;
int NUM_ROWS=100000;
int CHECK_RESULTS=0;
int littlenode = 0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;
int ALLOW_DUPS=0;

static struct timeval starttime;
static double UU() elapsed_time (void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec - starttime.tv_sec + 1e-6*(now.tv_usec - starttime.tv_usec);
}

static void preload_dbs(DB **dbs)
{
    gettimeofday(&starttime, NULL);
    int r;
    DB_TXN    *txn;
    uint32_t db_flags[MAX_DBS];
    uint32_t dbt_flags[MAX_DBS];
    uint32_t flags = DB_NOOVERWRITE;
    flags = DB_YESOVERWRITE;
    for(int i=0;i<MAX_DBS;i++) { 
        db_flags[i] = flags;
        dbt_flags[i] = 0;
    }
    

    DBT skey, sval;
    DBT key, val;
    dbt_init_realloc(&key);
    dbt_init_realloc(&val);
    unsigned int k, v;
    if ( verbose ) { printf("loading");fflush(stdout); }
    int outer_loop_num = ( NUM_ROWS <= ROWS_PER_TRANSACTION ) ? 1 : (NUM_ROWS / ROWS_PER_TRANSACTION);
    for(int x=0;x<outer_loop_num;x++) {
        r = env->txn_begin(env, NULL, &txn, 0);                                                              CKERR(r);
        for(int i=1;i<=ROWS_PER_TRANSACTION;i++) {
            k = i + (x*ROWS_PER_TRANSACTION);
            v = generate_val(k, 0);
            dbt_init(&skey, &k, sizeof(unsigned int));
            dbt_init(&sval, &v, sizeof(unsigned int));

            for(int db = 0;db < NUM_DBS;db++) {
                put_multiple_generate(dbs[db], // dest_db
                                      NULL, // src_db, ignored
                                      &key, &val, // dest_key, dest_val
                                      &skey, &sval, // src_key, src_val
                                      NULL); // extra, ignored

                r = dbs[db]->put(dbs[db], txn, &key, &val, 0);                                               CKERR(r);
                if (key.flags == 0) { dbt_init_realloc(&key); }
                if (val.flags == 0) { dbt_init_realloc(&val); }
            }
        }
        r = txn->commit(txn, 0);                                                                             CKERR(r);
        if ( verbose ) {printf(".");fflush(stdout);}
    }
    if ( key.flags ) { toku_free(key.data); key.data = NULL; }
    if ( val.flags ) { toku_free(val.data); key.data = NULL; }

    if ( CHECK_RESULTS) {
        if ( verbose ) {printf("\nchecking");fflush(stdout);}
        check_results(env, dbs, NUM_DBS, NUM_ROWS);
    }
    if ( verbose) {printf("\ndone\n");fflush(stdout);}
}


char *free_me = NULL;
char *env_dir = ENVDIR; // the default env_dir.

static void run_test(void) 
{
    int r;
    {
	int len = strlen(env_dir) + 20;
	char syscmd[len];
	r = snprintf(syscmd, len, "rm -rf %s", env_dir);
	assert(r<len);
	r = system(syscmd);                                                                                   CKERR(r);
    }
    r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO);                                                      CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
//    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
//    r = env->set_default_dup_compare(env, uint_dbt_cmp);                                                      CKERR(r);
//    if ( verbose ) printf("CACHESIZE = %d MB\n", CACHESIZE);
//    r = env->set_cachesize(env, CACHESIZE / 1024, (CACHESIZE % 1024)*1024*1024, 1);                           CKERR(r);
//    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 0);                                                                CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    char name[MAX_NAME*2];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[MAX_DBS];
    for(int i=0;i<NUM_DBS;i++) {
        idx[i] = i;
        r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
	if (littlenode) {
	    r=dbs[i]->set_pagesize(dbs[i], 4096);
	    CKERR(0);	    
	}
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x", i);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            CHK(dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0));
        });
    }

    generate_permute_tables();

    // -------------------------- //
    preload_dbs(dbs);
    // -------------------------- //

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                                                         CKERR(r);
        dbs[i] = NULL;
    }

    if (verbose >= 2)
	print_engine_status(env);
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);

    /*********** DO NOT TRIM LOGFILES: Trimming logfiles defeats purpose of upgrade tests which must handle untrimmed logfiles.
    // reopen, then close environment to trim logfiles
    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                           CKERR(r);
    r = env->close(env, 0);                                                                                   CKERR(r);
    ***********/

}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const argv[]) {
    do_args(argc, argv);
    run_test();
    if (free_me) toku_free(free_me);
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
	    fprintf(stderr, "Usage: -h -c -n -d <num_dbs> -r <num_rows> %s\n", cmd);
	    exit(resultcode);
        } else if (strcmp(argv[0], "-d")==0) {
            argc--; argv++;
            NUM_DBS = atoi(argv[0]);
            if ( NUM_DBS > MAX_DBS ) {
                fprintf(stderr, "max value for -d field is %d\n", MAX_DBS);
                resultcode=1;
                goto do_usage;
            }
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-n")==0) {
            littlenode = 1;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

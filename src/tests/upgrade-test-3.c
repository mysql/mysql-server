/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id: loader-stress-test.c 20470 2010-05-20 18:30:04Z bkuszmaul $"

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
int NUM_DBS=5;
int NUM_ROWS=100000;
int CHECK_RESULTS=0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;

char *db_v3_dir = "../../utils/dir.preload-3.1-db.c.tdb";
char *db_v4_dir = "dir.preload-3.1-db.c.tdb";
char *env_dir = ENVDIR; // the default env_dir.

int SRC_VERSION = 4;

static void upgrade_test_3(DB **dbs) {
    int r;
    // open the DBS
    {
        DBT desc;
        dbt_init(&desc, "foo", sizeof("foo"));
        char name[MAX_NAME*2];

        int idx[MAX_DBS];
        for(int i=0;i<NUM_DBS;i++) {
            idx[i] = i;
            r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
            r = dbs[i]->set_descriptor(dbs[i], 1, &desc);                                                         CKERR(r);
            dbs[i]->app_private = &idx[i];
            snprintf(name, sizeof(name), "db_%04x", i);
            r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        }
    }
    // insert some rows
    printf("ToDo : insert rows\n");
    // close
    {
        for(int i=0;i<NUM_DBS;i++) {
            dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
            dbs[i] = NULL;
        }
    }
    // open
    {
        DBT desc;
        dbt_init(&desc, "foo", sizeof("foo"));
        char name[MAX_NAME*2];

        int idx[MAX_DBS];
        for(int i=0;i<NUM_DBS;i++) {
            idx[i] = i;
            r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
            r = dbs[i]->set_descriptor(dbs[i], 1, &desc);                                                         CKERR(r);
            dbs[i]->app_private = &idx[i];
            snprintf(name, sizeof(name), "db_%04x", i);
            r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        }
    }

    // read and verify all rows
    {
        if ( verbose ) {printf("checking");fflush(stdout);}
        check_results(env, dbs, NUM_DBS, NUM_ROWS);
        if ( verbose) {printf("\ndone\n");fflush(stdout);}
    }
    // close
    {
        for(int i=0;i<NUM_DBS;i++) {
            dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
            dbs[i] = NULL;
        }
    }
}

static void run_test(void) 
{
    int r;

    char *src_db_dir;
    if ( SRC_VERSION == 3 ) 
        src_db_dir = db_v3_dir;
    else if ( SRC_VERSION == 4 )
        src_db_dir = db_v4_dir;
    else {
        fprintf(stderr, "unsupported TokuDB version %d to upgrade\n", SRC_VERSION);
        assert(0);
    }
    
    {
	int len = 256;
	char syscmd[len];
	r = snprintf(syscmd, len, "rm -rf %s", env_dir);
	assert(r<len);
	r = system(syscmd);                                                                                   CKERR(r);

        r = snprintf(syscmd, len, "cp -r %s %s", src_db_dir, env_dir);
	assert(r<len);
	r = system(syscmd);                                                                                   CKERR(r);
    }

    generate_permute_tables();

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 60);                                                                CKERR(r);

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);

    // --------------------------
    upgrade_test_3(dbs);
    // --------------------------

    if (verbose >= 2)
	print_engine_status(env);
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);

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
	    fprintf(stderr, "Usage: -h -c -d <num_dbs> -r <num_rows> %s\n", cmd);
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
        } else if (strcmp(argv[0], "-V")==0) {
            argc--; argv++;
            SRC_VERSION = atoi(argv[0]);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

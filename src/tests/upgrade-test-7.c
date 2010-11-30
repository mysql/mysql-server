/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"


// Purpose of this test is to verify that an environment created by TokuDB 3.1.0
// is properly rejected by the upgrade logic of TokuDB 5.x and later.

#include "test.h"
#include "toku_pthread.h"
#include "toku_atomic.h"
#include <db.h>
#include <sys/stat.h>
#include "ydb-internal.h"

DB_ENV *env;

#if defined(__cilkplusplus) || defined (__cplusplus)
extern "C" {
#endif


char *free_me = NULL;
char *env_dir = ENVDIR; // the default env_dir.
char *tmp_subdir = "tmp.subdir";

#define OLDDATADIR "../../../../tokudb.data/"
    char *db_v3_dir        = OLDDATADIR "env_preload.3.1.0.simple.cleanshutdown";


static void setup(void) {
    int r;
    int len = 256;
    char syscmd[len];
    char * src_db_dir;

    src_db_dir = db_v3_dir;

    r = snprintf(syscmd, len, "rm -rf %s", env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                  
    CKERR(r);
    
    r = snprintf(syscmd, len, "cp -r %s %s", src_db_dir, env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                 
    CKERR(r);
}




static void run_test(void) 
{
    int r;

    setup();
    {
	char len = strlen(env_dir) + strlen(tmp_subdir) + 10;
	char tmpdir[len];
	r = snprintf(tmpdir, len, "%s/%s", env_dir, tmp_subdir);
	assert(r<len);
	r = db_env_create(&env, 0);                                                                           CKERR(r);
	//
	// NOTE: If tmp_dir is set, then attempt to open database created with 3.x will fail with error message:
	// Couldn't start tokudb because some other tokudb process is using the same directory [dir.upgrade-test-7.c.tdb/tmp.subdir] for [temp]
	//
	//	r = env->set_tmp_dir(env, tmp_subdir);                                                                CKERR(r);
	//
    }

    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            
    CKERR2(r, TOKUDB_DICTIONARY_TOO_OLD);

    r = env->close(env, 0);                                                                                   CKERR(r);
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
	    fprintf(stderr, "Usage: -h -v -q\n%s\n", cmd);
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

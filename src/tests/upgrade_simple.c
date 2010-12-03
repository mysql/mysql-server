/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id: env_startup.c 20778 2010-05-28 20:38:42Z yfogel $"

/* Purpose of this test is to verify simplest part of upgrade logic.
 * Start by creating two very simple 4.x environments,
 * one in each of two states:
 *  - after a clean shutdown
 *  - without a clean shutdown
 *
 * The two different environments will be used to exercise upgrade logic
 * for 5.x.
 *
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

#define OLDDATADIR "../../../../tokudb.data/"

static char *env_dir = ENVDIR; // the default env_dir.

static char * dir_v4_clean = "env_simple.4.1.1.cleanshutdown";
static char * dir_v4_dirty = "env_simple.4.1.1.dirtyshutdown";
static char * dir_v4_dirty_multilogfile = OLDDATADIR "env_preload.4.1.1.multilog.dirtyshutdown";


static void
setup (u_int32_t flags, BOOL clean, char * src_db_dir) {
    int r;
    int len = 256;
    char syscmd[len];

    if (env)
        test_shutdown();

    r = snprintf(syscmd, len, "rm -rf %s", env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                  
    CKERR(r);
    
    r = snprintf(syscmd, len, "cp -r %s %s", src_db_dir, env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                 
    CKERR(r);

    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, flags, mode); 
    if (clean)
	CKERR(r);
    else
	CKERR2(r, TOKUDB_UPGRADE_FAILURE);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}


static void
test_env_startup(void) {
    u_int32_t flags;
    
    flags = FLAGS_LOG;

    setup(flags, TRUE, dir_v4_clean);
    print_engine_status(env);
    test_shutdown();

    setup(flags, FALSE, dir_v4_dirty);
    if (verbose) {
	printf("\n\nEngine status after aborted env->open() will have some garbage values:\n");
    }
    print_engine_status(env);
    test_shutdown();

    setup(flags, FALSE, dir_v4_dirty_multilogfile);
    if (verbose) {
	printf("\n\nEngine status after aborted env->open() will have some garbage values:\n");
    }
    print_engine_status(env);
    test_shutdown();
}


int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    test_env_startup();
    return 0;
}

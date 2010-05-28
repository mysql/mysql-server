/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify correct behavior of
 * environment startup:
 *
 * All three of the following should exist or all three should not exist:
 *  - persistent environment
 *  - fileops directory
 *  - recovery log  (if DB_INIT_LOG)
 *
 * If all three are missing, env->open() should create a new environment.
 * If any one is present and any other is missing, env->open() should return ENOENT.
 *
 * TODO: experiment with DB_INIT_LOG off.
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

static void
setup (u_int32_t flags) {
    int r;
    if (env)
        test_shutdown();
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, flags, mode); 
    CKERR(r);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}


static void
reopen_env(u_int32_t flags, int expected_r) {
    int r;
    if (env)
        test_shutdown();
    r = db_env_create(&env, 0);                                           
    CKERR(r);
    r = env->open(env, ENVDIR, flags, mode);
    CKERR2(r, expected_r);
}

static void
delete_persistent(void) {
    char cmd[1024];
    sprintf(cmd, "rm -rf %s%s%s", ENVDIR, "/", "tokudb.environment");
    int r = system(cmd);
    CKERR(r);
}


static void
delete_directory(void) {
    char cmd[1024];
    sprintf(cmd, "rm -rf %s%s%s", ENVDIR, "/", "tokudb.directory");
    int r = system(cmd);
    CKERR(r);
}


static void
delete_log(void) {
    char cmd[1024];
    sprintf(cmd, "rm -rf %s%s%s", ENVDIR, "/", "*.tokulog*");
    int r = system(cmd);
    CKERR(r);
}


static void
create_env(u_int32_t flags) {
    setup(flags);                     // create new environment
    test_shutdown();
    reopen_env(flags, 0);             // reopen existing environment, should have log now
    test_shutdown();
}


static void
test_env_startup(int logging) {
    u_int32_t flags;
    
    if (logging)
	flags = FLAGS_LOG;
    else
	flags = FLAGS_NOLOG;

    create_env(flags);

    // delete persistent info and try to reopen
    delete_persistent();
    reopen_env(flags, ENOENT);

    // recreate, then try to open with missing fileops directory
    create_env(flags);
    delete_directory();
    reopen_env(flags, ENOENT);
    

    if (logging) {
	// recreate, then try to open with missing recovery log
	create_env(flags);
	delete_log();
	reopen_env(flags, ENOENT);

	
	// now try two missing items, if log can be present

	// log is only item present
	create_env(flags);
	delete_persistent();
	delete_directory();
	reopen_env(flags, ENOENT);

	// persistent env is only item present
	create_env(flags);
	delete_log();
	delete_directory();
	reopen_env(flags, ENOENT);
	
	// directory is only item present
	create_env(flags);
	delete_persistent();
	delete_log();
	reopen_env(flags, ENOENT);
    }

    test_shutdown();
}


int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    test_env_startup(0);  // transactionless env
    test_env_startup(1);  // with transactions and logging
    return 0;
}

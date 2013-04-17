#include "test.h"
#include <db.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>


DB_TXN *null_txn=0;

static void do_test1753 (int do_create_on_reopen) {

    if (IS_TDB==0 && DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<7 && do_create_on_reopen==0) {
	return; // do_create_on_reopen==0 segfaults in 4.6
    }

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    int r;

    // Create an empty file
    {
	DB_ENV *env;
	DB *db;
	
	const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_PRIVATE ;

	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);

	r = db_create(&db, env, 0);                                           CKERR(r);
	r = db->open(db, null_txn, "main", 0,     DB_BTREE, DB_CREATE, 0666); CKERR(r);

	r = db->close(db, 0);                                                 CKERR(r);
	r = env->close(env, 0);                                               CKERR(r);
    }
    // Now open the empty file and insert
    {
	DB_ENV *env;
	int envflags = DB_INIT_MPOOL| DB_THREAD |DB_PRIVATE;
	if (do_create_on_reopen) envflags |= DB_CREATE;
	
	r = db_env_create(&env, 0);                                           CKERR(r);
	env->set_errfile(env, 0);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
	if (do_create_on_reopen) CKERR(r);
        else CKERR2(r, ENOENT);
	r = env->close(env, 0);                                               CKERR(r);

    }
}

int test_main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    do_test1753(1);
    do_test1753(0);
    return 0;
}

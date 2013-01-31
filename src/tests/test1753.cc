/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"
#include <db.h>
#include <sys/stat.h>
#include <stdlib.h>



DB_TXN *null_txn=0;

static void do_test1753 (int do_create_on_reopen) {

    if (IS_TDB==0 && DB_VERSION_MAJOR==4 && DB_VERSION_MINOR<7 && do_create_on_reopen==0) {
	return; // do_create_on_reopen==0 segfaults in 4.6
    }

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    // Create an empty file
    {
	DB_ENV *env;
	DB *db;
	
	const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_PRIVATE ;

	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);

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
	r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
	if (do_create_on_reopen) CKERR(r);
        else CKERR2(r, ENOENT);
	r = env->close(env, 0);                                               CKERR(r);

    }
}

int test_main (int argc __attribute__((__unused__)), char * const argv[] __attribute__((__unused__))) {
    do_test1753(1);
    do_test1753(0);
    return 0;
}

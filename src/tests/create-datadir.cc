/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test data directories

#include <sys/stat.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

const char *namea="a.db";
const char *nameb="b.db";

static void run_test (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                          CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                       CKERR(r);

    DB *db;  
    r = db_create(&db, env, 0);                                                          CKERR(r);
    r = db->open(db, NULL, "a.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);      CKERR(r);
    r = db->close(db, 0);                                                                CKERR(r);

    r = db_create(&db, env, 0);                                                          CKERR(r);
    r = db->open(db, NULL, "bdir/b.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);
#if USE_TDB
        CKERR(r); //Success, so need a new handle
    r = db->close(db, 0);                                                                CKERR(r);
    r = db_create(&db, env, 0);                                                          CKERR(r);
#else
        assert(r != 0);
#endif
    char path[TOKU_PATH_MAX+1];
    r = toku_os_mkdir(toku_path_join(path, 2, TOKU_TEST_FILENAME, "bdir"), 0777); assert(r == 0);
    r = db->open(db, NULL, "bdir/b.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666); CKERR(r);
    r = db->close(db, 0);                                                                CKERR(r);

    r = env->close(env, 0);                                                              CKERR(r);

    r = toku_os_mkdir(toku_path_join(path, 2, TOKU_TEST_FILENAME, "cdir"), 0777); assert(r == 0);
    r = db_env_create(&env, 0);                                                          CKERR(r);
    r = env->set_data_dir(env, "cdir");                                                  CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                       CKERR(r);

    r = db_create(&db, env, 0);                                                          CKERR(r);
    r = db->open(db, NULL, "c.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);      CKERR(r);
    r = db->close(db, 0);                                                                CKERR(r);

#if 0
    // test fname with absolute path
    r = db_create(&db, env, 0);                                                          CKERR(r);
    r = db->open(db, NULL, "/tmp/d.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666); CKERR(r);
    r = db->close(db, 0);                                                                CKERR(r);
#endif

    r = env->close(env, 0);                                                              CKERR(r);
}

const char *cmd;

static void test_parse_args (int argc, char * const argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--test | --recover } \n", cmd);
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

int test_main (int argc, char * const argv[]) {
    test_parse_args(argc, argv);
    run_test();
    return 0;
}

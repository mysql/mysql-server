/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test delboth commit before checkpoint

#include "test.h"


const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
const char *namea="a.db";

static void
run_test (bool do_commit, bool do_abort) {
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    DB *dba;
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);

    // insert (i,i) pairs
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
    for (int i=0; i<256; i++) {
        unsigned char c = (unsigned char) i;
	DBT k = {.data=&c, .size=sizeof c};
	DBT v = {.data=&c, .size=sizeof c};
	r = dba->put(dba, txn, &k, &v, 0);                                              CKERR(r);
    }
    r = txn->commit(txn, 0);                                                            CKERR(r);

    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

    // delete (128,128)
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
    {
        unsigned char c = 128;
        DBT k = {.data=&c, .size=sizeof c};
        r = dba->del(dba, txn, &k, 0);                                                  CKERR(r);
    }

    if (do_commit) {
	r = txn->commit(txn, 0);                                                        CKERR(r);
    } else if (do_abort) {
        r = txn->abort(txn);                                                            CKERR(r);
        
        // force an fsync of the log
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        r = txn->commit(txn, DB_TXN_SYNC);                                              CKERR(r);
    }
    //printf("shutdown\n");
    toku_hard_crash_on_purpose();
}

static void
run_recover (bool UU(did_commit)) {
    DB_ENV *env;
    int r;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);

    // verify all but (128,128) exist
    DB *dba;
    r = db_create(&dba, env, 0);                                                            CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);        CKERR(r);
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 CKERR(r);
    DBC *ca;
    r = dba->cursor(dba, txn, &ca, 0);                                                      CKERR(r);
    int i;
    for (i=0; ; i++) {
        if (i == 128)
            continue;
        DBT k,v;
        dbt_init(&k, NULL, 0);
        dbt_init(&v, NULL, 0);
        r = ca->c_get(ca, &k, &v, DB_NEXT);
        if (r != 0)
            break;
        assert(k.size == 1 && v.size == 1);
        unsigned char kk, vv;
        memcpy(&kk, k.data, k.size); 
        memcpy(&vv, v.data, v.size);
        assert(kk == i);
        assert(vv == i);
    }
    assert(i == 256);

    r = ca->c_close(ca);                                                                    CKERR(r);

    r = txn->commit(txn, 0);                                                                CKERR(r);

    r = dba->close(dba, 0);                                                                 CKERR(r);

    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

static void
run_recover_only (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

static void
run_no_recover (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == DB_RUNRECOVERY);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

const char *cmd;

bool do_commit=false, do_abort=false, do_explicit_abort=false, do_recover_committed=false,  do_recover_aborted=false, do_recover_only=false, do_no_recover = false;

static void
x1_parse_args (int argc, char * const argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--commit")==0 || strcmp(argv[0], "--test") == 0) {
	    do_commit=true;
	} else if (strcmp(argv[0], "--abort")==0) {
	    do_abort=true;
	} else if (strcmp(argv[0], "--explicit-abort")==0) {
	    do_explicit_abort=true;
	} else if (strcmp(argv[0], "--recover-committed")==0 || strcmp(argv[0], "--recover") == 0) {
	    do_recover_committed=true;
	} else if (strcmp(argv[0], "--recover-aborted")==0) {
	    do_recover_aborted=true;
        } else if (strcmp(argv[0], "--recover-only") == 0) {
            do_recover_only=true;
        } else if (strcmp(argv[0], "--no-recover") == 0) {
            do_no_recover=true;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--commit | --abort | --explicit-abort | --recover-committed | --recover-aborted } \n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
    {
	int n_specified=0;
	if (do_commit)            n_specified++;
	if (do_abort)             n_specified++;
	if (do_explicit_abort)    n_specified++;
	if (do_recover_committed) n_specified++;
	if (do_recover_aborted)   n_specified++;
	if (do_recover_only)      n_specified++;
	if (do_no_recover)        n_specified++;
	if (n_specified>1) {
	    printf("Specify only one of --commit or --abort or --recover-committed or --recover-aborted\n");
	    resultcode=1;
	    goto do_usage;
	}
    }
}

int
test_main (int argc, char * const argv[])
{
    x1_parse_args(argc, argv);
    if (do_commit) {
	run_test (true, false);
    } else if (do_abort) {
	run_test (false, false);
    } else if (do_explicit_abort) {
        run_test(false, true);
    } else if (do_recover_committed) {
	run_recover(true);
    } else if (do_recover_aborted) {
	run_recover(false);
    } else if (do_recover_only) {
        run_recover_only();
    } else if (do_no_recover) {
        run_no_recover();
    } 
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}

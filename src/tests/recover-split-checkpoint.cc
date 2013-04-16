/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// force a checkpoint to span multiple tokulog files.  in other words, the begin checkpoint log entry and the
// end checkpoint log entry for the same checkpoint are in different log files.

#include <sys/stat.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

static void test_checkpoint_callback(void *extra) {
    int r;
    DB_ENV *env = (DB_ENV *) extra;

    // create and commit a bunch of transactions.  the last commit fsync's the log.  since the log is
    // really small, a new log file is created before the end checkpoint is logged. 
    int i;
    for (i=0; i<100; i++) {
        DB_TXN *txn = NULL;
        r = env->txn_begin(env, NULL, &txn, 0);                                        CKERR(r);
        r = txn->commit(txn, i == 99 ? DB_TXN_SYNC : 0);                               CKERR(r);
    }
}

static void test_checkpoint_callback2(void *extra) {
    (void) extra;
}

static void run_test (bool do_commit, bool do_abort) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                         CKERR(r);

    db_env_set_checkpoint_callback(test_checkpoint_callback, env);
    db_env_set_checkpoint_callback2(test_checkpoint_callback2, env);

    r = env->set_lg_max(env, 1024);                                                     CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);

    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

    if (do_commit) {
	r = txn->commit(txn, 0);                                                        CKERR(r);
    } else if (do_abort) {
        r = txn->abort(txn);                                                            CKERR(r);
        
        // force an fsync of the log
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }
    //printf("shutdown\n");
    toku_hard_crash_on_purpose();
}

static void run_recover (bool did_commit) {
    (void) did_commit;
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
}

static void run_recover_only (void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
}

static void run_no_recover (void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == DB_RUNRECOVERY);
    r = env->close(env, 0);                                                                 CKERR(r);
}

const char *cmd;


bool do_commit=false, do_abort=false, do_explicit_abort=false, do_recover_committed=false,  do_recover_aborted=false, do_recover_only=false, do_no_recover = false;

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

int test_main (int argc, char * const argv[]) {
    test_parse_args(argc, argv);
    if (do_commit) {
	run_test(true, false);
    } else if (do_abort) {
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
    return 0;
}

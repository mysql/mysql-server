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
    extra = extra;
}

static void run_test (BOOL do_commit, BOOL do_abort) {
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                         CKERR(r);

    db_env_set_checkpoint_callback(test_checkpoint_callback, env);
    db_env_set_checkpoint_callback2(test_checkpoint_callback2, env);

    r = env->set_lg_max(env, 1024);                                                     CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

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

static void run_recover (BOOL did_commit) {
    did_commit = did_commit;
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
}

static void run_recover_only (void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
}

static void run_no_recover (void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == DB_RUNRECOVERY);
    r = env->close(env, 0);                                                                 CKERR(r);
}

const char *cmd;


BOOL do_commit=FALSE, do_abort=FALSE, do_explicit_abort=FALSE, do_recover_committed=FALSE,  do_recover_aborted=FALSE, do_recover_only=FALSE, do_no_recover = FALSE;

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
	    do_commit=TRUE;
	} else if (strcmp(argv[0], "--abort")==0) {
	    do_abort=TRUE;
	} else if (strcmp(argv[0], "--explicit-abort")==0) {
	    do_explicit_abort=TRUE;
	} else if (strcmp(argv[0], "--recover-committed")==0 || strcmp(argv[0], "--recover") == 0) {
	    do_recover_committed=TRUE;
	} else if (strcmp(argv[0], "--recover-aborted")==0) {
	    do_recover_aborted=TRUE;
        } else if (strcmp(argv[0], "--recover-only") == 0) {
            do_recover_only=TRUE;
        } else if (strcmp(argv[0], "--no-recover") == 0) {
            do_no_recover=TRUE;
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
	run_test(TRUE, FALSE);
    } else if (do_abort) {
        run_test(FALSE, TRUE);
    } else if (do_recover_committed) {
        run_recover(TRUE);
    } else if (do_recover_aborted) {
        run_recover(FALSE);
    } else if (do_recover_only) {
        run_recover_only();
    } else if (do_no_recover) {
        run_no_recover();
    }
    return 0;
}

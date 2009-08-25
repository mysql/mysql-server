#include <sys/stat.h>
#include <sys/wait.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN;

char *namea="a.db";
char *nameb="b.db";

static void
run_test (void) {
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    DB *dba, *dbb;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);

    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);

    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
    {
	DBT a={.data="a", .size=2};
	DBT b={.data="b", .size=2};
	r = dba->put(dba, txn, &a, &b, DB_YESOVERWRITE);                                CKERR(r);
	r = env->txn_checkpoint(env, 0, 0, 0);                                          CKERR(r);
	r = dbb->put(dbb, txn, &b, &a, DB_YESOVERWRITE);                                CKERR(r);
    }

    r = txn->commit(txn, 0);                                                            CKERR(r);

    abort();
}

static void
run_recover (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);             CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

static void
run_no_recover (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);            CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

const char *cmd;

BOOL do_commit=FALSE, do_abort=FALSE, do_explicit_abort=FALSE, do_recover_committed=FALSE,  do_recover_aborted=FALSE, do_recover_only=FALSE, do_no_recover = FALSE;

static void test_parse_args (int argc, char *argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--commit")==0) {
	    do_commit=TRUE;
	} else if (strcmp(argv[0], "--abort")==0) {
	    do_abort=TRUE;
	} else if (strcmp(argv[0], "--explicit-abort")==0) {
	    do_explicit_abort=TRUE;
	} else if (strcmp(argv[0], "--recover-committed")==0) {
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

int
test_main (int argc, char *argv[]) {
    test_parse_args(argc, argv);
    if (do_commit) {
	run_test();
    } else if (do_recover_only) {
        run_recover();
    } else if (do_no_recover) {
        run_no_recover();
    } 
    return 0;
}

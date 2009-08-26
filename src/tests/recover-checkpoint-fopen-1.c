#include <sys/stat.h>
#include <sys/wait.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN |DB_RECOVER;
char *namea="a.db";
char *nameb="b.db";


static void run_test (BOOL do_commit, BOOL do_abort) {
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    DB *dba, *dbb;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);

    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
    {
	DBT a={.data="a", .size=2};
	DBT b={.data="b", .size=2};
	r = dba->put(dba, txn, &a, &b, 0);                                              CKERR(r);
	r = dbb->put(dbb, txn, &b, &a, 0);                                              CKERR(r);
    }
    //printf("opened\n");
    if (do_commit) {
	r = txn->commit(txn, 0);                                                        CKERR(r);
    } else if (do_abort) {
        r = txn->abort(txn);                                                            CKERR(r);
        
        // force an fsync of the log
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }
    //printf("shutdown\n");
    abort();
}

#if 0

static void run_recover (BOOL did_commit) {
    DB_ENV *env;
    DB *dba, *dbb;
    int r;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = db_create(&dba, env, 0);                                                            CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);        CKERR(r);
    r = db_create(&dbb, env, 0);                                                            CKERR(r);
    r = dba->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);        CKERR(r);
    DBT aa={.size=0}, ab={.size=0};
    DBT ba={.size=0}, bb={.size=0};
    DB_TXN *txn;
    DBC *ca,*cb;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 CKERR(r);
    r = dba->cursor(dba, txn, &ca, 0);                                                      CKERR(r);
    r = dbb->cursor(dbb, txn, &cb, 0);                                                      CKERR(r);
    int ra = ca->c_get(ca, &aa, &ab, DB_FIRST);                                             CKERR(r);
    int rb = cb->c_get(cb, &ba, &bb, DB_FIRST);                                             CKERR(r);
    if (did_commit) {
	assert(ra==0);
	assert(rb==0);
	// verify key-value pairs
	assert(aa.size==2);
	assert(ab.size==2);
	assert(ba.size==2);
	assert(bb.size==2);
	const char a[2] = "a";
	const char b[2] = "b";
        assert(memcmp(aa.data, &a, 2)==0);
        assert(memcmp(ab.data, &b, 2)==0);
        assert(memcmp(ab.data, &b, 2)==0);
        assert(memcmp(bb.data, &a, 2)==0);
	// make sure no other entries in DB
	assert(ca->c_get(ca, &aa, &ab, DB_NEXT) == DB_NOTFOUND);
	assert(cb->c_get(cb, &ba, &bb, DB_NEXT) == DB_NOTFOUND);
	fprintf(stderr, "Both verified. Yay!\n");
    } else {
	// It wasn't committed (it also wasn't aborted), but a checkpoint happened.
	assert(ra==DB_NOTFOUND);
	assert(rb==DB_NOTFOUND);
	fprintf(stderr, "Neither present. Yay!\n");
    }
    r = ca->c_close(ca);                                                                    CKERR(r);
    r = cb->c_close(cb);                                                                    CKERR(r);
    r = txn->commit(txn, 0);                                                                CKERR(r);
    r = dba->close(dba, 0);                                                                 CKERR(r);
    r = dbb->close(dbb, 0);                                                                 CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
}

#endif

static void run_recover_only (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
}

static void run_no_recover (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);            CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
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

int test_main (int argc, char *argv[]) {
    test_parse_args(argc, argv);
    if (do_commit) {
	run_test(TRUE, FALSE);
    } else if (do_abort) {
        run_test(FALSE, TRUE);
    } else if (do_recover_only) {
        run_recover_only();
    } else if (do_no_recover) {
        run_no_recover();
    }
    return 0;
}

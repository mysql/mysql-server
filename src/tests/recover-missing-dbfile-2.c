// verify that DB_RUNRECOVERY is returned when there is a missing db file

#include <sys/stat.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN;

#define NAMEA "a.db"
const char *namea=NAMEA;
#define NAMEB "b.db"
const char *nameb=NAMEB;

// needed to get .bdb versions to compile
#ifndef DB_CLOSE_DONT_TRIM_LOG
#define DB_CLOSE_DONT_TRIM_LOG 0
#endif

static void run_test (void) {
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;

    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    DB *dba;
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = dba->close(dba, 0); CKERR(r);

    DB *dbb;
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = dbb->close(dbb, 0); CKERR(r);

    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);

    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);    CKERR(r);

    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);    CKERR(r);
    
    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

    r = txn->commit(txn, 0);                                                            CKERR(r);

    abort();
}

static void run_recover (void) {
    DB_ENV *env;
    int r;

    r = rename(ENVDIR "/" NAMEB, ENVDIR "/" NAMEB ".save" ); printf("r=%d error=%d\n", r, errno); CKERR(r);

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == DB_RUNRECOVERY);

    r = rename(ENVDIR "/" NAMEB ".save" , ENVDIR "/" NAMEB); printf("r=%d error=%d\n", r, errno); CKERR(r);

    r = env->open(env, ENVDIR, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);             CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

static void run_no_recover (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);            CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

const char *cmd;

BOOL do_test=FALSE, do_recover=FALSE, do_recover_only=FALSE, do_no_recover = FALSE;

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
	} else if (strcmp(argv[0], "--test")==0) {
	    do_test=TRUE;
        } else if (strcmp(argv[0], "--recover") == 0) {
            do_recover=TRUE;
        } else if (strcmp(argv[0], "--recover-only") == 0) {
            do_recover_only=TRUE;
        } else if (strcmp(argv[0], "--no-recover") == 0) {
            do_no_recover=TRUE;
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

int test_main (int argc, char *argv[]) {
    test_parse_args(argc, argv);
    if (do_test) {
	run_test();
    } else if (do_recover) {
        run_recover();
    } else if (do_recover_only) {
        run_recover();
    } else if (do_no_recover) {
        run_no_recover();
    } 
    return 0;
}

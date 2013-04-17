// test data directories

#include <sys/stat.h>
#include <sys/wait.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN;

char *namea="a.db";
char *nameb="b.db";

static void run_test (void) {
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    DB *dba;
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, "a.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = dba->close(dba, 0); CKERR(r);

    DB *dbb;
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, "bdir/b.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666); assert(r != 0);
    r = toku_os_mkdir(ENVDIR "/bdir", 0777); assert(r == 0);
    r = dbb->open(dbb, NULL, "bdir/b.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666); CKERR(r);
    r = dbb->close(dbb, 0); CKERR(r);

    r = env->close(env, 0); CKERR(r);

    r = toku_os_mkdir(ENVDIR "/cdir", 0777); assert(r == 0);
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_data_dir(env, "cdir"); CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    DB *dbc;
    r = db_create(&dbc, env, 0);                                                        CKERR(r);
    r = dbc->open(dbc, NULL, "c.db", NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = dbc->close(dbc, 0); CKERR(r);

    r = env->close(env, 0); CKERR(r);
}

const char *cmd;

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
    run_test();
    return 0;
}

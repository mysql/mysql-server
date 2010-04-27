// verify that the table lock log entry is handled

#include <sys/stat.h>
#include "test.h"


const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
char *namea="a.db";

DB_ENV *env;
DB_TXN *tid;
DB     *db;
DBT key,data;

static void
do_x1_shutdown (void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    {
        DB_TXN *oldest;
        r=env->txn_begin(env, 0, &oldest, 0);
        CKERR(r);
    }

    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r=tid->commit(tid, 0);                                                     assert(r==0);

    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db->put(db, tid, dbt_init(&key, "a", 2), dbt_init(&data, "b", 2), 0);    assert(r==0);
    r=tid->commit(tid, 0);                                                     assert(r==0);

    r=db->close(db, 0);                                                        assert(r==0);

    //printf("shutdown\n");
    toku_hard_crash_on_purpose();
}

static void
do_x1_recover (BOOL UU(did_commit)) {
    int r;
    r=system("rm " ENVDIR "/*.tokudb"); CKERR(r);

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);

    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO);                       CKERR(r);
    r=db->get(db, tid, dbt_init(&key, "a", 2), dbt_init_malloc(&data), 0);     assert(r==0); 
    r=tid->commit(tid, 0);                                                     assert(r==0);
    toku_free(data.data);
    r=db->close(db, 0);                                                        CKERR(r);
    r=env->close(env, 0);                                                      CKERR(r);
}

BOOL do_commit=FALSE, do_recover_committed=FALSE;

static void
x1_parse_args (int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--test") == 0) {
	    do_commit=TRUE;
	} else if (strcmp(argv[0], "--recover") == 0) {
	    do_recover_committed=TRUE;
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
	if (do_recover_committed) n_specified++;
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
	do_x1_shutdown();
    } else if (do_recover_committed) {
	do_x1_recover(TRUE);
    } 
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}

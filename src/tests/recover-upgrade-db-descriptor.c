// verify that the comparison function get a valid db object pointer

#include <sys/stat.h>
#include "test.h"


char *descriptor_contents[] = {
    "Spoon full of sugar",
    "Bucket full of pants"
};

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
char *namea="a.db";

int verified = 0;
uint32_t forced_version = 2;

#if USE_TDB

static int my_compare(DB *UU(db), const DBT *a, const DBT *b) {
    assert(db);
    assert(db->descriptor);
    uint32_t version = db->descriptor->version;
    assert(version > 0);
    assert(version == forced_version);
    uint32_t which = version-1;
    size_t len = strlen(descriptor_contents[which])+1;

    assert(db->descriptor->dbt.size == len);
    assert(memcmp(db->descriptor->dbt.data, descriptor_contents[which], len) == 0);

    assert(a->size == b->size);
    verified = 1;
    return memcmp(a->data, b->data, a->size);
}   

#endif

static void
set_descriptor(DB* db, int which) {
#if USE_TDB
    DBT descriptor;
    size_t len = strlen(descriptor_contents[which])+1;
    dbt_init(&descriptor, descriptor_contents[which], len);
    int r = db->set_descriptor(db, which+1, &descriptor);  CKERR(r);
#endif
}

static void
do_x1_shutdown (BOOL do_commit, BOOL do_abort) {
    int r;
    r = system("rm -rf " ENVDIR); CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = toku_os_mkdir(ENVDIR"/data", S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    DB_ENV *env;
    DB *dba;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_data_dir(env, "data");                                                 CKERR(r);
#if USE_TDB
    r = env->set_default_bt_compare(env, my_compare);                                   CKERR(r);
#endif
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    r = db_create(&dba, env, 0);                                                        CKERR(r);
    set_descriptor(dba, 0);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = dba->close(dba, 0);                                                                 CKERR(r);

    r = db_create(&dba, env, 0);                                                        CKERR(r);
    set_descriptor(dba, 1);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);

    

    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
    {
	DBT a={.data="a", .size=2};
	DBT b={.data="b", .size=2};
	r = dba->put(dba, txn, &a, &b, 0);                                              CKERR(r);
	r = dba->put(dba, txn, &b, &a, 0);                                              CKERR(r);
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
    assert(verified);
    toku_hard_crash_on_purpose();
}

static void
do_x1_recover (BOOL did_commit) {
    DB_ENV *env;
    DB *dba;
    int r;
    r = system("rm -rf " ENVDIR"/data"); /* Delete dictionaries */                          CKERR(r);
    r = toku_os_mkdir(ENVDIR"/data", S_IRWXU+S_IRWXG+S_IRWXO);                              CKERR(r);
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->set_data_dir(env, "data");                                                     CKERR(r);
#if USE_TDB
    r = env->set_default_bt_compare(env, my_compare);                                       CKERR(r);
#endif
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r = db_create(&dba, env, 0);                                                            CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);        CKERR(r);
    DBT aa={.size=0}, ab={.size=0};
    DB_TXN *txn;
    DBC *ca;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 CKERR(r);
    r = dba->cursor(dba, txn, &ca, 0);                                                      CKERR(r);
    int ra = ca->c_get(ca, &aa, &ab, DB_FIRST);                                             CKERR(r);
    if (did_commit) {
	assert(ra==0);
	// verify key-value pairs
	assert(aa.size==2);
	assert(ab.size==2);
	const char a[2] = "a";
	const char b[2] = "b";
        assert(memcmp(aa.data, &a, 2)==0);
        assert(memcmp(ab.data, &b, 2)==0);
        assert(memcmp(ab.data, &b, 2)==0);
	assert(ca->c_get(ca, &aa, &ab, DB_NEXT) == 0);
        assert(aa.size == 2 && ab.size == 2 && memcmp(aa.data, b, 2) == 0 && memcmp(ab.data, a, 2) == 0);
	// make sure no other entries in DB
	assert(ca->c_get(ca, &aa, &ab, DB_NEXT) == DB_NOTFOUND);
    } else {
	// It wasn't committed (it also wasn't aborted), but a checkpoint happened.
	assert(ra==DB_NOTFOUND);
    }
    r = ca->c_close(ca);                                                                    CKERR(r);
    r = txn->commit(txn, 0);                                                                CKERR(r);
    r = dba->close(dba, 0);                                                                 CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    assert(verified);
    exit(0);
}

static void
do_x1_recover_only (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

static void
do_x1_no_recover (void) {
    DB_ENV *env;
    int r;

    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == DB_RUNRECOVERY);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

const char *cmd;

#if 0

static void
do_test_internal (BOOL commit)
{
    pid_t pid;
    if (0 == (pid=fork())) {
	int r=execl(cmd, verbose ? "-v" : "-q", commit ? "--commit" : "--abort", NULL);
	assert(r==-1);
	printf("execl failed: %d (%s)\n", errno, strerror(errno));
	assert(0);
    }
    {
	int r;
	int status;
	r = waitpid(pid, &status, 0);
	//printf("signaled=%d sig=%d\n", WIFSIGNALED(status), WTERMSIG(status));
	assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    }
    // Now find out what happend
    
    if (0 == (pid = fork())) {
	int r=execl(cmd, verbose ? "-v" : "-q", commit ? "--recover-committed" : "--recover-aborted", NULL);
	assert(r==-1);
	printf("execl failed: %d (%s)\n", errno, strerror(errno));
	assert(0);
    }
    {
	int r;
	int status;
	r = waitpid(pid, &status, 0);
	//printf("recovery exited=%d\n", WIFEXITED(status));
	assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
}

static void
do_test (void) {
    do_test_internal(TRUE);
    do_test_internal(FALSE);
}

#endif


BOOL do_commit=FALSE, do_abort=FALSE, do_explicit_abort=FALSE, do_recover_committed=FALSE,  do_recover_aborted=FALSE, do_recover_only=FALSE, do_no_recover = FALSE;

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

int
test_main (int argc, char * const argv[])
{
    x1_parse_args(argc, argv);
    if (do_commit) {
	do_x1_shutdown (TRUE, FALSE);
    } else if (do_abort) {
	do_x1_shutdown (FALSE, FALSE);
    } else if (do_explicit_abort) {
        do_x1_shutdown(FALSE, TRUE);
    } else if (do_recover_committed) {
	do_x1_recover(TRUE);
    } else if (do_recover_aborted) {
	do_x1_recover(FALSE);
    } else if (do_recover_only) {
        do_x1_recover_only();
    } else if (do_no_recover) {
        do_x1_no_recover();
    } 
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}

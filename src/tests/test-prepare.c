#include "test.h"
#include <sys/wait.h>

#define ENVDIR2 ENVDIR "2"

static void clean_env (const char *envdir) {
    const int len = strlen(envdir)+100;
    char cmd[len];
    snprintf(cmd, len, "rm -rf %s", envdir);
    system(cmd);
    CKERR(toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void setup_env (DB_ENV **envp, const char *envdir) {
    CHK(db_env_create(envp, 0));
    (*envp)->set_errfile(*envp, stderr);
#ifdef TOKUDB
    CHK((*envp)->set_redzone(*envp, 0));
#endif
    CHK((*envp)->open(*envp, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void setup_env_and_prepare (DB_ENV **envp, const char *envdir, bool commit) {
    DB *db;
    DB_TXN *txn;
    clean_env(envdir);
    setup_env(envp, envdir);
    CKERR(db_create(&db, *envp, 0));
    CKERR(db->open(db, NULL, "foo.db", 0, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO));
    CKERR((*envp)->txn_begin(*envp, 0, &txn, 0));
    DBT key={.size=4, .data="foo"};
    CKERR(db->put(db, txn, &key, &key, 0));
    CHK(db->close(db, 0));
    u_int8_t gid[DB_GID_SIZE];
    memset(gid, 0, DB_GID_SIZE);
    gid[0]=42;
    CKERR(txn->prepare(txn, gid));
    if (commit)
	CKERR(txn->commit(txn, 0));
}

static void test1 (void) {
    pid_t pid;
    bool do_fork = true;
    if (!do_fork || 0==(pid=fork())) {
	DB_ENV *env;
	setup_env_and_prepare(&env, ENVDIR, false);
	{
	    DB_PREPLIST l[1];
	    long count=-1;
	    CKERR(env->txn_recover(env, l, 1, &count, DB_FIRST));
	    printf("%s:%d count=%ld\n", __FILE__, __LINE__, count);
	    assert(count==1);
	    assert(l[0].gid[0]==42);
	}
	exit(0);
    }
    int status;
    if (do_fork) {
	pid_t pid2 = wait(&status);
	assert(pid2==pid);
    }
    
    DB_ENV *env2;
    setup_env_and_prepare(&env2, ENVDIR2, true);

    // Now we can look at env2 in the debugger to see if we managed to make it the same


    DB_ENV *env;
    setup_env(&env, ENVDIR);
    {
	DB_PREPLIST l[1];
	long count=-1;
	int r = env->txn_recover(env, l, 1, &count, DB_FIRST);
	printf("r=%d count=%ld\n", r, count);
	assert(count==1);
	assert(l[0].gid[0]==42);
	for (int i=1; i<DB_GID_SIZE; i++) {
	    assert(l[0].gid[i]==0);
	}
	CHK(l->txn->commit(l->txn, 0));
    }
    CHK(env2->close(env2, 0));
    CHK(env ->close(env,  0));
}

int test_main (int argc, char *const argv[]) {
    default_parse_args(argc, argv);
    // first test: open an environment, a db, a txn, and do a prepare.   Then do txn_prepare (without even closing the environment).
    test1();
    

    // second test: poen environment, a db, a txn, prepare, close the environment.  Then reopen and do txn_prepare.

    // third test: make sure there is an fsync on txn_prepare, but not on the following commit.

    
    // Then close the environment Find out what BDB does when ask for the txn prepares.
    // Other tests: read prepared txns, 1 at a time. Then close it and read them again.
    return 0;
}

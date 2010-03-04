/* Transaction consistency:  
 *  fork a process:
 *   Open two tables, A and B
 *   begin transaction U
 *   begin transaction V
 *   store U.A into A using U
 *   store V.B into B using V
 *   checkpoint
 *   store U.C into A using U
 *   store V.D into B using V
 *   commit U
 *   maybe commit V
 *   abort the process abruptly
 *  wait for the process to finish
 *   open the environment doing recovery
 *   check to see if both rows are present in A and maybe present in B
 */
#include <sys/stat.h>
#include "test.h"


const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
char *namea="a.db";
char *nameb="b.db";

static void
put (DB_TXN *txn, DB *db, char *key, char *data) {
    DBT k = {.data = key,  .size=1+strlen(key) };
    DBT d = {.data = data, .size=1+strlen(data)};
    int r = db->put(db, txn, &k, &d, 0);
    CKERR(r);
}
   
static void
do_x2_shutdown (BOOL do_commit) {
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    DB *dba, *dbb; // Use two DBs so that BDB doesn't get a lock conflict
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dba->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    DB_TXN *txnU, *txnV;
    r = env->txn_begin(env, NULL, &txnU, 0);                                            CKERR(r);
    r = env->txn_begin(env, NULL, &txnV, 0);                                            CKERR(r);
    put(txnU, dba, "u.a", "u.a.data");
    put(txnV, dbb, "v.b", "v.b.data");
    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);
    put(txnU, dba, "u.c", "u.c.data");
    put(txnV, dbb, "v.d", "v.d.data");
    r = txnU->commit(txnU, 0);                                                          CKERR(r);
    if (do_commit) {
	r = txnV->commit(txnV, 0);                                                      CKERR(r);
    }
    toku_hard_crash_on_purpose();
}

static void
checkcurs (DBC *curs, int cursflags, char *key, char *val, BOOL expect_it) {
    DBT k={.size=0}, v={.size=0};
    int r = curs->c_get(curs, &k, &v, cursflags);
    if (expect_it) {
	assert(r==0);
	printf("Got %s expected %s\n", (char*)k.data, key);
	assert(strcmp(k.data, key)==0);
	assert(strcmp(v.data, val)==0);
    } else {
	printf("Expected nothing, got r=%d\n", r);
	assert(r!=0);
    }
}

static void
do_x2_recover (BOOL did_commit) {
    DB_ENV *env;
    int r;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 CKERR(r);
    {
	DB *dba;
	r = db_create(&dba, env, 0);                                                        CKERR(r);
	r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
	DBC *c;
	r = dba->cursor(dba, txn, &c, 0);                                                   CKERR(r);
	checkcurs(c, DB_FIRST, "u.a", "u.a.data", TRUE);
	checkcurs(c, DB_NEXT,  "u.c", "u.c.data", TRUE);
	checkcurs(c, DB_NEXT,  NULL,  NULL,       FALSE);
	r = c->c_close(c);                                                                  CKERR(r);	
	r = dba->close(dba, 0);                                                             CKERR(r);
    }
    {
	DB *dbb;
	r = db_create(&dbb, env, 0);                                                        CKERR(r);
	r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
	DBC *c;
	r = dbb->cursor(dbb, txn, &c, 0);                                                   CKERR(r);
	checkcurs(c, DB_FIRST, "v.b", "v.b.data", did_commit);
	checkcurs(c, DB_NEXT,  "v.d", "v.d.data", did_commit);
	checkcurs(c, DB_NEXT,  NULL,  NULL,       FALSE);
	r = c->c_close(c);                                                                  CKERR(r);	
	r = dbb->close(dbb, 0);                                                             CKERR(r);
    }
 
    r = txn->commit(txn, 0);                                                                CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

const char *cmd;

#if 0

static void
do_test_internal (BOOL commit) {
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

BOOL do_commit=FALSE, do_abort=FALSE, do_recover_committed=FALSE,  do_recover_aborted=FALSE;

static void
x2_parse_args (int argc, char * const argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0],"--abort")==0) {
	    do_abort=1;
	} else if (strcmp(argv[0],"--commit")==0 || strcmp(argv[0], "--test") == 0) {
	    do_commit=1;
	} else if (strcmp(argv[0],"--recover-committed")==0 || strcmp(argv[0], "--recover") == 0) {
	    do_recover_committed=1;
	} else if (strcmp(argv[0],"--recover-aborted")==0) {
	    do_recover_aborted=1;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--abort | --commit | --recover-committed | --recover-aborted } \n", cmd);
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
	if (do_recover_committed) n_specified++;
	if (do_recover_aborted)   n_specified++;
	if (n_specified>1) {
	    printf("Specify only one of --commit or --abort or --recover-committed or --recover-aborted\n");
	    resultcode=1;
	    goto do_usage;
	}
    }
}

int
test_main (int argc, char * const argv[]) {
    x2_parse_args(argc, argv);
    if (do_commit) {
	do_x2_shutdown (TRUE);
    } else if (do_abort) {
	do_x2_shutdown (FALSE);
    } else if (do_recover_committed) {
	do_x2_recover(TRUE);
    } else if (do_recover_aborted) {
	do_x2_recover(FALSE);
    } 
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}

// verify that the table lock log entry is handled

#include <sys/stat.h>
#include "test.h"


const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;
DB_TXN *tid;
DB     *db;
DBT key,data;
int i;
enum {N=10000};
char *keys[N];
char *vals[N];

static void
do_x1_shutdown (void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                                                     assert(r==0);

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
    {
        DB_LOADER *loader;
        DB *dbs[1] = {db};
        uint32_t db_flags[1]  = {DB_NOOVERWRITE};
        uint32_t dbt_flags[1] = {0};
        uint32_t loader_flags = 0;

        r = env->create_loader(env, tid, &loader, NULL, 1, dbs, db_flags, dbt_flags, loader_flags);
        CKERR(r);
        r = loader->set_error_callback(loader, NULL, NULL);
        CKERR(r);
        r = loader->set_poll_function(loader, NULL, NULL);
        CKERR(r);
        // close the loader
        r = loader->close(loader);
        CKERR(r);
    }
    for (i=0; i<N; i++) {
	r=db->put(db, tid, dbt_init(&key, keys[i], strlen(keys[i])+1), dbt_init(&data, vals[i], strlen(vals[i])+1), 0);    assert(r==0);
	if (i%500==499) {
	    r=tid->commit(tid, 0);                                                     assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	}
    }
    r=tid->commit(tid, 0);                                                     assert(r==0);

    //leave db open (prevent local checkpoint)

    //printf("shutdown\n");
    toku_hard_crash_on_purpose();
}

static void
do_x1_recover (BOOL UU(did_commit)) {
    int r;
    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO);                       CKERR(r);
    for (i=0; i<N; i++) {
	r=db->get(db, tid, dbt_init(&key, keys[i], 1+strlen(keys[i])), dbt_init_malloc(&data), 0);     assert(r==0);
	assert(strcmp(data.data, vals[i])==0);
	toku_free(data.data);
	data.data=0;
	if (i%500==499) {
	    r=tid->commit(tid, 0);                                                     assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	}
    }
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
    srandom(0xDEADBEEF);
    for (i=0; i<N; i++) {
	char ks[100];  snprintf(ks, sizeof(ks), "k%09ld.%d", random(), i);
	char vs[1000]; snprintf(vs, sizeof(vs), "v%d.%0*d", i, (int)(sizeof(vs)-100), i);
	keys[i]=toku_strdup(ks);
	vals[i]=toku_strdup(vs);
    }
    x1_parse_args(argc, argv);
    if (do_commit) {
	do_x1_shutdown();
    } else if (do_recover_committed) {
	do_x1_recover(TRUE);
    } 
    for (i=0; i<N; i++) {
        toku_free(keys[i]);
        toku_free(vals[i]);
    }
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}

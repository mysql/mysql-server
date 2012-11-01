/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "$Id$"


#include "test.h"
#include "toku_pthread.h"

static int my_compare (DB *db, const DBT *a, const DBT *b) {
    assert(db);
    assert(db->cmp_descriptor);
    assert(db->cmp_descriptor->dbt.size >= 3);
    char *CAST_FROM_VOIDP(data, db->cmp_descriptor->dbt.data);
    assert(data[0]=='f');
    assert(data[1]=='o');
    assert(data[2]=='o');
    if (verbose) printf("compare descriptor=%s\n", data);
    sched_yield();
    return uint_dbt_cmp(db, a, b);
}

DB_ENV *env;
DB     *db;
const char   *env_dir = ENVDIR;
volatile int done = 0;

static void *startA (void *ignore __attribute__((__unused__))) {
    for (int i=0;i<999; i++) {
	DBT k,v;
	int a = (random()<<16) + i;
	dbt_init(&k, &a, sizeof(a));
	dbt_init(&v, &a, sizeof(a));
	DB_TXN *txn;
    again:
	{ int chk_r = env->txn_begin(env, NULL, &txn, DB_TXN_NOSYNC); CKERR(chk_r); }
	{
	    int r = db->put(db, txn, &k, &v, 0);
	    if (r==DB_LOCK_NOTGRANTED) {
		if (verbose) printf("lock not granted on %d\n", i);
		{ int chk_r = txn->abort(txn); CKERR(chk_r); }
		goto again;
	    }
	    assert(r==0);
	}
	{ int chk_r = txn->commit(txn, 0); CKERR(chk_r); }
    }
    int r __attribute__((__unused__)) = __sync_fetch_and_add(&done, 1);
    return NULL;
}
static void change_descriptor (DB_TXN *txn, int i) {
    DBT desc;
    char foo[100];
    snprintf(foo, 99, "foo%d", i);
    dbt_init(&desc, foo, 1+strlen(foo));
    int r;
    if (verbose) printf("trying to change to %s\n", foo);
    while ((r=db->change_descriptor(db, txn, &desc, 0))) {
	if (verbose) printf("Change failed r=%d, try again\n", r);
    }
    if (verbose) printf("ok\n");
}
static void startB (void) {
    for (int i=0; !done; i++) {
	IN_TXN_COMMIT(env, NULL, txn, 0,
		      change_descriptor(txn, i));
	sched_yield();
    }
}

static void my_parse_args (int argc, char * const argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[1],"--envdir")==0) {
	    assert(argc>2);
	    env_dir = argv[2];
	    argc--;
	    argv++;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q] [-h] [--envdir <envdir>]\n", argv0);
	    exit(resultcode);
	} else {
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

int test_main(int argc, char * const argv[]) {
    my_parse_args(argc, argv);

    db_env_set_num_bucket_mutexes(32);
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    { int chk_r = env->set_redzone(env, 0); CKERR(chk_r); }
    { int chk_r = env->set_default_bt_compare(env, my_compare); CKERR(chk_r); }
    {
	const int size = 10+strlen(env_dir);
	char cmd[size];
	snprintf(cmd, size, "rm -rf %s", env_dir);
	int r = system(cmd);
        CKERR(r);
    }
    { int chk_r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
    { int chk_r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }

    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->set_pagesize(db, 1024); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    IN_TXN_COMMIT(env, NULL, txn, 0,
                  { int chk_r = db->change_descriptor(db, txn, &desc, DB_UPDATE_CMP_DESCRIPTOR); CKERR(chk_r); });
    pthread_t thd;
    { int chk_r = toku_pthread_create(&thd, NULL, startA, NULL); CKERR(chk_r); }

    startB();

    void *retval;
    { int chk_r = toku_pthread_join(thd, &retval); CKERR(chk_r); }
    assert(retval==NULL);

    { int chk_r = db->close(db, 0); CKERR(chk_r); }


    { int chk_r = env->close(env, 0); CKERR(chk_r); }

    return 0;
}

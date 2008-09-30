/* Test to see if a big nested transaction (so big that it's rollbacks spill into a file)
 * can commit properly. 
 *  Four Tests:
 *     big child aborts, parent aborts
 *     big child aborts, parent commits
 *     big child commits, parent aborts  (This test)
 *     big child commits, parent commits
 */

#include <db.h>
#include <sys/stat.h>
#include "test.h"

int N = 50000;

static DB_ENV *env;
static DB *db;
static DB_TXN *xchild, *xparent;

static void insert (int i) {
    char hello[30], there[30];
    DBT key,data;
    if (verbose) printf("Insert %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", i);
    int r = db->put(db, xchild,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    dbt_init(&data, there, strlen(there)+1),
		    DB_YESOVERWRITE);
    CKERR(r);
}

static void lookup (int i, int expect, int expectj) {
    char hello[30], there[30];
    DBT key,data;
    snprintf(hello, sizeof(hello), "hello%d", i);
    memset(&data, 0, sizeof(data));
    if (verbose) printf("Looking up %d (expecting %s)\n", i, expect==0 ? "to find" : "not to find");
    int r = db->get(db, xchild,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    &data,
		    0);
    assert(expect==r);
    if (expect==0) {
	CKERR(r);
	snprintf(there, sizeof(there), "there%d", expectj);
	assert(data.size==strlen(there)+1);
	assert(strcmp(data.data, there)==0);
    }
}

static void
test_commit_abort (void) {
    int i, r;
    r=env->txn_begin(env, 0, &xparent, 0);  CKERR(r);
    r=env->txn_begin(env, xparent, &xchild, 0); CKERR(r);
    for (i=0; i<N; i++) {
	insert(i);
    }
    r=xchild->commit(xchild, 0); CKERR(r);
    r=env->txn_begin(env, xparent, &xchild, 0); CKERR(r);
    for (i=0; i<N; i++) {
	lookup(i, 0, i);
    }
    r=xchild->commit(xchild, 0); CKERR(r);
    r=xparent->abort(xparent); CKERR(r);
    r=env->txn_begin(env, 0, &xchild, 0); CKERR(r);
    for (i=0; i<N; i++) {
	lookup(i, DB_NOTFOUND, 0);
    }
    r=xchild->commit(xchild, 0); CKERR(r);
}

static void
setup (void) {
    DB_TXN *txn;
    int r;
    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       CKERR(r);

    r=db_env_create(&env, 0); CKERR(r);
    r=env->set_lk_max_locks(env, N); CKERR(r);
#ifndef TOKUDB
    r=env->set_lk_max_objects(env, N); CKERR(r);
#endif
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->set_flags(db, DB_DUPSORT);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);
}

static void
shutdown (void) {
    int r;
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    setup();
    test_commit_abort();
    shutdown();
    return 0;
}

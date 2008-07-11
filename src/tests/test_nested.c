/* Nested transactions. */

#include <db.h>
#include <sys/stat.h>
#include "test.h"

static DB_ENV *env;
static DB *db;

static void insert (int i, DB_TXN *x) {
    char hello[30], there[30];
    DBT key,data;
    if (verbose) printf("Insert %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", i);
    int r = db->put(db, x,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    dbt_init(&data, there, strlen(there)+1),
		    0);
    CKERR(r);
}

static void delete (int i, DB_TXN *x) {
    char hello[30];
    DBT key;
    if (verbose) printf("delete %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    int r = db->del(db, x,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    0);
    CKERR(r);
}

static void lookup (int i, DB_TXN *x, int expect) {
    char hello[30], there[30];
    DBT key,data;
    snprintf(hello, sizeof(hello), "hello%d", i);
    memset(&data, 0, sizeof(data));
    if (verbose) printf("Looking up %d (expecting %s)\n", i, expect==0 ? "to find" : "not to find");
    int r = db->get(db, x,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    &data,
		    0);
    assert(expect==r);
    if (expect==0) {
	CKERR(r);
	snprintf(there, sizeof(there), "there%d", i);
	assert(data.size==strlen(there)+1);
	assert(strcmp(data.data, there)==0);
    }
}

static DB_TXN *txn, *txn2;

void test_nested (void) {
    int r;
    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       assert(r==0);

    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    insert(0, txn);

    insert(1, txn);
    insert(2, txn);
    insert(3, txn);
    lookup(0, txn, 0);
    lookup(1, txn, 0);
    lookup(2, txn, 0);
    lookup(3, txn, 0);
    r=txn->commit(txn, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    delete(0, txn);
    delete(3, txn);
    r=env->txn_begin(env, txn, &txn2, 0); CKERR(r);
    delete(1, txn2);                     CKERR(r);
    lookup(3, txn2, DB_NOTFOUND);
    insert(3, txn2);
    lookup(3, txn2, 0);
    r=txn2->commit(txn2, 0); CKERR(r);
    lookup(0, txn, DB_NOTFOUND);
    lookup(1, txn, DB_NOTFOUND);
    lookup(2, txn, 0);
    lookup(3, txn, 0);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    lookup(0, txn, DB_NOTFOUND);
    lookup(1, txn, DB_NOTFOUND);
    lookup(2, txn, 0);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(4, txn);
    r=txn->commit(txn, 0); CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    r=env->txn_begin(env, txn, &txn2, 0); CKERR(r);
    delete(4, txn2);
    r=txn->commit(txn2, 0); CKERR(r);
    lookup(4, txn, DB_NOTFOUND);
    insert(4, txn);
    r=txn->commit(txn, 0); CKERR(r);
    lookup(4, 0, 0);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(5, txn);
    r=env->txn_begin(env, txn, &txn2, 0); CKERR(r);
    lookup(5, txn2, 0);
    insert(5, txn2);
    lookup(5, txn2, 0);
    r=txn->commit(txn2, 0); CKERR(r);
    lookup(5, txn, 0);
    r=env->txn_begin(env, txn, &txn2, 0); CKERR(r);
    lookup(5, txn2, 0);
    delete(5, txn2);
    r=txn->commit(txn2, 0); CKERR(r);
    lookup(5, txn, DB_NOTFOUND);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(6, txn);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    insert(6, txn);
    r=env->txn_begin(env, txn, &txn2, 0); CKERR(r);
    delete(6, txn2);
    r=txn->commit(txn2, 0); CKERR(r);
    r=txn->commit(txn, 0); CKERR(r);

    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    test_nested();
    return 0;
}

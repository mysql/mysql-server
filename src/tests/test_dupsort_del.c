/* Test to see if delete works right with dupsort.
 * The issue is that things might look OK before the commit, but bad after the commit.
 */

#include <stdint.h>
#include <db.h>
#include <sys/stat.h>
#include "test.h"

static DB_ENV *env;
static DB *db;
static DB_TXN *txn;

#ifndef TOKUDB
#define DB_YESOVERWRITE 0
#endif

static void insert (int i, int j) {
    char hello[30], there[30];
    DBT key,data;
    if (verbose) printf("Insert %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", j);
    int r = db->put(db, txn,
		    dbt_init(&key,  hello, strlen(hello)+1),
		    dbt_init(&data, there, strlen(there)+1),
		    DB_YESOVERWRITE);
    CKERR(r);
}

static void delete (int i, int j) {
    char hello[30], there[30];
    DBC *dbc;
    DBT key, val;
    if (verbose) printf("delete %d\n", i);
    snprintf(hello, sizeof(hello), "hello%d", i);
    snprintf(there, sizeof(there), "there%d", j);
    int r = db->cursor(db, txn, &dbc, 0);
    CKERR(r);
    r = dbc->c_get(dbc,
		   dbt_init(&key, hello, strlen(hello)+1),
		   dbt_init(&val, there, strlen(there)+1),
		   DB_GET_BOTH);
    CKERR(r);
    r = dbc->c_del(dbc, 0);
    CKERR(r);
    r = dbc->c_close(dbc);
    CKERR(r);
}

static void lookup (int i, int expect, int expectj) {
    char hello[30], there[30];
    DBT key,data;
    snprintf(hello, sizeof(hello), "hello%d", i);
    memset(&data, 0, sizeof(data));
    if (verbose) printf("Looking up %d (expecting %s)\n", i, expect==0 ? "to find" : "not to find");
    int r = db->get(db, txn,
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
test_dupsort_del (void) {
    int r;
    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       assert(r==0);

    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->set_flags(db, DB_DUPSORT);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    insert(0, 0);
    insert(0, 1);
    r=txn->commit(txn, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    lookup(0, 0, 0);
    delete(0, 0);
    lookup(0, 0, 1);
    r=txn->commit(txn, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0);    CKERR(r);
    lookup(0, 0, 1);
    r=txn->commit(txn, 0); CKERR(r);

    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    test_dupsort_del();
    return 0;
}

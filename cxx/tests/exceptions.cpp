#include <assert.h>
#include <db_cxx.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define TC(expr, expect) ({        \
  try {                            \
    expr;                          \
    assert(expect==0); 	           \
  } catch (DbException e) {        \
    if (e.get_errno()!=expect) fprintf(stderr, "err=%d %s\n", e.get_errno(), db_strerror(e.get_errno())); \
    assert(e.get_errno()==expect); \
  }                                \
})

void test_env_exceptions (void) {
    {
	DbEnv env(0);
	TC(env.open("no.such.dir", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE, 0777),        ENOENT);
    }
    {
	DbEnv env(0);
	TC(env.open("no.such.dir", -1, 0777),                                            EINVAL);
    }
    {
	DbEnv env(0);
	TC(env.open(".", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE, 0777),                  0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    EINVAL); // not configured for transactions
    }
    {
	DbEnv env(0);
	TC(env.open(".", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE | DB_INIT_LOG, 0777),                  0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    0);
	TC(txn->commit(0),                                                               0); 
	delete txn;
    }

    {
	DbEnv env(0);
	TC(env.open(".", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE | DB_INIT_LOG, 0777),    0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    0);
	TC(txn->commit(-1),                                                              EINVAL);
	delete txn;
    }
}


void test_db_exceptions (void) {
    DbEnv env(0);
    TC(env.open(".", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE , 0777),    0);
    TC( ({ Db db(&env, -1); assert(0); }),   EINVAL); // Create with flags=-1 should do an EINVAL
    Db db(&env, 0);
    DB *dbdb=db.get_DB();
    assert(dbdb!=0);
    assert(dbdb==db.get_const_DB());
    assert(&db==Db::get_const_Db(dbdb));
    unlink("foo.db");
    TC(db.open(0, "foo.db", 0, DB_BTREE, DB_CREATE, 0777), 0);
    TC(db.open(0, "foo.db", 0, DB_BTREE, DB_CREATE, 0777), EINVAL); // it was already open
    {
	Db db2(&env, 0);
	TC(db2.open(0, "foo2.db", 0, DB_BTREE, 0, 0777), ENOENT); // it doesn't exist
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, "foo.db", 0, DB_BTREE, 0, 0777), 0); // it does exist
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, "foo.db", 0, DB_BTREE, -1, 0777), EINVAL); // bad flags
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, "foo.db", 0, (DBTYPE)-1, 0, 0777), EINVAL); // bad type
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, "foo.db", "sub.db", DB_BTREE, DB_CREATE, 0777), EINVAL); // sub DB cannot exist
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, "foo.db", "sub.db", DB_BTREE, 0, 0777), EINVAL); // sub DB cannot exist withou DB_CREATE
    }
    {
	Dbc *curs;
	TC(db.cursor(0, &curs, -1),  EINVAL);
    }
    {
	Dbc *curs;
	TC(db.cursor(0, &curs, 0),  0);
	Dbt key,val;
	TC(curs->get(&key, &val, DB_FIRST), DB_NOTFOUND);
	TC(curs->get(&key, &val, -1), EINVAL); // bad flags
	curs->close(); // no deleting cursors.
    }
    {
	Dbt key,val;
	TC(db.del(0, &key, -1), EINVAL);
	TC(db.get(0, &key, &val, -1), EINVAL);
	TC(db.put(0, &key, &val, -1), EINVAL);
    }
    {
	Dbt key((char*)"hello", 6);
	Dbt val((char*)"there", 6);
	Dbt valget;
	TC(db.put(0, &key, &val, 0), 0);
	TC(db.get(0, &key, &valget, 0), 0);
	assert(strcmp((const char*)(valget.get_data()), "there")==0);
    }
}
	

void test_dbc_exceptions () {
    DbEnv env(0);
    TC(env.open(".", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE , 0777),    0);
    Db db(&env, 0);
    unlink("foo.db");
    TC(db.open(0, "foo.db", 0, DB_BTREE, DB_CREATE, 0777), 0);
    for (int k = 1; k<4; k++) {
        Dbt key(&k, sizeof k);
        Dbt val(&k, sizeof k);
        TC(db.put(0, &key, &val, 0), 0);
    }
    Dbc *curs;
    TC(db.cursor(0, &curs, 0),  0);
    Dbt key; key.set_flags(DB_DBT_MALLOC);
    Dbt val; val.set_flags(DB_DBT_MALLOC);
    TC(curs->get(&key, &val, DB_FIRST), 0);
    free(key.get_data());
    free(val.get_data());
    TC(curs->del(DB_DELETE_ANY), 0);
    TC(curs->get(&key, &val, DB_CURRENT), DB_KEYEMPTY);
    TC(curs->del(DB_DELETE_ANY), DB_NOTFOUND);
    curs->close(); // no deleting cursors.
}

int main(int argc, char *argv[]) {
    test_env_exceptions();
    test_db_exceptions();
    test_dbc_exceptions();
    system("rm *.tokulog");
    return 0;
}

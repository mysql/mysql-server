#include <assert.h>
#include <db_cxx.h>
#include <errno.h>

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
	TC(env.txn_begin(0, &txn, 0),                                                    EINVAL); // not configured for transations
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


int main(int argc, char *argv[]) {
    test_env_exceptions();
    return 0;
}

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

void test_no_env () {
#if 0
    DbEnv env(0);
    TC(env.open(".", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE , 0777),    0);
    Db db(&env, 0);
#else
    Db db(0, 0);
#endif
    unlink("foo.db");
    TC(db.open(0, "foo.db", 0, DB_BTREE, DB_CREATE, 0777), 0);
}

int main(int argc, char *argv[]) {
    test_no_env();
    system("rm -f *.tokulog");
    return 0;
}

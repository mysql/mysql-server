#include <db_cxx.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <iostream>
using namespace std;

int cmp(DB *db, const DBT *dbt1, const DBT *dbt2) {
    return 0;
}

void test_db(void) {
    DbEnv env(0);
    env.open(NULL, DB_CREATE|DB_PRIVATE, 0666);
    Db db(&env, 0);
    
    int r;
    
    r = db.set_bt_compare(cmp);                 assert(r == 0);
    try {
	r = db.remove("DoesNotExist.db", NULL, 0);
	abort(); // must not make it here.
    } catch (DbException e) {
	assert(e.get_errno() == ENOENT);
    }
    // The db is closed.
    env.close(0);
}

void test_db_env(void) {
    DbEnv dbenv(0);
    int r;
    
    r = dbenv.set_data_dir(".");    assert(r == 0);
    try {
	r = dbenv.set_data_dir(NULL);
	abort();
    } catch (DbException e) {
	assert(e.get_errno() == EINVAL);
    }
    dbenv.set_errpfx("Prefix");
    dbenv.set_errfile(stdout);
    dbenv.err(0, "Hello %s!\n", "Name");
    dbenv.close(0);
}

int main()
{
    test_db();
    test_db_env();
    return 0;
}

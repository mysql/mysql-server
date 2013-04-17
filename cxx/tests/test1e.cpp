#include <db_cxx.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <iostream>
using namespace std;

int cmp(DB *db, const DBT *dbt1, const DBT *dbt2) {
    return 0;
}

#define DIR "test1e.dir"

void test_db(void) {
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    DbEnv env(0);
    { int r = env.set_redzone(0);              assert(r==0); }
    { int r = env.set_default_bt_compare(cmp); assert(r == 0); }
    env.open(DIR, DB_CREATE|DB_PRIVATE, 0666);
    Db db(&env, 0);
    
    int r;
    
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

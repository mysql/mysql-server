#include <db_cxx.h>
#include <errno.h>
#include <assert.h>


#include <iostream>
using namespace std;

void test_dbt(void) {
    u_int32_t size  = 3;
    u_int32_t flags = 5;
    u_int32_t ulen  = 7;
    void*     data  = &size;
    Dbt dbt;

    dbt.set_size(size);
    dbt.set_flags(flags);
    dbt.set_data(data);
    dbt.set_ulen(ulen);
    assert(dbt.get_size()  == size);
    assert(dbt.get_flags() == flags);
    assert(dbt.get_data()  == data);
    assert(dbt.get_ulen()  == ulen);
}

int cmp(DB *db, const DBT *dbt1, const DBT *dbt2) {
    return 0;
}

void test_db(void) {
    DbEnv env(0);
    env.open(NULL, DB_PRIVATE, 0666);
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
    r = dbenv.set_data_dir("..");   assert(r == 0);
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
    test_dbt();
    test_db();
    test_db_env();
    return 0;
}

#include <db_cxx.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>

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
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    { int r = env.set_redzone(0); assert(r==0); }
    int r = env.open("test1.dir", DB_CREATE|DB_PRIVATE, 0666);
    assert(r==0);
    Db db(&env, 0);
    
    r = db.set_bt_compare(cmp);                 assert(r == 0);
    r = db.remove("DoesNotExist.db", NULL, 0);  assert(r == ENOENT);
    // The db is closed
    r = env.close(0);                           assert(r== 0);
}

int main()
{
    system("rm -rf test1.dir");
    mkdir("test1.dir", 0777);
    test_dbt();
    test_db();
    return 0;
}

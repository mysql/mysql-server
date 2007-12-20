#include <db_cxx.h>
#include <errno.h>


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
    Db db(NULL, 0);
    int r;
    
    r = db.set_bt_compare(cmp);                 assert(r == 0);
    r = db.remove("DoesNotExist.db", NULL, 0);  assert(r == ENOENT);
}

int main()
{
    test_dbt();
    test_db();
    cout << "Hello World!" << endl;   cout << "Welcome to C++ Programming" << endl;
    return 0;
}

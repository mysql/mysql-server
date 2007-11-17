#include <stdio.h>
#include <assert.h>
#include <db.h>

// DIR is defined in the Makefile

int main() {
    DB_ENV *dbenv;
    int r;

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->set_data_dir(dbenv, DIR);
    assert(r == 0);

    r = dbenv->set_data_dir(dbenv, DIR);
    assert(r == 0);

    r = dbenv->open(dbenv, DIR, DB_PRIVATE+DB_INIT_MPOOL, 0);
    assert(r == 0);

    r = dbenv->set_data_dir(dbenv, DIR);
    assert(r != 0);
    
    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}

#include <stdio.h>
#include <assert.h>
#include <db.h>

int main() {
    DB_ENV *dbenv;
    int r;

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->set_tmp_dir(dbenv, ".");
    //    assert(r == 0);

    r = dbenv->set_tmp_dir(dbenv, ".");
    //    assert(r == 0);

    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}

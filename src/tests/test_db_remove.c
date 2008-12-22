#include <toku_portability.h>
#include <fcntl.h>
#include "test.h"

DB_ENV * const null_env = 0;
DB *db1, *db2;
DB_TXN * const null_txn = 0;

const char * const fname = ENVDIR "/" "test_db_remove.brt";

void test_db_remove (void) {
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    // create the DB
    r = db_create(&db1, null_env, 0);                                  assert(r == 0);
    r = db1->open(db1, null_txn, fname, 0, DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    // Now remove it, while it is open.
    r = db_create(&db2, null_env, 0);                                  assert(r==0);
    r = db2->remove(db2, fname, 0, 0);
#ifdef USE_TDB
    assert(r!=0);
#else
    assert(r==0);
#endif

    r = db1->close(db1, 0);                                            assert(r==0);
}

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    test_db_remove();

    return 0;
}

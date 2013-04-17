#include "test.h"

int test_main(int argc, char * const argv[])
{
    int r;
    DB * db;
    DB_ENV * db_env;
    (void) argc;
    (void) argv;

    char buf[200];
    snprintf(buf, 200, "rm -rf " ENVDIR);
    r = system(buf); CHK(r);
    r = toku_os_mkdir(ENVDIR, 0755); CHK(r);

    // set things up
    r = db_env_create(&db_env, 0); CHK(r);
    r = db_env->open(db_env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0755); CHK(r);
    r = db_create(&db, db_env, 0); CHK(r);
    r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0644); CHK(r);

    // - does not test low bounds, so a 0 byte key is "okay"
    // - assuming 32k keys and 32mb values are the max
    r = db->row_size_supported(db, 0, 0);
    assert(r == 0);
    r = db->row_size_supported(db, 100000000, 100000000);
    assert(r != 0);
    r = db->row_size_supported(db, 100, 1);
    assert(r == 0);
    r = db->row_size_supported(db, 1, 100);
    assert(r == 0);
    r = db->row_size_supported(db, 4*1024, 4*1024*1024);
    assert(r == 0);
    r = db->row_size_supported(db, 32*1024, 32*1024*1024);
    assert(r == 0);
    r = db->row_size_supported(db, 32*1024 + 1, 32*1024*1024 + 1);
    assert(r != 0);

    // clean things up
    r = db->close(db, 0); CHK(r);
    r = db_env->close(db_env, 0); CHK(r);

    return 0;
}

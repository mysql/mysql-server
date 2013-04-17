/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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
    r = system(buf); { int chk_r = r; CKERR(chk_r); }
    r = toku_os_mkdir(ENVDIR, 0755); { int chk_r = r; CKERR(chk_r); }

    // set things up
    r = db_env_create(&db_env, 0); { int chk_r = r; CKERR(chk_r); }
    r = db_env->open(db_env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0755); { int chk_r = r; CKERR(chk_r); }
    r = db_create(&db, db_env, 0); { int chk_r = r; CKERR(chk_r); }
    r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0644); { int chk_r = r; CKERR(chk_r); }

    // - does not test low bounds, so a 0 byte key is "okay"
    // - assuming 32k keys and 32mb values are the max
    uint32_t max_key, max_val;
    db->get_max_row_size(db, &max_key, &max_val);
    // assume it is a red flag for the key to be outside the 16-32kb range
    assert(max_key >= 16*1024);
    assert(max_key <= 32*1024);
    // assume it is a red flag for the value to be outside the 16-32mb range
    assert(max_val >= 16*1024*1024);
    assert(max_val <= 32*1024*1024);

    // clean things up
    r = db->close(db, 0); { int chk_r = r; CKERR(chk_r); }
    r = db_env->close(db_env, 0); { int chk_r = r; CKERR(chk_r); }

    return 0;
}

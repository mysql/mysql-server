/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"
#include <sys/wait.h>

static void clean_env (const char *envdir) {
    const int len = strlen(envdir)+100;
    char cmd[len];
    snprintf(cmd, len, "rm -rf %s", envdir);
    int r = system(cmd);
    CKERR(r);
    CKERR(toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void setup_env (DB_ENV **envp, const char *envdir) {
    { int chk_r = db_env_create(envp, 0); CKERR(chk_r); }
    (*envp)->set_errfile(*envp, stderr);
#ifdef TOKUDB
    { int chk_r = (*envp)->set_redzone(*envp, 0); CKERR(chk_r); }
#endif
    { int chk_r = (*envp)->open(*envp, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void setup_env_and_prepare (DB_ENV **envp, const char *envdir, bool commit) {
    DB *db;
    DB_TXN *txn;
    clean_env(envdir);
    setup_env(envp, envdir);
    CKERR(db_create(&db, *envp, 0));
    CKERR(db->open(db, NULL, "foo.db", 0, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO));
    CKERR((*envp)->txn_begin(*envp, 0, &txn, 0));
    uint8_t gid[DB_GID_SIZE];
    memset(gid, 0, DB_GID_SIZE);
    gid[0]=42;
    CKERR(txn->prepare(txn, gid));
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    if (commit)
        CKERR(txn->commit(txn, 0));
}

int test_main (int argc, char *const argv[]) {
    default_parse_args(argc, argv);
    DB_ENV *env;
    setup_env_and_prepare(&env, TOKU_TEST_FILENAME, true);
    { int chk_r = env ->close(env,  0); CKERR(chk_r); }
    return 0;
}

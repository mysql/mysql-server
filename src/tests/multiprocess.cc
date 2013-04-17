/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

#include "test.h"

static inline size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

static void
test_env (const char *envdir0, const char *envdir1, int expect_open_return) {
    int r;
    toku_os_recursive_delete(envdir0);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    if (strcmp(envdir0, envdir1) != 0) {
        toku_os_recursive_delete(envdir1);
        r = toku_os_mkdir(envdir1, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }
    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, envdir0, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->set_redzone(env2, 0);
        CKERR(r);
    r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, expect_open_return);

    r = env->close(env, 0);
        CKERR(r);

    if (expect_open_return != 0) {
        r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }

    r = env2->close(env2, 0);
        CKERR(r);
}

static void
test_datadir (const char *envdir0, const char *datadir0, const char *envdir1, const char *datadir1, int expect_open_return) {
    char s[256];

    int r;
    sprintf(s, "rm -rf %s", envdir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", envdir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0);
        CKERR(r);
    r = env->set_data_dir(env, datadir0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, envdir0, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->set_redzone(env2, 0);
        CKERR(r);
    r = env2->set_data_dir(env2, datadir1);
        CKERR(r);
    r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, expect_open_return);

    r = env->close(env, 0);
        CKERR(r);

    if (expect_open_return != 0) {
        r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }

    r = env2->close(env2, 0);
        CKERR(r);
}
static void
test_logdir (const char *envdir0, const char *datadir0, const char *envdir1, const char *datadir1, int expect_open_return) {
    char s[256];

    int r;
    sprintf(s, "rm -rf %s", envdir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", envdir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0);
        CKERR(r);
    r = env->set_lg_dir(env, datadir0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, envdir0, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->set_redzone(env2, 0);
        CKERR(r);
    r = env2->set_lg_dir(env2, datadir1);
        CKERR(r);
    r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, expect_open_return);

    r = env->close(env, 0);
        CKERR(r);

    if (expect_open_return != 0) {
        r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }

    r = env2->close(env2, 0);
        CKERR(r);
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU|S_IRWXG|S_IRWXO);
    assert_zero(r);

    char env0[TOKU_PATH_MAX+1];
    char env1[TOKU_PATH_MAX+1];
    toku_path_join(env0, 2, TOKU_TEST_FILENAME, "e0");
    toku_path_join(env1, 2, TOKU_TEST_FILENAME, "e1");
    test_env(env0, env1, 0);
    test_env(env0, env0, EWOULDBLOCK);
    char wd[TOKU_PATH_MAX+1];
    char *cwd = getcwd(wd, sizeof wd);
    assert(cwd != nullptr);
    char data0[TOKU_PATH_MAX+1];
    toku_path_join(data0, 3, cwd, TOKU_TEST_FILENAME, "d0");
    char data1[TOKU_PATH_MAX+1];
    toku_path_join(data1, 3, cwd, TOKU_TEST_FILENAME, "d1");
    test_datadir(env0, data0, env1, data1, 0);
    test_datadir(env0, data0, env1, data0, EWOULDBLOCK);
    test_logdir(env0, data0, env1, data1, 0);
    test_logdir(env0, data0, env1, data0, EWOULDBLOCK);

    toku_os_recursive_delete(env0);
    toku_os_recursive_delete(env1);
    toku_os_recursive_delete(data0);
    toku_os_recursive_delete(data1);

    return 0;
}

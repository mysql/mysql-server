/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

#include "test.h"

static size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

static void
test_env (const char *envdir0, const char *envdir1, int expect_open_return) {
    char rmcmd[32 + max(strlen(envdir0), strlen(envdir1)) + 1];
    int r;
    sprintf(rmcmd, "rm -rf %s", envdir0);
    r = system(rmcmd);
    CKERR(r);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    if (strcmp(envdir0, envdir1) != 0) {
        sprintf(rmcmd, "rm -rf %s", envdir1);
        r = system(rmcmd);
        CKERR(r);
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

static char *
full_name(const char *subdir) {
    char wd[256];
    assert(getcwd(wd, sizeof wd) != NULL);
    char *path = toku_malloc(strlen(wd) + strlen(subdir) + 2);
    sprintf(path, "%s/%s", wd, subdir);
    return path;
}

static void
unlink_dir (const char *dir) {
    int len = strlen(dir)+100;
    char cmd[len];
    snprintf(cmd, len, "rm -rf %s", dir);
    system(cmd);
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    const char *env0 = ENVDIR "e0";
    const char *env1 = ENVDIR "e1";
    test_env(env0, env1, 0);
    test_env(env0, env0, EWOULDBLOCK);
    char *data0 = full_name(ENVDIR "d0");
    char *data1 = full_name(ENVDIR "d1");
    test_datadir(env0, data0, env1, data1, 0);
    test_datadir(env0, data0, env1, data0, EWOULDBLOCK);
    test_logdir(env0, data0, env1, data1, 0);
    test_logdir(env0, data0, env1, data0, EWOULDBLOCK);

    unlink_dir(env0);
    unlink_dir(env1);
    unlink_dir(data0);
    unlink_dir(data1);

    toku_free(data0);
    toku_free(data1);

    return 0;
}


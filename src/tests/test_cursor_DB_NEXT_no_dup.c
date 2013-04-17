/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <portability.h>
#include <db.h>

#include "test.h"

static DBC* cursor      = NULL;
static DB*  db          = NULL;
static DB_ENV* env      = NULL;
static int r            = 0;
static BOOL dups        = FALSE;
static DB_TXN* null_txn = NULL;

static void setup_env(void) {
    assert(!env && !db && !cursor);
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);
        CKERR(r);
    assert(env);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_THREAD|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    assert(env);
}

static void close_env(void) {
    assert(env && !db && !cursor);
    r = env->close(env, 0);
        CKERR(r);
    env = NULL;
}

static void setup_db(u_int32_t dup_flags) {
    assert(env && !db && !cursor);
    r = db_create(&db, env, 0);
        CKERR(r);
    assert(db);
    db->set_errfile(db, stderr);
    if (dup_flags) {
        r = db->set_flags(db, dup_flags);
        CKERR(r);
    }
    r = db->open(db, null_txn, "foo.db", "main", DB_BTREE, DB_CREATE, 0666);
        CKERR(r);
    assert(db);
}

static void close_db(void) {
    assert(env && db && !cursor);
    r = db->close(db, 0);
        CKERR(r);
    db = NULL;
}

static void setup_cursor(void) {
    assert(env && db && !cursor);
    r = db->cursor(db, NULL, &cursor, 0);
        CKERR(r);
    assert(cursor);
}

static void close_cursor(void) {
    assert(env && db && cursor);
    r = cursor->c_close(cursor);
        CKERR(r);
    cursor = NULL;
}

#ifdef USE_BDB
#define DB_YESOVERWRITE 0
#endif

static void insert(char k, char d) {
    DBT key;
    DBT data;
    r = db->put(db, null_txn, dbt_init(&key, &k, sizeof(k)), dbt_init(&data, &d, sizeof(d)), DB_YESOVERWRITE);
        CKERR(r);
}

static void c_get(u_int32_t flag, char key_expect, char data_expect) {
    DBT key;
    DBT data;

    r = cursor->c_get(cursor, dbt_init(&key, 0, 0), dbt_init(&data, 0, 0), flag);
        CKERR(r);
    assert(key.size == sizeof(key_expect));
    assert(data.size == sizeof(data_expect));
    char got_key  = *(char*)key.data;
    char got_data = *(char*)data.data;
    if (verbose &&
        (got_key != key_expect || got_data != data_expect)) {
        printf("DUPS [%d] c_get(%u) Expect (%c,%c)\n"
               "   Got (%c,%c)\n",
               (int)dups, flag, key_expect, data_expect, got_key, got_data);
    }
    assert(got_key  == key_expect);
    assert(got_data == data_expect);
}

static void test_skip_key(u_int32_t dup_flags, u_int32_t flag, BOOL is_next) {
    setup_env();
    setup_db(dup_flags);
    setup_cursor();

    /* ********************************************************************** */

    char key           = 'g';
    char data          = 'g';
    int forward = is_next ? 1 : -1;

    insert(key, data);
    insert((char)(key + forward), data);
    c_get(flag, key, data);
    insert(key, (char)(data + forward));
    c_get(flag, (char)(key + forward), data);

    /* ********************************************************************** */
    close_cursor();
    close_db();
    close_env();
}

static void test_do_not_skip_key(u_int32_t dup_flags, u_int32_t flag, BOOL is_next) {
    setup_env();
    setup_db(dup_flags);
    setup_cursor();

    char key    = 'g';
    char data   = 'g';
    int forward = is_next ? 1 : -1;

    insert(key, data);
    insert((char)(key + forward), data);
    c_get(flag, key, data);
    insert(key, (char)(data + forward));
    c_get(flag, key, (char)(data + forward));

    close_cursor();
    close_db();
    close_env();
}

static void run_test(u_int32_t dup_flags) {
    dups = (BOOL)(dup_flags != 0);
    /* ********************************************************************** */
    /* Test DB_NEXT works properly. */
    if (dups) {
        test_do_not_skip_key(dup_flags, DB_NEXT, TRUE);
    }
    else {
        test_skip_key(dup_flags, DB_NEXT, TRUE);
    }
    /* ********************************************************************** */
    /* Test DB_PREV works properly. */
    if (dups) {
        test_do_not_skip_key(dup_flags, DB_PREV, FALSE);
    }
    else {
        test_skip_key(dup_flags, DB_PREV, FALSE);
    }
    /* ********************************************************************** */
    /* Test DB_PREV_NODUP works properly. */
    test_skip_key(dup_flags, DB_PREV_NODUP, FALSE);
    /* ********************************************************************** */
    /* Test DB_NEXT_NODUP works properly. */
    test_skip_key(dup_flags, DB_NEXT_NODUP, TRUE);
    /* ********************************************************************** */
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    
    run_test(0);
    run_test(DB_DUP | DB_DUPSORT);

    return 0;
}

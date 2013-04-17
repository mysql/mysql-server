/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <memory.h>
#include <toku_portability.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// ENVDIR is defined in the Makefile
#define FNAME       "nonames.db"
char *name = NULL;

#define NUM         8
#define MAX_LENGTH  (1<<16)

int order[NUM+1];
u_int32_t length[NUM];
u_int8_t data[NUM][MAX_LENGTH];
DBT descriptors[NUM];
DB_ENV *env;

DB *db;
DB_TXN *null_txn = NULL;
int last_open_descriptor = -1;



static void
verify_db_matches(void) {
    DBT * dbt = &db->descriptor;

    if (last_open_descriptor<0) {
        assert(dbt->size == 0 && dbt->data == NULL);
    }
    else {
        assert(last_open_descriptor < NUM);
        assert(dbt->size == descriptors[last_open_descriptor].size);
        assert(!memcmp(dbt->data, descriptors[last_open_descriptor].data, dbt->size));
        assert(dbt->data != descriptors[last_open_descriptor].data);
    }
}

static int
verify_int_cmp (DB *dbp, const DBT *a, const DBT *b) {
    verify_db_matches();
    int r = int_dbt_cmp(dbp, a, b);
    return r;
}

static void
open_db(int descriptor) {
    /* create the dup database file */
    int r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_bt_compare(db, verify_int_cmp);
    CKERR(r);
    if (descriptor >= 0) {
        assert(descriptor < NUM);
        r = db->set_descriptor(db, &descriptors[descriptor]);
        CKERR(r);
        last_open_descriptor = descriptor;
    }
    r = db->open(db, null_txn, FNAME, name, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    verify_db_matches();
}

static void
close_db(void) {
    int r = db->close(db, 0);
    CKERR(r);
}

static void
setup_data(void) {
    int r = db_env_create(&env, 0);                                           CKERR(r);
    const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK |DB_THREAD |DB_PRIVATE;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);
    int i;
    for (i=0; i < NUM; i++) {
        length[i] = i * MAX_LENGTH / (NUM-1);
        u_int32_t j;
        for (j = 0; j < length[i]; j++) {
            data[i][j] = random() & 0xFF;
        }
        memset(&descriptors[i], 0, sizeof(descriptors[i]));
        descriptors[i].size = length[i];
        descriptors[i].data = &data[i][0];
    }
    last_open_descriptor = -1;
}

static void
permute_order(void) {
    int i;
    for (i=0; i < NUM; i++) {
        order[i] = i;
    }
    for (i=0; i < NUM; i++) {
        int which = (random() % (NUM-i)) + i;
        int temp = order[i];
        order[i] = order[which];
        order[which] = temp;
    }
}

static void
test_insert (int n) {
    int i;
    static int last = 0;
    for (i=0; i<n; i++) {
        int k = last++;
        DBT key, val;
        int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &i, sizeof i), DB_YESOVERWRITE);
        CKERR(r);
    }
}

   
static void
runtest(void) {
    int i;
    for (i=0; i < NUM; i++) {
        open_db(-1);
        test_insert(i);
        close_db();

        open_db(order[i]);
        test_insert(i);
        close_db();
    }
}

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    system("rm -rf " ENVDIR);
    int r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    setup_data();
    permute_order();
    name = NULL;
    runtest();
    env->close(env, 0);

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    setup_data();
    permute_order();
    name = "main.db";
    runtest();
    env->close(env, 0);

    return 0;
}                                        


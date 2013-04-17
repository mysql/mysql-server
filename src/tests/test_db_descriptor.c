/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <memory.h>
#include <toku_portability.h>
#include <db.h>

#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// ENVDIR is defined in the Makefile
#define FNAME       "foo.tokudb"
char *name = NULL;

#define NUM         8
#define MAX_LENGTH  (1<<16)

int order[NUM+1];
u_int32_t length[NUM];
u_int8_t data[NUM][MAX_LENGTH];
DBT descriptors[NUM];
DB_ENV *env;

DB *db;
DB_TXN *txn = NULL;
DB_TXN *null_txn;
int last_open_descriptor = -1;

int abort_type;
int get_table_lock;
u_int64_t num_called = 0;
int manual_truncate = 0;


static void
verify_db_matches(void) {
    const DBT * dbt = db->descriptor;

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
    num_called++;
    verify_db_matches();
    int r = int_dbt_cmp(dbp, a, b);
    return r;
}

static void
open_db(int descriptor) {
    /* create the dup database file */
    assert(txn==NULL);
    int r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_bt_compare(db, verify_int_cmp);
    CKERR(r);
    assert(abort_type >=0 && abort_type <= 2);
    if (abort_type==2) {
        r = env->txn_begin(env, null_txn, &txn, 0);
            CKERR(r);
        last_open_descriptor = -1; //DB was destroyed at end of last close, did not hang around.
    }
    if (descriptor >= 0) {
        assert(descriptor < NUM);
        u_int32_t descriptor_version = 1;
        r = db->set_descriptor(db, descriptor_version, &descriptors[descriptor], abort_on_upgrade);
        CKERR(r);
        last_open_descriptor = descriptor;
    }
    r = db->open(db, txn, FNAME, name, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    verify_db_matches();
    if (abort_type!=2) {
        r = env->txn_begin(env, null_txn, &txn, 0);
            CKERR(r);
    }
    assert(txn);
    if (get_table_lock) {
        r = db->pre_acquire_table_lock(db, txn);
        CKERR(r);
    }
}

static void
delete_db(void) {
    int r = db_create(&db, env, 0);
    CKERR(r);
    r = db->remove(db, FNAME, name, 0);
    if (abort_type==2) {
        CKERR2(r, ENOENT); //Abort deleted it
    }
    else CKERR(r);
    last_open_descriptor = -1;
}

static void
close_db(void) {
    int r;
    if (manual_truncate) {
        u_int32_t ignore_row_count;
        r = db->truncate(db, txn, &ignore_row_count, 0);
        CKERR(r);
    }
    if (abort_type>0) {
        r = db->close(db, 0);
        CKERR(r);
        r = txn->abort(txn);
        CKERR(r);
    }
    else {
        r = txn->commit(txn, 0);
        CKERR(r);
        r = db->close(db, 0);
        CKERR(r);
    }
    txn = NULL;
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
            data[i][j] = (u_int8_t)(random() & 0xFF);
        }
        memset(&descriptors[i], 0, sizeof(descriptors[i]));
        descriptors[i].size = length[i];
        descriptors[i].data = &data[i][0];
    }
    last_open_descriptor = -1;
    txn = NULL;
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
        u_int64_t called = num_called;
        int r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &i, sizeof i), DB_YESOVERWRITE);
        if (i>0) assert(num_called > called);
        CKERR(r);
    }
}

   
static void
runtest(void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    setup_data();
    permute_order();

    int i;
    for (i=0; i < NUM; i++) {
        open_db(-1);
        test_insert(i);
        close_db();
        open_db(-1);
        test_insert(i);
        close_db();
        delete_db();

        open_db(order[i]);
        test_insert(i);
        close_db();
        open_db(-1);
        test_insert(i);
        close_db();
        open_db(order[i]);
        test_insert(i);
        close_db();
        delete_db();
    }
    env->close(env, 0);
}


int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    for (abort_type = 0; abort_type < 3; abort_type++) {
        for (get_table_lock = 0; get_table_lock < 2; get_table_lock++) {
            for (manual_truncate = 0; manual_truncate < 2; manual_truncate++) {
                name = NULL;
                runtest();

                name = "bar";
                runtest();
            }
        }
    }

    return 0;
}                                        


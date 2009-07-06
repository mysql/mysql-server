/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

static DB *db;
static DB_ENV *env;

static __attribute__((__unused__)) int
length_int_dbt_cmp (DB *db_compare, const DBT *a, const DBT *b) {
    assert(db_compare && a && b);
    assert(a->size > sizeof(int));
    assert(b->size > sizeof(int));

    int i;
    int extra_len_a = *(u_int8_t*)(a->data+4);
    assert(a->size == sizeof(int)+extra_len_a);

    for (i = 1; i < extra_len_a; i++) {
        assert(((char*)a->data+4)[i] == ' ');
    }

    int extra_len_b = *(u_int8_t*)(b->data+4);
    assert(b->size == sizeof(int)+extra_len_b);
    for (i = 1; i < extra_len_b; i++) {
        assert(((char*)b->data+4)[i] == ' ');
    }

    int x = *(int *) a->data;
    int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;

    if (extra_len_a<extra_len_b) return -1;
    if (extra_len_a>extra_len_b) return 1;
    return 0;
}

static void
setup_db (u_int32_t dup_mode) {
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int r;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_data_dir(env, ENVDIR);
    r = env->set_lg_dir(env, ENVDIR);
    r = env->open(env, 0, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    CKERR(r);

    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r=db->set_bt_compare(db, int_dbt_cmp); CKERR(r);
        r=db->set_dup_compare(db, length_int_dbt_cmp); CKERR(r);
        r = db->set_flags(db, dup_mode); assert(r == 0); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }
}

static inline DBT *
dbt_init_length(DBT *dbt, int val, u_int8_t extra_len, u_int8_t* buf) {
    *(int*)buf = val;
    buf[sizeof(int)] = extra_len;
    int i;
    for (i = 1; i < extra_len; i++) {
        buf[sizeof(int)+i] = ' ';
    }
    return dbt_init(dbt, buf, sizeof(int)+extra_len);
}

static void
test_txn_abort (u_int32_t dup_mode) {
    setup_db(dup_mode);
    DBT key, val;
    int r;


    DB_TXN *txn = NULL;
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    u_int8_t value1[256];
    u_int8_t value2[256];

    int k = 1;
    int v1 = 1;
    int v2 = 1;
    int extra_1 = 1;
    int extra_2 = 2;
    r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init_length(&val, v1, extra_1, value1), DB_YESOVERWRITE); 
        CKERR(r);
    r = txn->commit(txn, DB_TXN_NOSYNC); 
    txn = NULL;

    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init_length(&val, v2, extra_2, value2), DB_YESOVERWRITE); 
        CKERR(r);
    r = db->del(db, txn, dbt_init(&key, &k, sizeof k), DB_DELETE_ANY);
        CKERR(r);
    //Flush by scanning
    {
        DBC *c;
        r = db->cursor(db, txn, &c, 0);
            CKERR(r);
        DBT ck;
        DBT cv;
        memset(&ck, 0, sizeof(ck));
        memset(&cv, 0, sizeof(cv));
        do {
            r = c->c_get(c, &ck, &cv, DB_NEXT);
        } while (r==0);
        CKERR2(r, DB_NOTFOUND);
        r = c->c_close(c);
            CKERR(r);
    }
    r = txn->commit(txn, 0); 
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);

}

int
test_main(int argc, char *argv[]) {
    parse_args(argc, argv);
    test_txn_abort(0);
//    test_txn_abort(DB_DUP|DB_DUPSORT);
    return 0;
}
/*
BNC_NBYTESINBUF
comparison does assert that 'val(value) == length(value)'
insert 1,'1' (commit it)
       1,'2 '
do a db->del (1) we'll crash on the assert
*/

// this test makes sure the LSN filtering is used during recovery of put_multiple

#include <sys/stat.h>
#include <fcntl.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

enum {MAX_DBS = 64, MAX_KEY = 8, MAX_VAL = 8};
DB *dbs_multiple[MAX_DBS];
DB *dbs_single[MAX_DBS];
char names_single[MAX_DBS][sizeof("dbs_0xFFF")];
char names_multiple[MAX_DBS][sizeof("dbm_0xFFF")];
uint32_t num_dbs;
uint32_t flags[MAX_DBS];
uint32_t ids[MAX_DBS];
uint32_t kbuf[MAX_DBS][MAX_KEY/4];
uint32_t vbuf[MAX_DBS][MAX_VAL/4];
DBT dest_keys[MAX_DBS];
DBT dest_vals[MAX_DBS];

#define CKERRIFNOT0(r)           do { if (num_dbs>0) { CKERR(r); } else { CKERR2(r, EINVAL); } } while (0)
#define CKERR2IFNOT0(r, rexpect) do { if (num_dbs>0) { CKERR2(r, rexpect); } else { CKERR2(r, EINVAL); } } while (0)

static int
put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {

    src_db = src_db;

    uint32_t which = *(uint32_t*)dest_db->app_private;
    assert(which < MAX_DBS);

    assert(src_key->size == 4);
    assert(src_val->size == 4);
    kbuf[which][0] = *(uint32_t*)src_key->data;
    kbuf[which][1] = which;
    vbuf[which][0] = which;
    vbuf[which][1] = *(uint32_t*)src_val->data;
    dest_key->data = kbuf[which];
    dest_key->size = sizeof(kbuf[which]);
    dest_val->data = vbuf[which];
    dest_val->size = sizeof(vbuf[which]);
    return 0;
}

static void run_test (void) {
    int r;
    if (verbose)
        printf("env-put-multiple num_dbs[%u]\n", num_dbs);

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    uint32_t which;
    {
        //Create dbs.
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);
        DB *db;
        for (which = 0; which < num_dbs; which++) {
            ids[which] = which;
            r = db_create(&dbs_multiple[which], env, 0);
            CKERR(r);
            db = dbs_multiple[which];
            r = db->open(db, txn, names_multiple[which], NULL, DB_BTREE, DB_CREATE, 0666);
            CKERR(r);
            db->app_private = &ids[which];
            r = db_create(&dbs_single[which], env, 0);
            CKERR(r);
            db = dbs_single[which];
            r = db->open(db, txn, names_single[which], NULL, DB_BTREE, DB_CREATE, 0666);
            CKERR(r);
        }
        r = txn->commit(txn, 0);
    }


    uint32_t magic = 0xDEADBEEF;
    // txn_begin; insert magic number
    {
        for (which = 0; which < num_dbs; which++) {
            flags[which] = 0;
        }
        memset(flags, 0, sizeof(flags)); //reset
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        uint32_t magic2 = ~magic;
        DBT keydbt = {.data=&magic, .size=sizeof(magic)};
        DBT valdbt = {.data=&magic2, .size=sizeof(magic2)};
        r = env->put_multiple(env, NULL, txn, &keydbt, &valdbt, num_dbs, dbs_multiple, dest_keys, dest_vals, flags);
        CKERRIFNOT0(r);
        for (which = 0; which < num_dbs; which++) {
            DBT key={.data = kbuf[which], .size = sizeof(kbuf[which])};
            DBT val={.data = vbuf[which], .size = sizeof(vbuf[which])};
            DB *db = dbs_single[which];
            r = db->put(db, txn, &key, &val, flags[which]);
            CKERR(r);
        }
        r = txn->commit(txn, 0);
    }
    {
        //Insert again with DB_YESOVERWRITE, expect it to work.
        for (which = 0; which < num_dbs; which++) {
            flags[which] = DB_YESOVERWRITE;
        }
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        uint32_t magic2 = ~magic;
        DBT keydbt = {.data=&magic, .size=sizeof(magic)};
        DBT valdbt = {.data=&magic2, .size=sizeof(magic2)};
        r = env->put_multiple(env, NULL, txn, &keydbt, &valdbt, num_dbs, dbs_multiple, dest_keys, dest_vals, flags);
        CKERRIFNOT0(r);
        for (which = 0; which < num_dbs; which++) {
            DBT key={.data = kbuf[which], .size = sizeof(kbuf[which])};
            DBT val={.data = vbuf[which], .size = sizeof(vbuf[which])};
            DB *db = dbs_single[which];
            r = db->put(db, txn, &key, &val, flags[which]);
            CKERR(r);
        }
        r = txn->commit(txn, 0);
    }
    {
        //Insert again with DB_NOOVERWRITE, expect it to fail (unless 0 dbs).
        for (which = 0; which < num_dbs; which++) {
            flags[which] = DB_NOOVERWRITE;
        }
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        uint32_t magic2 = ~magic;
        DBT keydbt = {.data=&magic, .size=sizeof(magic)};
        DBT valdbt = {.data=&magic2, .size=sizeof(magic2)};
        r = env->put_multiple(env, NULL, txn, &keydbt, &valdbt, num_dbs, dbs_multiple, dest_keys, dest_vals, flags);
        CKERR2IFNOT0(r, DB_KEYEXIST);
        for (which = 0; which < num_dbs; which++) {
            DBT key={.data = kbuf[which], .size = sizeof(kbuf[which])};
            DBT val={.data = vbuf[which], .size = sizeof(vbuf[which])};
            DB *db = dbs_single[which];
            r = db->put(db, txn, &key, &val, flags[which]);
            CKERR2(r, DB_KEYEXIST);
        }
        r = txn->commit(txn, 0);
    }

    {
        //Different number
        magic = 0xFEEDADAD;
        //Insert again with DB_YESOVERWRITE, using 2 transactions, expect it to fail (unless 0 dbs).
        for (which = 0; which < num_dbs; which++) {
            flags[which] = DB_YESOVERWRITE;
        }
        DB_TXN *txna;
        r = env->txn_begin(env, NULL, &txna, 0);
        CKERR(r);

        uint32_t magic2 = ~magic;
        DBT keydbt = {.data=&magic, .size=sizeof(magic)};
        DBT valdbt = {.data=&magic2, .size=sizeof(magic2)};
        r = env->put_multiple(env, NULL, txna, &keydbt, &valdbt, num_dbs, dbs_multiple, dest_keys, dest_vals, flags);
        CKERRIFNOT0(r);
        for (which = 0; which < num_dbs; which++) {
            DBT key={.data = kbuf[which], .size = sizeof(kbuf[which])};
            DBT val={.data = vbuf[which], .size = sizeof(vbuf[which])};
            DB *db = dbs_single[which];
            r = db->put(db, txna, &key, &val, flags[which]);
            CKERR(r);
        }

        DB_TXN *txnb;
        r = env->txn_begin(env, NULL, &txnb, 0);
        CKERR(r);

        //Lock should fail
        r = env->put_multiple(env, NULL, txnb, &keydbt, &valdbt, num_dbs, dbs_multiple, dest_keys, dest_vals, flags);
        CKERR2IFNOT0(r, DB_LOCK_NOTGRANTED);
        for (which = 0; which < num_dbs; which++) {
            DBT key={.data = kbuf[which], .size = sizeof(kbuf[which])};
            DBT val={.data = vbuf[which], .size = sizeof(vbuf[which])};
            DB *db = dbs_single[which];
            r = db->put(db, txnb, &key, &val, flags[which]);
            CKERR2(r, DB_LOCK_NOTGRANTED);
        }
        r = txna->commit(txna, 0);

        //Should succeed this time.
        r = env->put_multiple(env, NULL, txnb, &keydbt, &valdbt, num_dbs, dbs_multiple, dest_keys, dest_vals, flags);
        CKERRIFNOT0(r);
        for (which = 0; which < num_dbs; which++) {
            DBT key={.data = kbuf[which], .size = sizeof(kbuf[which])};
            DBT val={.data = vbuf[which], .size = sizeof(vbuf[which])};
            DB *db = dbs_single[which];
            r = db->put(db, txnb, &key, &val, flags[which]);
            CKERR(r);
        }

        r = txnb->commit(txnb, 0);
    }

    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);
        DBC *c_single;
        DBC *c_multiple;
        
        DBT k_single, v_single, k_multiple, v_multiple;
        memset(&k_single, 0, sizeof(k_single));
        memset(&v_single, 0, sizeof(v_single));
        memset(&k_multiple, 0, sizeof(k_multiple));
        memset(&v_multiple, 0, sizeof(v_multiple));
        for (which = 0; which < num_dbs; which++) {
            r = dbs_multiple[which]->cursor(dbs_multiple[which], txn, &c_multiple, 0);
            CKERR(r);
            r = dbs_single[which]->cursor(dbs_single[which], txn, &c_single, 0);
            CKERR(r);

            int r1 = 0;
            int r2;
            while (r1 == 0) {
                r1 = c_single->c_get(c_single, &k_single, &v_single, DB_NEXT);
                r2 = c_multiple->c_get(c_multiple, &k_multiple, &v_multiple, DB_NEXT);
                assert(r1==r2);
                CKERR2s(r1, 0, DB_NOTFOUND);
                if (r1 == 0) {
                    assert(k_single.size == k_multiple.size);
                    assert(v_single.size == v_multiple.size);
                    assert(memcmp(k_single.data, k_multiple.data, k_single.size) == 0);
                    assert(memcmp(v_single.data, v_multiple.data, v_single.size) == 0);
                }
            }
            r = c_single->c_close(c_single);
            CKERR(r);
            r = c_multiple->c_close(c_multiple);
            CKERR(r);
        }
        r = txn->commit(txn, 0);
    }
    {
        for (which = 0; which < num_dbs; which++) {
            r = dbs_single[which]->close(dbs_single[which], 0);
            CKERR(r);
            r = dbs_multiple[which]->close(dbs_multiple[which], 0);
            CKERR(r);
        }
    }
    r = env->close(env, 0);
    CKERR(r);
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    uint32_t which;
    for (which = 0; which < MAX_DBS; which++) {
        sprintf(names_multiple[which], "dbm_0x%02X", which);
        sprintf(names_single[which], "dbs_0x%02X", which);
        dbt_init(&dest_keys[which], NULL, 0);
        dbt_init(&dest_vals[which], NULL, 0);
    }
    for (num_dbs = 0; num_dbs < 4; num_dbs++) {
        run_test();
    }
    for (num_dbs = 4; num_dbs <= MAX_DBS; num_dbs *= 2) {
        run_test();
    }
    return 0;
}

// This test sets the cache size to be small and then inserts enough data
// to make some basement nodes get evicted.  Then sends a broadcast update
// and checks all the data.  If the msns for evicted basement nodes and
// leaf nodes are not managed properly, this test should fail (because the
// broadcast message will not be applied to basement nodes being brought
// back in).

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const unsigned int NUM_KEYS = (1<<17);
const unsigned int MAGIC_EXTRA = 0x4ac0ffee;

const char original_data[] = "original: ha.rpbkasrkcabkshtabksraghpkars3cbkarpcpktkpbarkca.hpbtkvaekragptknbnsaotbknotbkaontekhba";
const char updated_data[]  = "updated: crkphi30bi8a9hpckbrap.k98a.pkrh3miachpk0[alr3s4nmubrp8.9girhp,bgoekhrl,nurbperk8ochk,bktoe";

static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) {
    unsigned int *e;
    assert(extra->size == sizeof(*e));
    e = extra->data;
    assert(*e == MAGIC_EXTRA);
    assert(old_val->size == sizeof(original_data));
    assert(memcmp(old_val->data, original_data, sizeof(original_data)) == 0);

    {
        DBT newval;
        set_val(dbt_init(&newval, updated_data, sizeof(updated_data)), set_extra);
    }

    return 0;
}

static int
int_cmp(DB *UU(db), const DBT *a, const DBT *b) {
    unsigned int *ap, *bp;
    assert(a->size == sizeof(*ap));
    ap = a->data;
    assert(b->size == sizeof(*bp));
    bp = b->data;
    return (*ap > *bp) - (*ap < *bp);
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    env->set_cachesize(env, 0, 10*(1<<20), 1);
    CHK(env->set_default_bt_compare(env, int_cmp));
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static int do_inserts(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, original_data, sizeof(original_data));
    for (i = 0; i < NUM_KEYS; ++i) {
        r = CHK(db->put(db, txn, keyp, valp, 0));
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db) {
    DBT extra;
    unsigned int e = MAGIC_EXTRA;
    DBT *extrap = dbt_init(&extra, &e, sizeof(e));
    int r = CHK(db->update_broadcast(db, txn, extrap, 0));
    return r;
}

static int do_verify_results(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);
    for (i = 0; i < NUM_KEYS; ++i) {
        r = CHK(db->get(db, txn, keyp, valp, 0));
        assert(val.size == sizeof(updated_data));
        assert(memcmp(val.data, updated_data, sizeof(updated_data)) == 0);
    }
    return r;
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

    CHK(db_create(&db, env, 0));
    db->set_pagesize(db, 256*1024);
    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));

            CHK(do_inserts(txn_1, db));
        });

    IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db));
        });

    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(do_verify_results(txn_3, db));
        });

    CHK(db->close(db, 0));

    cleanup();

    return 0;
}

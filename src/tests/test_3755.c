// stress test for update broadcast.  10M 8-byte keys should be 2, maybe 3
// levels of treeness, makes sure flushes work

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const unsigned int NUM_KEYS = 1024;


static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) 
{
    assert(extra->size == sizeof(unsigned int));
    assert(old_val->size == sizeof(unsigned int));
    unsigned int e = *(unsigned int *)extra->data;    
    unsigned int ov = *(unsigned int *)old_val->data;
    assert(e == (ov+1));
    {
        DBT newval;
        set_val(dbt_init(&newval, &e, sizeof(e)), set_extra);
    }
    //usleep(10);
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
    CHK(env->set_default_bt_compare(env, int_cmp));
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
    // make a really small checkpointing period
    CHK(env->checkpointing_set_period(env,1));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static int do_inserts(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i, v;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, &v, sizeof(v));
    for (i = 0; i < NUM_KEYS; ++i) {
        v = 0;
        r = CHK(db->put(db, txn, keyp, valp, 0));
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db, unsigned int i) {
    DBT extra;
    unsigned int e = i;
    DBT *extrap = dbt_init(&extra, &e, sizeof(e));
    int r = CHK(db->update_broadcast(db, txn, extrap, 0));
    return r;
}


int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->set_pagesize(db, 1<<8));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));

            CHK(do_inserts(txn_1, db));
        });

    for(unsigned int i = 1; i < 100; i++) {
        IN_TXN_COMMIT(env, NULL, txn_2, 0, {
                CHK(do_updates(txn_2, db, i));
            });
        for (unsigned int curr_key = 0; curr_key < NUM_KEYS; ++curr_key) {
            DBT key, val;
            unsigned int *vp;
            DBT *keyp = dbt_init(&key, &curr_key, sizeof(curr_key));
            DBT *valp = dbt_init(&val, NULL, 0);
            IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(db->get(db, txn_3, keyp, valp, 0));
            });
            assert(val.size == sizeof(*vp));
            vp = val.data;
            assert(*vp==i);
        }
    }

    CHK(db->close(db, 0));

    cleanup();

    return 0;
}

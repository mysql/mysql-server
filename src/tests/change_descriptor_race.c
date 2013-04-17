/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:

// test that inserts a bunch of rows, induces background flushes, and randomly
// changes descriptor every so often. if background threads don't synchronize
// with the descriptor change, there could be a data race on the descriptor's
// pointer value in the FT.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

static DB *db;
static DB_ENV *env;

static int desc_magic;

static int
int_cmp(DB *cmpdb, const DBT *a, const DBT *b) {
    assert(a);
    assert(b);

    assert(a->size == sizeof(int));
    assert(b->size == sizeof(int));
    assert(cmpdb->cmp_descriptor->dbt.size == sizeof(int));
    int magic = *(int *) cmpdb->cmp_descriptor->dbt.data;
    if (magic != desc_magic) {
        printf("got magic %d, wanted desc_magic %d\n", magic, desc_magic);
    }
    assert(magic == desc_magic);

    int x = *(int *) a->data;
    int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static void setup (void) {
    int r;
    { int chk_r = system("rm -rf " ENVDIR); CKERR(chk_r); }
    { int chk_r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    r = env->set_default_bt_compare(env, int_cmp); CKERR(r);
    //r = env->set_cachesize(env, 0, 50000, 1); CKERR(r);
    { int chk_r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    //r = db->set_pagesize(db, 2048); CKERR(r);
    //r = db->set_readpagesize(db, 1024); CKERR(r);
    IN_TXN_COMMIT(env, NULL, txn, 0, {
        { int chk_r = db->open(db, txn, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
    });
}

static void 
cleanup(void) {
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static void 
next_descriptor(void) {
    IN_TXN_COMMIT(env, NULL, txn, 0, {
        // get a new magic value
        desc_magic++;
        DBT desc_dbt;
        dbt_init(&desc_dbt, &desc_magic, sizeof(int));
        { int chk_r = db->change_descriptor(db, txn, &desc_dbt, DB_UPDATE_CMP_DESCRIPTOR); CKERR(chk_r); }
    });
}


static void
insert_change_descriptor_stress(void) {
    const int num_changes = 1000000;
    const int inserts_per_change = 100;
    const int valsize = 200 - sizeof(int);
    // bigger rows cause more flushes
    DBT key, value;
    char valbuf[valsize];
    memset(valbuf, 0, valsize);
    dbt_init(&value, valbuf, valsize);
    // do a bunch of inserts before changing the descriptor
    // the idea is that the inserts will cause flusher threads
    // which might race with the descriptor change. there's no
    // contract violation because we're not inserting and changing 
    // the descriptor in parallel. 
    for (int i = 0; i < num_changes; i++) {
        next_descriptor();
        IN_TXN_COMMIT(env, NULL, txn, 0, {
            for (int j = 0; j < inserts_per_change; j++) {
                int k = random64();
                dbt_init(&key, &k, sizeof(int));
                int r = db->put(db, txn, &key, &value, 0); CKERR(r);
            }
        });
    }
}

int 
test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    insert_change_descriptor_stress();
    cleanup();
    return 0;
}

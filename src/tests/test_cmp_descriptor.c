// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

BOOL cmp_desc_is_four;
u_int32_t four_byte_desc = 0xffffffff;
u_int64_t eight_byte_desc = 0x12345678ffffffff;


static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db), 
    DBT *dest_key, 
    DBT *dest_val, 
    const DBT *src_key, 
    const DBT *src_val
    ) 
{    
    dest_key->data = src_key->data;
    dest_key->size = src_key->size;
    dest_key->flags = 0;
    dest_val->data = src_val->data;
    dest_val->size = src_val->size;
    dest_val->flags = 0;
    return 0;
}
static void assert_cmp_desc_valid (DB* db) {
    if (cmp_desc_is_four) {
        assert(db->cmp_descriptor->dbt.size == sizeof(four_byte_desc));
    }
    else {
        assert(db->cmp_descriptor->dbt.size == sizeof(eight_byte_desc));
    }
    unsigned char* cmp_desc_data = db->cmp_descriptor->dbt.data;
    assert(cmp_desc_data[0] == 0xff);
    assert(cmp_desc_data[1] == 0xff);
    assert(cmp_desc_data[2] == 0xff);
    assert(cmp_desc_data[3] == 0xff);
}

static void assert_desc_four (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(four_byte_desc));
    assert(*(u_int32_t *)(db->descriptor->dbt.data) == four_byte_desc);
}
static void assert_desc_eight (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(eight_byte_desc));
    assert(*(u_int64_t *)(db->descriptor->dbt.data) == eight_byte_desc);
}

static int
desc_int64_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
    assert_cmp_desc_valid(db);
    assert(a);
    assert(b);

    assert(a->size == sizeof(int64_t));
    assert(b->size == sizeof(int64_t));

    int64_t x = *(int64_t *) a->data;
    int64_t y = *(int64_t *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static void setup (void) {
    int r;
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    r = env->set_default_bt_compare(env, desc_int64_dbt_cmp); CKERR(r);
    //r = env->set_cachesize(env, 0, 500000, 1); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static void do_inserts_and_queries(DB* db) {
    int r = 0;
    DB_TXN* write_txn = NULL;
    r = env->txn_begin(env, NULL, &write_txn, 0);
    CKERR(r);
    for (int i = 0; i < 2000; i++) {
        u_int64_t key_data = random();
        u_int64_t val_data = random();
        DBT key, val;
        dbt_init(&key, &key_data, sizeof(key_data));
        dbt_init(&val, &val_data, sizeof(val_data));
        CHK(db->put(db, write_txn, &key, &val, 0));
    }
    r = write_txn->commit(write_txn, 0);
    CKERR(r);
    for (int i = 0; i < 2; i++) {
        DB_TXN* read_txn = NULL;
        r = env->txn_begin(env, NULL, &read_txn, 0);
        CKERR(r);
        DBC* cursor = NULL;
        r = db->cursor(db, read_txn, &cursor, 0);
        CKERR(r);
        if (i == 0) { 
            r = cursor->c_pre_acquire_range_lock(
                cursor,
                db->dbt_neg_infty(),
                db->dbt_pos_infty()
                );
            CKERR(r);
        }
        while(r != DB_NOTFOUND) {
            DBT key, val;
            memset(&key, 0, sizeof(key));
            memset(&val, 0, sizeof(val));
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
            assert(r == 0 || r == DB_NOTFOUND);
        }
        r = cursor->c_close(cursor);
        CKERR(r);
        r = read_txn->commit(read_txn, 0);
        CKERR(r);
    }
}

static void run_test(void) {
    DB* db = NULL;
    int r;
    cmp_desc_is_four = TRUE;

    DBT orig_desc;
    memset(&orig_desc, 0, sizeof(orig_desc));
    orig_desc.size = sizeof(four_byte_desc);
    orig_desc.data = &four_byte_desc;

    DBT other_desc;
    memset(&other_desc, 0, sizeof(other_desc));
    other_desc.size = sizeof(eight_byte_desc);
    other_desc.data = &eight_byte_desc;

    DB_LOADER *loader = NULL;    
    DBT key, val;
    u_int64_t k = 0;
    u_int64_t v = 0;
    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            CHK(db_create(&db, env, 0));
            assert(db->descriptor == NULL);
            r = db->set_pagesize(db, 2048);
            CKERR(r);
            r = db->set_readpagesize(db, 1024);
            CKERR(r);
            CHK(db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
            assert(db->descriptor->dbt.size == 0);
            assert(db->cmp_descriptor->dbt.size == 0);
            CHK(db->change_descriptor(db, txn_create, &orig_desc, DB_UPDATE_CMP_DESCRIPTOR));
            assert_desc_four(db);
            assert_cmp_desc_valid(db);
            r = env->create_loader(env, txn_create, &loader, db, 1, &db, NULL, NULL, 0); 
            CKERR(r);
	    dbt_init(&key, &k, sizeof k);
	    dbt_init(&val, &v, sizeof v);
            r = loader->put(loader, &key, &val); 
            CKERR(r);
            r = loader->close(loader);
            CKERR(r);
            assert_cmp_desc_valid(db);    
        });
    assert_cmp_desc_valid(db);    
    CKERR(r);
    do_inserts_and_queries(db);
    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
        CHK(db->change_descriptor(db, txn_1, &other_desc, 0));
        assert_desc_eight(db);
        assert_cmp_desc_valid(db);
    });
    assert_desc_eight(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);

    IN_TXN_ABORT(env, NULL, txn_1, 0, {
        CHK(db->change_descriptor(db, txn_1, &orig_desc, 0));
        assert_desc_four(db);
        assert_cmp_desc_valid(db);
    });
    assert_desc_eight(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);
    
    CHK(db->close(db, 0));

    // verify that after close and reopen, cmp_descriptor is now
    // latest descriptor
    cmp_desc_is_four = FALSE;
    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666));
    assert_desc_eight(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);
    CHK(db->close(db, 0));

    cmp_desc_is_four = TRUE;
    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666));
    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
        CHK(db->change_descriptor(db, txn_1, &orig_desc, DB_UPDATE_CMP_DESCRIPTOR));
        assert_desc_four(db);
        assert_cmp_desc_valid(db);
    });
    assert_desc_four(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);
    CHK(db->close(db, 0));
    
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}

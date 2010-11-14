#include "test.h"

// verify that update_multiple where we only change key0

static int
get_key(int i, int dbnum) {
    return htonl(i + dbnum);
}

static void
get_data(int *v, int i, int ndbs) {
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        v[dbnum] = get_key(i, dbnum);
    }
}

static int
put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_data = dest_data; src_key = src_key; src_data = src_data;
    assert(src_db == NULL);

    unsigned int dbnum;
    assert(dest_db->descriptor->dbt.size == sizeof dbnum);
    memcpy(&dbnum, dest_db->descriptor->dbt.data, sizeof dbnum);
    assert(dbnum < src_data->size / sizeof (int));

    int *pri_key = (int *) src_key->data;
    int *pri_data = (int *) src_data->data;

    switch (dest_key->flags) {
    case 0:
        dest_key->size = sizeof (int);
        dest_key->data = dbnum == 0 ? &pri_key[dbnum] : &pri_data[dbnum];
        break;
    case DB_DBT_REALLOC:
        dest_key->size = sizeof (int);
        dest_key->data = toku_realloc(dest_key->data, dest_key->size);
        memcpy(dest_key->data, dbnum == 0 ? &pri_key[dbnum] : &pri_data[dbnum], dest_key->size);
        break;
    default:
        assert(0);
    }

    if (dest_data) {
        switch (dest_data->flags) {
        case 0:
            if (dbnum == 0) {
                dest_data->size = src_data->size;
                dest_data->data = src_data->data;
            } else
                dest_data->size = 0;
            break;
        case DB_DBT_REALLOC:
            if (dbnum == 0) {
                dest_data->size = src_data->size;
                dest_data->data = toku_realloc(dest_data->data, dest_data->size);
                memcpy(dest_data->data, src_data->data, dest_data->size);
            } else
                dest_data->size = 0;
            break;
        default:
            assert(0);
        }
    }
    
    return 0;
}

static int
del_callback(DB *dest_db, DB *src_db, DBT *dest_key, const DBT *src_key, const DBT *src_data) {
    return put_callback(dest_db, src_db, dest_key, NULL, src_key, src_data);
}

static void
verify_locked(DB_ENV *env, DB *db, int k) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    DBT key; dbt_init(&key, &k, sizeof k);
    r = db->del(db, txn, &key, DB_DELETE_ANY); assert(r == DB_LOCK_NOTGRANTED);
    r = txn->abort(txn); assert_zero(r);
}

#if 0
static void
verify_empty(DB_ENV *env, DB *db) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    int i;
    for (i = 0; ; i++) {
        DBT key; memset(&key, 0, sizeof key);
        DBT val; memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0)
            break;
    }
    assert_zero(i);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}
#endif

static void
verify_seq(DB_ENV *env, DB *db, int dbnum, int ndbs, int nrows) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    int i;
    for (i = 0; ; i++) {
        DBT key; memset(&key, 0, sizeof key);
        DBT val; memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0)
            break;
        int k;
        int expectk = dbnum == 0 ? get_key(i + nrows, dbnum) : get_key(i, dbnum);
        assert(key.size == sizeof k);
        memcpy(&k, key.data, key.size);
        assert(k == expectk);

        if (dbnum == 0) {
            assert(val.size == ndbs * sizeof (int));
            int v[ndbs]; get_data(v, i, ndbs);
            assert(memcmp(val.data, v, val.size) == 0);
        } else
            assert(val.size == 0);
    }
    assert(i == nrows);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
update_key0(DB_ENV *env, DB *db[], int ndbs, int nrows) {
    assert(ndbs > 0);
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    for (int i = 0; i < nrows; i++) {

        // update where new key0 = old key0 + nrows

        int k = get_key(i, 0);
        DBT old_key; dbt_init(&old_key, &k, sizeof k);
        int newk = get_key(i + nrows, 0);
        DBT new_key; dbt_init(&new_key, &newk, sizeof newk);

        int v[ndbs]; get_data(v, i, ndbs);
        DBT old_data; dbt_init(&old_data, &v[0], sizeof v);
        DBT new_data = old_data;
      
        int ndbts = 2 * ndbs;
        DBT keys[ndbts]; memset(keys, 0, sizeof keys);
        DBT vals[ndbts]; memset(vals, 0, sizeof vals);
        uint32_t flags_array[ndbs]; memset(flags_array, 0, sizeof(flags_array));

        r = env->update_multiple(env, NULL, txn, &old_key, &old_data, &new_key, &new_data, ndbs, db, flags_array, ndbts, keys, ndbts, vals);
        assert_zero(r);

        verify_locked(env, db[0], k);
        verify_locked(env, db[0], newk);
    }
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
populate_primary(DB_ENV *env, DB *db, int ndbs, int nrows) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = get_key(i, 0);
        int v[ndbs]; get_data(v, i, ndbs);
        DBT key; dbt_init(&key, &k, sizeof k);
        DBT val; dbt_init(&val, &v[0], sizeof v);
        r = db->put(db, txn, &key, &val, DB_YESOVERWRITE); assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);
}

static void
populate_secondary(DB_ENV *env, DB *db, int dbnum, int nrows) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = get_key(i, dbnum);
        DBT key; dbt_init(&key, &k, sizeof k);
        DBT val; dbt_init(&val, NULL, 0);
        r = db->put(db, txn, &key, &val, DB_YESOVERWRITE); assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);
}

static void
run_test(int ndbs, int nrows) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_put(env, put_callback); assert_zero(r);
    r = env->set_generate_row_callback_for_del(env, del_callback); assert_zero(r);

    r = env->open(env, ENVDIR, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *db[ndbs];
    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        r = db_create(&db[dbnum], env, 0); assert_zero(r);

        DBT dbt_dbnum; dbt_init(&dbt_dbnum, &dbnum, sizeof dbnum);
        r = db[dbnum]->set_descriptor(db[dbnum], 1, &dbt_dbnum); assert_zero(r);

        char dbname[32]; sprintf(dbname, "%d.tdb", dbnum);
        r = db[dbnum]->open(db[dbnum], NULL, dbname, NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);
    }

    for (int dbnum = 0; dbnum < ndbs; dbnum++) {
        if (dbnum == 0)
            populate_primary(env, db[dbnum], ndbs, nrows);
        else
            populate_secondary(env, db[dbnum], dbnum, nrows);
    }

    update_key0(env, db, ndbs, nrows);
    for (int dbnum = 0; dbnum < ndbs; dbnum++) 
        verify_seq(env, db[dbnum], dbnum, ndbs, nrows);

    for (int dbnum = 0; dbnum < ndbs; dbnum++) 
        r = db[dbnum]->close(db[dbnum], 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;
    int ndbs = 2;
    int nrows = 2;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(arg, "--ndbs") == 0 && i+1 < argc) {
            ndbs = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
    }

    r = system("rm -rf " ENVDIR); assert_zero(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    run_test(ndbs, nrows);

    return 0;
}


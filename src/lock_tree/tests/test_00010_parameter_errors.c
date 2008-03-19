/* We are going to test whether create and close properly check their input. */

#include "test.h"

static DBT _key;
static DBT _data;
DBT* key;
DBT* data;
u_int32_t max_locks = 1000;
toku_ltm* ltm = NULL;

static void do_range_test(int (*acquire)(toku_lock_tree*, DB_TXN*,
                                         const DBT*, const DBT*,
                                         const DBT*, const DBT*)) {
    int r;
    toku_lock_tree* lt  = NULL;
    DB*             db  = (DB*)1;
    DB_TXN*         txn = (DB_TXN*)1;  // Fake.
    BOOL duplicates = FALSE;
    DBT _key_l  = _key;
    DBT _key_r  = _key;
    DBT _data_l = _data;
    DBT _data_r = _data;
    DBT* key_l  = &_key_l;
    DBT* key_r  = &_key_r;
    DBT* data_l;
    DBT* data_r;
    DBT* reverse_data_l;
    DBT* reverse_data_r;
    for (duplicates = 0; duplicates < 2; duplicates++) {
        if (duplicates) {
            data_l         = &_data_l;
            data_r         = &_data_r;
            reverse_data_l = NULL;
            reverse_data_r = NULL;
        }
        else {
            data_l         = NULL;
            data_r         = NULL;
            reverse_data_l = &_data_l;
            reverse_data_r = &_data_r;
        }
        r = toku_lt_create(&lt, db, duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR(r);
        assert(lt);

        if (acquire == toku_lt_acquire_range_write_lock) {
            r = acquire(lt,   txn,  key_l,  data_l,
                                    key_r,  data_r);
            CKERR2(r, ENOSYS);
        }


        r = acquire(NULL,   txn,  key_l,  data_l,
                                  key_r,  data_r);
        CKERR2(r, EINVAL);
        r = acquire(lt,     NULL, key_l,  data_l,
                                  key_r,  data_r);
        CKERR2(r, EINVAL);
        r = acquire(lt,     txn,  NULL,   data_l,
                                  key_r,  data_r);
        CKERR2(r, EINVAL);
        if (duplicates) {
            r = acquire(lt,     txn,  key_l,  reverse_data_l,
                                      key_r,  data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt,     txn,  key_l,  data_l,
                                      key_r,  reverse_data_r);
            CKERR2(r, EINVAL);
        }
        r = acquire(lt,     txn,  key_l,  data_l,
                                  NULL,   data_r);
        CKERR2(r, EINVAL);

        /* Infinite tests. */
        if (duplicates) {
            r = acquire(lt, txn,  toku_lt_infinity,       data_l,
                                  key_r,                  data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  toku_lt_neg_infinity,   data_l,
                                  key_r,                  data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  key_l,                  data_l,
                                  toku_lt_infinity,       data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  key_l,                  data_l,
                                  toku_lt_neg_infinity,   data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  toku_lt_infinity,       toku_lt_neg_infinity,
                                  key_r,                  data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  toku_lt_neg_infinity,   toku_lt_infinity,
                                  key_r,                  data_r);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  key_l,                  data_l,
                                  toku_lt_infinity,       toku_lt_neg_infinity);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn,  key_l,                  data_l,
                                  toku_lt_neg_infinity,   toku_lt_infinity);
            CKERR2(r, EINVAL);
        }
        /* left > right tests. */
        const DBT* d_inf     = duplicates ? toku_lt_infinity      : NULL;
        const DBT* inf       =              toku_lt_infinity;
        const DBT* d_ninf    = duplicates ? toku_lt_neg_infinity  : NULL;
        const DBT* ninf      =              toku_lt_neg_infinity;
        r = acquire(lt,     txn,  inf,    d_inf,
                                  key_r,  data_r);
        CKERR2(r, EDOM);
        r = acquire(lt,     txn,  key_l,  data_l,
                                  ninf,   d_ninf);
        CKERR2(r, EDOM);
        r = acquire(lt,     txn,  inf,    d_inf,
                                  ninf,   d_ninf);
        CKERR2(r, EDOM);

        /* Cleanup. */
        r = toku_lt_close(lt);
        CKERR(r);

        lt = NULL;
    }
}

static void do_point_test(int (*acquire)(toku_lock_tree*, DB_TXN*,
                                         const DBT*, const DBT*)) {
    int r;
    toku_lock_tree* lt  = NULL;
    DB*             db  = (DB*)1;
    DB_TXN*         txn = (DB_TXN*)1;  // Fake.
    BOOL duplicates = FALSE;

    lt = NULL;
    DBT* reverse_data;

    /* Point read tests. */
    key  = &_key;
    for (duplicates = 0; duplicates < 2; duplicates++) {
        if (duplicates) {
            data         = &_data;
            reverse_data = NULL;
        }
        else {
            reverse_data = &_data;
            data         = NULL;
        }
        r = toku_lt_create(&lt, db, duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR(r);
        assert(lt);

        r = toku_lt_unlock(NULL, (DB_TXN*)1);
        CKERR2(r, EINVAL);

        r = toku_lt_unlock(lt, NULL);
        CKERR2(r, EINVAL);

        r = acquire(NULL, txn,  key,  data);
        CKERR2(r, EINVAL);

        r = acquire(lt,   NULL, key,  data);
        CKERR2(r, EINVAL);

        r = acquire(lt,   txn,  NULL, data);
        CKERR2(r, EINVAL);

        if (duplicates) {
            r = acquire(lt,   txn,  key,  reverse_data);
            CKERR2(r, EINVAL);
        }

        /* Infinite tests. */
        if (duplicates) {
            r = acquire(lt, txn, toku_lt_infinity,     data);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn, toku_lt_neg_infinity, data);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn, toku_lt_infinity, toku_lt_neg_infinity);
            CKERR2(r, EINVAL);
            r = acquire(lt, txn, toku_lt_neg_infinity, toku_lt_infinity);
            CKERR2(r, EINVAL);
        }

        /* Cleanup. */
        r = toku_lt_close(lt);
        CKERR(r);

        lt = NULL;
    }
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    int r;
    toku_lock_tree* lt  = NULL;
    DB*             db  = (DB*)1;
    BOOL duplicates = FALSE;

    r = toku_ltm_create(NULL, max_locks, toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);
        assert(ltm == NULL);
    r = toku_ltm_create(&ltm, 0,         toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);
        assert(ltm == NULL);
    r = toku_ltm_create(&ltm, max_locks, NULL,        toku_free, toku_realloc);
        CKERR2(r, EINVAL);
        assert(ltm == NULL);
    r = toku_ltm_create(&ltm, max_locks, toku_malloc, NULL,      toku_realloc);
        CKERR2(r, EINVAL);
        assert(ltm == NULL);
    r = toku_ltm_create(&ltm, max_locks, toku_malloc, toku_free, NULL);
        CKERR2(r, EINVAL);
        assert(ltm == NULL);

    /* Actually create it. */
    r = toku_ltm_create(&ltm, max_locks, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(ltm);

    r = toku_ltm_set_max_locks(NULL, max_locks);
        CKERR2(r, EINVAL);
    r = toku_ltm_set_max_locks(ltm,  0);
        CKERR2(r, EINVAL);
    r = toku_ltm_set_max_locks(ltm,  max_locks);
        CKERR(r);

    u_int32_t get_max = 73; //Some random number that isn't 0.
    r = toku_ltm_get_max_locks(NULL, &get_max);
        CKERR2(r, EINVAL);
        assert(get_max == 73);
    r = toku_ltm_get_max_locks(ltm,  NULL);
        CKERR2(r, EINVAL);
        assert(get_max == 73);
    r = toku_ltm_get_max_locks(ltm,  &get_max);
        CKERR(r);
        assert(get_max == max_locks);

    /* create tests. */
    for (duplicates = 0; duplicates < 2; duplicates++) {
        r = toku_lt_create(NULL, db,   duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  NULL, duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  db,   duplicates, NULL,    ltm,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  db,   duplicates, dbpanic, NULL,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  db,   duplicates, dbpanic, ltm,
                           NULL,  dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  db,   duplicates, dbpanic, ltm,
                           dbcmp, NULL,  toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);
        r = toku_lt_create(&lt,  db,   duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, NULL,        toku_free, toku_realloc);
        CKERR2(r, EINVAL);
        r = toku_lt_create(&lt,  db,   duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, toku_malloc, NULL,      toku_realloc);
        CKERR2(r, EINVAL);
        r = toku_lt_create(&lt,  db,   duplicates, dbpanic, ltm,
                           dbcmp, dbcmp, toku_malloc, toku_free, NULL);
        CKERR2(r, EINVAL);
    }

    /* Close tests. */
    r = toku_lt_close(NULL);
    CKERR2(r, EINVAL);

    do_point_test(toku_lt_acquire_read_lock);
    do_point_test(toku_lt_acquire_write_lock);

    do_range_test(toku_lt_acquire_range_read_lock);
    do_range_test(toku_lt_acquire_range_write_lock);

    return 0;
}

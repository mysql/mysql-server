/* We are going to test whether create and close properly check their input. */

#include "test.h"

enum { MAX_LOCKS = 1000, MAX_LOCK_MEMORY = MAX_LOCKS * 64 };

static void do_ltm_status(toku_ltm *ltm) {
    uint32_t max_locks, curr_locks;
    uint64_t max_lock_memory, curr_lock_memory;
    LTM_STATUS_S s;
    toku_ltm_get_status(ltm, &max_locks, &curr_locks, &max_lock_memory, &curr_lock_memory, &s);
    assert(max_locks == MAX_LOCKS);
    assert(curr_locks == 0);
    assert(max_lock_memory == MAX_LOCK_MEMORY);
    assert(curr_lock_memory == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);

    int r;

    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, MAX_LOCKS, MAX_LOCK_MEMORY, dbpanic,
                        get_compare_fun_from_db,
                        toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    do_ltm_status(ltm);
#if 0
    r = toku_ltm_set_max_locks(NULL, max_locks);
        CKERR2(r, EINVAL);
    r = toku_ltm_set_max_locks(ltm,  0);
        CKERR2(r, EINVAL);
    r = toku_ltm_set_max_locks(ltm,  max_locks);
        CKERR(r);

    uint32_t get_max = 73; //Some random number that isn't 0.
    r = toku_ltm_get_max_locks(NULL, &get_max);
        CKERR2(r, EINVAL);
        assert(get_max == 73);
    r = toku_ltm_get_max_locks(ltm,  NULL);
        CKERR2(r, EINVAL);
        assert(get_max == 73);
    r = toku_ltm_get_max_locks(ltm,  &get_max);
        CKERR(r);
        assert(get_max == max_locks);

    r = toku_ltm_set_max_lock_memory(NULL, max_lock_memory);
        CKERR2(r, EINVAL);
    r = toku_ltm_set_max_lock_memory(ltm,  0);
        CKERR2(r, EINVAL);
    r = toku_ltm_set_max_lock_memory(ltm,  max_lock_memory);
        CKERR(r);

    uint64_t get_max_memory = 73; //Some random number that isn't 0.
    r = toku_ltm_get_max_lock_memory(NULL, &get_max_memory);
        CKERR2(r, EINVAL);
        assert(get_max_memory == 73);
    r = toku_ltm_get_max_lock_memory(ltm,  NULL);
        CKERR2(r, EINVAL);
        assert(get_max_memory == 73);
    r = toku_ltm_get_max_lock_memory(ltm,  &get_max_memory);
        CKERR(r);
        assert(get_max_memory == max_lock_memory);

    /* create tests. */
    {
        r = toku_lt_create(NULL, dbpanic, ltm,
                           get_compare_fun_from_db,
                           toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  NULL,    ltm,
                           get_compare_fun_from_db,
                           toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  dbpanic, NULL,
                           get_compare_fun_from_db,
                           toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  dbpanic, ltm,
                           NULL,
                           toku_malloc, toku_free, toku_realloc);
        CKERR2(r, EINVAL);

        r = toku_lt_create(&lt,  dbpanic, ltm,
                           get_compare_fun_from_db,
                           NULL,        toku_free, toku_realloc);
        CKERR2(r, EINVAL);
        r = toku_lt_create(&lt,  dbpanic, ltm,
                           get_compare_fun_from_db,
                           toku_malloc, NULL,      toku_realloc);
        CKERR2(r, EINVAL);
        r = toku_lt_create(&lt,  dbpanic, ltm,
                           get_compare_fun_from_db,
                           toku_malloc, toku_free, NULL);
        CKERR2(r, EINVAL);
    }

    /* Close tests. */
    r = toku_lt_close(NULL);
    CKERR2(r, EINVAL);

    do_point_test(toku_lt_acquire_read_lock);
    do_point_test(toku_lt_acquire_write_lock);

    do_range_test(toku_lt_acquire_range_read_lock);
    do_range_test(toku_lt_acquire_range_write_lock);
#endif

    toku_ltm_close(ltm);

    return 0;
}

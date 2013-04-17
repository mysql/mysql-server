// benchmark point write locks acquisition rate.
// rate = nrows / time to execute the benchmark.
//
// example: ./benchmark_point_write_locks.tlog --max_locks 1000000 --max_lock_memory 1000000000 --nrows 1000000

#define TOKU_ALLOW_DEPRECATED
#include <malloc.h>
#include "test.h"
#include <byteswap.h>

static uint64_t htonl64(uint64_t x) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return bswap_64(x);
#else
#error
#endif
}

struct my_ltm_status {
    uint32_t max_locks, curr_locks;
    uint64_t max_lock_memory, curr_lock_memory;
    LTM_STATUS_S status;
};

static void my_ltm_get_status(toku_ltm *ltm, struct my_ltm_status *my_status) {
    toku_ltm_get_status(ltm, &my_status->max_locks, &my_status->curr_locks, &my_status->max_lock_memory, &my_status->curr_lock_memory, &my_status->status);
}

static void *my_malloc(size_t s) {
    void * p = malloc(s);
    if (verbose) 
        printf("%s %lu %lu\n", __FUNCTION__, s, malloc_usable_size(p));
    return p;
}

static void *my_realloc(void *p, size_t s) {
    if (verbose)
        printf("%s %p %lu\n", __FUNCTION__, p, s);
    return realloc(p, s);
}

static void my_free(void *p) {
    if (verbose) 
        printf("%s %p %lu\n", __FUNCTION__, p, malloc_usable_size(p));
    free(p);
}


int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;
    uint64_t nrows = 1;
    bool do_malloc_trace = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--malloc") == 0) {
            do_malloc_trace = true;
            continue;
        }
        assert(0);
    }


    if (do_malloc_trace) {
        toku_set_func_malloc(my_malloc);
        toku_set_func_free(my_free);
        toku_set_func_realloc(my_realloc);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    assert(r == 0 && ltm);
    
    struct my_ltm_status s;
    my_ltm_get_status(ltm, &s);
    assert(s.max_locks == max_locks);
    assert(s.curr_locks == 0);
    assert(s.max_lock_memory == max_lock_memory);
    assert(s.curr_lock_memory == 0);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db);
    assert(r == 0 && lt);

    DB *db_a = (DB *) 2;
    TXNID txn_a = 1;

    // acquire the locks on keys 1 .. nrows
    for (uint64_t i = 1; i <= nrows; i++) {
        uint64_t k_left = htonl64(2*i);
        uint64_t k_right = htonl64(2*i+1);
        DBT key_left = { .data = &k_left, .size = sizeof k_left };
        DBT key_right = { .data = &k_right, .size = sizeof k_right };
        r = toku_lt_acquire_range_write_lock(lt, db_a, txn_a, &key_left, &key_right);
        if (r != 0) {
            assert(r == TOKUDB_OUT_OF_LOCKS);
            break;
        }

        struct my_ltm_status t;
        my_ltm_get_status(ltm, &t);
        assert(t.max_locks == max_locks);
        assert(t.curr_locks == i);
        assert(t.max_lock_memory == max_lock_memory);
        assert(t.curr_lock_memory > s.curr_lock_memory);
        
        if (verbose)
            printf("%"PRIu64" %"PRIu64"\n", i, t.curr_lock_memory - s.curr_lock_memory);
        
        s = t;
    }
    

    // release the locks
    r = toku_lt_unlock(lt, txn_a);  assert(r == 0);

    my_ltm_get_status(ltm, &s);
    assert(s.curr_locks == 0);

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}

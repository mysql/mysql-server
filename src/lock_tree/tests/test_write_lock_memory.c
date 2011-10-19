// benchmark point write locks acquisition rate.
// rate = nrows / time to execute the benchmark.
//
// example: ./benchmark_point_write_locks.tlog --max_locks 1000000 --max_lock_memory 1000000000 --nrows 1000000

#include "test.h"

struct my_ltm_status {
    uint32_t max_locks, curr_locks;
    uint64_t max_lock_memory, curr_lock_memory;
    LTM_STATUS_S status;
};

static void my_ltm_get_status(toku_ltm *ltm, struct my_ltm_status *my_status) {
    toku_ltm_get_status(ltm, &my_status->max_locks, &my_status->curr_locks, &my_status->max_lock_memory, &my_status->curr_lock_memory, &my_status->status);
}

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;
    uint64_t nrows = 1;

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
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && ltm);
    
    struct my_ltm_status s;
    my_ltm_get_status(ltm, &s);
    assert(s.max_locks == max_locks);
    assert(s.curr_locks == 0);
    assert(s.max_lock_memory == max_lock_memory);
    assert(s.curr_lock_memory == 0);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && lt);

    DB *db_a = (DB *) 2;
    TXNID txn_a = 1;

    // acquire the locks on keys 1 .. nrows
    for (uint64_t k = 1; k <= nrows; k++) {
        DBT key = { .data = &k, .size = sizeof k };
        r = toku_lt_acquire_write_lock(lt, db_a, txn_a, &key);
        if (r != 0) {
            assert(r == TOKUDB_OUT_OF_LOCKS);
            break;
        }

        struct my_ltm_status t;
        my_ltm_get_status(ltm, &t);
        assert(t.max_locks == max_locks);
        assert(t.curr_locks == k);
        assert(t.max_lock_memory == max_lock_memory);
        assert(t.curr_lock_memory > s.curr_lock_memory);
        
        if (verbose)
            printf("%"PRIu64" %"PRIu64"\n", k, t.curr_lock_memory);
        
        s = t;
    }
    

    // release the locks
    r = toku_lt_unlock(lt, txn_a);  assert(r == 0);

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}

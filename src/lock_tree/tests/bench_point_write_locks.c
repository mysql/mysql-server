// benchmark point write locks acquisition rate.
// rate = nrows / time to execute the benchmark.
//
// example: ./benchmark_point_write_locks.tlog --max_locks 1000000 --max_lock_memory 1000000000 --nrows 1000000

#include "test.h"

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
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db);
    assert(r == 0 && lt);

    DB *db_a = (DB *) 2;
    TXNID txn_a = 1;

    // acquire the locks on keys 0 .. nrows-1
    for (uint64_t k = 0; k < nrows; k++) {
        DBT key = { .data = &k, .size = sizeof k };
        r = toku_lt_acquire_write_lock(lt, db_a, txn_a, &key); assert(r == 0);
    }

    // release the locks
    r = toku_lt_unlock(lt, txn_a);  assert(r == 0);

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}

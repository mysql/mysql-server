// T(A) gets W(L)
// T(B) tries W(L), gets DB_LOCK_NOTGRANTED
// T(A) releases locks
// T(B) gets W(L)
// T(B) releases locks

#include "test.h"

static int write_lock(toku_lock_tree *lt, TXNID txnid, char *k) {
    DBT key; dbt_init(&key, k, strlen(k));
    int r = toku_lt_acquire_write_lock(lt, (DB*)1, txnid, &key);
    return r;
}

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 1;
    uint64_t max_lock_memory = 4096;

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
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;
    const TXNID txn_b = 2;

    r = write_lock(lt, txn_a, "L"); assert(r == 0);
    r = write_lock(lt, txn_b, "L"); assert(r == DB_LOCK_NOTGRANTED);
    r = toku_lt_unlock(lt, txn_a);  assert(r == 0);
    r = write_lock(lt, txn_b, "L"); assert(r == 0);
    r = toku_lt_unlock(lt, txn_b);  assert(r == 0);

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}

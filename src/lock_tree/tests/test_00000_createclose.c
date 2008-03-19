#include <test.h>

int main() {
    int r;
    toku_lock_tree* lt  = NULL;
    toku_ltm*       mgr = NULL;
    DB* db = (DB*)1;
    u_int32_t max_locks = 1000;
    BOOL duplicates;

    r = toku_ltm_create(&mgr, max_locks, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    
    for (duplicates = 0; duplicates < 2; duplicates++) {
        r = toku_lt_create(&lt, db, duplicates, dbpanic, mgr,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR(r);
        assert(lt);
        r = toku_lt_close(lt);
        CKERR(r);
        lt = NULL;
    }

    r = toku_ltm_close(mgr);
    CKERR(r);
    mgr = NULL;

    return 0;
}

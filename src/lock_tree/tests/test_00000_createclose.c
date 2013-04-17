#include <test.h>

int main() {
    int r;
    toku_lock_tree* lt  = NULL;
    toku_ltm*       mgr = NULL;
    u_int32_t max_locks = 1000;
    BOOL duplicates;

    r = toku_ltm_create(&mgr, max_locks, dbpanic,
                        get_compare_fun_from_db, get_dup_compare_from_db,
                        toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    
    for (duplicates = FALSE; duplicates < TRUE; duplicates++) {
        r = toku_lt_create(&lt, duplicates, dbpanic, mgr,
                           get_compare_fun_from_db, get_dup_compare_from_db,
                           toku_malloc, toku_free, toku_realloc);
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

#include <test.h>

int main() {
    int r;
    toku_lock_tree* lt = NULL;
    DB* db = (DB*)1;
    u_int32_t max_locks = 1000;
    u_int32_t memcnt = 0;
    BOOL duplicates;

    for (duplicates = 0; duplicates < 2; duplicates++) {
        r = toku_lt_create(&lt, db, duplicates, dbpanic, max_locks, &memcnt,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR(r);
        assert(lt);
        r = toku_lt_close(lt);
        CKERR(r);
        lt = NULL;
    }

    return 0;
}

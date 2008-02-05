#include <test.h>

int main() {
    int r;
    toku_lock_tree* lt = NULL;
    DB* db = (DB*)1;
    u_int32_t mem = 4096 * 1000;
    u_int32_t memcnt;
    BOOL duplicates;

    for (duplicates = 0; duplicates < 2; duplicates++) {
        r = toku_lt_create(&lt, db, duplicates, dbpanic, mem, &memcnt,
                           dbcmp, dbcmp, toku_malloc, toku_free, toku_realloc);
        CKERR(r);
        assert(lt);
        r = toku_lt_close(lt);
        CKERR(r);
        lt = NULL;
    }

    return 0;
}

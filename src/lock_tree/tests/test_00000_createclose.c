#include <test.h>

int main() {
    int r;
    toku_lock_tree* lt = NULL;
    DB* db = (DB*)1;
    size_t mem = 4096 * 1000;
    BOOL duplicates;

    for (duplicates = 0; duplicates < 2; duplicates++) {
        r = toku_lt_create(&lt, db, duplicates, mem, dbcmp, dbcmp,
                           toku_malloc, toku_free, toku_realloc);
        CKERR(r);
        assert(lt);
        r = toku_lt_close(lt);
        CKERR(r);
        lt = NULL;
    }

    return 0;
}

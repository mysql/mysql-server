/* Test for a memory leak from just closing the lock tree manager (should close
   all lock trees. */

#include "test.h"

toku_range_tree* toku__lt_ifexist_selfwrite(toku_lock_tree* tree, DB_TXN* txn);
toku_range_tree* toku__lt_ifexist_selfread(toku_lock_tree* tree, DB_TXN* txn);

int r;
toku_lock_tree* lt  = NULL;
toku_ltm*       ltm = NULL;
DB*             db  = (DB*)1;
u_int32_t max_locks = 10;
BOOL duplicates = FALSE;
int  nums[10000];

void setup_tree(BOOL dups) {
    assert(!lt && !ltm);
    r = toku_ltm_create(&ltm, max_locks, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(ltm);
    r = toku_lt_create(&lt, db, dups, dbpanic, ltm, intcmp, intcmp,
                       toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lt);
}

void close_ltm(void) {
    assert(lt && ltm);
    r = toku_ltm_close(ltm);
        CKERR(r);
    lt = NULL;
    ltm = NULL;
}

void run_test(BOOL dups) {
    setup_tree(dups);
    close_ltm();    
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);


    run_test(FALSE);
    run_test(TRUE);

    return 0;
}

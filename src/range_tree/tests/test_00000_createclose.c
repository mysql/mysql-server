/* We are going to test whether we can create and close range trees */

#include "test.h"

static void test_create_close(BOOL allow_overlaps) {
    int r;
#ifdef TOKU_RT_NOOVERLAPS
    if (allow_overlaps) return;
#endif
    toku_range_tree *tree = NULL;
    r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    
    assert(tree!=NULL);
    BOOL temp;
    r = toku_rt_get_allow_overlaps(tree, &temp);
    CKERR(r);
    assert((temp != 0) == (allow_overlaps != 0));
    
    r = toku_rt_close(tree);
    CKERR(r);
    tree = NULL;
}

static void test_create_close_nomem(BOOL allow_overlaps) {
#ifdef TOKU_RT_NOOVERLAPS
    if (allow_overlaps) return;
#endif
    for (int i = 1; i <= 1; i++) {
        mallocced = 0;
        failon = i;
        toku_range_tree *tree = NULL;
        int r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps,
                               fail_malloc, toku_free, toku_realloc);
        CKERR2(r, ENOMEM);

        assert(tree==NULL);
    }
}


int main(int argc, const char *argv[]) {

    parse_args(argc, argv);

    test_create_close(false);
    test_create_close(true);

    test_create_close_nomem(false);
    test_create_close_nomem(true);
    
    return 0;
}


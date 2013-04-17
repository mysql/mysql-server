/* We are going to test whether we can create and close range trees */

#include "test.h"

static void test_create_close(bool allow_overlaps) {
    int r;
#ifdef TOKU_RT_NOOVERLAPS
    if (allow_overlaps) return;
#endif
    toku_range_tree *tree = NULL;
    r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    
    assert(tree!=NULL);
    bool temp;
    r = toku_rt_get_allow_overlaps(tree, &temp);
    CKERR(r);
    assert((temp != 0) == (allow_overlaps != 0));
    
    r = toku_rt_close(tree);
    CKERR(r);
    tree = NULL;
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);

    test_create_close(false);
    test_create_close(true);
    
    return 0;
}


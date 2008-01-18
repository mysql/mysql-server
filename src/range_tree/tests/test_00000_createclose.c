/* We are going to test whether we can create and close range trees */

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree;

    parse_args(argc, argv);

    /* Test no overlap */
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, FALSE);
    CKERR(r);
    
    assert(tree!=NULL);

    r = toku_rt_close(tree);
    CKERR(r);

    tree = NULL;

    /* Test overlap */
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, TRUE);
    CKERR(r);

    assert(tree!=NULL);

    r = toku_rt_close(tree);
    CKERR(r);

    tree = NULL;

    return 0;
}

/* We are going to test whether close can clean up after itself. */

#include "test.h"

void run_test (BOOL overlap_allowed) {
    int r;
    toku_range_tree *tree;
    toku_range range;
    int nums[8] = {0,1,2,3,4,5,6,7};
    char letters[2] = {'A','B'};



    /* Test overlap case */
    /*
        1   2   3   4   5   6   7
        |---A-----------|
    */
    r = toku_rt_create(
        &tree, 
        int_cmp, 
        char_cmp, 
        overlap_allowed, 
        malloc, 
        free, 
        realloc
        );
    CKERR(r);

    range.left = (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[5];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

#ifndef TOKU_RT_NOOVERLAPS
    run_test(TRUE);
#endif
    run_test(FALSE);
    return 0;
}

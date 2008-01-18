/* We are going to test whether close can clean up after itself. */

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree;
    toku_range range;
    int nums[8] = {0,1,2,3,4,5,6,7};
    char letters[2] = {'A','B'};


    parse_args(argc, argv);

    /* Test overlap case */
    /*
        1   2   3   4   5   6   7
        |---A-----------|
    */
    r = toku_rt_create(&tree, int_cmp, char_cmp, TRUE);
    CKERR(r);

    range.left = &nums[1];
    range.right = &nums[5];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

    /* Test non-overlap case */
    /*
        1   2   3   4   5   6   7
        |---A-----------|
    */
    r = toku_rt_create(&tree, int_cmp, char_cmp, FALSE);
    CKERR(r);

    range.left = &nums[1];
    range.right = &nums[5];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

    return 0;
}

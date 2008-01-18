/* We are going to test whether we can insert and delete. */

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
        |-------A-------|
            |-------A-------|
                |-------A-------|
            |-------B-------|
    */
    r = toku_rt_create(&tree, int_cmp, char_cmp, TRUE);
    CKERR(r);

    range.left = &nums[1];
    range.right = &nums[5];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    /* Try and fail to insert exact same thing. */
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = &nums[3];
    range.right = &nums[7];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Try to delete again, make sure it fails. (Not there anymore) */
    r = toku_rt_delete(tree, &range);   CKERR2(r,EDOM);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Clean up. */
    range.left = &nums[1];
    range.right = &nums[5];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    range.left = &nums[3];
    range.right = &nums[7];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);
    /* Done */

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

    /* Test no overlap case. */
    /* Test overlap case */
    /*
        1   2   3   4   5   6   7
        |---A---|
                    |---B---|
    */
    r = toku_rt_create(&tree, int_cmp, char_cmp, TRUE);
    CKERR(r);

    range.left = &nums[1];
    range.right = &nums[3];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    /* Try and fail to insert exact same thing. */
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);

    /* Try (and fail) to insert an overlapping range in a nooverlap tree. */
    range.left = &nums[0];
    range.right = &nums[4];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);
    
    /* Try (and fail) to insert an overlapping range (different data) in a
       nooverlap tree. */
    range.left = &nums[0];
    range.right = &nums[4];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);
    
    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Try to delete again, make sure it fails. (Not there anymore) */
    r = toku_rt_delete(tree, &range);   CKERR2(r,EDOM);

    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = &nums[1];
    range.right = &nums[3];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Clean up. */
    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);
    /* Done */

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

    return 0;
}

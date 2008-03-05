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
    r = toku_rt_create(&tree, int_cmp, char_cmp, TRUE, malloc, free, realloc);
    CKERR(r);

    /* Verify we can insert a trivial range and lose it. */
    range.left =  (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[1];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);
    u_int32_t num_in_range;
    r = toku_rt_get_size(tree, &num_in_range); CKERR(r);
    assert(num_in_range == 1);
    r = toku_rt_delete(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[5];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    /* Try and fail to insert exact same thing. */
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);

    /* Try and succeed to insert (and delete) similar yet different things */
    range.right = (toku_point*)&nums[6];
    r = toku_rt_insert(tree, &range);   CKERR(r);
    r = toku_rt_delete(tree, &range);   CKERR(r);
    range.right = (toku_point*)&nums[5];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);
    r = toku_rt_delete(tree, &range);   CKERR(r);
    range.data = (DB_TXN*)&letters[0];

    range.left = (toku_point*)&nums[2];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[3];
    range.right = (toku_point*)&nums[7];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[2];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[2];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Try to delete again, make sure it fails. (Not there anymore) */
    r = toku_rt_delete(tree, &range);   CKERR2(r,EDOM);

    range.left = (toku_point*)&nums[2];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[2];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Clean up. */
    range.left = (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[5];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[2];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[3];
    range.right = (toku_point*)&nums[7];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);
    /* Done */

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

    /* Test no overlap case. */
    /*
        1   2   3   4   5   6   7
        |---A---|
                    |---B---|
    */
    r = toku_rt_create(&tree, int_cmp, char_cmp, FALSE, malloc, free, realloc);
    CKERR(r);

    /* Verify we can insert a trivial range and lose it. */
    range.left = (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[1];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);
    r = toku_rt_delete(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[3];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    /* Try and fail to insert exact same thing. */
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);

    /* Try (and fail) to insert an overlapping range in a nooverlap tree. */
    range.left = (toku_point*)&nums[0];
    range.right = (toku_point*)&nums[4];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);
    
    /* Try (and fail) to insert an overlapping range (different data) in a
       nooverlap tree. */
    range.left = (toku_point*)&nums[0];
    range.right = (toku_point*)&nums[4];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_insert(tree, &range);   CKERR2(r,EDOM);
    
    range.left = (toku_point*)&nums[4];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[4];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Try to delete again, make sure it fails. (Not there anymore) */
    r = toku_rt_delete(tree, &range);   CKERR2(r,EDOM);

    range.left = (toku_point*)&nums[4];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    range.left = (toku_point*)&nums[1];
    range.right = (toku_point*)&nums[3];
    range.data = (DB_TXN*)&letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    /* Clean up. */
    range.left = (toku_point*)&nums[4];
    range.right = (toku_point*)&nums[6];
    range.data = (DB_TXN*)&letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);
    /* Done */

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;

    return 0;
}

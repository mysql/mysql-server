/* We are going to test whether we can insert and delete. */

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree = NULL;
    toku_range range;
    toku_interval find_range;
    toku_interval find_all_range;
    int nums[1000];
    char letters[2] = {'A','B'};
    unsigned found;
    unsigned bufsize;
    toku_range* buf;
    unsigned j = 0;
    unsigned i;

    parse_args(argc, argv);

    for (j = 0; j < sizeof(nums)/sizeof(nums[0]); j++) {
        nums[j] = j;
    }

    find_range.left  = (toku_point*)&nums[4];
    find_range.right = (toku_point*)&nums[4];

    find_all_range.left  = (toku_point*)&nums[0];
    find_all_range.right = (toku_point*)&nums[sizeof(nums)/sizeof(nums[0]) - 1];

#ifndef TOKU_RT_NOOVERLAPS
    bufsize = 2;
    buf = (toku_range*)toku_malloc(bufsize*sizeof(toku_range));

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

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize == 2);

    range.ends.left = (toku_point*)&nums[1];
    range.ends.right = (toku_point*)&nums[5];
    range.data = (TXNID)letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize == 2);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[2];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize == 2);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[3];
    range.ends.right = (toku_point*)&nums[7];
    range.data = (TXNID)letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    /* Now that we find 3, we are testing that realloc works. */
    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 3);
    assert(bufsize >= 3);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[2];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 4);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);
    

    range.ends.left = (toku_point*)&nums[2];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 3);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    for (i = 0; i < found; i++) {
        assert(*(int*)buf[i].ends.left != 2 || 
               *(int*)buf[i].ends.right != 6 ||
               (char)buf[i].data == letters[1]);
    }

    range.ends.left = (toku_point*)&nums[2];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 4);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[2];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 3);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    for (i = 0; i < found; i++) {
        assert(*(int*)buf[i].ends.left != 2 || 
               *(int*)buf[i].ends.right != 6 ||
               (char)buf[i].data == letters[0]);
    }

    /* Clean up. */
    range.ends.left = (toku_point*)&nums[1];
    range.ends.right = (toku_point*)&nums[5];
    range.data = (TXNID)letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[2];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    assert(*(int*)buf[0].ends.left == 3 &&
           *(int*)buf[0].ends.right == 7 &&
           (char)buf[0].data == letters[0]);

    range.ends.left = (toku_point*)&nums[3];
    range.ends.right = (toku_point*)&nums[7];
    range.data = (TXNID)letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Done */

    r = toku_rt_close(tree);            CKERR(r);
    free(buf);
    buf = NULL;
#endif /* #ifdef TOKU_RT_NOOVERLAPS */


    tree = NULL;
    bufsize = 2;
    buf = (toku_range*)toku_malloc(bufsize*sizeof(toku_range));

    /* Test no overlap case. */
    /*
        1   2   3   4   5   6   7
        |---A---|
                    |---B---|
    */
    find_range.left  = (toku_point*)&nums[3];
    find_range.right = (toku_point*)&nums[4];
        
    r = toku_rt_create(&tree, int_cmp, char_cmp, FALSE, malloc, free, realloc);
    CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize >= 2);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[1];
    range.ends.right = (toku_point*)&nums[3];
    range.data = (TXNID)letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= 2);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[4];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize >= 2);
    verify_all_overlap(&find_range, buf, found);

    u_int32_t inserted = 2;
    const u_int32_t start_loop    = 100;
    const u_int32_t end_loop      = 200;
    for (i = start_loop; i < end_loop; i += 4) {
        range.ends.left  = (toku_point*)&nums[i];
        range.ends.right = (toku_point*)&nums[i+2];
        range.data  = (TXNID)letters[0];
        r = toku_rt_insert(tree, &range);   CKERR(r);
        inserted++;
    
        r = toku_rt_find(tree, &find_all_range, 0, &buf, &bufsize, &found);  CKERR(r);
        assert(found == inserted);
        assert(bufsize >= inserted);
    }
    for (i = start_loop; i < end_loop; i += 4) {
        range.ends.left  = (toku_point*)&nums[i];
        range.ends.right = (toku_point*)&nums[i+2];
        range.data  = (TXNID)letters[0];

        r = toku_rt_delete(tree, &range);   CKERR(r);
    }


    range.ends.left = (toku_point*)&nums[4];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= inserted);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    assert(*(int*)buf[0].ends.left == 1 && 
           *(int*)buf[0].ends.right == 3 &&
           (char)buf[0].data == letters[0]);

    range.ends.left = (toku_point*)&nums[4];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize >= inserted);
    verify_all_overlap(&find_range, buf, found);

    range.ends.left = (toku_point*)&nums[1];
    range.ends.right = (toku_point*)&nums[3];
    range.data = (TXNID)letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= inserted);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    assert(*(int*)buf[0].ends.left == 4 && 
           *(int*)buf[0].ends.right == 6 &&
           (char)buf[0].data == letters[1]);

    /* Clean up. */
    range.ends.left = (toku_point*)&nums[4];
    range.ends.right = (toku_point*)&nums[6];
    range.data = (TXNID)letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize >= inserted);
    verify_all_overlap(&find_range, buf, found);

    /* Done */

    r = toku_rt_close(tree);            CKERR(r);

    /* Cleanup. */
    tree = NULL;

    toku_free(buf);
    buf = NULL;
    return 0;
}

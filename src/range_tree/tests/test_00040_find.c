/* We are going to test whether we can insert and delete. */

#include "test.h"

void verify_all_overlap(toku_range* query, toku_range* list, unsigned listlen) {
    unsigned i;
    
    for (i = 0; i < listlen; i++) {
        /* Range A and B overlap iff A.left <= B.right && B.left <= A.right */
        assert(int_cmp(query->left, list[i].right) <= 0 &&
               int_cmp(list[i].left, query->right) <= 0);
    }
}

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree;
    toku_range range;
    toku_range find_range;
    int nums[8] = {0,1,2,3,4,5,6,7};
    char letters[2] = {'A','B'};
    unsigned found;
    toku_range* buf = (toku_range*)toku_malloc(2*sizeof(toku_range));
    unsigned bufsize = 2;
    int i;
    
    parse_args(argc, argv);

    find_range.left  = &nums[4];
    find_range.right = &nums[4];
    find_range.data  = NULL;

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

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize == 2);

    range.left = &nums[1];
    range.right = &nums[5];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize == 2);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize == 2);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[3];
    range.right = &nums[7];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    /* Now that we find 3, we are testing that realloc works. */
    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 3);
    assert(bufsize >= 3);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 4);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);
    

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 3);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    for (i = 0; i < found; i++) {
        assert(*(int*)buf[i].left != 2 || 
               *(int*)buf[i].right != 6 ||
               *(char*)buf[i].data == letters[1]);
    }

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 4);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 3);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    for (i = 0; i < found; i++) {
        assert(*(int*)buf[i].left != 2 || 
               *(int*)buf[i].right != 6 ||
               *(char*)buf[i].data == letters[0]);
    }

    /* Clean up. */
    range.left = &nums[1];
    range.right = &nums[5];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[2];
    range.right = &nums[6];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    assert(*(int*)buf[0].left == 3 &&
           *(int*)buf[0].right == 7 &&
           *(char*)buf[0].data == letters[0]);

    range.left = &nums[3];
    range.right = &nums[7];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

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
    find_range.left  = &nums[3];
    find_range.right = &nums[4];
    find_range.data  = NULL;
        
    r = toku_rt_create(&tree, int_cmp, char_cmp, TRUE);
    CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[1];
    range.right = &nums[3];
    range.data = &letters[0];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    assert(*(int*)buf[0].left == 1 && 
           *(int*)buf[0].right == 3 &&
           *(char*)buf[0].data == letters[0]);

    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_insert(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 2);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    range.left = &nums[1];
    range.right = &nums[3];
    range.data = &letters[0];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 1);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Verify the right one is still there, and the wrong one is not there. */
    assert(*(int*)buf[0].left == 4 && 
           *(int*)buf[0].right == 6 &&
           *(char*)buf[0].data == letters[1]);

    /* Clean up. */
    range.left = &nums[4];
    range.right = &nums[6];
    range.data = &letters[1];
    r = toku_rt_delete(tree, &range);   CKERR(r);

    r = toku_rt_find(tree, &find_range, 4, &buf, &bufsize, &found);  CKERR(r);
    assert(found == 0);
    assert(bufsize >= 4);
    verify_all_overlap(&find_range, buf, found);

    /* Done */

    r = toku_rt_close(tree);            CKERR(r);

    /* Cleanup. */
    tree = NULL;

    toku_free(buf);
    buf = NULL;
    return 0;
}

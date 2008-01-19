/* We are going to test whether create and close properly check their input. */

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree = NULL;
    toku_range range;

    parse_args(argc, argv);

    /* Create tests */
    r = toku_rt_create(NULL, dummy_cmp, dummy_cmp, FALSE);
    CKERR2(r, EINVAL);

    r = toku_rt_create(&tree, NULL, dummy_cmp, FALSE);
    CKERR2(r, EINVAL);
    
    assert(tree == NULL);

    r = toku_rt_create(&tree, dummy_cmp, NULL, FALSE);
    CKERR2(r, EINVAL);

    assert(tree == NULL);

    /* Close tests */
    r = toku_rt_close(NULL);
    CKERR2(r, EINVAL);

    /* Insert tests */
    r = toku_rt_insert(NULL, &range);
    CKERR2(r, EINVAL);
    
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, FALSE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_insert(tree, NULL);                         CKERR2(r, EINVAL);
    r = toku_rt_close(tree);                                CKERR(r);
    
    tree = NULL;

    /* Delete tests */
    r = toku_rt_delete(NULL, &range);
    CKERR2(r, EINVAL);
    
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, FALSE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_delete(tree, NULL);                         CKERR2(r, EINVAL);
    r = toku_rt_close(tree);                                CKERR(r);

    /* Find tests */
    toku_range* buf = (toku_range*)toku_malloc(2*sizeof(toku_range));
    unsigned bufsize = 2;
    unsigned found;

    int stuff[3] = {0,1,2};
    range.left = &stuff[0];
    range.right = &stuff[1];
    range.data = NULL;
    
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, FALSE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_find(NULL, &range, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
    
    r = toku_rt_find(tree, NULL, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
    
    range.data = &stuff[2];
    r = toku_rt_find(tree, &range, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
    range.data = NULL;
    
    r = toku_rt_find(tree, &range, 2, NULL, &bufsize, &found);
    CKERR2(r, EINVAL);
    
    r = toku_rt_find(tree, &range, 2, &buf, NULL, &found);
    CKERR2(r, EINVAL);
    
    unsigned oldbufsize = bufsize;
    bufsize = 0;
    r = toku_rt_find(tree, &range, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
    bufsize = oldbufsize;
    
    r = toku_rt_find(tree, &range, 2, &buf, &bufsize, NULL);
    CKERR2(r, EINVAL);
    
    r = toku_rt_close(tree);                                CKERR(r);

    /* Predecessor tests */
    int foo;
    BOOL wasfound;
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, FALSE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_predecessor(NULL, &foo, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_predecessor(tree, NULL, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_predecessor(tree, &foo, NULL, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_predecessor(tree, &foo, &range, NULL);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);

    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, TRUE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_predecessor(tree, &foo, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);


    /* Successor tests */
    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, FALSE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_successor(NULL, &foo, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_successor(tree, NULL, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_successor(tree, &foo, NULL, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_successor(tree, &foo, &range, NULL);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);

    r = toku_rt_create(&tree, dummy_cmp, dummy_cmp, TRUE); CKERR(r);
    assert(tree != NULL);

    r = toku_rt_successor(tree, &foo, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);

    tree = NULL;
    toku_free(buf);
    buf = NULL;
    return 0;
}

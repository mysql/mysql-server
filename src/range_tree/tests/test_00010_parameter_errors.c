/* We are going to test whether create and close properly check their input. */

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree = NULL;
    toku_range range;

    parse_args(argc, argv);

    /* Create tests */
    r = toku_rt_create(NULL,  int_cmp, TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR2(r, EINVAL);

    r = toku_rt_create(&tree, NULL,    TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR2(r, EINVAL);
    assert(tree == NULL);

    r = toku_rt_create(&tree, int_cmp, NULL,      false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR2(r, EINVAL);

    /* Close tests */
    r = toku_rt_close(NULL);
    CKERR2(r, EINVAL);

    /* Insert tests */
    r = toku_rt_insert(NULL, &range);
    CKERR2(r, EINVAL);
    
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_insert(tree, NULL);                         CKERR2(r, EINVAL);
    r = toku_rt_close(tree);                                CKERR(r);
    
    tree = NULL;

    /* Delete tests */
    r = toku_rt_delete(NULL, &range);
    CKERR2(r, EINVAL);
    
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_delete(tree, NULL);                         CKERR2(r, EINVAL);
    r = toku_rt_close(tree);                                CKERR(r);

    /* Find tests */
    toku_range* buf = (toku_range*)toku_malloc(2*sizeof(toku_range));
    unsigned bufsize = 2;
    unsigned found;

    toku_point stuff[3] = {{0},{1},{2}};
    range.ends.left  = (toku_point*)&stuff[0];
    range.ends.right = (toku_point*)&stuff[1];
    range.data       = 0;
    
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_find(NULL, &range.ends, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
    
    r = toku_rt_find(tree, NULL, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
    
    r = toku_rt_find(tree, &range.ends, 2, NULL, &bufsize, &found);
    CKERR2(r, EINVAL);
    
    r = toku_rt_find(tree, &range.ends, 2, &buf, NULL, &found);
    CKERR2(r, EINVAL);
    
    unsigned oldbufsize = bufsize;
    bufsize = 0;
#if 0
    r = toku_rt_find(tree, &range.ends, 2, &buf, &bufsize, &found);
    CKERR2(r, EINVAL);
#endif
    bufsize = oldbufsize;
    
    r = toku_rt_find(tree, &range.ends, 2, &buf, &bufsize, NULL);
    CKERR2(r, EINVAL);
    
    r = toku_rt_close(tree);                                CKERR(r);

    /* Predecessor tests */
    toku_point* foo = (toku_point*)&stuff[0];
    bool wasfound;
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_predecessor(NULL, foo,  &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_predecessor(tree, NULL, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_predecessor(tree, foo,  NULL, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_predecessor(tree, foo,  &range, NULL);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);

#ifndef TOKU_RT_NOOVERLAPS
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   true, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_predecessor(tree, foo,  &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);
    
    tree = NULL;
#endif


    /* Successor tests */
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_successor(NULL, foo,  &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_successor(tree, NULL, &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_successor(tree, foo,  NULL, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_successor(tree, foo,  &range, NULL);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);

#ifndef TOKU_RT_NOOVERLAPS
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   true, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_successor(tree, foo,  &range, &wasfound);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);

    tree = NULL;
#endif

    /* Get allow overlap */
    bool allowed;
    r = toku_rt_get_allow_overlaps(NULL, &allowed);
    CKERR2(r, EINVAL);
    
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_get_allow_overlaps(tree, NULL);
    CKERR2(r, EINVAL);

    r = toku_rt_close(tree);                                CKERR(r);
    tree = NULL;

    /* size tests */
    r = toku_rt_create(&tree, int_cmp,   TXNID_cmp,   false, test_incr_memory_size, test_decr_memory_size, NULL);
    CKERR(r);
    assert(tree != NULL);

    r = toku_rt_close(tree);                                CKERR(r);
    tree = NULL;    

    /* That's it: clean up and go home */
    toku_free(buf);
    buf = NULL;
    return 0;
}

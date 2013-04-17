/* We are going to test whether we can insert and delete. */

#include "test.h"

int nums[200];
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

#include "run.h"

static void
tests (BOOL allow_overlaps) {
    toku_interval query;
    toku_range insert;
    /*
        Limited/Unlimited Queries

        Limit of k does not produce all, but limit of 0 does.         Single point overlaps
    */

    /* Tree: {|0-1|,|2-3|,|4-5|,|6-7|,|8-9|}, query of |2-7|, limit 2 finds 2,
        limit 3 finds 3, limit 4 finds 3, limit 0 finds 3 */
    setup_tree(allow_overlaps, TRUE, 0, 1, 0);
    runinsert(0, init_range(&insert, 2, 3, 0)); 
    runinsert(0, init_range(&insert, 4, 5, 0)); 
    runinsert(0, init_range(&insert, 6, 7, 0)); 
    runinsert(0, init_range(&insert, 8, 9, 0));
    
    runlimitsearch(init_query(&query, 2, 7), 0, 3);
    runlimitsearch(init_query(&query, 2, 7), 1, 1);
    runlimitsearch(init_query(&query, 2, 7), 2, 2);
    runlimitsearch(init_query(&query, 2, 7), 3, 3);
    runlimitsearch(init_query(&query, 2, 7), 4, 3);
    close_tree();
    
    /* Tree is empty (return none) */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runlimitsearch(init_query(&query, 0, 0), 0, 0);
    close_tree();
    
    /* Tree contains only elements to the left. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runinsert(0, init_range(&insert, 1, 2, 0));
    runinsert(0, init_range(&insert, 3, 4, 0));
    runlimitsearch(init_query(&query, 8, 30), 0, 0);
    close_tree();
    
    /* Tree contains only elements to the right. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runinsert(0, init_range(&insert, 10, 20, 0));
    runinsert(0, init_range(&insert, 30, 40, 0));
    runlimitsearch(init_query(&query, 5, 7), 0, 0);
    close_tree();

    /* Tree contains only elements to the left and to the right. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runinsert(0, init_range(&insert, 10, 20, 0));
    runinsert(0, init_range(&insert, 30, 40, 0));
    runinsert(0, init_range(&insert, 70, 80, 0));
    runinsert(0, init_range(&insert, 90, 100, 0));
    runlimitsearch(init_query(&query, 60, 65), 0, 0);
    close_tree();
    
    /* Tree contains overlaps and elements to the left. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runinsert(0, init_range(&insert, 10, 20, 0));
    runinsert(0, init_range(&insert, 30, 40, 0));
    runinsert(0, init_range(&insert, 60, 80, 0));
    runinsert(0, init_range(&insert, 90, 100, 0));
    runlimitsearch(init_query(&query, 70, 95), 0, 2);
    close_tree();

    /* Tree contains overlaps and elements to the right. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runinsert(0, init_range(&insert, 110, 120, 0));
    runinsert(0, init_range(&insert, 130, 140, 0));
    runinsert(0, init_range(&insert, 60, 80, 0));
    runinsert(0, init_range(&insert, 90, 100, 0));
    runlimitsearch(init_query(&query, 70, 95), 0, 2);
    close_tree();

    /* Tree contains overlaps and elements to the left and to the right. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    runinsert(0, init_range(&insert, 10, 20, 0));
    runinsert(0, init_range(&insert, 30, 40, 0));
    runinsert(0, init_range(&insert, 110, 120, 0));
    runinsert(0, init_range(&insert, 130, 140, 0));
    runinsert(0, init_range(&insert, 60, 80, 0));
    runinsert(0, init_range(&insert, 90, 100, 0));
    runlimitsearch(init_query(&query, 70, 95), 0, 2);
    close_tree();
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    unsigned i;
    for (i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) nums[i] = i; 
    buflen = 2;
    buf = (toku_range*)toku_malloc(2 * sizeof(toku_range));
    tests(FALSE);
#ifndef TOKU_RT_NOOVERLAPS
    tests(TRUE);
#endif
    tree = NULL;
    toku_free(buf);
    buf = NULL;
    return 0;
}

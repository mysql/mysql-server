/* We are going to test whether we can insert and delete. */

#include "test.h"

int nums[8] = {0,1,2,3,4,5,6,7};
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

#include "run.h"

static void verify_overlap(BOOL allow_overlaps) {
    int r;
    BOOL allowed;

    r = toku_rt_get_allow_overlaps(tree, &allowed);
    CKERR(r);
    assert(allowed == allow_overlaps);
}

static void tests(BOOL allow_overlaps) {
    toku_range expect;
    toku_interval query;
    toku_range toinsert;

    /*
        Single point overlaps

        * Tree: {|0-1|}, query of |1-2| returns |0-1|
        * Tree: {|1-2|}, query of |0-1| returns |1-2|
        * Tree: {|1-2|}, insert of of |0-1| success == allow_overlaps
        * Tree: {|0-1|}, insert of of |1-2| success == allow_overlaps
    */

    /* Tree: {|0-1|}, query of |1-2| returns |0-1| 
       In this test, I am also going to verify that the allow_overlaps bit
       is set appropriately. */
    setup_tree(allow_overlaps, TRUE, 0, 1, 0);
    verify_overlap(allow_overlaps);
    runsearch(0, init_query(&query, 1, 2), init_range(&expect, 0, 1, 0));
    close_tree();

    /* Tree: {|1-2|}, query of |0-1| returns |1-2| */
    setup_tree(allow_overlaps, TRUE, 1, 2, 0);
    runsearch(0, init_query(&query, 0, 1), init_range(&expect, 1, 2, 0));
    close_tree();

    /* Tree: {|1-2|}, insert of of |0-1| success == allow_overlaps */
    setup_tree(allow_overlaps, TRUE, 1, 2, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 0, 1, 0));
    close_tree();

    /* Tree: {|0-1|}, insert of of |1-2| success == allow_overlaps */
    setup_tree(allow_overlaps, TRUE, 0, 1, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 1, 2, 0));
    close_tree();

    /*
        Complete overlaps
    
        * Tree: {|0-3|}, query of |1-2| returns |0-3|
        * Tree: {|1-2|}, query of |0-3| returns |1-2|
        * Tree: {|1-2|}, insert of of |0-3| success == allow_overlaps
        * Tree: {|0-3|}, insert of of |1-2| success == allow_overlaps
    */

    /* Tree: {|0-3|}, query of |1-2| returns |0-3| */
    setup_tree(allow_overlaps, TRUE, 0, 3, 0);
    runsearch(0, init_query(&query, 1, 2), init_range(&expect, 0, 3, 0));
    close_tree();

    /* Tree: {|1-2|}, query of |0-3| returns |1-2| */
    setup_tree(allow_overlaps, TRUE, 1, 2, 0);
    runsearch(0, init_query(&query, 0, 3), init_range(&expect, 1, 2, 0));
    close_tree();

    /* Tree: {|1-2|}, insert of of |0-3| success == allow_overlaps */
    setup_tree(allow_overlaps, TRUE, 1, 2, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 0, 3, 0));
    close_tree();

    /* Tree: {|0-3|}, insert of of |1-2| success == allow_overlaps */
    setup_tree(allow_overlaps, TRUE, 0, 3, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 1, 2, 0));
    close_tree();

    /*
        Exact overlaps

        * Tree: {|0-3|}, query of |0-3| returns |0-3|
        * Tree: {|0-3|}, insert of of |0-3| success == allow_overlaps
    */

    /* Tree: {|0-3|}, query of |0-3| returns |0-3| */
    setup_tree(allow_overlaps, TRUE, 0, 3, 0);
    runsearch(0, init_query(&query, 0, 3), init_range(&expect, 0, 3, 0));
    close_tree();

    /* Tree: {(|0-3|,0)}, insert of of (|0-3|,1) success == allow_overlaps */
    setup_tree(allow_overlaps, TRUE, 0, 3, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 0, 3, 1));
    close_tree();
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

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

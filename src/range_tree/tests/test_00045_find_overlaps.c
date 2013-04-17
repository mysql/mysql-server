/* We are going to test whether we can insert and delete. */

#include "test.h"

int nums[8] = {0,1,2,3,4,5,6,7};
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

#include "run.h"

static void verify_overlap(bool allow_overlaps) {
    int r;
    bool allowed;

    r = toku_rt_get_allow_overlaps(tree, &allowed);
    CKERR(r);
    assert(allowed == allow_overlaps);
}

static void tests(bool allow_overlaps) {
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
    setup_tree(allow_overlaps, true, 0, 1, 0);
    verify_overlap(allow_overlaps);
    runsearch(0, init_query(&query, 1, 2), init_range(&expect, 0, 1, 0));
    close_tree();

    /* Tree: {|1-2|}, query of |0-1| returns |1-2| */
    setup_tree(allow_overlaps, true, 1, 2, 0);
    runsearch(0, init_query(&query, 0, 1), init_range(&expect, 1, 2, 0));
    close_tree();

    /* Tree: {|1-2|}, insert of of |0-1| success == allow_overlaps */
    setup_tree(allow_overlaps, true, 1, 2, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 0, 1, 0));
    close_tree();

    /* Tree: {|0-1|}, insert of of |1-2| success == allow_overlaps */
    setup_tree(allow_overlaps, true, 0, 1, 0);
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
    setup_tree(allow_overlaps, true, 0, 3, 0);
    runsearch(0, init_query(&query, 1, 2), init_range(&expect, 0, 3, 0));
    close_tree();

    /* Tree: {|1-2|}, query of |0-3| returns |1-2| */
    setup_tree(allow_overlaps, true, 1, 2, 0);
    runsearch(0, init_query(&query, 0, 3), init_range(&expect, 1, 2, 0));
    close_tree();

    /* Tree: {|1-2|}, insert of of |0-3| success == allow_overlaps */
    setup_tree(allow_overlaps, true, 1, 2, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 0, 3, 0));
    close_tree();

    /* Tree: {|0-3|}, insert of of |1-2| success == allow_overlaps */
    setup_tree(allow_overlaps, true, 0, 3, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 1, 2, 0));
    close_tree();

    /*
        Exact overlaps

        * Tree: {|0-3|}, query of |0-3| returns |0-3|
        * Tree: {|0-3|}, insert of of |0-3| success == allow_overlaps
    */

    /* Tree: {|0-3|}, query of |0-3| returns |0-3| */
    setup_tree(allow_overlaps, true, 0, 3, 0);
    runsearch(0, init_query(&query, 0, 3), init_range(&expect, 0, 3, 0));
    close_tree();

    /* Tree: {(|0-3|,0)}, insert of of (|0-3|,1) success == allow_overlaps */
    setup_tree(allow_overlaps, true, 0, 3, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&toinsert, 0, 3, 1));
    close_tree();

    /* Tree: {(|1-3|,0),(|5-6|,0)} */
    setup_tree(allow_overlaps, true, 1, 3, 0);
    runinsert(0, init_range(&toinsert, 5, 6, 0));
    runsearch(0, init_query(&query, 3, 4), init_range(&expect, 1, 3, 0));
    runsearch(0, init_query(&query, 4, 5), init_range(&expect, 5, 6, 0));
    runsearch(0, init_query(&query, 4, 6), init_range(&expect, 5, 6, 0));
    runsearch(0, init_query(&query, 4, 7), init_range(&expect, 5, 6, 0));
    toku_range expect1, expect2;
    runsearch2(0, init_query(&query, 3, 7), init_range(&expect1, 1, 3, 0), init_range(&expect2, 5, 6, 0));
    close_tree();
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    buflen = 2;
    buf = (toku_range*)toku_malloc(2 * sizeof(toku_range));
    tests(false);
#ifndef TOKU_RT_NOOVERLAPS
    tests(true);
#endif

    tree = NULL;
    toku_free(buf);
    buf = NULL;
    return 0;
}

#include "test.h"

int nums[200];
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

#include "run.h"

static void *
init_point (unsigned left) {
    assert(left < sizeof(nums) / sizeof(nums[0]));
    return ((toku_point*)&nums[left]);
}

#if 0
static void
runsearch (int rexpect, toku_interval* query, toku_range* expect) {
    int r;
    unsigned found;
    r = toku_rt_find(tree, query, 0, &buf, &buflen, &found);
    CKERR2(r, rexpect);
    
    if (rexpect != 0) return;
    assert(found == 1);
    assert(int_cmp(buf[0].ends.left, expect->ends.left) == 0 &&
           int_cmp(buf[0].ends.right, expect->ends.right) == 0 &&
           char_cmp(buf[0].data, expect->data) == 0);
}
#endif

typedef enum {PRED=0, SUCC=1} predsucc;
static void
runtest (predsucc testtype, toku_point* query, bool findexpect,
	 unsigned left, unsigned right, unsigned data) {
    int r;
    bool found;
    toku_range out;
    assert(data < sizeof(letters) / sizeof(letters[0]));
    assert(left < sizeof(nums) / sizeof(nums[0]));
    assert(right < sizeof(nums) / sizeof(nums[0]));
    if (testtype == PRED) {
        r = toku_rt_predecessor(tree, query, &out, &found);

    }
    else {
        assert(testtype == SUCC);
        r = toku_rt_successor(tree, query, &out, &found);
    }
    CKERR(r);
    assert(found == findexpect);
    if (findexpect) {
        assert(int_cmp(out.ends.left, (toku_point*)&nums[left]) == 0);
        assert(int_cmp(out.ends.right, (toku_point*)&nums[right]) == 0);
        assert(char_cmp(out.data, (TXNID)letters[data]) == 0);
    }
}


static void
tests (bool allow_overlaps) {
    toku_range insert;

    /*
        Empty
            Only empty space test.
        1 element
        standard tree
        Pred/Succ:
            1: In empty space
                * Nothing to the left/Right (pred/succ) respectively.
                * something to the left/right (pred/succ) respectively.
            2: On left endpoint.
                * Nothing to the left/Right (pred/succ) respectively.
                * something to the left/right (pred/succ) respectively.
            3: On right endpoint.
                * Nothing to the left/Right (pred/succ) respectively.
                * something to the left/right (pred/succ) respectively.
            4: In middle of range.
                * Nothing to the left/Right (pred/succ) respectively.
                * something to the left/right (pred/succ) respectively.
    */

    /* Empty tree. */
    setup_tree(allow_overlaps, false, 0, 0, 0);
    runtest(PRED, init_point(5), false, 0, 0, 0);
    runtest(SUCC, init_point(5), false, 0, 0, 0);
    close_tree();

    /* Single element tree.  Before left, left end point, middle,
       right end point, after right. */
    setup_tree(allow_overlaps, false, 0, 0, 0);
    runinsert(0, init_range(&insert, 10, 20, 0)); 
    runtest(PRED, init_point(5),  false, 0, 0, 0);
    runtest(PRED, init_point(10), false, 0, 0, 0);
    runtest(PRED, init_point(15), false, 0, 0, 0);
    runtest(PRED, init_point(20), false, 0, 0, 0);
    runtest(PRED, init_point(25), true, 10, 20, 0);
    runtest(SUCC, init_point(5),  true, 10, 20, 0);
    runtest(SUCC, init_point(10), false, 0, 0, 0);
    runtest(SUCC, init_point(15), false, 0, 0, 0);
    runtest(SUCC, init_point(20), false, 0, 0, 0);
    runtest(SUCC, init_point(25), false, 0, 0, 0);
    close_tree();

    /*
        Swap left and right for succ.
        Multi element tree.
         * In empty space.
          * Something on left.
          * Nothing on left.

         * At a left end point.
          * Something on left.
          * Nothing on left.

         * Inside a range.
          * Something on left.
          * Nothing on left.

         * At a right end point.
          * Something on left.
          * Nothing on left.
    */
    setup_tree(allow_overlaps, false, 0, 0, 0);
    runinsert(0, init_range(&insert, 10, 20, 0)); 
    runinsert(0, init_range(&insert, 30, 40, 0)); 

    /*
     * In empty space.
      * Something on left.
      * Nothing on left.
    */
    runtest(PRED, init_point(25), true, 10, 20, 0);
    runtest(PRED, init_point(5), false, 0, 0, 0);

    /*
     * At a left end point.
      * Something on left.
      * Nothing on left.
    */
    runtest(PRED, init_point(30), true, 10, 20, 0);
    runtest(PRED, init_point(10), false, 0, 0, 0);

    /*
     * Inside a range.
      * Something on left.
      * Nothing on left.
     */
    runtest(PRED, init_point(35), true, 10, 20, 0);
    runtest(PRED, init_point(15), false, 0, 0, 0);

    /*
     * At a right end point.
      * Something on left.
      * Nothing on left.
    */
    runtest(PRED, init_point(40), true, 10, 20, 0);
    runtest(PRED, init_point(20), false, 0, 0, 0);

    /*
     * In empty space.
      * Something on right.
      * Nothing on right.
    */
    runtest(SUCC, init_point(25), true, 30, 40, 0);
    runtest(SUCC, init_point(45), false, 0, 0, 0);

    /*
     * At a right end point.
      * Something on right.
      * Nothing on right.
    */
    runtest(SUCC, init_point(20), true, 30, 40, 0);
    runtest(SUCC, init_point(40), false, 0, 0, 0);

    /*
     * Inside a range.
      * Something on right.
      * Nothing on right.
     */
    runtest(SUCC, init_point(15), true, 30, 40, 0);
    runtest(SUCC, init_point(35), false, 0, 0, 0);

    /*
     * At a right end point.
      * Something on right.
      * Nothing on right.
    */
    runtest(SUCC, init_point(20), true, 30, 40, 0);
    runtest(SUCC, init_point(40), false, 0, 0, 0);

    close_tree();


    /*
        With other interval that cannot be the predecessor
        or the successor, but that need to be looked at.  */

    setup_tree(allow_overlaps, false, 0, 0, 0);
    runinsert(0, init_range(&insert,  5, 7, 0)); 
    runinsert(0, init_range(&insert, 50, 60, 0)); 
    runinsert(0, init_range(&insert, 10, 20, 0)); 
    runinsert(0, init_range(&insert, 30, 40, 0)); 
    runinsert(0, init_range(&insert,  2, 4, 0)); 
    runinsert(0, init_range(&insert, 70, 80, 0)); 

    runtest(PRED, init_point(25), true, 10, 20, 0);
    runtest(PRED, init_point(4), false, 0, 0, 0);
    runtest(SUCC, init_point(25), true, 30, 40, 0);
    runtest(SUCC, init_point(95), false, 0, 0, 0);
    
    close_tree();
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    unsigned i;
    
    for (i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) nums[i] = i; 
    buflen = 2;
    buf = (toku_range*)toku_malloc(2 * sizeof(toku_range));
    tests(false);

    tree = NULL;
    toku_free(buf);
    buf = NULL;
    return 0;
}

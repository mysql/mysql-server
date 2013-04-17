/* We are going to test whether we can insert and delete. */

#include "test.h"

int nums[64 << 3];
const size_t numlen = sizeof(nums) / sizeof(nums[0]);
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

#include "run.h"

static void
rundelete (int rexpect, toku_range* todelete) {
    int r;
    r = toku_rt_delete(tree, todelete);
    CKERR2(r, rexpect);
}

static void
tests (BOOL allow_overlaps) {
    toku_range insert;
    unsigned i;
    /* Force buf to increase. */
    setup_tree(allow_overlaps, FALSE, 0, 0, 0);
    for (i = 0; i < numlen / 2; i++) {
    	runinsert(0, init_range(&insert, i, i, 0));
        unsigned j = numlen /2 + i;
        runinsert(0, init_range(&insert, j, j, 1));  
    }
    int k;
    for (k = numlen/2 -1; k >= 0; k--) {
        i = k;
        rundelete(0, init_range(&insert, i, i, 0));
        unsigned j = numlen /2 + i;
        rundelete(0, init_range(&insert, j, j, 1));
    }
     
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

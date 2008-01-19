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

int nums[8] = {0,1,2,3,4,5,6,7};
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

toku_range* init_range(toku_range* range, int left, int right, int data) {
    range->left = &nums[left];
    range->right = &nums[right];
    if (data < 0)   range->data = NULL;
    else            range->data = &letters[data];
    return range;
}

void setup_tree(BOOL allow_overlaps, int left, int right, int data) {
    int r;
    toku_range range;
    r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps);   CKERR(r);

    r = toku_rt_insert(tree, init_range(&range, left, right, data));CKERR(r);
}
void close_tree(void) {
    int r;
    r = toku_rt_close(tree);    CKERR(r);
}

void runsearch(int rexpect, toku_range* query, toku_range* expect) {
    int r;
    unsigned found;
    r = toku_rt_find(tree, query, 0, &buf, &buflen, &found);
    CKERR2(r, rexpect);
    
    if (rexpect != 0) return;
    assert(found == 1);
    assert(int_cmp(&buf[0].left, expect->left) == 0 &&
           int_cmp(&buf[0].right, expect->right) == 0 &&
           char_cmp(&buf[0].data, expect->data) == 0);
}

void runinsert(int rexpect, toku_range* toinsert) {
    int r;
    r = toku_rt_insert(tree, toinsert);
    CKERR2(r, rexpect);
}

void tests(BOOL allow_overlaps) {
    toku_range expect;
    toku_range query;

    /*
        Single point overlaps

        * Tree: {|0-1|}, query of |1-2| returns |0-1|
        * Tree: {|1-2|}, query of |0-1| returns |1-2|
        * Tree: {|1-2|}, insert of of |0-1| success == allow_overlaps
        * Tree: {|0-1|}, insert of of |1-2| success == allow_overlaps
    */

    /* Tree: {|0-1|}, query of |1-2| returns |0-1| */
    setup_tree(allow_overlaps, 0, 1, 0);
    runsearch(0, init_range(&query, 1, 2, 0), init_range(&expect, 0, 1, 0));
    close_tree();

    /* Tree: {|1-2|}, query of |0-1| returns |1-2| */
    setup_tree(allow_overlaps, 1, 2, 0);
    runsearch(0, init_range(&query, 0, 1, 0), init_range(&expect, 1, 2, 0));
    close_tree();

    /* Tree: {|1-2|}, insert of of |0-1| success == allow_overlaps */
    setup_tree(allow_overlaps, 1, 2, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&query, 0, 1, 0));
    close_tree();

    /* Tree: {|0-1|}, insert of of |1-2| success == allow_overlaps */
    setup_tree(allow_overlaps, 0, 1, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&query, 1, 2, 0));
    close_tree();

    /*
        Complete overlaps
    
        * Tree: {|0-3|}, query of |1-2| returns |0-3|
        * Tree: {|1-2|}, query of |0-3| returns |1-2|
        * Tree: {|1-2|}, insert of of |0-3| success == allow_overlaps
        * Tree: {|0-3|}, insert of of |1-2| success == allow_overlaps
    */

    /* Tree: {|0-3|}, query of |1-2| returns |0-3| */
    setup_tree(allow_overlaps, 0, 3, 0);
    runsearch(0, init_range(&query, 1, 2, 0), init_range(&expect, 0, 3, 0));
    close_tree();

    /* Tree: {|1-2|}, query of |0-3| returns |1-2| */
    setup_tree(allow_overlaps, 1, 2, 0);
    runsearch(0, init_range(&query, 0, 3, 0), init_range(&expect, 1, 2, 0));
    close_tree();

    /* Tree: {|1-2|}, insert of of |0-3| success == allow_overlaps */
    setup_tree(allow_overlaps, 1, 2, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&query, 0, 3, 0));
    close_tree();

    /* Tree: {|0-3|}, insert of of |1-2| success == allow_overlaps */
    setup_tree(allow_overlaps, 0, 3, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&query, 1, 2, 0));
    close_tree();

    /*
        Exact overlaps

        * Tree: {|0-3|}, query of |0-3| returns |0-3|
        * Tree: {|0-3|}, insert of of |0-3| success == allow_overlaps
    */

    /* Tree: {|0-3|}, query of |0-3| returns |0-3| */
    setup_tree(allow_overlaps, 0, 3, 0);
    runsearch(0, init_range(&query, 0, 3, 0), init_range(&expect, 0, 3, 0));
    close_tree();

    /* Tree: {|0-3|}, insert of of |0-3| success == allow_overlaps */
    setup_tree(allow_overlaps, 0, 3, 0);
    runinsert((allow_overlaps ? 0 : EDOM), init_range(&query, 0, 3, 0));
    close_tree();
}

int main(int argc, const char *argv[]) {
    buflen = 2;
    buf = (toku_range*)toku_malloc(2 * sizeof(toku_range));
    tests(FALSE);
    tests(TRUE);

    tree = NULL;
    toku_free(buf);
    buf = NULL;
    return 0;
}

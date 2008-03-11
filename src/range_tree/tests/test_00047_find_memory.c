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

int nums[64 << 3];
const size_t numlen = sizeof(nums) / sizeof(nums[0]);
char letters[2] = {'A','B'};

toku_range_tree *tree;
toku_range* buf;
unsigned buflen;

toku_range* init_range(toku_range* range, unsigned left, unsigned right,
                       int data) {
    range->left = (toku_point*)&nums[left];
    range->right = (toku_point*)&nums[right];
    if (data < 0)   range->data = NULL;
    else            range->data = (DB_TXN*)&letters[data];
    return range;
}

void setup_tree(BOOL allow_overlaps, BOOL insert,
                unsigned left, unsigned  right, unsigned data) {
    int r;
    toku_range range;
    r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps, malloc, free, realloc);
    CKERR(r);

    if (insert) {
        r = toku_rt_insert(tree, init_range(&range, left, right, (int)data));
        CKERR(r);
    }
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
    assert(int_cmp(buf[0].left, expect->left) == 0 &&
           int_cmp(buf[0].right, expect->right) == 0 &&
           char_cmp(buf[0].data, expect->data) == 0);
}

void runinsert(int rexpect, toku_range* toinsert) {
    int r;
    r = toku_rt_insert(tree, toinsert);
    CKERR2(r, rexpect);
}

void rundelete(int rexpect, toku_range* todelete) {
    int r;
    r = toku_rt_delete(tree, todelete);
    CKERR2(r, rexpect);
}

void runlimitsearch(toku_range* query, unsigned limit, unsigned findexpect) {
    int r;
    unsigned found;
    r=toku_rt_find(tree, query, limit, &buf, &buflen, &found);  CKERR(r);
    verify_all_overlap(query, buf, found);
    
    assert(found == findexpect);
}

void tests(BOOL allow_overlaps) {
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

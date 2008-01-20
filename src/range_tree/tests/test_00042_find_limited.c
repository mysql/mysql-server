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

int nums[100];
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

void runlimitsearch(toku_range* query, unsigned limit, unsigned findexpect) {
    int r;
    unsigned found;
    r=toku_rt_find(tree, query, limit, &buf, &buflen, &found);  CKERR(r);
    assert(found == findexpect);
}

void tests(BOOL allow_overlaps) {
    toku_range query;
    toku_range insert;
    /*
        Limited/Unlimited Queries

        Limit of k does not produce all, but limit of 0 does.         Single point overlaps
    */

    /* Tree: {|0-1|,|2-3|,|4-5|,|6-7|,|8-9|}, query of |2-7|, limit 2 finds 2,
        limit 3 finds 3, limit 4 finds 3, limit 0 finds 3 */
    setup_tree(allow_overlaps, 0, 1, 0);
    runinsert(0, init_range(&insert, 2, 3, 0)); 
    runinsert(0, init_range(&insert, 4, 5, 0)); 
    runinsert(0, init_range(&insert, 6, 7, 0)); 
    runinsert(0, init_range(&insert, 8, 9, 0));
    
    runlimitsearch(init_range(&query, 2, 7, 0), 0, 3);
    runlimitsearch(init_range(&query, 2, 7, 0), 1, 1);
    runlimitsearch(init_range(&query, 2, 7, 0), 2, 2);
    runlimitsearch(init_range(&query, 2, 7, 0), 3, 3);
    runlimitsearch(init_range(&query, 2, 7, 0), 4, 3);
    close_tree();
}

int main(int argc, const char *argv[]) {
    int i;
    for (i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) nums[i] = i; 
    buflen = 2;
    buf = (toku_range*)toku_malloc(2 * sizeof(toku_range));
    tests(FALSE);
    tests(TRUE);

    tree = NULL;
    toku_free(buf);
    buf = NULL;
    return 0;
}

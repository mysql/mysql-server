static toku_interval *
init_query(toku_interval* range, int left, int right) {
    range->left = (toku_point*)&nums[left];
    range->right = (toku_point*)&nums[right];
    return range;
}

static toku_range *
init_range (toku_range* range, int left, int right, int data) {
    init_query(&range->ends, left, right);
    if (data < 0)   range->data = 0;
    else            range->data = (TXNID)letters[data];
    return range;
}

static void
setup_tree (BOOL allow_overlaps, BOOL insert, int left, int right, int data) {
    int r;
    toku_range range;
    r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps, toku_malloc, toku_free, toku_realloc);
    CKERR(r);

    if (insert) {
        r = toku_rt_insert(tree, init_range(&range, left, right, data));
        CKERR(r);
    }
}

static void
close_tree (void) {
    int r;
    r = toku_rt_close(tree);    CKERR(r);
}

static void
runinsert (int rexpect, toku_range* toinsert) {
    int r;
    r = toku_rt_insert(tree, toinsert);
    CKERR2(r, rexpect);
}

static __attribute__((__unused__)) void 
runsearch (int rexpect, toku_interval* query, toku_range* expect);

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

static __attribute__((__unused__)) void
runlimitsearch (toku_interval* query, unsigned limit, unsigned findexpect);

static void
runlimitsearch (toku_interval* query, unsigned limit, unsigned findexpect) {
    int r;
    unsigned found;
    r=toku_rt_find(tree, query, limit, &buf, &buflen, &found);  CKERR(r);
    verify_all_overlap(query, buf, found);
    
    assert(found == findexpect);
}

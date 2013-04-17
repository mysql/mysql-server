// test that the toku_rt_clear function works

#include "test.h"

static int count_range_callback(toku_range *range UU(), void *extra) {
    int *counter = (int *) extra;
    *counter += 1;
    return 0;
}

static int count_ranges(toku_range_tree *tree) {
    int counter = 0;
    int r = toku_rt_iterate(tree, count_range_callback, &counter); CKERR(r);
    return counter;
}

static void my_init_range(toku_range *range, int *left, int *right, int data) {
    range->ends.left = (toku_point *) left;
    range->ends.right = (toku_point *) right;
    range->data = data;
}

int main(int argc, const char *argv[]) {
    int r;

    parse_args(argc, argv);

    toku_range_tree *tree;
    r = toku_rt_create(&tree, int_cmp, char_cmp, FALSE, toku_malloc, toku_free, toku_realloc); CKERR(r);
    assert(count_ranges(tree) == 0);

    const int nranges = 10;
    int nums[nranges];
    for (int i = 0; i < nranges; i++) {
        assert(count_ranges(tree) == i);
        u_int32_t treesize = 0;
        r = toku_rt_get_size(tree, &treesize); CKERR(r);
        assert(treesize == (u_int32_t) i);
        nums[i] = i;
        toku_range range; my_init_range(&range, &nums[i], &nums[i], 'a');
        r = toku_rt_insert(tree, &range); CKERR(r);
    }

    assert(count_ranges(tree) == nranges);
    toku_rt_clear(tree);
    assert(count_ranges(tree) == 0);

    r = toku_rt_close(tree); CKERR(r);

    return 0;
}

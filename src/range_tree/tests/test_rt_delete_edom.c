// test that deleting an overlapping range fails

#include "test.h"

static void my_init_range(toku_range *range, int *left, int *right, int data) {
    range->ends.left = (toku_point *) left;
    range->ends.right = (toku_point *) right;
    range->data = data;
}

int main(int argc, const char *argv[]) {
    int r;

    parse_args(argc, argv);

    toku_range_tree *tree;
    r = toku_rt_create(&tree, int_cmp, char_cmp, false, test_incr_memory_size, test_decr_memory_size, NULL); 
    CKERR(r);

    int insert_left = 10; int insert_right = 20;
    toku_range insert_range; my_init_range(&insert_range, &insert_left, &insert_right, 'a');
    r = toku_rt_insert(tree, &insert_range); CKERR(r);

    int delete_left = 5; int delete_right = 15;
    toku_range delete_range; my_init_range(&delete_range, &delete_left, &delete_right, 'b');
    r = toku_rt_delete(tree, &delete_range); 
    assert(r == EDOM);

    r = toku_rt_close(tree); CKERR(r);

    return 0;
}

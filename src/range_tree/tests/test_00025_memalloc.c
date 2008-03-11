/* We are going to test whether we can manage memory once we do lots of 
   insert and delete. */

#include "test.h"

unsigned malloc_cnt;
unsigned malloc_cntl;

/* Controllable malloc failure: it fails only the ith time it is invoked */
static void* malloc_fail(size_t size) {
    if (malloc_cntl == ++malloc_cnt) {
        errno = ENOMEM;
        return NULL;
    } else 
        return malloc(size);
}

void RunTest (BOOL f_overlaps_allowed) {
    int i, j;
    int r;
    toku_range_tree *tree;
    toku_range range;
    int nums[1024];
    char letters[2] = {'A','B'};

    for (i = 0; i < 1024; i++)
      nums[i] = i;


    /* Insert and delete lots of ranges to force memory increase and decrease */

    r = toku_rt_create(&tree, int_cmp, char_cmp, f_overlaps_allowed, malloc, free, realloc);
    CKERR(r);

    /* Insert lots of ranges */
    for (i = 0; i < 512; i++) {
      j = i + i;
      range.left  = (toku_point*)&nums[j];
      range.right = (toku_point*)&nums[j+1];
      range.data  = (DB_TXN*)&letters[0];
      r = toku_rt_insert(tree, &range);   CKERR(r);
    }

    /* Decrease lots of ranges */
    for (i = 0; i < 512; i++) {
      j = i + i;
      range.left  = (toku_point*)&nums[j];
      range.right = (toku_point*)&nums[j+1];
      range.data  = (DB_TXN*)&letters[0];
      r = toku_rt_delete(tree, &range);   CKERR(r);
    }

    r = toku_rt_close(tree);            CKERR(r);

    tree = NULL;


    /* Force malloc to fail */

    /* Failure when allocating the tree */
    malloc_cnt  = 0;
    malloc_cntl = 1;
    r = toku_rt_create(&tree, int_cmp, char_cmp, f_overlaps_allowed, malloc_fail, free, 
                       realloc);
    CKERR2(r, ENOMEM);

    /* Failure when allocating the tree ranges */
    malloc_cnt  = 0;
    malloc_cntl = 2;
    r = toku_rt_create(&tree, int_cmp, char_cmp, f_overlaps_allowed, malloc_fail, free, 
                       realloc);
    CKERR2(r, ENOMEM);

}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

#ifndef TOKU_RT_NOOVERLAPS
    RunTest(TRUE);
#endif
    RunTest(FALSE);
    
    return 0;
}

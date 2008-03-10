/* We are going to test whether we can create and close range trees */

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;
    toku_range_tree *tree;
    BOOL allow_overlaps;
    BOOL temp;

    parse_args(argc, argv);

    for (allow_overlaps = 0; allow_overlaps < 2; allow_overlaps++) {
#ifdef TOKU_RT_NOOVERLAPS
    if (allow_overlaps) continue;
#endif
    	r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps, malloc, free, realloc);
    	CKERR(r);
    
    	assert(tree!=NULL);
    	r = toku_rt_get_allow_overlaps(tree, &temp);
        CKERR(r);
        assert((temp != 0) == (allow_overlaps != 0));

    	r = toku_rt_close(tree);
    	CKERR(r);

    	tree = NULL;
    }
    
    for (allow_overlaps = 0; allow_overlaps < 2; allow_overlaps++) {
#ifdef TOKU_RT_NOOVERLAPS
    if (allow_overlaps) continue;
#endif
        int i;
        for (i = 1; i <= 2; i++) {
            mallocced = 0;
            failon = i;
            r = toku_rt_create(&tree, int_cmp, char_cmp, allow_overlaps,
                               fail_malloc, free, realloc);
            CKERR2(r, ENOMEM);

            assert(tree==NULL);
        }
    }
    return 0;
}


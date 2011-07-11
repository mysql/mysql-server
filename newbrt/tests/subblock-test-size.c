// test the choose sub block size function
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include "test.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "sub_block.h"
int verbose;

static void
test_sub_block_size(int total_size) {
    if (verbose)
        printf("%s:%d %d\n", __FUNCTION__, __LINE__, total_size);
    int r;
    int sub_block_size, n_sub_blocks;
    r = choose_sub_block_size(total_size, 0, &sub_block_size, &n_sub_blocks);
    assert(r == EINVAL);
    for (int i = 1; i < max_sub_blocks; i++) {
        r = choose_sub_block_size(total_size, i, &sub_block_size, &n_sub_blocks);
        assert(r == 0);
        assert(0 <= n_sub_blocks && n_sub_blocks <= i);
        assert(total_size <= n_sub_blocks * sub_block_size);
    }
}

int
test_main (int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0)
            verbose++;
    }
    test_sub_block_size(0);
    for (int total_size = 1; total_size <= 4*1024*1024; total_size *= 2) {
        test_sub_block_size(total_size);
    }
    return 0;
}

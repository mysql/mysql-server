// test the sub block index function

#include <toku_portability.h>
#include "test.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "sub_block.h"
int verbose;

static void
test_sub_block_index(void) {
    if (verbose)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
    
    const int n_sub_blocks = max_sub_blocks;
    struct sub_block sub_block[n_sub_blocks];
    
    size_t max_offset = 0;
    for (int i = 0 ; i < n_sub_blocks; i++) {
        size_t size = i+1;
        sub_block_init(&sub_block[i]);
        sub_block[i].uncompressed_size = size;
        max_offset += size;
    }
    
    int offset_to_sub_block[max_offset];
    for (int i = 0; i < (int) max_offset; i++)
        offset_to_sub_block[i] = -1;

    size_t start_offset = 0;
    for (int i = 0; i < n_sub_blocks; i++) {
        size_t size = sub_block[i].uncompressed_size;
        for (int j = 0; j < (int) (start_offset + size); j++) {
            if (offset_to_sub_block[j] == -1)
                offset_to_sub_block[j] = i;
        }
        start_offset += size;
    }

    int r;
    for (size_t offset = 0; offset < max_offset; offset++) {
        r = get_sub_block_index(n_sub_blocks, sub_block, offset);
        if (verbose)
            printf("%s:%d %u %d\n", __FUNCTION__, __LINE__, (unsigned int) offset, r);
        assert(0 <= r && r < n_sub_blocks);
        assert(r == offset_to_sub_block[offset]);
    }
    
    r = get_sub_block_index(n_sub_blocks, sub_block, max_offset);
    assert(r == -1);
}

int
test_main (int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0)
            verbose++;
    }
    test_sub_block_index();
    return 0;
}

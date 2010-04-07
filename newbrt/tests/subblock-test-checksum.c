// test that corrupt checksums are detected

#include <toku_portability.h>
#include "test.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "sub_block.h"

int verbose;

static uint8_t
get_uint8_at_offset(void *vp, size_t offset) {
    uint8_t *ip = (uint8_t *) vp;
    return ip[offset];
}

static void
set_uint8_at_offset(void *vp, size_t offset, uint8_t newv) {
    uint8_t *ip = (uint8_t *) vp;
    ip[offset] = newv;
}

static void
test_sub_block_checksum(void *buf, int total_size, int my_max_sub_blocks, int n_cores) {
    if (verbose)
        printf("%s:%d %d %d\n", __FUNCTION__, __LINE__, total_size, my_max_sub_blocks);

    int r;

    int sub_block_size, n_sub_blocks;
    r = choose_sub_block_size(total_size, my_max_sub_blocks, &sub_block_size, &n_sub_blocks);
    assert(r == 0);
    if (verbose)
        printf("%s:%d %d %d\n", __FUNCTION__, __LINE__, sub_block_size, n_sub_blocks);

    struct sub_block sub_blocks[n_sub_blocks];
    set_all_sub_block_sizes(total_size, sub_block_size, n_sub_blocks, sub_blocks);

    size_t cbuf_size_bound = get_sum_compressed_size_bound(n_sub_blocks, sub_blocks);
    void *cbuf = toku_malloc(cbuf_size_bound);
    assert(cbuf);

    size_t cbuf_size = compress_all_sub_blocks(n_sub_blocks, sub_blocks, buf, cbuf, n_cores);
    assert(cbuf_size <= cbuf_size_bound);

    void *ubuf = toku_malloc(total_size);
    assert(ubuf);

    for (int xidx = 0; xidx < n_sub_blocks; xidx++) {
        // corrupt a checksum
        sub_blocks[xidx].xsum += 1;

        r = decompress_all_sub_blocks(n_sub_blocks, sub_blocks, cbuf, ubuf, n_cores);
        assert(r != 0);

        // reset the checksums
        sub_blocks[xidx].xsum -= 1;

        r = decompress_all_sub_blocks(n_sub_blocks, sub_blocks, cbuf, ubuf, n_cores);
        assert(r == 0);
        assert(memcmp(buf, ubuf, total_size) == 0);

        // corrupt the data
        size_t offset = random() % cbuf_size;
        unsigned char c = get_uint8_at_offset(cbuf, offset);
        set_uint8_at_offset(cbuf, offset, c+1);

        r = decompress_all_sub_blocks(n_sub_blocks, sub_blocks, cbuf, ubuf, n_cores);
        assert(r != 0);

        // reset the data
        set_uint8_at_offset(cbuf, offset, c);

        r = decompress_all_sub_blocks(n_sub_blocks, sub_blocks, cbuf, ubuf, n_cores);
        assert(r == 0);
        assert(memcmp(buf, ubuf, total_size) == 0);
    }

    toku_free(ubuf);
    toku_free(cbuf);
}

static void
set_random(void *buf, int total_size) {
    char *bp = (char *) buf;
    for (int i = 0; i < total_size; i++)
        bp[i] = random();
}

static void
run_test(int total_size, int n_cores) {
    void *buf = toku_malloc(total_size);
    assert(buf);

    for (int my_max_sub_blocks = 1; my_max_sub_blocks <= max_sub_blocks; my_max_sub_blocks++) {
        memset(buf, 0, total_size);
        test_sub_block_checksum(buf, total_size, my_max_sub_blocks, n_cores);

        set_random(buf, total_size);
        test_sub_block_checksum(buf, total_size, my_max_sub_blocks, n_cores);
    }

    toku_free(buf);
}
int
test_main (int argc, const char *argv[]) {
    int n_cores = 1;
    int e = 1;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-n") == 0) {
            if (i+1 < argc) {
                n_cores = atoi(argv[++i]);
                continue;
            }
        }
        if (strcmp(arg, "-e") == 0) {
            if (i+1 < argc) {
                e = atoi(argv[++i]);
                continue;
            }
        }
    }

    for (int total_size = 256*1024; total_size <= 4*1024*1024; total_size *= 2) {
        for (int size = total_size - e; size <= total_size + e; size++) {
            run_test(size, n_cores);
        }
    }

    return 0;
}

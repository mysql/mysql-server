#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "toku_assert.h"
#include "x1764.h"
#include "sub_block.h"

void 
sub_block_init(struct sub_block *sub_block) {
    sub_block->uncompressed_ptr = 0;
    sub_block->uncompressed_size = 0;

    sub_block->compressed_ptr = 0;
    sub_block->compressed_size_bound = 0;
    sub_block->compressed_size = 0;

    sub_block->xsum = 0;
}

// get the size of the compression header
size_t 
sub_block_header_size(int n_sub_blocks) {
    return sizeof (u_int32_t) + n_sub_blocks * sizeof (struct stored_sub_block);
}

// get the sum of the sub block compressed sizes 
size_t 
get_sum_compressed_size_bound(int n_sub_blocks, struct sub_block sub_block[]) {
    size_t compressed_size_bound = 0;
    for (int i = 0; i < n_sub_blocks; i++) {
        sub_block[i].compressed_size_bound = compressBound(sub_block[i].uncompressed_size);
        compressed_size_bound += sub_block[i].compressed_size_bound;
    }
    return compressed_size_bound;
}

// get the sum of the sub block uncompressed sizes 
size_t 
get_sum_uncompressed_size(int n_sub_blocks, struct sub_block sub_block[]) {
    size_t uncompressed_size = 0;
    for (int i = 0; i < n_sub_blocks; i++) 
        uncompressed_size += sub_block[i].uncompressed_size;
    return uncompressed_size;
}

// round up n
static inline int 
alignup32(int a, int b) {
    return ((a+b-1) / b) * b;
}

// Choose n_sub_blocks and sub_block_size such that the product is >= total_size and the sub_block_size is at
// least >= the target_sub_block_size.
int
choose_sub_block_size(int total_size, int n_sub_blocks_limit, int *sub_block_size_ret, int *n_sub_blocks_ret) {
    if (total_size < 0 || n_sub_blocks_limit < 1)
        return EINVAL;

    const int alignment = 32;

    int n_sub_blocks, sub_block_size;
    n_sub_blocks = total_size / target_sub_block_size;
    if (n_sub_blocks <= 1) {
	n_sub_blocks = n_sub_blocks;
        if (total_size > 0 && n_sub_blocks_limit > 0)
            n_sub_blocks = 1;
        sub_block_size = total_size;
    } else {
        if (n_sub_blocks > n_sub_blocks_limit) // limit the number of sub-blocks
            n_sub_blocks = n_sub_blocks_limit;
	sub_block_size = alignup32(total_size / n_sub_blocks, alignment);
        while (sub_block_size * n_sub_blocks < total_size) // round up the sub-block size until big enough
            sub_block_size += alignment;
    }

    *sub_block_size_ret = sub_block_size;
    *n_sub_blocks_ret = n_sub_blocks;

    return 0;
}

void
set_all_sub_block_sizes(int total_size, int sub_block_size, int n_sub_blocks, struct sub_block sub_block[]) {
    int size_left = total_size;
    int i;
    for (i = 0; i < n_sub_blocks-1; i++) {
        sub_block[i].uncompressed_size = sub_block_size;
        size_left -= sub_block_size;
    }
    if (i == 0 || size_left > 0) 
        sub_block[i].uncompressed_size = size_left;
}

// find the index of the first sub block that contains offset
// Returns the sub block index, else returns -1
int
get_sub_block_index(int n_sub_blocks, struct sub_block sub_block[], size_t offset) {
    size_t start_offset = 0;
    for (int i = 0; i < n_sub_blocks; i++) {
        size_t size = sub_block[i].uncompressed_size;
        if (offset < start_offset + size)
            return i;
        start_offset += size;
    }
    return -1;
}       

#include "workset.h"

void
compress_work_init(struct compress_work *w, struct sub_block *sub_block) {
    w->sub_block = sub_block;
}

void
compress_sub_block(struct sub_block *sub_block) {
    // compress it
    Bytef *uncompressed_ptr = (Bytef *) sub_block->uncompressed_ptr;
    Bytef *compressed_ptr = (Bytef *) sub_block->compressed_ptr;
    uLongf uncompressed_len = sub_block->uncompressed_size;
    uLongf real_compressed_len = sub_block->compressed_size_bound;
    int compression_level = 5;

    int r = compress2((Bytef*)compressed_ptr, &real_compressed_len,
                      (Bytef*)uncompressed_ptr, uncompressed_len,
                      compression_level);
    assert(r == Z_OK);
    sub_block->compressed_size = real_compressed_len; // replace the compressed size estimate with the real size

    // checksum it
    sub_block->xsum = x1764_memory(sub_block->compressed_ptr, sub_block->compressed_size);
}

void *
compress_worker(void *arg) {
    struct workset *ws = (struct workset *) arg;
    while (1) {
        struct compress_work *w = (struct compress_work *) workset_get(ws);
        if (w == NULL)
            break;
        compress_sub_block(w->sub_block);
    }
    return arg;
}

size_t
compress_all_sub_blocks(int n_sub_blocks, struct sub_block sub_block[], char *uncompressed_ptr, char *compressed_ptr, int num_cores) {
    char *compressed_base_ptr = compressed_ptr;
    size_t compressed_len;

    if (n_sub_blocks == 1) {
        // single sub-block 
        sub_block[0].uncompressed_ptr = uncompressed_ptr;
        sub_block[0].compressed_ptr = compressed_ptr;
        compress_sub_block(&sub_block[0]);
        compressed_len = sub_block[0].compressed_size;
    } else {
        // multiple sub-blocks
        int T = num_cores; // T = min(num_cores, n_sub_blocks) - 1
        if (T > n_sub_blocks)
            T = n_sub_blocks;
        if (T > 0)
            T = T - 1;     // threads in addition to the running thread

        struct workset ws;
        workset_init(&ws);

        struct compress_work work[n_sub_blocks];
        workset_lock(&ws);
        for (int i = 0; i < n_sub_blocks; i++) {
            sub_block[i].uncompressed_ptr = uncompressed_ptr;
            sub_block[i].compressed_ptr = compressed_ptr;
            compress_work_init(&work[i], &sub_block[i]);
            workset_put_locked(&ws, &work[i].base);
            uncompressed_ptr += sub_block[i].uncompressed_size;
            compressed_ptr += sub_block[i].compressed_size_bound;
        }
        workset_unlock(&ws);

        // compress the sub-blocks
        if (0) printf("%s:%d T=%d N=%d\n", __FUNCTION__, __LINE__, T, n_sub_blocks);
        toku_pthread_t tids[T];
        threadset_create(tids, &T, compress_worker, &ws);
        compress_worker(&ws);

        // wait for all of the work to complete
        threadset_join(tids, T);

        // squeeze out the holes not used by the compress bound
        compressed_ptr = compressed_base_ptr + sub_block[0].compressed_size;
        for (int i = 1; i < n_sub_blocks; i++) {
            memmove(compressed_ptr, sub_block[i].compressed_ptr, sub_block[i].compressed_size);
            compressed_ptr += sub_block[i].compressed_size;
        }

        compressed_len = compressed_ptr - compressed_base_ptr;
    }
    return compressed_len;
}

// initialize the decompression work
void 
decompress_work_init(struct decompress_work *dw,
                     void *compress_ptr, u_int32_t compress_size,
                     void *uncompress_ptr, u_int32_t uncompress_size,
                     u_int32_t xsum) {
    dw->compress_ptr = compress_ptr; 
    dw->compress_size = compress_size;
    dw->uncompress_ptr = uncompress_ptr; 
    dw->uncompress_size = uncompress_size;
    dw->xsum = xsum;
    dw->error = 0;
}

// decompress one block
int
decompress_sub_block(void *compress_ptr, u_int32_t compress_size, void *uncompress_ptr, u_int32_t uncompress_size, u_int32_t expected_xsum) {
    // verify checksum
    u_int32_t xsum = x1764_memory(compress_ptr, compress_size);
    if (xsum != expected_xsum)
        return EINVAL;

    // decompress
    uLongf destlen = uncompress_size;
    int r = uncompress(uncompress_ptr, &destlen, compress_ptr, compress_size);
    if (r != Z_OK || destlen != uncompress_size)
        return EINVAL;

    return 0;
}

// decompress blocks until there is no more work to do
void *
decompress_worker(void *arg) {
    struct workset *ws = (struct workset *) arg;
    while (1) {
        struct decompress_work *dw = (struct decompress_work *) workset_get(ws);
        if (dw == NULL)
            break;
        dw->error = decompress_sub_block(dw->compress_ptr, dw->compress_size, dw->uncompress_ptr, dw->uncompress_size, dw->xsum);
    }
    return arg;
}

int
decompress_all_sub_blocks(int n_sub_blocks, struct sub_block sub_block[], unsigned char *compressed_data, unsigned char *uncompressed_data, int num_cores) {
    int r;

    if (n_sub_blocks == 1) {
        r = decompress_sub_block(compressed_data, sub_block[0].compressed_size, uncompressed_data, sub_block[0].uncompressed_size, sub_block[0].xsum);
    } else {
        // compute the number of additional threads needed for decompressing this node
        int T = num_cores; // T = min(#cores, #blocks) - 1
        if (T > n_sub_blocks)
            T = n_sub_blocks;
        if (T > 0)
            T = T - 1;       // threads in addition to the running thread

        // init the decompression work set
        struct workset ws;
        workset_init(&ws);

        // initialize the decompression work and add to the work set
        struct decompress_work decompress_work[n_sub_blocks];
        workset_lock(&ws);
        for (int i = 0; i < n_sub_blocks; i++) {
            decompress_work_init(&decompress_work[i], compressed_data, sub_block[i].compressed_size, uncompressed_data, sub_block[i].uncompressed_size, sub_block[i].xsum);
            workset_put_locked(&ws, &decompress_work[i].base);

            uncompressed_data += sub_block[i].uncompressed_size;
            compressed_data += sub_block[i].compressed_size;
        }
        workset_unlock(&ws);
    
        // decompress the sub-blocks
        if (0) printf("%s:%d Cores=%d Blocks=%d T=%d\n", __FUNCTION__, __LINE__, num_cores, n_sub_blocks, T);
        toku_pthread_t tids[T];
        threadset_create(tids, &T, decompress_worker, &ws);
        decompress_worker(&ws);

        // cleanup
        threadset_join(tids, T);
        workset_destroy(&ws);

        r = 0;
        for (int i = 0; i < n_sub_blocks; i++) {
            r = decompress_work[i].error;
            if (r != 0)
                break;
        }
    }

    return r;
}

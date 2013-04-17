#ifndef TOKU_SUB_BLOCK_H
#define TOKU_SUB_BLOCK_H

static const int max_sub_blocks = 8;
static const int target_sub_block_size = 512*1024;

struct sub_block {
    void *uncompressed_ptr;
    u_int32_t uncompressed_size;

    void *compressed_ptr;
    u_int32_t compressed_size;         // real compressed size
    u_int32_t compressed_size_bound;   // estimated compressed size

    u_int32_t xsum;                    // sub block checksum
};

struct stored_sub_block {
    u_int32_t uncompressed_size;
    u_int32_t compressed_size;
    u_int32_t xsum;
};

void 
sub_block_init(struct sub_block *sub_block);

// get the size of the compression header
size_t 
sub_block_header_size(int n_sub_blocks);

// get the sum of the sub block compressed sizes 
size_t 
get_sum_compressed_size_bound(int n_sub_blocks, struct sub_block sub_block[]);

// get the sum of the sub block uncompressed sizes 
size_t 
get_sum_uncompressed_size(int n_sub_blocks, struct sub_block sub_block[]);

// Choose n_sub_blocks and sub_block_size such that the product is >= total_size and the sub_block_size is at
// least >= the target_sub_block_size.
void
choose_sub_block_size(int total_size, int n_sub_blocks_limit, int *sub_block_size_ret, int *n_sub_blocks_ret);

void
set_all_sub_block_sizes(int total_size, int sub_block_size, int n_sub_blocks, struct sub_block sub_block[]);

#include "workset.h"

struct compress_work {
    struct work base;
    struct sub_block *sub_block;
};

void
compress_work_init(struct compress_work *w, struct sub_block *sub_block);

void
compress_sub_block(struct sub_block *sub_block);

void *
compress_worker(void *arg);

size_t
compress_all_sub_blocks(int n_sub_blocks, struct sub_block sub_block[], char *uncompressed_ptr, char *compressed_ptr, int num_cores);

struct decompress_work {
    struct work base;
    void *compress_ptr;
    void *uncompress_ptr;
    u_int32_t compress_size;
    u_int32_t uncompress_size;
    u_int32_t xsum;
    int error;
};

// initialize the decompression work
void 
decompress_work_init(struct decompress_work *dw,
                     void *compress_ptr, u_int32_t compress_size,
                     void *uncompress_ptr, u_int32_t uncompress_size,
                     u_int32_t xsum);

// decompress one block
int
decompress_sub_block(void *compress_ptr, u_int32_t compress_size, void *uncompress_ptr, u_int32_t uncompress_size, u_int32_t expected_xsum);

// decompress blocks until there is no more work to do
void *
decompress_worker(void *arg);

void
decompress_all_sub_blocks(int n_sub_blocks, struct sub_block sub_block[], unsigned char *compressed_data, unsigned char *uncompressed_data, int num_cores);


#endif

/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BLOCKTABLE_H
#define BLOCKTABLE_H
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

typedef struct block_table *BLOCK_TABLE;

//Needed by tests, brtdump
struct block_translation_pair {
    DISKOFF diskoff; // When in free list, set to the next free block.  In this case it's really a BLOCKNUM.
    DISKOFF size;    // set to 0xFFFFFFFFFFFFFFFF for free
};

void toku_block_realloc(BLOCK_TABLE bt, BLOCKNUM b, u_int64_t size, u_int64_t *offset, int *dirty);
void toku_block_alloc(BLOCK_TABLE bt, u_int64_t size, u_int64_t *offset);
void toku_block_free(BLOCK_TABLE bt, u_int64_t offset);
DISKOFF toku_block_get_offset(BLOCK_TABLE bt, BLOCKNUM b);
DISKOFF toku_block_get_size(BLOCK_TABLE bt, BLOCKNUM b);
void toku_block_get_offset_size(BLOCK_TABLE bt, BLOCKNUM b, DISKOFF *offset, DISKOFF *size);
int toku_allocate_diskblocknumber(BLOCK_TABLE bt, BLOCKNUM *res, int *dirty);
int toku_free_diskblocknumber(BLOCK_TABLE bt, BLOCKNUM *b, int *dirty);
void toku_verify_diskblocknumber_allocated(BLOCK_TABLE bt, BLOCKNUM b);
void toku_block_verify_no_free_blocks(BLOCK_TABLE bt);
u_int64_t toku_block_allocator_allocated_limit(BLOCK_TABLE bt);
void toku_block_dump_translation_table(FILE *f, BLOCK_TABLE bt);
void toku_block_dump_translation(BLOCK_TABLE bt, u_int64_t offset);

void toku_blocktable_destroy(BLOCK_TABLE *btp);
void toku_blocktable_debug_set_translation(BLOCK_TABLE bt,
        u_int64_t limit,
        struct block_translation_pair *table);
void toku_blocktable_create(BLOCK_TABLE *btp,
        BLOCKNUM free_blocks,
        BLOCKNUM unused_blocks,
        u_int64_t translated_blocknum_limit,
        u_int64_t block_translation_address_on_disk,
        unsigned char *buffer);
void toku_blocktable_create_from_loggedheader(BLOCK_TABLE *btp, LOGGEDBRTHEADER);
void toku_blocktable_create_new(BLOCK_TABLE *bt);

BLOCKNUM toku_block_get_unused_blocks(BLOCK_TABLE bt);
BLOCKNUM toku_block_get_free_blocks(BLOCK_TABLE bt);
u_int64_t toku_block_get_translated_blocknum_limit(BLOCK_TABLE bt);

void toku_block_memcpy_translation_table(BLOCK_TABLE bt, size_t n, void *p);

//Unlocked/multi ops
void toku_block_lock_for_multiple_operations(BLOCK_TABLE bt);
void toku_block_unlock_for_multiple_operations(BLOCK_TABLE bt);

void toku_block_realloc_translation_unlocked(BLOCK_TABLE bt);
void toku_block_wbuf_free_blocks_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf);
void toku_block_wbuf_unused_blocks_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf);
void toku_block_wbuf_translated_blocknum_limit_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf);
void toku_block_wbuf_block_translation_address_on_disk_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf);
void toku_block_wbuf_init_and_fill_unlocked(BLOCK_TABLE bt, struct wbuf *w,
                                            u_int64_t *size, u_int64_t *address);



#endif


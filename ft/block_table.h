/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef BLOCKTABLE_H
#define BLOCKTABLE_H
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "fttypes.h"


typedef struct block_table *BLOCK_TABLE;

//Needed by tests, ftdump
struct block_translation_pair {
    union { // If in the freelist, use next_free_blocknum, otherwise diskoff.
        DISKOFF  diskoff; 
        BLOCKNUM next_free_blocknum;
    } u;
    DISKOFF size;    // set to 0xFFFFFFFFFFFFFFFF for free
};

void toku_blocktable_create_new(BLOCK_TABLE *btp);
int toku_blocktable_create_from_buffer(int fd, BLOCK_TABLE *btp, DISKOFF location_on_disk, DISKOFF size_on_disk, unsigned char *translation_buffer);
void toku_blocktable_destroy(BLOCK_TABLE *btp);

void toku_ft_lock(FT h);
void toku_ft_unlock(FT h);

void toku_block_translation_note_start_checkpoint_unlocked(BLOCK_TABLE bt);
void toku_block_translation_note_end_checkpoint(BLOCK_TABLE bt, int fd);
void toku_block_translation_note_skipped_checkpoint(BLOCK_TABLE bt);
void toku_maybe_truncate_file_on_open(BLOCK_TABLE bt, int fd);

//Blocknums
void toku_allocate_blocknum(BLOCK_TABLE bt, BLOCKNUM *res, FT h);
void toku_allocate_blocknum_unlocked(BLOCK_TABLE bt, BLOCKNUM *res, FT h);
void toku_free_blocknum(BLOCK_TABLE bt, BLOCKNUM *b, FT h, bool for_checkpoint);
void toku_verify_blocknum_allocated(BLOCK_TABLE bt, BLOCKNUM b);
void toku_block_verify_no_data_blocks_except_root(BLOCK_TABLE bt, BLOCKNUM root);
void toku_free_unused_blocknums(BLOCK_TABLE bt, BLOCKNUM root);
void toku_block_verify_no_free_blocknums(BLOCK_TABLE bt);
void toku_realloc_descriptor_on_disk(BLOCK_TABLE bt, DISKOFF size, DISKOFF *offset, FT h, int fd);
void toku_realloc_descriptor_on_disk_unlocked(BLOCK_TABLE bt, DISKOFF size, DISKOFF *offset, FT h);
void toku_get_descriptor_offset_size(BLOCK_TABLE bt, DISKOFF *offset, DISKOFF *size);

//Blocks and Blocknums
void toku_blocknum_realloc_on_disk(BLOCK_TABLE bt, BLOCKNUM b, DISKOFF size, DISKOFF *offset, FT ft, int fd, bool for_checkpoint);
void toku_translate_blocknum_to_offset_size(BLOCK_TABLE bt, BLOCKNUM b, DISKOFF *offset, DISKOFF *size);

//Serialization
void toku_serialize_translation_to_wbuf(BLOCK_TABLE bt, int fd, struct wbuf *w, int64_t *address, int64_t *size);

void toku_block_table_swap_for_redirect(BLOCK_TABLE old_bt, BLOCK_TABLE new_bt);


//DEBUG ONLY (ftdump included), tests included
void toku_blocknum_dump_translation(BLOCK_TABLE bt, BLOCKNUM b);
void toku_dump_translation_table(FILE *f, BLOCK_TABLE bt);
void toku_block_free(BLOCK_TABLE bt, uint64_t offset);
typedef int(*BLOCKTABLE_CALLBACK)(BLOCKNUM b, int64_t size, int64_t address, void *extra);
enum translation_type {TRANSLATION_NONE=0,
                       TRANSLATION_CURRENT,
                       TRANSLATION_INPROGRESS,
                       TRANSLATION_CHECKPOINTED,
                       TRANSLATION_DEBUG};

int toku_blocktable_iterate(BLOCK_TABLE bt, enum translation_type type, BLOCKTABLE_CALLBACK f, void *extra, bool data_only, bool used_only); 
void toku_blocktable_internal_fragmentation(BLOCK_TABLE bt, int64_t *total_sizep, int64_t *used_sizep);

void toku_block_table_get_fragmentation_unlocked(BLOCK_TABLE bt, TOKU_DB_FRAGMENTATION report);
//Requires:  blocktable lock is held.
//Requires:  report->file_size_bytes is already filled in.

int64_t toku_block_get_blocks_in_use_unlocked(BLOCK_TABLE bt);

//Unmovable reserved first, then reallocable.
// We reserve one blocknum for the translation table itself.
enum {RESERVED_BLOCKNUM_NULL       =0,
      RESERVED_BLOCKNUM_TRANSLATION=1,
      RESERVED_BLOCKNUM_DESCRIPTOR =2,
      RESERVED_BLOCKNUMS};


#endif


/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef BLOCKTABLE_H
#define BLOCKTABLE_H
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
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
void toku_dump_translation_table_pretty(FILE *f, BLOCK_TABLE bt);
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

void toku_blocktable_get_info64(BLOCK_TABLE, struct ftinfo64 *);

int toku_blocktable_iterate_translation_tables(BLOCK_TABLE, uint64_t, int (*)(uint64_t, int64_t, int64_t, int64_t, int64_t, void *), void *);

//Unmovable reserved first, then reallocable.
// We reserve one blocknum for the translation table itself.
enum {RESERVED_BLOCKNUM_NULL       =0,
      RESERVED_BLOCKNUM_TRANSLATION=1,
      RESERVED_BLOCKNUM_DESCRIPTOR =2,
      RESERVED_BLOCKNUMS};


#endif


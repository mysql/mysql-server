/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2014 Tokutek, Inc.

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

#pragma once

#ident "Copyright (c) 2007-2014 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db.h>

#include "portability/toku_stdint.h"
#include "portability/toku_pthread.h"

#include "ft/serialize/block_allocator.h"
#include "util/nb_mutex.h"

struct ft;

typedef struct blocknum_s { int64_t b; } BLOCKNUM;

// Offset in a disk. -1 is the 'null' pointer.
typedef int64_t DISKOFF;

// Unmovable reserved first, then reallocable.
// We reserve one blocknum for the translation table itself.
enum {
    RESERVED_BLOCKNUM_NULL = 0,
    RESERVED_BLOCKNUM_TRANSLATION = 1,
    RESERVED_BLOCKNUM_DESCRIPTOR = 2,
    RESERVED_BLOCKNUMS
};

typedef int (*BLOCKTABLE_CALLBACK)(BLOCKNUM b, int64_t size, int64_t address, void *extra);

static inline BLOCKNUM make_blocknum(int64_t b) {
    BLOCKNUM result = { .b = b };
    return result;
}
static const BLOCKNUM ROLLBACK_NONE = { .b = 0 };

/**
 *  There are three copies of the translation table (btt) in the block table:
 *
 *    checkpointed   Is initialized by deserializing from disk,
 *                   and is the only version ever read from disk.
 *                   When read from disk it is copied to current.
 *                   It is immutable. It can be replaced by an inprogress btt.
 *
 *    inprogress     Is only filled by copying from current,
 *                   and is the only version ever serialized to disk.
 *                   (It is serialized to disk on checkpoint and clean shutdown.)
 *                   At end of checkpoint it replaces 'checkpointed'.
 *                   During a checkpoint, any 'pending' dirty writes will update
 *                   inprogress.
 *
 *    current        Is initialized by copying from checkpointed,
 *                   is the only version ever modified while the database is in use, 
 *                   and is the only version ever copied to inprogress.
 *                   It is never stored on disk.
 */
class block_table {
public:
    enum translation_type {
        TRANSLATION_NONE = 0,
        TRANSLATION_CURRENT,
        TRANSLATION_INPROGRESS,
        TRANSLATION_CHECKPOINTED,
        TRANSLATION_DEBUG
    };

    void create();

    int create_from_buffer(int fd, DISKOFF location_on_disk, DISKOFF size_on_disk, unsigned char *translation_buffer);

    void destroy();

    // Checkpointing
    void note_start_checkpoint_unlocked();
    void note_end_checkpoint(int fd);
    void note_skipped_checkpoint();
    void maybe_truncate_file_on_open(int fd);

    // Blocknums
    void allocate_blocknum(BLOCKNUM *res, struct ft *ft);
    void realloc_on_disk(BLOCKNUM b, DISKOFF size, DISKOFF *offset, struct ft *ft, int fd, bool for_checkpoint, uint64_t heat);
    void free_blocknum(BLOCKNUM *b, struct ft *ft, bool for_checkpoint);
    void translate_blocknum_to_offset_size(BLOCKNUM b, DISKOFF *offset, DISKOFF *size);
    void free_unused_blocknums(BLOCKNUM root);
    void realloc_descriptor_on_disk(DISKOFF size, DISKOFF *offset, struct ft *ft, int fd);
    void get_descriptor_offset_size(DISKOFF *offset, DISKOFF *size);

    // External verfication
    void verify_blocknum_allocated(BLOCKNUM b);
    void verify_no_data_blocks_except_root(BLOCKNUM root);
    void verify_no_free_blocknums();

    // Serialization
    void serialize_translation_to_wbuf(int fd, struct wbuf *w, int64_t *address, int64_t *size);

    // DEBUG ONLY (ftdump included), tests included
    void blocknum_dump_translation(BLOCKNUM b);
    void dump_translation_table_pretty(FILE *f);
    void dump_translation_table(FILE *f);
    void block_free(uint64_t offset);

    int iterate(enum translation_type type, BLOCKTABLE_CALLBACK f, void *extra, bool data_only, bool used_only); 
    void internal_fragmentation(int64_t *total_sizep, int64_t *used_sizep);

    // Requires: blocktable lock is held.
    // Requires: report->file_size_bytes is already filled in.
    void get_fragmentation_unlocked(TOKU_DB_FRAGMENTATION report);

    int64_t get_blocks_in_use_unlocked();

    void get_info64(struct ftinfo64 *);

    int iterate_translation_tables(uint64_t, int (*)(uint64_t, int64_t, int64_t, int64_t, int64_t, void *), void *);

private:
    struct block_translation_pair {
        // If in the freelist, use next_free_blocknum, otherwise diskoff.
        union {
            DISKOFF  diskoff; 
            BLOCKNUM next_free_blocknum;
        } u;

        // Set to 0xFFFFFFFFFFFFFFFF for free
        DISKOFF size;
    };

    // This is the BTT (block translation table)
    // When the translation (btt) is stored on disk:
    //   In Header:
    //       size_on_disk
    //       location_on_disk
    //   In block translation table (in order):
    //       smallest_never_used_blocknum
    //       blocknum_freelist_head
    //       array
    //       a checksum
    struct translation {
        enum translation_type type;

        // Number of elements in array (block_translation).  always >= smallest_never_used_blocknum
        int64_t length_of_array;
        BLOCKNUM smallest_never_used_blocknum;

        // Next (previously used) unused blocknum (free list)
        BLOCKNUM blocknum_freelist_head;
        struct block_translation_pair *block_translation;

        // size_on_disk is stored in block_translation[RESERVED_BLOCKNUM_TRANSLATION].size
        // location_on is stored in block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff
    };

    void _create_internal();
    int _translation_deserialize_from_buffer(struct translation *t,    // destination into which to deserialize
                                             DISKOFF location_on_disk, // location of translation_buffer
                                             uint64_t size_on_disk,
                                             unsigned char * translation_buffer);   // buffer with serialized translation

    void _copy_translation(struct translation *dst, struct translation *src, enum translation_type newtype);
    void _maybe_optimize_translation(struct translation *t);
    void _maybe_expand_translation(struct translation *t);
    bool _translation_prevents_freeing(struct translation *t, BLOCKNUM b, struct block_translation_pair *old_pair);
    void _free_blocknum_in_translation(struct translation *t, BLOCKNUM b);
    int64_t _calculate_size_on_disk(struct translation *t);
    bool _pair_is_unallocated(struct block_translation_pair *pair);
    void _alloc_inprogress_translation_on_disk_unlocked();
    void _dump_translation_internal(FILE *f, struct translation *t);

    // Blocknum management
    void _allocate_blocknum_unlocked(BLOCKNUM *res, struct ft *ft);
    void _free_blocknum_unlocked(BLOCKNUM *bp, struct ft *ft, bool for_checkpoint);
    void _realloc_descriptor_on_disk_unlocked(DISKOFF size, DISKOFF *offset, struct ft *ft);
    void _realloc_on_disk_internal(BLOCKNUM b, DISKOFF size, DISKOFF *offset, struct ft *ft, bool for_checkpoint, uint64_t heat);
    void _translate_blocknum_to_offset_size_unlocked(BLOCKNUM b, DISKOFF *offset, DISKOFF *size);

    // File management
    void _maybe_truncate_file(int fd, uint64_t size_needed_before);
    void _ensure_safe_write_unlocked(int fd, DISKOFF block_size, DISKOFF block_offset);

    // Verification
    bool _is_valid_blocknum(struct translation *t, BLOCKNUM b);
    void _verify_valid_blocknum(struct translation *t, BLOCKNUM b);
    bool _is_valid_freeable_blocknum(struct translation *t, BLOCKNUM b);
    void _verify_valid_freeable_blocknum(struct translation *t, BLOCKNUM b);
    bool _no_data_blocks_except_root(BLOCKNUM root);
    bool _blocknum_allocated(BLOCKNUM b);

    // Locking 
    //
    // TODO: Move the lock to the FT
    void _mutex_lock();
    void _mutex_unlock();

    // The current translation is the one used by client threads. 
    // It is not represented on disk.
    struct translation _current;

    // The translation used by the checkpoint currently in progress. 
    // If the checkpoint thread allocates a block, it must also update the current translation.
    struct translation _inprogress;

    // The translation for the data that shall remain inviolate on disk until the next checkpoint finishes,
    // after which any blocks used only in this translation can be freed.
    struct translation _checkpointed;

    // The in-memory data structure for block allocation. 
    // There is no on-disk data structure for block allocation.
    // Note: This is *allocation* not *translation* - the block allocator is unaware of which
    //       blocks are used for which translation, but simply allocates and deallocates blocks.
    block_allocator _bt_block_allocator;
    toku_mutex_t _mutex;
    struct nb_mutex _safe_file_size_lock;
    bool _checkpoint_skipped;
    uint64_t _safe_file_size;

    // Because the lock is in a weird place right now
    friend void toku_ft_lock(struct ft *ft);
    friend void toku_ft_unlock(struct ft *ft);
};

// For serialize / deserialize

#include "ft/serialize/wbuf.h"

static inline void wbuf_BLOCKNUM (struct wbuf *w, BLOCKNUM b) {
    wbuf_ulonglong(w, b.b);
}

static inline void wbuf_nocrc_BLOCKNUM (struct wbuf *w, BLOCKNUM b) {
    wbuf_nocrc_ulonglong(w, b.b);
}

static inline void wbuf_DISKOFF(struct wbuf *wb, DISKOFF off) {
    wbuf_ulonglong(wb, (uint64_t) off);
}

#include "ft/serialize/rbuf.h"

static inline DISKOFF rbuf_DISKOFF(struct rbuf *rb) {
    return rbuf_ulonglong(rb);
}

static inline BLOCKNUM rbuf_blocknum(struct rbuf *rb) {
    BLOCKNUM result = make_blocknum(rbuf_longlong(rb));
    return result;
}

static inline void rbuf_ma_BLOCKNUM(struct rbuf *rb, memarena *UU(ma), BLOCKNUM *blocknum) {
    *blocknum = rbuf_blocknum(rb);
}

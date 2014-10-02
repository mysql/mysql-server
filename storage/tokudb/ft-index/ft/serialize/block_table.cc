/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#include <config.h>

#include "portability/memory.h"
#include "portability/toku_assert.h"
#include "portability/toku_portability.h"
#include "portability/toku_pthread.h"

// ugly but pragmatic, need access to dirty bits while holding translation lock
// TODO: Refactor this (possibly with FT-301)
#include "ft/ft-internal.h"

// TODO: reorganize this dependency (FT-303)
#include "ft/ft-ops.h" // for toku_maybe_truncate_file
#include "ft/serialize/block_table.h"
#include "ft/serialize/rbuf.h"
#include "ft/serialize/wbuf.h"
#include "ft/serialize/block_allocator.h"

#include "util/nb_mutex.h"
#include "util/scoped_malloc.h"

// indicates the end of a freelist
static const BLOCKNUM freelist_null = { -1 };

// value of block_translation_pair.size if blocknum is unused
static const DISKOFF size_is_free = (DISKOFF) -1;

// value of block_translation_pair.u.diskoff if blocknum is used but does not yet have a diskblock
static const DISKOFF diskoff_unused = (DISKOFF) -2;

void block_table::_mutex_lock() {
    toku_mutex_lock(&_mutex);
}

void block_table::_mutex_unlock() {
    toku_mutex_unlock(&_mutex);
}

// TODO: Move lock to FT
void toku_ft_lock(FT ft) {
    block_table *bt = &ft->blocktable;
    bt->_mutex_lock();
}

// TODO: Move lock to FT
void toku_ft_unlock(FT ft) {
    block_table *bt = &ft->blocktable;
    toku_mutex_assert_locked(&bt->_mutex);
    bt->_mutex_unlock();
}

// There are two headers: the reserve must fit them both and be suitably aligned.
static_assert(block_allocator::BLOCK_ALLOCATOR_HEADER_RESERVE %
              block_allocator::BLOCK_ALLOCATOR_ALIGNMENT == 0,
              "Block allocator's header reserve must be suitibly aligned");
static_assert(block_allocator::BLOCK_ALLOCATOR_HEADER_RESERVE * 2 ==
              block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE,
              "Block allocator's total header reserve must exactly fit two headers");

// does NOT initialize the block allocator: the caller is responsible
void block_table::_create_internal() {
    memset(&_current, 0, sizeof(struct translation));
    memset(&_inprogress, 0, sizeof(struct translation));
    memset(&_checkpointed, 0, sizeof(struct translation));
    memset(&_mutex, 0, sizeof(_mutex));
    toku_mutex_init(&_mutex, nullptr);
    nb_mutex_init(&_safe_file_size_lock);
}

// Fill in the checkpointed translation from buffer, and copy checkpointed to current.
// The one read from disk is the last known checkpointed one, so we are keeping it in 
// place and then setting current (which is never stored on disk) for current use.
// The translation_buffer has translation only, we create the rest of the block_table.
int block_table::create_from_buffer(int fd,
                                    DISKOFF location_on_disk, //Location of translation_buffer
                                    DISKOFF size_on_disk,
                                    unsigned char *translation_buffer) {
    // Does not initialize the block allocator
    _create_internal();

    // Deserialize the translation and copy it to current
    int r = _translation_deserialize_from_buffer(&_checkpointed,
                                                 location_on_disk, size_on_disk,
                                                 translation_buffer);
    if (r != 0) {
        return r;
    }
    _copy_translation(&_current, &_checkpointed, TRANSLATION_CURRENT);

    // Determine the file size
    int64_t file_size;
    r = toku_os_get_file_size(fd, &file_size);
    lazy_assert_zero(r);
    invariant(file_size >= 0);
    _safe_file_size = file_size;

    // Gather the non-empty translations and use them to create the block allocator
    toku::scoped_malloc pairs_buf(_checkpointed.smallest_never_used_blocknum.b *
                                  sizeof(struct block_allocator::blockpair));
    struct block_allocator::blockpair *CAST_FROM_VOIDP(pairs, pairs_buf.get());
    uint64_t n_pairs = 0;
    for (int64_t i = 0; i < _checkpointed.smallest_never_used_blocknum.b; i++) {
        struct block_translation_pair pair = _checkpointed.block_translation[i];
        if (pair.size > 0) {
            invariant(pair.u.diskoff != diskoff_unused);
            pairs[n_pairs++] = block_allocator::blockpair(pair.u.diskoff, pair.size);
        }
    }

    _bt_block_allocator.create_from_blockpairs(block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE,
                                               block_allocator::BLOCK_ALLOCATOR_ALIGNMENT,
                                               pairs, n_pairs);

    return 0;
}

void block_table::create() {
    // Does not initialize the block allocator
    _create_internal();

    _checkpointed.type = TRANSLATION_CHECKPOINTED;
    _checkpointed.smallest_never_used_blocknum = make_blocknum(RESERVED_BLOCKNUMS);
    _checkpointed.length_of_array = _checkpointed.smallest_never_used_blocknum.b;
    _checkpointed.blocknum_freelist_head = freelist_null;
    XMALLOC_N(_checkpointed.length_of_array, _checkpointed.block_translation);
    for (int64_t i = 0; i < _checkpointed.length_of_array; i++) {
        _checkpointed.block_translation[i].size = 0;
        _checkpointed.block_translation[i].u.diskoff = diskoff_unused;
    }

    // we just created a default checkpointed, now copy it to current.  
    _copy_translation(&_current, &_checkpointed, TRANSLATION_CURRENT);

    // Create an empty block allocator.
    _bt_block_allocator.create(block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE,
                               block_allocator::BLOCK_ALLOCATOR_ALIGNMENT);
}

// TODO: Refactor with FT-303
static void ft_set_dirty(FT ft, bool for_checkpoint) {
    invariant(ft->h->type == FT_CURRENT);
    if (for_checkpoint) {
        invariant(ft->checkpoint_header->type == FT_CHECKPOINT_INPROGRESS);
        ft->checkpoint_header->dirty = 1;
    } else {
        ft->h->dirty = 1;
    }
}

void block_table::_maybe_truncate_file(int fd, uint64_t size_needed_before) {
    toku_mutex_assert_locked(&_mutex);
    uint64_t new_size_needed = _bt_block_allocator.allocated_limit();
    //Save a call to toku_os_get_file_size (kernel call) if unlikely to be useful.
    if (new_size_needed < size_needed_before && new_size_needed < _safe_file_size) {
        nb_mutex_lock(&_safe_file_size_lock, &_mutex);

        // Must hold _safe_file_size_lock to change _safe_file_size.
        if (new_size_needed < _safe_file_size) {
            int64_t safe_file_size_before = _safe_file_size;
            // Not safe to use the 'to-be-truncated' portion until truncate is done.
            _safe_file_size = new_size_needed;
            _mutex_unlock();

            uint64_t size_after;
            toku_maybe_truncate_file(fd, new_size_needed, safe_file_size_before, &size_after);
            _mutex_lock();

            _safe_file_size = size_after;
        }
        nb_mutex_unlock(&_safe_file_size_lock);
    }
}

void block_table::maybe_truncate_file_on_open(int fd) {
    _mutex_lock();
    _maybe_truncate_file(fd, _safe_file_size);
    _mutex_unlock();
}

void block_table::_copy_translation(struct translation *dst, struct translation *src, enum translation_type newtype) {
    // We intend to malloc a fresh block, so the incoming translation should be empty
    invariant_null(dst->block_translation);

    invariant(src->length_of_array >= src->smallest_never_used_blocknum.b);
    invariant(newtype == TRANSLATION_DEBUG ||
              (src->type == TRANSLATION_CURRENT && newtype == TRANSLATION_INPROGRESS) ||
              (src->type == TRANSLATION_CHECKPOINTED && newtype == TRANSLATION_CURRENT));
    dst->type = newtype;
    dst->smallest_never_used_blocknum = src->smallest_never_used_blocknum;
    dst->blocknum_freelist_head = src->blocknum_freelist_head; 

    // destination btt is of fixed size. Allocate + memcpy the exact length necessary.
    dst->length_of_array = dst->smallest_never_used_blocknum.b;
    XMALLOC_N(dst->length_of_array, dst->block_translation);
    memcpy(dst->block_translation, src->block_translation, dst->length_of_array * sizeof(*dst->block_translation));

    // New version of btt is not yet stored on disk.
    dst->block_translation[RESERVED_BLOCKNUM_TRANSLATION].size = 0;
    dst->block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff = diskoff_unused;
}

int64_t block_table::get_blocks_in_use_unlocked() {
    BLOCKNUM b;
    struct translation *t = &_current;
    int64_t num_blocks = 0;
    {
        //Reserved blocknums do not get upgraded; They are part of the header.
        for (b.b = RESERVED_BLOCKNUMS; b.b < t->smallest_never_used_blocknum.b; b.b++) {
            if (t->block_translation[b.b].size != size_is_free) {
                num_blocks++;
            }
        }
    }
    return num_blocks;
}

void block_table::_maybe_optimize_translation(struct translation *t) {
    //Reduce 'smallest_never_used_blocknum.b' (completely free blocknums instead of just
    //on a free list.  Doing so requires us to regenerate the free list.
    //This is O(n) work, so do it only if you're already doing that.

    BLOCKNUM b;
    paranoid_invariant(t->smallest_never_used_blocknum.b >= RESERVED_BLOCKNUMS);
    //Calculate how large the free suffix is.
    int64_t freed;
    {
        for (b.b = t->smallest_never_used_blocknum.b; b.b > RESERVED_BLOCKNUMS; b.b--) {
            if (t->block_translation[b.b-1].size != size_is_free) {
                break;
            }
        }
        freed = t->smallest_never_used_blocknum.b - b.b;
    }
    if (freed>0) {
        t->smallest_never_used_blocknum.b = b.b;
        if (t->length_of_array/4 > t->smallest_never_used_blocknum.b) {
            //We're using more memory than necessary to represent this now.  Reduce.
            uint64_t new_length = t->smallest_never_used_blocknum.b * 2;
            XREALLOC_N(new_length, t->block_translation);
            t->length_of_array = new_length;
            //No need to zero anything out. 
        }

        //Regenerate free list.
        t->blocknum_freelist_head.b = freelist_null.b;
        for (b.b = RESERVED_BLOCKNUMS; b.b < t->smallest_never_used_blocknum.b; b.b++) {
            if (t->block_translation[b.b].size == size_is_free) {
                t->block_translation[b.b].u.next_free_blocknum = t->blocknum_freelist_head;
                t->blocknum_freelist_head                      = b;
            }
        }
    }
}

// block table must be locked by caller of this function
void block_table::note_start_checkpoint_unlocked() {
    toku_mutex_assert_locked(&_mutex);

    // We're going to do O(n) work to copy the translation, so we
    // can afford to do O(n) work by optimizing the translation
    _maybe_optimize_translation(&_current);

    // Copy current translation to inprogress translation.
    _copy_translation(&_inprogress, &_current, TRANSLATION_INPROGRESS);

    _checkpoint_skipped = false;
}

void block_table::note_skipped_checkpoint() {
    //Purpose, alert block translation that the checkpoint was skipped, e.x. for a non-dirty header
    _mutex_lock();
    paranoid_invariant_notnull(_inprogress.block_translation);
    _checkpoint_skipped = true;
    _mutex_unlock();
}

// Purpose: free any disk space used by previous checkpoint that isn't in use by either
//           - current state
//           - in-progress checkpoint
//          capture inprogress as new checkpointed.
// For each entry in checkpointBTT
//   if offset does not match offset in inprogress
//      assert offset does not match offset in current
//      free (offset,len) from checkpoint
// move inprogress to checkpoint (resetting type)
// inprogress = NULL
void block_table::note_end_checkpoint(int fd) {
    // Free unused blocks
    _mutex_lock();
    uint64_t allocated_limit_at_start = _bt_block_allocator.allocated_limit();
    paranoid_invariant_notnull(_inprogress.block_translation);
    if (_checkpoint_skipped) {
        toku_free(_inprogress.block_translation);
        memset(&_inprogress, 0, sizeof(_inprogress));
        goto end;
    }

    //Make certain inprogress was allocated space on disk
    assert(_inprogress.block_translation[RESERVED_BLOCKNUM_TRANSLATION].size > 0);
    assert(_inprogress.block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff > 0);

    {
        struct translation *t = &_checkpointed;
        for (int64_t i = 0; i < t->length_of_array; i++) {
            struct block_translation_pair *pair = &t->block_translation[i];
            if (pair->size > 0 && !_translation_prevents_freeing(&_inprogress, make_blocknum(i), pair)) {
                assert(!_translation_prevents_freeing(&_current, make_blocknum(i), pair));
                _bt_block_allocator.free_block(pair->u.diskoff);
            }
        }
        toku_free(_checkpointed.block_translation);
        _checkpointed = _inprogress;
        _checkpointed.type = TRANSLATION_CHECKPOINTED;
        memset(&_inprogress, 0, sizeof(_inprogress));
        _maybe_truncate_file(fd, allocated_limit_at_start);
    }
end:
    _mutex_unlock();
}

bool block_table::_is_valid_blocknum(struct translation *t, BLOCKNUM b) {
    invariant(t->length_of_array >= t->smallest_never_used_blocknum.b);
    return b.b >= 0 && b.b < t->smallest_never_used_blocknum.b;
}

void block_table::_verify_valid_blocknum(struct translation *UU(t), BLOCKNUM UU(b)) {
    invariant(_is_valid_blocknum(t, b));
}

bool block_table::_is_valid_freeable_blocknum(struct translation *t, BLOCKNUM b) {
    invariant(t->length_of_array >= t->smallest_never_used_blocknum.b);
    return b.b >= RESERVED_BLOCKNUMS && b.b < t->smallest_never_used_blocknum.b;
}

// should be freeable
void block_table::_verify_valid_freeable_blocknum(struct translation *UU(t), BLOCKNUM UU(b)) {
    invariant(_is_valid_freeable_blocknum(t, b));
}

// Also used only in ft-serialize-test.
void block_table::block_free(uint64_t offset) {
    _mutex_lock();
    _bt_block_allocator.free_block(offset);
    _mutex_unlock();
}

int64_t block_table::_calculate_size_on_disk(struct translation *t) {
    return 8 + // smallest_never_used_blocknum
           8 + // blocknum_freelist_head
           t->smallest_never_used_blocknum.b * 16 + // Array
           4; // 4 for checksum
}

// We cannot free the disk space allocated to this blocknum if it is still in use by the given translation table.
bool block_table::_translation_prevents_freeing(struct translation *t, BLOCKNUM b, struct block_translation_pair *old_pair) {
    return t->block_translation &&
           b.b < t->smallest_never_used_blocknum.b &&
           old_pair->u.diskoff == t->block_translation[b.b].u.diskoff;
}

void block_table::_realloc_on_disk_internal(BLOCKNUM b, DISKOFF size, DISKOFF *offset, FT ft, bool for_checkpoint, uint64_t heat) {
    toku_mutex_assert_locked(&_mutex);
    ft_set_dirty(ft, for_checkpoint);

    struct translation *t = &_current;
    struct block_translation_pair old_pair = t->block_translation[b.b];
    //Free the old block if it is not still in use by the checkpoint in progress or the previous checkpoint
    bool cannot_free = (bool)
        ((!for_checkpoint && _translation_prevents_freeing(&_inprogress,   b, &old_pair)) ||
         _translation_prevents_freeing(&_checkpointed, b, &old_pair));
    if (!cannot_free && old_pair.u.diskoff!=diskoff_unused) {
        _bt_block_allocator.free_block(old_pair.u.diskoff);
    }

    uint64_t allocator_offset = diskoff_unused;
    t->block_translation[b.b].size = size;
    if (size > 0) {
        // Allocate a new block if the size is greater than 0,
        // if the size is just 0, offset will be set to diskoff_unused
        _bt_block_allocator.alloc_block(size, heat, &allocator_offset);
    }
    t->block_translation[b.b].u.diskoff = allocator_offset;
    *offset = allocator_offset;

    //Update inprogress btt if appropriate (if called because Pending bit is set).
    if (for_checkpoint) {
        paranoid_invariant(b.b < _inprogress.length_of_array);
        _inprogress.block_translation[b.b] = t->block_translation[b.b];
    }
}

void block_table::_ensure_safe_write_unlocked(int fd, DISKOFF block_size, DISKOFF block_offset) {
    // Requires: holding _mutex
    uint64_t size_needed = block_size + block_offset;
    if (size_needed > _safe_file_size) {
        // Must hold _safe_file_size_lock to change _safe_file_size.
        nb_mutex_lock(&_safe_file_size_lock, &_mutex);
        if (size_needed > _safe_file_size) {
            _mutex_unlock();

            int64_t size_after;
            toku_maybe_preallocate_in_file(fd, size_needed, _safe_file_size, &size_after);

            _mutex_lock();
            _safe_file_size = size_after;
        }
        nb_mutex_unlock(&_safe_file_size_lock);
    }
}

void block_table::realloc_on_disk(BLOCKNUM b, DISKOFF size, DISKOFF *offset, FT ft, int fd, bool for_checkpoint, uint64_t heat) {
    _mutex_lock();
    struct translation *t = &_current;
    _verify_valid_freeable_blocknum(t, b);
    _realloc_on_disk_internal(b, size, offset, ft, for_checkpoint, heat);

    _ensure_safe_write_unlocked(fd, size, *offset);
    _mutex_unlock();
}

bool block_table::_pair_is_unallocated(struct block_translation_pair *pair) {
    return pair->size == 0 && pair->u.diskoff == diskoff_unused;
}

// Effect: figure out where to put the inprogress btt on disk, allocate space for it there.
//   The space must be 512-byte aligned (both the starting address and the size).
//   As a result, the allcoated space may be a little bit bigger (up to the next 512-byte boundary) than the actual btt.
void block_table::_alloc_inprogress_translation_on_disk_unlocked() {
    toku_mutex_assert_locked(&_mutex);

    struct translation *t = &_inprogress;
    paranoid_invariant_notnull(t->block_translation);
    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_TRANSLATION);
    //Each inprogress is allocated only once
    paranoid_invariant(_pair_is_unallocated(&t->block_translation[b.b]));

    //Allocate a new block
    int64_t size = _calculate_size_on_disk(t);
    uint64_t offset;
    _bt_block_allocator.alloc_block(size, 0, &offset);
    t->block_translation[b.b].u.diskoff = offset;
    t->block_translation[b.b].size      = size;
}

// Effect: Serializes the blocktable to a wbuf (which starts uninitialized)
//   A clean shutdown runs checkpoint start so that current and inprogress are copies.
//   The resulting wbuf buffer is guaranteed to be be 512-byte aligned and the total length is a multiple of 512 (so we pad with zeros at the end if needd)
//   The address is guaranteed to be 512-byte aligned, but the size is not guaranteed.
//   It *is* guaranteed that we can read up to the next 512-byte boundary, however
void block_table::serialize_translation_to_wbuf(int fd, struct wbuf *w,
                                                int64_t *address, int64_t *size) {
    _mutex_lock();
    struct translation *t = &_inprogress;

    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_TRANSLATION);
    _alloc_inprogress_translation_on_disk_unlocked(); // The allocated block must be 512-byte aligned to make O_DIRECT happy.
    uint64_t size_translation = _calculate_size_on_disk(t);
    uint64_t size_aligned     = roundup_to_multiple(512, size_translation);
    assert((int64_t)size_translation==t->block_translation[b.b].size);
    {
        //Init wbuf
        if (0)
            printf("%s:%d writing translation table of size_translation %" PRIu64 " at %" PRId64 "\n", __FILE__, __LINE__, size_translation, t->block_translation[b.b].u.diskoff);
        char *XMALLOC_N_ALIGNED(512, size_aligned, buf);
        for (uint64_t i=size_translation; i<size_aligned; i++) buf[i]=0; // fill in the end of the buffer with zeros.
        wbuf_init(w, buf, size_aligned);
    }
    wbuf_BLOCKNUM(w, t->smallest_never_used_blocknum); 
    wbuf_BLOCKNUM(w, t->blocknum_freelist_head); 
    int64_t i;
    for (i=0; i<t->smallest_never_used_blocknum.b; i++) {
        if (0)
            printf("%s:%d %" PRId64 ",%" PRId64 "\n", __FILE__, __LINE__, t->block_translation[i].u.diskoff, t->block_translation[i].size);
        wbuf_DISKOFF(w, t->block_translation[i].u.diskoff);
        wbuf_DISKOFF(w, t->block_translation[i].size);
    }
    uint32_t checksum = toku_x1764_finish(&w->checksum);
    wbuf_int(w, checksum);
    *address = t->block_translation[b.b].u.diskoff;
    *size    = size_translation;
    assert((*address)%512 == 0);

    _ensure_safe_write_unlocked(fd, size_aligned, *address);
    _mutex_unlock();
}

// Perhaps rename: purpose is get disk address of a block, given its blocknum (blockid?)
void block_table::_translate_blocknum_to_offset_size_unlocked(BLOCKNUM b, DISKOFF *offset, DISKOFF *size) {
    struct translation *t = &_current;
    _verify_valid_blocknum(t, b);
    if (offset) {
        *offset = t->block_translation[b.b].u.diskoff;
    }
    if (size) {
        *size = t->block_translation[b.b].size;
    }
}

// Perhaps rename: purpose is get disk address of a block, given its blocknum (blockid?)
void block_table::translate_blocknum_to_offset_size(BLOCKNUM b, DISKOFF *offset, DISKOFF *size) {
    _mutex_lock();
    _translate_blocknum_to_offset_size_unlocked(b, offset, size);
    _mutex_unlock();
}

// Only called by toku_allocate_blocknum
// Effect: expand the array to maintain size invariant
// given that one more never-used blocknum will soon be used.
void block_table::_maybe_expand_translation(struct translation *t) {
    if (t->length_of_array <= t->smallest_never_used_blocknum.b) {
        //expansion is necessary
        uint64_t new_length = t->smallest_never_used_blocknum.b * 2;
        XREALLOC_N(new_length, t->block_translation);
        uint64_t i;
        for (i = t->length_of_array; i < new_length; i++) {
            t->block_translation[i].u.next_free_blocknum = freelist_null;
            t->block_translation[i].size                 = size_is_free;
        }
        t->length_of_array = new_length;
    }
}

void block_table::_allocate_blocknum_unlocked(BLOCKNUM *res, FT ft) {
    toku_mutex_assert_locked(&_mutex);
    BLOCKNUM result;
    struct translation *t = &_current;
    if (t->blocknum_freelist_head.b == freelist_null.b) {
        // no previously used blocknums are available
        // use a never used blocknum
        _maybe_expand_translation(t); //Ensure a never used blocknums is available
        result = t->smallest_never_used_blocknum;
        t->smallest_never_used_blocknum.b++;
    } else {  // reuse a previously used blocknum
        result = t->blocknum_freelist_head;
        BLOCKNUM next = t->block_translation[result.b].u.next_free_blocknum;
        t->blocknum_freelist_head = next;
    }
    //Verify the blocknum is free
    paranoid_invariant(t->block_translation[result.b].size == size_is_free);
    //blocknum is not free anymore
    t->block_translation[result.b].u.diskoff = diskoff_unused;
    t->block_translation[result.b].size    = 0;
    _verify_valid_freeable_blocknum(t, result);
    *res = result;
    ft_set_dirty(ft, false);
}

void block_table::allocate_blocknum(BLOCKNUM *res, FT ft) {
    _mutex_lock();
    _allocate_blocknum_unlocked(res, ft);
    _mutex_unlock();
}

void block_table::_free_blocknum_in_translation(struct translation *t, BLOCKNUM b) {
    _verify_valid_freeable_blocknum(t, b);
    paranoid_invariant(t->block_translation[b.b].size != size_is_free);

    t->block_translation[b.b].size                 = size_is_free;
    t->block_translation[b.b].u.next_free_blocknum = t->blocknum_freelist_head;
    t->blocknum_freelist_head                      = b;
}

// Effect: Free a blocknum.
// If the blocknum holds the only reference to a block on disk, free that block
void block_table::_free_blocknum_unlocked(BLOCKNUM *bp, FT ft, bool for_checkpoint) {
    toku_mutex_assert_locked(&_mutex);
    BLOCKNUM b = *bp;
    bp->b = 0; //Remove caller's reference.

    struct block_translation_pair old_pair = _current.block_translation[b.b];

    _free_blocknum_in_translation(&_current, b);
    if (for_checkpoint) {
        paranoid_invariant(ft->checkpoint_header->type == FT_CHECKPOINT_INPROGRESS);
        _free_blocknum_in_translation(&_inprogress, b);
    }

    //If the size is 0, no disk block has ever been assigned to this blocknum.
    if (old_pair.size > 0) {
        //Free the old block if it is not still in use by the checkpoint in progress or the previous checkpoint
        bool cannot_free = (bool)
            (_translation_prevents_freeing(&_inprogress,   b, &old_pair) ||
             _translation_prevents_freeing(&_checkpointed, b, &old_pair));
        if (!cannot_free) {
            _bt_block_allocator.free_block(old_pair.u.diskoff);
        }
    }
    else {
        paranoid_invariant(old_pair.size==0);
        paranoid_invariant(old_pair.u.diskoff == diskoff_unused);
    }
    ft_set_dirty(ft, for_checkpoint);
}

void block_table::free_blocknum(BLOCKNUM *bp, FT ft, bool for_checkpoint) {
    _mutex_lock();
    _free_blocknum_unlocked(bp, ft, for_checkpoint);
    _mutex_unlock();
}

// Verify there are no free blocks.
void block_table::verify_no_free_blocknums() {
    invariant(_current.blocknum_freelist_head.b == freelist_null.b);
}

// Frees blocknums that have a size of 0 and unused diskoff
// Currently used for eliminating unused cached rollback log nodes
void block_table::free_unused_blocknums(BLOCKNUM root) {
    _mutex_lock();
    int64_t smallest = _current.smallest_never_used_blocknum.b;
    for (int64_t i=RESERVED_BLOCKNUMS; i < smallest; i++) {
        if (i == root.b) {
            continue;
        }
        BLOCKNUM b = make_blocknum(i);
        if (_current.block_translation[b.b].size == 0) {
            invariant(_current.block_translation[b.b].u.diskoff == diskoff_unused);
            _free_blocknum_in_translation(&_current, b);
        }
    }
    _mutex_unlock();
}

bool block_table::_no_data_blocks_except_root(BLOCKNUM root) {
    bool ok = true;
    _mutex_lock();
    int64_t smallest = _current.smallest_never_used_blocknum.b;
    if (root.b < RESERVED_BLOCKNUMS) {
        ok = false;
        goto cleanup;
    }
    for (int64_t i = RESERVED_BLOCKNUMS; i < smallest; i++) {
        if (i == root.b) {
            continue;
        }
        BLOCKNUM b = make_blocknum(i);
        if (_current.block_translation[b.b].size != size_is_free) {
            ok = false;
            goto cleanup;
        }
    }
 cleanup:
    _mutex_unlock();
    return ok;
}

// Verify there are no data blocks except root.
// TODO(leif): This actually takes a lock, but I don't want to fix all the callers right now.
void block_table::verify_no_data_blocks_except_root(BLOCKNUM UU(root)) {
    paranoid_invariant(_no_data_blocks_except_root(root));
}

bool block_table::_blocknum_allocated(BLOCKNUM b) {
    _mutex_lock();
    struct translation *t = &_current;
    _verify_valid_blocknum(t, b);
    bool ok = t->block_translation[b.b].size != size_is_free;
    _mutex_unlock();
    return ok;
}

// Verify a blocknum is currently allocated.
void block_table::verify_blocknum_allocated(BLOCKNUM UU(b)) {
    paranoid_invariant(_blocknum_allocated(b));
}

// Only used by toku_dump_translation table (debug info)
void block_table::_dump_translation_internal(FILE *f, struct translation *t) {
    if (t->block_translation) {
        BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_TRANSLATION);
        fprintf(f, " length_of_array[%" PRId64 "]", t->length_of_array);
        fprintf(f, " smallest_never_used_blocknum[%" PRId64 "]", t->smallest_never_used_blocknum.b);
        fprintf(f, " blocknum_free_list_head[%" PRId64 "]", t->blocknum_freelist_head.b);
        fprintf(f, " size_on_disk[%" PRId64 "]", t->block_translation[b.b].size);
        fprintf(f, " location_on_disk[%" PRId64 "]\n", t->block_translation[b.b].u.diskoff);
        int64_t i;
        for (i=0; i<t->length_of_array; i++) {
            fprintf(f, " %" PRId64 ": %" PRId64 " %" PRId64 "\n", i, t->block_translation[i].u.diskoff, t->block_translation[i].size);
        }
        fprintf(f, "\n");
    } else {
        fprintf(f, " does not exist\n");
    }
}

// Only used by toku_ft_dump which is only for debugging purposes
// "pretty" just means we use tabs so we can parse output easier later
void block_table::dump_translation_table_pretty(FILE *f) {
    _mutex_lock();
    struct translation *t = &_checkpointed;
    assert(t->block_translation != nullptr);
    for (int64_t i = 0; i < t->length_of_array; ++i) {
        fprintf(f, "%" PRId64 "\t%" PRId64 "\t%" PRId64 "\n", i, t->block_translation[i].u.diskoff, t->block_translation[i].size);
    }
    _mutex_unlock();
}

// Only used by toku_ft_dump which is only for debugging purposes
void block_table::dump_translation_table(FILE *f) {
    _mutex_lock();
    fprintf(f, "Current block translation:");
    _dump_translation_internal(f, &_current);
    fprintf(f, "Checkpoint in progress block translation:");
    _dump_translation_internal(f, &_inprogress);
    fprintf(f, "Checkpointed block translation:");
    _dump_translation_internal(f, &_checkpointed);
    _mutex_unlock();
}

// Only used by ftdump
void block_table::blocknum_dump_translation(BLOCKNUM b) {
    _mutex_lock();

    struct translation *t = &_current;
    if (b.b < t->length_of_array) {
        struct block_translation_pair *bx = &t->block_translation[b.b];
        printf("%" PRId64 ": %" PRId64 " %" PRId64 "\n", b.b, bx->u.diskoff, bx->size);
    }
    _mutex_unlock();
}

// Must not call this function when anything else is using the blocktable.
// No one may use the blocktable afterwards.
void block_table::destroy(void) {
    // TODO: translation.destroy();
    toku_free(_current.block_translation);
    toku_free(_inprogress.block_translation);
    toku_free(_checkpointed.block_translation);

    _bt_block_allocator.destroy();
    toku_mutex_destroy(&_mutex);
    nb_mutex_destroy(&_safe_file_size_lock);
}

int block_table::_translation_deserialize_from_buffer(struct translation *t,
                                                      DISKOFF location_on_disk,
                                                      uint64_t size_on_disk,
                                                      // out: buffer with serialized translation
                                                      unsigned char *translation_buffer) {
    int r = 0;
    assert(location_on_disk != 0);
    t->type = TRANSLATION_CHECKPOINTED;

    // check the checksum
    uint32_t x1764 = toku_x1764_memory(translation_buffer, size_on_disk - 4);
    uint64_t offset = size_on_disk - 4;
    uint32_t stored_x1764 = toku_dtoh32(*(int*)(translation_buffer + offset));
    if (x1764 != stored_x1764) {
        fprintf(stderr, "Translation table checksum failure: calc=0x%08x read=0x%08x\n", x1764, stored_x1764);
        r = TOKUDB_BAD_CHECKSUM;
        goto exit;
    }

    struct rbuf rb;
    rb.buf = translation_buffer;
    rb.ndone = 0;
    rb.size = size_on_disk-4;//4==checksum

    t->smallest_never_used_blocknum = rbuf_blocknum(&rb); 
    t->length_of_array = t->smallest_never_used_blocknum.b;
    invariant(t->smallest_never_used_blocknum.b >= RESERVED_BLOCKNUMS);
    t->blocknum_freelist_head = rbuf_blocknum(&rb); 
    XMALLOC_N(t->length_of_array, t->block_translation);
    for (int64_t i = 0; i < t->length_of_array; i++) {
        t->block_translation[i].u.diskoff = rbuf_DISKOFF(&rb);
        t->block_translation[i].size = rbuf_DISKOFF(&rb);
    }
    invariant(_calculate_size_on_disk(t) == (int64_t) size_on_disk);
    invariant(t->block_translation[RESERVED_BLOCKNUM_TRANSLATION].size == (int64_t) size_on_disk);
    invariant(t->block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff == location_on_disk);

exit:
    return r;
}

int block_table::iterate(enum translation_type type,
                         BLOCKTABLE_CALLBACK f, void *extra, bool data_only, bool used_only) {
    struct translation *src;
    
    int r = 0;
    switch (type) {
    case TRANSLATION_CURRENT:
        src = &_current;
        break;
    case TRANSLATION_INPROGRESS:
        src = &_inprogress;
        break;
    case TRANSLATION_CHECKPOINTED:
        src = &_checkpointed;
        break;
    default:
        r = EINVAL;
    }

    struct translation fakecurrent;
    memset(&fakecurrent, 0, sizeof(struct translation));

    struct translation *t = &fakecurrent;
    if (r == 0) {
        _mutex_lock();
        _copy_translation(t, src, TRANSLATION_DEBUG);
        t->block_translation[RESERVED_BLOCKNUM_TRANSLATION] =
            src->block_translation[RESERVED_BLOCKNUM_TRANSLATION];
        _mutex_unlock();
        int64_t i;
        for (i=0; i<t->smallest_never_used_blocknum.b; i++) {
            struct block_translation_pair pair = t->block_translation[i];
            if (data_only && i< RESERVED_BLOCKNUMS) continue;
            if (used_only && pair.size <= 0) continue;
            r = f(make_blocknum(i), pair.size, pair.u.diskoff, extra);
            if (r!=0) break;
        }
        toku_free(t->block_translation);
    }
    return r;
}

typedef struct {
    int64_t used_space;
    int64_t total_space;
} frag_extra;

static int frag_helper(BLOCKNUM UU(b), int64_t size, int64_t address, void *extra) {
    frag_extra *info = (frag_extra *) extra;

    if (size + address > info->total_space)
        info->total_space = size + address;
    info->used_space += size;
    return 0;
}

void block_table::internal_fragmentation(int64_t *total_sizep, int64_t *used_sizep) {
    frag_extra info = { 0, 0 };
    int r = iterate(TRANSLATION_CHECKPOINTED, frag_helper, &info, false, true);
    assert_zero(r);

    if (total_sizep) *total_sizep = info.total_space;
    if (used_sizep)  *used_sizep  = info.used_space;
}

void block_table::_realloc_descriptor_on_disk_unlocked(DISKOFF size, DISKOFF *offset, FT ft) {
    toku_mutex_assert_locked(&_mutex);
    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_DESCRIPTOR);
    _realloc_on_disk_internal(b, size, offset, ft, false, 0);
}

void block_table::realloc_descriptor_on_disk(DISKOFF size, DISKOFF *offset, FT ft, int fd) {
    _mutex_lock();
    _realloc_descriptor_on_disk_unlocked(size, offset, ft);
    _ensure_safe_write_unlocked(fd, size, *offset);
    _mutex_unlock();
}

void block_table::get_descriptor_offset_size(DISKOFF *offset, DISKOFF *size) {
    _mutex_lock();
    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_DESCRIPTOR);
    _translate_blocknum_to_offset_size_unlocked(b, offset, size);
    _mutex_unlock();
}

void block_table::get_fragmentation_unlocked(TOKU_DB_FRAGMENTATION report) {
    // Requires:  blocktable lock is held.
    // Requires:  report->file_size_bytes is already filled in.
    
    // Count the headers.
    report->data_bytes = block_allocator::BLOCK_ALLOCATOR_HEADER_RESERVE;
    report->data_blocks = 1;
    report->checkpoint_bytes_additional = block_allocator::BLOCK_ALLOCATOR_HEADER_RESERVE;
    report->checkpoint_blocks_additional = 1;

    struct translation *current = &_current;
    for (int64_t i = 0; i < current->length_of_array; i++) {
        struct block_translation_pair *pair = &current->block_translation[i];
        if (pair->size > 0) {
            report->data_bytes += pair->size;
            report->data_blocks++;
        }
    }

    struct translation *checkpointed = &_checkpointed;
    for (int64_t i = 0; i < checkpointed->length_of_array; i++) {
        struct block_translation_pair *pair = &checkpointed->block_translation[i];
        if (pair->size > 0 && !(i < current->length_of_array &&
                                current->block_translation[i].size > 0 &&
                                current->block_translation[i].u.diskoff == pair->u.diskoff)) {
                report->checkpoint_bytes_additional += pair->size;
                report->checkpoint_blocks_additional++;
        }
    }

    struct translation *inprogress = &_inprogress;
    for (int64_t i = 0; i < inprogress->length_of_array; i++) {
        struct block_translation_pair *pair = &inprogress->block_translation[i];
        if (pair->size > 0 && !(i < current->length_of_array &&
                                current->block_translation[i].size > 0 &&
                                current->block_translation[i].u.diskoff == pair->u.diskoff) &&
                              !(i < checkpointed->length_of_array &&
                                checkpointed->block_translation[i].size > 0 &&
                                checkpointed->block_translation[i].u.diskoff == pair->u.diskoff)) {
            report->checkpoint_bytes_additional += pair->size;
            report->checkpoint_blocks_additional++;
        }
    }

    _bt_block_allocator.get_unused_statistics(report);
}

void block_table::get_info64(struct ftinfo64 *s) {
    _mutex_lock();

    struct translation *current = &_current;
    s->num_blocks_allocated = current->length_of_array;
    s->num_blocks_in_use = 0;
    s->size_allocated = 0;
    s->size_in_use = 0;

    for (int64_t i = 0; i < current->length_of_array; ++i) {
        struct block_translation_pair *block = &current->block_translation[i];
        if (block->size != size_is_free) {
            ++s->num_blocks_in_use;
            s->size_in_use += block->size;
            if (block->u.diskoff != diskoff_unused) {
                uint64_t limit = block->u.diskoff + block->size;
                if (limit > s->size_allocated) {
                    s->size_allocated = limit;
                }
            }
        }
    }

    _mutex_unlock();
}

int block_table::iterate_translation_tables(uint64_t checkpoint_count,
                                            int (*iter)(uint64_t checkpoint_count,
                                                        int64_t total_num_rows,
                                                        int64_t blocknum,
                                                        int64_t diskoff,
                                                        int64_t size,
                                                        void *extra),
                                            void *iter_extra) {
    int error = 0;
    _mutex_lock();

    int64_t total_num_rows = _current.length_of_array + _checkpointed.length_of_array;
    for (int64_t i = 0; error == 0 && i < _current.length_of_array; ++i) {
        struct block_translation_pair *block = &_current.block_translation[i];
        error = iter(checkpoint_count, total_num_rows, i, block->u.diskoff, block->size, iter_extra);
    }
    for (int64_t i = 0; error == 0 && i < _checkpointed.length_of_array; ++i) {
        struct block_translation_pair *block = &_checkpointed.block_translation[i];
        error = iter(checkpoint_count - 1, total_num_rows, i, block->u.diskoff, block->size, iter_extra);
    }

    _mutex_unlock();
    return error;
}

/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "brt-internal.h"	// ugly but pragmatic, need access to dirty bits while holding translation lock
#include "brttypes.h"
#include "block_table.h"
#include "memory.h"
#include "toku_assert.h"
#include "toku_pthread.h"
#include "block_allocator.h"
#include "rbuf.h"
#include "wbuf.h"

//When the translation (btt) is stored on disk:
//  In Header:
//      size_on_disk
//      location_on_disk
//  In block translation table (in order):
//      smallest_never_used_blocknum
//      blocknum_freelist_head
//      array
//      a checksum
struct translation { //This is the BTT (block translation table)
    enum translation_type type;
    int64_t length_of_array;                           //Number of elements in array (block_translation).  always >= smallest_never_used_blocknum
    BLOCKNUM smallest_never_used_blocknum;
    BLOCKNUM blocknum_freelist_head;                     // next (previously used) unused blocknum (free list)
    struct block_translation_pair *block_translation;

    // Where and how big is the block translation vector stored on disk.
    // size_on_disk is stored in block_translation[RESERVED_BLOCKNUM_TRANSLATION].size
    // location_on is stored in block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff
};

//Unmovable reserved first, then reallocable.
// We reserve one blocknum for the translation table itself.
enum {RESERVED_BLOCKNUM_NULL       =0,
      RESERVED_BLOCKNUM_TRANSLATION=1,
      RESERVED_BLOCKNUM_DESCRIPTOR =2,
      RESERVED_BLOCKNUMS};

static const BLOCKNUM freelist_null  = {-1}; // in a freelist, this indicates end of list
static const DISKOFF  size_is_free   = (DISKOFF)-1;  // value of block_translation_pair.size if blocknum is unused
static const DISKOFF  diskoff_unused = (DISKOFF)-2;  // value of block_translation_pair.u.diskoff if blocknum is used but does not yet have a diskblock

/********
 *  There are three copies of the translation table (btt) in the block table:
 *
 *    checkpointed   Is initialized by deserializing from disk,
 *                   and is the only version ever read from disk.
 *                   It is immutable.  Once read from disk it is never changed.
 *
 *    inprogress     Is only filled by copying from current,
 *                   and is the only version ever serialized to disk.
 *                   (It is serialized to disk on checkpoint and clean shutdown.)
 *                   It is immutable.  Once copied from current it is never changed.
 *
 *    current        Is initialized by copying from checkpointed,
 *                   is the only version ever modified while the database is in use, 
 *                   and is the only version ever copied to inprogress.
 *                   It is never stored on disk.
 ********/


struct block_table {
    struct translation current;      // The current translation is the one used by client threads.  It is not represented on disk.
    struct translation inprogress;   // the translation used by the checkpoint currently in progress.  If the checkpoint thread allocates a block, it must also update the current translation.
    struct translation checkpointed; // the translation for the data that shall remain inviolate on disk until the next checkpoint finishes, after which any blocks used only in this translation can be freed.

    // The in-memory data structure for block allocation.  There is no on-disk data structure for block allocation.
    // Note: This is *allocation* not *translation*.  The block_allocator is unaware of which blocks are used for which translation, but simply allocates and deallocates blocks.
    BLOCK_ALLOCATOR block_allocator;
    toku_pthread_mutex_t mutex;
    int is_locked;
    BOOL checkpoint_skipped;
    BOOL checkpoint_failed;
};

//forward decls
static int64_t calculate_size_on_disk (struct translation *t);
static inline BOOL translation_prevents_freeing (struct translation *t, BLOCKNUM b, struct block_translation_pair *old_pair);
static inline void lock_for_blocktable (BLOCK_TABLE bt);
static inline void unlock_for_blocktable (BLOCK_TABLE bt);



static void 
brtheader_set_dirty(struct brt_header *h, BOOL for_checkpoint){
    assert(h->blocktable->is_locked);
    assert(h->type == BRTHEADER_CURRENT);
    h->dirty = 1;
    if (for_checkpoint) {
	assert(h->checkpoint_header->type == BRTHEADER_CHECKPOINT_INPROGRESS);
	h->checkpoint_header->dirty = 1;
    }
}

static void
maybe_truncate_cachefile(BLOCK_TABLE bt, struct brt_header *h, u_int64_t size_needed_before) {
    assert(bt->is_locked);
    u_int64_t new_size_needed = block_allocator_allocated_limit(bt->block_allocator);
    //Save a call to toku_os_get_file_size (kernel call) if unlikely to be useful.
    if (new_size_needed < size_needed_before)
        toku_maybe_truncate_cachefile(h->cf, new_size_needed);
}

void
toku_maybe_truncate_cachefile_on_open(BLOCK_TABLE bt, struct brt_header *h) {
    lock_for_blocktable(bt);
    u_int64_t size_needed = block_allocator_allocated_limit(bt->block_allocator);
    toku_maybe_truncate_cachefile(h->cf, size_needed);
    unlock_for_blocktable(bt);
}



static void
copy_translation(struct translation * dst, struct translation * src, enum translation_type newtype) {
    assert(src->length_of_array >= src->smallest_never_used_blocknum.b); //verify invariant
    assert(newtype==TRANSLATION_DEBUG ||
           (src->type == TRANSLATION_CURRENT      && newtype == TRANSLATION_INPROGRESS) ||
           (src->type == TRANSLATION_CHECKPOINTED && newtype == TRANSLATION_CURRENT));
    dst->type = newtype;
    dst->smallest_never_used_blocknum = src->smallest_never_used_blocknum;
    dst->blocknum_freelist_head       = src->blocknum_freelist_head; 
    // destination btt is of fixed size.  Allocate+memcpy the exact length necessary.
    dst->length_of_array              = dst->smallest_never_used_blocknum.b;
    XMALLOC_N(dst->length_of_array, dst->block_translation);
    memcpy(dst->block_translation,
	   src->block_translation,
	   dst->length_of_array * sizeof(*dst->block_translation));
    //New version of btt is not yet stored on disk.
    dst->block_translation[RESERVED_BLOCKNUM_TRANSLATION].size      = 0;
    dst->block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff = diskoff_unused;
}

// block table must be locked by caller of this function
void
toku_block_translation_note_start_checkpoint_unlocked (BLOCK_TABLE bt) {
    assert(bt->is_locked);
    // Copy current translation to inprogress translation.
    assert(bt->inprogress.block_translation == NULL);
    copy_translation(&bt->inprogress, &bt->current, TRANSLATION_INPROGRESS);

    bt->checkpoint_skipped = FALSE;
    bt->checkpoint_failed  = FALSE;
}

//#define PRNTF(str, b, siz, ad, bt) printf("%s[%d] %s %"PRId64" %"PRId64" %"PRId64"\n", __FUNCTION__, __LINE__, str, b, siz, ad); fflush(stdout); if (bt) block_allocator_validate(((BLOCK_TABLE)(bt))->block_allocator);
//Debugging function
#define PRNTF(str, b, siz, ad, bt) 

void
toku_block_translation_note_failed_checkpoint (BLOCK_TABLE bt) {
    lock_for_blocktable(bt);
    assert(bt->inprogress.block_translation);
    bt->checkpoint_failed = TRUE;
    unlock_for_blocktable(bt);
}


void
toku_block_translation_note_skipped_checkpoint (BLOCK_TABLE bt) {
    //Purpose, alert block translation that the checkpoint was skipped, e.x. for a non-dirty header
    lock_for_blocktable(bt);
    assert(bt->inprogress.block_translation);
    bt->checkpoint_skipped = TRUE;
    unlock_for_blocktable(bt);
}

static void
cleanup_failed_checkpoint (BLOCK_TABLE bt) {
    int64_t i;
    struct translation *t = &bt->inprogress;

    for (i = 0; i < t->length_of_array; i++) {
        struct block_translation_pair *pair = &t->block_translation[i];
	if (pair->size > 0 &&
            !translation_prevents_freeing(&bt->current, make_blocknum(i), pair) &&
            !translation_prevents_freeing(&bt->checkpointed, make_blocknum(i), pair)) {
PRNTF("free", i, pair->size, pair->u.diskoff, bt);
            block_allocator_free_block(bt->block_allocator, pair->u.diskoff);
        }
    }
    toku_free(bt->inprogress.block_translation);
    memset(&bt->inprogress, 0, sizeof(bt->inprogress));
}

// Purpose: free disk space used by previous checkpoint, unless still in use by current.
//          capture inprogress as new checkpointed.
// For each entry in checkpointBTT
//   if offset does not match offset in inprogress
//      assert offset does not match offset in current
//      free (offset,len) from checkpoint
// move inprogress to checkpoint (resetting type)
// inprogress = NULL
void
toku_block_translation_note_end_checkpoint (BLOCK_TABLE bt, struct brt_header *h) {
    // Free unused blocks
    lock_for_blocktable(bt);
    u_int64_t allocated_limit_at_start = block_allocator_allocated_limit(bt->block_allocator);
    assert(bt->inprogress.block_translation);
    if (bt->checkpoint_skipped || bt->checkpoint_failed) {
        cleanup_failed_checkpoint(bt);
        goto end;
    }

    //Make certain inprogress was allocated space on disk
    assert(bt->inprogress.block_translation[RESERVED_BLOCKNUM_TRANSLATION].size > 0);
    assert(bt->inprogress.block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff > 0);

    int64_t i;
    struct translation *t = &bt->checkpointed;

    for (i = 0; i < t->length_of_array; i++) {
        struct block_translation_pair *pair = &t->block_translation[i];
	if (pair->size > 0 && !translation_prevents_freeing(&bt->inprogress, make_blocknum(i), pair)) {
            assert(!translation_prevents_freeing(&bt->current, make_blocknum(i), pair));
PRNTF("free", i, pair->size, pair->u.diskoff, bt);
            block_allocator_free_block(bt->block_allocator, pair->u.diskoff);
        }
    }
    toku_free(bt->checkpointed.block_translation);
    bt->checkpointed = bt->inprogress;
    bt->checkpointed.type = TRANSLATION_CHECKPOINTED;
    memset(&bt->inprogress, 0, sizeof(bt->inprogress));
    maybe_truncate_cachefile(bt, h, allocated_limit_at_start);
end:
    unlock_for_blocktable(bt);
}



static inline void
verify_valid_blocknum (struct translation *t, BLOCKNUM b) {
    assert(b.b >= 0);
    assert(b.b < t->smallest_never_used_blocknum.b);

    //Sanity check: Verify invariant
    assert(t->length_of_array >= t->smallest_never_used_blocknum.b);
}

//Can be freed
static inline void
verify_valid_freeable_blocknum (struct translation *t, BLOCKNUM b) {
    assert(t->type == TRANSLATION_CURRENT);
    assert(b.b >= RESERVED_BLOCKNUMS);
    assert(b.b < t->smallest_never_used_blocknum.b);

    //Sanity check: Verify invariant
    assert(t->length_of_array >= t->smallest_never_used_blocknum.b);
}

static void
blocktable_lock_init (BLOCK_TABLE bt) {
    memset(&bt->mutex, 0, sizeof(bt->mutex));
    int r = toku_pthread_mutex_init(&bt->mutex, NULL); assert(r == 0);
    bt->is_locked = 0;
}

static void
blocktable_lock_destroy (BLOCK_TABLE bt) {
    int r = toku_pthread_mutex_destroy(&bt->mutex); assert(r == 0);
}

static inline void
lock_for_blocktable (BLOCK_TABLE bt) {
    // Locks the blocktable_mutex. 
    int r = toku_pthread_mutex_lock(&bt->mutex);
    assert(r==0);
    bt->is_locked = 1;
}

static inline void
unlock_for_blocktable (BLOCK_TABLE bt) {
    bt->is_locked = 0;
    int r = toku_pthread_mutex_unlock(&bt->mutex);
    assert(r==0);
}

void
toku_brtheader_lock (struct brt_header *h) {
    BLOCK_TABLE bt = h->blocktable;
    lock_for_blocktable(bt);
}

void
toku_brtheader_unlock (struct brt_header *h) {
    BLOCK_TABLE bt = h->blocktable;
    assert(bt->is_locked);
    unlock_for_blocktable(bt);
}

// This is a special debugging function used only in the brt-serialize-test.
void
toku_block_alloc(BLOCK_TABLE bt, u_int64_t size, u_int64_t *offset) {
    lock_for_blocktable(bt);
PRNTF("allocSomethingUnknown", 0L, (int64_t)size, 0L, bt);
    block_allocator_alloc_block(bt->block_allocator, size, offset);
PRNTF("allocSomethingUnknownd", 0L, (int64_t)size, (int64_t)*offset, bt);
    unlock_for_blocktable(bt);
}

// Also used only in brt-serialize-test.
void
toku_block_free(BLOCK_TABLE bt, u_int64_t offset) {
    lock_for_blocktable(bt);
PRNTF("freeSOMETHINGunknown", 0L, 0L, offset, bt);
    block_allocator_free_block(bt->block_allocator, offset);
    unlock_for_blocktable(bt);
}

static int64_t
calculate_size_on_disk (struct translation *t) {
    int64_t r = (8 + // smallest_never_used_blocknum
                 8 + // blocknum_freelist_head
	         t->smallest_never_used_blocknum.b * 16 + // Array
                 4); // 4 for checksum
    return r;
}

static void
translation_update_size_on_disk (struct translation *t) {
    t->block_translation[RESERVED_BLOCKNUM_TRANSLATION].size = calculate_size_on_disk(t);
}

// We cannot free the disk space allocated to this blocknum if it is still in use by the given translation table.
static inline BOOL
translation_prevents_freeing(struct translation *t, BLOCKNUM b, struct block_translation_pair *old_pair) {
    BOOL r = (BOOL)
        (t->block_translation &&
         b.b < t->smallest_never_used_blocknum.b &&
         old_pair->u.diskoff == t->block_translation[b.b].u.diskoff);
    return r;
}

static void
blocknum_realloc_on_disk_internal (BLOCK_TABLE bt, BLOCKNUM b, DISKOFF size, DISKOFF *offset, struct brt_header * h, BOOL for_checkpoint) {
    assert(bt->is_locked);
    brtheader_set_dirty(h, for_checkpoint);

    struct translation *t = &bt->current;
    struct block_translation_pair old_pair = t->block_translation[b.b];
PRNTF("old", b.b, old_pair.size, old_pair.u.diskoff, bt);
    //Free the old block if it is not still in use by the checkpoint in progress or the previous checkpoint
    BOOL cannot_free = (BOOL)
        ((!for_checkpoint && translation_prevents_freeing(&bt->inprogress,   b, &old_pair)) ||
         translation_prevents_freeing(&bt->checkpointed, b, &old_pair));
    if (!cannot_free && old_pair.u.diskoff!=diskoff_unused) {
PRNTF("Freed", b.b, old_pair.size, old_pair.u.diskoff, bt);
        block_allocator_free_block(bt->block_allocator, old_pair.u.diskoff);
    }

    u_int64_t allocator_offset;
    //Allocate a new block
    block_allocator_alloc_block(bt->block_allocator, size, &allocator_offset);
    t->block_translation[b.b].u.diskoff = allocator_offset;
    t->block_translation[b.b].size    = size;
    *offset = allocator_offset;

PRNTF("New", b.b, t->block_translation[b.b].size, t->block_translation[b.b].u.diskoff, bt);
    //Update inprogress btt if appropriate (if called because Pending bit is set).
    if (for_checkpoint) {
	assert(b.b < bt->inprogress.length_of_array);
	bt->inprogress.block_translation[b.b] = t->block_translation[b.b];
    }
}

void
toku_blocknum_realloc_on_disk (BLOCK_TABLE bt, BLOCKNUM b, DISKOFF size, DISKOFF *offset, struct brt_header * h, BOOL for_checkpoint) {
    lock_for_blocktable(bt);
    struct translation *t = &bt->current;
    verify_valid_freeable_blocknum(t, b);
    blocknum_realloc_on_disk_internal(bt, b, size, offset, h, for_checkpoint);
    unlock_for_blocktable(bt);
}

// Purpose of this function is to figure out where to put the inprogress btt on disk, allocate space for it there.
static void
blocknum_alloc_translation_on_disk_unlocked (BLOCK_TABLE bt) {
    assert(bt->is_locked);

    struct translation *t = &bt->inprogress;
    assert(t->block_translation);
    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_TRANSLATION);
    struct block_translation_pair old_pair = t->block_translation[b.b];
    //Each inprogress is allocated only once
    assert(old_pair.size == 0 && old_pair.u.diskoff == diskoff_unused);

    //Allocate a new block
    int64_t size = calculate_size_on_disk(t);
    u_int64_t offset;
    block_allocator_alloc_block(bt->block_allocator, size, &offset);
PRNTF("blokAllokator", 1L, size, offset, bt);
    t->block_translation[b.b].u.diskoff = offset;
    t->block_translation[b.b].size      = size;
}

//Fills wbuf with bt
//A clean shutdown runs checkpoint start so that current and inprogress are copies.
void
toku_serialize_translation_to_wbuf_unlocked(BLOCK_TABLE bt, struct wbuf *w,
                                            int64_t *address, int64_t *size) {
    assert(bt->is_locked);
    struct translation *t = &bt->inprogress;

    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_TRANSLATION);
    blocknum_alloc_translation_on_disk_unlocked(bt);
    {
        //Init wbuf
        u_int64_t size_translation = calculate_size_on_disk(t);
        assert((int64_t)size_translation==t->block_translation[b.b].size);
        if (0)
            printf("%s:%d writing translation table of size_translation %"PRIu64" at %"PRId64"\n", __FILE__, __LINE__, size_translation, t->block_translation[b.b].u.diskoff);
        wbuf_init(w, toku_malloc(size_translation), size_translation);
        assert(w->size==size_translation);
    }
    wbuf_BLOCKNUM(w, t->smallest_never_used_blocknum); 
    wbuf_BLOCKNUM(w, t->blocknum_freelist_head); 
    int64_t i;
    for (i=0; i<t->smallest_never_used_blocknum.b; i++) {
        if (0)
            printf("%s:%d %"PRId64",%"PRId64"\n", __FILE__, __LINE__, t->block_translation[i].u.diskoff, t->block_translation[i].size);
        wbuf_DISKOFF(w, t->block_translation[i].u.diskoff);
        wbuf_DISKOFF(w, t->block_translation[i].size);
    }
    u_int32_t checksum = x1764_finish(&w->checksum);
    wbuf_int(w, checksum);
    *address = t->block_translation[b.b].u.diskoff;
    *size    = t->block_translation[b.b].size;
}


// Perhaps rename: purpose is get disk address of a block, given its blocknum (blockid?)
static void
translate_blocknum_to_offset_size_unlocked(BLOCK_TABLE bt, BLOCKNUM b, DISKOFF *offset, DISKOFF *size) {
    struct translation *t = &bt->current;
    verify_valid_blocknum(t, b);
    if (offset) *offset = t->block_translation[b.b].u.diskoff;
    if (size)   *size = t->block_translation[b.b].size;
}

// Perhaps rename: purpose is get disk address of a block, given its blocknum (blockid?)
void
toku_translate_blocknum_to_offset_size(BLOCK_TABLE bt, BLOCKNUM b, DISKOFF *offset, DISKOFF *size) {
    lock_for_blocktable(bt);
    translate_blocknum_to_offset_size_unlocked(bt, b, offset, size);
    unlock_for_blocktable(bt);
}

//Only called by toku_allocate_blocknum
static void
maybe_expand_translation (struct translation *t) {
// Effect: expand the array to maintain size invariant
// given that one more never-used blocknum will soon be used.
    if (t->length_of_array <= t->smallest_never_used_blocknum.b) {
        //expansion is necessary
        u_int64_t new_length = t->smallest_never_used_blocknum.b * 2;
        XREALLOC_N(new_length, t->block_translation);
        u_int64_t i;
        for (i = t->length_of_array; i < new_length; i++) {
            t->block_translation[i].u.next_free_blocknum = freelist_null;
            t->block_translation[i].size                 = size_is_free;
        }
        t->length_of_array = new_length;
    }
}

void
toku_allocate_blocknum_unlocked(BLOCK_TABLE bt, BLOCKNUM *res, struct brt_header * h) {
    assert(bt->is_locked);
    BLOCKNUM result;
    struct translation * t = &bt->current;
    if (t->blocknum_freelist_head.b == freelist_null.b) {
        // no previously used blocknums are available
        // use a never used blocknum
        maybe_expand_translation(t); //Ensure a never used blocknums is available
        result = t->smallest_never_used_blocknum;
        t->smallest_never_used_blocknum.b++;
        translation_update_size_on_disk(t);
    } else {  // reuse a previously used blocknum
        result = t->blocknum_freelist_head;
        BLOCKNUM next = t->block_translation[result.b].u.next_free_blocknum;
        t->blocknum_freelist_head = next;
    }
    //Verify the blocknum is free
    assert(t->block_translation[result.b].size == size_is_free);
    //blocknum is not free anymore
    t->block_translation[result.b].u.diskoff = diskoff_unused;
    t->block_translation[result.b].size    = 0;
    verify_valid_freeable_blocknum(t, result);
    *res = result;
    brtheader_set_dirty(h, FALSE);
}

void
toku_allocate_blocknum(BLOCK_TABLE bt, BLOCKNUM *res, struct brt_header * h) {
    lock_for_blocktable(bt);
    toku_allocate_blocknum_unlocked(bt, res, h);
    unlock_for_blocktable(bt);
}

static void
free_blocknum_unlocked(BLOCK_TABLE bt, BLOCKNUM *bp, struct brt_header * h) {
// Effect: Free a blocknum.
// If the blocknum holds the only reference to a block on disk, free that block
    assert(bt->is_locked);
    BLOCKNUM b = *bp;
    bp->b = 0; //Remove caller's reference.
    struct translation *t = &bt->current;
    verify_valid_freeable_blocknum(t, b);
    struct block_translation_pair old_pair = t->block_translation[b.b];
    assert(old_pair.size != size_is_free);

PRNTF("free_blocknum", b.b, t->block_translation[b.b].size, t->block_translation[b.b].u.diskoff, bt);
    t->block_translation[b.b].size                 = size_is_free;
    t->block_translation[b.b].u.next_free_blocknum = t->blocknum_freelist_head;
    t->blocknum_freelist_head                      = b;

    //If the size is 0, no disk block has ever been assigned to this blocknum.
    if (old_pair.size > 0) {
        //Free the old block if it is not still in use by the checkpoint in progress or the previous checkpoint
        BOOL cannot_free = (BOOL)
            (translation_prevents_freeing(&bt->inprogress,   b, &old_pair) ||
             translation_prevents_freeing(&bt->checkpointed, b, &old_pair));
        if (!cannot_free) {
PRNTF("free_blocknum_free", b.b, old_pair.size, old_pair.u.diskoff, bt);
            block_allocator_free_block(bt->block_allocator, old_pair.u.diskoff);
        }
    }
    else assert(old_pair.size==0 && old_pair.u.diskoff == diskoff_unused);
    brtheader_set_dirty(h, FALSE);
}

void
toku_free_blocknum(BLOCK_TABLE bt, BLOCKNUM *bp, struct brt_header * h) {
    lock_for_blocktable(bt);
    free_blocknum_unlocked(bt, bp, h);
    unlock_for_blocktable(bt);
}
    
void
toku_block_translation_truncate_unlocked(BLOCK_TABLE bt, struct brt_header *h) {
    assert(bt->is_locked);
    u_int64_t allocated_limit_at_start = block_allocator_allocated_limit(bt->block_allocator);
    brtheader_set_dirty(h, FALSE);
    //Free all regular/data blocks (non reserved)
    //Meta data is stored in reserved blocks
    struct translation *t = &bt->current;
    int64_t i;
    for (i=RESERVED_BLOCKNUMS; i<t->smallest_never_used_blocknum.b; i++) {
        BLOCKNUM b = make_blocknum(i);
        if (t->block_translation[i].size >= 0) free_blocknum_unlocked(bt, &b, h);
    }
    maybe_truncate_cachefile(bt, h, allocated_limit_at_start);
}

//Verify there are no free blocks.
void
toku_block_verify_no_free_blocknums(BLOCK_TABLE bt) {
    assert(bt->current.blocknum_freelist_head.b == freelist_null.b);
}

//Verify a blocknum is currently allocated.
void
toku_verify_blocknum_allocated(BLOCK_TABLE bt, BLOCKNUM b) {
    lock_for_blocktable(bt);
    struct translation *t = &bt->current;
    verify_valid_blocknum(t, b);
    assert(t->block_translation[b.b].size != size_is_free);
    unlock_for_blocktable(bt);
}

//Only used by toku_dump_translation table (debug info)
static void
dump_translation(FILE *f, struct translation *t) {
    if (t->block_translation) {
        BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_TRANSLATION);
        fprintf(f, " length_of_array[%"PRId64"]", t->length_of_array);
        fprintf(f, " smallest_never_used_blocknum[%"PRId64"]", t->smallest_never_used_blocknum.b);
        fprintf(f, " blocknum_free_list_head[%"PRId64"]", t->blocknum_freelist_head.b);
        fprintf(f, " size_on_disk[%"PRId64"]", t->block_translation[b.b].size);
        fprintf(f, " location_on_disk[%"PRId64"]", t->block_translation[b.b].u.diskoff);
        int64_t i;
        for (i=0; i<t->length_of_array; i++) {
            fprintf(f, " %"PRId64": %"PRId64" %"PRId64"", i, t->block_translation[i].u.diskoff, t->block_translation[i].size);
        }
        fprintf(f, "\n");
    }
    else fprintf(f, " does not exist\n");
}

//Only used by toku_brt_dump which is only for debugging purposes
void
toku_dump_translation_table(FILE *f, BLOCK_TABLE bt) {
    lock_for_blocktable(bt);
    fprintf(f, "Current block translation:");
    dump_translation(f, &bt->current);
    fprintf(f, "Checkpoint in progress block translation:");
    dump_translation(f, &bt->inprogress);
    fprintf(f, "Checkpointed block translation:");
    dump_translation(f, &bt->checkpointed);
    unlock_for_blocktable(bt);
}

//Only used by brtdump
void
toku_blocknum_dump_translation(BLOCK_TABLE bt, BLOCKNUM b) {
    lock_for_blocktable(bt);

    struct translation *t = &bt->current;
    if (b.b < t->length_of_array) {
        struct block_translation_pair *bx = &t->block_translation[b.b];
        printf("%" PRId64 ": %" PRId64 " %" PRId64 "\n", b.b, bx->u.diskoff, bx->size);
    }
    unlock_for_blocktable(bt);
}


//Must not call this function when anything else is using the blocktable.
//No one may use the blocktable afterwards.
void
toku_blocktable_destroy(BLOCK_TABLE *btp) {
    BLOCK_TABLE bt = *btp;
    *btp = NULL;
    if (bt->current.block_translation)      toku_free(bt->current.block_translation);
    if (bt->inprogress.block_translation)   toku_free(bt->inprogress.block_translation);
    if (bt->checkpointed.block_translation) toku_free(bt->checkpointed.block_translation);

    destroy_block_allocator(&bt->block_allocator);
    blocktable_lock_destroy(bt);
    toku_free(bt);
}


static BLOCK_TABLE
blocktable_create_internal (void) {
// Effect: Fill it in, including the translation table, which is uninitialized
    BLOCK_TABLE XMALLOC(bt);
    memset(bt, 0, sizeof(*bt));
    blocktable_lock_init(bt);

    //There are two headers, so we reserve space for two.
    u_int64_t reserve_per_header = BLOCK_ALLOCATOR_HEADER_RESERVE;

    //Must reserve in multiples of BLOCK_ALLOCATOR_ALIGNMENT
    //Round up the per-header usage if necessary.
    //We want each header aligned.
    u_int64_t remainder = BLOCK_ALLOCATOR_HEADER_RESERVE % BLOCK_ALLOCATOR_ALIGNMENT;
    if (remainder!=0) {
        reserve_per_header += BLOCK_ALLOCATOR_ALIGNMENT;
        reserve_per_header -= remainder;
    }
    assert(2*reserve_per_header == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    create_block_allocator(&bt->block_allocator,
                           BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE,
                           BLOCK_ALLOCATOR_ALIGNMENT);
    return bt;
}



static void
translation_default(struct translation *t) {  // destination into which to create a default translation
    t->type = TRANSLATION_CHECKPOINTED;
    t->smallest_never_used_blocknum = make_blocknum(RESERVED_BLOCKNUMS);
    t->length_of_array              = t->smallest_never_used_blocknum.b;
    t->blocknum_freelist_head       = freelist_null;
    XMALLOC_N(t->length_of_array, t->block_translation);
    int64_t i;
    for (i = 0; i < t->length_of_array; i++) {
        t->block_translation[i].size      = 0;
        t->block_translation[i].u.diskoff = diskoff_unused;
    }
}


static void
translation_deserialize_from_buffer(struct translation *t,    // destination into which to deserialize
                                    DISKOFF location_on_disk, //Location of translation_buffer
                                    u_int64_t size_on_disk,
                                    unsigned char * translation_buffer) {   // buffer with serialized translation
    assert(location_on_disk!=0);
    t->type = TRANSLATION_CHECKPOINTED;
    {
        // check the checksum
        u_int32_t x1764 = x1764_memory(translation_buffer, size_on_disk - 4);
        u_int64_t offset = size_on_disk - 4;
        //printf("%s:%d read from %ld (x1764 offset=%ld) size=%ld\n", __FILE__, __LINE__, block_translation_address_on_disk, offset, block_translation_size_on_disk);
        u_int32_t stored_x1764 = toku_dtoh32(*(int*)(translation_buffer + offset));
        assert(x1764 == stored_x1764);
    }
    struct rbuf rt;
    rt.buf = translation_buffer;
    rt.ndone = 0;
    rt.size = size_on_disk-4;//4==checksum

    t->smallest_never_used_blocknum = rbuf_blocknum(&rt); 
    t->length_of_array = t->smallest_never_used_blocknum.b;
    assert(t->smallest_never_used_blocknum.b >= RESERVED_BLOCKNUMS);
    t->blocknum_freelist_head       = rbuf_blocknum(&rt); 
    XMALLOC_N(t->length_of_array, t->block_translation);
    int64_t i;
    for (i=0; i < t->length_of_array; i++) {
        t->block_translation[i].u.diskoff = rbuf_diskoff(&rt);
        t->block_translation[i].size    = rbuf_diskoff(&rt);
PRNTF("ReadIn", i, t->block_translation[i].size, t->block_translation[i].u.diskoff, NULL);
    }
    assert(calculate_size_on_disk(t)                                     == (int64_t)size_on_disk);
    assert(t->block_translation[RESERVED_BLOCKNUM_TRANSLATION].size      == (int64_t)size_on_disk);
    assert(t->block_translation[RESERVED_BLOCKNUM_TRANSLATION].u.diskoff == location_on_disk);
}

// We just initialized a translation, inform block allocator to reserve space for each blocknum in use.
static void
blocktable_note_translation (BLOCK_ALLOCATOR allocator, struct translation *t) {
    //This is where the space for them will be reserved (in addition to normal blocks).
    //See RESERVED_BLOCKNUMS
    int64_t i;
    for (i=0; i<t->smallest_never_used_blocknum.b; i++) {
        struct block_translation_pair pair = t->block_translation[i];
        if (pair.size > 0) {
            assert(pair.u.diskoff != diskoff_unused);
            block_allocator_alloc_block_at(allocator, pair.size, pair.u.diskoff);
        }
    }
}


// Fill in the checkpointed translation from buffer, and copy checkpointed to current.
// The one read from disk is the last known checkpointed one, so we are keeping it in 
// place and then setting current (which is never stored on disk) for current use.
// The translation_buffer has translation only, we create the rest of the block_table.
void
toku_blocktable_create_from_buffer(BLOCK_TABLE *btp,
                                   DISKOFF location_on_disk, //Location of translation_buffer
                                   DISKOFF size_on_disk,
                                   unsigned char *translation_buffer) {
    BLOCK_TABLE bt = blocktable_create_internal();
    translation_deserialize_from_buffer(&bt->checkpointed, location_on_disk, size_on_disk, translation_buffer);
    blocktable_note_translation(bt->block_allocator, &bt->checkpointed);
    // we just filled in checkpointed, now copy it to current.  
    copy_translation(&bt->current, &bt->checkpointed, TRANSLATION_CURRENT);
    *btp = bt;
}


void
toku_blocktable_create_new(BLOCK_TABLE *btp) {
    BLOCK_TABLE bt = blocktable_create_internal();
    translation_default(&bt->checkpointed);  // create default btt (empty except for reserved blocknums)
    blocktable_note_translation(bt->block_allocator, &bt->checkpointed);
    // we just created a default checkpointed, now copy it to current.  
    copy_translation(&bt->current, &bt->checkpointed, TRANSLATION_CURRENT);

    *btp = bt;
}    

int
toku_blocktable_iterate (BLOCK_TABLE bt, enum translation_type type, BLOCKTABLE_CALLBACK f, void *extra, BOOL data_only, BOOL used_only) {
    struct translation *src;
    
    int r = 0;
    switch (type) {
        case TRANSLATION_CURRENT:      src = &bt->current; break;
        case TRANSLATION_INPROGRESS:   src = &bt->inprogress; break;
        case TRANSLATION_CHECKPOINTED: src = &bt->checkpointed; break;
        default: r = EINVAL; break;
    }
    struct translation fakecurrent;
    struct translation *t = &fakecurrent;
    if (r==0) {
        lock_for_blocktable(bt);
        copy_translation(t, src, TRANSLATION_DEBUG);
        t->block_translation[RESERVED_BLOCKNUM_TRANSLATION] =
           src->block_translation[RESERVED_BLOCKNUM_TRANSLATION];
        unlock_for_blocktable(bt);
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

static int
frag_helper(BLOCKNUM UU(b), int64_t size, int64_t address, void *extra) {
    frag_extra *info = extra;

    if (size + address > info->total_space)
        info->total_space = size + address;
    info->used_space += size;
    return 0;
}

void
toku_blocktable_internal_fragmentation (BLOCK_TABLE bt, int64_t *total_sizep, int64_t *used_sizep) {
    frag_extra info = {0,0};
    int r = toku_blocktable_iterate(bt, TRANSLATION_CHECKPOINTED, frag_helper, &info, FALSE, TRUE);
    assert(r==0);

    if (total_sizep) *total_sizep = info.total_space;
    if (used_sizep)  *used_sizep  = info.used_space;
}

void
toku_realloc_descriptor_on_disk(BLOCK_TABLE bt, DISKOFF size, DISKOFF *offset, struct brt_header * h) {
    lock_for_blocktable(bt);
    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_DESCRIPTOR);
    blocknum_realloc_on_disk_internal(bt, b, size, offset, h, FALSE);
    unlock_for_blocktable(bt);
}

void
toku_get_descriptor_offset_size(BLOCK_TABLE bt, DISKOFF *offset, DISKOFF *size) {
    lock_for_blocktable(bt);
    BLOCKNUM b = make_blocknum(RESERVED_BLOCKNUM_DESCRIPTOR);
    translate_blocknum_to_offset_size_unlocked(bt, b, offset, size);
    unlock_for_blocktable(bt);
}



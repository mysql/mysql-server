//TODO: What about h->block_translation_size_on_disk
//TODO: What about h->block_translation_address_on_disk
//TODO: What about h->block_allocator

#include "toku_portability.h"
#include "brttypes.h"
#include "block_table.h"
#include "memory.h"
#include "toku_assert.h"
#include "toku_pthread.h"
#include "block_allocator.h"
#include "rbuf.h"
#include "wbuf.h"

struct block_table {
    // This is the map from block numbers to offsets
    //int n_blocks, n_blocks_array_size;
    //struct block_descriptor *blocks;
    BLOCKNUM free_blocks; // free list for blocks.  Use -1 to indicate that there are no free blocks
    BLOCKNUM unused_blocks; // first unused block

    u_int64_t translated_blocknum_limit;
    struct block_translation_pair *block_translation;

    // Where and how big is the block translation vector stored on disk.
    // The size of the on_disk buffer may no longer match the max_blocknum_translated field, since blocks may have been allocated or freed.
    // We need to remember this old information so we can free it properly.
    u_int64_t block_translation_size_on_disk;    // the size of the block containing the translation (i.e. 8 times the number of entries)
    u_int64_t block_translation_address_on_disk; // 0 if there is no memory allocated
    
    // The in-memory data structure  for block allocation
    BLOCK_ALLOCATOR block_allocator;
};

static const DISKOFF diskoff_is_null = (DISKOFF)-1; // in a freelist, this indicates end of list
static const DISKOFF size_is_free = (DISKOFF)-1;

static void
extend_block_translation(BLOCK_TABLE bt, BLOCKNUM blocknum)
// Effect: Record a block translation.  This means extending the translation table, and setting the diskoff and size to zero in any of the unused spots.
{
    assert(0<=blocknum.b);
    if (bt->translated_blocknum_limit <= (u_int64_t)blocknum.b) {
        if (bt->block_translation == 0) assert(bt->translated_blocknum_limit==0);
        u_int64_t new_limit = blocknum.b + 1;
        u_int64_t old_limit = bt->translated_blocknum_limit;
        u_int64_t j;
        XREALLOC_N(new_limit, bt->block_translation);
        for (j=old_limit; j<new_limit; j++) {
            bt->block_translation[j].diskoff = 0;
            bt->block_translation[j].size    = 0;
        }
        bt->translated_blocknum_limit = new_limit;
    }
}

static inline void
verify(BLOCK_TABLE bt, BLOCKNUM b) {
    // 0<=b<limit (limit is exclusive)
    assert(0 <= b.b);
    assert((u_int64_t)b.b < bt->translated_blocknum_limit);
}

static toku_pthread_mutex_t blocktable_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;
static int blocktable_is_locked=0;

void toku_blocktable_lock_init(void) {
    int r = toku_pthread_mutex_init(&blocktable_mutex, NULL); assert(r == 0);
}

void toku_blocktable_lock_destroy(void) {
    int r = toku_pthread_mutex_destroy(&blocktable_mutex); assert(r == 0);
}

static inline void
lock_for_blocktable (void) {
    // Locks the blocktable_mutex. 
    int r = toku_pthread_mutex_lock(&blocktable_mutex);
    assert(r==0);
    blocktable_is_locked = 1;
}

static inline void
unlock_for_blocktable (void) {
    blocktable_is_locked = 0;
    int r = toku_pthread_mutex_unlock(&blocktable_mutex);
    assert(r==0);
}

static void
block_free(BLOCK_TABLE bt, u_int64_t offset) {
    block_allocator_free_block(bt->block_allocator, offset);
}

static void
block_free_blocknum(BLOCK_TABLE bt, BLOCKNUM b) {
    verify(bt, b);
    if (bt->block_translation[b.b].size > 0) {
        block_free(bt, bt->block_translation[b.b].diskoff);
        bt->block_translation[b.b].diskoff = 0;
        bt->block_translation[b.b].size    = 0;
    }
}

static void
block_alloc(BLOCK_TABLE bt, u_int64_t size, u_int64_t *offset) {
    block_allocator_alloc_block(bt->block_allocator, size, offset);
}

static void
block_alloc_and_set_translation(BLOCK_TABLE bt, BLOCKNUM b, u_int64_t size, u_int64_t *offset) {
    verify(bt, b);
    block_alloc(bt, size, offset);
    bt->block_translation[b.b].diskoff = *offset;
    bt->block_translation[b.b].size = size;
}

void
toku_block_alloc(BLOCK_TABLE bt, u_int64_t size, u_int64_t *offset) {
    lock_for_blocktable();
    block_alloc(bt, size, offset);
    unlock_for_blocktable();
}

void
toku_block_free(BLOCK_TABLE bt, u_int64_t offset) {
    lock_for_blocktable();
    block_free(bt, offset);
    unlock_for_blocktable();
}

static void
update_size_on_disk(BLOCK_TABLE bt) {
    bt->block_translation_size_on_disk = 4 +//4 for checksum
                bt->translated_blocknum_limit*sizeof(bt->block_translation[0]);
}

void
toku_block_realloc(BLOCK_TABLE bt, BLOCKNUM b, u_int64_t size, u_int64_t *offset) {
    lock_for_blocktable();
    extend_block_translation(bt, b);
    block_free_blocknum(bt, b);
    block_alloc_and_set_translation(bt, b, size, offset);
    unlock_for_blocktable();
}

void
toku_block_lock_for_multiple_operations(void) {
    lock_for_blocktable();
}

void
toku_block_unlock_for_multiple_operations(void) {
    assert(blocktable_is_locked);
    unlock_for_blocktable();
}


void
toku_block_realloc_translation_unlocked(BLOCK_TABLE bt) {
    assert(blocktable_is_locked);
    if (bt->block_translation_address_on_disk != 0) {
        block_allocator_free_block(bt->block_allocator, bt->block_translation_address_on_disk);
    }
    update_size_on_disk(bt);
    block_allocator_alloc_block(bt->block_allocator,
                         bt->block_translation_size_on_disk,
                         &bt->block_translation_address_on_disk);
}

void
toku_block_wbuf_free_blocks_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf) {
    assert(blocktable_is_locked);
    wbuf_BLOCKNUM(wbuf, bt->free_blocks);
}

void
toku_block_wbuf_unused_blocks_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf) {
    assert(blocktable_is_locked);
    wbuf_BLOCKNUM(wbuf, bt->unused_blocks);
}

void
toku_block_wbuf_translated_blocknum_limit_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf) {
    assert(blocktable_is_locked);
    wbuf_ulonglong(wbuf, bt->translated_blocknum_limit);
}

void
toku_block_wbuf_block_translation_address_on_disk_unlocked(BLOCK_TABLE bt, struct wbuf *wbuf) {
    assert(blocktable_is_locked);
    wbuf_DISKOFF(wbuf, bt->block_translation_address_on_disk);
}

void
toku_block_wbuf_init_and_fill_unlocked(BLOCK_TABLE bt, struct wbuf *w,
        u_int64_t *size, u_int64_t *address) {
    assert(blocktable_is_locked);
    update_size_on_disk(bt);
    u_int64_t size_translation = bt->block_translation_size_on_disk;
    //printf("%s:%d writing translation table of size_translation %ld at %ld\n", __FILE__, __LINE__, size_translation, bt->block_translation_address_on_disk);
    wbuf_init(w, toku_malloc(size_translation), size_translation);
    assert(w->size==size_translation);
    u_int64_t i;
    for (i=0; i<bt->translated_blocknum_limit; i++) {
        //printf("%s:%d %ld,%ld\n", __FILE__, __LINE__, bt->block_translation[i].diskoff, bt->block_translation[i].size_translation);
        wbuf_ulonglong(w, bt->block_translation[i].diskoff);
        wbuf_ulonglong(w, bt->block_translation[i].size);
    }
    u_int32_t checksum = x1764_finish(&w->checksum);
    wbuf_int(w, checksum);
    *size = size_translation;
    *address = bt->block_translation_address_on_disk;
}

DISKOFF
toku_block_get_offset(BLOCK_TABLE bt, BLOCKNUM b) {
    lock_for_blocktable();
    verify(bt, b);
    DISKOFF r = bt->block_translation[b.b].diskoff;
    unlock_for_blocktable();
    return r;
}

DISKOFF
toku_block_get_size(BLOCK_TABLE bt, BLOCKNUM b) {
    lock_for_blocktable();
    verify(bt, b);
    DISKOFF r = bt->block_translation[b.b].size;
    unlock_for_blocktable();
    return r;
}

void
toku_block_get_offset_size(BLOCK_TABLE bt, BLOCKNUM b, DISKOFF *offset, DISKOFF *size) {
    lock_for_blocktable();
    verify(bt, b);
    *offset = bt->block_translation[b.b].diskoff;
    *size = bt->block_translation[b.b].size;
    unlock_for_blocktable();
}

int
toku_allocate_diskblocknumber(BLOCK_TABLE bt, BLOCKNUM *res, int *dirty) {
    lock_for_blocktable();
    BLOCKNUM result;
    if (bt->free_blocks.b == diskoff_is_null) {
        // no blocks in the free list
        result = bt->unused_blocks;
        bt->unused_blocks.b++;
    } else {
        result = bt->free_blocks;
        assert(bt->block_translation[result.b].size = size_is_free);
        bt->block_translation[result.b].size = 0;
        bt->free_blocks.b = bt->block_translation[result.b].diskoff; // pop the freelist
    }
    assert(result.b>0);
    *res = result;
    *dirty = 1;
    unlock_for_blocktable();
    return 0;
}
////CONVERTED above already
//TODO: Convert below


int
toku_free_diskblocknumber(BLOCK_TABLE bt, BLOCKNUM *b, int *dirty)
// Effect: Free a diskblock
//  Watch out for the case where the disk block was never yet written to disk
{
    lock_for_blocktable();
    extend_block_translation(bt, *b);
    // If the block_translation indicates that the size is <=0
    // then there is no disk block allocated.
    if (bt->block_translation[b->b].size > 0) {
        block_allocator_free_block(bt->block_allocator,
                                   bt->block_translation[b->b].diskoff);
    }
    verify(bt, *b);
    assert(bt->block_translation[b->b].size != size_is_free);
    bt->block_translation[b->b].size = size_is_free;
    bt->block_translation[b->b].diskoff = bt->free_blocks.b;
    bt->free_blocks.b = b->b;
    b->b = 0;
    *dirty = 1;
    unlock_for_blocktable();
    return 0;
}

//Verify there are no free blocks.
void
toku_block_verify_no_free_blocks(BLOCK_TABLE bt) {
    assert(bt->free_blocks.b==-1);
}

//Verify a block has been allocated at least once.
void
toku_verify_diskblocknumber_allocated(BLOCK_TABLE bt, BLOCKNUM b) {
    lock_for_blocktable();
    assert(0 <= b.b);
    assert(     b.b < bt->unused_blocks.b);
    unlock_for_blocktable();
}

u_int64_t toku_block_allocator_allocated_limit(BLOCK_TABLE bt) {
    lock_for_blocktable();
    u_int64_t r = block_allocator_allocated_limit(bt->block_allocator);
    unlock_for_blocktable();
    return r;
}

void
toku_block_dump_translation_table(FILE *f, BLOCK_TABLE bt) {
    lock_for_blocktable();
    u_int64_t i;
    fprintf(f, "Block translation:");
    for (i=0; i<bt->translated_blocknum_limit; i++) {
        fprintf(f, " %"PRIu64": %"PRId64" %"PRId64"", i, bt->block_translation[i].diskoff, bt->block_translation[i].size);
    }
    fprintf(f, "\n");
    unlock_for_blocktable();
}

void
toku_block_dump_translation(BLOCK_TABLE bt, u_int64_t offset) {
    lock_for_blocktable();
    if (offset < bt->translated_blocknum_limit) {
        struct block_translation_pair *bx = &bt->block_translation[offset];
        printf("%" PRIu64 ": %" PRId64 " %" PRId64 "\n", offset, bx->diskoff, bx->size);
    }
    unlock_for_blocktable();
}

void
toku_block_recovery_set_unused_blocks(BLOCK_TABLE bt, BLOCKNUM newunused) {
    lock_for_blocktable();
    bt->unused_blocks = newunused;
    unlock_for_blocktable();
}

void
toku_block_recovery_set_free_blocks(BLOCK_TABLE bt, BLOCKNUM newfree) {
    lock_for_blocktable();
    bt->free_blocks = newfree;
    unlock_for_blocktable();
}

void
toku_block_memcpy_translation_table(BLOCK_TABLE bt, size_t n, void *p) {
    lock_for_blocktable();
    memcpy(p, bt->block_translation, n);
    unlock_for_blocktable();
}

u_int64_t
toku_block_get_translated_blocknum_limit(BLOCK_TABLE bt) {
    lock_for_blocktable();
    u_int64_t r = bt->translated_blocknum_limit;
    unlock_for_blocktable();
    return r;
}

BLOCKNUM
toku_block_get_free_blocks(BLOCK_TABLE bt) {
    lock_for_blocktable();
    BLOCKNUM r = bt->free_blocks;
    unlock_for_blocktable();
    return r;
}

BLOCKNUM
toku_block_get_unused_blocks(BLOCK_TABLE bt) {
    lock_for_blocktable();
    BLOCKNUM r = bt->unused_blocks;
    unlock_for_blocktable();
    return r;
}

void
toku_blocktable_destroy(BLOCK_TABLE *btp) {
    lock_for_blocktable();
    BLOCK_TABLE bt = *btp;
    *btp = NULL;
    toku_free(bt->block_translation);
    bt->block_translation = NULL;
    destroy_block_allocator(&bt->block_allocator);
    toku_free(bt);
    unlock_for_blocktable();
}

void
toku_blocktable_debug_set_translation(BLOCK_TABLE bt,
        u_int64_t limit,
        struct block_translation_pair *table) {
    lock_for_blocktable();
    if (bt->block_translation) toku_free(bt->block_translation);
    bt->translated_blocknum_limit = limit;
    bt->block_translation = table;
    unlock_for_blocktable();
} 

void
toku_blocktable_create(BLOCK_TABLE *btp,
        BLOCKNUM free_blocks,
        BLOCKNUM unused_blocks,
        u_int64_t translated_blocknum_limit,
        u_int64_t block_translation_address_on_disk,
        u_int64_t block_translation_size_on_disk,
        unsigned char *buffer) {
    lock_for_blocktable();

    BLOCK_TABLE bt;
    XMALLOC(bt);

    bt->free_blocks   = free_blocks;
    bt->unused_blocks = unused_blocks;
    bt->translated_blocknum_limit = translated_blocknum_limit;
    bt->block_translation_address_on_disk = block_translation_address_on_disk;
    update_size_on_disk(bt);
    if (block_translation_address_on_disk==0 && block_translation_size_on_disk == 0) {
        bt->block_translation_size_on_disk = 0;
    }
    assert(block_translation_size_on_disk==bt->block_translation_size_on_disk);


    // Set up the the block translation buffer.
    create_block_allocator(&bt->block_allocator, BLOCK_ALLOCATOR_HEADER_RESERVE, BLOCK_ALLOCATOR_ALIGNMENT);
    if (block_translation_address_on_disk==0) {
        bt->block_translation = NULL;
        assert(buffer==NULL);
    }
    else {
        XMALLOC_N(translated_blocknum_limit, bt->block_translation);
        //Mark where the translation table is stored on disk.
	block_allocator_alloc_block_at(bt->block_allocator, bt->block_translation_size_on_disk, bt->block_translation_address_on_disk);
        //Load translations from the buffer.
        u_int64_t i;
    	struct rbuf rt;
	rt.buf = buffer;
	rt.ndone = 0;
	rt.size = bt->block_translation_size_on_disk-4;//4==checksum
	assert(rt.size>0);
        for (i=0; i<bt->translated_blocknum_limit; i++) {
            bt->block_translation[i].diskoff = rbuf_diskoff(&rt);
            bt->block_translation[i].size    = rbuf_diskoff(&rt);
            if (bt->block_translation[i].size > 0)
                block_allocator_alloc_block_at(bt->block_allocator, bt->block_translation[i].size, bt->block_translation[i].diskoff);
            //printf("%s:%d %ld %ld\n", __FILE__, __LINE__, bt->block_translation[i].diskoff, bt->block_translation[i].size);
        }

    }
    
    // printf("%s:%d translated_blocknum_limit=%ld, block_translation_address_on_disk=%ld\n", __FILE__, __LINE__, bt->translated_blocknum_limit, bt->block_translation_address_on_disk);

    *btp = bt;
    unlock_for_blocktable();
}

void
toku_blocktable_create_new(BLOCK_TABLE *btp) {
    toku_blocktable_create(btp,
                           make_blocknum(-1),
                           make_blocknum(2),
                           0, 0, 0, NULL);
}


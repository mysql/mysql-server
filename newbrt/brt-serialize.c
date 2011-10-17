/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "sort.h"
#include "toku_atomic.h"
#include "threadpool.h"
#include <compress.h>

#if defined(HAVE_CILK)
#include <cilk/cilk.h>
#define cilk_worker_count (__cilkrts_get_nworkers())
#else
#define cilk_spawn
#define cilk_sync
#define cilk_for for
#define cilk_worker_count 1
#endif

static BRT_UPGRADE_STATUS_S upgrade_status;  // accountability, used in backwards_x.c

void 
toku_brt_get_upgrade_status (BRT_UPGRADE_STATUS s) {
    *s = upgrade_status;
}

// performance tracing
#define DO_TOKU_TRACE 0
#if DO_TOKU_TRACE

static inline void do_toku_trace(const char *cp, int len) {
    const int toku_trace_fd = -1;
    write(toku_trace_fd, cp, len);
}
#define toku_trace(a)  do_toku_trace(a, strlen(a))
#else
#define toku_trace(a)
#endif

static int num_cores = 0; // cache the number of cores for the parallelization
static struct toku_thread_pool *brt_pool = NULL;

int 
toku_brt_serialize_init(void) {
    num_cores = toku_os_get_number_active_processors();
    int r = toku_thread_pool_create(&brt_pool, num_cores); lazy_assert_zero(r);
    return 0;
}

int 
toku_brt_serialize_destroy(void) {
    toku_thread_pool_destroy(&brt_pool);
    return 0;
}

// This mutex protects pwrite from running in parallel, and also protects modifications to the block allocator.
static toku_pthread_mutex_t pwrite_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;
static int pwrite_is_locked=0;

int 
toku_pwrite_lock_init(void) {
    int r = toku_pthread_mutex_init(&pwrite_mutex, NULL); resource_assert_zero(r);
    return r;
}

int 
toku_pwrite_lock_destroy(void) {
    int r = toku_pthread_mutex_destroy(&pwrite_mutex); resource_assert_zero(r);
    return r;
}

static inline void
lock_for_pwrite (void) {
    // Locks the pwrite_mutex.
    int r = toku_pthread_mutex_lock(&pwrite_mutex); resource_assert_zero(r);
    pwrite_is_locked = 1;
}

static inline void
unlock_for_pwrite (void) {
    pwrite_is_locked = 0;
    int r = toku_pthread_mutex_unlock(&pwrite_mutex); resource_assert_zero(r);
}

enum {FILE_CHANGE_INCREMENT = (16<<20)};

static inline u_int64_t 
alignup64(u_int64_t a, u_int64_t b) {
    return ((a+b-1)/b)*b;
}

//Race condition if ydb lock is split.
//Ydb lock is held when this function is called.
//Not going to truncate and delete (redirect to devnull) at same time.
//Must be holding a read or write lock on fdlock (fd is protected)
void
toku_maybe_truncate_cachefile (CACHEFILE cf, int fd, u_int64_t size_used)
// Effect: If file size >= SIZE+32MiB, reduce file size.
// (32 instead of 16.. hysteresis).
// Return 0 on success, otherwise an error number.
{
    //Check file size before taking pwrite lock to reduce likelihood of taking
    //the pwrite lock needlessly.
    //Check file size after taking lock to avoid race conditions.
    int64_t file_size;
    if (toku_cachefile_is_dev_null_unlocked(cf)) goto done;
    {
        int r = toku_os_get_file_size(fd, &file_size);
        lazy_assert_zero(r);
        invariant(file_size >= 0);
    }
    // If file space is overallocated by at least 32M
    if ((u_int64_t)file_size >= size_used + (2*FILE_CHANGE_INCREMENT)) {
        lock_for_pwrite();
        {
            int r = toku_os_get_file_size(fd, &file_size);
            lazy_assert_zero(r);
            invariant(file_size >= 0);
        }
        if ((u_int64_t)file_size >= size_used + (2*FILE_CHANGE_INCREMENT)) {
            toku_off_t new_size = alignup64(size_used, (2*FILE_CHANGE_INCREMENT)); //Truncate to new size_used.
            invariant(new_size < file_size);
            int r = toku_cachefile_truncate(cf, new_size);
            lazy_assert_zero(r);
        }
        unlock_for_pwrite();
    }
done:
    return;
}

static u_int64_t 
umin64(u_int64_t a, u_int64_t b) {
    if (a<b) return a;
    return b;
}

int
maybe_preallocate_in_file (int fd, u_int64_t size)
// Effect: If file size is less than SIZE, make it bigger by either doubling it or growing by 16MiB whichever is less.
// Return 0 on success, otherwise an error number.
{
    int64_t file_size;
    {
        int r = toku_os_get_file_size(fd, &file_size);
        if (r != 0) { // debug #2463
            int the_errno = errno;
            fprintf(stderr, "%s:%d fd=%d size=%"PRIu64" r=%d errno=%d\n", __FUNCTION__, __LINE__, fd, size, r, the_errno); fflush(stderr);
        }
        lazy_assert_zero(r);
    }
    invariant(file_size >= 0);
    if ((u_int64_t)file_size < size) {
	const int N = umin64(size, FILE_CHANGE_INCREMENT); // Double the size of the file, or add 16MiB, whichever is less.
	char *MALLOC_N(N, wbuf);
	memset(wbuf, 0, N);
	toku_off_t start_write = alignup64(file_size, 4096);
	invariant(start_write >= file_size);
	toku_os_full_pwrite(fd, wbuf, N, start_write);
	toku_free(wbuf);
    }
    return 0;
}

static void
toku_full_pwrite_extend (int fd, const void *buf, size_t count, toku_off_t offset)
// requires that the pwrite has been locked
// On failure, this does not return (an assertion fails or something).
{
    invariant(pwrite_is_locked);
    {
        int r = maybe_preallocate_in_file(fd, offset+count);
        lazy_assert_zero(r);
    }
    toku_os_full_pwrite(fd, buf, count, offset);
}

// Don't include the sub_block header
// Overhead calculated in same order fields are written to wbuf
enum {
    node_header_overhead = (8+   // magic "tokunode" or "tokuleaf" or "tokuroll"
                            4+   // layout_version
                            4+   // layout_version_original
                            4),  // build_id
};

#include "sub_block.h"
#include "sub_block_map.h"

// uncompressed header offsets
enum {
    uncompressed_magic_offset = 0,
    uncompressed_version_offset = 8,
};

static u_int32_t
serialize_node_header_size(BRTNODE node) {
    u_int32_t retval = 0;
    retval += 8; // magic
    retval += sizeof(node->layout_version);
    retval += sizeof(node->layout_version_original);
    retval += 4; // BUILD_ID
    retval += 4; // n_children
    retval += node->n_children*8; // encode start offset and length of each partition
    retval += 4; // checksum
    return retval;
}

static void
serialize_node_header(BRTNODE node, struct wbuf *wbuf) {
    if (node->height == 0) 
        wbuf_nocrc_literal_bytes(wbuf, "tokuleaf", 8);
    else 
        wbuf_nocrc_literal_bytes(wbuf, "tokunode", 8);
    invariant(node->layout_version == BRT_LAYOUT_VERSION);
    wbuf_nocrc_int(wbuf, node->layout_version);
    wbuf_nocrc_int(wbuf, node->layout_version_original);
    wbuf_nocrc_uint(wbuf, BUILD_ID);
    wbuf_nocrc_int (wbuf, node->n_children);
    for (int i=0; i<node->n_children; i++) {
	assert(BP_SIZE(node,i)>0);
	wbuf_nocrc_int(wbuf, BP_START(node, i)); // save the beginning of the partition
        wbuf_nocrc_int(wbuf, BP_SIZE (node, i));         // and the size
    }
    // checksum the header
    u_int32_t end_to_end_checksum = x1764_memory(wbuf->buf, wbuf_get_woffset(wbuf));
    wbuf_nocrc_int(wbuf, end_to_end_checksum);
    invariant(wbuf->ndone == wbuf->size);
}

static int
wbufwriteleafentry (OMTVALUE lev, u_int32_t UU(idx), void *v) {
    LEAFENTRY le=lev;
    struct wbuf *thisw=v;
    wbuf_nocrc_LEAFENTRY(thisw, le);
    return 0;
}

static u_int32_t 
serialize_brtnode_partition_size (BRTNODE node, int i)
{
    u_int32_t result = 0;
    assert(node->bp[i].state == PT_AVAIL);
    result++; // Byte that states what the partition is
    if (node->height > 0) {
        result += 4; // size of bytes in buffer table
        result += toku_bnc_nbytesinbuf(BNC(node, i));
    }
    else {
        result += 4; // n_entries in buffer table
        result += 4; // optimized_for_upgrade, see if we can get rid of this
        result += BLB_NBYTESINBUF(node, i);
    }
    result += 4; // checksum
    return result;
}

#define BRTNODE_PARTITION_OMT_LEAVES 0xaa
#define BRTNODE_PARTITION_FIFO_MSG 0xbb

static void
serialize_nonleaf_childinfo(NONLEAF_CHILDINFO bnc, struct wbuf *wb)
{
    unsigned char ch = BRTNODE_PARTITION_FIFO_MSG;
    wbuf_nocrc_char(wb, ch);
    // serialize the FIFO, first the number of entries, then the elements
    wbuf_nocrc_int(wb, toku_bnc_n_entries(bnc));
    FIFO_ITERATE(
        bnc->buffer, key, keylen, data, datalen, type, msn, xids, is_fresh,
        {
            invariant((int)type>=0 && type<256);
            wbuf_nocrc_char(wb, (unsigned char)type);
            wbuf_nocrc_char(wb, (unsigned char)is_fresh);
            wbuf_MSN(wb, msn);
            wbuf_nocrc_xids(wb, xids);
            wbuf_nocrc_bytes(wb, key, keylen);
            wbuf_nocrc_bytes(wb, data, datalen);
        });
}

//
// Serialize the i'th partition of node into sb
// For leaf nodes, this would be the i'th basement node
// For internal nodes, this would be the i'th internal node
//
static void
serialize_brtnode_partition(BRTNODE node, int i, struct sub_block *sb) {
    assert(sb->uncompressed_size == 0);
    assert(sb->uncompressed_ptr == NULL);
    sb->uncompressed_size = serialize_brtnode_partition_size(node,i);
    sb->uncompressed_ptr = toku_xmalloc(sb->uncompressed_size);
    //
    // Now put the data into sb->uncompressed_ptr
    //
    struct wbuf wb;
    wbuf_init(&wb, sb->uncompressed_ptr, sb->uncompressed_size);
    if (node->height > 0) {
        // TODO: (Zardosht) possibly exit early if there are no messages
        serialize_nonleaf_childinfo(BNC(node, i), &wb);
    }
    else {
        unsigned char ch = BRTNODE_PARTITION_OMT_LEAVES;
        wbuf_nocrc_char(&wb, ch);
        wbuf_nocrc_int(&wb, BLB_OPTIMIZEDFORUPGRADE(node, i));
        
        wbuf_nocrc_uint(&wb, toku_omt_size(BLB_BUFFER(node, i)));

        //
        // iterate over leafentries and place them into the buffer
        //
        toku_omt_iterate(BLB_BUFFER(node, i), wbufwriteleafentry, &wb);
    }
    u_int32_t end_to_end_checksum = x1764_memory(sb->uncompressed_ptr, wbuf_get_woffset(&wb));
    wbuf_nocrc_int(&wb, end_to_end_checksum);
    invariant(wb.ndone == wb.size);
    invariant(sb->uncompressed_size==wb.ndone);
}

//
// Takes the data in sb->uncompressed_ptr, and compresses it 
// into a newly allocated buffer sb->compressed_ptr
// 
static void
compress_brtnode_sub_block(struct sub_block *sb) {
    assert(sb->compressed_ptr == NULL);
    set_compressed_size_bound(sb);
    // add 8 extra bytes, 4 for compressed size,  4 for decompressed size
    sb->compressed_ptr = toku_xmalloc(sb->compressed_size_bound + 8);
    //
    // This probably seems a bit complicated. Here is what is going on.
    // In TokuDB 5.0, sub_blocks were compressed and the compressed data
    // was checksummed. The checksum did NOT include the size of the compressed data
    // and the size of the uncompressed data. The fields of sub_block only reference the
    // compressed data, and it is the responsibility of the user of the sub_block
    // to write the length
    //
    // For Dr. No, we want the checksum to also include the size of the compressed data, and the 
    // size of the decompressed data, because this data
    // may be read off of disk alone, so it must be verifiable alone.
    //
    // So, we pass in a buffer to compress_nocrc_sub_block that starts 8 bytes after the beginning
    // of sb->compressed_ptr, so we have space to put in the sizes, and then run the checksum.
    //
    sb->compressed_size = compress_nocrc_sub_block(
        sb,
        (char *)sb->compressed_ptr + 8,
        sb->compressed_size_bound
        );

    u_int32_t* extra = (u_int32_t *)(sb->compressed_ptr);
    // store the compressed and uncompressed size at the beginning
    extra[0] = toku_htod32(sb->compressed_size);
    extra[1] = toku_htod32(sb->uncompressed_size);
    // now checksum the entire thing
    sb->compressed_size += 8; // now add the eight bytes that we saved for the sizes
    sb->xsum = x1764_memory(sb->compressed_ptr,sb->compressed_size);


    //
    // This is the end result for Dr. No and forward. For brtnodes, sb->compressed_ptr contains
    // two integers at the beginning, the size and uncompressed size, and then the compressed
    // data. sb->xsum contains the checksum of this entire thing.
    // 
    // In TokuDB 5.0, sb->compressed_ptr only contained the compressed data, sb->xsum
    // checksummed only the compressed data, and the checksumming of the sizes were not
    // done here.
    //
}

//
// Returns the size needed to serialize the brtnode info
// Does not include header information that is common with rollback logs
// such as the magic, layout_version, and build_id
// Includes only node specific info such as pivot information, n_children, and so on
//
static u_int32_t
serialize_brtnode_info_size(BRTNODE node)
{
    u_int32_t retval = 0;
    retval += 8; // max_msn_applied_to_node_on_disk
    retval += 4; // nodesize
    retval += 4; // flags
    retval += 4; // height;
    retval += (3*8+1)*node->n_children; // subtree estimates for each child
    retval += node->totalchildkeylens; // total length of pivots
    retval += (node->n_children-1)*4; // encode length of each pivot
    if (node->height > 0) {
        retval += node->n_children*8; // child blocknum's
    }
    retval += 4; // checksum
    return retval;
}

static void serialize_brtnode_info(BRTNODE node, 
				   SUB_BLOCK sb // output
				   ) {
    assert(sb->uncompressed_size == 0);
    assert(sb->uncompressed_ptr == NULL);
    sb->uncompressed_size = serialize_brtnode_info_size(node);
    sb->uncompressed_ptr = toku_xmalloc(sb->uncompressed_size);
    assert(sb->uncompressed_ptr);
    struct wbuf wb;
    wbuf_init(&wb, sb->uncompressed_ptr, sb->uncompressed_size);

    wbuf_MSN(&wb, node->max_msn_applied_to_node_on_disk);
    wbuf_nocrc_uint(&wb, node->nodesize);
    wbuf_nocrc_uint(&wb, node->flags);
    wbuf_nocrc_int (&wb, node->height);    
    // subtree estimates of each child
    for (int i = 0; i < node->n_children; i++) {
        wbuf_nocrc_ulonglong(&wb, BP_SUBTREE_EST(node,i).nkeys);
        wbuf_nocrc_ulonglong(&wb, BP_SUBTREE_EST(node,i).ndata);
        wbuf_nocrc_ulonglong(&wb, BP_SUBTREE_EST(node,i).dsize);
        wbuf_nocrc_char     (&wb, (char)BP_SUBTREE_EST(node,i).exact);
    }
    // pivot information
    for (int i = 0; i < node->n_children-1; i++) {
        wbuf_nocrc_bytes(&wb, kv_pair_key(node->childkeys[i]), toku_brt_pivot_key_len(node->childkeys[i]));
    }
    // child blocks, only for internal nodes
    if (node->height > 0) {
        for (int i = 0; i < node->n_children; i++) {
            wbuf_nocrc_BLOCKNUM(&wb, BP_BLOCKNUM(node,i));
        }
    }

    u_int32_t end_to_end_checksum = x1764_memory(sb->uncompressed_ptr, wbuf_get_woffset(&wb));
    wbuf_nocrc_int(&wb, end_to_end_checksum);
    invariant(wb.ndone == wb.size);
    invariant(sb->uncompressed_size==wb.ndone);
}


// This is the size of the uncompressed data, not including the compression headers
unsigned int
toku_serialize_brtnode_size (BRTNODE node) {
    unsigned int result = 0;
    //
    // As of now, this seems to be called if and only if the entire node is supposed
    // to be in memory, so we will assert it.
    //
    toku_assert_entire_node_in_memory(node);
    result += serialize_node_header_size(node);
    result += serialize_brtnode_info_size(node);
    for (int i = 0; i < node->n_children; i++) {
        result += serialize_brtnode_partition_size(node,i);
    }
    return result;
}

struct array_info {
    u_int32_t offset;
    OMTVALUE* array;
};

static int
array_item (OMTVALUE lev, u_int32_t idx, void *vsi) {
    struct array_info *ai = vsi;
    ai->array[idx+ai->offset] = lev;
    return 0;
}

struct sum_info {
    unsigned int dsum;
    unsigned int msum;
    unsigned int count;
};

static int
sum_item (OMTVALUE lev, u_int32_t UU(idx), void *vsi) {
    LEAFENTRY le=lev;
    struct sum_info *si = vsi;
    si->count++;
    si->dsum += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
    si->msum += leafentry_memsize(le);
    return 0;
}

// There must still be at least one child
// Requires that all messages in buffers above have been applied.
// Because all messages above have been applied, setting msn of all new basements 
// to max msn of existing basements is correct.  (There cannot be any messages in
// buffers above that still need to be applied.)
static void
rebalance_brtnode_leaf(BRTNODE node, unsigned int basementnodesize)
{
    assert(node->height == 0);
    assert(node->dirty);
    // first create an array of OMTVALUE's that store all the data
    u_int32_t num_le = 0;
    for (int i = 0; i < node->n_children; i++) {
        lazy_assert(BLB_BUFFER(node, i));
        num_le += toku_omt_size(BLB_BUFFER(node, i));
    }
    OMTVALUE *XMALLOC_N(num_le, array);
    // creating array that will store id's of new pivots.
    // allocating num_le of them is overkill, but num_le is an upper bound
    u_int32_t *XMALLOC_N(num_le, new_pivots);
    // now fill in the values into array
    u_int32_t curr_le = 0;
    for (int i = 0; i < node->n_children; i++) {
        OMT curr_omt = BLB_BUFFER(node, i);
        struct array_info ai;
        ai.offset = curr_le;
	ai.array = array;
        toku_omt_iterate(curr_omt, array_item, &ai);
        curr_le += toku_omt_size(curr_omt);
    }

    // figure out the new pivots
    u_int32_t curr_pivot = 0;
    u_int32_t num_le_in_curr_bn = 0;
    u_int32_t bn_size_so_far = 0;
    for (u_int32_t i = 0; i < num_le; i++) {
        u_int32_t curr_size = leafentry_disksize(array[i]);
        if ((bn_size_so_far + curr_size > basementnodesize) && (num_le_in_curr_bn != 0)) {
            // cap off the current basement node to end with the element before i
            new_pivots[curr_pivot] = i-1;
            curr_pivot++;
            num_le_in_curr_bn = 0;
            bn_size_so_far = 0;
        }
        num_le_in_curr_bn++;
        bn_size_so_far += curr_size;
    }

    // now we need to fill in the new basement nodes and pivots

    // TODO: (Zardosht) this is an ugly thing right now
    // Need to figure out how to properly deal with the values seqinsert
    // and optimized_for_upgrade. I am not happy with how this is being
    // handled with basement nodes
    u_int32_t tmp_optimized_for_upgrade = BLB_OPTIMIZEDFORUPGRADE(node, node->n_children-1);
    u_int32_t tmp_seqinsert = BLB_SEQINSERT(node, node->n_children-1);

    MSN max_msn = MIN_MSN;
    for (int i = 0; i < node->n_children; i++) {
        MSN curr_msn = BLB_MAX_MSN_APPLIED(node,i);
        max_msn = (curr_msn.msn > max_msn.msn) ? curr_msn : max_msn;
    }

    // Now destroy the old stuff;
    toku_destroy_brtnode_internals(node);

    // now reallocate pieces and start filling them in
    int num_children = curr_pivot + 1;
    assert(num_children > 0);
    node->totalchildkeylens = 0;

    XMALLOC_N(num_children-1, node->childkeys);
    node->n_children = num_children;
    XMALLOC_N(num_children, node->bp);
    for (int i = 0; i < num_children; i++) {
        set_BLB(node, i, toku_create_empty_bn());
    }

    // now we start to fill in the data

    // first the pivots
    for (int i = 0; i < num_children-1; i++) {
        LEAFENTRY curr_le_pivot = array[new_pivots[i]];
        node->childkeys[i] = kv_pair_malloc(
            le_key(curr_le_pivot),
            le_keylen(curr_le_pivot),
            0,
            0
            );
        assert(node->childkeys[i]);
        node->totalchildkeylens += toku_brt_pivot_key_len(node->childkeys[i]);
    }
    // now the basement nodes
    for (int i = 0; i < num_children; i++) {
        // put back optimized_for_upgrade and seqinsert
        BLB_SEQINSERT(node, i) = tmp_seqinsert;
        BLB_OPTIMIZEDFORUPGRADE(node, i) = tmp_optimized_for_upgrade;

        // create start (inclusive) and end (exclusive) boundaries for data of basement node
        u_int32_t curr_start = (i==0) ? 0 : new_pivots[i-1]+1;
        u_int32_t curr_end = (i==num_children-1) ? num_le : new_pivots[i]+1;
        u_int32_t num_in_bn = curr_end - curr_start;

        OMTVALUE *XMALLOC_N(num_in_bn, bn_array);
        assert(bn_array);
        memcpy(bn_array, &array[curr_start], num_in_bn*(sizeof(array[0])));
        toku_omt_destroy(&BLB_BUFFER(node, i));
        int r = toku_omt_create_steal_sorted_array(
            &BLB_BUFFER(node, i),
            &bn_array,
            num_in_bn,
            num_in_bn
            );
        lazy_assert_zero(r);
        struct sum_info sum_info = {0,0,0};
        toku_omt_iterate(BLB_BUFFER(node, i), sum_item, &sum_info);
        BLB_NBYTESINBUF(node, i) = sum_info.dsum;

        BP_STATE(node,i) = PT_AVAIL;
        BP_TOUCH_CLOCK(node,i);
        BLB_MAX_MSN_APPLIED(node,i) = max_msn;
    }
    node->max_msn_applied_to_node_on_disk = max_msn;

    // now the subtree estimates
    toku_brt_leaf_reset_calc_leaf_stats(node);

    toku_free(array);
    toku_free(new_pivots);
}

static void
serialize_and_compress(BRTNODE node, int npartitions, struct sub_block sb[]);

static void
serialize_and_compress_partition(BRTNODE node, int childnum, SUB_BLOCK sb)
{
    serialize_brtnode_partition(node, childnum, sb);
    compress_brtnode_sub_block(sb);
}

void
toku_create_compressed_partition_from_available(
    BRTNODE node, 
    int childnum, 
    SUB_BLOCK sb
    )
{
    serialize_and_compress_partition(node, childnum, sb);
    //
    // now we have an sb that would be ready for being written out,
    // but we are not writing it out, we are storing it in cache for a potentially
    // long time, so we need to do some cleanup
    //
    // The buffer created above contains metadata in the first 8 bytes, and is overallocated
    // It allocates a bound on the compressed length (evaluated before compression) as opposed
    // to just the amount of the actual compressed data. So, we create a new buffer and copy
    // just the compressed data.
    //
    u_int32_t compressed_size = toku_dtoh32(*(u_int32_t *)sb->compressed_ptr);
    void* compressed_data = toku_xmalloc(compressed_size);
    memcpy(compressed_data, (char *)sb->compressed_ptr + 8, compressed_size);
    toku_free(sb->compressed_ptr);
    sb->compressed_ptr = compressed_data;
    sb->compressed_size = compressed_size;
    if (sb->uncompressed_ptr) {
        toku_free(sb->uncompressed_ptr);
        sb->uncompressed_ptr = NULL;
    }
    
}


// tests are showing that serial insertions are slightly faster 
// using the pthreads than using CILK. Disabling CILK until we have
// some evidence that it is faster
#ifdef HAVE_CILK

static void
serialize_and_compress(BRTNODE node, int npartitions, struct sub_block sb[]) {
#pragma cilk grainsize = 2
    cilk_for (int i = 0; i < npartitions; i++) {
        serialize_and_compress_partition(node, i, &sb[i]);
    }
}

#else

struct serialize_compress_work {
    struct work base;
    BRTNODE node;
    int i;
    struct sub_block *sb;
};

static void *
serialize_and_compress_worker(void *arg) {
    struct workset *ws = (struct workset *) arg;
    while (1) {
        struct serialize_compress_work *w = (struct serialize_compress_work *) workset_get(ws);
        if (w == NULL)
            break;
        int i = w->i;
        serialize_and_compress_partition(w->node, i, &w->sb[i]);
    }
    workset_release_ref(ws);
    return arg;
}

static void
serialize_and_compress(BRTNODE node, int npartitions, struct sub_block sb[]) {
    if (npartitions == 1) {
        serialize_and_compress_partition(node, 0, &sb[0]);
    } else {
        int T = num_cores;
        if (T > npartitions)
            T = npartitions;
        if (T > 0)
            T = T - 1;
        struct workset ws;
        workset_init(&ws);
        struct serialize_compress_work work[npartitions];
        workset_lock(&ws);
        for (int i = 0; i < npartitions; i++) {
            work[i] = (struct serialize_compress_work) { .node = node, .i = i, .sb = sb };
            workset_put_locked(&ws, &work[i].base);
        }
        workset_unlock(&ws);
        toku_thread_pool_run(brt_pool, 0, &T, serialize_and_compress_worker, &ws);
        workset_add_ref(&ws, T);
        serialize_and_compress_worker(&ws);
        workset_join(&ws);
        workset_destroy(&ws);
    }
}

#endif

// Writes out each child to a separate malloc'd buffer, then compresses
// all of them, and writes the uncompressed header, to bytes_to_write,
// which is malloc'd.
//
int
toku_serialize_brtnode_to_memory (BRTNODE node,
                                  unsigned int basementnodesize,
                          /*out*/ size_t *n_bytes_to_write,
                          /*out*/ char  **bytes_to_write)
{
    toku_assert_entire_node_in_memory(node);

    if (node->height == 0) {
        rebalance_brtnode_leaf(node, basementnodesize);
    }
    const int npartitions = node->n_children;

    // Each partition represents a compressed sub block
    // For internal nodes, a sub block is a message buffer
    // For leaf nodes, a sub block is a basement node
    struct sub_block *XMALLOC_N(npartitions, sb);
    struct sub_block sb_node_info;
    for (int i = 0; i < npartitions; i++) {
        sub_block_init(&sb[i]);;
    }
    sub_block_init(&sb_node_info);

    //
    // First, let's serialize and compress the individual sub blocks
    //
    serialize_and_compress(node, npartitions, sb);

    //
    // Now lets create a sub-block that has the common node information,
    // This does NOT include the header
    //
    serialize_brtnode_info(node, &sb_node_info);
    compress_brtnode_sub_block(&sb_node_info);

    // now we have compressed each of our pieces into individual sub_blocks,
    // we can put the header and all the subblocks into a single buffer
    // and return it.

    // The total size of the node is:
    // size of header + disk size of the n+1 sub_block's created above
    u_int32_t total_node_size = (serialize_node_header_size(node) // uncomrpessed header
				 + sb_node_info.compressed_size   // compressed nodeinfo (without its checksum)
				 + 4);                            // nodinefo's checksum
    // store the BP_SIZESs
    for (int i = 0; i < node->n_children; i++) {
	u_int32_t len         = sb[i].compressed_size + 4; // data and checksum
        BP_SIZE (node,i) = len;
	BP_START(node,i) = total_node_size;
        total_node_size += sb[i].compressed_size + 4;
    }

    char *data = toku_xmalloc(total_node_size);
    char *curr_ptr = data;
    // now create the final serialized node

    // write the header
    struct wbuf wb;
    wbuf_init(&wb, curr_ptr, serialize_node_header_size(node));
    serialize_node_header(node, &wb);
    assert(wb.ndone == wb.size);
    curr_ptr += serialize_node_header_size(node);

    // now write sb_node_info
    memcpy(curr_ptr, sb_node_info.compressed_ptr, sb_node_info.compressed_size);
    curr_ptr += sb_node_info.compressed_size;
    // write the checksum
    *(u_int32_t *)curr_ptr = toku_htod32(sb_node_info.xsum);
    curr_ptr += sizeof(sb_node_info.xsum);

    for (int i = 0; i < npartitions; i++) {
        memcpy(curr_ptr, sb[i].compressed_ptr, sb[i].compressed_size);
        curr_ptr += sb[i].compressed_size;
        // write the checksum
        *(u_int32_t *)curr_ptr = toku_htod32(sb[i].xsum);
        curr_ptr += sizeof(sb[i].xsum);
    }
    assert(curr_ptr - data == total_node_size);
    *bytes_to_write = data;
    *n_bytes_to_write = total_node_size;

    //
    // now that node has been serialized, go through sub_block's and free
    // memory
    //
    toku_free(sb_node_info.compressed_ptr);
    toku_free(sb_node_info.uncompressed_ptr);
    for (int i = 0; i < npartitions; i++) {
        toku_free(sb[i].compressed_ptr);
        toku_free(sb[i].uncompressed_ptr);
    }

    toku_free(sb);
    return 0;
}

int
toku_serialize_brtnode_to (int fd, BLOCKNUM blocknum, BRTNODE node, struct brt_header *h, int UU(n_workitems), int UU(n_threads), BOOL for_checkpoint) {

    size_t n_to_write;
    char *compressed_buf = NULL;
    {
	int r = toku_serialize_brtnode_to_memory(node, h->basementnodesize,
                                                 &n_to_write, &compressed_buf);
	if (r!=0) return r;
    }

    //write_now: printf("%s:%d Writing %d bytes\n", __FILE__, __LINE__, w.ndone);
    {
	// If the node has never been written, then write the whole buffer, including the zeros
	invariant(blocknum.b>=0);
	//printf("%s:%d h=%p\n", __FILE__, __LINE__, h);
	//printf("%s:%d translated_blocknum_limit=%lu blocknum.b=%lu\n", __FILE__, __LINE__, h->translated_blocknum_limit, blocknum.b);
	//printf("%s:%d allocator=%p\n", __FILE__, __LINE__, h->block_allocator);
	//printf("%s:%d bt=%p\n", __FILE__, __LINE__, h->block_translation);
	DISKOFF offset;

        toku_blocknum_realloc_on_disk(h->blocktable, blocknum, n_to_write, &offset,
                                      h, for_checkpoint); //dirties h
	lock_for_pwrite();
	toku_full_pwrite_extend(fd, compressed_buf, n_to_write, offset);
	unlock_for_pwrite();
    }

    //printf("%s:%d wrote %d bytes for %lld size=%lld\n", __FILE__, __LINE__, w.ndone, off, size);
    toku_free(compressed_buf);
    node->dirty = 0;  // See #1957.   Must set the node to be clean after serializing it so that it doesn't get written again on the next checkpoint or eviction.
    return 0;
}

static void
deserialize_child_buffer(NONLEAF_CHILDINFO bnc, struct rbuf *rbuf,
                         DB *cmp_extra, brt_compare_func cmp) {
    int r;
    int n_bytes_in_buffer = 0;
    int n_in_this_buffer = rbuf_int(rbuf);
    void **fresh_offsets, **stale_offsets;
    void **broadcast_offsets;
    int nfresh = 0, nstale = 0;
    int nbroadcast_offsets = 0;
    if (cmp) {
        XMALLOC_N(n_in_this_buffer, stale_offsets);
        XMALLOC_N(n_in_this_buffer, fresh_offsets);
        XMALLOC_N(n_in_this_buffer, broadcast_offsets);
    }
    for (int i = 0; i < n_in_this_buffer; i++) {
        bytevec key; ITEMLEN keylen;
        bytevec val; ITEMLEN vallen;
        // this is weird but it's necessary to pass icc and gcc together
        unsigned char ctype = rbuf_char(rbuf);
        enum brt_msg_type type = (enum brt_msg_type) ctype;
        bool is_fresh = rbuf_char(rbuf);
        MSN msn = rbuf_msn(rbuf);
        XIDS xids;
        xids_create_from_buffer(rbuf, &xids);
        rbuf_bytes(rbuf, &key, &keylen); /* Returns a pointer into the rbuf. */
        rbuf_bytes(rbuf, &val, &vallen);
        //printf("Found %s,%s\n", (char*)key, (char*)val);
        long *dest;
        if (cmp) {
            if (brt_msg_type_applies_once(type)) {
                if (is_fresh) {
                    dest = (long *) &fresh_offsets[nfresh];
                    nfresh++;
                } else {
                    dest = (long *) &stale_offsets[nstale];
                    nstale++;
                }
            } else if (brt_msg_type_applies_all(type) || brt_msg_type_does_nothing(type)) {
                dest = (long *) &broadcast_offsets[nbroadcast_offsets];
                nbroadcast_offsets++;
            } else {
                assert(FALSE);
            }
        } else {
            dest = NULL;
        }
        r = toku_fifo_enq(bnc->buffer, key, keylen, val, vallen, type, msn, xids, is_fresh, dest); /* Copies the data into the fifo */
        lazy_assert_zero(r);
        n_bytes_in_buffer += keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_get_serialize_size(xids);
        //printf("Inserted\n");
        xids_destroy(&xids);
    }
    invariant(rbuf->ndone == rbuf->size);

    if (cmp) {
        struct toku_fifo_entry_key_msn_cmp_extra extra = { .cmp_extra = cmp_extra, .cmp = cmp, .fifo = bnc->buffer };
        r = mergesort_r(fresh_offsets, nfresh, sizeof fresh_offsets[0], &extra, toku_fifo_entry_key_msn_cmp);
        assert_zero(r);
        toku_omt_destroy(&bnc->fresh_message_tree);
        r = toku_omt_create_steal_sorted_array(&bnc->fresh_message_tree, &fresh_offsets, nfresh, n_in_this_buffer);
        assert_zero(r);
        r = mergesort_r(stale_offsets, nstale, sizeof stale_offsets[0], &extra, toku_fifo_entry_key_msn_cmp);
        assert_zero(r);
        toku_omt_destroy(&bnc->stale_message_tree);
        r = toku_omt_create_steal_sorted_array(&bnc->stale_message_tree, &stale_offsets, nstale, n_in_this_buffer);
        assert_zero(r);
        toku_omt_destroy(&bnc->broadcast_list);
        r = toku_omt_create_steal_sorted_array(&bnc->broadcast_list, &broadcast_offsets, nbroadcast_offsets, n_in_this_buffer);
        assert_zero(r);
    }
    bnc->n_bytes_in_buffer = n_bytes_in_buffer;
}

// dump a buffer to stderr
// no locking around this for now
static void
dump_bad_block(unsigned char *vp, u_int64_t size) {
    const u_int64_t linesize = 64;
    u_int64_t n = size / linesize;
    for (u_int64_t i = 0; i < n; i++) {
        fprintf(stderr, "%p: ", vp);
	for (u_int64_t j = 0; j < linesize; j++) {
	    unsigned char c = vp[j];
	    fprintf(stderr, "%2.2X", c);
	}
	fprintf(stderr, "\n");
	vp += linesize;
    }
    size = size % linesize;
    for (u_int64_t i=0; i<size; i++) {
	if ((i % linesize) == 0)
	    fprintf(stderr, "%p: ", vp+i);
	fprintf(stderr, "%2.2X", vp[i]);
	if (((i+1) % linesize) == 0)
	    fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

BASEMENTNODE toku_create_empty_bn(void) {
    BASEMENTNODE bn = toku_create_empty_bn_no_buffer();
    int r;
    r = toku_omt_create(&bn->buffer);
    assert_zero(r);
    return bn;
}

BASEMENTNODE toku_create_empty_bn_no_buffer(void) {
    BASEMENTNODE XMALLOC(bn);
    bn->max_msn_applied.msn = 0;
    bn->buffer = NULL;
    bn->n_bytes_in_buffer = 0;
    bn->seqinsert = 0;
    bn->optimized_for_upgrade = 0;
    bn->stale_ancestor_messages_applied = false;
    return bn;
}

NONLEAF_CHILDINFO toku_create_empty_nl(void) {
    NONLEAF_CHILDINFO XMALLOC(cn);
    cn->n_bytes_in_buffer = 0;
    int r = toku_fifo_create(&cn->buffer); assert_zero(r);
    r = toku_omt_create(&cn->fresh_message_tree); assert_zero(r);
    r = toku_omt_create(&cn->stale_message_tree); assert_zero(r);
    r = toku_omt_create(&cn->broadcast_list); assert_zero(r);
    return cn;
}

void destroy_basement_node (BASEMENTNODE bn)
{
    // The buffer may have been freed already, in some cases.
    if (bn->buffer) {
	toku_omt_destroy(&bn->buffer);
    }
    toku_free(bn);
}

void destroy_nonleaf_childinfo (NONLEAF_CHILDINFO nl)
{
    toku_fifo_free(&nl->buffer);
    toku_omt_destroy(&nl->fresh_message_tree);
    toku_omt_destroy(&nl->stale_message_tree);
    toku_omt_destroy(&nl->broadcast_list);
    toku_free(nl);
}

// 
static int
read_block_from_fd_into_rbuf(
    int fd, 
    BLOCKNUM blocknum,
    struct brt_header *h,
    struct rbuf *rb
    ) 
{
    if (h->panic) {
        toku_trace("panic set, will not read block from fd into buf");
        return h->panic;
    }
    toku_trace("deserial start nopanic");
    
    // get the file offset and block size for the block
    DISKOFF offset, size;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &offset, &size);
    u_int8_t *XMALLOC_N(size, raw_block);
    rbuf_init(rb, raw_block, size);
    {
        // read the block
        ssize_t rlen = toku_os_pread(fd, raw_block, size, offset);
        lazy_assert((DISKOFF)rlen == size);
    }
    
    return 0;
}

//
// read the compressed partition into the sub_block,
// validate the checksum of the compressed data
//
static void
read_compressed_sub_block(struct rbuf *rb, struct sub_block *sb)
{
    sb->compressed_size = rbuf_int(rb);
    sb->uncompressed_size = rbuf_int(rb);
    bytevec* cp = (bytevec*)&sb->compressed_ptr;
    rbuf_literal_bytes(rb, cp, sb->compressed_size);
    sb->xsum = rbuf_int(rb);
    // let's check the checksum
    u_int32_t actual_xsum = x1764_memory((char *)sb->compressed_ptr-8, 8+sb->compressed_size);
    invariant(sb->xsum == actual_xsum);

}

static void 
read_and_decompress_sub_block(struct rbuf *rb, struct sub_block *sb)
{
    read_compressed_sub_block(rb, sb);
    sb->uncompressed_ptr = toku_xmalloc(sb->uncompressed_size);
    assert(sb->uncompressed_ptr);
    
    toku_decompress(
        sb->uncompressed_ptr,
        sb->uncompressed_size,
        sb->compressed_ptr,
        sb->compressed_size
        );
}

// verify the checksum
static void
verify_brtnode_sub_block (struct sub_block *sb)
{
    // first verify the checksum
    u_int32_t data_size = sb->uncompressed_size - 4; // checksum is 4 bytes at end
    u_int32_t stored_xsum = toku_dtoh32(*((u_int32_t *)((char *)sb->uncompressed_ptr + data_size)));
    u_int32_t actual_xsum = x1764_memory(sb->uncompressed_ptr, data_size);
    if (stored_xsum != actual_xsum) {
        dump_bad_block(sb->uncompressed_ptr, sb->uncompressed_size);
        assert(FALSE);
    }
}

// This function deserializes the data stored by serialize_brtnode_info
static void 
deserialize_brtnode_info(
    struct sub_block *sb, 
    BRTNODE node
    )
{
    // sb_node_info->uncompressed_ptr stores the serialized node information
    // this function puts that information into node

    // first verify the checksum
    verify_brtnode_sub_block(sb);
    u_int32_t data_size = sb->uncompressed_size - 4; // checksum is 4 bytes at end

    // now with the data verified, we can read the information into the node
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};
    rbuf_init(&rb, sb->uncompressed_ptr, data_size);

    node->max_msn_applied_to_node_on_disk = rbuf_msn(&rb);
    node->nodesize = rbuf_int(&rb);
    node->flags = rbuf_int(&rb);
    node->height = rbuf_int(&rb);

    // now create the basement nodes or childinfos, depending on whether this is a
    // leaf node or internal node    
    // now the subtree_estimates

    // n_children is now in the header, nd the allocatio of the node->bp is in deserialize_brtnode_from_rbuf.
    assert(node->bp!=NULL); // 

    for (int i=0; i < node->n_children; i++) {
        SUBTREE_EST curr_se = &BP_SUBTREE_EST(node,i);
        curr_se->nkeys = rbuf_ulonglong(&rb);
        curr_se->ndata = rbuf_ulonglong(&rb);
        curr_se->dsize = rbuf_ulonglong(&rb);
        curr_se->exact = (BOOL) (rbuf_char(&rb) != 0);
    }

    // now the pivots
    node->totalchildkeylens = 0;
    if (node->n_children > 1) {
        XMALLOC_N(node->n_children - 1, node->childkeys);
        assert(node->childkeys);
        for (int i=0; i < node->n_children-1; i++) {
            bytevec childkeyptr;
            unsigned int cklen;
            rbuf_bytes(&rb, &childkeyptr, &cklen);
            node->childkeys[i] = kv_pair_malloc((void*)childkeyptr, cklen, 0, 0);
            node->totalchildkeylens += toku_brt_pivot_key_len(node->childkeys[i]);
        }
    }
    else {
	node->childkeys = NULL;
	node->totalchildkeylens = 0;
    }

    // if this is an internal node, unpack the block nums, and fill in necessary fields
    // of childinfo
    if (node->height > 0) {
        for (int i = 0; i < node->n_children; i++) {
            BP_BLOCKNUM(node,i) = rbuf_blocknum(&rb);
	    BP_WORKDONE(node, i) = 0;
        }        
    }
 
    // make sure that all the data was read
    if (data_size != rb.ndone) {
        dump_bad_block(rb.buf, rb.size);
        assert(FALSE);
    }
}

static void
setup_available_brtnode_partition(BRTNODE node, int i) {
    if (node->height == 0) {
	set_BLB(node, i, toku_create_empty_bn());
        BLB_MAX_MSN_APPLIED(node,i) = node->max_msn_applied_to_node_on_disk;
    }
    else {
	set_BNC(node, i, toku_create_empty_nl());
    }
}

static void
setup_brtnode_partitions(BRTNODE node, struct brtnode_fetch_extra* bfe) {
    if (bfe->type == brtnode_fetch_subset && bfe->search != NULL) {
        // we do not take into account prefetching yet
        // as of now, if we need a subset, the only thing
        // we can possibly require is a single basement node
        // we find out what basement node the query cares about
        // and check if it is available
        assert(bfe->cmp);
        assert(bfe->search);
        bfe->child_to_read = toku_brt_search_which_child(
            bfe->cmp_extra,
            bfe->cmp,
            node,
            bfe->search
            );
    }
    int lc, rc;
    if (bfe->type == brtnode_fetch_subset || bfe->type == brtnode_fetch_prefetch) {
        lc = toku_bfe_leftmost_child_wanted(bfe, node);
        rc = toku_bfe_rightmost_child_wanted(bfe, node);
    } else {
        lc = -1;
        rc = -1;
    }
    //
    // setup memory needed for the node
    //
    //printf("node height %d, blocknum %"PRId64", type %d lc %d rc %d\n", node->height, node->thisnodename.b, bfe->type, lc, rc);
    for (int i = 0; i < node->n_children; i++) {
        BP_INIT_UNTOUCHED_CLOCK(node,i);
        BP_STATE(node, i) = ((toku_bfe_wants_child_available(bfe, i) || (lc <= i && i <= rc))
                             ? PT_AVAIL : PT_COMPRESSED);
        BP_WORKDONE(node,i) = 0;
        if (BP_STATE(node,i) == PT_AVAIL) {
	    //printf(" %d is available\n", i);
            setup_available_brtnode_partition(node, i);
            BP_TOUCH_CLOCK(node,i);
        }
        else if (BP_STATE(node,i) == PT_COMPRESSED) {
	    //printf(" %d is compressed\n", i);
            set_BSB(node, i, sub_block_creat());
        }
        else {
            assert(FALSE);
        }
    }
}

static void
deserialize_brtnode_partition(
    struct sub_block *sb,
    BRTNODE node,
    int index,
    DB *cmp_extra,
    brt_compare_func cmp
    )
{
    verify_brtnode_sub_block(sb);
    u_int32_t data_size = sb->uncompressed_size - 4; // checksum is 4 bytes at end

    // now with the data verified, we can read the information into the node
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};
    rbuf_init(&rb, sb->uncompressed_ptr, data_size);
    u_int32_t start_of_data;

    if (node->height > 0) {
        unsigned char ch = rbuf_char(&rb);
        assert(ch == BRTNODE_PARTITION_FIFO_MSG);
        deserialize_child_buffer(BNC(node, index), &rb, cmp_extra, cmp);
        BP_WORKDONE(node, index) = 0;
    }
    else {
        unsigned char ch = rbuf_char(&rb);
        assert(ch == BRTNODE_PARTITION_OMT_LEAVES);
        BLB_OPTIMIZEDFORUPGRADE(node, index) = rbuf_int(&rb);
        BLB_SEQINSERT(node, index) = 0;
        u_int32_t num_entries = rbuf_int(&rb);
        OMTVALUE *XMALLOC_N(num_entries, array);
        start_of_data = rb.ndone;
        for (u_int32_t i = 0; i < num_entries; i++) {
            LEAFENTRY le = (LEAFENTRY)(&rb.buf[rb.ndone]);
            u_int32_t disksize = leafentry_disksize(le);
            rb.ndone += disksize;
            invariant(rb.ndone<=rb.size);
            array[i]=toku_xmalloc(disksize);
            assert(array[i]);
            memcpy(array[i], le, disksize);
        }
        u_int32_t end_of_data = rb.ndone;
        BLB_NBYTESINBUF(node, index) += end_of_data-start_of_data + num_entries*OMT_ITEM_OVERHEAD;
        // destroy old buffer that was created by toku_setup_basementnode, so we can create a new one
        toku_omt_destroy(&BLB_BUFFER(node, index));
        int r = toku_omt_create_steal_sorted_array(&BLB_BUFFER(node, index), &array, num_entries, num_entries);
        assert(r == 0);
    }
    assert(rb.ndone == rb.size);
}

static void
decompress_and_deserialize_worker(struct rbuf curr_rbuf, struct sub_block curr_sb, BRTNODE node, int child, DB *cmp_extra, brt_compare_func cmp)
{
    read_and_decompress_sub_block(&curr_rbuf, &curr_sb);
    // at this point, sb->uncompressed_ptr stores the serialized node partition
    deserialize_brtnode_partition(&curr_sb, node, child, cmp_extra, cmp);
    toku_free(curr_sb.uncompressed_ptr);
}

static void
check_and_copy_compressed_sub_block_worker(struct rbuf curr_rbuf, struct sub_block curr_sb, BRTNODE node, int child)
{
    read_compressed_sub_block(&curr_rbuf, &curr_sb);
    SUB_BLOCK bp_sb = BSB(node, child);
    bp_sb->compressed_size = curr_sb.compressed_size;
    bp_sb->uncompressed_size = curr_sb.uncompressed_size;
    bp_sb->compressed_ptr = toku_xmalloc(bp_sb->compressed_size);
    memcpy(bp_sb->compressed_ptr, curr_sb.compressed_ptr, bp_sb->compressed_size);
}

//
// deserializes a brtnode that is in rb (with pointer of rb just past the magic) into a BRTNODE
//
static int
deserialize_brtnode_from_rbuf(
    BRTNODE *brtnode,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    struct brtnode_fetch_extra* bfe,
    struct rbuf *rb
    )
{
    int r = 0;
    BRTNODE node = NULL;
    u_int32_t stored_checksum, checksum;
    struct sub_block sb_node_info;
    node = toku_xmalloc(sizeof(*node));
    if (node == NULL) goto cleanup;

    // fill in values that are known and not stored in rb
    node->fullhash = fullhash;
    node->thisnodename = blocknum;
    node->dirty = 0;

    // now start reading from rbuf

    // first thing we do is read the header information
    node->layout_version_read_from_disk = rbuf_int(rb);
    // TODO: (Zardosht), worry about upgrade
    if (node->layout_version_read_from_disk != BRT_LAYOUT_VERSION) {
        r = EINVAL;
        goto cleanup;
    }
    node->layout_version = node->layout_version_read_from_disk;
    node->layout_version_original = rbuf_int(rb);
    node->build_id = rbuf_int(rb);
    node->n_children = rbuf_int(rb);
    XMALLOC_N(node->n_children, node->bp);
    // read the partition locations
    for (int i=0; i<node->n_children; i++) {
        BP_START(node,i) = rbuf_int(rb);
        BP_SIZE (node,i) = rbuf_int(rb);
    }
    // verify checksum of header stored
    checksum = x1764_memory(rb->buf, rb->ndone);
    stored_checksum = rbuf_int(rb);
    if (stored_checksum != checksum) {
        dump_bad_block(rb->buf, rb->size);
        invariant(stored_checksum == checksum);
    }

    //now we read and decompress the pivot and child information
    sub_block_init(&sb_node_info);
    read_and_decompress_sub_block(rb, &sb_node_info);
    // at this point, sb->uncompressed_ptr stores the serialized node info
    deserialize_brtnode_info(&sb_node_info, node);
    toku_free(sb_node_info.uncompressed_ptr);

    // now that the node info has been deserialized, we can proceed to deserialize
    // the individual sub blocks
    assert(bfe->type == brtnode_fetch_none || bfe->type == brtnode_fetch_subset || bfe->type == brtnode_fetch_all || bfe->type == brtnode_fetch_prefetch);

    // setup the memory of the partitions
    // for partitions being decompressed, create either FIFO or basement node
    // for partitions staying compressed, create sub_block
    setup_brtnode_partitions(node,bfe);

    // Previously, this code was a for loop with spawns inside and a sync at the end.
    // But now the loop is parallelizeable since we don't have a dependency on the work done so far.
    cilk_for (int i = 0; i < node->n_children; i++) {
        u_int32_t curr_offset = BP_START(node,i);
        u_int32_t curr_size   = BP_SIZE(node,i);
        // the compressed, serialized partitions start at where rb is currently pointing,
        // which would be rb->buf + rb->ndone
        // we need to intialize curr_rbuf to point to this place
        struct rbuf curr_rbuf  = {.buf = NULL, .size = 0, .ndone = 0};
        rbuf_init(&curr_rbuf, rb->buf + curr_offset, curr_size);

        //
        // now we are at the point where we have:
        //  - read the entire compressed node off of disk,
        //  - decompressed the pivot and offset information,
        //  - have arrived at the individual partitions.
        //
        // Based on the information in bfe, we want to decompress a subset of
        // of the compressed partitions (also possibly none or possibly all)
        // The partitions that we want to decompress and make available
        // to the node, we do, the rest we simply copy in compressed
        // form into the node, and set the state of the partition to PT_COMPRESSED
        //

        struct sub_block curr_sb;
        sub_block_init(&curr_sb);

	// curr_rbuf is passed by value to decompress_and_deserialize_worker, so there's no ugly race condition.
	// This would be more obvious if curr_rbuf were an array.

        // deserialize_brtnode_info figures out what the state
        // should be and sets up the memory so that we are ready to use it

	switch (BP_STATE(node,i)) {
	case PT_AVAIL:
	    //  case where we read and decompress the partition
            decompress_and_deserialize_worker(curr_rbuf, curr_sb, node, i, bfe->cmp_extra, bfe->cmp);
	    continue;
	case PT_COMPRESSED:
	    // case where we leave the partition in the compressed state
            check_and_copy_compressed_sub_block_worker(curr_rbuf, curr_sb, node, i);
	    continue;
	case PT_INVALID: // this is really bad
	case PT_ON_DISK: // it's supposed to be in memory.
	    assert(0);
	    continue;
        }
	assert(0);
    }
    *brtnode = node;
    r = 0;
cleanup:
    if (r != 0) {
        if (node) toku_free(node);
    }
    return r;
}

void
toku_deserialize_bp_from_disk(BRTNODE node, int childnum, int fd, struct brtnode_fetch_extra* bfe) {
    assert(BP_STATE(node,childnum) == PT_ON_DISK);
    assert(node->bp[childnum].ptr.tag == BCT_NULL);
    
    //
    // setup the partition
    //
    setup_available_brtnode_partition(node, childnum);
    BP_STATE(node,childnum) = PT_AVAIL;
    
    //
    // read off disk and make available in memory
    // 
    // get the file offset and block size for the block
    DISKOFF node_offset, total_node_disk_size;
    toku_translate_blocknum_to_offset_size(
        bfe->h->blocktable, 
        node->thisnodename, 
        &node_offset, 
        &total_node_disk_size
        );

    u_int32_t curr_offset = BP_START(node, childnum);
    u_int32_t curr_size   = BP_SIZE (node, childnum);
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};

    u_int8_t *XMALLOC_N(curr_size, raw_block);
    rbuf_init(&rb, raw_block, curr_size);
    {
        // read the block
        ssize_t rlen = toku_os_pread(fd, raw_block, curr_size, node_offset+curr_offset);
        lazy_assert((DISKOFF)rlen == curr_size);
    }

    struct sub_block curr_sb;
    sub_block_init(&curr_sb);

    read_and_decompress_sub_block(&rb, &curr_sb);
    // at this point, sb->uncompressed_ptr stores the serialized node partition
    deserialize_brtnode_partition(&curr_sb, node, childnum, bfe->cmp_extra, bfe->cmp);
    if (node->height == 0) {
        toku_brt_bn_reset_stats(node, childnum);
    }
    toku_free(curr_sb.uncompressed_ptr);
    toku_free(raw_block);
}

// Take a brtnode partition that is in the compressed state, and make it avail
void
toku_deserialize_bp_from_compressed(BRTNODE node, int childnum,
                                    DB *cmp_extra, brt_compare_func cmp) {
    assert(BP_STATE(node, childnum) == PT_COMPRESSED);
    SUB_BLOCK curr_sb = BSB(node, childnum);

    assert(curr_sb->uncompressed_ptr == NULL);
    curr_sb->uncompressed_ptr = toku_xmalloc(curr_sb->uncompressed_size);

    setup_available_brtnode_partition(node, childnum);
    BP_STATE(node,childnum) = PT_AVAIL;
    // decompress the sub_block
    toku_decompress(
        curr_sb->uncompressed_ptr,
        curr_sb->uncompressed_size,
        curr_sb->compressed_ptr,
        curr_sb->compressed_size
        );
    deserialize_brtnode_partition(curr_sb, node, childnum, cmp_extra, cmp);
    if (node->height == 0) {
        toku_brt_bn_reset_stats(node, childnum);
    }
    toku_free(curr_sb->uncompressed_ptr);
    toku_free(curr_sb->compressed_ptr);
    toku_free(curr_sb);
}


// Read brt node from file into struct.  Perform version upgrade if necessary.
int
toku_deserialize_brtnode_from (
    int fd, 
    BLOCKNUM blocknum, 
    u_int32_t fullhash,
    BRTNODE *brtnode, 
    struct brtnode_fetch_extra* bfe
    )
{
    toku_trace("deserial start");

    int r;
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};

    r = read_block_from_fd_into_rbuf(fd, blocknum, bfe->h, &rb);
    if (r != 0) { goto cleanup; }

    bytevec magic;
    rbuf_literal_bytes(&rb, &magic, 8);
    if (memcmp(magic, "tokuleaf", 8)!=0 &&
        memcmp(magic, "tokunode", 8)!=0) {
        r = toku_db_badformat();
        goto cleanup;
    }

    r = deserialize_brtnode_from_rbuf(brtnode, blocknum, fullhash, bfe, &rb);
    if (r!=0) {
        dump_bad_block(rb.buf,rb.size);
    }
    lazy_assert_zero(r);

    toku_trace("deserial done");

cleanup:
    if (rb.buf) toku_free(rb.buf);
    return r;
}


int
toku_maybe_upgrade_brt(BRT t) {	// possibly do some work to complete the version upgrade of brt
    // If someday we need to inject a message to upgrade the brt, this is where 
    // it should be done.  Whenever an upgrade is done, all nodes will be marked
    // as dirty, so it makes sense here to always inject an OPTIMIZE message.
    // (Note, if someday the version number is stored in the translation instead
    // of in each node, then the upgrade would not necessarily dirty each node.)
    int r = 0;

    int version = t->h->layout_version_read_from_disk;

    int upgrade = 0;
    if (!t->h->upgrade_brt_performed) { // upgrade may be necessary
	switch (version) {
            case BRT_LAYOUT_VERSION_13:
                r = 0;
                upgrade++;
                //Fall through on purpose.
            case BRT_LAYOUT_VERSION:
                if (r == 0 && upgrade) {
                    r = toku_brt_optimize_for_upgrade(t);
		    if (r==0)
			toku_sync_fetch_and_increment_uint64(&upgrade_status.optimized_for_upgrade);
                }
                if (r == 0) {
                    t->h->upgrade_brt_performed = TRUE;  // no further upgrade necessary
                }
                break;
            default:
                invariant(FALSE);
	}
    }
    if (r) {
	if (t->h->panic==0) {
	    char *e = strerror(r);
	    int   l = 200 + strlen(e);
	    char s[l];
	    t->h->panic=r;
	    snprintf(s, l-1, "While upgrading brt version, error %d (%s)", r, e);
	    t->h->panic_string = toku_strdup(s);
	}
    }
    return r;
}


// ################


void
toku_verify_or_set_counts (BRTNODE node) {
    node = node;
    if (node->height==0) {
        for (int i=0; i<node->n_children; i++) {
            lazy_assert(BLB_BUFFER(node, i));
            struct sum_info sum_info = {0,0,0};
            toku_omt_iterate(BLB_BUFFER(node, i), sum_item, &sum_info);
            lazy_assert(sum_info.count==toku_omt_size(BLB_BUFFER(node, i)));
            lazy_assert(sum_info.dsum==BLB_NBYTESINBUF(node, i));
        }
    }
    else {
        // nothing to do because we no longer store n_bytes_in_buffers for
        // the whole node
    }
}

static u_int32_t
serialize_brt_header_min_size (u_int32_t version) {
    u_int32_t size = 0;


    switch(version) {
        case BRT_LAYOUT_VERSION_15:
            size += 4;  // basement node size
        case BRT_LAYOUT_VERSION_14:
            size += 8;  //TXNID that created
        case BRT_LAYOUT_VERSION_13:
            size += ( 4 // build_id
                     +4 // build_id_original
                     +8 // time_of_creation
                     +8 // time_of_last_modification
                    );
            // fall through
        case BRT_LAYOUT_VERSION_12:
	    size += (+8 // "tokudata"
		     +4 // version
		     +4 // original_version
		     +4 // size
		     +8 // byte order verification
		     +8 // checkpoint_count
		     +8 // checkpoint_lsn
		     +4 // tree's nodesize
		     +8 // translation_size_on_disk
		     +8 // translation_address_on_disk
		     +4 // checksum
                     +8 // Number of blocks in old version.
	             +8 // diskoff
		     +4 // flags
		   );
	    break;
        default:
            lazy_assert(FALSE);
    }
    lazy_assert(size <= BLOCK_ALLOCATOR_HEADER_RESERVE);
    return size;
}

int toku_serialize_brt_header_size (struct brt_header *h) {
    u_int32_t size = serialize_brt_header_min_size(h->layout_version);
    //There is no dynamic data.
    lazy_assert(size <= BLOCK_ALLOCATOR_HEADER_RESERVE);
    return size;
}


int toku_serialize_brt_header_to_wbuf (struct wbuf *wbuf, struct brt_header *h, DISKOFF translation_location_on_disk, DISKOFF translation_size_on_disk) {
    unsigned int size = toku_serialize_brt_header_size (h); // !!! seems silly to recompute the size when the caller knew it.  Do we really need the size?
    wbuf_literal_bytes(wbuf, "tokudata", 8);
    wbuf_network_int  (wbuf, h->layout_version); //MUST be in network order regardless of disk order
    wbuf_network_int  (wbuf, BUILD_ID); //MUST be in network order regardless of disk order
    wbuf_network_int  (wbuf, size); //MUST be in network order regardless of disk order
    wbuf_literal_bytes(wbuf, &toku_byte_order_host, 8); //Must not translate byte order
    wbuf_ulonglong(wbuf, h->checkpoint_count);
    wbuf_LSN    (wbuf, h->checkpoint_lsn);
    wbuf_int    (wbuf, h->nodesize);

    //printf("%s:%d bta=%lu size=%lu\n", __FILE__, __LINE__, h->block_translation_address_on_disk, 4 + 16*h->translated_blocknum_limit);
    wbuf_DISKOFF(wbuf, translation_location_on_disk);
    wbuf_DISKOFF(wbuf, translation_size_on_disk);
    wbuf_BLOCKNUM(wbuf, h->root);
    wbuf_int(wbuf, h->flags);
    wbuf_int(wbuf, h->layout_version_original);
    wbuf_int(wbuf, h->build_id_original);
    wbuf_ulonglong(wbuf, h->time_of_creation);
    wbuf_ulonglong(wbuf, h->time_of_last_modification);
    wbuf_ulonglong(wbuf, h->num_blocks_to_upgrade);
    wbuf_TXNID(wbuf, h->root_xid_that_created);
    wbuf_int(wbuf, h->basementnodesize);
    u_int32_t checksum = x1764_finish(&wbuf->checksum);
    wbuf_int(wbuf, checksum);
    lazy_assert(wbuf->ndone == wbuf->size);
    return 0;
}

int toku_serialize_brt_header_to (int fd, struct brt_header *h) {
    int rr = 0;
    if (h->panic) return h->panic;
    lazy_assert(h->type==BRTHEADER_CHECKPOINT_INPROGRESS);
    toku_brtheader_lock(h);
    struct wbuf w_translation;
    int64_t size_translation;
    int64_t address_translation;
    {
        //Must serialize translation first, to get address,size for header.
        toku_serialize_translation_to_wbuf_unlocked(h->blocktable, &w_translation,
                                                   &address_translation,
                                                   &size_translation);
        lazy_assert(size_translation==w_translation.size);
    }
    struct wbuf w_main;
    unsigned int size_main = toku_serialize_brt_header_size (h);
    {
	wbuf_init(&w_main, toku_xmalloc(size_main), size_main);
	{
	    int r=toku_serialize_brt_header_to_wbuf(&w_main, h, address_translation, size_translation);
	    lazy_assert_zero(r);
	}
	lazy_assert(w_main.ndone==size_main);
    }
    toku_brtheader_unlock(h);
    lock_for_pwrite();
    {
        //Actual Write translation table
	toku_full_pwrite_extend(fd, w_translation.buf,
				size_translation, address_translation);
    }
    {
        //Everything but the header MUST be on disk before header starts.
        //Otherwise we will think the header is good and some blocks might not
        //yet be on disk.
        //If the header has a cachefile we need to do cachefile fsync (to
        //prevent crash if we redirected to dev null)
        //If there is no cachefile we still need to do an fsync.
        if (h->cf) {
            rr = toku_cachefile_fsync(h->cf);
        }
        else {
            rr = toku_file_fsync(fd);
        }
        if (rr==0) {
            //Alternate writing header to two locations:
            //   Beginning (0) or BLOCK_ALLOCATOR_HEADER_RESERVE
            toku_off_t main_offset;
            main_offset = (h->checkpoint_count & 0x1) ? 0 : BLOCK_ALLOCATOR_HEADER_RESERVE;
            toku_full_pwrite_extend(fd, w_main.buf, w_main.ndone, main_offset);
        }
    }
    toku_free(w_main.buf);
    toku_free(w_translation.buf);
    unlock_for_pwrite();
    return rr;
}

// not version-sensitive because we only serialize a descriptor using the current layout_version
u_int32_t
toku_serialize_descriptor_size(const DESCRIPTOR desc) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4; // four bytes for size of descriptor
    size += desc->dbt.size;
    return size;
}

static u_int32_t
deserialize_descriptor_size(const DESCRIPTOR desc, int layout_version) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4; // four bytes for size of descriptor
    if (layout_version == BRT_LAYOUT_VERSION_13)
	size += 4;   // for version 13, include four bytes of "version"
    size += desc->dbt.size;
    return size;
}

void
toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc) {
    wbuf_bytes(wb, desc->dbt.data, desc->dbt.size);
}

//Descriptor is written to disk during toku_brt_open iff we have a new (or changed)
//descriptor.
//Descriptors are NOT written during the header checkpoint process.
int
toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset) {
    int r = 0;
    // make the checksum
    int64_t size = toku_serialize_descriptor_size(desc)+4; //4 for checksum
    struct wbuf w;
    wbuf_init(&w, toku_xmalloc(size), size);
    toku_serialize_descriptor_contents_to_wbuf(&w, desc);
    {
        //Add checksum
        u_int32_t checksum = x1764_finish(&w.checksum);
        wbuf_int(&w, checksum);
    }
    lazy_assert(w.ndone==w.size);
    {
        lock_for_pwrite();
        //Actual Write translation table
	toku_full_pwrite_extend(fd, w.buf, size, offset);
        unlock_for_pwrite();
    }
    toku_free(w.buf);
    return r;
}

static void
deserialize_descriptor_from_rbuf(struct rbuf *rb, DESCRIPTOR desc, int layout_version) {
    if (layout_version == BRT_LAYOUT_VERSION_13) {
	// in older versions of TokuDB the Descriptor had a 4 byte version, which we must skip over
	u_int32_t dummy_version;
	dummy_version = rbuf_int(rb);
    }
    u_int32_t size;
    bytevec   data;
    rbuf_bytes(rb, &data, &size);
    bytevec   data_copy = data;;
    if (size>0) {
	data_copy = toku_memdup(data, size); //Cannot keep the reference from rbuf. Must copy.
	lazy_assert(data_copy);
    }
    else {
        lazy_assert(size==0);
        data_copy = NULL;
    }
    toku_fill_dbt(&desc->dbt, data_copy, size);
}

static void
deserialize_descriptor_from(int fd, BLOCK_TABLE bt, DESCRIPTOR desc, int layout_version) {
    DISKOFF offset;
    DISKOFF size;
    toku_get_descriptor_offset_size(bt, &offset, &size);
    memset(desc, 0, sizeof(*desc));
    if (size > 0) {
        lazy_assert(size>=4); //4 for checksum
        {
            unsigned char *XMALLOC_N(size, dbuf);
            {
                lock_for_pwrite();
                ssize_t r = toku_os_pread(fd, dbuf, size, offset);
                lazy_assert(r==size);
                unlock_for_pwrite();
            }
            {
                // check the checksum
                u_int32_t x1764 = x1764_memory(dbuf, size-4);
                //printf("%s:%d read from %ld (x1764 offset=%ld) size=%ld\n", __FILE__, __LINE__, block_translation_address_on_disk, offset, block_translation_size_on_disk);
                u_int32_t stored_x1764 = toku_dtoh32(*(int*)(dbuf + size-4));
                lazy_assert(x1764 == stored_x1764);
            }
            {
                struct rbuf rb = {.buf = dbuf, .size = size, .ndone = 0};
                //Not temporary; must have a toku_memdup'd copy.
                deserialize_descriptor_from_rbuf(&rb, desc, layout_version);
            }
            lazy_assert(deserialize_descriptor_size(desc, layout_version)+4 == size);
            toku_free(dbuf);
        }
    }
}


// We only deserialize brt header once and then share everything with all the brts.
static int
deserialize_brtheader (int fd, struct rbuf *rb, struct brt_header **brth) {
    // We already know:
    //  we have an rbuf representing the header.
    //  The checksum has been validated

    //Steal rbuf (used to simplify merge, reduce diff size, and keep old code)
    struct rbuf rc = *rb;
    memset(rb, 0, sizeof(*rb));

    //Verification of initial elements.
    {
        //Check magic number
        bytevec magic;
        rbuf_literal_bytes(&rc, &magic, 8);
        lazy_assert(memcmp(magic,"tokudata",8)==0);
    }
 

    struct brt_header *CALLOC(h);
    if (h==0) return errno;
    int ret=-1;
    if (0) { died1: toku_free(h); return ret; }
    h->type = BRTHEADER_CURRENT;
    h->checkpoint_header = NULL;
    h->dirty=0;
    h->panic = 0;
    h->panic_string = 0;
    toku_list_init(&h->live_brts);
    toku_list_init(&h->zombie_brts);
    toku_list_init(&h->checkpoint_before_commit_link);

    //version MUST be in network order on disk regardless of disk order
    h->layout_version = rbuf_network_int(&rc);
    //TODO: #1924
    invariant(h->layout_version >= BRT_LAYOUT_MIN_SUPPORTED_VERSION);
    invariant(h->layout_version <= BRT_LAYOUT_VERSION);
    h->layout_version_read_from_disk = h->layout_version;

    //build_id MUST be in network order on disk regardless of disk order
    h->build_id = rbuf_network_int(&rc);

    //Size MUST be in network order regardless of disk order.
    u_int32_t size = rbuf_network_int(&rc);
    lazy_assert(size==rc.size);

    bytevec tmp_byte_order_check;
    rbuf_literal_bytes(&rc, &tmp_byte_order_check, 8); //Must not translate byte order
    int64_t byte_order_stored = *(int64_t*)tmp_byte_order_check;
    lazy_assert(byte_order_stored == toku_byte_order_host);

    h->checkpoint_count = rbuf_ulonglong(&rc);
    h->checkpoint_lsn   = rbuf_lsn(&rc);
    h->nodesize      = rbuf_int(&rc);
    DISKOFF translation_address_on_disk = rbuf_diskoff(&rc);
    DISKOFF translation_size_on_disk    = rbuf_diskoff(&rc);
    lazy_assert(translation_address_on_disk>0);
    lazy_assert(translation_size_on_disk>0);

    // printf("%s:%d translated_blocknum_limit=%ld, block_translation_address_on_disk=%ld\n", __FILE__, __LINE__, h->translated_blocknum_limit, h->block_translation_address_on_disk);
    //Load translation table
    {
        lock_for_pwrite();
        unsigned char *XMALLOC_N(translation_size_on_disk, tbuf);
        {
            // This cast is messed up in 32-bits if the block translation table is ever more than 4GB.  But in that case, the translation table itself won't fit in main memory.
            ssize_t r = toku_os_pread(fd, tbuf, translation_size_on_disk, translation_address_on_disk);
            lazy_assert(r==translation_size_on_disk);
        }
        unlock_for_pwrite();
        // Create table and read in data.
        toku_blocktable_create_from_buffer(&h->blocktable,
                                           translation_address_on_disk,
                                           translation_size_on_disk,
                                           tbuf);
        toku_free(tbuf);
    }

    h->root = rbuf_blocknum(&rc);
    h->root_hash.valid = FALSE;
    h->flags = rbuf_int(&rc);
    h->layout_version_original = rbuf_int(&rc);    
    h->build_id_original = rbuf_int(&rc);
    h->time_of_creation  = rbuf_ulonglong(&rc);
    h->time_of_last_modification = rbuf_ulonglong(&rc);
    h->num_blocks_to_upgrade   = rbuf_ulonglong(&rc);

    if (h->layout_version >= BRT_LAYOUT_VERSION_14) { 
        // at this layer, this new field is the only difference between versions 13 and 14
        rbuf_TXNID(&rc, &h->root_xid_that_created);
    }
    if (h->layout_version >= BRT_LAYOUT_VERSION_15) {
        h->basementnodesize = rbuf_int(&rc);
    }
    (void)rbuf_int(&rc); //Read in checksum and ignore (already verified).
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died1;}
    toku_free(rc.buf);
    rc.buf = NULL;
    *brth = h;
    return 0;
}



static int 
write_descriptor_to_disk_unlocked(struct brt_header * h, DESCRIPTOR d, int fd) {
    int r = 0;
    DISKOFF offset;
    //4 for checksum
    toku_realloc_descriptor_on_disk_unlocked(h->blocktable, toku_serialize_descriptor_size(d)+4, &offset, h);
    r = toku_serialize_descriptor_contents_to_fd(fd, d, offset);
    return r;
}


//TODO: When version 15 exists, add case for version 14 that looks like today's version 13 case,
static int
deserialize_brtheader_versioned (int fd, struct rbuf *rb, struct brt_header **brth, u_int32_t version) {
    int rval;
    int upgrade = 0;

    struct brt_header *h = NULL;
    rval = deserialize_brtheader (fd, rb, &h); //deserialize from rbuf and fd into header
    invariant ((uint32_t) h->layout_version == version);
    deserialize_descriptor_from(fd, h->blocktable, &(h->descriptor), version);
    if (rval == 0) {
        invariant(h);
        switch (version) {
            case BRT_LAYOUT_VERSION_13:
                invariant(h->layout_version == BRT_LAYOUT_VERSION_13);
                {
                    //Upgrade root_xid_that_created
                    //Fake creation during the last checkpoint.
                    h->root_xid_that_created = h->checkpoint_lsn.lsn;
                }
                {
                    //Deprecate 'TOKU_DB_VALCMP_BUILTIN'.  Just remove the flag
                    h->flags &= ~TOKU_DB_VALCMP_BUILTIN_13;
                }
                h->layout_version++;
		toku_sync_fetch_and_increment_uint64(&upgrade_status.header_13);  // how many header nodes upgraded from v13
                upgrade++;
                //Fall through on purpose
            case BRT_LAYOUT_VERSION_14:
                h->basementnodesize = 128*1024;  // basement nodes added in v15
                //fall through on purpose
            case BRT_LAYOUT_VERSION_15:
                invariant(h->layout_version == BRT_LAYOUT_VERSION);
                h->upgrade_brt_performed = FALSE;
                if (upgrade) {
                    toku_brtheader_lock(h);
                    h->num_blocks_to_upgrade = toku_block_get_blocks_in_use_unlocked(h->blocktable); //Total number of blocks
		    if (version == BRT_LAYOUT_VERSION_13) {
			// write upgraded descriptor to disk if descriptor upgraded from version 13
			rval = write_descriptor_to_disk_unlocked(h, &(h->descriptor), fd);
		    }
                    h->dirty = 1;
                    toku_brtheader_unlock(h);
                }
                *brth = h;
                break;    // this is the only break
            default:
                invariant(FALSE);
        }
    }
    return rval;
}



// Simply reading the raw bytes of the header into an rbuf is insensitive to disk format version.  
// If that ever changes, then modify this.
//TOKUDB_DICTIONARY_NO_HEADER means we can overwrite everything in the file AND the header is useless
static int
deserialize_brtheader_from_fd_into_rbuf(int fd, toku_off_t offset_of_header, struct rbuf *rb, 
					u_int64_t *checkpoint_count, LSN *checkpoint_lsn, u_int32_t * version_p) {
    int r = 0;
    const int64_t prefix_size = 8 + // magic ("tokudata")
                                4 + // version
                                4 + // build_id
                                4;  // size
    unsigned char prefix[prefix_size];
    rb->buf = NULL;
    int64_t n = toku_os_pread(fd, prefix, prefix_size, offset_of_header);
    if (n==0) r = TOKUDB_DICTIONARY_NO_HEADER;
    else if (n<0) {r = errno; lazy_assert(r!=0);}
    else if (n!=prefix_size) r = EINVAL;
    else {
        rb->size  = prefix_size;
        rb->ndone = 0;
        rb->buf   = prefix;
        {
            //Check magic number
            bytevec magic;
            rbuf_literal_bytes(rb, &magic, 8);
            if (memcmp(magic,"tokudata",8)!=0) {
                if ((*(u_int64_t*)magic) == 0) r = TOKUDB_DICTIONARY_NO_HEADER;
                else r = EINVAL; //Not a tokudb file! Do not use.
            }
        }
        u_int32_t version = 0;
        u_int32_t build_id = 0;
        if (r==0) {
            //Version MUST be in network order regardless of disk order.
            version = rbuf_network_int(rb);
	    *version_p = version;
            if (version < BRT_LAYOUT_MIN_SUPPORTED_VERSION) r = TOKUDB_DICTIONARY_TOO_OLD; //Cannot use
            if (version > BRT_LAYOUT_VERSION) r = TOKUDB_DICTIONARY_TOO_NEW; //Cannot use
            //build_id MUST be in network order regardless of disk order.
            build_id = rbuf_network_int(rb);
        }
        u_int32_t size;
        if (r==0) {
            const int64_t max_header_size = BLOCK_ALLOCATOR_HEADER_RESERVE;
            int64_t min_header_size = serialize_brt_header_min_size(version);
            //Size MUST be in network order regardless of disk order.
            size = rbuf_network_int(rb);
            //If too big, it is corrupt.  We would probably notice during checksum
            //but may have to do a multi-gigabyte malloc+read to find out.
            //If its too small reading rbuf would crash, so verify.
            if (size > max_header_size || size < min_header_size) r = TOKUDB_DICTIONARY_NO_HEADER;
        }
        if (r!=0) {
            rb->buf = NULL; //Prevent freeing of 'prefix'
        }
        if (r==0) {
            lazy_assert(rb->ndone==prefix_size);
            rb->size = size;
            rb->buf  = toku_xmalloc(rb->size);
        }
        if (r==0) {
            n = toku_os_pread(fd, rb->buf, rb->size, offset_of_header);
            if (n==-1) {
                r = errno;
                lazy_assert(r!=0);
            }
            else if (n!=(int64_t)rb->size) r = EINVAL; //Header might be useless (wrong size) or could be a disk read error.
        }
        //It's version 10 or later.  Magic looks OK.
        //We have an rbuf that represents the header.
        //Size is within acceptable bounds.
        if (r==0) {
            //Verify checksum (BRT_LAYOUT_VERSION_13 or later, when checksum function changed)
            u_int32_t calculated_x1764 = x1764_memory(rb->buf, rb->size-4);
            u_int32_t stored_x1764     = toku_dtoh32(*(int*)(rb->buf+rb->size-4));
            if (calculated_x1764!=stored_x1764) r = TOKUDB_DICTIONARY_NO_HEADER; //Header useless
        }
        if (r==0) {
            //Verify byte order
            bytevec tmp_byte_order_check;
            rbuf_literal_bytes(rb, &tmp_byte_order_check, 8); //Must not translate byte order
            int64_t byte_order_stored = *(int64_t*)tmp_byte_order_check;
            if (byte_order_stored != toku_byte_order_host) r = TOKUDB_DICTIONARY_NO_HEADER; //Cannot use dictionary
        }
        if (r==0) {
            //Load checkpoint count
            *checkpoint_count = rbuf_ulonglong(rb);
	    *checkpoint_lsn   = rbuf_lsn(rb);
            //Restart at beginning during regular deserialization
            rb->ndone = 0;
        }
    }
    if (r!=0 && rb->buf) {
        toku_free(rb->buf);
        rb->buf = NULL;
    }
    return r;
}


// Read brtheader from file into struct.  Read both headers and use one.
// We want the latest acceptable header whose checkpoint_lsn is no later
// than max_acceptable_lsn.
int 
toku_deserialize_brtheader_from (int fd, LSN max_acceptable_lsn, struct brt_header **brth) {
    struct rbuf rb_0;
    struct rbuf rb_1;
    u_int64_t checkpoint_count_0;
    u_int64_t checkpoint_count_1;
    LSN checkpoint_lsn_0;
    LSN checkpoint_lsn_1;
    u_int32_t version_0, version_1, version = 0;
    BOOL h0_acceptable = FALSE;
    BOOL h1_acceptable = FALSE;
    struct rbuf *rb = NULL;
    int r0, r1, r;

    {
        toku_off_t header_0_off = 0;
        r0 = deserialize_brtheader_from_fd_into_rbuf(fd, header_0_off, &rb_0, &checkpoint_count_0, &checkpoint_lsn_0, &version_0);
	if ( (r0==0) && (checkpoint_lsn_0.lsn <= max_acceptable_lsn.lsn) )
	    h0_acceptable = TRUE;
    }
    {
        toku_off_t header_1_off = BLOCK_ALLOCATOR_HEADER_RESERVE;
        r1 = deserialize_brtheader_from_fd_into_rbuf(fd, header_1_off, &rb_1, &checkpoint_count_1, &checkpoint_lsn_1, &version_1);
	if ( (r1==0) && (checkpoint_lsn_1.lsn <= max_acceptable_lsn.lsn) )
	    h1_acceptable = TRUE;
    }

    // if either header is too new, the dictionary is unreadable
    if (r0!=TOKUDB_DICTIONARY_TOO_NEW && r1!=TOKUDB_DICTIONARY_TOO_NEW) {
	if (h0_acceptable && h1_acceptable) {
	    if (checkpoint_count_0 > checkpoint_count_1) {
		invariant(checkpoint_count_0 == checkpoint_count_1 + 1);
		invariant(version_0 >= version_1);
		rb = &rb_0;
		version = version_0;
		r = 0;
	    }
	    else {
		invariant(checkpoint_count_1 == checkpoint_count_0 + 1);
		invariant(version_1 >= version_0);
		rb = &rb_1;
		version = version_1;
		r = 0;
	    }
	}
	else if (h0_acceptable) {
	    rb = &rb_0;
	    version = version_0;
	    r = 0;
	}
	else if (h1_acceptable) {
	    rb = &rb_1;
	    version = version_1;
	    r = 0;
	}
    }

    if (rb==NULL) {
        // We were unable to read either header or at least one is too new.
        // Certain errors are higher priority than others. Order of these if/else if is important.
        if (r0==TOKUDB_DICTIONARY_TOO_NEW || r1==TOKUDB_DICTIONARY_TOO_NEW)
            r = TOKUDB_DICTIONARY_TOO_NEW;
        else if (r0==TOKUDB_DICTIONARY_TOO_OLD || r1==TOKUDB_DICTIONARY_TOO_OLD) {
            r = TOKUDB_DICTIONARY_TOO_OLD;
        }
        else if (r0==TOKUDB_DICTIONARY_NO_HEADER || r1==TOKUDB_DICTIONARY_NO_HEADER) {
            r = TOKUDB_DICTIONARY_NO_HEADER;
        }
        else r = r0 ? r0 : r1; //Arbitrarily report the error from the first header, unless it's readable

	// it should not be possible for both headers to be later than the max_acceptable_lsn
	invariant(!( (r0==0 && checkpoint_lsn_0.lsn > max_acceptable_lsn.lsn) &&
		     (r1==0 && checkpoint_lsn_1.lsn > max_acceptable_lsn.lsn) ));
        invariant(r!=0);
    }

    if (r==0) r = deserialize_brtheader_versioned(fd, rb, brth, version);
    if (rb_0.buf) toku_free(rb_0.buf);
    if (rb_1.buf) toku_free(rb_1.buf);
    return r;
}

unsigned int 
toku_brt_pivot_key_len (struct kv_pair *pk) {
    return kv_pair_keylen(pk);
}

int 
toku_db_badformat(void) {
    return DB_BADFORMAT;
}

static size_t
serialize_rollback_log_size(ROLLBACK_LOG_NODE log) {
    size_t size = node_header_overhead //8 "tokuroll", 4 version, 4 version_original, 4 build_id
                 +8 //TXNID
                 +8 //sequence
                 +8 //thislogname
                 +8 //older (blocknum)
                 +8 //resident_bytecount
                 +8 //memarena_size_needed_to_load
                 +log->rollentry_resident_bytecount;
    return size;
}

static void
serialize_rollback_log_node_to_buf(ROLLBACK_LOG_NODE log, char *buf, size_t calculated_size, int UU(n_sub_blocks), struct sub_block UU(sub_block[])) {
    struct wbuf wb;
    wbuf_init(&wb, buf, calculated_size);
    {   //Serialize rollback log to local wbuf
        wbuf_nocrc_literal_bytes(&wb, "tokuroll", 8);
        lazy_assert(log->layout_version == BRT_LAYOUT_VERSION);
        wbuf_nocrc_int(&wb, log->layout_version);
        wbuf_nocrc_int(&wb, log->layout_version_original);
        wbuf_nocrc_uint(&wb, BUILD_ID);
        wbuf_nocrc_TXNID(&wb, log->txnid);
        wbuf_nocrc_ulonglong(&wb, log->sequence);
        wbuf_nocrc_BLOCKNUM(&wb, log->thislogname);
        wbuf_nocrc_BLOCKNUM(&wb, log->older);
        wbuf_nocrc_ulonglong(&wb, log->rollentry_resident_bytecount);
        //Write down memarena size needed to restore
        wbuf_nocrc_ulonglong(&wb, memarena_total_size_in_use(log->rollentry_arena));

        {
            //Store rollback logs
            struct roll_entry *item;
            size_t done_before = wb.ndone;
            for (item = log->newest_logentry; item; item = item->prev) {
                toku_logger_rollback_wbuf_nocrc_write(&wb, item);
            }
            lazy_assert(done_before + log->rollentry_resident_bytecount == wb.ndone);
        }
    }
    lazy_assert(wb.ndone == wb.size);
    lazy_assert(calculated_size==wb.ndone);
}

static int
serialize_uncompressed_block_to_memory(char * uncompressed_buf,
                                       int n_sub_blocks,
                                       struct sub_block sub_block[/*n_sub_blocks*/],
                               /*out*/ size_t *n_bytes_to_write,
                               /*out*/ char  **bytes_to_write) {
    // allocate space for the compressed uncompressed_buf
    size_t compressed_len = get_sum_compressed_size_bound(n_sub_blocks, sub_block);
    size_t sub_block_header_len = sub_block_header_size(n_sub_blocks);
    size_t header_len = node_header_overhead + sub_block_header_len + sizeof (uint32_t); // node + sub_block + checksum
    char *XMALLOC_N(header_len + compressed_len, compressed_buf);
    if (compressed_buf == NULL)
        return errno;

    // copy the header
    memcpy(compressed_buf, uncompressed_buf, node_header_overhead);
    if (0) printf("First 4 bytes before compressing data are %02x%02x%02x%02x\n",
                  uncompressed_buf[node_header_overhead],   uncompressed_buf[node_header_overhead+1],
                  uncompressed_buf[node_header_overhead+2], uncompressed_buf[node_header_overhead+3]);

    // compress all of the sub blocks
    char *uncompressed_ptr = uncompressed_buf + node_header_overhead;
    char *compressed_ptr = compressed_buf + header_len;
    compressed_len = compress_all_sub_blocks(n_sub_blocks, sub_block, uncompressed_ptr, compressed_ptr, num_cores, brt_pool);

    //if (0) printf("Block %" PRId64 " Size before compressing %u, after compression %"PRIu64"\n", blocknum.b, calculated_size-node_header_overhead, (uint64_t) compressed_len);

    // serialize the sub block header
    uint32_t *ptr = (uint32_t *)(compressed_buf + node_header_overhead);
    *ptr++ = toku_htod32(n_sub_blocks);
    for (int i=0; i<n_sub_blocks; i++) {
        ptr[0] = toku_htod32(sub_block[i].compressed_size);
        ptr[1] = toku_htod32(sub_block[i].uncompressed_size);
        ptr[2] = toku_htod32(sub_block[i].xsum);
        ptr += 3;
    }

    // compute the header checksum and serialize it
    uint32_t header_length = (char *)ptr - (char *)compressed_buf;
    uint32_t xsum = x1764_memory(compressed_buf, header_length);
    *ptr = toku_htod32(xsum);

    *n_bytes_to_write = header_len + compressed_len;
    *bytes_to_write   = compressed_buf;

    return 0;
}



static int
toku_serialize_rollback_log_to_memory (ROLLBACK_LOG_NODE log,
                                       int UU(n_workitems), int UU(n_threads),
                               /*out*/ size_t *n_bytes_to_write,
                               /*out*/ char  **bytes_to_write) {
    // get the size of the serialized node
    size_t calculated_size = serialize_rollback_log_size(log);

    // choose sub block parameters
    int n_sub_blocks = 0, sub_block_size = 0;
    size_t data_size = calculated_size - node_header_overhead;
    choose_sub_block_size(data_size, max_sub_blocks, &sub_block_size, &n_sub_blocks);
    lazy_assert(0 < n_sub_blocks && n_sub_blocks <= max_sub_blocks);
    lazy_assert(sub_block_size > 0);

    // set the initial sub block size for all of the sub blocks
    struct sub_block sub_block[n_sub_blocks];
    for (int i = 0; i < n_sub_blocks; i++) 
        sub_block_init(&sub_block[i]);
    set_all_sub_block_sizes(data_size, sub_block_size, n_sub_blocks, sub_block);

    // allocate space for the serialized node
    char *XMALLOC_N(calculated_size, buf);
    // serialize the node into buf
    serialize_rollback_log_node_to_buf(log, buf, calculated_size, n_sub_blocks, sub_block);

    //Compress and malloc buffer to write
    int result = serialize_uncompressed_block_to_memory(buf, n_sub_blocks, sub_block,
                                                        n_bytes_to_write, bytes_to_write);
    toku_free(buf);
    return result;
}

int
toku_serialize_rollback_log_to (int fd, BLOCKNUM blocknum, ROLLBACK_LOG_NODE log,
                                struct brt_header *h, int n_workitems, int n_threads,
                                BOOL for_checkpoint) {
    size_t n_to_write;
    char *compressed_buf;
    {
        int r = toku_serialize_rollback_log_to_memory(log, n_workitems, n_threads, &n_to_write, &compressed_buf);
	if (r!=0) return r;
    }

    {
	lazy_assert(blocknum.b>=0);
	DISKOFF offset;
        toku_blocknum_realloc_on_disk(h->blocktable, blocknum, n_to_write, &offset,
                                      h, for_checkpoint); //dirties h
	lock_for_pwrite();
	toku_full_pwrite_extend(fd, compressed_buf, n_to_write, offset);
	unlock_for_pwrite();
    }
    toku_free(compressed_buf);
    log->dirty = 0;  // See #1957.   Must set the node to be clean after serializing it so that it doesn't get written again on the next checkpoint or eviction.
    return 0;
}

static int
deserialize_rollback_log_from_rbuf (BLOCKNUM blocknum, u_int32_t fullhash, ROLLBACK_LOG_NODE *log_p,
                                    struct brt_header *h, struct rbuf *rb) {
    ROLLBACK_LOG_NODE MALLOC(result);
    int r;
    if (result==NULL) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }

    //printf("Deserializing %lld datasize=%d\n", off, datasize);
    bytevec magic;
    rbuf_literal_bytes(rb, &magic, 8);
    lazy_assert(!memcmp(magic, "tokuroll", 8));

    result->layout_version    = rbuf_int(rb);
    lazy_assert(result->layout_version == BRT_LAYOUT_VERSION);
    result->layout_version_original = rbuf_int(rb);
    result->layout_version_read_from_disk = result->layout_version;
    result->build_id = rbuf_int(rb);
    result->dirty = FALSE;
    //TODO: Maybe add descriptor (or just descriptor version) here eventually?
    //TODO: This is hard.. everything is shared in a single dictionary.
    rbuf_TXNID(rb, &result->txnid);
    result->sequence = rbuf_ulonglong(rb);
    result->thislogname = rbuf_blocknum(rb);
    if (result->thislogname.b != blocknum.b) {
        r = toku_db_badformat();
        goto died0;
    }
    result->thishash    = toku_cachetable_hash(h->cf, result->thislogname);
    if (result->thishash != fullhash) {
        r = toku_db_badformat();
        goto died0;
    }
    result->older       = rbuf_blocknum(rb);
    result->older_hash  = toku_cachetable_hash(h->cf, result->older);
    result->rollentry_resident_bytecount = rbuf_ulonglong(rb);

    size_t arena_initial_size = rbuf_ulonglong(rb);
    result->rollentry_arena = memarena_create_presized(arena_initial_size);
    if (0) { died1: memarena_close(&result->rollentry_arena); goto died0; }

    //Load rollback entries
    lazy_assert(rb->size > 4);
    //Start with empty list
    result->oldest_logentry = result->newest_logentry = NULL;
    while (rb->ndone < rb->size) {
        struct roll_entry *item;
        uint32_t rollback_fsize = rbuf_int(rb); //Already read 4.  Rest is 4 smaller
        bytevec item_vec;
        rbuf_literal_bytes(rb, &item_vec, rollback_fsize-4);
        unsigned char* item_buf = (unsigned char*)item_vec;
        r = toku_parse_rollback(item_buf, rollback_fsize-4, &item, result->rollentry_arena);
        if (r!=0) {
            r = toku_db_badformat();
            goto died1;
        }
        //Add to head of list
        if (result->oldest_logentry) {
            result->oldest_logentry->prev = item;
            result->oldest_logentry       = item;
            item->prev = NULL;
        }
        else {
            result->oldest_logentry = result->newest_logentry = item;
            item->prev = NULL;
        }
    }

    toku_free(rb->buf);
    rb->buf = NULL;
    *log_p = result;
    return 0;
}

static int
deserialize_rollback_log_from_rbuf_versioned (u_int32_t version, BLOCKNUM blocknum, u_int32_t fullhash,
                                              ROLLBACK_LOG_NODE *log,
                                              struct brt_header *h, struct rbuf *rb) {
    int r = 0;
    ROLLBACK_LOG_NODE rollback_log_node = NULL;
    invariant(version==BRT_LAYOUT_VERSION); //Rollback log nodes do not survive version changes.
    r = deserialize_rollback_log_from_rbuf(blocknum, fullhash, &rollback_log_node, h, rb);
    if (r==0) {
        *log = rollback_log_node;
    }
    return r;
}

static int
decompress_from_raw_block_into_rbuf(u_int8_t *raw_block, size_t raw_block_size, struct rbuf *rb, BLOCKNUM blocknum) {
    toku_trace("decompress");
    // get the number of compressed sub blocks
    int n_sub_blocks;
    n_sub_blocks = toku_dtoh32(*(u_int32_t*)(&raw_block[node_header_overhead]));

    // verify the number of sub blocks
    invariant(0 <= n_sub_blocks && n_sub_blocks <= max_sub_blocks);

    { // verify the header checksum
        u_int32_t header_length = node_header_overhead + sub_block_header_size(n_sub_blocks);
        invariant(header_length <= raw_block_size);
        u_int32_t xsum = x1764_memory(raw_block, header_length);
        u_int32_t stored_xsum = toku_dtoh32(*(u_int32_t *)(raw_block + header_length));
        invariant(xsum == stored_xsum);
    }
    int r;

    // deserialize the sub block header
    struct sub_block sub_block[n_sub_blocks];
    u_int32_t *sub_block_header = (u_int32_t *) &raw_block[node_header_overhead+4];
    for (int i = 0; i < n_sub_blocks; i++) {
        sub_block_init(&sub_block[i]);
        sub_block[i].compressed_size = toku_dtoh32(sub_block_header[0]);
        sub_block[i].uncompressed_size = toku_dtoh32(sub_block_header[1]);
        sub_block[i].xsum = toku_dtoh32(sub_block_header[2]);
        sub_block_header += 3;
    }

    // verify sub block sizes
    for (int i = 0; i < n_sub_blocks; i++) {
        u_int32_t compressed_size = sub_block[i].compressed_size;
        if (compressed_size<=0   || compressed_size>(1<<30)) { r = toku_db_badformat(); return r; }

        u_int32_t uncompressed_size = sub_block[i].uncompressed_size;
        if (0) printf("Block %" PRId64 " Compressed size = %u, uncompressed size=%u\n", blocknum.b, compressed_size, uncompressed_size);
        if (uncompressed_size<=0 || uncompressed_size>(1<<30)) { r = toku_db_badformat(); return r; }
    }

    // sum up the uncompressed size of the sub blocks
    size_t uncompressed_size = get_sum_uncompressed_size(n_sub_blocks, sub_block);

    // allocate the uncompressed buffer
    size_t size = node_header_overhead + uncompressed_size;
    unsigned char *buf = toku_xmalloc(size);
    lazy_assert(buf);
    rbuf_init(rb, buf, size);

    // copy the uncompressed node header to the uncompressed buffer
    memcpy(rb->buf, raw_block, node_header_overhead);

    // point at the start of the compressed data (past the node header, the sub block header, and the header checksum)
    unsigned char *compressed_data = raw_block + node_header_overhead + sub_block_header_size(n_sub_blocks) + sizeof (u_int32_t);

    // point at the start of the uncompressed data
    unsigned char *uncompressed_data = rb->buf + node_header_overhead;    

    // decompress all the compressed sub blocks into the uncompressed buffer
    r = decompress_all_sub_blocks(n_sub_blocks, sub_block, compressed_data, uncompressed_data, num_cores, brt_pool);
    if (r != 0) {
        fprintf(stderr, "%s:%d block %"PRId64" failed %d at %p size %lu\n", __FUNCTION__, __LINE__, blocknum.b, r, raw_block, raw_block_size);
        dump_bad_block(raw_block, raw_block_size);
    }
    lazy_assert_zero(r);

    toku_trace("decompress done");

    rb->ndone=0;

    return 0;
}

static int
decompress_from_raw_block_into_rbuf_versioned(u_int32_t version, u_int8_t *raw_block, size_t raw_block_size, struct rbuf *rb, BLOCKNUM blocknum) {
    // This function exists solely to accomodate future changes in compression.
    int r;

    switch (version) {
        case BRT_LAYOUT_VERSION_13:
        case BRT_LAYOUT_VERSION_14:
        case BRT_LAYOUT_VERSION:
            r = decompress_from_raw_block_into_rbuf(raw_block, raw_block_size, rb, blocknum);
            break;
        default:
            lazy_assert(FALSE);
    }
    return r;
}

static int
read_and_decompress_block_from_fd_into_rbuf(int fd, BLOCKNUM blocknum,
                                            struct brt_header *h,
                                            struct rbuf *rb,
                                  /* out */ int *layout_version_p) {
    int r;
    if (0) printf("Deserializing Block %" PRId64 "\n", blocknum.b);
    if (h->panic) return h->panic;

    toku_trace("deserial start nopanic");

    // get the file offset and block size for the block
    DISKOFF offset, size;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &offset, &size);
    u_int8_t *XMALLOC_N(size, raw_block);
    {
        // read the (partially compressed) block
        ssize_t rlen = toku_os_pread(fd, raw_block, size, offset);
        lazy_assert((DISKOFF)rlen == size);
    }
    // get the layout_version
    int layout_version;
    {
        u_int8_t *magic = raw_block + uncompressed_magic_offset;
        if (memcmp(magic, "tokuleaf", 8)!=0 &&
            memcmp(magic, "tokunode", 8)!=0 &&
            memcmp(magic, "tokuroll", 8)!=0) {
            r = toku_db_badformat();
            goto cleanup;
        }
        u_int8_t *version = raw_block + uncompressed_version_offset;
        layout_version = toku_dtoh32(*(uint32_t*)version);
        if (layout_version < BRT_LAYOUT_MIN_SUPPORTED_VERSION || layout_version > BRT_LAYOUT_VERSION) {
            r = toku_db_badformat();
            goto cleanup;
        }
    }

    r = decompress_from_raw_block_into_rbuf_versioned(layout_version, raw_block, size, rb, blocknum);
    if (r!=0) goto cleanup;

    *layout_version_p = layout_version;
cleanup:
    if (r!=0) {
        if (rb->buf) toku_free(rb->buf);
        rb->buf = NULL;
    }
    if (raw_block) toku_free(raw_block);
    return r;
}

// Read rollback log node from file into struct.  Perform version upgrade if necessary.
int
toku_deserialize_rollback_log_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash,
                                    ROLLBACK_LOG_NODE *logp, struct brt_header *h) {
    toku_trace("deserial start");

    int r;
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};

    int layout_version = 0;
    r = read_and_decompress_block_from_fd_into_rbuf(fd, blocknum, h, &rb, &layout_version);
    if (r!=0) goto cleanup;

    {
        u_int8_t *magic = rb.buf + uncompressed_magic_offset;
        if (memcmp(magic, "tokuroll", 8)!=0) {
            r = toku_db_badformat();
            goto cleanup;
        }
    }

    r = deserialize_rollback_log_from_rbuf_versioned(layout_version, blocknum, fullhash, logp, h, &rb);

    toku_trace("deserial done");

cleanup:
    if (rb.buf) toku_free(rb.buf);
    return r;
}



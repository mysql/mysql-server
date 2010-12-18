/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "toku_atomic.h"
#include "threadpool.h"

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

// #define USE_PWRITE_LOCK

#ifdef USE_PWRITE_LOCK
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
#else
static const int pwrite_is_locked = 1;
int 
toku_pwrite_lock_init(void) {
    return 0;
}
int 
toku_pwrite_lock_destroy(void) {
    return 0;
}
static inline void
lock_for_pwrite (void) {
}
static inline void
unlock_for_pwrite (void) {
}
#endif

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
            fprintf(stderr, "%s:%d fd=%d size=%"PRIu64"r=%d errno=%d\n", __FUNCTION__, __LINE__, fd, size, r, the_errno); fflush(stderr);
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
                            4),  // layout_version_original

    extended_node_header_overhead = (4+   // nodesize
                                     4+   // flags
                                     4+   // height
                                     4+   // random for fingerprint
                                     4),  // localfingerprint
};

#include "sub_block.h"
#include "sub_block_map.h"

static int
addupsize (OMTVALUE lev, u_int32_t UU(idx), void *vp) {
    LEAFENTRY le=lev;
    unsigned int *ip=vp;
    (*ip) += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
    return 0;
}

static unsigned int 
toku_serialize_brtnode_size_slow (BRTNODE node) {
    unsigned int size = node_header_overhead + extended_node_header_overhead;
    if (node->height > 0) {
	unsigned int hsize=0;
	unsigned int csize=0;
	size += 4; /* n_children */
	size += 4; /* subtree fingerprint. */
	size += 4*(node->u.n.n_children-1); /* key lengths*/
	for (int i=0; i<node->u.n.n_children-1; i++) {
	    csize += toku_brt_pivot_key_len(node->u.n.childkeys[i]);
	}
	size += (8+4+4+1+3*8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and 3*8 for the subtree estimates and 1 for the exact bit for the estimates. */
	int n_buffers = node->u.n.n_children;
        invariant(0 <= n_buffers && n_buffers < TREE_FANOUT+1);
	for (int i=0; i< n_buffers; i++) {
	    FIFO_ITERATE(BNC_BUFFER(node,i),
			 key, keylen,
			 data __attribute__((__unused__)), datalen,
			 type __attribute__((__unused__)), xids,
			 (hsize+=BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+keylen+datalen+
                          xids_get_serialize_size(xids)));
	}
	invariant(hsize==node->u.n.n_bytes_in_buffers);
	invariant(csize==node->u.n.totalchildkeylens);
        size += node->u.n.n_children*stored_sub_block_map_size;
	return size+hsize+csize;
    } else {
	unsigned int hsize=0;
	toku_omt_iterate(node->u.l.buffer, addupsize, &hsize);
	invariant(hsize==node->u.l.n_bytes_in_buffer);
	hsize += 4;                                   // add n entries in buffer table
	hsize += 3*8;                                 // add the three leaf stats, but no exact bit
        hsize += 4;                                   // optimized_for_upgrade
        size += 4 + 1*stored_sub_block_map_size;      // one partition
	return size+hsize;
    }
}

// This is the size of the uncompressed data, not including the compression headers
unsigned int 
toku_serialize_brtnode_size (BRTNODE node) {
    unsigned int result = node_header_overhead + extended_node_header_overhead;
    invariant(sizeof(toku_off_t)==8);
    if (node->height > 0) {
	result += 4; /* subtree fingerpirnt */
	result += 4; /* n_children */
	result += 4*(node->u.n.n_children-1); /* key lengths*/
	invariant(node->u.n.totalchildkeylens < (1<<30));
	result += node->u.n.totalchildkeylens; /* the lengths of the pivot keys, without their key lengths. */
	result += (8+4+4+1+3*8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and 3*8 for the subtree estimates and one for the exact bit. */
	result += node->u.n.n_bytes_in_buffers;
        result += node->u.n.n_children*stored_sub_block_map_size;
    } else {
	result += 4;                                  // n_entries in buffer table
	result += 3*8;                                // the three leaf stats
        result += 4;                                   // optimized_for_upgrade
	result += node->u.l.n_bytes_in_buffer;
        result += 4 + 1*stored_sub_block_map_size;    // one partition
    }
    if (toku_memory_check) {
        unsigned int slowresult = toku_serialize_brtnode_size_slow(node);
        if (result!=slowresult) printf("%s:%d result=%u slowresult=%u\n", __FILE__, __LINE__, result, slowresult);
        invariant(result==slowresult);
    }
    return result;
}

// uncompressed header offsets
enum {
    uncompressed_magic_offset = 0,
    uncompressed_version_offset = 8,
};

static void
serialize_node_header(BRTNODE node, struct wbuf *wbuf) {
    if (node->height == 0) 
        wbuf_nocrc_literal_bytes(wbuf, "tokuleaf", 8);
    else 
        wbuf_nocrc_literal_bytes(wbuf, "tokunode", 8);
    invariant(node->layout_version == BRT_LAYOUT_VERSION);
    wbuf_nocrc_int(wbuf, node->layout_version);
    wbuf_nocrc_int(wbuf, node->layout_version_original);

    //printf("%s:%d %lld.calculated_size=%d\n", __FILE__, __LINE__, off, calculated_size);
    wbuf_nocrc_uint(wbuf, node->nodesize);
    wbuf_nocrc_uint(wbuf, node->flags);
    wbuf_nocrc_int(wbuf,  node->height);
    //printf("%s:%d %lld rand=%08x sum=%08x height=%d\n", __FILE__, __LINE__, node->thisnodename, node->rand4fingerprint, node->subtree_fingerprint, node->height);
    wbuf_nocrc_uint(wbuf, node->rand4fingerprint);
    wbuf_nocrc_uint(wbuf, node->local_fingerprint);
    //printf("%s:%d wrote %08x for node %lld\n", __FILE__, __LINE__, node->local_fingerprint, (long long)node->thisnodename);
    //printf("%s:%d local_fingerprint=%8x\n", __FILE__, __LINE__, node->local_fingerprint);
    //printf("%s:%d w.ndone=%d n_children=%d\n", __FILE__, __LINE__, w.ndone, node->n_children);
}

static void
serialize_nonleaf(BRTNODE node, int n_sub_blocks, struct sub_block sub_block[], struct wbuf *wbuf) {
    // serialize the nonleaf header
    invariant(node->u.n.n_children>0);
    // Local fingerprint is not actually stored while in main memory.  Must calculate it.
    // Subtract the child fingerprints from the subtree fingerprint to get the local fingerprint.
    {
        u_int32_t subtree_fingerprint = node->local_fingerprint;
        for (int i = 0; i < node->u.n.n_children; i++) {
            subtree_fingerprint += BNC_SUBTREE_FINGERPRINT(node, i);
        }
        wbuf_nocrc_uint(wbuf, subtree_fingerprint);
    }
    wbuf_nocrc_int(wbuf, node->u.n.n_children);
    for (int i = 0; i < node->u.n.n_children; i++) {
        wbuf_nocrc_uint(wbuf, BNC_SUBTREE_FINGERPRINT(node, i));
        struct subtree_estimates *se = &(BNC_SUBTREE_ESTIMATES(node, i));
        wbuf_nocrc_ulonglong(wbuf, se->nkeys);
        wbuf_nocrc_ulonglong(wbuf, se->ndata);
        wbuf_nocrc_ulonglong(wbuf, se->dsize);
        wbuf_nocrc_char     (wbuf, (char)se->exact);
    }
    //printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
    for (int i = 0; i < node->u.n.n_children-1; i++) {
	wbuf_nocrc_bytes(wbuf, kv_pair_key(node->u.n.childkeys[i]), toku_brt_pivot_key_len(node->u.n.childkeys[i]));
        //printf("%s:%d w.ndone=%d (childkeylen[%d]=%d\n", __FILE__, __LINE__, w.ndone, i, node->childkeylens[i]);
    }
    for (int i = 0; i < node->u.n.n_children; i++) {
        wbuf_nocrc_BLOCKNUM(wbuf, BNC_BLOCKNUM(node,i));
        //printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
    }

    // map the child buffers
    struct sub_block_map child_buffer_map[node->u.n.n_children];
    size_t offset = wbuf_get_woffset(wbuf) - node_header_overhead + node->u.n.n_children * stored_sub_block_map_size;
    for (int i = 0; i < node->u.n.n_children; i++) {
        int idx = get_sub_block_index(n_sub_blocks, sub_block, offset);
        invariant(idx >= 0);
        size_t size = sizeof (u_int32_t) + BNC_NBYTESINBUF(node, i); // # elements + size of the elements
        sub_block_map_init(&child_buffer_map[i], idx, offset, size);
        offset += size;
    }

    // serialize the child buffer map
    for (int i = 0; i < node->u.n.n_children ; i++)
        sub_block_map_serialize(&child_buffer_map[i], wbuf);

    // serialize the child buffers
    {
        int n_buffers = node->u.n.n_children;
        u_int32_t check_local_fingerprint = 0;
        for (int i = 0; i < n_buffers; i++) {
            //printf("%s:%d p%d=%p n_entries=%d\n", __FILE__, __LINE__, i, node->mdicts[i], mdict_n_entries(node->mdicts[i]));
            // invariant(child_buffer_map[i].offset == wbuf_get_woffset(wbuf));
            wbuf_nocrc_int(wbuf, toku_fifo_n_entries(BNC_BUFFER(node,i)));
            FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xids,
                         {
                             invariant(type>=0 && type<256);
                             wbuf_nocrc_char(wbuf, (unsigned char)type);
                             wbuf_nocrc_xids(wbuf, xids);
                             wbuf_nocrc_bytes(wbuf, key, keylen);
                             wbuf_nocrc_bytes(wbuf, data, datalen);
                             check_local_fingerprint+=node->rand4fingerprint*toku_calc_fingerprint_cmd(type, xids, key, keylen, data, datalen);
                         });
        }
        //printf("%s:%d check_local_fingerprint=%8x\n", __FILE__, __LINE__, check_local_fingerprint);
        if (check_local_fingerprint!=node->local_fingerprint) printf("%s:%d node=%" PRId64 " fingerprint expected=%08x actual=%08x\n", __FILE__, __LINE__, node->thisnodename.b, check_local_fingerprint, node->local_fingerprint);
        invariant(check_local_fingerprint==node->local_fingerprint);
    }
}

static int
wbufwriteleafentry (OMTVALUE lev, u_int32_t UU(idx), void *v) {
    LEAFENTRY le=lev;
    struct wbuf *thisw=v;
    wbuf_nocrc_LEAFENTRY(thisw, le);
    return 0;
}

static void
serialize_leaf(BRTNODE node, int n_sub_blocks, struct sub_block sub_block[], struct wbuf *wbuf) {
    // serialize the leaf stats
    wbuf_nocrc_ulonglong(wbuf, node->u.l.leaf_stats.nkeys);
    wbuf_nocrc_ulonglong(wbuf, node->u.l.leaf_stats.ndata);
    wbuf_nocrc_ulonglong(wbuf, node->u.l.leaf_stats.dsize);

    wbuf_nocrc_int(wbuf, node->u.l.optimized_for_upgrade);

    // RFP partition the leaf elements. for now, 1 partition
    const int npartitions = 1;
    wbuf_nocrc_int(wbuf, npartitions);

    struct sub_block_map part_map[npartitions];
    for (int i = 0; i < npartitions; i++) {
        size_t offset = wbuf_get_woffset(wbuf) - node_header_overhead;
        int idx = get_sub_block_index(n_sub_blocks, sub_block, offset);
        invariant(idx >= 0);
        size_t size = sizeof (u_int32_t) +  node->u.l.n_bytes_in_buffer; // # in partition + size of partition
        sub_block_map_init(&part_map[i], idx, offset, size);
    }

    // RFP serialize the partition pivots
    for (int i = 0; i < npartitions-1; i++) {
        lazy_assert(0);
    }

    // RFP serialize the partition maps
    for (int i = 0; i < npartitions; i++) 
        sub_block_map_serialize(&part_map[i], wbuf);

    // serialize the leaf entries
    wbuf_nocrc_uint(wbuf, toku_omt_size(node->u.l.buffer));
    toku_omt_iterate(node->u.l.buffer, wbufwriteleafentry, wbuf);
}

static void
serialize_node(BRTNODE node, char *buf, size_t calculated_size, int n_sub_blocks, struct sub_block sub_block[]) {
    struct wbuf wb;
    wbuf_init(&wb, buf, calculated_size);

    serialize_node_header(node, &wb);

    if (node->height > 0)
        serialize_nonleaf(node, n_sub_blocks, sub_block, &wb);
    else
        serialize_leaf(node, n_sub_blocks, sub_block, &wb);

    invariant(wb.ndone == wb.size);
    invariant(calculated_size==wb.ndone);
}


static int
serialize_uncompressed_block_to_memory(char * uncompressed_buf,
                                       int n_sub_blocks,
                                       struct sub_block sub_block[n_sub_blocks],
                               /*out*/ size_t *n_bytes_to_write,
                               /*out*/ char  **bytes_to_write) {
    // allocate space for the compressed uncompressed_buf
    size_t compressed_len = get_sum_compressed_size_bound(n_sub_blocks, sub_block);
    size_t sub_block_header_len = sub_block_header_size(n_sub_blocks);
    size_t header_len = node_header_overhead + sub_block_header_len + sizeof (uint32_t); // node + sub_block + checksum
    char *MALLOC_N(header_len + compressed_len, compressed_buf);
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

int
toku_serialize_brtnode_to_memory (BRTNODE node, int UU(n_workitems), int UU(n_threads), /*out*/ size_t *n_bytes_to_write, /*out*/ char  **bytes_to_write) {
    int result = 0;

    // get the size of the serialized node
    size_t calculated_size = toku_serialize_brtnode_size(node); 

    // choose sub block parameters
    int n_sub_blocks = 0, sub_block_size = 0;
    size_t data_size = calculated_size - node_header_overhead;
    choose_sub_block_size(data_size, max_sub_blocks, &sub_block_size, &n_sub_blocks);
    invariant(0 < n_sub_blocks && n_sub_blocks <= max_sub_blocks);
    invariant(sub_block_size > 0);

    // set the initial sub block size for all of the sub blocks
    struct sub_block sub_block[n_sub_blocks];
    for (int i = 0; i < n_sub_blocks; i++) 
        sub_block_init(&sub_block[i]);
    set_all_sub_block_sizes(data_size, sub_block_size, n_sub_blocks, sub_block);

    // allocate space for the serialized node
    char *MALLOC_N(calculated_size, buf);
    if (buf == NULL)
        result = errno;
    else {
        //toku_verify_counts(node);
        //invariant(size>0);
        //printf("%s:%d serializing %lld w height=%d p0=%p\n", __FILE__, __LINE__, off, node->height, node->mdicts[0]);

        // serialize the node into buf
        serialize_node(node, buf, calculated_size, n_sub_blocks, sub_block);

        //Compress and malloc buffer to write
        result = serialize_uncompressed_block_to_memory(buf, n_sub_blocks, sub_block,
                                                        n_bytes_to_write, bytes_to_write);
        toku_free(buf);
    }
    return result;
}

int 
toku_serialize_brtnode_to (int fd, BLOCKNUM blocknum, BRTNODE node, struct brt_header *h, int n_workitems, int n_threads, BOOL for_checkpoint) {

    size_t n_to_write;
    char *compressed_buf;
    {
	int r = toku_serialize_brtnode_to_memory (node, n_workitems, n_threads,	&n_to_write, &compressed_buf);
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

static void deserialize_descriptor_from_rbuf(struct rbuf *rb, DESCRIPTOR desc, BOOL temporary);

#include "workset.h"

struct deserialize_child_buffer_work {
    struct work base;

    BRTNODE node;               // in node pointer
    int cnum;                   // in child number
    struct rbuf rb;             // in child rbuf

    uint32_t local_fingerprint; // out node fingerprint
};

static void
deserialize_child_buffer_init(struct deserialize_child_buffer_work *dw, BRTNODE node, int cnum, unsigned char *buf, size_t size) {
    dw->node = node;
    dw->cnum = cnum;
    rbuf_init(&dw->rb, buf, size);
}

static void
deserialize_child_buffer(BRTNODE node, int cnum, struct rbuf *rbuf, u_int32_t *local_fingerprint_ret) {
    uint32_t local_fingerprint = 0;
    int n_bytes_in_buffer = 0;
    int n_in_this_buffer = rbuf_int(rbuf);
    for (int i = 0; i < n_in_this_buffer; i++) {
        bytevec key; ITEMLEN keylen;
        bytevec val; ITEMLEN vallen;
        //toku_verify_counts(result);
        int type = rbuf_char(rbuf);
        XIDS xids;
        xids_create_from_buffer(rbuf, &xids);
        rbuf_bytes(rbuf, &key, &keylen); /* Returns a pointer into the rbuf. */
        rbuf_bytes(rbuf, &val, &vallen);
        local_fingerprint += node->rand4fingerprint * toku_calc_fingerprint_cmd(type, xids, key, keylen, val, vallen);
        //printf("Found %s,%s\n", (char*)key, (char*)val);
        int r = toku_fifo_enq(BNC_BUFFER(node, cnum), key, keylen, val, vallen, type, xids); /* Copies the data into the fifo */
        lazy_assert_zero(r);
        n_bytes_in_buffer += keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_get_serialize_size(xids);
        //printf("Inserted\n");
        xids_destroy(&xids);
    }
    invariant(rbuf->ndone == rbuf->size);

    BNC_NBYTESINBUF(node, cnum) = n_bytes_in_buffer;
    *local_fingerprint_ret = local_fingerprint;
}

static void *
deserialize_child_buffer_worker(void *arg) {
    struct workset *ws = (struct workset *) arg;
    while (1) {
        struct deserialize_child_buffer_work *dw = (struct deserialize_child_buffer_work *) workset_get(ws);
        if (dw == NULL)
            break;
        deserialize_child_buffer(dw->node, dw->cnum, &dw->rb, &dw->local_fingerprint);
    }
    workset_release_ref(ws);
    return arg;
}

static void
deserialize_all_child_buffers(BRTNODE result, struct rbuf *rbuf, struct sub_block_map child_buffer_map[], int my_num_cores, uint32_t *check_local_fingerprint_ret) {
    int n_nonempty_fifos = 0; // how many fifos are nonempty?
    for(int i = 0; i < result->u.n.n_children; i++) {
        if (child_buffer_map[i].size > 4)
            n_nonempty_fifos++;
    }

    int T = my_num_cores; // T = min(num_cores, n_nonempty_fifos) - 1
    if (T > n_nonempty_fifos)
        T = n_nonempty_fifos;
    if (T > 0)
        T = T - 1;       // threads in addition to the running thread

    struct workset ws;
    workset_init(&ws);

    struct deserialize_child_buffer_work work[result->u.n.n_children];
    workset_lock(&ws);
    for (int i = 0; i < result->u.n.n_children; i++) {
        deserialize_child_buffer_init(&work[i], result, i, rbuf->buf + node_header_overhead + child_buffer_map[i].offset, child_buffer_map[i].size);
        workset_put_locked(&ws, &work[i].base);
    }
    workset_unlock(&ws);

    // deserialize the fifos
    if (0) printf("%s:%d T=%d N=%d %d\n", __FUNCTION__, __LINE__, T, result->u.n.n_children, n_nonempty_fifos);
    toku_thread_pool_run(brt_pool, 0, &T, deserialize_child_buffer_worker, &ws);
    workset_add_ref(&ws, T);
    deserialize_child_buffer_worker(&ws);
    workset_join(&ws);

    // combine the fingerprints and update the buffer counts
    uint32_t check_local_fingerprint = 0;
    for (int i = 0; i < result->u.n.n_children; i++) {
        check_local_fingerprint += work[i].local_fingerprint;
        result->u.n.n_bytes_in_buffers += BNC_NBYTESINBUF(result, i);
    }

    // cleanup
    workset_destroy(&ws);

    *check_local_fingerprint_ret = check_local_fingerprint;
}

static int
deserialize_brtnode_nonleaf_from_rbuf (BRTNODE result, bytevec magic, struct rbuf *rb) {
    int r;

    if (memcmp(magic, "tokunode", 8)!=0) {
        r = toku_db_badformat();
        return r;
    }

    result->u.n.totalchildkeylens=0;
    u_int32_t subtree_fingerprint = rbuf_int(rb);
    u_int32_t check_subtree_fingerprint = 0;
    result->u.n.n_children = rbuf_int(rb);
    MALLOC_N(result->u.n.n_children+1,   result->u.n.childinfos);
    MALLOC_N(result->u.n.n_children, result->u.n.childkeys);
    //printf("n_children=%d\n", result->n_children);
    invariant(result->u.n.n_children>=0);
    for (int i=0; i<result->u.n.n_children; i++) {
        u_int32_t childfp = rbuf_int(rb);
        BNC_SUBTREE_FINGERPRINT(result, i)= childfp;
        check_subtree_fingerprint += childfp;
        struct subtree_estimates *se = &(BNC_SUBTREE_ESTIMATES(result, i));
        se->nkeys = rbuf_ulonglong(rb);
        se->ndata = rbuf_ulonglong(rb);
        se->dsize = rbuf_ulonglong(rb);
        se->exact = (BOOL) (rbuf_char(rb) != 0);
    }
    for (int i=0; i<result->u.n.n_children-1; i++) {
	bytevec childkeyptr;
	unsigned int cklen;
	rbuf_bytes(rb, &childkeyptr, &cklen); /* Returns a pointer into the rbuf. */
	result->u.n.childkeys[i] = kv_pair_malloc((void*)childkeyptr, cklen, 0, 0);
        //printf(" key %d length=%d data=%s\n", i, result->childkeylens[i], result->childkeys[i]);
        result->u.n.totalchildkeylens+=toku_brt_pivot_key_len(result->u.n.childkeys[i]);
    }
    for (int i=0; i<result->u.n.n_children; i++) {
        BNC_BLOCKNUM(result,i) = rbuf_blocknum(rb);
        BNC_HAVE_FULLHASH(result, i) = FALSE;
        BNC_NBYTESINBUF(result,i) = 0;
        //printf("Child %d at %lld\n", i, result->children[i]);
    }

    // deserialize the child buffer map
    struct sub_block_map child_buffer_map[result->u.n.n_children];
    for (int i = 0; i < result->u.n.n_children; i++) 
        sub_block_map_deserialize(&child_buffer_map[i], rb);

    // init the child buffers
    result->u.n.n_bytes_in_buffers = 0;
    for (int i=0; i<result->u.n.n_children; i++) {
        r=toku_fifo_create(&BNC_BUFFER(result,i));
        if (r!=0) {
            for (int j=0; j<i; j++) toku_fifo_free(&BNC_BUFFER(result,j));
            return toku_db_badformat();
        }
	toku_fifo_size_hint(BNC_BUFFER(result,i), child_buffer_map[i].size);
    }

    // deserialize all child buffers, like the function says
    uint32_t check_local_fingerprint;
    deserialize_all_child_buffers(result, rb, child_buffer_map, num_cores, &check_local_fingerprint);

    if (check_local_fingerprint != result->local_fingerprint) {
        fprintf(stderr, "%s:%d local fingerprint is wrong (found %8x calcualted %8x\n", __FILE__, __LINE__, result->local_fingerprint, check_local_fingerprint);
        return toku_db_badformat();
    }
    if (check_subtree_fingerprint+check_local_fingerprint != subtree_fingerprint) {
        fprintf(stderr, "%s:%d subtree fingerprint is wrong\n", __FILE__, __LINE__);
        return toku_db_badformat();
    }

    return 0;
}

static int
deserialize_brtnode_leaf_from_rbuf (BRTNODE result, bytevec magic, struct rbuf *rb) {
    int r;

    if (memcmp(magic, "tokuleaf", 8)!=0) {
        r = toku_db_badformat();
        return r;
    }

    result->u.l.leaf_stats.nkeys = rbuf_ulonglong(rb);
    result->u.l.leaf_stats.ndata = rbuf_ulonglong(rb);
    result->u.l.leaf_stats.dsize = rbuf_ulonglong(rb);
    result->u.l.leaf_stats.exact = TRUE;

    if (result->layout_version >= BRT_LAYOUT_VERSION_13) {
	result->u.l.optimized_for_upgrade = rbuf_int(rb);
    }
    else {
	result->u.l.optimized_for_upgrade = 0;
    }

    // deserialize the number of partitions
    int npartitions = rbuf_int(rb);
    invariant(npartitions == 1);
    
    // deserialize partition pivots
    for (int p = 0; p < npartitions-1; p++) { 
	// just throw them away for now
	bytevec childkeyptr;
	unsigned int cklen;
	rbuf_bytes(rb, &childkeyptr, &cklen);
    }

    // deserialize the partition map
    struct sub_block_map part_map[npartitions];
    for (int p = 0; p < npartitions; p++) 
        sub_block_map_deserialize(&part_map[p], rb);

    int n_in_buf = rbuf_int(rb);
    result->u.l.n_bytes_in_buffer = 0;
    result->u.l.seqinsert = 0;

    //printf("%s:%d r PMA= %p\n", __FILE__, __LINE__, result->u.l.buffer);
    toku_mempool_init(&result->u.l.buffer_mempool, rb->buf, rb->size);

    u_int32_t actual_sum = 0;
    u_int32_t start_of_data = rb->ndone;
    OMTVALUE *MALLOC_N(n_in_buf, array);
    if (result->layout_version == BRT_LAYOUT_VERSION) {
        for (int i=0; i<n_in_buf; i++) {
            LEAFENTRY le = (LEAFENTRY)(&rb->buf[rb->ndone]);
            u_int32_t disksize = leafentry_disksize(le);
            rb->ndone += disksize;
	    invariant(rb->ndone<=rb->size);
            array[i]=(OMTVALUE)le;
            actual_sum += x1764_memory(le, disksize);
        }
    }
    else if (result->layout_version == BRT_LAYOUT_VERSION_12) {
        for (int i=0; i<n_in_buf; i++) {
            // these two lines and optimized_for_upgrade logic above are only difference in handling 
            // versions 12 and 13 at this layer (more logic at higher layer)
            LEAFENTRY_12 le = (LEAFENTRY_12)(&rb->buf[rb->ndone]); 
            u_int32_t disksize = leafentry_disksize_12(le);        
            rb->ndone += disksize;
            invariant(rb->ndone<=rb->size);

            array[i]=(OMTVALUE)le;
            actual_sum += x1764_memory(le, disksize);
        }
    }
    else {
        invariant(FALSE);
    }
    toku_trace("fill array");
    u_int32_t end_of_data = rb->ndone;
    result->u.l.n_bytes_in_buffer += end_of_data-start_of_data + n_in_buf*OMT_ITEM_OVERHEAD;
    actual_sum *= result->rand4fingerprint;
    r = toku_omt_create_steal_sorted_array(&result->u.l.buffer, &array, n_in_buf, n_in_buf);
    toku_trace("create omt");
    if (r!=0) {
        toku_free(array);
        r = toku_db_badformat();
        if (0) { died_1: toku_omt_destroy(&result->u.l.buffer); }
        return r;
    }
    lazy_assert(array==NULL);

    result->u.l.buffer_mempool.frag_size = start_of_data;
    result->u.l.buffer_mempool.free_offset = end_of_data;

    if (r!=0) goto died_1;
    if (actual_sum!=result->local_fingerprint) {
        //fprintf(stderr, "%s:%d Corrupted checksum stored=%08x rand=%08x actual=%08x height=%d n_keys=%d\n", __FILE__, __LINE__, result->rand4fingerprint, result->local_fingerprint, actual_sum, result->height, n_in_buf);
        r = toku_db_badformat();
        goto died_1;
    } else {
        //fprintf(stderr, "%s:%d Good checksum=%08x height=%d\n", __FILE__, __LINE__, actual_sum, result->height);
    }
    
    //toku_verify_counts(result);

    if (rb->ndone != rb->size) { //Verify we read exactly the entire block.
        r = toku_db_badformat(); goto died_1;
    }

    r = toku_leaflock_borrow(result->u.l.leaflock_pool, &result->u.l.leaflock);
    if (r!=0) goto died_1;
    rb->buf = NULL; //Buffer was used for node's mempool.
    return 0;
}


static int
deserialize_brtnode_from_rbuf (BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h, struct rbuf *rb) {
    BRTNODE MALLOC(result);
    int r;
    if (result==0) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }
    result->ever_been_written = 1;

    //printf("Deserializing %lld datasize=%d\n", off, datasize);
    bytevec magic;
    rbuf_literal_bytes(rb, &magic, 8);
    result->layout_version    = rbuf_int(rb);
    invariant(result->layout_version >= BRT_LAYOUT_MIN_SUPPORTED_VERSION);
    invariant(result->layout_version <= BRT_LAYOUT_VERSION);
    result->layout_version_original = rbuf_int(rb);
    result->layout_version_read_from_disk = result->layout_version;
    result->nodesize = rbuf_int(rb);

    result->thisnodename = blocknum;
    result->flags = rbuf_int(rb);
    result->height = rbuf_int(rb);
    result->rand4fingerprint = rbuf_int(rb);
    result->local_fingerprint = rbuf_int(rb);
//    printf("%s:%d read %08x\n", __FILE__, __LINE__, result->local_fingerprint);
    result->dirty = 0;
    result->fullhash = fullhash;
    //printf("height==%d\n", result->height);

    if (result->height>0) 
        r = deserialize_brtnode_nonleaf_from_rbuf(result, magic, rb);
    else {
        result->u.l.leaflock_pool = toku_cachefile_leaflock_pool(h->cf);
        r = deserialize_brtnode_leaf_from_rbuf(result, magic, rb);
    }
    if (r!=0) goto died0;

    //printf("%s:%d Ok got %lld n_children=%d\n", __FILE__, __LINE__, result->thisnodename, result->n_children);
    if (result->height>0) {
	// For height==0 we used the buf inside the OMT
	toku_free(rb->buf);
        rb->buf = NULL;
    }
    *brtnode = result;
    //toku_verify_counts(result);
    return 0;
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
        case BRT_LAYOUT_VERSION_12:
        case BRT_LAYOUT_VERSION:
            r = decompress_from_raw_block_into_rbuf(raw_block, raw_block_size, rb, blocknum);
            break;
        default:
            lazy_assert(FALSE);
    }
    return r;
}

static int
deserialize_brtnode_from_rbuf_versioned (u_int32_t version, BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h, struct rbuf *rb) {
    int r = 0;
    BRTNODE node = NULL;
    r = deserialize_brtnode_from_rbuf(blocknum, fullhash, &node, h, rb);  // we just filled the node with contents from rbuf
    if (r==0) {
        invariant(node);
        int upgrade = 0;
        switch (version) {
            case BRT_LAYOUT_VERSION_12:
                invariant(node->layout_version == BRT_LAYOUT_VERSION_12);
                //Any upgrade necessary.
                if (node->height == 0) {
                    //leaf
                    uint32_t i;
                    OMT omt = node->u.l.buffer;
                    uint32_t num_les = toku_omt_size(omt);
                    LEAFENTRY *XCALLOC_N(num_les, new_les);
                    OMTVALUE v;

                    u_int32_t incremental_fingerprint = 0;
                    u_int32_t incremental_size = 0;
                    for (i = 0; i < num_les; i++) {
                        r = toku_omt_fetch(omt, i, &v, NULL);
                        invariant(r==0);
                        size_t new_memsize, new_disksize;
                        // Translate packed version 12 leafentry to packed version 13 leafentry
                        r = toku_le_upgrade_12_13(v, &new_memsize, &new_disksize, &new_les[i]);
                        invariant(r==0);
                        invariant(new_memsize == new_disksize);
                        incremental_size        += OMT_ITEM_OVERHEAD + new_memsize;
                        incremental_fingerprint += toku_le_crc(new_les[i]);
                    }
                    //Regenerate fingerprint.
                    node->local_fingerprint = node->rand4fingerprint * incremental_fingerprint;
                    //Set buffer size.
                    node->u.l.n_bytes_in_buffer = incremental_size;

                    //Replace mempool (destroy old, create new).
                    uint8_t *p;
                    {
                        void *mpbase = toku_mempool_get_base(&node->u.l.buffer_mempool);
                        toku_mempool_fini(&node->u.l.buffer_mempool);
                        toku_free(mpbase);
                        mpbase = toku_xmalloc(incremental_size);
                        toku_mempool_init(&node->u.l.buffer_mempool, mpbase, incremental_size);
                        node->u.l.buffer_mempool.free_offset = incremental_size;
                        p = mpbase;
                    }
                    //p points to beginning of new mempool
                    for (i = 0; i < num_les; i++) {
                        size_t len = leafentry_memsize(new_les[i]);
                        memcpy(p, new_les[i], len);
                        r = toku_omt_set_at(omt, p, i);
                        invariant(r==0);
                        p += len;
                        toku_free(new_les[i]); //Free malloced version of new leafentry (copy exists in mempool)
                    }
		    toku_free(new_les);  // Free array of pointers to new leafentries
                    //Regenerate nkeys, ndata, dsize
                    toku_brt_leaf_reset_calc_leaf_stats(node);
		    toku_sync_fetch_and_increment_uint64(&upgrade_status.leaf_12);  // how many leaf nodes upgraded from v12
                }
		else {
		    toku_sync_fetch_and_increment_uint64(&upgrade_status.nonleaf_12);  // how many nonleaf nodes upgraded from v12
		}
                node->flags &= ~TOKU_DB_VALCMP_BUILTIN_12;  // delete obsolete flag
                node->layout_version = BRT_LAYOUT_VERSION;
                upgrade++;
                //Fall through on purpose
            case BRT_LAYOUT_VERSION:
                invariant(node->layout_version == BRT_LAYOUT_VERSION);
                if (upgrade) {
                    toku_brtheader_lock(h);
                    invariant(h->num_blocks_to_upgrade>0);
                    h->num_blocks_to_upgrade--;
                    toku_brtheader_unlock(h);
                    node->dirty = 1;
                }
                *brtnode = node;
                break;    // this is the only break
            default:
                invariant(FALSE);
        }
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

// Read brt node from file into struct.  Perform version upgrade if necessary.
int
toku_deserialize_brtnode_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash,
                               BRTNODE *brtnode, struct brt_header *h) {
    toku_trace("deserial start");

    int r;
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};

    int layout_version;
    r = read_and_decompress_block_from_fd_into_rbuf(fd, blocknum, h, &rb, &layout_version);
    if (r!=0) goto cleanup;

    {
        u_int8_t *magic = rb.buf + uncompressed_magic_offset;
        if (memcmp(magic, "tokuleaf", 8)!=0 &&
            memcmp(magic, "tokunode", 8)!=0) {
            r = toku_db_badformat();
            goto cleanup;
        }
    }

    r = deserialize_brtnode_from_rbuf_versioned(layout_version, blocknum, fullhash, brtnode, h, &rb);

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
            case BRT_LAYOUT_VERSION_12:
                r = 0;
                upgrade++;
                //Fall through on purpose.
            case BRT_LAYOUT_VERSION:
                if (r == 0 && upgrade) {
                    r = toku_brt_optimize_for_upgrade(t);
		    toku_sync_fetch_and_increment_uint64(&upgrade_status.optimized_for_upgrade_12);
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


struct sum_info {
    unsigned int dsum;
    unsigned int msum;
    unsigned int count;
    u_int32_t    fp;
};

static int
sum_item (OMTVALUE lev, u_int32_t UU(idx), void *vsi) {
    LEAFENTRY le=lev;
    struct sum_info *si = vsi;
    si->count++;
    si->dsum += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
    si->msum += leafentry_memsize(le);
    si->fp  += toku_le_crc(le);
    return 0;
}

void
toku_verify_or_set_counts (BRTNODE node, BOOL set_fingerprints) {
    /*foo*/
    if (node->height==0) {
	lazy_assert(node->u.l.buffer);
	struct sum_info sum_info = {0,0,0,0};
	toku_omt_iterate(node->u.l.buffer, sum_item, &sum_info);
	lazy_assert(sum_info.count==toku_omt_size(node->u.l.buffer));
	lazy_assert(sum_info.dsum==node->u.l.n_bytes_in_buffer);
	lazy_assert(sum_info.msum == node->u.l.buffer_mempool.free_offset - node->u.l.buffer_mempool.frag_size);

	u_int32_t fps = node->rand4fingerprint * sum_info.fp;
        if (set_fingerprints) {
            node->local_fingerprint = fps;
        }
	lazy_assert(fps==node->local_fingerprint);
    } else {
	unsigned int sum = 0;
	for (int i=0; i<node->u.n.n_children; i++)
	    sum += BNC_NBYTESINBUF(node,i);
	// We don't rally care of the later buffers have garbage in them.  Valgrind would do a better job noticing if we leave it uninitialized.
	// But for now the code always initializes the later tables so they are 0.
        uint32_t fp = 0;
        int i;
        for (i=0; i<node->u.n.n_children; i++)
            FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
                              {
                                  fp += node->rand4fingerprint * toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
                              });
        if (set_fingerprints) {
            node->local_fingerprint = fp;
        }
        lazy_assert(fp==node->local_fingerprint);
	lazy_assert(sum==node->u.n.n_bytes_in_buffers);
    }
}

static u_int32_t
serialize_brt_header_min_size (u_int32_t version) {
    u_int32_t size = 0;
    switch(version) {
        case BRT_LAYOUT_VERSION_13:
            size += 8;  //TXNID that created
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
    wbuf_ulonglong(wbuf, h->num_blocks_to_upgrade);
    wbuf_TXNID(wbuf, h->root_xid_that_created);
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
	wbuf_init(&w_main, toku_malloc(size_main), size_main);
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

u_int32_t
toku_serialize_descriptor_size(const DESCRIPTOR desc) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4+ //version
                     4; //size
    size += desc->dbt.size;
    return size;
}

void
toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc) {
    if (desc->version==0) lazy_assert(desc->dbt.size==0);
    wbuf_int(wb, desc->version);
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
deserialize_descriptor_from_rbuf(struct rbuf *rb, DESCRIPTOR desc, BOOL temporary) {
    desc->version  = rbuf_int(rb);
    u_int32_t size;
    bytevec   data;
    rbuf_bytes(rb, &data, &size);
    bytevec   data_copy = data;;
    if (size>0) {
        if (!temporary) {
            data_copy = toku_memdup(data, size); //Cannot keep the reference from rbuf. Must copy.
            lazy_assert(data_copy);
        }
    }
    else {
        lazy_assert(size==0);
        data_copy = NULL;
    }
    toku_fill_dbt(&desc->dbt, data_copy, size);
    if (desc->version==0) lazy_assert(desc->dbt.size==0);
}

static void
deserialize_descriptor_from(int fd, struct brt_header *h, DESCRIPTOR desc) {
    DISKOFF offset;
    DISKOFF size;
    toku_get_descriptor_offset_size(h->blocktable, &offset, &size);
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
                deserialize_descriptor_from_rbuf(&rb, desc, FALSE);
            }
            lazy_assert(toku_serialize_descriptor_size(desc)+4 == size);
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
    deserialize_descriptor_from(fd, h, &h->descriptor);
    h->layout_version_original = rbuf_int(&rc);    
    h->num_blocks_to_upgrade   = rbuf_ulonglong(&rc);
    if (h->layout_version >= BRT_LAYOUT_VERSION_13) { 
        // at this layer, this new field is the only difference between versions 12 and 13
        rbuf_TXNID(&rc, &h->root_xid_that_created);
    }
    (void)rbuf_int(&rc); //Read in checksum and ignore (already verified).
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died1;}
    toku_free(rc.buf);
    rc.buf = NULL;
    *brth = h;
    return 0;
}



//TODO: When version 14 exists, add case for version 13 that looks like version 12 case,
static int
deserialize_brtheader_versioned (int fd, struct rbuf *rb, struct brt_header **brth, u_int32_t version) {
    int rval;
    int upgrade = 0;

    struct brt_header *h = NULL;
    rval = deserialize_brtheader (fd, rb, &h); //deserialize from rbuf and fd into header
    if (rval == 0) {
        invariant(h);
        switch (version) {
            case BRT_LAYOUT_VERSION_12:
                invariant(h->layout_version == BRT_LAYOUT_VERSION_12);
                {
                    //Upgrade root_xid_that_created
                    //Fake creation during the last checkpoint.
                    h->root_xid_that_created = h->checkpoint_lsn.lsn;
                }
                {
                    //Deprecate 'TOKU_DB_VALCMP_BUILTIN'.  Just remove the flag
                    h->flags &= ~TOKU_DB_VALCMP_BUILTIN_12;
                }
                h->layout_version++;
		toku_sync_fetch_and_increment_uint64(&upgrade_status.header_12);  // how many header nodes upgraded from v12
                upgrade++;
                //Fall through on purpose
            case BRT_LAYOUT_VERSION:
                invariant(h->layout_version == BRT_LAYOUT_VERSION);
                h->upgrade_brt_performed = FALSE;
                if (upgrade) {
                    toku_brtheader_lock(h);
                    h->num_blocks_to_upgrade = toku_block_get_blocks_in_use_unlocked(h->blocktable); //Total number of blocks
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
deserialize_brtheader_from_fd_into_rbuf(int fd, toku_off_t offset_of_header, struct rbuf *rb, u_int64_t *checkpoint_count, u_int32_t * version_p) {
    int r = 0;
    const int64_t prefix_size = 8 + // magic ("tokudata")
                                4 + // version
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
        if (r==0) {
            //Version MUST be in network order regardless of disk order.
            version = rbuf_network_int(rb);
	    *version_p = version;
            if (version < BRT_LAYOUT_MIN_SUPPORTED_VERSION) r = TOKUDB_DICTIONARY_TOO_OLD; //Cannot use
            if (version > BRT_LAYOUT_VERSION) r = TOKUDB_DICTIONARY_TOO_NEW; //Cannot use
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
            //Verify checksum (BRT_LAYOUT_VERSION_12 or later, when checksum function changed)
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


// Read brtheader from file into struct.  Read both headers and use the latest one.
int 
toku_deserialize_brtheader_from (int fd, struct brt_header **brth) {
    struct rbuf rb_0;
    struct rbuf rb_1;
    u_int64_t checkpoint_count_0;
    u_int64_t checkpoint_count_1;
    int r0;
    int r1;
    u_int32_t version_0, version_1, version = 0;

    {
        toku_off_t header_0_off = 0;
        r0 = deserialize_brtheader_from_fd_into_rbuf(fd, header_0_off, &rb_0, &checkpoint_count_0, &version_0);
    }
    {
        toku_off_t header_1_off = BLOCK_ALLOCATOR_HEADER_RESERVE;
        r1 = deserialize_brtheader_from_fd_into_rbuf(fd, header_1_off, &rb_1, &checkpoint_count_1, &version_1);
    }
    struct rbuf *rb = NULL;
    
    if (r0!=TOKUDB_DICTIONARY_TOO_NEW && r1!=TOKUDB_DICTIONARY_TOO_NEW) {
        if (r0==0) {
	    rb = &rb_0;
	    version = version_0;
	}
        if (r1==0 && (r0!=0 || checkpoint_count_1 > checkpoint_count_0)) {
	    rb = &rb_1;
	    version = version_1;
	}
        if (r0==0 && r1==0) {
	    lazy_assert(checkpoint_count_1 != checkpoint_count_0);
	    if (rb == &rb_0) lazy_assert(version_0 >= version_1);
	    else lazy_assert(version_0 <= version_1);
	}
    }
    int r = 0;
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
        else r = r0; //Arbitrarily report the error from the first header.
        lazy_assert(r!=0);
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
    size_t size = node_header_overhead //8 "tokuroll", 4 version, 4 version_original
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

// Read rollback log node from file into struct.  Perform version upgrade if necessary.
int
toku_deserialize_rollback_log_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash,
                                    ROLLBACK_LOG_NODE *logp, struct brt_header *h) {
    toku_trace("deserial start");

    int r;
    struct rbuf rb = {.buf = NULL, .size = 0, .ndone = 0};

    int layout_version;
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



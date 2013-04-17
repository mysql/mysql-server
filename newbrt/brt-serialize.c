/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

#if 0
static u_int64_t ntohll(u_int64_t v) {
    union u {
	u_int32_t l[2];
	u_int64_t ll;
    } uv;
    uv.ll = v;
    return (((u_int64_t)uv.l[0])<<32) + uv.l[1];
}
#endif

static u_int64_t umin64(u_int64_t a, u_int64_t b) {
    if (a<b) return a;
    return b;
}

static inline u_int64_t alignup (u_int64_t a, u_int64_t b) {
    return ((a+b-1)/b)*b;
}

// This mutex protects pwrite from running in parallel, and also protects modifications to the block allocator.
static toku_pthread_mutex_t pwrite_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;
static int pwrite_is_locked=0;

void toku_pwrite_lock_init(void) {
    int r = toku_pthread_mutex_init(&pwrite_mutex, NULL); assert(r == 0);
}

void toku_pwrite_lock_destroy(void) {
    int r = toku_pthread_mutex_destroy(&pwrite_mutex); assert(r == 0);
}

static inline void
lock_for_pwrite (void) {
    // Locks the pwrite_mutex.
    int r = toku_pthread_mutex_lock(&pwrite_mutex);
    assert(r==0);
    pwrite_is_locked = 1;
}

static inline void
unlock_for_pwrite (void) {
    pwrite_is_locked = 0;
    int r = toku_pthread_mutex_unlock(&pwrite_mutex);
    assert(r==0);
}


enum {FILE_CHANGE_INCREMENT = (16<<20)};

void
toku_maybe_truncate_cachefile (CACHEFILE cf, u_int64_t size_used)
// Effect: If file size >= SIZE+32MiB, reduce file size.
// (32 instead of 16.. hysteresis).
// Return 0 on success, otherwise an error number.
{
    //Check file size before taking pwrite lock to reduce likelihood of taking
    //the lock needlessly.
    //Check file size after taking lock to avoid race conditions.
    int64_t file_size;
    {
        int r = toku_os_get_file_size(toku_cachefile_fd(cf), &file_size);
        if (r!=0 && toku_cachefile_is_dev_null(cf)) goto done;
        assert(r==0);
        assert(file_size >= 0);
    }
    // If file space is overallocated by at least 32M
    if ((u_int64_t)file_size >= size_used + (2*FILE_CHANGE_INCREMENT)) {
        lock_for_pwrite();
        {
            int r = toku_os_get_file_size(toku_cachefile_fd(cf), &file_size);
            if (r!=0 && toku_cachefile_is_dev_null(cf)) goto cleanup;
            assert(r==0);
            assert(file_size >= 0);
        }
        if ((u_int64_t)file_size >= size_used + (2*FILE_CHANGE_INCREMENT)) {
            toku_off_t new_size = alignup(size_used, (2*FILE_CHANGE_INCREMENT)); //Truncate to new size_used.
            assert(new_size < file_size);
            int r = toku_cachefile_truncate(cf, new_size);
            assert(r==0);
        }
cleanup:
        unlock_for_pwrite();
    }
done:
    return;
}

int
maybe_preallocate_in_file (int fd, u_int64_t size)
// Effect: If file size is less than SIZE, make it bigger by either doubling it or growing by 16MiB whichever is less.
// Return 0 on success, otherwise an error number.
{
    int64_t file_size;
    {
        int r = toku_os_get_file_size(fd, &file_size);
        assert(r==0);
    }
    assert(file_size >= 0);
    if ((u_int64_t)file_size < size) {
	const int N = umin64(size, FILE_CHANGE_INCREMENT); // Double the size of the file, or add 16MiB, whichever is less.
	char *MALLOC_N(N, wbuf);
	memset(wbuf, 0, N);
	toku_off_t start_write = alignup(file_size, 4096);
	assert(start_write >= file_size);
	ssize_t r = toku_os_pwrite(fd, wbuf, N, start_write);
	if (r==-1) {
	    int e=errno; // must save errno before calling toku_free.
	    toku_free(wbuf);
	    return e;
	}
	toku_free(wbuf);
	assert(r==N);  // We don't handle short writes properly, which is the case where 0<= r < N.
    }
    return 0;
}

static int
toku_pwrite_extend (int fd, const void *buf, size_t count, toku_off_t offset, ssize_t *num_wrote)
// requires that the pwrite has been locked
// Returns 0 on success (and fills in *num_wrote for how many bytes are written)
// Returns nonzero error number problems.
{
    assert(pwrite_is_locked);
    {
	int r = maybe_preallocate_in_file(fd, offset+count);
	if (r!=0) {
	    *num_wrote = 0;
	    return r;
	}
    }
    {
	*num_wrote = toku_os_pwrite(fd, buf, count, offset);
	if (*num_wrote < 0) {
	    int r = errno;
	    *num_wrote = 0;
	    return r;
	} else {
	    return 0;
	}
    }
}

// Don't include the compression header
static const int brtnode_header_overhead = (8+   // magic "tokunode" or "tokuleaf"
					    4+   // nodesize
					    8+   // checkpoint number
					    4+   // target node size
					    4+   // flags
					    4+   // height
					    4+   // random for fingerprint
					    4+   // localfingerprint
					    4);  // crc32 at the end

static int
addupsize (OMTVALUE lev, u_int32_t UU(idx), void *vp) {
    LEAFENTRY le=lev;
    unsigned int *ip=vp;
    (*ip) += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
    return 0;
}

static unsigned int toku_serialize_brtnode_size_slow (BRTNODE node) {
    unsigned int size=brtnode_header_overhead;
    size += toku_serialize_descriptor_size(node->desc);
    if (node->height>0) {
	unsigned int hsize=0;
	unsigned int csize=0;
	int i;
	size+=4; /* n_children */
	size+=4; /* subtree fingerprint. */
	size+=4*(node->u.n.n_children-1); /* key lengths*/
	if (node->flags & TOKU_DB_DUPSORT) size += 4*(node->u.n.n_children-1);
	for (i=0; i<node->u.n.n_children-1; i++) {
	    csize+=toku_brtnode_pivot_key_len(node, node->u.n.childkeys[i]);
	}
	size+=(8+4+4+1+3*8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and 3*8 for the subtree estimates and 1 for the exact bit for the estimates. */
	int n_buffers = node->u.n.n_children;
        assert(0 <= n_buffers && n_buffers < TREE_FANOUT+1);
	for (i=0; i< n_buffers; i++) {
	    FIFO_ITERATE(BNC_BUFFER(node,i),
			 key __attribute__((__unused__)), keylen,
			 data __attribute__((__unused__)), datalen,
			 type __attribute__((__unused__)), xid __attribute__((__unused__)),
			 (hsize+=BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+keylen+datalen));
	}
	assert(hsize==node->u.n.n_bytes_in_buffers);
	assert(csize==node->u.n.totalchildkeylens);
	return size+hsize+csize;
    } else {
	unsigned int hsize=0;
	toku_omt_iterate(node->u.l.buffer,
			 addupsize,
			 &hsize);
	assert(hsize<=node->u.l.n_bytes_in_buffer);
	hsize+=4; /* add n entries in buffer table. */
	hsize+=3*8; /* add the three leaf stats, but no exact bit. */
	return size+hsize;
    }
}

// This is the size of the uncompressed data, not including the compression headers
unsigned int toku_serialize_brtnode_size (BRTNODE node) {
    unsigned int result =brtnode_header_overhead;
    assert(sizeof(toku_off_t)==8);
    result += toku_serialize_descriptor_size(node->desc);
    if (node->height>0) {
	result+=4; /* subtree fingerpirnt */
	result+=4; /* n_children */
	result+=4*(node->u.n.n_children-1); /* key lengths*/
        if (node->flags & TOKU_DB_DUPSORT) result += 4*(node->u.n.n_children-1); /* data lengths */
	assert(node->u.n.totalchildkeylens < (1<<30));
	result+=node->u.n.totalchildkeylens; /* the lengths of the pivot keys, without their key lengths. */
	result+=(8+4+4+1+3*8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and 3*8 for the subtree estimates and one for the exact bit. */
	result+=node->u.n.n_bytes_in_buffers;
    } else {
	result+=4; /* n_entries in buffer table. */
	result+=3*8; /* the three leaf stats. */
	result+=node->u.l.n_bytes_in_buffer;
	if (toku_memory_check) {
	    unsigned int slowresult = toku_serialize_brtnode_size_slow(node);
	    if (result!=slowresult) printf("%s:%d result=%u slowresult=%u\n", __FILE__, __LINE__, result, slowresult);
	    assert(result==slowresult);
	}
    }
    return result;
}

static int
wbufwriteleafentry (OMTVALUE lev, u_int32_t UU(idx), void *v) {
    LEAFENTRY le=lev;
    struct wbuf *thisw=v;
    wbuf_LEAFENTRY(thisw, le);
    return 0;
}

enum { uncompressed_magic_len = (8 // tokuleaf or tokunode
				 +4 // version
				 +8 // lsn
				 ) 
};

// uncompressed header offsets
enum {
    uncompressed_magic_offset = 0,
    uncompressed_version_offset = 8,
    uncompressed_lsn_offset = 12,
};

// compression header sub block sizes
struct sub_block_sizes {
    u_int32_t compressed_size;
    u_int32_t uncompressed_size;
};

// round up n
static inline int roundup2(int n, int alignment) {
    return (n+alignment-1)&~(alignment-1);
}

// choose the number of sub blocks such that the sub block size
// is around 1 meg.  put an upper bound on the number of sub blocks.
static int get_sub_block_sizes(int totalsize, int maxn, struct sub_block_sizes sizes[]) {
    const int meg = 1024*1024;
    const int alignment = 256;

    int n, subsize;
    n = totalsize/meg;
    if (n == 0) {
	n = 1;
        subsize = totalsize;
    } else {
        if (n > maxn)
            n = maxn;
	subsize = roundup2(totalsize/n, alignment);
        while (n < maxn && subsize >= meg + meg/8) {
            n++;
            subsize = roundup2(totalsize/n, alignment);
        }
    }

    // generate the sub block sizes
    int i;
    for (i=0; i<n-1; i++) {
        sizes[i].uncompressed_size = subsize;
        sizes[i].compressed_size = compressBound(subsize);
        totalsize -= subsize;
    }
    if (i == 0 || totalsize > 0) {
        sizes[i].uncompressed_size = totalsize;
        sizes[i].compressed_size = compressBound(totalsize);
        i++;
    }
    
    return i;
}

// get the size of the compression header
static size_t get_compression_header_size(int layout_version, int n) {
    if (layout_version < BRT_LAYOUT_VERSION_10)
        return n * sizeof (struct sub_block_sizes);
    else
        return sizeof (u_int32_t) + n * sizeof (struct sub_block_sizes);
}

// get the sum of the sub block compressed sizes 
static size_t get_sum_compressed_size(int n, struct sub_block_sizes sizes[]) {
    int i;
    size_t compressed_size = 0;
    for (i=0; i<n; i++) 
        compressed_size += sizes[i].compressed_size;
    return compressed_size;
}


// get the sum of the sub block uncompressed sizes 
static size_t get_sum_uncompressed_size(int n, struct sub_block_sizes sizes[]) {
    int i;
    size_t uncompressed_size = 0;
    for (i=0; i<n; i++) 
        uncompressed_size += sizes[i].uncompressed_size;
    return uncompressed_size;
}

static inline void ignore_int (int UU(ignore_me)) {}

static void serialize_descriptor_contents_to_wbuf(struct wbuf *wb, struct descriptor *desc);

int toku_serialize_brtnode_to (int fd, BLOCKNUM blocknum, BRTNODE node, struct brt_header *h, int n_workitems, int n_threads, BOOL for_checkpoint) {
    struct wbuf w;
    int i;

    // serialize the node into buf
    unsigned int calculated_size = toku_serialize_brtnode_size(node); 
    //printf("%s:%d serializing %" PRIu64 " size=%d\n", __FILE__, __LINE__, blocknum.b, calculated_size);
    //assert(calculated_size<=size);
    //char buf[size];
    char *MALLOC_N(calculated_size, buf);
    //toku_verify_counts(node);
    //assert(size>0);
    //printf("%s:%d serializing %lld w height=%d p0=%p\n", __FILE__, __LINE__, off, node->height, node->mdicts[0]);
    wbuf_init(&w, buf, calculated_size);
    wbuf_literal_bytes(&w, "toku", 4);
    if (node->height==0) wbuf_literal_bytes(&w, "leaf", 4);
    else wbuf_literal_bytes(&w, "node", 4);
    assert(node->layout_version == BRT_LAYOUT_VERSION_9 || node->layout_version == BRT_LAYOUT_VERSION);
    wbuf_int(&w, node->layout_version);
    wbuf_ulonglong(&w, node->log_lsn.lsn);
    assert(node->desc == &h->descriptor);
    serialize_descriptor_contents_to_wbuf(&w, node->desc);
    //printf("%s:%d %lld.calculated_size=%d\n", __FILE__, __LINE__, off, calculated_size);
    wbuf_uint(&w, node->nodesize);
    wbuf_uint(&w, node->flags);
    wbuf_int(&w,  node->height);
    //printf("%s:%d %lld rand=%08x sum=%08x height=%d\n", __FILE__, __LINE__, node->thisnodename, node->rand4fingerprint, node->subtree_fingerprint, node->height);
    wbuf_uint(&w, node->rand4fingerprint);
    wbuf_uint(&w, node->local_fingerprint);
//    printf("%s:%d wrote %08x for node %lld\n", __FILE__, __LINE__, node->local_fingerprint, (long long)node->thisnodename);
    //printf("%s:%d local_fingerprint=%8x\n", __FILE__, __LINE__, node->local_fingerprint);
    //printf("%s:%d w.ndone=%d n_children=%d\n", __FILE__, __LINE__, w.ndone, node->n_children);
    if (node->height>0) {
	assert(node->u.n.n_children>0);
	// Local fingerprint is not actually stored while in main memory.  Must calculate it.
	// Subtract the child fingerprints from the subtree fingerprint to get the local fingerprint.
	{
	    u_int32_t subtree_fingerprint = node->local_fingerprint;
	    for (i=0; i<node->u.n.n_children; i++) {
		subtree_fingerprint += BNC_SUBTREE_FINGERPRINT(node, i);
	    }
	    wbuf_uint(&w, subtree_fingerprint);
	}
	wbuf_int(&w, node->u.n.n_children);
	for (i=0; i<node->u.n.n_children; i++) {
	    wbuf_uint(&w, BNC_SUBTREE_FINGERPRINT(node, i));
	    struct subtree_estimates *se = &(BNC_SUBTREE_ESTIMATES(node, i));
	    wbuf_ulonglong(&w, se->nkeys);
	    wbuf_ulonglong(&w, se->ndata);
	    wbuf_ulonglong(&w, se->dsize);
	    wbuf_char     (&w, (char)se->exact);
	}
	//printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
	for (i=0; i<node->u.n.n_children-1; i++) {
            if (node->flags & TOKU_DB_DUPSORT) {
                wbuf_bytes(&w, kv_pair_key(node->u.n.childkeys[i]), kv_pair_keylen(node->u.n.childkeys[i]));
                wbuf_bytes(&w, kv_pair_val(node->u.n.childkeys[i]), kv_pair_vallen(node->u.n.childkeys[i]));
            } else {
                wbuf_bytes(&w, kv_pair_key(node->u.n.childkeys[i]), toku_brtnode_pivot_key_len(node, node->u.n.childkeys[i]));
            }
	    //printf("%s:%d w.ndone=%d (childkeylen[%d]=%d\n", __FILE__, __LINE__, w.ndone, i, node->childkeylens[i]);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    wbuf_BLOCKNUM(&w, BNC_BLOCKNUM(node,i));
	    //printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
	}

	{
	    int n_buffers = node->u.n.n_children;
	    u_int32_t check_local_fingerprint = 0;
	    for (i=0; i< n_buffers; i++) {
		//printf("%s:%d p%d=%p n_entries=%d\n", __FILE__, __LINE__, i, node->mdicts[i], mdict_n_entries(node->mdicts[i]));
		wbuf_int(&w, toku_fifo_n_entries(BNC_BUFFER(node,i)));
		FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
				  {
				      assert(type>=0 && type<256);
				      wbuf_char(&w, (unsigned char)type);
				      wbuf_TXNID(&w, xid);
				      wbuf_bytes(&w, key, keylen);
				      wbuf_bytes(&w, data, datalen);
				      check_local_fingerprint+=node->rand4fingerprint*toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
				  });
	    }
	    //printf("%s:%d check_local_fingerprint=%8x\n", __FILE__, __LINE__, check_local_fingerprint);
	    if (check_local_fingerprint!=node->local_fingerprint) printf("%s:%d node=%" PRId64 " fingerprint expected=%08x actual=%08x\n", __FILE__, __LINE__, node->thisnodename.b, check_local_fingerprint, node->local_fingerprint);
	    assert(check_local_fingerprint==node->local_fingerprint);
	}
    } else {
	//printf("%s:%d writing node %lld n_entries=%d\n", __FILE__, __LINE__, node->thisnodename, toku_gpma_n_entries(node->u.l.buffer));
	wbuf_ulonglong(&w, node->u.l.leaf_stats.nkeys);
	wbuf_ulonglong(&w, node->u.l.leaf_stats.ndata);
	wbuf_ulonglong(&w, node->u.l.leaf_stats.dsize);
	wbuf_uint(&w, toku_omt_size(node->u.l.buffer));
	toku_omt_iterate(node->u.l.buffer, wbufwriteleafentry, &w);
    }
    assert(w.ndone<=w.size);
#ifdef CRC_ATEND
    wbuf_int(&w, crc32(toku_null_crc, w.buf, w.ndone));
#endif
#ifdef CRC_INCR
    {
	u_int32_t checksum = x1764_finish(&w.checksum);
	wbuf_uint(&w, checksum);
    }
#endif

    if (calculated_size!=w.ndone)
	printf("%s:%d w.done=%u calculated_size=%u\n", __FILE__, __LINE__, w.ndone, calculated_size);
    assert(calculated_size==w.ndone);

    // The uncompressed part of the block header is
    //   tokuleaf(8),
    //   version(4),
    //   lsn(8),
    //   n_sub_blocks(4), followed by n length pairs
    //   compressed_len(4)
    //   uncompressed_len(4)

    // select the number of sub blocks and their sizes. 
    // impose an upper bound on the number of sub blocks.
    int max_sub_blocks = 4;
    if (node->layout_version < BRT_LAYOUT_VERSION_10)
        max_sub_blocks = 1;
    struct sub_block_sizes sub_block_sizes[max_sub_blocks];
    int n_sub_blocks = get_sub_block_sizes(calculated_size-uncompressed_magic_len, max_sub_blocks, sub_block_sizes);
    assert(0 < n_sub_blocks && n_sub_blocks <= max_sub_blocks);
    if (0 && n_sub_blocks != 1) {
        printf("%s:%d %d:", __FUNCTION__, __LINE__, n_sub_blocks);
        for (i=0; i<n_sub_blocks; i++)
            printf("%u ", sub_block_sizes[i].uncompressed_size);
        printf("\n");
    }
    
    size_t compressed_len = get_sum_compressed_size(n_sub_blocks, sub_block_sizes);
    size_t compression_header_len = get_compression_header_size(node->layout_version, n_sub_blocks);
    char *MALLOC_N(compressed_len+uncompressed_magic_len+compression_header_len, compressed_buf);
    memcpy(compressed_buf, buf, uncompressed_magic_len);
    if (0) printf("First 4 bytes before compressing data are %02x%02x%02x%02x\n",
                  buf[uncompressed_magic_len],   buf[uncompressed_magic_len+1],
                  buf[uncompressed_magic_len+2], buf[uncompressed_magic_len+3]);

    // TBD compress all of the sub blocks
    char *uncompressed_ptr = buf + uncompressed_magic_len;
    char *compressed_base_ptr = compressed_buf + uncompressed_magic_len + compression_header_len;
    char *compressed_ptr = compressed_base_ptr;
    for (i=0; i<n_sub_blocks; i++) {
        uLongf uncompressed_len = sub_block_sizes[i].uncompressed_size;

        uLongf real_compressed_len   = sub_block_sizes[i].compressed_size;

        {
#ifdef ADAPTIVE_COMPRESSION
            // Marketing has expressed concern that this algorithm will make customers go crazy.
            int compression_level;
            if      (n_workitems <=   n_threads) compression_level = 5;
            else if (n_workitems <= 2*n_threads) compression_level = 4;
            else if (n_workitems <= 3*n_threads) compression_level = 3;
            else if (n_workitems <= 4*n_threads) compression_level = 2;
            else                                 compression_level = 1;
#else
            int compression_level = 5;
            ignore_int(n_workitems); ignore_int(n_threads);
#endif
            //printf("compress(%d) n_workitems=%d n_threads=%d\n", compression_level, n_workitems, n_threads);
            int r = compress2((Bytef*)compressed_ptr, &real_compressed_len,
                              (Bytef*)uncompressed_ptr, uncompressed_len,
                              compression_level);
            assert(r==Z_OK);
            sub_block_sizes[i].compressed_size = real_compressed_len; // replace the compressed size estimate with the real size
            uncompressed_ptr += uncompressed_len;                     // update the uncompressed and compressed buffer pointers
            compressed_ptr += real_compressed_len;
        }
    }
    compressed_len = compressed_ptr - compressed_base_ptr;

    if (0) printf("Block %" PRId64 " Size before compressing %u, after compression %"PRIu64"\n", blocknum.b, calculated_size-uncompressed_magic_len, (uint64_t) compressed_len);

    // write out the compression header
    uint32_t *compressed_header_ptr = (uint32_t *)(compressed_buf + uncompressed_magic_len);
    if (node->layout_version >= BRT_LAYOUT_VERSION_10)
        *compressed_header_ptr++ = toku_htod32(n_sub_blocks);
    for (i=0; i<n_sub_blocks; i++) {
        compressed_header_ptr[0] = toku_htod32(sub_block_sizes[i].compressed_size);
        compressed_header_ptr[1] = toku_htod32(sub_block_sizes[i].uncompressed_size);
        compressed_header_ptr += 2;
    }

    //write_now: printf("%s:%d Writing %d bytes\n", __FILE__, __LINE__, w.ndone);
    int r;
    {
	// If the node has never been written, then write the whole buffer, including the zeros
	assert(blocknum.b>=0);
	//printf("%s:%d h=%p\n", __FILE__, __LINE__, h);
	//printf("%s:%d translated_blocknum_limit=%lu blocknum.b=%lu\n", __FILE__, __LINE__, h->translated_blocknum_limit, blocknum.b);
	//printf("%s:%d allocator=%p\n", __FILE__, __LINE__, h->block_allocator);
	//printf("%s:%d bt=%p\n", __FILE__, __LINE__, h->block_translation);
	size_t n_to_write = uncompressed_magic_len + compression_header_len + compressed_len;
	DISKOFF offset;

        //h will be dirtied
        toku_blocknum_realloc_on_disk(h->blocktable, blocknum, n_to_write, &offset,
                                      h, for_checkpoint);
	ssize_t n_wrote;
	lock_for_pwrite();
	r=toku_pwrite_extend(fd, compressed_buf, n_to_write, offset, &n_wrote);
	if (r) {
	    // fprintf(stderr, "%s:%d: Error writing data to file.  errno=%d (%s)\n", __FILE__, __LINE__, r, strerror(r));
	} else {
	    r=0;
	}
	unlock_for_pwrite();
    }

    //printf("%s:%d wrote %d bytes for %lld size=%lld\n", __FILE__, __LINE__, w.ndone, off, size);
    assert(w.ndone==calculated_size);
    toku_free(buf);
    toku_free(compressed_buf);
    return r;
}

#define DO_DECOMPRESS_WORKER 1

struct decompress_work {
    toku_pthread_t id;
    void *compress_ptr;
    void *uncompress_ptr;
    u_int32_t compress_size;
    u_int32_t uncompress_size;
};

// initialize the decompression work
static void init_decompress_work(struct decompress_work *w,
                                 void *compress_ptr, u_int32_t compress_size,
                                 void *uncompress_ptr, u_int32_t uncompress_size) {
    w->id = 0;
    w->compress_ptr = compress_ptr; w->compress_size = compress_size;
    w->uncompress_ptr = uncompress_ptr; w->uncompress_size = uncompress_size;
}

// do the decompression work
static void do_decompress_work(struct decompress_work *w) {
    uLongf destlen = w->uncompress_size;
    int r = uncompress(w->uncompress_ptr, &destlen,
                       w->compress_ptr, w->compress_size);
    assert(destlen==w->uncompress_size);
    assert(r==Z_OK);
}

#if DO_DECOMPRESS_WORKER

static void *decompress_worker(void *);

static void start_decompress_work(struct decompress_work *w) {
    int r = toku_pthread_create(&w->id, NULL, decompress_worker, w); assert(r == 0);
}

static void wait_decompress_work(struct decompress_work *w) {
    void *ret;
    int r = toku_pthread_join(w->id, &ret); assert(r == 0);
}

static void *decompress_worker(void *arg) {
    struct decompress_work *w = (struct decompress_work *) arg;
    do_decompress_work(w);
    return arg;
}

#endif

#define DO_TOKU_TRACE 0
#if DO_TOKU_TRACE
static int toku_trace_fd = -1;

static inline void do_toku_trace(const char *cp, int len) {
    write(toku_trace_fd, cp, len);
}
#define toku_trace(a)  do_toku_trace(a, strlen(a))
#else
#define toku_trace(a)
#endif

static void deserialize_descriptor_from_rbuf(struct rbuf *rb, struct descriptor *desc, BOOL temporary);

int toku_deserialize_brtnode_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h) {
    if (0) printf("Deserializing Block %" PRId64 "\n", blocknum.b);
    if (h->panic) return h->panic;

#if DO_TOKU_TRACE
    if (toku_trace_fd == -1) 
        toku_trace_fd = open("/dev/null", O_WRONLY);
    toku_trace("deserial start");
#endif

    // get the file offset and block size for the block
    DISKOFF offset, size;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &offset, &size);
    TAGMALLOC(BRTNODE, result);
    struct rbuf rc;
    int i;
    int r;
    if (result==0) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }
    result->desc = &h->descriptor;
    result->ever_been_written = 1;

    unsigned char *MALLOC_N(size, compressed_block);

    // read the compressed block
    ssize_t rlen = pread(fd, compressed_block, size, offset);
    assert((DISKOFF)rlen == size);

    // get the layout_version
    unsigned char *uncompressed_header = compressed_block;
    int layout_version = toku_dtoh32(*(uint32_t*)(uncompressed_header+uncompressed_version_offset));

    // get the number of compressed sub blocks
    int n_sub_blocks;
    int compression_header_offset;
    if (layout_version < BRT_LAYOUT_VERSION_10) {
        n_sub_blocks = 1; 
        compression_header_offset = uncompressed_magic_len;
    } else {
        n_sub_blocks = toku_dtoh32(*(u_int32_t*)(&uncompressed_header[uncompressed_magic_len]));
        compression_header_offset = uncompressed_magic_len + 4;
    }
    assert(0 < n_sub_blocks);

    // verify the sizes of the compressed sub blocks
    if (0 && n_sub_blocks != 1) printf("%s:%d %d\n", __FUNCTION__, __LINE__, n_sub_blocks);

    struct sub_block_sizes sub_block_sizes[n_sub_blocks];
    for (i=0; i<n_sub_blocks; i++) {
        u_int32_t compressed_size = toku_dtoh32(*(u_int32_t*)(&uncompressed_header[compression_header_offset+8*i]));
        if (compressed_size<=0   || compressed_size>(1<<30)) { r = toku_db_badformat(); goto died0; }
        u_int32_t uncompressed_size = toku_dtoh32(*(u_int32_t*)(&uncompressed_header[compression_header_offset+8*i+4]));
        if (0) printf("Block %" PRId64 " Compressed size = %u, uncompressed size=%u\n", blocknum.b, compressed_size, uncompressed_size);
        if (uncompressed_size<=0 || uncompressed_size>(1<<30)) { r = toku_db_badformat(); goto died0; }

        sub_block_sizes[i].compressed_size = compressed_size;
        sub_block_sizes[i].uncompressed_size = uncompressed_size;
    }

    unsigned char *compressed_data = compressed_block + uncompressed_magic_len + get_compression_header_size(layout_version, n_sub_blocks);

    size_t uncompressed_size = get_sum_uncompressed_size(n_sub_blocks, sub_block_sizes);
    rc.size= uncompressed_magic_len + uncompressed_size;
    assert(rc.size>0);

    rc.buf=toku_malloc(rc.size);
    assert(rc.buf);

    // construct the uncompressed block from the header and compressed sub blocks
    memcpy(rc.buf, uncompressed_header, uncompressed_magic_len);

    // decompress the sub blocks
    unsigned char *uncompressed_data = rc.buf+uncompressed_magic_len;
    struct decompress_work decompress_work[n_sub_blocks];

    for (i=0; i<n_sub_blocks; i++) {
        init_decompress_work(&decompress_work[i], compressed_data, sub_block_sizes[i].compressed_size, uncompressed_data, sub_block_sizes[i].uncompressed_size);
        if (i>0) {
#if DO_DECOMPRESS_WORKER
            start_decompress_work(&decompress_work[i]);
#else
            do_decompress_work(&decompress_work[i]);
#endif
        }
        uncompressed_data += sub_block_sizes[i].uncompressed_size;
        compressed_data += sub_block_sizes[i].compressed_size;
    }
    do_decompress_work(&decompress_work[0]);
#if DO_DECOMPRESS_WORKER
    for (i=1; i<n_sub_blocks; i++)
        wait_decompress_work(&decompress_work[i]);
#endif
    toku_trace("decompress done");

    if (0) printf("First 4 bytes of uncompressed data are %02x%02x%02x%02x\n",
		  rc.buf[uncompressed_magic_len],   rc.buf[uncompressed_magic_len+1],
		  rc.buf[uncompressed_magic_len+2], rc.buf[uncompressed_magic_len+3]);

    toku_free(compressed_block);

    // deserialize the uncompressed block
    rc.ndone=0;
    //printf("Deserializing %lld datasize=%d\n", off, datasize);
    {
	bytevec tmp;
	rbuf_literal_bytes(&rc, &tmp, 8);
	if (memcmp(tmp, "tokuleaf", 8)!=0
	    && memcmp(tmp, "tokunode", 8)!=0) {
	    r = toku_db_badformat();
	    return r;
	}
    }
    result->layout_version    = rbuf_int(&rc);
    {
	switch (result->layout_version) {
	case BRT_LAYOUT_VERSION_10: goto ok_layout_version;
	    // Don't support older versions.
	}
	r=toku_db_badformat();
	return r;
    ok_layout_version: ;
    }
    result->disk_lsn.lsn = rbuf_ulonglong(&rc);
    {
        //Restrict scope for now since we do not support upgrades.
        struct descriptor desc;
        //desc.dbt.data is TEMPORARY.  Will be unusable when the rc buffer is freed.
        deserialize_descriptor_from_rbuf(&rc, &desc, TRUE);
        assert(desc.version == result->desc->version); //We do not yet support upgrading the dbts.
    }
    result->nodesize = rbuf_int(&rc);
    result->log_lsn = result->disk_lsn;

    result->thisnodename = blocknum;
    result->flags = rbuf_int(&rc);
    result->height = rbuf_int(&rc);
    result->rand4fingerprint = rbuf_int(&rc);
    result->local_fingerprint = rbuf_int(&rc);
//    printf("%s:%d read %08x\n", __FILE__, __LINE__, result->local_fingerprint);
    result->dirty = 0;
    result->fullhash = fullhash;
    //printf("height==%d\n", result->height);
    if (result->height>0) {
	result->u.n.totalchildkeylens=0;
	u_int32_t subtree_fingerprint = rbuf_int(&rc);
	u_int32_t check_subtree_fingerprint = 0;
	result->u.n.n_children = rbuf_int(&rc);
	MALLOC_N(result->u.n.n_children+1,   result->u.n.childinfos);
	MALLOC_N(result->u.n.n_children, result->u.n.childkeys);
	//printf("n_children=%d\n", result->n_children);
	assert(result->u.n.n_children>=0);
	for (i=0; i<result->u.n.n_children; i++) {
	    u_int32_t childfp = rbuf_int(&rc);
	    BNC_SUBTREE_FINGERPRINT(result, i)= childfp;
	    check_subtree_fingerprint += childfp;
	    struct subtree_estimates *se = &(BNC_SUBTREE_ESTIMATES(result, i));
	    se->nkeys = rbuf_ulonglong(&rc);
	    se->ndata = rbuf_ulonglong(&rc);
	    se->dsize = rbuf_ulonglong(&rc);
	    se->exact = rbuf_char(&rc);
	}
	for (i=0; i<result->u.n.n_children-1; i++) {
            if (result->flags & TOKU_DB_DUPSORT) {
                bytevec keyptr, dataptr;
                unsigned int keylen, datalen;
                rbuf_bytes(&rc, &keyptr, &keylen);
                rbuf_bytes(&rc, &dataptr, &datalen);
                result->u.n.childkeys[i] = kv_pair_malloc(keyptr, keylen, dataptr, datalen);
            } else {
                bytevec childkeyptr;
		unsigned int cklen;
                rbuf_bytes(&rc, &childkeyptr, &cklen); /* Returns a pointer into the rbuf. */
                result->u.n.childkeys[i] = kv_pair_malloc((void*)childkeyptr, cklen, 0, 0);
            }
            //printf(" key %d length=%d data=%s\n", i, result->childkeylens[i], result->childkeys[i]);
	    result->u.n.totalchildkeylens+=toku_brtnode_pivot_key_len(result, result->u.n.childkeys[i]);
	}
	for (i=0; i<result->u.n.n_children; i++) {
	    BNC_BLOCKNUM(result,i) = rbuf_blocknum(&rc);
	    BNC_HAVE_FULLHASH(result, i) = FALSE;
	    BNC_NBYTESINBUF(result,i) = 0;
	    //printf("Child %d at %lld\n", i, result->children[i]);
	}
	result->u.n.n_bytes_in_buffers = 0;
	for (i=0; i<result->u.n.n_children; i++) {
	    r=toku_fifo_create(&BNC_BUFFER(result,i));
	    if (r!=0) {
		int j;
		if (0) { died_12: j=result->u.n.n_bytes_in_buffers; }
		for (j=0; j<i; j++) toku_fifo_free(&BNC_BUFFER(result,j));
		return toku_db_badformat();
	    }
	}
	{
	    int cnum;
	    u_int32_t check_local_fingerprint = 0;
	    for (cnum=0; cnum<result->u.n.n_children; cnum++) {
		int n_in_this_hash = rbuf_int(&rc);
		//printf("%d in hash\n", n_in_hash);
		for (i=0; i<n_in_this_hash; i++) {
		    int diff;
		    bytevec key; ITEMLEN keylen;
		    bytevec val; ITEMLEN vallen;
		    //toku_verify_counts(result);
                    int type = rbuf_char(&rc);
		    TXNID xid  = rbuf_ulonglong(&rc);
		    rbuf_bytes(&rc, &key, &keylen); /* Returns a pointer into the rbuf. */
		    rbuf_bytes(&rc, &val, &vallen);
		    check_local_fingerprint += result->rand4fingerprint * toku_calc_fingerprint_cmd(type, xid, key, keylen, val, vallen);
		    //printf("Found %s,%s\n", (char*)key, (char*)val);
		    {
			r=toku_fifo_enq(BNC_BUFFER(result, cnum), key, keylen, val, vallen, type, xid); /* Copies the data into the hash table. */
			if (r!=0) { goto died_12; }
		    }
		    diff =  keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
		    result->u.n.n_bytes_in_buffers += diff;
		    BNC_NBYTESINBUF(result,cnum)   += diff;
		    //printf("Inserted\n");
		}
	    }
	    if (check_local_fingerprint != result->local_fingerprint) {
		fprintf(stderr, "%s:%d local fingerprint is wrong (found %8x calcualted %8x\n", __FILE__, __LINE__, result->local_fingerprint, check_local_fingerprint);
		return toku_db_badformat();
	    }
	    if (check_subtree_fingerprint+check_local_fingerprint != subtree_fingerprint) {
		fprintf(stderr, "%s:%d subtree fingerprint is wrong\n", __FILE__, __LINE__);
		return toku_db_badformat();
	    }
	}
    } else {
	result->u.l.leaf_stats.nkeys = rbuf_ulonglong(&rc);
	result->u.l.leaf_stats.ndata = rbuf_ulonglong(&rc);
	result->u.l.leaf_stats.dsize = rbuf_ulonglong(&rc);
	result->u.l.leaf_stats.exact = TRUE;
	int n_in_buf = rbuf_int(&rc);
	result->u.l.n_bytes_in_buffer = 0;
        result->u.l.seqinsert = 0;

	//printf("%s:%d r PMA= %p\n", __FILE__, __LINE__, result->u.l.buffer);
	toku_mempool_init(&result->u.l.buffer_mempool, rc.buf, uncompressed_size + uncompressed_magic_len);

	u_int32_t actual_sum = 0;
	u_int32_t start_of_data = rc.ndone;
	OMTVALUE *MALLOC_N(n_in_buf, array);
	for (i=0; i<n_in_buf; i++) {
	    LEAFENTRY le = (LEAFENTRY)(&rc.buf[rc.ndone]);
	    u_int32_t disksize = leafentry_disksize(le);
	    rc.ndone += disksize;
	    assert(rc.ndone<=rc.size);

	    array[i]=(OMTVALUE)le;
	    actual_sum += x1764_memory(le, disksize);
	}
        toku_trace("fill array");
	u_int32_t end_of_data = rc.ndone;
	result->u.l.n_bytes_in_buffer += end_of_data-start_of_data + n_in_buf*OMT_ITEM_OVERHEAD;
	actual_sum *= result->rand4fingerprint;
	r = toku_omt_create_steal_sorted_array(&result->u.l.buffer, &array, n_in_buf, n_in_buf);
        toku_trace("create omt");
	if (r!=0) {
            toku_free(array);
	    if (0) { died_21: toku_omt_destroy(&result->u.l.buffer); }
	    return toku_db_badformat();
	}
        assert(array==NULL);
        r = toku_leaflock_borrow(&result->u.l.leaflock);
        if (r!=0) goto died_21;

	result->u.l.buffer_mempool.frag_size = start_of_data;
	result->u.l.buffer_mempool.free_offset = end_of_data;

	if (r!=0) goto died_21;
	if (actual_sum!=result->local_fingerprint) {
	    //fprintf(stderr, "%s:%d Corrupted checksum stored=%08x rand=%08x actual=%08x height=%d n_keys=%d\n", __FILE__, __LINE__, result->rand4fingerprint, result->local_fingerprint, actual_sum, result->height, n_in_buf);
	    return toku_db_badformat();
	    // goto died_21;
	} else {
	    //fprintf(stderr, "%s:%d Good checksum=%08x height=%d\n", __FILE__, __LINE__, actual_sum, result->height);
	}
	
	//toku_verify_counts(result);
    }
    {
	unsigned int n_read_so_far = rc.ndone;
	if (n_read_so_far+4!=rc.size) {
	    r = toku_db_badformat(); goto died_21;
	}
        toku_trace("x1764 start");
	uint32_t crc = x1764_memory(rc.buf, n_read_so_far);
        toku_trace("x1764");
	uint32_t storedcrc = rbuf_int(&rc);
	if (crc!=storedcrc) {
	    printf("Bad CRC\n");
	    printf("%s:%d crc=%08x stored=%08x\n", __FILE__, __LINE__, crc, storedcrc);
	    assert(0);//this is wrong!!!
	    r = toku_db_badformat();
	    goto died_21;
	}
    }
    //printf("%s:%d Ok got %lld n_children=%d\n", __FILE__, __LINE__, result->thisnodename, result->n_children);
    if (result->height>0) {
	// For height==0 we used the buf inside the OMT
	toku_free(rc.buf);
    }
    toku_trace("deserial done");
    *brtnode = result;
    //toku_verify_counts(result);
    return 0;
}

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

void toku_verify_counts (BRTNODE node) {
    /*foo*/
    if (node->height==0) {
	assert(node->u.l.buffer);
	struct sum_info sum_info = {0,0,0,0};
	toku_omt_iterate(node->u.l.buffer, sum_item, &sum_info);
	assert(sum_info.count==toku_omt_size(node->u.l.buffer));
	assert(sum_info.dsum==node->u.l.n_bytes_in_buffer);
	assert(sum_info.msum == node->u.l.buffer_mempool.free_offset - node->u.l.buffer_mempool.frag_size);

	u_int32_t fps = node->rand4fingerprint * sum_info.fp;
	assert(fps==node->local_fingerprint);
    } else {
	unsigned int sum = 0;
	int i;
	for (i=0; i<node->u.n.n_children; i++)
	    sum += BNC_NBYTESINBUF(node,i);
	// We don't rally care of the later buffers have garbage in them.  Valgrind would do a better job noticing if we leave it uninitialized.
	// But for now the code always initializes the later tables so they are 0.
	assert(sum==node->u.n.n_bytes_in_buffers);
    }
}

static u_int32_t
serialize_brt_header_min_size (u_int32_t version) {
    u_int32_t size;
    switch(version) {
        case BRT_LAYOUT_VERSION_10:
            size = (+8 // "tokudata"
                    +4 // version
                    +4 // size
                    +8 // byte order verification
                    +8 // checkpoint_count
                    +8 // checkpoint_lsn
                    +4 // tree's nodesize
                    +8 // translation_size_on_disk
                    +8 // translation_address_on_disk
                    +4 // checksum
                    );
            size+=(+8 // diskoff
                   +4 // flags
                   );
            break;
        default:
            assert(FALSE);
    }
    assert(size <= BLOCK_ALLOCATOR_HEADER_RESERVE);
    return size;
}

int toku_serialize_brt_header_size (struct brt_header *h) {
    u_int32_t size = serialize_brt_header_min_size(h->layout_version);
    //Add any dynamic data.
    assert(size <= BLOCK_ALLOCATOR_HEADER_RESERVE);
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
    wbuf_int    (wbuf, h->flags);
    u_int32_t checksum = x1764_finish(&wbuf->checksum);
    wbuf_int(wbuf, checksum);
    assert(wbuf->ndone<=wbuf->size);
    return 0;
}

int toku_serialize_brt_header_to (int fd, struct brt_header *h) {
    int rr = 0;
    if (h->panic) return h->panic;
    assert(h->type==BRTHEADER_CHECKPOINT_INPROGRESS);
    toku_brtheader_lock(h);
    struct wbuf w_translation;
    int64_t size_translation;
    int64_t address_translation;
    {
        //Must serialize translation first, to get address,size for header.
        toku_serialize_translation_to_wbuf_unlocked(h->blocktable, &w_translation,
                                                   &address_translation,
                                                   &size_translation);
        assert(size_translation==w_translation.size);
    }
    struct wbuf w_main;
    unsigned int size_main = toku_serialize_brt_header_size (h);
    {
	wbuf_init(&w_main, toku_malloc(size_main), size_main);
	{
	    int r=toku_serialize_brt_header_to_wbuf(&w_main, h, address_translation, size_translation);
	    assert(r==0);
	}
	assert(w_main.ndone==size_main);
    }
    toku_brtheader_unlock(h);
    char *writing_what;
    lock_for_pwrite();
    {
        //Actual Write translation table
	ssize_t nwrote;
	rr = toku_pwrite_extend(fd, w_translation.buf,
                                size_translation, address_translation, &nwrote);
	if (rr) {
            writing_what = "translation";
            goto panic;
	}
	assert(nwrote==size_translation);
    }
    {
        //Actual Write main header
	ssize_t nwrote;
        //Alternate writing header to two locations:
        //   Beginning (0) or BLOCK_ALLOCATOR_HEADER_RESERVE
        toku_off_t main_offset;
        //TODO: #1623 uncomment next line when ready for 2 headers
        main_offset = (h->checkpoint_count & 0x1) ? 0 : BLOCK_ALLOCATOR_HEADER_RESERVE;
	rr = toku_pwrite_extend(fd, w_main.buf, w_main.ndone, main_offset, &nwrote);
	if (rr) {
            writing_what = "header";
            panic:
	    if (h->panic==0) {
		char *e = strerror(rr);
		int l = 200 + strlen(e);
		char s[l];
		h->panic=rr;
		snprintf(s, l-1, "%s:%d: Error writing %s to data file.  errno=%d (%s)\n", __FILE__, __LINE__, writing_what, rr, e);
		h->panic_string = toku_strdup(s);
	    }
	    goto finish;
	}
	assert((u_int64_t)nwrote==size_main);
    }
 finish:
    toku_free(w_main.buf);
    toku_free(w_translation.buf);
    unlock_for_pwrite();
    return rr;
}

u_int32_t
toku_serialize_descriptor_size(struct descriptor *desc) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4+ //version
                     4; //size
    size += desc->dbt.size;
    return size;
}

static void
serialize_descriptor_contents_to_wbuf(struct wbuf *wb, struct descriptor *desc) {
    if (desc->version==0) assert(desc->dbt.size==0);
    wbuf_int(wb, desc->version);
    wbuf_bytes(wb, desc->dbt.data, desc->dbt.size);
}

//Descriptor is written to disk during toku_brt_open iff we have a new (or changed)
//descriptor.
//Descriptors are NOT written during the header checkpoint process.
int
toku_serialize_descriptor_contents_to_fd(int fd, struct descriptor *desc, DISKOFF offset) {
    int r;
    // make the checksum
    int64_t size = toku_serialize_descriptor_size(desc)+4; //4 for checksum
    struct wbuf w;
    wbuf_init(&w, toku_xmalloc(size), size);
    serialize_descriptor_contents_to_wbuf(&w, desc);
    {
        //Add checksum
        u_int32_t checksum = x1764_finish(&w.checksum);
        wbuf_int(&w, checksum);
    }
    assert(w.ndone==w.size);
    {
        lock_for_pwrite();
        //Actual Write translation table
	ssize_t nwrote;
	r = toku_pwrite_extend(fd, w.buf, size, offset, &nwrote);
        unlock_for_pwrite();
        if (r==0) assert(nwrote==size);
    }
    toku_free(w.buf);
    return r;
}

static void
deserialize_descriptor_from_rbuf(struct rbuf *rb, struct descriptor *desc, BOOL temporary) {
    desc->version  = rbuf_int(rb);
    u_int32_t size;
    bytevec   data;
    rbuf_bytes(rb, &data, &size);
    bytevec   data_copy = data;;
    if (size>0) {
        if (!temporary) {
            data_copy = toku_memdup(data, size); //Cannot keep the reference from rbuf. Must copy.
            assert(data_copy);
        }
    }
    else {
        assert(size==0);
        data_copy = NULL;
    }
    toku_fill_dbt(&desc->dbt, data_copy, size);
    if (desc->version==0) assert(desc->dbt.size==0);
}

static void
deserialize_descriptor_from(int fd, struct brt_header *h, struct descriptor *desc) {
    DISKOFF offset;
    DISKOFF size;
    toku_get_descriptor_offset_size(h->blocktable, &offset, &size);
    memset(desc, 0, sizeof(*desc));
    if (size > 0) {
        assert(size>=4); //4 for checksum
        {
            unsigned char *XMALLOC_N(size, dbuf);
            {
                lock_for_pwrite();
                ssize_t r = pread(fd, dbuf, size, offset);
                assert(r==size);
                unlock_for_pwrite();
            }
            {
                // check the checksum
                u_int32_t x1764 = x1764_memory(dbuf, size-4);
                //printf("%s:%d read from %ld (x1764 offset=%ld) size=%ld\n", __FILE__, __LINE__, block_translation_address_on_disk, offset, block_translation_size_on_disk);
                u_int32_t stored_x1764 = toku_dtoh32(*(int*)(dbuf + size-4));
                assert(x1764 == stored_x1764);
            }
            {
                struct rbuf rb = {.buf = dbuf, .size = size, .ndone = 0};
                //Not temporary; must have a toku_memdup'd copy.
                deserialize_descriptor_from_rbuf(&rb, desc, FALSE);
            }
            assert(toku_serialize_descriptor_size(desc)+4 == size);
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

    //Steal rbuf (used to simplify merge/reduce diff size/keep old code)
    struct rbuf rc = *rb;
    memset(rb, 0, sizeof(*rb));

    //Verification of initial elements.
    {
        //Check magic number
        bytevec magic;
        rbuf_literal_bytes(&rc, &magic, 8);
        assert(memcmp(magic,"tokudata",8)==0);
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
    list_init(&h->live_brts);
    list_init(&h->zombie_brts);
    //version MUST be in network order on disk regardless of disk order
    h->layout_version = rbuf_network_int(&rc);
    assert(h->layout_version==BRT_LAYOUT_VERSION_10);

    //Size MUST be in network order regardless of disk order.
    u_int32_t size = rbuf_network_int(&rc);
    assert(size==rc.size);

    bytevec tmp_byte_order_check;
    rbuf_literal_bytes(&rc, &tmp_byte_order_check, 8); //Must not translate byte order
    int64_t byte_order_stored = *(int64_t*)tmp_byte_order_check;
    assert(byte_order_stored == toku_byte_order_host);

    h->checkpoint_count = rbuf_ulonglong(&rc);
    h->checkpoint_lsn   = rbuf_lsn(&rc);
    h->nodesize      = rbuf_int(&rc);
    DISKOFF translation_address_on_disk = rbuf_diskoff(&rc);
    DISKOFF translation_size_on_disk    = rbuf_diskoff(&rc);
    assert(translation_address_on_disk>0);
    assert(translation_size_on_disk>0);

    // printf("%s:%d translated_blocknum_limit=%ld, block_translation_address_on_disk=%ld\n", __FILE__, __LINE__, h->translated_blocknum_limit, h->block_translation_address_on_disk);
    //Load translation table
    {
        lock_for_pwrite();
        unsigned char *XMALLOC_N(translation_size_on_disk, tbuf);
        {
            // This cast is messed up in 32-bits if the block translation table is ever more than 4GB.  But in that case, the translation table itself won't fit in main memory.
            ssize_t r = pread(fd, tbuf, translation_size_on_disk, translation_address_on_disk);
            assert(r==translation_size_on_disk);
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
    (void)rbuf_int(&rc); //Read in checksum and ignore (already verified).
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died1;}
    toku_free(rc.buf);
    rc.buf = NULL;
    *brth = h;
    return 0;
}

//TOKUDB_DICTIONARY_NO_HEADER means we can overwrite everything in the file AND the header is useless
static int
deserialize_brtheader_from_fd_into_rbuf(int fd, toku_off_t offset, struct rbuf *rb, u_int64_t *checkpoint_count) {
    int r = 0;
    const int64_t prefix_size = 8 + // magic ("tokudata")
                                4 + // version
                                4;  // size
    unsigned char prefix[prefix_size];
    rb->buf = NULL;
    int64_t n = pread(fd, prefix, prefix_size, offset);
    if (n==0) r = TOKUDB_DICTIONARY_NO_HEADER;
    else if (n<0) {r = errno; assert(r!=0);}
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
            if (version < BRT_LAYOUT_VERSION_10) r = TOKUDB_DICTIONARY_TOO_OLD; //Cannot use
            if (version > BRT_LAYOUT_VERSION_10) r = TOKUDB_DICTIONARY_TOO_NEW; //Cannot use
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
            assert(rb->ndone==prefix_size);
            rb->size = size;
            rb->buf  = toku_xmalloc(rb->size);
        }
        if (r==0) {
            n = pread(fd, rb->buf, rb->size, offset);
            if (n==-1) {
                r = errno;
                assert(r!=0);
            }
            else if (n!=(int64_t)rb->size) r = EINVAL; //Header might be useless (wrong size) or could be a disk read error.
        }
        //It's version 10 or later.  Magic looks OK.
        //We have an rbuf that represents the header.
        //Size is within acceptable bounds.
        if (r==0) {
            //Verify checksum
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

//TODO:
// * read in whole thing, do checksum
// * switch to using rbuf
// * read in size, version, LSN and checkpoint count (pre)
// * read in LSN and checkpoint count  to save in header object
int toku_deserialize_brtheader_from (int fd, struct brt_header **brth) {
    struct rbuf rb_0;
    struct rbuf rb_1;
    u_int64_t checkpoint_count_0;
    u_int64_t checkpoint_count_1;
    int r0;
    int r1;
    {
        toku_off_t header_0_off = 0;
        r0 = deserialize_brtheader_from_fd_into_rbuf(fd, header_0_off, &rb_0, &checkpoint_count_0);
    }
    {
        toku_off_t header_1_off = BLOCK_ALLOCATOR_HEADER_RESERVE;
        r1 = deserialize_brtheader_from_fd_into_rbuf(fd, header_1_off, &rb_1, &checkpoint_count_1);
    }
    struct rbuf *rb = NULL;
    
    if (r0!=TOKUDB_DICTIONARY_TOO_NEW && r1!=TOKUDB_DICTIONARY_TOO_NEW) {
        if (r0==0) rb = &rb_0;
        if (r1==0 && (r0!=0 || checkpoint_count_1 > checkpoint_count_0)) rb = &rb_1;
        if (r0==0 && r1==0) assert(checkpoint_count_1 != checkpoint_count_0);
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
        assert(r!=0);
    }

    if (r==0) r = deserialize_brtheader(fd, rb, brth);
    if (rb_0.buf) toku_free(rb_0.buf);
    if (rb_1.buf) toku_free(rb_1.buf);
    return r;
}

unsigned int toku_brt_pivot_key_len (BRT brt, struct kv_pair *pk) {
    if (brt->flags & TOKU_DB_DUPSORT) {
	return kv_pair_keylen(pk) + kv_pair_vallen(pk);
    } else {
	return kv_pair_keylen(pk);
    }
}

unsigned int toku_brtnode_pivot_key_len (BRTNODE node, struct kv_pair *pk) {
    if (node->flags & TOKU_DB_DUPSORT) {
	return kv_pair_keylen(pk) + kv_pair_vallen(pk);
    } else {
	return kv_pair_keylen(pk);
    }
}

static int
read_int (int fd, toku_off_t *at, u_int32_t *result) {
    int v;
    ssize_t r = pread(fd, &v, 4, *at);
    if (r<0) return errno;
    assert(r==4);
    *result = toku_dtoh32(v);
    (*at) += 4;
    return 0;
}

static int read_u_int64_t UU((int fd, toku_off_t *at, u_int64_t *result));
static int
read_u_int64_t (int fd, toku_off_t *at, u_int64_t *result) {
    u_int32_t v1=0,v2=0;
    int r;
    if ((r = read_int(fd, at, &v1))) return r;
    if ((r = read_int(fd, at, &v2))) return r;
    *result = (((u_int64_t)v1)<<32) + v2;
    return 0;
}

int toku_db_badformat(void) {
    return DB_BADFORMAT;
}

/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

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

int
maybe_preallocate_in_file (int fd, u_int64_t size)
// Effect: If file size is less than SIZE, make it bigger by either doubling it or growing by 16MB whichever is less.
// Return 0 on success, otherwise an error number.
{
    int64_t file_size;
    {
        int r = toku_os_get_file_size(fd, &file_size);
        assert(r==0);
    }
    assert(file_size >= 0);
    if ((u_int64_t)file_size < size) {
	const int N = umin64(size, 16<<20); // Double the size of the file, or add 16MB, whichever is less.
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

// Don't include the compressed data size or the uncompressed data size.

static const int brtnode_header_overhead = (8+   // magic "tokunode" or "tokuleaf"
					    4+   // nodesize
					    8+   // checkpoint number
					    4+   // target node size
 					    4+   // compressed data size
					    4+   // uncompressed data size
					    4+   // flags
					    4+   // height
					    4+   // random for fingerprint
					    4+   // localfingerprint
					    4);  // crc32 at the end

static int deserialize_fifo_at (int fd, toku_off_t at, FIFO *fifo);

static int
addupsize (OMTVALUE lev, u_int32_t UU(idx), void *vp) {
    LEAFENTRY le=lev;
    unsigned int *ip=vp;
    (*ip) += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
    return 0;
}

static unsigned int toku_serialize_brtnode_size_slow (BRTNODE node) {
    unsigned int size=brtnode_header_overhead;
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
	size+=(8+4+4+8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and the leafentry_estimate. */
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
	return size+hsize;
    }
}

// This is the size of the uncompressed data, including the uncompressed header, and including the 4 bytes for the information about how big is the compressed version, and how big is the uncompressed version.
unsigned int toku_serialize_brtnode_size (BRTNODE node) {
    unsigned int result =brtnode_header_overhead;
    assert(sizeof(toku_off_t)==8);
    if (node->height>0) {
	result+=4; /* subtree fingerpirnt */
	result+=4; /* n_children */
	result+=4*(node->u.n.n_children-1); /* key lengths*/
        if (node->flags & TOKU_DB_DUPSORT) result += 4*(node->u.n.n_children-1); /* data lengths */
	assert(node->u.n.totalchildkeylens < (1<<30));
	result+=node->u.n.totalchildkeylens; /* the lengths of the pivot keys, without their key lengths. */
	result+=(8+4+4+8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and the leafentry_estimate. */
	result+=node->u.n.n_bytes_in_buffers;
    } else {
	result+=4; /* n_entries in buffer table. */
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
				 ) };

enum { compression_header_len = (4   // compressed_len
				 +4 // uncompressed_len
				 ) };

static inline void ignore_int (int UU(ignore_me)) {}

int toku_serialize_brtnode_to (int fd, BLOCKNUM blocknum, BRTNODE node, struct brt_header *h, int n_workitems, int n_threads) {
    struct wbuf w;
    int i;
    unsigned int calculated_size = toku_serialize_brtnode_size(node) - 8; // don't include the compressed or uncompressed sizes
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
    wbuf_int(&w, BRT_LAYOUT_VERSION);
    wbuf_ulonglong(&w, node->log_lsn.lsn);
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
	    wbuf_ulonglong(&w, BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, i));
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
    //   compressed_len(4),[which includes only the compressed data]
    //   uncompressed_len(4)[which includes only the compressed data, not the header]

    // The first part of the data is uncompressed
    uLongf uncompressed_len = calculated_size-uncompressed_magic_len;
    uLongf compressed_len   = compressBound(uncompressed_len);
    char *MALLOC_N(compressed_len+uncompressed_magic_len+compression_header_len, compressed_buf);

    memcpy(compressed_buf, buf, uncompressed_magic_len);
    if (0) printf("First 4 bytes before compressing data are %02x%02x%02x%02x\n",
		  buf[uncompressed_magic_len],   buf[uncompressed_magic_len+1],
		  buf[uncompressed_magic_len+2], buf[uncompressed_magic_len+3]);
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
	int r = compress2(((Bytef*)compressed_buf)+uncompressed_magic_len + compression_header_len, &compressed_len,
			  ((Bytef*)buf)+uncompressed_magic_len, calculated_size-uncompressed_magic_len,
			  compression_level);
	assert(r==Z_OK);
    }

    if (0) printf("Block %" PRId64 " Size before compressing %u, after compression %lu\n", blocknum.b, calculated_size-uncompressed_magic_len, compressed_len);

    ((int32_t*)(compressed_buf+uncompressed_magic_len))[0] = toku_htonl(compressed_len);
    ((int32_t*)(compressed_buf+uncompressed_magic_len))[1] = toku_htonl(uncompressed_len);

    //write_now: printf("%s:%d Writing %d bytes\n", __FILE__, __LINE__, w.ndone);
    int r;
    {
	lock_for_pwrite();
//TODO: #1463 START (might not be the entire range
	// If the node has never been written, then write the whole buffer, including the zeros
	assert(blocknum.b>=0);
	//printf("%s:%d h=%p\n", __FILE__, __LINE__, h);
	//printf("%s:%d translated_blocknum_limit=%lu blocknum.b=%lu\n", __FILE__, __LINE__, h->translated_blocknum_limit, blocknum.b);
	//printf("%s:%d allocator=%p\n", __FILE__, __LINE__, h->block_allocator);
	//printf("%s:%d bt=%p\n", __FILE__, __LINE__, h->block_translation);
	h->dirty = 1; // Allocating a block dirties the header.
	size_t n_to_write = uncompressed_magic_len + compression_header_len + compressed_len;
	u_int64_t offset;
        toku_block_realloc(h->blocktable, blocknum, n_to_write, &offset);
	ssize_t n_wrote;
	r=toku_pwrite_extend(fd, compressed_buf, n_to_write, offset, &n_wrote);
	if (r) {
	    // fprintf(stderr, "%s:%d: Error writing data to file.  errno=%d (%s)\n", __FILE__, __LINE__, r, strerror(r));
	} else {
	    r=0;
	}
//TODO: #1463 END
	unlock_for_pwrite();
    }

    //printf("%s:%d wrote %d bytes for %lld size=%lld\n", __FILE__, __LINE__, w.ndone, off, size);
    assert(w.ndone==calculated_size);
    toku_free(buf);
    toku_free(compressed_buf);
    return r;
}

int toku_deserialize_brtnode_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h) {
    if (0) printf("Deserializing Block %" PRId64 "\n", blocknum.b);
    if (h->panic) return h->panic;
    DISKOFF offset, size;
    toku_block_get_offset_size(h->blocktable, blocknum, &offset, &size);
    TAGMALLOC(BRTNODE, result);
    struct rbuf rc;
    int i;
    int r;
    if (result==0) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }
    result->ever_been_written = 1;

    unsigned char *MALLOC_N(size, compressed_block);

    ssize_t rlen = pread(fd, compressed_block, size, offset);
    assert((DISKOFF)rlen == size);

    unsigned char *uncompressed_header = compressed_block;
    u_int32_t compressed_size   = toku_ntohl(*(u_int32_t*)(&uncompressed_header[uncompressed_magic_len]));
    if (compressed_size<=0   || compressed_size>(1<<30)) { r = toku_db_badformat(); goto died0; }
    u_int32_t uncompressed_size = toku_ntohl(*(u_int32_t*)(&uncompressed_header[uncompressed_magic_len+4]));
    if (0) printf("Block %" PRId64 " Compressed size = %u, uncompressed size=%u\n", blocknum.b, compressed_size, uncompressed_size);
    if (uncompressed_size<=0 || uncompressed_size>(1<<30)) { r = toku_db_badformat(); goto died0; }

    unsigned char *compressed_data = compressed_block + uncompressed_magic_len + compression_header_len;

    rc.size= uncompressed_size + uncompressed_magic_len;
    assert(rc.size>0);

    rc.buf=toku_malloc(rc.size);
    assert(rc.buf);

    memcpy(rc.buf, uncompressed_header, uncompressed_magic_len);
    {
	uLongf destlen = uncompressed_size;
	r = uncompress(rc.buf+uncompressed_magic_len, &destlen,
		       compressed_data, compressed_size);
	assert(destlen==uncompressed_size);
	assert(r==Z_OK);
    }
    if (0) printf("First 4 bytes of uncompressed data are %02x%02x%02x%02x\n",
		  rc.buf[uncompressed_magic_len],   rc.buf[uncompressed_magic_len+1],
		  rc.buf[uncompressed_magic_len+2], rc.buf[uncompressed_magic_len+3]);

    toku_free(compressed_block);

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
	case BRT_LAYOUT_VERSION_9: goto ok_layout_version;
	    // Don't support older versions.
	}
	r=toku_db_badformat();
	return r;
    ok_layout_version: ;
    }
    result->disk_lsn.lsn = rbuf_ulonglong(&rc);
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
	    BNC_SUBTREE_LEAFENTRY_ESTIMATE(result, i)=rbuf_ulonglong(&rc);
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
	u_int32_t end_of_data = rc.ndone;
	result->u.l.n_bytes_in_buffer += end_of_data-start_of_data + n_in_buf*OMT_ITEM_OVERHEAD;
	actual_sum *= result->rand4fingerprint;
	r = toku_omt_create_from_sorted_array(&result->u.l.buffer, array, n_in_buf);
	toku_free(array);
	if (r!=0) {
	    if (0) { died_21: toku_omt_destroy(&result->u.l.buffer); }
	    return toku_db_badformat();
	}
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
	uint32_t crc = x1764_memory(rc.buf, n_read_so_far);
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

int toku_serialize_brt_header_size (struct brt_header *h) {
    unsigned int size = (+8 // "tokudata"
			 +4 // size
			 +4 // version
			 +4 // tree's nodesize
			 +8 // free blocks
			 +8 // unused blocks
			 +4 // n_named_roots
			 +8 // max_blocknum_translated
			 +8 // block_translation_address_on_disk
			 );
    if (h->n_named_roots<0) {
	size+=(+8 // diskoff
	       +4 // flags
	       );
    } else {
	int i;
	for (i=0; i<h->n_named_roots; i++) {
	    size+=(+8 // root diskoff
		   +4 // flags
		   +4 // length of null terminated string (including null)
		   +1 + strlen(h->names[i]) // null-terminated string
		   );
	}
    }
    return size;
}

int toku_serialize_brt_header_to_wbuf (struct wbuf *wbuf, struct brt_header *h) {
    unsigned int size = toku_serialize_brt_header_size (h); // !!! seems silly to recompute the size when the caller knew it.  Do we really need the size?
    wbuf_literal_bytes(wbuf, "tokudata", 8);
    wbuf_int    (wbuf, size);
    wbuf_int    (wbuf, BRT_LAYOUT_VERSION);
    wbuf_int    (wbuf, h->nodesize);
    //TODO: Use 'prelocked/unlocked' versions to make this atomic
//TODO: #1463 START

    toku_block_realloc_translation_unlocked(h->blocktable);
    toku_block_wbuf_free_blocks_unlocked(h->blocktable, wbuf);
    toku_block_wbuf_unused_blocks_unlocked(h->blocktable, wbuf);
//TODO: #1463 END
    wbuf_int    (wbuf, h->n_named_roots);
//TODO: #1463 START
    //printf("%s:%d bta=%lu size=%lu\n", __FILE__, __LINE__, h->block_translation_address_on_disk, 4 + 16*h->translated_blocknum_limit);
    toku_block_wbuf_translated_blocknum_limit_unlocked(h->blocktable, wbuf);
    toku_block_wbuf_block_translation_address_on_disk_unlocked(h->blocktable, wbuf);
//TODO: #1463 END
    if (h->n_named_roots>=0) {
	int i;
	for (i=0; i<h->n_named_roots; i++) {
	    char *s = h->names[i];
	    unsigned int l = 1+strlen(s);
	    wbuf_BLOCKNUM(wbuf, h->roots[i]);
	    wbuf_int    (wbuf, h->flags_array[i]);
	    wbuf_bytes  (wbuf,  s, l);
	    assert(l>0 && s[l-1]==0);
	}
    } else {
	wbuf_BLOCKNUM(wbuf, h->roots[0]);
	wbuf_int    (wbuf, h->flags_array[0]);
    }
    assert(wbuf->ndone<=wbuf->size);
    return 0;
}

int toku_serialize_brt_header_to (int fd, struct brt_header *h) {
    int rr = 0;
    if (h->panic) return h->panic;
    lock_for_pwrite();
    toku_block_lock_for_multiple_operations();
    struct wbuf w_main;
    unsigned int size_main = toku_serialize_brt_header_size (h);
    {
	wbuf_init(&w_main, toku_malloc(size_main), size_main);
	{
	    int r=toku_serialize_brt_header_to_wbuf(&w_main, h);
	    assert(r==0);
	}
	assert(w_main.ndone==size_main);
    }
    struct wbuf w_translation;
    u_int64_t size_translation;
    u_int64_t address_translation;
    {
        toku_block_wbuf_init_and_fill_unlocked(h->blocktable, &w_translation,
                                               &size_translation, &address_translation);
        size_translation = w_translation.size;
    }
    toku_block_unlock_for_multiple_operations();
    {
        //Actual Write main header
	ssize_t nwrote;
	rr = toku_pwrite_extend(fd, w_main.buf, w_main.ndone, 0, &nwrote);
	toku_free(w_main.buf);
	if (rr) {
	    if (h->panic==0) {
		char *e = strerror(rr);
		int l = 200 + strlen(e);
		char s[l];
		h->panic=rr;
		snprintf(s, l-1, "%s:%d: Error writing header to data file.  errno=%d (%s)\n", __FILE__, __LINE__, rr, e);
		h->panic_string = toku_strdup(s);
	    }
	    goto finish;
	}
	assert((u_int64_t)nwrote==size_main);
    }
    {
        //Actual Write translation table
	ssize_t nwrote;
	rr = toku_pwrite_extend(fd, w_translation.buf,
                                size_translation, address_translation, &nwrote);
	if (rr) {
	    //fprintf(stderr, "%s:%d: Error writing data to file.  errno=%d (%s)\n", __FILE__, __LINE__, rr, strerror(rr));
	    goto finish;
	}
	assert((u_int64_t)nwrote==size_translation);
    }
 finish:
    toku_free(w_translation.buf);
    unlock_for_pwrite();
    return rr;
}

// We only deserialize brt header once and then share everything with all the brts.
static int
deserialize_brtheader (u_int32_t size, int fd, DISKOFF off, struct brt_header **brth) {
    // We already know the first 8 bytes are "tokudata", and we read in the size.
    struct brt_header *MALLOC(h);
    if (h==0) return errno;
    int ret=-1;
    if (0) { died0: toku_free(h); return ret; }
    struct rbuf rc;
    rc.buf = toku_malloc(size-12); // we can skip the first 12 bytes.
    if (rc.buf == NULL) { ret=errno; if (0) { died1: toku_free(rc.buf); } goto died0; }
    rc.size = size-12;
    if (rc.size<=0) { ret = EINVAL; goto died1; }
    rc.ndone = 0;
    {
	ssize_t r = pread(fd, rc.buf, size-12, off+12);
	if (r!=(ssize_t)size-12) { ret = EINVAL; goto died1; }
    }
    h->dirty=0;
    h->panic = 0;
    h->panic_string = 0;
    h->layout_version = rbuf_int(&rc);
    h->nodesize      = rbuf_int(&rc);
    assert(h->layout_version==BRT_LAYOUT_VERSION_9);
    BLOCKNUM free_blocks = rbuf_blocknum(&rc);
    BLOCKNUM unused_blocks = rbuf_blocknum(&rc);
    h->n_named_roots = rbuf_int(&rc);
    u_int64_t translated_blocknum_limit = rbuf_diskoff(&rc);
    u_int64_t block_translation_address_on_disk = rbuf_diskoff(&rc);
    u_int64_t block_translation_size_on_disk = 4 +//4 for checksum
                                               16*translated_blocknum_limit;
    // printf("%s:%d translated_blocknum_limit=%ld, block_translation_address_on_disk=%ld\n", __FILE__, __LINE__, h->translated_blocknum_limit, h->block_translation_address_on_disk);
    if (block_translation_address_on_disk == 0) {
        //There is no data on the disk.
        //Create empty translation table.
        toku_blocktable_create(&h->blocktable,
                               free_blocks, unused_blocks,
                               translated_blocknum_limit,
                               block_translation_address_on_disk,
                               block_translation_size_on_disk, NULL);
    }
    else {
        //Load translation table if it exists on disk.
	lock_for_pwrite();
        //TODO: #1463 load!
	unsigned char *XMALLOC_N(block_translation_size_on_disk, tbuf);
	{
	    ssize_t r = pread(fd, tbuf, block_translation_size_on_disk, block_translation_address_on_disk);
	    // This cast is messed up in 32-bits if the block translation table is ever more than 4GB.  But in that case, the translation table itself won't fit in main memory.
	    assert((u_int64_t)r==block_translation_size_on_disk);
	}
	{
	    // check the checksum
	    u_int32_t x1764 = x1764_memory(tbuf, block_translation_size_on_disk - 4);
	    u_int64_t offset = block_translation_size_on_disk - 4;
	    //printf("%s:%d read from %ld (x1764 offset=%ld) size=%ld\n", __FILE__, __LINE__, block_translation_address_on_disk, offset, block_translation_size_on_disk);
	    u_int32_t stored_x1764 = toku_ntohl(*(int*)(tbuf + offset));
	    assert(x1764 == stored_x1764);
	}
	// Create table and read in data.
        toku_blocktable_create(&h->blocktable,
                               free_blocks, unused_blocks,
                               translated_blocknum_limit,
                               block_translation_address_on_disk,
                               block_translation_size_on_disk,
                               tbuf);
	unlock_for_pwrite();
	toku_free(tbuf);
    }
    if (h->n_named_roots>=0) {
	int i;
	int n_to_malloc = (h->n_named_roots == 0) ? 1 : h->n_named_roots;
	MALLOC_N(n_to_malloc, h->flags_array); if (h->flags_array==0) { ret=errno; if (0) { died2: toku_free(h->flags_array); }                    goto died1; }
	MALLOC_N(n_to_malloc, h->roots);       if (h->roots==0)       { ret=errno; if (0) { died3: if (h->n_named_roots>=0) toku_free(h->roots); } goto died2; }
	MALLOC_N(n_to_malloc, h->root_hashes); if (h->root_hashes==0) { ret=errno; if (0) { died4: if (h->n_named_roots>=0) toku_free(h->root_hashes); } goto died3; }
	MALLOC_N(n_to_malloc, h->names);       if (h->names==0)       { ret=errno; if (0) { died5: if (h->n_named_roots>=0) toku_free(h->names); } goto died4; }
	for (i=0; i<h->n_named_roots; i++) {
	    h->root_hashes[i].valid = FALSE;
	    h->roots[i]       = rbuf_blocknum(&rc);
	    h->flags_array[i] = rbuf_int(&rc);
	    bytevec nameptr;
	    unsigned int len;
	    rbuf_bytes(&rc, &nameptr, &len);
	    assert(strlen(nameptr)+1==len);
	    h->names[i] = toku_memdup(nameptr, len);
	    assert(len == 0 || h->names[i] != NULL); // make sure the malloc worked.  Give up if this malloc failed...
	}
    } else {
	int n_to_malloc = 1;
	MALLOC_N(n_to_malloc, h->flags_array); if (h->flags_array==0) { ret=errno; goto died1; }
	MALLOC_N(n_to_malloc, h->roots);       if (h->roots==0) { ret=errno; goto died2; }
	MALLOC_N(n_to_malloc, h->root_hashes); if (h->root_hashes==0) { ret=errno; goto died3; }
	h->names = 0;
	h->roots[0] = rbuf_blocknum(&rc);
	h->root_hashes[0].valid = FALSE;
	h->flags_array[0] = rbuf_int(&rc);
    }
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died5;}
    toku_free(rc.buf);
    {
	int r;
	if ((r = deserialize_fifo_at(fd, toku_block_allocator_allocated_limit(h->blocktable), &h->fifo))) return r;
    }
    *brth = h;
    return 0;
}

int toku_deserialize_brtheader_from (int fd, BLOCKNUM blocknum, struct brt_header **brth) {
    //printf("%s:%d calling MALLOC\n", __FILE__, __LINE__);
    assert(blocknum.b==0);
    DISKOFF offset = 0;
    //printf("%s:%d malloced %p\n", __FILE__, __LINE__, h);

    char     magic[12];
    ssize_t r = pread(fd, magic,  12, offset);
    if (r==0) return -1;
    if (r<0)  return errno;
    if (r!=12) return EINVAL;
    assert(memcmp(magic,"tokudata",8)==0);
    // It's version 7 or later, and the magi clooks OK
    return deserialize_brtheader(toku_ntohl(*(int*)(&magic[8])), fd, offset, brth);
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

// To serialize the fifo, we just write it all at the end of the file.
// For now, just do all the writes as separate system calls.  This function is hardly ever called, and
// we might not be able to allocate a large enough buffer to hold everything,
// and it would be more complex to batch up several writes.
int toku_serialize_fifo_at (int fd, toku_off_t freeoff, FIFO fifo) {
    //printf("%s:%d Serializing fifo at %" PRId64 " (count=%d)\n", __FILE__, __LINE__, freeoff, toku_fifo_n_entries(fifo));
    lock_for_pwrite();
    {
	enum { size=4 };
	char buf[size];
	struct wbuf w;
	wbuf_init(&w, buf, size);
	wbuf_int(&w, toku_fifo_n_entries(fifo));
	ssize_t nwrote;
	int r = toku_pwrite_extend(fd, w.buf, size, freeoff, &nwrote);
	if (r) {
	    unlock_for_pwrite();
	    return r;
	}
	assert(nwrote==size);
	freeoff+=size;
    }
    FIFO_ITERATE(fifo, key, keylen, val, vallen, type, xid,
		 {
		     size_t size=keylen+vallen+1+8+4+4;
		     char  *MALLOC_N(size, buf);
		     assert(buf!=0);
		     struct wbuf w;
		     wbuf_init(&w, buf, size);
		     assert(type>=0 && type<256);
		     wbuf_char(&w, (unsigned char)type);
		     wbuf_TXNID(&w, xid);
		     wbuf_bytes(&w, key, keylen);
		     //printf("%s:%d Writing %d bytes: %s\n", __FILE__, __LINE__, vallen, (char*)val);
		     wbuf_bytes(&w, val, vallen);
		     assert(w.ndone==size);
		     ssize_t nwrote;
		     int r = toku_pwrite_extend(fd, w.buf, (size_t)size, freeoff, &nwrote);
		     if (r) {
			 unlock_for_pwrite();
			 return r;
		     }
		     assert((ssize_t)size==nwrote);
		     freeoff+=size;
		     toku_free(buf);
		 });
    unlock_for_pwrite();
    return 0;
}

static int
read_int (int fd, toku_off_t *at, u_int32_t *result) {
    int v;
    ssize_t r = pread(fd, &v, 4, *at);
    if (r<0) return errno;
    assert(r==4);
    *result = toku_ntohl(v);
    (*at) += 4;
    return 0;
}

static int
read_char (int fd, toku_off_t *at, char *result) {
    ssize_t r = pread(fd, result, 1, *at);
    if (r<0) return errno;
    assert(r==1);
    (*at)++;
    return 0;
}

static int
read_u_int64_t (int fd, toku_off_t *at, u_int64_t *result) {
    u_int32_t v1=0,v2=0;
    int r;
    if ((r = read_int(fd, at, &v1))) return r;
    if ((r = read_int(fd, at, &v2))) return r;
    *result = (((u_int64_t)v1)<<32) + v2;
    return 0;
}

static int
read_nbytes (int fd, toku_off_t *at, char **data, u_int32_t len) {
    char *result = toku_malloc(len);
    if (result==0) return errno;
    ssize_t r = pread(fd, result, len, *at);
    //printf("%s:%d read %d bytes at %" PRId64 ", which are %s\n", __FILE__, __LINE__, len, *at, result);
    if (r<0) return errno;
    assert(r==(ssize_t)len);
    (*at)+=len;
    *data=result;
    return 0;
}

static int deserialize_fifo_at (int fd, toku_off_t at, FIFO *fifo) {
    FIFO result;
    int r = toku_fifo_create(&result);
    if (r) return r;
    u_int32_t count=0;
    if ((r=read_int(fd, &at, &count))) return r;
    u_int32_t i;
    for (i=0; i<count; i++) {
	char type;
	TXNID xid;
	u_int32_t keylen=0, vallen=0;
	char *key=0, *val=0;
	if ((r=read_char(fd, &at, &type))) return r;
	if ((r=read_u_int64_t(fd, &at, &xid))) return r;
	if ((r=read_int(fd, &at, &keylen))) return r;
	if ((r=read_nbytes(fd, &at, &key, keylen))) return r;
	if ((r=read_int(fd, &at, &vallen))) return r;
	if ((r=read_nbytes(fd, &at, &val, vallen))) return r;
	//printf("%s:%d read %d byte key, key=%s\n dlen=%d data=%s\n", __FILE__, __LINE__, keylen, key, vallen, val);
	if ((r=toku_fifo_enq(result, key, keylen, val, vallen, type, xid))) return r;
	toku_free(key);
	toku_free(val);
    }
    *fifo = result;
    //printf("%s:%d *fifo=%p\n", __FILE__, __LINE__, result);
    return 0;
}

int toku_db_badformat(void) {
    return DB_BADFORMAT;
}

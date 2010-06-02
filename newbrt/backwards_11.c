/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt-serialize.c 18555 2010-03-18 01:20:07Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

#define PRINT 0


static u_int32_t x1764_memory_11 (const void *buf, int len)
{
    const u_int64_t *lbuf=buf;
    u_int64_t c=0;
    while (len>=8) {
	c = c*17 + *lbuf;
	if (PRINT) printf("%d: c=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, *lbuf, c);
	lbuf++;
	len-=8;
    }
    if (len>0) {
	const u_int8_t *cbuf=(u_int8_t*)lbuf;
	int i;
	u_int64_t input=0;
	for (i=0; i<len; i++) {
	    input |= ((u_int64_t)(cbuf[i]))<<(8*i);
	}
	c = c*17 + input;
    }
    return (c&0xFFFFFFFF) ^ (c>>32);
}

// Given a version 11 header, create a version 12 header.
// If new memory is needed for the new header, allocate it here and free the memory of the old version header.
static int
upgrade_brtheader_11_12(int fd, struct brt_header **brth_11, struct brt_header ** brth_12) {
    int r = 0;
    assert((*brth_11)->layout_version == BRT_LAYOUT_VERSION_11);
    *brth_12 = *brth_11;
    *brth_11 = NULL;
    (*brth_12)->layout_version = BRT_LAYOUT_VERSION_12;
    toku_list_init(&(*brth_12)->checkpoint_before_commit_link);
    (void) toku_sync_fetch_and_increment_uint64(&upgrade_status.header);
    { //Re-write descriptor to fix checksum (does not get done automatically).
        DISKOFF offset;
        DESCRIPTOR d = &(*brth_12)->descriptor;
        //4 for checksum
        toku_realloc_descriptor_on_disk((*brth_12)->blocktable, toku_serialize_descriptor_size(d)+4, &offset, (*brth_12));
        r = toku_serialize_descriptor_contents_to_fd(fd, d, offset);
    }
    return r;
}

// Structure of brtnode is same for versions 11, 12.  The only difference is in the 
// disk format and layout version.
static int
upgrade_brtnode_11_12 (BRTNODE *brtnode_11, BRTNODE *brtnode_12) {
    *brtnode_12 = *brtnode_11;
    *brtnode_11 = NULL;

    BRTNODE brt = *brtnode_12;
    brt->layout_version                = BRT_LAYOUT_VERSION_12;
    brt->dirty = 1;
    if (brt->height) {
	(void) toku_sync_fetch_and_increment_uint64(&upgrade_status.nonleaf);
    }
    else {
	(void) toku_sync_fetch_and_increment_uint64(&upgrade_status.leaf);
    }
    //x1764 calculation (fingerprint) has changed between 11 and 12.
    //Update all local fields based on x1764, verify several others.
    toku_verify_or_set_counts(brt, TRUE);
    return 0;
}


static u_int32_t
toku_serialize_descriptor_size_11(DESCRIPTOR desc) {
    //Checksum NOT included in this.  Checksum only exists in header's version.
    u_int32_t size = 4+ //version
                     4; //size
    size += desc->dbt.size;
    return size;
}


static unsigned int toku_brtnode_pivot_key_len_11 (BRTNODE node, struct kv_pair *pk) {
    if (node->flags & TOKU_DB_DUPSORT) {
	return kv_pair_keylen(pk) + kv_pair_vallen(pk);
    } else {
	return kv_pair_keylen(pk);
    }
}



enum { uncompressed_magic_len_11 = (8 // tokuleaf or tokunode
				 +4 // layout version
				 +4 // layout version original
				 ) 
};

// uncompressed header offsets
enum {
    uncompressed_magic_offset_11 = 0,
    uncompressed_version_offset_11 = 8,
};

// compression header sub block sizes
struct sub_block_sizes {
    u_int32_t compressed_size;        // real compressed size
    u_int32_t uncompressed_size; 
    u_int32_t compressed_size_bound;  // estimated compressed size
};

// target sub-block sizs and max number of sub-blocks per block.
static const int target_sub_block_size_11 = 512*1024;
static const int max_sub_blocks_11 = 8;

// round up n
static inline int roundup2(int n, int alignment) {
    return (n+alignment-1)&~(alignment-1);
}


// get the size of the compression header
static size_t get_compression_header_size(int UU(layout_version), int n) {
    return sizeof (u_int32_t) + (n * 2 * sizeof (u_int32_t));
}



// get the sum of the sub block uncompressed sizes 
static size_t get_sum_uncompressed_size_11(int n, struct sub_block_sizes sizes[]) {
    int i;
    size_t uncompressed_size = 0;
    for (i=0; i<n; i++) 
        uncompressed_size += sizes[i].uncompressed_size;
    return uncompressed_size;
}

static inline void ignore_int (int UU(ignore_me)) {}

static void deserialize_descriptor_from_rbuf_11(struct rbuf *rb, DESCRIPTOR desc, BOOL temporary);

static int
deserialize_brtnode_nonleaf_from_rbuf_11 (BRTNODE result, bytevec magic, struct rbuf *rb) {
    int r;
    int i;

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
    assert(result->u.n.n_children>=0);
    for (i=0; i<result->u.n.n_children; i++) {
        u_int32_t childfp = rbuf_int(rb);
        BNC_SUBTREE_FINGERPRINT(result, i)= childfp;
        check_subtree_fingerprint += childfp;
        struct subtree_estimates *se = &(BNC_SUBTREE_ESTIMATES(result, i));
        se->nkeys = rbuf_ulonglong(rb);
        se->ndata = rbuf_ulonglong(rb);
        se->dsize = rbuf_ulonglong(rb);
        se->exact = (BOOL) (rbuf_char(rb) != 0);
    }
    for (i=0; i<result->u.n.n_children-1; i++) {
        if (result->flags & TOKU_DB_DUPSORT) {
            bytevec keyptr, dataptr;
            unsigned int keylen, datalen;
            rbuf_bytes(rb, &keyptr, &keylen);
            rbuf_bytes(rb, &dataptr, &datalen);
            result->u.n.childkeys[i] = kv_pair_malloc(keyptr, keylen, dataptr, datalen);
        } else {
            bytevec childkeyptr;
            unsigned int cklen;
            rbuf_bytes(rb, &childkeyptr, &cklen); /* Returns a pointer into the rbuf. */
            result->u.n.childkeys[i] = kv_pair_malloc((void*)childkeyptr, cklen, 0, 0);
        }
        //printf(" key %d length=%d data=%s\n", i, result->childkeylens[i], result->childkeys[i]);
        result->u.n.totalchildkeylens+=toku_brtnode_pivot_key_len_11(result, result->u.n.childkeys[i]);
    }
    for (i=0; i<result->u.n.n_children; i++) {
        BNC_BLOCKNUM(result,i) = rbuf_blocknum(rb);
        BNC_HAVE_FULLHASH(result, i) = FALSE;
        BNC_NBYTESINBUF(result,i) = 0;
        //printf("Child %d at %lld\n", i, result->children[i]);
    }
    result->u.n.n_bytes_in_buffers = 0;
    for (i=0; i<result->u.n.n_children; i++) {
        r=toku_fifo_create(&BNC_BUFFER(result,i));
        if (r!=0) {
            int j;
            if (0) { died_1: j=result->u.n.n_bytes_in_buffers; }
            for (j=0; j<i; j++) toku_fifo_free(&BNC_BUFFER(result,j));
            return toku_db_badformat();
        }
    }
    {
        int cnum;
        u_int32_t check_local_fingerprint = 0;
        for (cnum=0; cnum<result->u.n.n_children; cnum++) {
            int n_in_this_hash = rbuf_int(rb);
            //printf("%d in hash\n", n_in_hash);
            for (i=0; i<n_in_this_hash; i++) {
                int diff;
                bytevec key; ITEMLEN keylen;
                bytevec val; ITEMLEN vallen;
                //toku_verify_counts_11(result);
                int type = rbuf_char(rb);
                XIDS xids;
                xids_create_from_buffer(rb, &xids);
                rbuf_bytes(rb, &key, &keylen); /* Returns a pointer into the rbuf. */
                rbuf_bytes(rb, &val, &vallen);
                check_local_fingerprint += result->rand4fingerprint * toku_calc_fingerprint_cmd(type, xids, key, keylen, val, vallen);
                //printf("Found %s,%s\n", (char*)key, (char*)val);
                {
                    r=toku_fifo_enq(BNC_BUFFER(result, cnum), key, keylen, val, vallen, type, xids); /* Copies the data into the hash table. */
                    if (r!=0) { goto died_1; }
                }
                diff = keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_get_serialize_size(xids);
                result->u.n.n_bytes_in_buffers += diff;
                BNC_NBYTESINBUF(result,cnum)   += diff;
                //printf("Inserted\n");
                xids_destroy(&xids);
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
    (void)rbuf_int(rb); //Ignore the crc (already verified).
    if (rb->ndone != rb->size) { //Verify we read exactly the entire block.
        r = toku_db_badformat(); goto died_1;
    }
    return 0;
}

static int
deserialize_brtnode_leaf_from_rbuf_11 (BRTNODE result, bytevec magic, struct rbuf *rb) {
    int r;
    int i;

    if (memcmp(magic, "tokuleaf", 8)!=0) {
        r = toku_db_badformat();
        return r;
    }

    result->u.l.leaf_stats.nkeys = rbuf_ulonglong(rb);
    result->u.l.leaf_stats.ndata = rbuf_ulonglong(rb);
    result->u.l.leaf_stats.dsize = rbuf_ulonglong(rb);
    result->u.l.leaf_stats.exact = TRUE;
    int n_in_buf = rbuf_int(rb);
    result->u.l.n_bytes_in_buffer = 0;
    result->u.l.seqinsert = 0;

    //printf("%s:%d r PMA= %p\n", __FILE__, __LINE__, result->u.l.buffer);
    toku_mempool_init(&result->u.l.buffer_mempool, rb->buf, rb->size);

    u_int32_t actual_sum = 0;
    u_int32_t start_of_data = rb->ndone;
    OMTVALUE *MALLOC_N(n_in_buf, array);
    for (i=0; i<n_in_buf; i++) {
        LEAFENTRY le = (LEAFENTRY)(&rb->buf[rb->ndone]);
        u_int32_t disksize = leafentry_disksize(le);
        rb->ndone += disksize;
        assert(rb->ndone<=rb->size);

        array[i]=(OMTVALUE)le;
        actual_sum += x1764_memory_11(le, disksize);
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
    assert(array==NULL);

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
    
    //toku_verify_counts_11(result);

    (void)rbuf_int(rb); //Ignore the crc (already verified).
    if (rb->ndone != rb->size) { //Verify we read exactly the entire block.
        r = toku_db_badformat(); goto died_1;
    }

    r = toku_leaflock_borrow(result->u.l.leaflock_pool, &result->u.l.leaflock);
    if (r!=0) goto died_1;
    rb->buf = NULL; //Buffer was used for node's mempool.
    return 0;
}


static int
deserialize_brtnode_from_rbuf_11 (BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h, struct rbuf *rb) {
    TAGMALLOC(BRTNODE, result);
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
    assert(result->layout_version == BRT_LAYOUT_VERSION_11);
    result->layout_version_original = rbuf_int(rb);
    result->layout_version_read_from_disk = result->layout_version;
    {
        //Restrict scope for now since we do not support upgrades.
        DESCRIPTOR_S desc;
        //desc.dbt.data is TEMPORARY.  Will be unusable when the rc buffer is freed.
        deserialize_descriptor_from_rbuf_11(rb, &desc, TRUE);
        //Just throw away.
    }
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
        r = deserialize_brtnode_nonleaf_from_rbuf_11(result, magic, rb);
    else {
        result->u.l.leaflock_pool = toku_cachefile_leaflock_pool(h->cf);
        r = deserialize_brtnode_leaf_from_rbuf_11(result, magic, rb);
    }
    if (r!=0) goto died0;

    //printf("%s:%d Ok got %lld n_children=%d\n", __FILE__, __LINE__, result->thisnodename, result->n_children);
    if (result->height>0) {
	// For height==0 we used the buf inside the OMT
	toku_free(rb->buf);
        rb->buf = NULL;
    }
    toku_trace("deserial done");
    *brtnode = result;
    //toku_verify_counts_11(result);
    return 0;
}

static int
verify_decompressed_brtnode_checksum (struct rbuf *rb) {
    int r = 0;

    if (rb->size >= 4) {
        uint32_t verify_size = rb->size - 4; //Not counting the checksum

        toku_trace("x1764 start");
        uint32_t crc = x1764_memory_11(rb->buf, verify_size);
        toku_trace("x1764");

        uint32_t *crcp = (uint32_t*)(((uint8_t*)rb->buf) + verify_size);
        uint32_t storedcrc = toku_dtoh32(*crcp);
        if (crc!=storedcrc) {
            printf("Bad CRC\n");
            printf("%s:%d crc=%08x stored=%08x\n", __FILE__, __LINE__, crc, storedcrc);
            r = toku_db_badformat();
        }
    }
    else r = toku_db_badformat();
    return r;
}

#define PAR_DECOMPRESS 1

#if PAR_DECOMPRESS

#include "workset.h"

struct decompress_work_11 {
    struct work base;
    void *compress_ptr;
    void *uncompress_ptr;
    u_int32_t compress_size;
    u_int32_t uncompress_size;
};

// initialize the decompression work
static void 
decompress_work_init_11(struct decompress_work_11 *dw,
                     void *compress_ptr, u_int32_t compress_size,
                     void *uncompress_ptr, u_int32_t uncompress_size) {
    dw->compress_ptr = compress_ptr; 
    dw->compress_size = compress_size;
    dw->uncompress_ptr = uncompress_ptr; 
    dw->uncompress_size = uncompress_size;
}

// decompress one block
static void 
decompress_block(struct decompress_work_11 *dw) {
    if (0) {
        toku_pthread_t self = toku_pthread_self();
        printf("%s:%d %x %p\n", __FUNCTION__, __LINE__, *(int*) &self, dw);
    }
    uLongf destlen = dw->uncompress_size;
    int r = uncompress(dw->uncompress_ptr, &destlen, dw->compress_ptr, dw->compress_size);
    assert(destlen == dw->uncompress_size);
    assert(r==Z_OK);
}

// decompress blocks until there is no more work to do
static void *
decompress_worker_11(void *arg) {
    struct workset *ws = (struct workset *) arg;
    while (1) {
        struct decompress_work_11 *dw = (struct decompress_work_11 *) workset_get(ws);
        if (dw == NULL)
            break;
        decompress_block(dw);
    }
    return arg;
}

#else

#define DO_DECOMPRESS_WORKER 0

struct decompress_work_11 {
    toku_pthread_t id;
    void *compress_ptr;
    void *uncompress_ptr;
    u_int32_t compress_size;
    u_int32_t uncompress_size;
};

// initialize the decompression work
static void init_decompress_work(struct decompress_work_11 *w,
                                 void *compress_ptr, u_int32_t compress_size,
                                 void *uncompress_ptr, u_int32_t uncompress_size) {
    memset(&w->id, 0, sizeof(w->id));
    w->compress_ptr = compress_ptr; w->compress_size = compress_size;
    w->uncompress_ptr = uncompress_ptr; w->uncompress_size = uncompress_size;
}

// do the decompression work
static void do_decompress_work(struct decompress_work_11 *w) {
    uLongf destlen = w->uncompress_size;
    int r = uncompress(w->uncompress_ptr, &destlen,
                       w->compress_ptr, w->compress_size);
    assert(destlen==w->uncompress_size);
    assert(r==Z_OK);
}

#if DO_DECOMPRESS_WORKER

static void *decompress_worker_11(void *);

static void start_decompress_work(struct decompress_work_11 *w) {
    int r = toku_pthread_create(&w->id, NULL, decompress_worker_11, w); assert(r == 0);
}

static void wait_decompress_work(struct decompress_work_11 *w) {
    void *ret;
    int r = toku_pthread_join(w->id, &ret); assert(r == 0);
}

static void *decompress_worker_11(void *arg) {
    struct decompress_work_11 *w = (struct decompress_work_11 *) arg;
    do_decompress_work(w);
    return arg;
}

#endif

#endif

static int
decompress_brtnode_from_raw_block_into_rbuf_11(u_int8_t *raw_block, struct rbuf *rb, BLOCKNUM blocknum) {
    int r;
    int i;
    // get the number of compressed sub blocks
    int n_sub_blocks;
    int compression_header_offset;
    {
        n_sub_blocks = toku_dtoh32(*(u_int32_t*)(&raw_block[uncompressed_magic_len_11]));
        compression_header_offset = uncompressed_magic_len_11 + 4;
    }
    assert(0 < n_sub_blocks);

    // verify the sizes of the compressed sub blocks
    if (0 && n_sub_blocks != 1) printf("%s:%d %d\n", __FUNCTION__, __LINE__, n_sub_blocks);

    struct sub_block_sizes sub_block_sizes[n_sub_blocks];
    for (i=0; i<n_sub_blocks; i++) {
        u_int32_t compressed_size = toku_dtoh32(*(u_int32_t*)(&raw_block[compression_header_offset+8*i]));
        if (compressed_size<=0   || compressed_size>(1<<30)) { r = toku_db_badformat(); return r; }
        u_int32_t uncompressed_size = toku_dtoh32(*(u_int32_t*)(&raw_block[compression_header_offset+8*i+4]));
        if (0) printf("Block %" PRId64 " Compressed size = %u, uncompressed size=%u\n", blocknum.b, compressed_size, uncompressed_size);
        if (uncompressed_size<=0 || uncompressed_size>(1<<30)) { r = toku_db_badformat(); return r; }

        sub_block_sizes[i].compressed_size = compressed_size;
        sub_block_sizes[i].uncompressed_size = uncompressed_size;
    }

    unsigned char *compressed_data = raw_block + uncompressed_magic_len_11 + get_compression_header_size(BRT_LAYOUT_VERSION_11, n_sub_blocks);

    size_t uncompressed_size = get_sum_uncompressed_size_11(n_sub_blocks, sub_block_sizes);
    rb->size= uncompressed_magic_len_11 + uncompressed_size;
    assert(rb->size>0);

    rb->buf=toku_xmalloc(rb->size);

    // construct the uncompressed block from the header and compressed sub blocks
    memcpy(rb->buf, raw_block, uncompressed_magic_len_11);

#if PAR_DECOMPRESS
    // compute the number of additional threads needed for decompressing this node
    int T = num_cores; // T = min(#cores, #blocks) - 1
    if (T > n_sub_blocks)
        T = n_sub_blocks;
    if (T > 0)
        T = T - 1;       // threads in addition to the running thread

    // init the decompression work set
    struct workset ws;
    workset_init(&ws);

    // initialize the decompression work and add to the work set
    unsigned char *uncompressed_data = rb->buf+uncompressed_magic_len_11;
    struct decompress_work_11 decompress_work_11[n_sub_blocks];
    workset_lock(&ws);
    for (i = 0; i < n_sub_blocks; i++) {
        decompress_work_init_11(&decompress_work_11[i], compressed_data, sub_block_sizes[i].compressed_size, uncompressed_data, sub_block_sizes[i].uncompressed_size);
        uncompressed_data += sub_block_sizes[i].uncompressed_size;
        compressed_data += sub_block_sizes[i].compressed_size;
        workset_put_locked(&ws, &decompress_work_11[i].base);
    }
    workset_unlock(&ws);
    
    // decompress the sub-blocks
    if (0) printf("%s:%d Cores=%d Blocks=%d T=%d\n", __FUNCTION__, __LINE__, num_cores, n_sub_blocks, T);
    toku_pthread_t tids[T];
    threadset_create(tids, &T, decompress_worker_11, &ws);
    decompress_worker_11(&ws);

    // cleanup
    threadset_join(tids, T);
    workset_destroy(&ws);

#else
    // decompress the sub blocks
    unsigned char *uncompressed_data = rb->buf+uncompressed_magic_len_11;
    struct decompress_work_11 decompress_work_11[n_sub_blocks];

    for (i=0; i<n_sub_blocks; i++) {
        init_decompress_work(&decompress_work_11[i], compressed_data, sub_block_sizes[i].compressed_size, uncompressed_data, sub_block_sizes[i].uncompressed_size);
        if (i>0) {
#if DO_DECOMPRESS_WORKER
            start_decompress_work(&decompress_work_11[i]);
#else
            do_decompress_work(&decompress_work_11[i]);
#endif
        }
        uncompressed_data += sub_block_sizes[i].uncompressed_size;
        compressed_data += sub_block_sizes[i].compressed_size;
    }
    do_decompress_work(&decompress_work_11[0]);
#if DO_DECOMPRESS_WORKER
    for (i=1; i<n_sub_blocks; i++)
        wait_decompress_work(&decompress_work_11[i]);
#endif

#endif

    toku_trace("decompress done");

    if (0) printf("First 4 bytes of uncompressed data are %02x%02x%02x%02x\n",
		  rb->buf[uncompressed_magic_len_11],   rb->buf[uncompressed_magic_len_11+1],
		  rb->buf[uncompressed_magic_len_11+2], rb->buf[uncompressed_magic_len_11+3]);

    rb->ndone=0;

    r = verify_decompressed_brtnode_checksum(rb);
    return r;
}







// ################


static void
deserialize_descriptor_from_rbuf_11(struct rbuf *rb, DESCRIPTOR desc, BOOL temporary) {
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
deserialize_descriptor_from_11(int fd, struct brt_header *h, DESCRIPTOR desc) {
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
                u_int32_t x1764 = x1764_memory_11(dbuf, size-4);
                //printf("%s:%d read from %ld (x1764 offset=%ld) size=%ld\n", __FILE__, __LINE__, block_translation_address_on_disk, offset, block_translation_size_on_disk);
                u_int32_t stored_x1764 = toku_dtoh32(*(int*)(dbuf + size-4));
                assert(x1764 == stored_x1764);
            }
            {
                struct rbuf rb = {.buf = dbuf, .size = size, .ndone = 0};
                //Not temporary; must have a toku_memdup'd copy.
                deserialize_descriptor_from_rbuf_11(&rb, desc, FALSE);
            }
            assert(toku_serialize_descriptor_size_11(desc)+4 == size);
            toku_free(dbuf);
        }
    }
}


// We only deserialize brt header once and then share everything with all the brts.
static int
deserialize_brtheader_11 (int fd, struct rbuf *rb, struct brt_header **brth) {
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
    toku_list_init(&h->live_brts);
    toku_list_init(&h->zombie_brts);
    //version MUST be in network order on disk regardless of disk order
    h->layout_version = rbuf_network_int(&rc);
    //TODO: #1924
    assert(h->layout_version==BRT_LAYOUT_VERSION_11);

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
                                           tbuf,
                                           TRUE);
        toku_free(tbuf);
    }

    h->root = rbuf_blocknum(&rc);
    h->root_hash.valid = FALSE;
    h->flags = rbuf_int(&rc);
    deserialize_descriptor_from_11(fd, h, &h->descriptor);
    h->layout_version_original = rbuf_int(&rc);    
    (void)rbuf_int(&rc); //Read in checksum and ignore (already verified).
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died1;}
    toku_free(rc.buf);
    rc.buf = NULL;
    *brth = h;
    return 0;
}








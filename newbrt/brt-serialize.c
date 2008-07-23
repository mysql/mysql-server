/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_assert.h"
#include "brt-internal.h"
#include "key.h"
#include "rbuf.h"
#include "wbuf.h"
#include "kv-pair.h"
#include "mempool.h"

#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>


static const int brtnode_header_overhead = (8+   // magic "tokunode" or "tokuleaf"
					    4+   // nodesize
					    8+   // checkpoint number
					    4+   // block size
					    4+   // data size
					    4+   // flags
					    4+   // height
					    4+   // random for fingerprint
					    4+   // localfingerprint
					    4);  // crc32 at the end

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
	int addupsize (OMTVALUE lev, u_int32_t UU(idx), void *vp) {
	    LEAFENTRY le=lev;
	    unsigned int *ip=vp;
	    (*ip) += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
	    return 0;
	}
	toku_omt_iterate(node->u.l.buffer,
			 addupsize,
			 &hsize);
	assert(hsize<=node->u.l.n_bytes_in_buffer);
	hsize+=4; /* add n entries in buffer table. */
	return size+hsize;
    }
}

unsigned int toku_serialize_brtnode_size (BRTNODE node) {
    unsigned int result =brtnode_header_overhead;
    assert(sizeof(off_t)==8);
    if (node->height>0) {
	result+=4; /* n_children */
	result+=4; /* subtree fingerpirnt */
	result+=4*(node->u.n.n_children-1); /* key lengths*/
        if (node->flags & TOKU_DB_DUPSORT) result += 4*(node->u.n.n_children-1); /* data lengths */
	result+=node->u.n.totalchildkeylens; /* the lengths of the pivot keys, without their key lengths. */
	result+=(8+4+4+8)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, the subtree fingerprint, and the leafentry_estimate. */
	result+=node->u.n.n_bytes_in_buffers;
    } else {
	result+=4; /* n_entries in buffer table. */
	result+=node->u.l.n_bytes_in_buffer;
	if (toku_memory_check) {
	    unsigned int slowresult = toku_serialize_brtnode_size_slow(node);
	    if (result!=slowresult) printf("%s:%d result=%d slowresult=%d\n", __FILE__, __LINE__, result, slowresult);
	    assert(result==slowresult);
	}
    }
    return result;
}

void toku_serialize_brtnode_to (int fd, DISKOFF off, BRTNODE node) {
    //printf("%s:%d serializing\n", __FILE__, __LINE__);
    struct wbuf w;
    int i;
    unsigned int calculated_size = toku_serialize_brtnode_size(node);
#if 0
    if (calculated_size!=toku_serialize_brtnode_size(node)) {
	//printf("Sizes don't match: %d %d\n", calculated_size, toku_serialize_brtnode_size(node));
    }
#endif
    //assert(calculated_size<=size);
    //char buf[size];
    size_t n_to_write = node->ever_been_written ? calculated_size : node->nodesize;
    char *MALLOC_N(n_to_write, buf);
    //toku_verify_counts(node);
    //assert(size>0);
    //printf("%s:%d serializing %lld w height=%d p0=%p\n", __FILE__, __LINE__, off, node->height, node->mdicts[0]);
    wbuf_init(&w, buf, node->nodesize);
    wbuf_literal_bytes(&w, "toku", 4);
    if (node->height==0) wbuf_literal_bytes(&w, "leaf", 4);
    else wbuf_literal_bytes(&w, "node", 4);
    wbuf_int(&w, BRT_LAYOUT_VERSION_7);
    wbuf_ulonglong(&w, node->log_lsn.lsn);
    //printf("%s:%d %lld.calculated_size=%d\n", __FILE__, __LINE__, off, calculated_size);
    wbuf_uint(&w, calculated_size);
    wbuf_int(&w, node->nodesize);
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
	    wbuf_DISKOFF(&w, BNC_DISKOFF(node,i));
	    //printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
	}

	{
	    int n_buffers = node->u.n.n_children;
	    u_int32_t check_local_fingerprint = 0;
	    for (i=0; i< n_buffers; i++) {
		//printf("%s:%d p%d=%p n_entries=%d\n", __FILE__, __LINE__, i, node->mdicts[i], mdict_n_entries(node->mdicts[i]));
		wbuf_int(&w, toku_fifo_n_entries(BNC_BUFFER(node,i)));
		FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
				  ({
				      wbuf_char(&w, type);
				      wbuf_TXNID(&w, xid);
				      wbuf_bytes(&w, key, keylen);
				      wbuf_bytes(&w, data, datalen);
				      check_local_fingerprint+=node->rand4fingerprint*toku_calccrc32_cmd(type, xid, key, keylen, data, datalen);
				  }));
	    }
	    //printf("%s:%d check_local_fingerprint=%8x\n", __FILE__, __LINE__, check_local_fingerprint);
	    if (check_local_fingerprint!=node->local_fingerprint) printf("%s:%d node=%lld fingerprint expected=%08x actual=%08x\n", __FILE__, __LINE__, (long long)node->thisnodename, check_local_fingerprint, node->local_fingerprint);
	    assert(check_local_fingerprint==node->local_fingerprint);
	}
    } else {
	//printf("%s:%d writing node %lld n_entries=%d\n", __FILE__, __LINE__, node->thisnodename, toku_gpma_n_entries(node->u.l.buffer));
	wbuf_uint(&w, toku_omt_size(node->u.l.buffer));
	int wbufwriteleafentry (OMTVALUE lev, u_int32_t UU(idx), void *v) {
	    LEAFENTRY le=lev;
	    struct wbuf *thisw=v;
	    wbuf_LEAFENTRY(thisw, le);
	    return 0;
	}
	toku_omt_iterate(node->u.l.buffer, wbufwriteleafentry, &w);
    }
    assert(w.ndone<=w.size);
#ifdef CRC_ATEND
    wbuf_int(&w, crc32(toku_null_crc, w.buf, w.ndone)); 
#endif
#ifdef CRC_INCR
    wbuf_uint(&w, w.crc32);
#endif

    if (!node->ever_been_written)
	memset(w.buf+w.ndone, 0, (size_t)(node->nodesize-w.ndone)); // fill with zeros

    //write_now: printf("%s:%d Writing %d bytes\n", __FILE__, __LINE__, w.ndone);
    {
	// If the node has never been written, then write the whole buffer, including the zeros
	//size_t n_to_write = node->nodesize;
	ssize_t r=pwrite(fd, w.buf, n_to_write, off);
	if (r<0) printf("r=%ld errno=%d\n", (long)r, errno);
	assert(r==(ssize_t)n_to_write);
    }

    if (calculated_size!=w.ndone)
	printf("%s:%d w.done=%d calculated_size=%d\n", __FILE__, __LINE__, w.ndone, calculated_size);
    assert(calculated_size==w.ndone);

    //printf("%s:%d wrote %d bytes for %lld size=%lld\n", __FILE__, __LINE__, w.ndone, off, size);
    assert(w.ndone<=node->nodesize);
    toku_free(buf);
}

int toku_deserialize_brtnode_from (int fd, DISKOFF off, u_int32_t fullhash, BRTNODE *brtnode) {
    TAGMALLOC(BRTNODE, result);
    struct rbuf rc;
    int i;
    u_int32_t datasize;
    int r;
    if (result==0) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }
    result->ever_been_written = 1; 
    {
	u_int32_t datasize_n;
	r = pread(fd, &datasize_n, sizeof(datasize_n), off +8+4+8);
	//printf("%s:%d r=%d the datasize=%d\n", __FILE__, __LINE__, r, ntohl(datasize_n));
	if (r!=sizeof(datasize_n)) {
	    if (r==-1) r=errno;
	    else r = DB_BADFORMAT;
	    goto died0;
	}
	datasize = ntohl(datasize_n);
	if (datasize<=0 || datasize>(1<<30)) { r = DB_BADFORMAT; goto died0; }
    }
    rc.buf=toku_malloc(datasize);
    //printf("%s:%d errno=%d\n", __FILE__, __LINE__, errno);
    if (rc.buf==0) {
	r=errno;
	if (0) { died1: toku_free(rc.buf); }
	goto died0;
    }
    rc.size=datasize;
    assert(rc.size>0);
    rc.ndone=0;
    //printf("Deserializing %lld datasize=%d\n", off, datasize);
    {
	ssize_t rlen=pread(fd, rc.buf, datasize, off);
	//printf("%s:%d pread->%d datasize=%d\n", __FILE__, __LINE__, r, datasize);
	if ((size_t)rlen!=datasize) {
	    //printf("%s:%d size messed up\n", __FILE__, __LINE__);
	    r=errno;
	    goto died1;
	}
	//printf("Got %d %d %d %d\n", rc.buf[0], rc.buf[1], rc.buf[2], rc.buf[3]);
    }
    {
	bytevec tmp;
	rbuf_literal_bytes(&rc, &tmp, 8);
	if (memcmp(tmp, "tokuleaf", 8)!=0
	    && memcmp(tmp, "tokunode", 8)!=0) {
	    r = DB_BADFORMAT;
	    goto died1;
	}
    }
    result->layout_version    = rbuf_int(&rc);
    {
	switch (result->layout_version) {
	case BRT_LAYOUT_VERSION_5:
	case BRT_LAYOUT_VERSION_6:
	case BRT_LAYOUT_VERSION_7: goto ok_layout_version;
	}
	r=DB_BADFORMAT;
	goto died1;
    ok_layout_version: ;
    }
    result->disk_lsn.lsn = rbuf_ulonglong(&rc);
    result->log_lsn = result->disk_lsn;
    {
	unsigned int stored_size = rbuf_int(&rc);
	if (stored_size!=datasize) { r=DB_BADFORMAT; goto died1; }
    }
    result->nodesize = rbuf_int(&rc);
    result->thisnodename = off;
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
	assert(result->u.n.n_children>=0 && result->u.n.n_children<=TREE_FANOUT);
	for (i=0; i<result->u.n.n_children; i++) {
	    u_int32_t childfp = rbuf_int(&rc);
	    BNC_SUBTREE_FINGERPRINT(result, i)= childfp;
	    check_subtree_fingerprint += childfp;
	    if (result->layout_version>BRT_LAYOUT_VERSION_5) {
		BNC_SUBTREE_LEAFENTRY_ESTIMATE(result, i)=rbuf_ulonglong(&rc);
	    } else {
		BNC_SUBTREE_LEAFENTRY_ESTIMATE(result, i)=0;
	    }
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
	    BNC_DISKOFF(result,i) = rbuf_diskoff(&rc);
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
		goto died1;
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
		    check_local_fingerprint += result->rand4fingerprint * toku_calccrc32_cmd(type, xid, key, keylen, val, vallen);
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
		return DB_BADFORMAT;
	    }
	    if (check_subtree_fingerprint+check_local_fingerprint != subtree_fingerprint) {
		fprintf(stderr, "%s:%d subtree fingerprint is wrong\n", __FILE__, __LINE__);
		return DB_BADFORMAT;
	    }
	}
    } else {
	int n_in_buf = rbuf_int(&rc);
	result->u.l.n_bytes_in_buffer = 0;
        result->u.l.seqinsert = 0;

	//printf("%s:%d r PMA= %p\n", __FILE__, __LINE__, result->u.l.buffer); 
	toku_mempool_init(&result->u.l.buffer_mempool, rc.buf, datasize);

	u_int32_t actual_sum = 0;
	u_int32_t start_of_data = rc.ndone;
	OMTVALUE *MALLOC_N(n_in_buf, array);
	for (i=0; i<n_in_buf; i++) {
	    LEAFENTRY le = (LEAFENTRY)(&rc.buf[rc.ndone]);
	    u_int32_t disksize = leafentry_disksize(le);
	    rc.ndone += disksize;
	    assert(rc.ndone<=rc.size);

	    array[i]=(OMTVALUE)le;
	    actual_sum += toku_crc32(toku_null_crc, le, disksize);
	}
	u_int32_t end_of_data = rc.ndone;
	result->u.l.n_bytes_in_buffer += end_of_data-start_of_data + n_in_buf*OMT_ITEM_OVERHEAD;
	actual_sum *= result->rand4fingerprint;
	r = toku_omt_create_from_sorted_array(&result->u.l.buffer, array, n_in_buf);
	toku_free(array);
	if (r!=0) {
	    if (0) { died_21: toku_omt_destroy(&result->u.l.buffer); }
	    goto died1;
	}

	result->u.l.buffer_mempool.frag_size = start_of_data;
	result->u.l.buffer_mempool.free_offset = end_of_data;

	if (r!=0) goto died_21;
	if (actual_sum!=result->local_fingerprint) {
	    //fprintf(stderr, "%s:%d Corrupted checksum stored=%08x rand=%08x actual=%08x height=%d n_keys=%d\n", __FILE__, __LINE__, result->rand4fingerprint, result->local_fingerprint, actual_sum, result->height, n_in_buf);
	    return DB_BADFORMAT;
	    goto died_21;
	} else {
	    //fprintf(stderr, "%s:%d Good checksum=%08x height=%d\n", __FILE__, __LINE__, actual_sum, result->height);
	}
	    
	//toku_verify_counts(result);
    }
    {
	unsigned int n_read_so_far = rc.ndone;
	if (n_read_so_far+4!=rc.size) {
	    r = DB_BADFORMAT; goto died_21;
	}
	uint32_t crc = toku_crc32(toku_null_crc, rc.buf, n_read_so_far);
	uint32_t storedcrc = rbuf_int(&rc);
	if (crc!=storedcrc) {
	    printf("Bad CRC\n");
	    assert(0);//this is wrong!!!
	    r = DB_BADFORMAT;
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

void toku_verify_counts (BRTNODE node) {
    /*foo*/
    if (node->height==0) {
	assert(node->u.l.buffer);
	struct sum_info {
	    unsigned int dsum;
	    unsigned int msum;
	    unsigned int count;
	    u_int32_t    fp;
	} sum_info = {0,0,0,0};
	int sum_item (OMTVALUE lev, u_int32_t UU(idx), void *vsi) {
	    LEAFENTRY le=lev;
	    struct sum_info *si = vsi;
	    si->count++;
	    si->dsum += OMT_ITEM_OVERHEAD + leafentry_disksize(le);
	    si->msum += leafentry_memsize(le);
	    si->fp  += toku_le_crc(le);
	    return 0;
	}
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
			 +4 // tree's nodesize
			 +4 // version
			 +8 // freelist
			 +8 // unused memory
			 +4); // n_named_roots
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
    wbuf_DISKOFF(wbuf, h->freelist);
    wbuf_DISKOFF(wbuf, h->unused_memory);
    wbuf_int    (wbuf, h->n_named_roots);
    if (h->n_named_roots>=0) {
	int i;
	for (i=0; i<h->n_named_roots; i++) {
	    char *s = h->names[i];
	    unsigned int l = 1+strlen(s);
	    wbuf_DISKOFF(wbuf, h->roots[i]);
	    wbuf_int    (wbuf, h->flags_array[i]);
	    wbuf_bytes  (wbuf,  s, l);
	    assert(l>0 && s[l-1]==0);
	}
    } else {
	wbuf_DISKOFF(wbuf, h->roots[0]);
	wbuf_int    (wbuf, h->flags_array[0]);
    }
    assert(wbuf->ndone<=wbuf->size);
    return 0;
}

int toku_serialize_brt_header_to (int fd, struct brt_header *h) {
    struct wbuf w;
    unsigned int size = toku_serialize_brt_header_size (h);
    wbuf_init(&w, toku_malloc(size), size);
    int r=toku_serialize_brt_header_to_wbuf(&w, h);
    assert(w.ndone==size);
    {
	ssize_t nwrote = pwrite(fd, w.buf, w.ndone, 0);
	if (nwrote<0) perror("pwrite");
	assert((size_t)nwrote==w.ndone);
    }
    toku_free(w.buf);
    return r;
}

static int deserialize_brtheader_6_or_earlier (int fd, DISKOFF off, struct brt_header **brth, u_int32_t fullhash) {
    // Deserialize a brt header from version 6 or earlier.
    struct brt_header *MALLOC(h);
    if (h==0) return errno;
    h->fullhash = fullhash;
    int ret=-1;
    if (0) { died0: toku_free(h); return ret; }
    int size;
    int sizeagain;
    h->layout_version = BRT_LAYOUT_VERSION_6;
    {
	uint32_t size_n;
	ssize_t r = pread(fd, &size_n, sizeof(size_n), off);
	assert(r==sizeof(size_n)); // we already read it earlier.
	size = ntohl(size_n);
    }
    struct rbuf rc;
    rc.buf = toku_malloc(size);
    if (rc.buf == NULL) { ret = ENOMEM; goto died0; }
    if (0) { died1: toku_free(rc.buf); goto died0; }
    rc.size=size;
    if (rc.size<=0) {ret = EINVAL; goto died1;}
    rc.ndone=0;
    {
	ssize_t r = pread(fd, rc.buf, size, off);
	if (r!=size) {ret = EINVAL; goto died1;}
    }
    h->dirty=0;
    sizeagain        = rbuf_int(&rc);
    if (sizeagain!=size) {ret = EINVAL; goto died1;}
    u_int32_t flags_for_all = rbuf_int(&rc);
    h->nodesize      = rbuf_int(&rc);
    h->freelist      = rbuf_diskoff(&rc);
    h->unused_memory = rbuf_diskoff(&rc);
    h->n_named_roots = rbuf_int(&rc);
    if (h->n_named_roots>=0) {
	int i;
	MALLOC_N(h->n_named_roots, h->flags_array);
	for (i=0; i<h->n_named_roots; i++) h->flags_array[i]=flags_for_all;
	MALLOC_N(h->n_named_roots, h->roots);
	MALLOC_N(h->n_named_roots, h->root_hashes);
	if (h->n_named_roots > 0 && (h->roots == NULL || h->root_hashes==NULL)) {ret = ENOMEM; goto died1;}
	if (0) {
	died2:
	    toku_free(h->roots);
	    toku_free(h->root_hashes);
	    goto died1;
	}
	MALLOC_N(h->n_named_roots, h->names);
	if (h->n_named_roots > 0 && h->names == NULL) {ret = ENOMEM; goto died2;}
	if (0) {
	died3:
	    toku_free(h->names);
	    for (i = 0; i < h->n_named_roots; i++) {
		if (h->names[i]) toku_free(h->names[i]);
	    }
	    goto died2;
	}
	
	for (i=0; i<h->n_named_roots; i++) {
	    bytevec nameptr;
	    unsigned int len;
	    h->root_hashes[i].valid = FALSE;
	    h->roots[i] = rbuf_diskoff(&rc);
	    rbuf_bytes(&rc, &nameptr, &len);
	    if (strlen(nameptr)+1!=len) {ret = EINVAL; goto died3;}
	    h->names[i] = toku_memdup(nameptr,len);
	    if (len > 0 && h->names[i] == NULL) {ret = ENOMEM; goto died3;}
	}
	
    } else {
	MALLOC_N(1, h->flags_array);
	MALLOC_N(1, h->roots);
	MALLOC_N(1, h->root_hashes);
	h->flags_array[0]=flags_for_all;
	h->roots[0] = rbuf_diskoff(&rc);
	h->root_hashes[0].valid = FALSE;
	h->names = 0;
    }
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died3;}
    toku_free(rc.buf);
    *brth = h;
    return 0;
}

int deserialize_brtheader_7_or_later(u_int32_t size, int fd, DISKOFF off, struct brt_header **brth, u_int32_t fullhash) {
    // We already know the first 8 bytes are "tokudata", and we read in the size.
    struct brt_header *MALLOC(h);
    if (h==0) return errno;
    int ret=-1;
    h->fullhash = fullhash;
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
    h->layout_version = rbuf_int(&rc);
    h->nodesize      = rbuf_int(&rc);
    assert(h->layout_version==BRT_LAYOUT_VERSION_7);
    h->freelist      = rbuf_diskoff(&rc);
    h->unused_memory = rbuf_diskoff(&rc);
    h->n_named_roots = rbuf_int(&rc);
    if (h->n_named_roots>=0) {
	int i;
	int n_to_malloc = (h->n_named_roots == 0) ? 1 : h->n_named_roots;
	MALLOC_N(n_to_malloc, h->flags_array); if (h->flags_array==0) { ret=errno; if (0) { died2: free(h->flags_array); }                    goto died1; }
	MALLOC_N(n_to_malloc, h->roots);       if (h->roots==0)       { ret=errno; if (0) { died3: if (h->n_named_roots>=0) free(h->roots); } goto died2; }
	MALLOC_N(n_to_malloc, h->root_hashes); if (h->root_hashes==0) { ret=errno; if (0) { died4: if (h->n_named_roots>=0) free(h->root_hashes); } goto died3; }
	MALLOC_N(n_to_malloc, h->names);       if (h->names==0)       { ret=errno; if (0) { died5: if (h->n_named_roots>=0) free(h->names); } goto died4; }
	for (i=0; i<h->n_named_roots; i++) {
	    h->root_hashes[i].valid = FALSE;
	    h->roots[i]       = rbuf_diskoff(&rc);
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
	h->roots[0] = rbuf_diskoff(&rc);
	h->root_hashes[0].valid = FALSE;
	h->flags_array[0] = rbuf_int(&rc);
    }
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died5;}
    toku_free(rc.buf);
    *brth = h;
    return 0;
}

int toku_deserialize_brtheader_from (int fd, DISKOFF off, u_int32_t fullhash, struct brt_header **brth) {
    //printf("%s:%d calling MALLOC\n", __FILE__, __LINE__);
    assert(off==0);
    //printf("%s:%d malloced %p\n", __FILE__, __LINE__, h);

    char     magic[12];
    ssize_t r = pread(fd, magic,  12, off);
    if (r==0) return -1;
    if (r<0)  return errno;
    if (r!=12) return EINVAL;
    if (memcmp(magic,"tokudata",8)==0) {
	// It's version 7 or later
	return deserialize_brtheader_7_or_later(ntohl(*(int*)(&magic[8])), fd, off, brth, fullhash);
    } else {
	return deserialize_brtheader_6_or_earlier(fd, off, brth, fullhash);
    }
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
int toku_serialize_fifo_at (int fd, off_t freeoff, FIFO fifo) {
    {
	int size=4;
	char buf[size];
	struct wbuf w;
	wbuf_init(&w, buf, size);
	wbuf_int(&w, toku_fifo_n_entries(fifo));
	ssize_t r = pwrite(fd, w.buf, size, freeoff);
	if (r!=size) return errno;
	freeoff+=size;
    }
    FIFO_ITERATE(fifo, key, keylen, val, vallen, type, xid,
		 ({
		     size_t size=keylen+vallen+1+8+4+4;
		     char  *MALLOC_N(size, buf);
		     assert(buf!=0);
		     struct wbuf w;
		     wbuf_init(&w, buf, size);
		     wbuf_char(&w, type);
		     wbuf_TXNID(&w, xid);
		     wbuf_bytes(&w, key, keylen);
		     //printf("%s:%d Writing %d bytes: %s\n", __FILE__, __LINE__, vallen, (char*)val); 
		     wbuf_bytes(&w, val, vallen);
		     assert(w.ndone==size);
		     ssize_t r = pwrite(fd, w.buf, (size_t)size, freeoff);
		     if (r<0) return errno;
		     assert(r==(ssize_t)size);
		     freeoff+=size;
		     toku_free(buf);
		 }));
    return 0;
}

int read_int (int fd, off_t *at, u_int32_t *result) {
    int v;
    ssize_t r = pread(fd, &v, 4, *at);
    if (r<0) return errno;
    assert(r==4);
    *result = ntohl(v);
    (*at) += 4;
    return 0;
}

int read_char (int fd, off_t *at, char *result) {
    ssize_t r = pread(fd, result, 1, *at);
    if (r<0) return errno;
    assert(r==1);
    (*at)++;
    return 0;
}

int read_u_int64_t (int fd, off_t *at, u_int64_t *result) {
    u_int32_t v1=0,v2=0;
    int r;
    if ((r = read_int(fd, at, &v1))) return r;
    if ((r = read_int(fd, at, &v2))) return r;
    *result = (((u_int64_t)v1)<<32) + v2;
    return 0;
}

int read_nbytes (int fd, off_t *at, char **data, u_int32_t len) {
    char *result = toku_malloc(len);
    if (result==0) return errno;
    ssize_t r = pread(fd, result, len, *at);
    //printf("%s:%d read %d bytes, which are %s\n", __FILE__, __LINE__, len, result);
    if (r<0) return errno;
    assert(r==(ssize_t)len);
    (*at)+=len;
    *data=result;
    return 0;
}

int toku_deserialize_fifo_at (int fd, off_t at, FIFO *fifo) {
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

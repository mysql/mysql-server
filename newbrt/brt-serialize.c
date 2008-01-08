/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#define _XOPEN_SOURCE 500

//#include "pma.h"
#include "brt-internal.h"
#include "key.h"
#include "rbuf.h"
#include "wbuf.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>


static const int brtnode_header_overhead = (8+   // magic "tokunode" or "tokuleaf"
					    8+   // checkpoint number
					    4+   // block size
					    4+   // data size
					    4+   // flags
					    4+   // height
					    4+   // random for fingerprint
					    4+   // localfingerprint
					    4);  // crc32 at the end

static unsigned int toku_serialize_brtnode_size_slow(BRTNODE node) {
    unsigned int size=brtnode_header_overhead;
    if (node->height>0) {
	unsigned int hsize=0;
	unsigned int csize=0;
	int i;
	size+=4; /* n_children */
	size+=4; /* subtree fingerprint. */
	for (i=0; i<node->u.n.n_children-1; i++) {
	    size+=4;
            if (node->flags & TOKU_DB_DUPSORT) size += 4;
	    csize+=toku_brtnode_pivot_key_len(node, node->u.n.childkeys[i]);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    size+=8; // diskoff
	    size+=4; // subsum
	}
	int n_hashtables = node->u.n.n_children;
	size+=4; /* n_entries */
        assert(0 <= n_hashtables && n_hashtables < TREE_FANOUT+1);
	for (i=0; i< n_hashtables; i++) {
	    HASHTABLE_ITERATE(node->u.n.htables[i],
			      key __attribute__((__unused__)), keylen,
			      data __attribute__((__unused__)), datalen,
                              type __attribute__((__unused__)),
			      (hsize+=BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+keylen+datalen));
	}
	assert(hsize==node->u.n.n_bytes_in_hashtables);
	assert(csize==node->u.n.totalchildkeylens);
	return size+hsize+csize;
    } else {
	unsigned int hsize=0;
	PMA_ITERATE(node->u.l.buffer,
		    key __attribute__((__unused__)), keylen,
		    data __attribute__((__unused__)), datalen,
		    (hsize+=PMA_ITEM_OVERHEAD+KEY_VALUE_OVERHEAD+keylen+datalen));
	assert(hsize==node->u.l.n_bytes_in_buffer);
	hsize+=4; /* the PMA size */
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
	result+=(8+4+4)*(node->u.n.n_children); /* For each child, a child offset, a count for the number of hash table entries, and the subtree fingerprint. */
	result+=node->u.n.n_bytes_in_hashtables;
    } else {
	result+=(4 /* n_entries in buffer table. */
		 +4); /* the pma size */
	result+=node->u.l.n_bytes_in_buffer;
	if (toku_memory_check) {
	    unsigned int slowresult = toku_serialize_brtnode_size_slow(node);
	    if (result!=slowresult) printf("%s:%d result=%d slowresult=%d\n", __FILE__, __LINE__, result, slowresult);
	    assert(result==slowresult);
	}
    }
    return result;
}

void toku_serialize_brtnode_to(int fd, DISKOFF off, DISKOFF size, BRTNODE node) {
    //printf("%s:%d serializing\n", __FILE__, __LINE__);
    struct wbuf w;
    int i;
    unsigned int calculated_size = toku_serialize_brtnode_size(node);
    //char buf[size];
    char *MALLOC_N(size,buf);
    toku_verify_counts(node);
    assert(size>0);
    wbuf_init(&w, buf, size);
    //printf("%s:%d serializing %lld w height=%d p0=%p\n", __FILE__, __LINE__, off, node->height, node->mdicts[0]);
    wbuf_literal_bytes(&w, "toku", 4);
    if (node->height==0) wbuf_literal_bytes(&w, "leaf", 4);
    else wbuf_literal_bytes(&w, "node", 4);
    wbuf_int(&w, node->layout_version);
    wbuf_ulonglong(&w, node->log_lsn.lsn);
    //printf("%s:%d %lld.calculated_size=%d\n", __FILE__, __LINE__, off, calculated_size);
    wbuf_int(&w, calculated_size);
    wbuf_int(&w, node->flags);
    wbuf_int(&w, node->height);
    //printf("%s:%d %lld rand=%08x sum=%08x height=%d\n", __FILE__, __LINE__, node->thisnodename, node->rand4fingerprint, node->subtree_fingerprint, node->height);
    wbuf_int(&w, node->rand4fingerprint);
    wbuf_int(&w, node->local_fingerprint);
    //printf("%s:%d local_fingerprint=%8x\n", __FILE__, __LINE__, node->local_fingerprint);
    //printf("%s:%d w.ndone=%d n_children=%d\n", __FILE__, __LINE__, w.ndone, node->n_children);
    if (node->height>0) {
	// Local fingerprint is not actually stored while in main memory.  Must calculate it.
	// Subtract the child fingerprints from the subtree fingerprint to get the local fingerprint.
	{
	    u_int32_t subtree_fingerprint = node->local_fingerprint;
	    for (i=0; i<node->u.n.n_children; i++) {
		subtree_fingerprint += BRTNODE_CHILD_SUBTREE_FINGERPRINTS(node, i);
	    }
	    wbuf_int(&w, subtree_fingerprint);
	}
	wbuf_int(&w, node->u.n.n_children);
	for (i=0; i<node->u.n.n_children; i++) {
	    wbuf_int(&w, BRTNODE_CHILD_SUBTREE_FINGERPRINTS(node, i));
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
	    wbuf_DISKOFF(&w, node->u.n.children[i]);
	    //printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
	}

	{
	    int n_hash_tables = node->u.n.n_children;
	    u_int32_t check_local_fingerprint = 0;
	    for (i=0; i< n_hash_tables; i++) {
		//printf("%s:%d p%d=%p n_entries=%d\n", __FILE__, __LINE__, i, node->mdicts[i], mdict_n_entries(node->mdicts[i]));
		wbuf_int(&w, toku_hashtable_n_entries(node->u.n.htables[i]));
		HASHTABLE_ITERATE(node->u.n.htables[i], key, keylen, data, datalen, type,
				  ({
				      wbuf_char(&w, type);
				      wbuf_bytes(&w, key, keylen);
				      wbuf_bytes(&w, data, datalen);
				      check_local_fingerprint+=node->rand4fingerprint*toku_calccrc32_cmd(type, key, keylen, data, datalen);
				  }));
	    }
	    //printf("%s:%d check_local_fingerprint=%8x\n", __FILE__, __LINE__, check_local_fingerprint);
	    assert(check_local_fingerprint==node->local_fingerprint);
	}
    } else {
	//printf(" n_entries=%d\n", toku_pma_n_entries(node->u.l.buffer));
	wbuf_int(&w, toku_pma_n_entries(node->u.l.buffer));
	wbuf_int(&w, toku_pma_index_limit(node->u.l.buffer));
	PMA_ITERATE_IDX(node->u.l.buffer, idx,
			key, keylen, data, datalen,
			({
			    wbuf_int(&w, idx);
			    wbuf_bytes(&w, key, keylen);
			    wbuf_bytes(&w, data, datalen);
			}));
    }
    assert(w.ndone<=w.size);
#ifdef CRC_ATEND
    wbuf_int(&w, crc32(toku_null_crc, w.buf, w.ndone)); 
#endif
#ifdef CRC_INCR
    wbuf_int(&w, w.crc32);
#endif

    //write_now: printf("%s:%d Writing %d bytes\n", __FILE__, __LINE__, w.ndone);
    {
	ssize_t r=pwrite(fd, w.buf, w.ndone, off);
	if (r<0) printf("r=%ld errno=%d\n", (long)r, errno);
	assert((size_t)r==w.ndone);
    }

    //printf("%s:%d w.done=%d r=%d\n", __FILE__, __LINE__, w.ndone, r);
    assert(calculated_size==w.ndone);

    //printf("%s:%d wrote %d bytes for %lld size=%lld\n", __FILE__, __LINE__, w.ndone, off, size);
    assert(w.ndone<=size);
    toku_free(buf);
}

int toku_deserialize_brtnode_from (int fd, DISKOFF off, BRTNODE *brtnode, int flags, int nodesize,
				   int (*bt_compare)(DB *, const DBT *, const DBT *),
				   int (*dup_compare)(DB *, const DBT *, const DBT *),
				   DB *db, FILENUM filenum) {
    TAGMALLOC(BRTNODE, result);
    struct rbuf rc;
    int i;
    u_int32_t datasize;
    int r;
    if (errno!=0) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }
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
    if (errno!=0) {
	if (0) { died1: toku_free(rc.buf); }
	r=errno;
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
    if (result->layout_version!=1) {
	r=DB_BADFORMAT;
	goto died1;
    }
    result->disk_lsn.lsn = rbuf_ulonglong(&rc);
    result->log_lsn = result->disk_lsn;
    {
	unsigned int stored_size = rbuf_int(&rc);
	if (stored_size!=datasize) { r=DB_BADFORMAT; goto died1; }
    }
    result->nodesize = nodesize; // How to compute the nodesize?
    result->thisnodename = off;
    result->flags = rbuf_int(&rc); assert(result->flags == (unsigned int) flags);
    result->height = rbuf_int(&rc);
    result->rand4fingerprint = rbuf_int(&rc);
    result->local_fingerprint = rbuf_int(&rc);
    result->dirty = 0;
    //printf("height==%d\n", result->height);
    if (result->height>0) {
	result->u.n.totalchildkeylens=0;
	for (i=0; i<TREE_FANOUT; i++) { 
	    BRTNODE_CHILD_SUBTREE_FINGERPRINTS(result, i)=0;
            result->u.n.childkeys[i]=0; 
        }
	for (i=0; i<TREE_FANOUT+1; i++) { 
            result->u.n.children[i]=0; 
            result->u.n.htables[i]=0; 
            result->u.n.n_bytes_in_hashtable[i]=0;
            result->u.n.n_cursors[i]=0;
        }
	u_int32_t subtree_fingerprint = rbuf_int(&rc);
	u_int32_t check_subtree_fingerprint = 0;
	result->u.n.n_children = rbuf_int(&rc);
	//printf("n_children=%d\n", result->n_children);
	assert(result->u.n.n_children>=0 && result->u.n.n_children<=TREE_FANOUT);
	for (i=0; i<result->u.n.n_children; i++) {
	    u_int32_t childfp = rbuf_int(&rc);
	    BRTNODE_CHILD_SUBTREE_FINGERPRINTS(result, i)= childfp;
	    check_subtree_fingerprint += childfp;
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
	    result->u.n.children[i] = rbuf_diskoff(&rc);
	    //printf("Child %d at %lld\n", i, result->children[i]);
	}
	for (i=0; i<TREE_FANOUT+1; i++) {
	    result->u.n.n_bytes_in_hashtable[i] = 0;
	}
	result->u.n.n_bytes_in_hashtables = 0; 
	for (i=0; i<result->u.n.n_children; i++) {
	    r=toku_hashtable_create(&result->u.n.htables[i]);
	    if (r!=0) {
		int j;
		if (0) { died_12: j=result->u.n.n_bytes_in_hashtables; }
		for (j=0; j<i; j++) toku_hashtable_free(&result->u.n.htables[j]);
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
                    int type;
		    bytevec key; ITEMLEN keylen; 
		    bytevec val; ITEMLEN vallen;
		    toku_verify_counts(result);
                    type = rbuf_char(&rc);
		    rbuf_bytes(&rc, &key, &keylen); /* Returns a pointer into the rbuf. */
		    rbuf_bytes(&rc, &val, &vallen);
		    check_local_fingerprint += result->rand4fingerprint * toku_calccrc32_cmd(type, key, keylen, val, vallen);
		    //printf("Found %s,%s\n", (char*)key, (char*)val);
		    {
			r=toku_hash_insert(result->u.n.htables[cnum], key, keylen, val, vallen, type); /* Copies the data into the hash table. */
			if (r!=0) { goto died_12; }
		    }
		    diff =  keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
		    result->u.n.n_bytes_in_hashtables += diff;
		    result->u.n.n_bytes_in_hashtable[cnum] += diff;
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
	r=toku_pma_create(&result->u.l.buffer, bt_compare, db, filenum, nodesize);
	if (r!=0) {
	    if (0) { died_21: toku_pma_free(&result->u.l.buffer); }
	    goto died1;
	}
        toku_pma_set_dup_mode(result->u.l.buffer, flags);
        toku_pma_set_dup_compare(result->u.l.buffer, dup_compare);
	//printf("%s:%d r PMA= %p\n", __FILE__, __LINE__, result->u.l.buffer); 
	toku_verify_counts(result);
#define BRT_USE_PMA_BULK_INSERT 1
#if BRT_USE_PMA_BULK_INSERT
{
        DBT keys[n_in_buf], vals[n_in_buf];
	int index_limit __attribute__((__unused__))= rbuf_int(&rc);
	for (i=0; i<n_in_buf; i++) {
	    bytevec key; ITEMLEN keylen; 
	    bytevec val; ITEMLEN vallen;
	    // The counts are wrong here
	    int idx __attribute__((__unused__)) = rbuf_int(&rc);
	    rbuf_bytes(&rc, &key, &keylen); /* Returns a pointer into the rbuf. */
            toku_fill_dbt(&keys[i], key, keylen);
	    rbuf_bytes(&rc, &val, &vallen);
            toku_fill_dbt(&vals[i], val, vallen);
	    result->u.l.n_bytes_in_buffer += keylen + vallen + KEY_VALUE_OVERHEAD + PMA_ITEM_OVERHEAD;
        }
        if (n_in_buf > 0) {
	    u_int32_t actual_sum = 0;
            r = toku_pma_bulk_insert((TOKUTXN)0, (FILENUM){0}, (DISKOFF)0, result->u.l.buffer, keys, vals, n_in_buf, result->rand4fingerprint, &actual_sum);
            if (r!=0) goto died_21;
	    if (actual_sum!=result->local_fingerprint) {
		//fprintf(stderr, "%s:%d Corrupted checksum stored=%08x rand=%08x actual=%08x height=%d n_keys=%d\n", __FILE__, __LINE__, result->rand4fingerprint, result->local_fingerprint, actual_sum, result->height, n_in_buf);
		return DB_BADFORMAT;
		goto died_21;
	    } else {
		//fprintf(stderr, "%s:%d Good checksum=%08x height=%d\n", __FILE__, __LINE__, actual_sum, result->height);
	    }
        }
}
#else
	for (i=0; i<n_in_buf; i++) {
	    bytevec key; ITEMLEN keylen; 
	    bytevec val; ITEMLEN vallen;
	    toku_verify_counts(result);
	    rbuf_bytes(&rc, &key, &keylen); /* Returns a pointer into the rbuf. */
	    rbuf_bytes(&rc, &val, &vallen);
	    {
		DBT k,v;
		r = toku_pma_insert(result->u.l.buffer, toku_fill_dbt(&k, key, keylen), toku_fill_dbt(&v, val, vallen), 0);
		if (r!=0) goto died_21;
	    }
	    result->u.l.n_bytes_in_buffer += keylen + vallen + KEY_VALUE_OVERHEAD + PMA_ITEM_OVERHEAD;
	}
#endif
	toku_verify_counts(result);
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
    toku_free(rc.buf);
    *brtnode = result;
    toku_verify_counts(result);
    return 0;
}

void toku_verify_counts (BRTNODE node) {
    /*foo*/
    if (node->height==0) {
	assert(node->u.l.buffer);
	unsigned int sum=0;
	PMA_ITERATE(node->u.l.buffer, key __attribute__((__unused__)), keylen, data  __attribute__((__unused__)), datalen,
		    sum+=(PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD + keylen + datalen));
	assert(sum==node->u.l.n_bytes_in_buffer);
    } else {
	unsigned int sum = 0;
	int i;
	for (i=0; i<node->u.n.n_children; i++)
	    sum += node->u.n.n_bytes_in_hashtable[i];
	// We don't rally care of the later hashtables have garbage in them.  Valgrind would do a better job noticing if we leave it uninitialized.
	// But for now the code always initializes the later tables so they are 0.
	for (; i<TREE_FANOUT+1; i++) {
	    assert(node->u.n.n_bytes_in_hashtable[i]==0);
        }
	assert(sum==node->u.n.n_bytes_in_hashtables);
    }
}
    
int toku_serialize_brt_header_size (struct brt_header *h) {
    unsigned int size = 4+4+4+8+8+4; /* this size, flags, the tree's nodesize, freelist, unused_memory, named_roots. */
    if (h->n_named_roots<0) {
	size+=8;
    } else {
	int i;
	for (i=0; i<h->n_named_roots; i++) {
	    size+=12 + 1 + strlen(h->names[i]);
	}
    }
    return size;
}

int toku_serialize_brt_header_to_wbuf (struct wbuf *wbuf, struct brt_header *h) {
    unsigned int size = toku_serialize_brt_header_size (h); // !!! seems silly to recompute the size when the caller knew it.  Do we really need the size?
    wbuf_int    (wbuf, size);
    wbuf_int    (wbuf, h->flags);
    wbuf_int    (wbuf, h->nodesize);
    wbuf_DISKOFF(wbuf, h->freelist);
    wbuf_DISKOFF(wbuf, h->unused_memory);
    wbuf_int    (wbuf, h->n_named_roots);
    if (h->n_named_roots>0) {
	int i;
	for (i=0; i<h->n_named_roots; i++) {
	    char *s = h->names[i];
	    unsigned int l = 1+strlen(s);
	    wbuf_DISKOFF(wbuf, h->roots[i]);
	    wbuf_bytes  (wbuf,  s, l);
	    assert(l>0 && s[l-1]==0);
	}
    } else {
	wbuf_DISKOFF(wbuf, h->unnamed_root);
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

int toku_deserialize_brtheader_from (int fd, DISKOFF off, struct brt_header **brth) {
    //printf("%s:%d calling MALLOC\n", __FILE__, __LINE__);
    struct brt_header *MALLOC(h);
    struct rbuf rc;
    int size;
    int sizeagain;
    int ret = -1;
    assert(off==0);
    //printf("%s:%d malloced %p\n", __FILE__, __LINE__, h);
    {
	uint32_t size_n;
	ssize_t r = pread(fd, &size_n, sizeof(size_n), off);
	if (r==0) {
        died0:
	    toku_free(h); return ret;
	}
	if (r!=sizeof(size_n)) {ret = EINVAL; goto died0;}
	size = ntohl(size_n);
    }
    rc.buf = toku_malloc(size);
    if (rc.buf == NULL) {ret = ENOMEM; goto died0;}
    if (0) {
        died1:
        toku_free(rc.buf);
        goto died0;
    }
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
    h->flags         = rbuf_int(&rc);
    h->nodesize      = rbuf_int(&rc);
    h->freelist      = rbuf_diskoff(&rc);
    h->unused_memory = rbuf_diskoff(&rc);
    h->n_named_roots = rbuf_int(&rc);
    if (h->n_named_roots>=0) {
	int i;
	MALLOC_N(h->n_named_roots, h->roots);
	if (h->n_named_roots > 0 && h->roots == NULL) {ret = ENOMEM; goto died1;}
	if (0) {
	    died2:
	    toku_free(h->roots);
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
	    h->roots[i] = rbuf_diskoff(&rc);
	    rbuf_bytes(&rc, &nameptr, &len);
	    if (strlen(nameptr)+1!=len) {ret = EINVAL; goto died3;}
	    h->names[i] = toku_memdup(nameptr,len);
	    if (len > 0 && h->names[i] == NULL) {ret = ENOMEM; goto died3;}
	}
	
	h->unnamed_root = -1;
    } else {
	h->roots = 0;
	h->names = 0;
	h->unnamed_root = rbuf_diskoff(&rc);
    }
    if (rc.ndone!=rc.size) {ret = EINVAL; goto died3;}
    toku_free(rc.buf);
    *brth = h;
    return 0;
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

/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt-serialize.c 13852 2009-08-05 19:34:12Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "ule.h"
#include "fifo.h"
/*******************
 * Purpose of this file is to provide backwards compatibility with earlier versions
 * of the file format.
 *
 * Used by brt-serialize.c.
 *
 * NOTE: All functions in this file are static.
 *       This file is included in brt-serialize.c.  It should not be compiled by itself.
 *       
 *
 */

enum { BRT_CMD_OVERHEAD_10 = (1     // the type
                            + 8)  // the xid
};
// FIFO_10 (data structure changed, so we need to provide the old one)
// Calculate the fingerprint for a kvpair
static void toku_calc_more_murmur_kvpair (struct x1764 *mm, const void *key, int keylen, const void *val, int vallen) {
    int i;
    i = toku_htod32(keylen);
    x1764_add(mm,  (void*)&i, 4);
    x1764_add(mm,  key, keylen);
    i = toku_htod32(vallen);
    x1764_add(mm, (void*)&i, 4);
    x1764_add(mm, val, vallen);
}

static u_int32_t calc_fingerprint_cmd10 (u_int32_t type, TXNID xid, const void *key, u_int32_t keylen, const void *val, u_int32_t vallen) {                                                                        
    unsigned char type_c = (unsigned char)type;
    unsigned int a = toku_htod32(xid>>32);
    unsigned int b = toku_htod32(xid&0xffffffff);
    struct x1764 mm;
    x1764_init(&mm);
    x1764_add(&mm, &type_c, 1);
    x1764_add(&mm, &a, 4);
    x1764_add(&mm, &b, 4);
    toku_calc_more_murmur_kvpair(&mm, key, keylen, val, vallen);
    return x1764_finish(&mm);
}

#define FIFO10_ITERATE(fifo10,keyvar,keylenvar,datavar,datalenvar,typevar,xidvar,body) do {    \
  int fifo10_iterate_off;                                                                    \
  for (fifo10_iterate_off = toku_fifo10_iterate_internal_start(fifo10);                          \
       toku_fifo10_iterate_internal_has_more(fifo10, fifo10_iterate_off);			   \
       fifo10_iterate_off = toku_fifo10_iterate_internal_next(fifo10, fifo10_iterate_off)) {       \
      struct fifo10_entry *e = toku_fifo10_iterate_internal_get_entry(fifo10, fifo10_iterate_off); \
      bytevec keyvar = e->key;                                                             \
      ITEMLEN keylenvar = e->keylen;                                                       \
      bytevec datavar = e->key + e->keylen;                                           \
      ITEMLEN datalenvar = e->vallen;                                                 \
      int     typevar = e->type;                                                      \
      TXNID   xidvar = e->xid;                                                        \
      body;                                                                           \
  } } while (0)
struct fifo10_entry {
    unsigned int keylen;
    unsigned int vallen;
    unsigned char type;
    TXNID xid;
    unsigned char key[];
};

struct fifo {
    int n_items_in_fifo;
    char *memory;       // An array of bytes into which fifo10_entries are embedded.
    int   memory_size;  // How big is fifo10_memory
    int   memory_start; // Where is the first used byte?
    int   memory_used;  // How many bytes are in use?
};

const int fifo10_initial_size = 4096;
static void fifo10_init(struct fifo *fifo10) {
    fifo10->n_items_in_fifo = 0;
    fifo10->memory       = 0;
    fifo10->memory_size  = 0;
    fifo10->memory_start = 0;
    fifo10->memory_used  = 0;
}

static int fifo10_entry_size(struct fifo10_entry *entry) {
    return sizeof (struct fifo10_entry) + entry->keylen + entry->vallen;
}

static int toku_fifo10_create(FIFO *ptr) {
    struct fifo *MALLOC(fifo10);
    if (fifo10 == 0) return ENOMEM;
    fifo10_init(fifo10);
    *ptr = fifo10;
    return 0;
}

static void toku_fifo10_free(FIFO *ptr) {
    FIFO fifo10 = *ptr;
    if (fifo10->memory) toku_free(fifo10->memory);
    fifo10->memory=0;
    toku_free(fifo10);
    *ptr = 0;
}

static int next_power_of_two (int n) {
    int r = 4096;
    while (r < n) {
	r*=2;
	assert(r>0);
    }
    return r;
}

static int toku_fifo10_enq(FIFO fifo10, const void *key, unsigned int keylen, const void *data, unsigned int datalen, int type, TXNID xid) {
    int need_space_here = sizeof(struct fifo10_entry) + keylen + datalen;
    int need_space_total = fifo10->memory_used+need_space_here;
    if (fifo10->memory == NULL) {
	fifo10->memory_size = next_power_of_two(need_space_total);
	fifo10->memory = toku_malloc(fifo10->memory_size);
    }
    if (fifo10->memory_start+need_space_total > fifo10->memory_size) {
	// Out of memory at the end.
	int next_2 = next_power_of_two(need_space_total);
	if ((2*next_2 > fifo10->memory_size)
	    || (8*next_2 < fifo10->memory_size)) {
	    // resize the fifo10
	    char *newmem = toku_malloc(next_2);
	    char *oldmem = fifo10->memory;
	    if (newmem==0) return ENOMEM;
	    memcpy(newmem, oldmem+fifo10->memory_start, fifo10->memory_used);
	    fifo10->memory_size = next_2;
	    fifo10->memory_start = 0;
	    fifo10->memory = newmem;
	    toku_free(oldmem);
	} else {
	    // slide things over
	    memmove(fifo10->memory, fifo10->memory+fifo10->memory_start, fifo10->memory_used);
	    fifo10->memory_start = 0;
	}
    }
    struct fifo10_entry *entry = (struct fifo10_entry *)(fifo10->memory + fifo10->memory_start + fifo10->memory_used);
    entry->type = (unsigned char)type;
    entry->xid  = xid;
    entry->keylen = keylen;
    memcpy(entry->key, key, keylen);
    entry->vallen = datalen;
    memcpy(entry->key + keylen, data, datalen);
    fifo10->n_items_in_fifo++;
    fifo10->memory_used += need_space_here;
    return 0;
}

static int toku_fifo10_iterate_internal_start(FIFO fifo10) { return fifo10->memory_start; }
static int toku_fifo10_iterate_internal_has_more(FIFO fifo10, int off) { return off < fifo10->memory_start + fifo10->memory_used; }
static int toku_fifo10_iterate_internal_next(FIFO fifo10, int off) {
    struct fifo10_entry *e = (struct fifo10_entry *)(fifo10->memory + off);
    return off + fifo10_entry_size(e);
}
static struct fifo10_entry * toku_fifo10_iterate_internal_get_entry(FIFO fifo10, int off) {
    return (struct fifo10_entry *)(fifo10->memory + off);
}

// LEAFENTRY ACCESSORS
//
// This ugly factorization of the macro is done so that we can do ## or not depending on which version of the
// compiler we are using, without repeating all this crufty offset calculation.

static inline void putint (unsigned char *p, u_int32_t i) {
#if 1
    *(u_int32_t*)p = toku_htod32(i);
#else
    p[0]=(i>>24)&0xff;
    p[1]=(i>>16)&0xff;
    p[2]=(i>> 8)&0xff;
    p[3]=(i>> 0)&0xff;
#endif
}
static inline void putint64 (unsigned char *p, u_int64_t i) {
    putint(p, (u_int32_t)(i>>32));
    putint(p+4, (u_int32_t)(i&0xffffffff));
}
static inline u_int32_t getint (unsigned char *p) {
#if 1
    return toku_dtoh32(*(u_int32_t*)p);
#else
    return (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+(p[3]);
#endif
}
static inline u_int64_t getint64 (unsigned char *p) {
    u_int64_t H = getint(p);
    u_int64_t L = getint(p+4);
    return (H<<32) + L;
}


#define DO_LE_COMMITTED(funname,le)  case LE_COMMITTED: {                                                            \
    unsigned char* __klenaddr = 1+(unsigned char*)le;  u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4      + __klenaddr;                                                                 \
    unsigned char* __clenaddr = __klen + __kvaladdr;   u_int32_t __clen = getint(__clenaddr);                        \
    unsigned char* __cvaladdr = 4 + __clenaddr;                                                                      \
    return funname ## _le10_committed(__klen, __kvaladdr, __clen, __cvaladdr

#define DO_LE_BOTH(funname,le)  case LE_BOTH: {                         \
    unsigned char* __xidaddr  = 1+(unsigned char*)le;  u_int64_t __xid  = getint64(__xidaddr);                       \
    unsigned char* __klenaddr = 8 + __xidaddr;         u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4 + __klenaddr;                                                                      \
    unsigned char* __clenaddr = __klen + __kvaladdr;   u_int32_t __clen = getint(__clenaddr);                        \
    unsigned char* __cvaladdr = 4 + __clenaddr;                                                                      \
    unsigned char* __plenaddr = __clen + __cvaladdr;   u_int32_t __plen = getint(__plenaddr);                        \
    unsigned char* __pvaladdr = 4 + __plenaddr;                                                                      \
    return funname ## _le10_both(__xid, __klen, __kvaladdr, __clen, __cvaladdr, __plen, __pvaladdr

#define DO_LE_PROVDEL(funname,le )  case LE_PROVDEL:  {                                                              \
    unsigned char* __xidaddr  = 1+(unsigned char*)le;  u_int64_t __xid  = getint64(__xidaddr);                       \
    unsigned char* __klenaddr = 8 + __xidaddr;         u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4 + __klenaddr;                                                                      \
    unsigned char* __dlenaddr = __klen + __kvaladdr;   u_int32_t __dlen = getint(__dlenaddr);                        \
    unsigned char* __dvaladdr = 4 + __dlenaddr;                                                                      \
    return funname ## _le10_provdel(__xid, __klen, __kvaladdr, __dlen, __dvaladdr

#define DO_LE_PROVPAIR(funname,le)   case LE_PROVPAIR:  {                                                            \
    unsigned char* __xidaddr  = 1+(unsigned char*)le;  u_int64_t __xid  = getint64(__xidaddr);                       \
    unsigned char* __klenaddr = 8 + __xidaddr;         u_int32_t __klen = getint(__klenaddr);                        \
    unsigned char* __kvaladdr = 4 + __klenaddr;                                                                      \
    unsigned char* __plenaddr = __klen + __kvaladdr;   u_int32_t __plen = getint(__plenaddr);                        \
    unsigned char* __pvaladdr = 4 + __plenaddr;                                                                      \
    return funname ## _le10_provpair(__xid, __klen, __kvaladdr, __plen, __pvaladdr

#ifdef __ICL
#define LESWITCHCALL(le,funname, ...) do {        \
  switch(get_le_state(le)) {                      \
    DO_LE_COMMITTED(funname,le) , __VA_ARGS__); } \
    DO_LE_BOTH     (funname,le) , __VA_ARGS__); } \
    DO_LE_PROVDEL  (funname,le) , __VA_ARGS__); } \
    DO_LE_PROVPAIR (funname,le) , __VA_ARGS__); } \
  } abort(); } while (0)
#else
#define LESWITCHCALL(le,funname, ...) do {           \
  switch(get_le_state(le)) {                         \
    DO_LE_COMMITTED(funname,le) , ## __VA_ARGS__); } \
    DO_LE_BOTH     (funname,le) , ## __VA_ARGS__); } \
    DO_LE_PROVDEL  (funname,le) , ## __VA_ARGS__); } \
    DO_LE_PROVPAIR (funname,le) , ## __VA_ARGS__); } \
  } abort(); } while (0)
#endif

static u_int32_t memsize_le10_committed (u_int32_t keylen, void *key __attribute__((__unused__)),
				       u_int32_t vallen, void *val __attribute__((__unused__))) {
    return 1+ 2*4 + keylen + vallen;
}

static u_int32_t memsize_le10_both (TXNID txnid __attribute__((__unused__)),
				  u_int32_t klen, void *kval __attribute__((__unused__)),
				  u_int32_t clen, void *cval __attribute__((__unused__)),
				  u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*3 + klen + clen + plen;
}

static u_int32_t memsize_le10_provdel (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t clen, void *cval __attribute__((__unused__))) {
    return 1 + 8 + 4*2 + klen + clen;
}

static u_int32_t memsize_le10_provpair (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*2 + klen + plen;
}

static u_int32_t leafentry_memsize_10 (LEAFENTRY le) {
    LESWITCHCALL(le, memsize);
    abort(); return 0;  // make certain compilers happy
}

static u_int32_t disksize_le10_committed (u_int32_t keylen, void *key __attribute__((__unused__)),
				       u_int32_t vallen, void *val __attribute__((__unused__))) {
    return 1 + 4 + 4 + keylen + vallen;
}

static u_int32_t disksize_le10_both (TXNID txnid __attribute__((__unused__)),
				  u_int32_t klen, void *kval __attribute__((__unused__)),
				  u_int32_t clen, void *cval __attribute__((__unused__)),
				  u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*3 + klen + clen + plen;
}

static u_int32_t disksize_le10_provdel (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t clen, void *cval __attribute__((__unused__))) {
    return 1 + 8 + 4 + 4 + klen + clen;
}

static u_int32_t disksize_le10_provpair (TXNID txnid __attribute__((__unused__)),
				       u_int32_t klen, void *kval __attribute__((__unused__)),
				       u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4 + 4 + klen + plen;
}


static u_int32_t
le10_disksize_internal (LEAFENTRY le) {
    LESWITCHCALL(le, disksize);
    abort(); return 0;  // make certain compilers happy
}

static u_int32_t le10_disksize (LEAFENTRY le) {
    u_int32_t d = le10_disksize_internal(le);
    // this computation is currently identical to the _disksize_internal
    u_int32_t m = leafentry_memsize_10(le);
    assert(m==d);
    return d;
}

//LEAFENTRY constructors

//Constructors for version 10 leafentries, possibly needed for upgrades.
int
le10_committed (u_int32_t klen, void* kval, u_int32_t dlen, void* dval, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result) {
    size_t size = 9+klen+dlen;
    unsigned char *lec=toku_malloc(size);
    assert(lec);
    lec[0] = LE_COMMITTED;
    putint(lec+1, klen);
    memcpy(lec+1+4, kval, klen);
    putint(lec+1+4+klen, dlen);
    memcpy(lec+1+4+klen+4, dval, dlen);
    *resultsize=size;
    *disksize  = 1 + 4 + 4 + klen + dlen;
    *result=(LEAFENTRY)lec;
    return 0;
}

int
le10_both (TXNID xid, u_int32_t klen, void* kval, u_int32_t clen, void* cval, u_int32_t plen, void* pval,
	     u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result) {
    size_t size = 1+8+4*3+klen+clen+plen;
    unsigned char *lec=toku_malloc(size);
    assert(lec);
    lec[0] = LE_BOTH;
    putint64(lec+1,          xid);
    putint  (lec+1+8,                       klen);
    memcpy  (lec+1+8+4,               kval, klen);
    putint  (lec+1+8+4+klen,                clen);
    memcpy  (lec+1+8+4+klen+4,        cval, clen);
    putint  (lec+1+8+4+klen+4+clen,         plen);
    memcpy  (lec+1+8+4+klen+4+clen+4, pval, plen);
    *resultsize=size;
    *disksize  = 1 + 8 + 4*3 + klen + clen + plen;
    *result=(LEAFENTRY)lec;
    return 0;

}

int
le10_provdel (TXNID xid, u_int32_t klen, void* kval, u_int32_t dlen, void* dval,
	    u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result) {
    size_t size = 1 + 8 + 2*4 + klen + dlen;
    unsigned char *lec= toku_malloc(size);
    assert(lec);
    lec[0] = LE_PROVDEL;
    putint64(lec+1,          xid);
    putint  (lec+1+8,                       klen);
    memcpy  (lec+1+8+4,               kval, klen);
    putint  (lec+1+8+4+klen,                dlen);
    memcpy  (lec+1+8+4+klen+4,        dval, dlen);
    *memsize=size;
    *disksize  = 1 + 4 + 4 + 8 + klen + dlen;
    *result=(LEAFENTRY)lec;
    return 0;
}

int
le10_provpair (TXNID xid, u_int32_t klen, void* kval, u_int32_t plen, void* pval, u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result) {
    size_t size = 1 + 8 + 2*4 + klen + plen;
    unsigned char *lec= toku_malloc(size);
    assert(lec);
    lec[0] = LE_PROVPAIR;
    putint64(lec+1,          xid);
    putint  (lec+1+8,                       klen);
    memcpy  (lec+1+8+4,               kval, klen);
    putint  (lec+1+8+4+klen,                plen);
    memcpy  (lec+1+8+4+klen+4,        pval, plen);
    *memsize=size;
    *disksize  = 1 + 4 + 4 + 8 + klen + plen;
    *result=(LEAFENTRY)lec;
    return 0;
}



// Given a version 10 header, create a version 11 header.
// If new memory is needed for the new header, allocate it here and free the memory of the old version header.
static int
upgrade_brtheader_10_11(struct brt_header **brth_10, struct brt_header ** brth_11) {
    assert((*brth_10)->layout_version == BRT_LAYOUT_VERSION_10);
    *brth_11 = *brth_10;
    *brth_10 = NULL;
    (*brth_11)->layout_version = BRT_LAYOUT_VERSION_11;
    (*brth_11)->layout_version_original = BRT_LAYOUT_VERSION_10;
    return 0;
}


static int
deserialize_brtheader_10 (int fd, struct rbuf *rb, struct brt_header **brth) {
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

static int
decompress_brtnode_from_raw_block_into_rbuf_10(u_int8_t *raw_block, struct rbuf *rb, BLOCKNUM blocknum) {
    int r = decompress_brtnode_from_raw_block_into_rbuf(raw_block, rb, blocknum);
    return r;
}

static int
deserialize_brtnode_leaf_from_rbuf_10 (BRTNODE result, bytevec magic, struct rbuf *rb) {
    //The only difference between this version and version 11 (for this function)
    //is the line that calculates size of leafentry.
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
        u_int32_t disksize = le10_disksize(le); //Only difference between 10 & 11
        rb->ndone += disksize;
        assert(rb->ndone<=rb->size);

        array[i]=(OMTVALUE)le;
        actual_sum += x1764_memory(le, disksize);
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
    
    //toku_verify_counts(result);

    (void)rbuf_int(rb); //Ignore the crc (already verified).
    if (rb->ndone != rb->size) { //Verify we read exactly the entire block.
        r = toku_db_badformat(); goto died_1;
    }

    r = toku_leaflock_borrow(&result->u.l.leaflock);
    if (r!=0) goto died_1;
    rb->buf = NULL; //Buffer was used for node's mempool.
    return 0;
}

static int
deserialize_brtnode_nonleaf_from_rbuf_10 (BRTNODE result, bytevec magic, struct rbuf *rb) {
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
        result->u.n.totalchildkeylens+=toku_brtnode_pivot_key_len(result, result->u.n.childkeys[i]);
    }
    for (i=0; i<result->u.n.n_children; i++) {
        BNC_BLOCKNUM(result,i) = rbuf_blocknum(rb);
        BNC_HAVE_FULLHASH(result, i) = FALSE;
        BNC_NBYTESINBUF(result,i) = 0;
        //printf("Child %d at %lld\n", i, result->children[i]);
    }
    result->u.n.n_bytes_in_buffers = 0;
    for (i=0; i<result->u.n.n_children; i++) {
        r=toku_fifo10_create(&BNC_BUFFER(result,i));
        if (r!=0) {
            int j;
            if (0) { died_1: j=result->u.n.n_bytes_in_buffers; }
            for (j=0; j<i; j++) toku_fifo10_free(&BNC_BUFFER(result,j));
            return toku_db_badformat();
        }
    }
    {
        int cnum;
        u_int32_t check_local_fingerprint = 0;
        for (cnum=0; cnum<result->u.n.n_children; cnum++) {
            int n_in_this_hash = rbuf_int(rb);
            //printf("%d in hash\n", n_in_hash);
            //START HERE
            for (i=0; i<n_in_this_hash; i++) {
                int diff;
                bytevec key; ITEMLEN keylen;
                bytevec val; ITEMLEN vallen;
                //toku_verify_counts(result);
                int type = rbuf_char(rb);
                TXNID xid  = rbuf_ulonglong(rb);
                rbuf_bytes(rb, &key, &keylen); /* Returns a pointer into the rbuf. */
                rbuf_bytes(rb, &val, &vallen);
                check_local_fingerprint += result->rand4fingerprint * calc_fingerprint_cmd10(type, xid, key, keylen, val, vallen);
                //printf("Found %s,%s\n", (char*)key, (char*)val);
                {
                    r=toku_fifo10_enq(BNC_BUFFER(result, cnum), key, keylen, val, vallen, type, xid); /* Copies the data into the hash table. */
                    if (r!=0) { goto died_1; }
                }
                diff =  keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD_10;
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
    (void)rbuf_int(rb); //Ignore the crc (already verified).
    if (rb->ndone != rb->size) { //Verify we read exactly the entire block.
        r = toku_db_badformat(); goto died_1;
    }
    return 0;
}

static int
deserialize_brtnode_from_rbuf_10 (BLOCKNUM blocknum, u_int32_t fullhash, BRTNODE *brtnode, struct brt_header *h, struct rbuf *rb) {
    TAGMALLOC(BRTNODE, result);
    int r;
    if (result==0) {
	r=errno;
	if (0) { died0: toku_free(result); }
	return r;
    }
    result->desc = &h->descriptor;
    result->ever_been_written = 1;

    //printf("Deserializing %lld datasize=%d\n", off, datasize);
    bytevec magic;
    rbuf_literal_bytes(rb, &magic, 8);
    result->layout_version    = rbuf_int(rb);
    assert(result->layout_version == BRT_LAYOUT_VERSION_10);
    result->disk_lsn.lsn = rbuf_ulonglong(rb);
    {
        //Restrict scope for now since we do not support upgrades.
        struct descriptor desc;
        //desc.dbt.data is TEMPORARY.  Will be unusable when the rc buffer is freed.
        deserialize_descriptor_from_rbuf(rb, &desc, TRUE);
        assert(desc.version == result->desc->version); //We do not yet support upgrading the dbts.
    }
    result->nodesize = rbuf_int(rb);
    result->log_lsn = result->disk_lsn;

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
        r = deserialize_brtnode_nonleaf_from_rbuf_10(result, magic, rb);
    else
        r = deserialize_brtnode_leaf_from_rbuf_10(result, magic, rb);
    if (r!=0) goto died0;

    //printf("%s:%d Ok got %lld n_children=%d\n", __FILE__, __LINE__, result->thisnodename, result->n_children);
    if (result->height>0) {
	// For height==0 we used the buf inside the OMT
	toku_free(rb->buf);
        rb->buf = NULL;
    }
    toku_trace("deserial done");
    *brtnode = result;
    //toku_verify_counts(result);
    return 0;
}

static void le_unpack_le10_committed(u_int32_t klen, void *kval, u_int32_t vallen, void *val, ULE ule) {
    //Committed value
    toku_upgrade_ule_init_empty_ule(ule, klen, kval);
    toku_upgrade_ule_remove_innermost_uxr(ule);	// pop committed delete
    toku_upgrade_ule_push_insert_uxr(ule, 0, vallen, val);
}

static void le_unpack_le10_both(TXNID xid, u_int32_t klen, void *kval, u_int32_t clen, void *cval, u_int32_t plen, void *pval, ULE ule) {
    if (xid==0) {
        //Really committed
        le_unpack_le10_committed(klen, kval, plen, pval, ule);
    }
    else {
        //committed value and provisional insert
        toku_upgrade_ule_init_empty_ule(ule, klen, kval);
        toku_upgrade_ule_remove_innermost_uxr(ule);	// pop committed delete
        toku_upgrade_ule_push_insert_uxr(ule, 0, clen, cval);	// push committed 
        toku_upgrade_ule_push_insert_uxr(ule, xid, plen, pval);	// push provisional
    }
}

static void le_unpack_le10_provdel(TXNID xid, u_int32_t klen, void *kval, u_int32_t clen, void *cval, ULE ule) {
    if (xid==0) {
        //Really committed delete
        toku_upgrade_ule_init_empty_ule(ule, klen, kval);
    }
    else {
        //committed value and provisional delete
        toku_upgrade_ule_init_empty_ule(ule, klen, kval);
        toku_upgrade_ule_remove_innermost_uxr(ule);	// pop committed delete
        toku_upgrade_ule_push_insert_uxr(ule, 0, clen, cval);	// push committed
        toku_upgrade_ule_push_delete_uxr(ule, xid);		// push provisional
    }
}

static void le_unpack_le10_provpair(TXNID xid, u_int32_t klen, void *kval, u_int32_t plen, void *pval, ULE ule) {
    if (xid==0) {
        //Really committed
        le_unpack_le10_committed(klen, kval, plen, pval, ule);
    }
    else {
        //committed delete and provisional insert
        toku_upgrade_ule_init_empty_ule(ule, klen, kval);
        toku_upgrade_ule_push_insert_uxr(ule, xid, plen, pval);	// push provisional
    }
}

//Used to unpack a version 10 record to ule, which can be packed to version 11.
static void
le_unpack_from_version_10(ULE ule, LEAFENTRY le) {
    LESWITCHCALL(le, le_unpack, ule);
    abort(); return;  // make certain compilers happy
}

static u_int32_t
le10_crc(LEAFENTRY v) {
    return x1764_memory(v, leafentry_memsize_10(v));
}

//old_le10 is opaque data only readable by accessors (Not a 'new' LEAFENTRY)
static void
upgrade_single_leafentry_10_11 (BRTNODE node, u_int32_t idx11, LEAFENTRY old_le10, OMT omt11, struct mempool *mp11) {
    //See brt_leaf_apply_cmd_once for template
    size_t newlen=0, newdisksize=0;
    LEAFENTRY new_le = NULL;
    ULE_S ule;
    int r;
    assert(old_le10);
    le_unpack_from_version_10(&ule, old_le10);
    r = le_pack(&ule,                // create packed leafentry
                &newlen, &newdisksize,
                &new_le,
                omt11, mp11, NULL);
    assert(r==0);


    //Update size of memory information and crc
    //Subtract old version 10 leafentry information
    node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + le10_disksize(old_le10);
    node->local_fingerprint     -= node->rand4fingerprint * le10_crc(old_le10);
    if (new_le) {
        //Version 10 leafentry is being upgraded
        assert(newdisksize == leafentry_disksize(new_le));

        //Add new version 10 leafentry information
        node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + newdisksize;
        node->local_fingerprint += node->rand4fingerprint*toku_le_crc(new_le);

        r = toku_omt_insert_at(omt11, new_le, idx11);
        assert(r==0);
    }
}

//Upgrade each leafentry from version 10 to 11(nested transactions)
//Need to update checksums, and memory pools
static void
upgrade_brtnode_leaf_10_11 (BRTNODE node) {
    int r;
    u_int32_t idx10 = 0;
    u_int32_t idx11 = 0;
    OMT omt10 = node->u.l.buffer;
    OMT omt11 = NULL;
    struct mempool mp10 = node->u.l.buffer_mempool;
    struct mempool mp11;
    {
        //Create a new mempool
        size_t starting_size = toku_mempool_get_size(&mp10);
        void *base = toku_xmalloc(starting_size);
        toku_mempool_init(&mp11, base, starting_size);
    }
    r = toku_omt_create(&omt11);
    assert(r==0);
    u_int32_t omt11size = toku_omt_size(node->u.l.buffer);
    while (idx10 < omt11size) {
        OMTVALUE old_le10;
        r = toku_omt_fetch(node->u.l.buffer, idx10, &old_le10, NULL);
        assert(r==0);
        assert(old_le10);
        upgrade_single_leafentry_10_11(node, idx11, old_le10, omt11, &mp11);

        u_int32_t omtsize11 = toku_omt_size(omt11);
        if (omtsize11 != idx11) {
            assert(omtsize11 == idx11+1);
            //Leafentry survived (insert)
            idx11++;
        }
        idx10++; //Always advance the old omt
    }
    //Free the old mempool
    {
        void *mpbase = toku_mempool_get_base(&mp10);
        toku_mempool_fini(&mp10);
        toku_free(mpbase);
    }
    //Free the old omt
    toku_omt_destroy(&omt10);

    //Assign new mempool
    node->u.l.buffer_mempool = mp11;
    //Assign new omt
    node->u.l.buffer = omt11;
    //Calculate statistics
    toku_calculate_leaf_stats(node);
}

static void
upgrade_brtnode_nonleaf_10_11 (BRTNODE node) {
    int i;
    int r;
    for (i=0; i<node->u.n.n_children; i++) {
        FIFO fifo11;
        FIFO fifo10 = BNC_BUFFER(node,i);
        BNC_BUFFER(node,i) = NULL;
        r = toku_fifo_create(&fifo11);
        assert(r==0);

        FIFO10_ITERATE(fifo10, keyp, keylen, valp, vallen, type, xid,
            XIDS xids;
            if (xid == 0)
                xids = xids_get_root_xids(); 
            else {
                //Assume all transactions have no parents.
                r = xids_create_child(xids_get_root_xids(), &xids, xid);
                assert(r==0);
            }
            //Remove checksum contribution of this fifo_entry
            node->local_fingerprint -= node->rand4fingerprint * calc_fingerprint_cmd10(type, xid, keyp, keylen, valp, vallen);
            //Remove bytes_in_buf contribution of this fifo_entry
            u_int32_t bytes10 = keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD_10;
            node->u.n.n_bytes_in_buffers -= bytes10;
            BNC_NBYTESINBUF(node, i)     -= bytes10;
            

            //Add checksum contribution of the new fifo_entry
            node->local_fingerprint += node->rand4fingerprint * toku_calc_fingerprint_cmd(type, xids, keyp, keylen, valp, vallen);
            //Add bytes_in_buf contribution of the new fifo_entry
            u_int32_t bytes11 = keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_get_serialize_size(xids);
            node->u.n.n_bytes_in_buffers += bytes11;
            BNC_NBYTESINBUF(node, i)     += bytes11;

            //Enqueue new fifo entry
            r = toku_fifo_enq(fifo11, keyp, keylen, valp, vallen, type, xids);
            assert(r==0);
            xids_destroy(&xids);

        );
        toku_fifo10_free(&fifo10);
        BNC_BUFFER(node,i) = fifo11;
        fifo11 = NULL;
    }
}

// Structure of brtnode is same for versions 10, 11.  The only difference is in the 
// contents of the leafentries and the messages.  For this reason, the outer structure
// of the brtnode is left in place (*brtnode_10 is reused.)
static int
upgrade_brtnode_10_11 (BRTNODE *brtnode_10, BRTNODE *brtnode_11) {
    if ((*brtnode_10)->height>0) 
        upgrade_brtnode_nonleaf_10_11(*brtnode_10);
    else
        upgrade_brtnode_leaf_10_11(*brtnode_10);
    *brtnode_11 = *brtnode_10;
    *brtnode_10 = NULL;
    (*brtnode_11)->layout_version = BRT_LAYOUT_VERSION_11;
    (*brtnode_11)->dirty = 1;
    return 0;
}


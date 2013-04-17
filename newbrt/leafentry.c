#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

u_int32_t toku_le_crc(LEAFENTRY v) {
    return x1764_memory(v, leafentry_memsize(v));
}


//TODO: #1125 delete function
static void *
le10_malloc(OMT omt, struct mempool *mp, size_t size, void **maybe_free)
{
    if (omt)
	return mempool_malloc_from_omt(omt, mp, size, maybe_free);
    else
	return toku_malloc(size);
}

//Constructors for version 10 leafentries, possibly needed for upgrades.
int
le10_committed (u_int32_t klen, void* kval, u_int32_t dlen, void* dval, u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result,
	      OMT omt, struct mempool *mp, void **maybe_free) {
    size_t size = 9+klen+dlen;
    unsigned char *lec=le10_malloc(omt, mp, size, maybe_free);
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
	     u_int32_t *resultsize, u_int32_t *disksize, LEAFENTRY *result,
	     OMT omt, struct mempool *mp, void **maybe_free) {
    size_t size = 1+8+4*3+klen+clen+plen;
    unsigned char *lec=le10_malloc(omt, mp, size, maybe_free);
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
	    u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result,
	    OMT omt, struct mempool *mp, void **maybe_free) {
    size_t size = 1 + 8 + 2*4 + klen + dlen;
    unsigned char *lec= le10_malloc(omt, mp, size, maybe_free);
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
le10_provpair (TXNID xid, u_int32_t klen, void* kval, u_int32_t plen, void* pval, u_int32_t *memsize, u_int32_t *disksize, LEAFENTRY *result,
	     OMT omt, struct mempool *mp, void **maybe_free) {
    size_t size = 1 + 8 + 2*4 + klen + plen;
    unsigned char *lec= le10_malloc(omt, mp, size, maybe_free);
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

#if 0 //Needed for upgrade (probably)
//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t memsize_le10_committed (u_int32_t keylen, void *key __attribute__((__unused__)),
				       u_int32_t vallen, void *val __attribute__((__unused__))) {
    return 1+ 2*4 + keylen + vallen;
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t memsize_le10_both (TXNID txnid __attribute__((__unused__)),
				  u_int32_t klen, void *kval __attribute__((__unused__)),
				  u_int32_t clen, void *cval __attribute__((__unused__)),
				  u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*3 + klen + clen + plen;
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t memsize_le10_provdel (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t clen, void *cval __attribute__((__unused__))) {
    return 1 + 8 + 4*2 + klen + clen;
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t memsize_le10_provpair (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*2 + klen + plen;
}

u_int32_t leafentry_memsize_10 (LEAFENTRY le) {
    LESWITCHCALL(le, memsize);
    abort(); return 0;  // make certain compilers happy
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t disksize_le10_committed (u_int32_t keylen, void *key __attribute__((__unused__)),
				       u_int32_t vallen, void *val __attribute__((__unused__))) {
    return 1 + 4 + 4 + keylen + vallen;
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t disksize_le10_both (TXNID txnid __attribute__((__unused__)),
				  u_int32_t klen, void *kval __attribute__((__unused__)),
				  u_int32_t clen, void *cval __attribute__((__unused__)),
				  u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4*3 + klen + clen + plen;
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t disksize_le10_provdel (TXNID txnid __attribute__((__unused__)),
				     u_int32_t klen, void *kval __attribute__((__unused__)),
				     u_int32_t clen, void *cval __attribute__((__unused__))) {
    return 1 + 8 + 4 + 4 + klen + clen;
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t disksize_le10_provpair (TXNID txnid __attribute__((__unused__)),
				       u_int32_t klen, void *kval __attribute__((__unused__)),
				       u_int32_t plen, void *pval __attribute__((__unused__))) {
    return 1 + 8 + 4 + 4 + klen + plen;
}


//TODO: #1125 FUNCTION NEEDED for upgrading?
static u_int32_t
le10_disksize_internal (LEAFENTRY le) {
    LESWITCHCALL(le, disksize);
    abort(); return 0;  // make certain compilers happy
}

//TODO: #1125 FUNCTION NEEDED for upgrading?
u_int32_t le10_disksize (LEAFENTRY le) {
    u_int32_t d = le10_disksize_internal(le);
#if 0
    // this computation is currently identical to the _disksize_internal
    u_int32_t m = le10_memsize(le);
    assert(m==d);
#endif
    return d;
}
#endif

void wbuf_LEAFENTRY(struct wbuf *w, LEAFENTRY le) {
    wbuf_literal_bytes(w, le, leafentry_disksize(le));
}


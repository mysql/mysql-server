#ifndef RBUF_H
#define RBUF_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "memarena.h"
#include "toku_assert.h"
#include "brttypes.h"
#include "memory.h"
#include "toku_htonl.h"

struct rbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
};

static inline void rbuf_init(struct rbuf *r, unsigned char *buf, unsigned int size) {
    r->buf = buf;
    r->size = size;
    r->ndone = 0;
}

static inline unsigned int rbuf_get_roffset(struct rbuf *r) {
    return r->ndone;
}

static inline unsigned int rbuf_char (struct rbuf *r) {
    assert(r->ndone<r->size);
    return r->buf[r->ndone++];
}

static inline void rbuf_ma_u_int8_t (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), u_int8_t *num) {
    *num = rbuf_char(r);
}

//Read an int that MUST be in network order regardless of disk order
static unsigned int rbuf_network_int (struct rbuf *r) __attribute__((__unused__));
static unsigned int rbuf_network_int (struct rbuf *r) {
    assert(r->ndone+4 <= r->size);
    u_int32_t result = toku_ntohl(*(u_int32_t*)(r->buf+r->ndone)); // This only works on machines where unaligned loads are OK.
    r->ndone+=4;
    return result;
}

static unsigned int rbuf_int (struct rbuf *r) {
#if 1
    assert(r->ndone+4 <= r->size);
    u_int32_t result = toku_dtoh32(*(u_int32_t*)(r->buf+r->ndone)); // This only works on machines where unaligned loads are OK.
    r->ndone+=4;
    return result;
#else
    unsigned char c0 = rbuf_char(r);
    unsigned char c1 = rbuf_char(r);
    unsigned char c2 = rbuf_char(r);
    unsigned char c3 = rbuf_char(r);
    return ((c0<<24)|
	    (c1<<16)|
	    (c2<<8)|
	    (c3<<0));
#endif
}

static inline void rbuf_literal_bytes (struct rbuf *r, bytevec *bytes, unsigned int n_bytes) {
    *bytes =   &r->buf[r->ndone];
    r->ndone+=n_bytes;
    assert(r->ndone<=r->size);
}

/* Return a pointer into the middle of the buffer. */
static inline void rbuf_bytes (struct rbuf *r, bytevec *bytes, unsigned int *n_bytes)
{
    *n_bytes = rbuf_int(r);
    rbuf_literal_bytes(r, bytes, *n_bytes);
}

static inline unsigned long long rbuf_ulonglong (struct rbuf *r) {
    unsigned i0 = rbuf_int(r);
    unsigned i1 = rbuf_int(r);
    return ((unsigned long long)(i0)<<32) | ((unsigned long long)(i1));
}

static inline signed long long rbuf_longlong (struct rbuf *r) {
    return (signed long long)rbuf_ulonglong(r);
}

static inline DISKOFF rbuf_diskoff (struct rbuf *r) {
    return rbuf_ulonglong(r);
}

static inline LSN rbuf_lsn (struct rbuf *r) {
    LSN lsn = {rbuf_ulonglong(r)};
    return lsn;
}

static inline BLOCKNUM rbuf_blocknum (struct rbuf *r) {
    BLOCKNUM result = make_blocknum(rbuf_longlong(r));
    return result;
}
static inline void rbuf_ma_BLOCKNUM (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), BLOCKNUM *blocknum) {
    *blocknum = rbuf_blocknum(r);
}

static inline void rbuf_ma_u_int32_t (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), u_int32_t *num) {
    *num = rbuf_int(r);
}

static inline void rbuf_ma_u_int64_t (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), u_int64_t *num) {
    *num = rbuf_ulonglong(r);
}


static inline void rbuf_TXNID (struct rbuf *r, TXNID *txnid) {
    *txnid = rbuf_ulonglong(r);
}
static inline void rbuf_ma_TXNID (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), TXNID *txnid) {
    rbuf_TXNID(r, txnid);
}

static inline void rbuf_FILENUM (struct rbuf *r, FILENUM *filenum) {
    filenum->fileid = rbuf_int(r);
}
static inline void rbuf_ma_FILENUM (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), FILENUM *filenum) {
    rbuf_FILENUM(r, filenum);
}

// Don't try to use the same space, malloc it
static inline void rbuf_BYTESTRING (struct rbuf *r, BYTESTRING *bs) {
    bs->len  = rbuf_int(r);
    u_int32_t newndone = r->ndone + bs->len;
    assert(newndone <= r->size);
    bs->data = toku_memdup(&r->buf[r->ndone], (size_t)bs->len);
    assert(bs->data);
    r->ndone = newndone;
}

static inline void rbuf_ma_BYTESTRING  (struct rbuf *r, MEMARENA ma, BYTESTRING *bs) {
    bs->len  = rbuf_int(r);
    u_int32_t newndone = r->ndone + bs->len;
    assert(newndone <= r->size);
    bs->data = memarena_memdup(ma, &r->buf[r->ndone], (size_t)bs->len);
    assert(bs->data);
    r->ndone = newndone;
}

#endif

#ifndef RBUF_H
#define RBUF_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "toku_assert.h"
#include "memory.h"
#include "memarena.h"
#include <arpa/inet.h>

struct rbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
};

static inline unsigned int rbuf_char (struct rbuf *r) {
    assert(r->ndone<r->size);
    return r->buf[r->ndone++];
}

static unsigned int rbuf_int (struct rbuf *r) {
#if 1
    assert(r->ndone+4 <= r->size);
    u_int32_t result = ntohl(*(u_int32_t*)(r->buf+r->ndone)); // This only works on machines where unaligned loads are OK.
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

static inline DISKOFF rbuf_diskoff (struct rbuf *r) {
    unsigned i0 = rbuf_int(r);  
    unsigned i1 = rbuf_int(r);
    return ((unsigned long long)(i0)<<32) | ((unsigned long long)(i1));
}

static inline void rbuf_TXNID (struct rbuf *r, TXNID *txnid) {
    *txnid = rbuf_ulonglong(r);
}
static inline void rbuf_ma_TXNID (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), TXNID *txnid) {
    return rbuf_TXNID(r, txnid);
}

static inline void rbuf_FILENUM (struct rbuf *r, FILENUM *filenum) {
    filenum->fileid = rbuf_int(r);
}
static inline void rbuf_ma_FILENUM (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), FILENUM *filenum) {
    return rbuf_FILENUM(r, filenum);
}

// Don't try to use the same space, malloc it
static inline void rbuf_BYTESTRING (struct rbuf *r, BYTESTRING *bs) {
    bs->len  = rbuf_int(r);
    u_int32_t newndone = r->ndone + bs->len;
    assert(newndone < r->size);
    bs->data = toku_memdup(&r->buf[r->ndone], (size_t)bs->len);
    assert(bs->data);
    r->ndone = newndone;
}

static inline void rbuf_ma_BYTESTRING  (struct rbuf *r, MEMARENA ma, BYTESTRING *bs) {
    bs->len  = rbuf_int(r);
    u_int32_t newndone = r->ndone + bs->len;
    assert(newndone < r->size);
    bs->data = memarena_memdup(ma, &r->buf[r->ndone], (size_t)bs->len);
    assert(bs->data);
    r->ndone = newndone;
}

#endif

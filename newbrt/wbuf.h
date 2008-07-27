#ifndef WBUF_H
#define WBUF_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "x1764.h"
#include "memory.h"
#include "toku_assert.h"
#include <errno.h>
#include <string.h>

#define CRC_INCR

/* When serializing a value, write it into a buffer. */
/* This code requires that the buffer be big enough to hold whatever you put into it. */ 
/* This abstraction doesn't do a good job of hiding its internals.
 * Why?  The performance of this code is important, and we want to inline stuff */
struct wbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
    struct x1764  checksum;    // The checksumx state
};

static inline void wbuf_init (struct wbuf *w, void *buf, DISKOFF size) {
    w->buf=buf;
    w->size=size;
    w->ndone=0;
    x1764_init(&w->checksum);
}

/* Write a character. */
static inline void wbuf_char (struct wbuf *w, unsigned int ch) {
    assert(w->ndone<w->size);
    w->buf[w->ndone++]=ch;
    x1764_add(&w->checksum, &w->buf[w->ndone-1], 1);
}

static void wbuf_int (struct wbuf *w, int32_t i) {
#if 0
    wbuf_char(w, i>>24);
    wbuf_char(w, i>>16);
    wbuf_char(w, i>>8);
    wbuf_char(w, i>>0);
#else
    assert(w->ndone + 4 <= w->size);
 #if 0
    w->buf[w->ndone+0] = i>>24;
    w->buf[w->ndone+1] = i>>16;
    w->buf[w->ndone+2] = i>>8;
    w->buf[w->ndone+3] = i>>0;
 #else
    *(u_int32_t*)(&w->buf[w->ndone]) = htonl(i);
 #endif
    x1764_add(&w->checksum, &w->buf[w->ndone], 4);
    w->ndone += 4;
#endif
}
static void wbuf_uint (struct wbuf *w, u_int32_t i) {
    wbuf_int(w, (int32_t)i);
}

static inline void wbuf_literal_bytes(struct wbuf *w, bytevec bytes_bv, u_int32_t nbytes) {
    const unsigned char *bytes=bytes_bv; 
#if 0
    { int i; for (i=0; i<nbytes; i++) wbuf_char(w, bytes[i]); }
#else
    assert(w->ndone + nbytes <= w->size);
    memcpy(w->buf + w->ndone, bytes, (size_t)nbytes);
    x1764_add(&w->checksum, &w->buf[w->ndone], nbytes);
    w->ndone += nbytes;
#endif
    
}

static void wbuf_bytes (struct wbuf *w, bytevec bytes_bv, u_int32_t nbytes) {
    wbuf_uint(w, nbytes);
    wbuf_literal_bytes(w, bytes_bv, nbytes);
}

static void wbuf_ulonglong (struct wbuf *w, u_int64_t ull) {
    wbuf_uint(w, (u_int32_t)(ull>>32));
    wbuf_uint(w, (u_int32_t)(ull&0xFFFFFFFF));
}

static inline void wbuf_BYTESTRING (struct wbuf *w, BYTESTRING v) {
    wbuf_bytes(w, v.data, v.len);
}

static inline void wbuf_u_int8_t (struct wbuf *w, u_int8_t v) {
    wbuf_char(w, v);
}

static inline void wbuf_u_int32_t (struct wbuf *w, u_int32_t v) {
    wbuf_uint(w, v);
}

static inline void wbuf_DISKOFF (struct wbuf *w, DISKOFF off) {
    wbuf_ulonglong(w, (u_int64_t)off);
}

static inline void wbuf_TXNID (struct wbuf *w, TXNID tid) {
    wbuf_ulonglong(w, tid);
}

static inline void wbuf_LSN (struct wbuf *w, LSN lsn) {
    wbuf_ulonglong(w, lsn.lsn);
}

static inline void wbuf_FILENUM (struct wbuf *w, FILENUM fileid) {
    wbuf_uint(w, fileid.fileid);
}

static inline void wbuf_LOGGEDBRTHEADER (struct wbuf *w, LOGGEDBRTHEADER h) {
    wbuf_uint(w, h.size);
    wbuf_uint(w, h.flags);
    wbuf_uint(w, h.nodesize);
    wbuf_DISKOFF(w, h.freelist);
    wbuf_DISKOFF(w, h.unused_memory);
    wbuf_int(w, h.n_named_roots);
    if ((signed)h.n_named_roots==-1) {
	wbuf_DISKOFF(w, h.u.one.root);
    } else {
	int i;
	for (i=0; i<h.n_named_roots; i++) {
	    wbuf_DISKOFF(w, h.u.many.roots[i]);
	    wbuf_bytes  (w, h.u.many.names[i], (u_int32_t)(1+strlen(h.u.many.names[i])));
	}
    }
}

static inline void wbuf_INTPAIRARRAY (struct wbuf *w, INTPAIRARRAY h) {
    u_int32_t i;
    wbuf_uint(w, h.size);
    for (i=0; i<h.size; i++) {
	wbuf_uint(w, h.array[i].a);
	wbuf_uint(w, h.array[i].b);
    }
}

#endif

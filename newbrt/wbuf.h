#ifndef WBUF_H
#define WBUF_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <string.h>
#include <errno.h>
#include "memory.h"

//#define CRC_NO
#define CRC_INCR
//#define CRC_ATEND

#ifndef CRC_NO
#include "crc.h"
#endif

/* When serializing a value, write it into a buffer. */
/* This code requires that the buffer be big enough to hold whatever you put into it. */ 
/* This abstraction doesn't do a good job of hiding its internals.
 * Why?  The performance of this code is important, and we want to inline stuff */
struct wbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
#ifdef CRC_INCR
    u_int32_t     crc32; // A 32-bit CRC of everything written so foar.
#endif
};

static void wbuf_init (struct wbuf *w, void *buf, DISKOFF size) {
    w->buf=buf;
    w->size=size;
    w->ndone=0;
#ifdef CRC_INCR
    w->crc32 = toku_crc32(0L, Z_NULL, 0);
#endif
}

/* Write a character. */
static inline void wbuf_char (struct wbuf *w, int ch) {
    assert(w->ndone<w->size);
    w->buf[w->ndone++]=ch;
#ifdef CRC_INCR
    w->crc32 = toku_crc32(w->crc32, &w->buf[w->ndone-1], 1);
#endif
}

static void wbuf_int (struct wbuf *w, unsigned int i) {
#if 0
    wbuf_char(w, i>>24);
    wbuf_char(w, i>>16);
    wbuf_char(w, i>>8);
    wbuf_char(w, i>>0);
#else
    assert(w->ndone + 4 <= w->size);
    w->buf[w->ndone+0] = i>>24;
    w->buf[w->ndone+1] = i>>16;
    w->buf[w->ndone+2] = i>>8;
    w->buf[w->ndone+3] = i>>0;
 #ifdef CRC_INCR
    w->crc32 = toku_crc32(w->crc32, &w->buf[w->ndone], 4);
 #endif
    w->ndone += 4;
#endif
}

static inline void wbuf_literal_bytes(struct wbuf *w, bytevec bytes_bv, int nbytes) {
    const unsigned char *bytes=bytes_bv; 
#if 0
    { int i; for (i=0; i<nbytes; i++) wbuf_char(w, bytes[i]); }
#else
    assert(w->ndone + nbytes <= w->size);
    memcpy(w->buf + w->ndone, bytes, nbytes);
 #ifdef CRC_INCR
    w->crc32 = toku_crc32(w->crc32, &w->buf[w->ndone], nbytes);
 #endif
    w->ndone += nbytes;
#endif
    
}

static void wbuf_bytes (struct wbuf *w, bytevec bytes_bv, int nbytes) {
    wbuf_int(w, nbytes);
    wbuf_literal_bytes(w, bytes_bv, nbytes);
}

static void wbuf_ulonglong (struct wbuf *w, unsigned long long ull) {
    wbuf_int(w, ull>>32);
    wbuf_int(w, ull&0xFFFFFFFF);
}

static inline void wbuf_BYTESTRING (struct wbuf *w, BYTESTRING v) {
    wbuf_bytes(w, v.data, v.len);
}

static inline void wbuf_u_int8_t (struct wbuf *w, u_int8_t v) {
    wbuf_char(w, v);
}

static inline void wbuf_u_int32_t (struct wbuf *w, u_int32_t v) {
    wbuf_int(w, v);
}

static inline void wbuf_DISKOFF (struct wbuf *w, DISKOFF off) {
    wbuf_ulonglong(w, off);
}

static inline void wbuf_TXNID (struct wbuf *w, TXNID tid) {
    wbuf_ulonglong(w, tid);
}

static inline void wbuf_LSN (struct wbuf *w, LSN lsn) {
    wbuf_ulonglong(w, lsn.lsn);
}

static inline void wbuf_FILENUM (struct wbuf *w, FILENUM fileid) {
    wbuf_int(w, fileid.fileid);
}

static inline void wbuf_LOGGEDBRTHEADER (struct wbuf *w, LOGGEDBRTHEADER h) {
    wbuf_int(w, h.size);
    wbuf_int(w, h.flags);
    wbuf_int(w, h.nodesize);
    wbuf_DISKOFF(w, h.freelist);
    wbuf_DISKOFF(w, h.unused_memory);
    wbuf_int(w, h.n_named_roots);
    if ((signed)h.n_named_roots==-1) {
	wbuf_DISKOFF(w, h.u.one.root);
    } else {
	unsigned int i;
	for (i=0; i<h.n_named_roots; i++) {
	    wbuf_DISKOFF(w, h.u.many.roots[i]);
	    wbuf_bytes  (w, h.u.many.names[i], 1+strlen(h.u.many.names[i]));
	}
    }
}

static inline void wbuf_INTPAIRARRAY (struct wbuf *w, INTPAIRARRAY h) {
    u_int32_t i;
    wbuf_int(w, h.size);
    for (i=0; i<h.size; i++) {
	wbuf_int(w, h.array[i].a);
	wbuf_int(w, h.array[i].b);
    }
}

#endif

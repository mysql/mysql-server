#ifndef WBUF_H
#define WBUF_H

#include <assert.h>
#include <string.h>
#include <errno.h>
#include "memory.h"

/* When serializing a value, write it into a buffer. */
/* This code requires that the buffer be big enough to hold whatever you put into it. */ 
/* This abstraction doesn't do a good job of hiding its internals.
 * Why?  The performance of this code is important, and we want to inline stuff */
struct wbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
};

static void wbuf_init (struct wbuf *w, void *buf, diskoff size) {
    w->buf=buf;
    w->size=size;
    w->ndone=0;
}

/* Write a character. */
static inline void wbuf_char (struct wbuf *w, int ch) {
    assert(w->ndone<w->size);
    w->buf[w->ndone++]=ch;
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
    w->ndone += 4;
#endif
}

static void wbuf_bytes (struct wbuf *w, bytevec bytes_bv, int nbytes) {
    const unsigned char *bytes=bytes_bv; 
    wbuf_int(w, nbytes);
#if 0
    { int i; for (i=0; i<nbytes; i++) wbuf_char(w, bytes[i]); }
#else
    assert(w->ndone + nbytes <= w->size);
    memcpy(w->buf + w->ndone, bytes, nbytes);
    w->ndone += nbytes;
#endif
}

static void wbuf_ulonglong (struct wbuf *w, unsigned long long ull) {
    wbuf_int(w, ull>>32);
    wbuf_int(w, ull&0xFFFFFFFF);
}

static void wbuf_diskoff (struct wbuf *w, diskoff off) {
    wbuf_ulonglong(w, off);
}

static inline void wbuf_txnid (struct wbuf *w, TXNID tid) {
    wbuf_ulonglong(w, tid);
}

static inline void wbuf_fileid (struct wbuf *w, unsigned long long fileid) {
    wbuf_ulonglong(w, fileid);
}

#endif

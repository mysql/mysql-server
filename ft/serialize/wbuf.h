/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <memory.h>
#include <string.h>

#include "portability/toku_htonl.h"

#include "util/bytestring.h"
#include "util/x1764.h"

/* When serializing a value, write it into a buffer. */
/* This code requires that the buffer be big enough to hold whatever you put into it. */
/* This abstraction doesn't do a good job of hiding its internals.
 * Why?  The performance of this code is important, and we want to inline stuff */
//Why is size here an int instead of DISKOFF like in the initializer?
struct wbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
    struct x1764  checksum;    // The checksum state
};

static inline void wbuf_nocrc_init (struct wbuf *w, void *buf, unsigned int size) {
    w->buf = (unsigned char *) buf;
    w->size = size;
    w->ndone = 0;
}

static inline void wbuf_init (struct wbuf *w, void *buf, unsigned int size) {
    wbuf_nocrc_init(w, buf, size);
    toku_x1764_init(&w->checksum);
}

static inline size_t wbuf_get_woffset(struct wbuf *w) {
    return w->ndone;
}

/* Write a character. */
static inline void wbuf_nocrc_char (struct wbuf *w, unsigned char ch) {
    assert(w->ndone<w->size);
    w->buf[w->ndone++]=ch;
}

/* Write a character. */
static inline void wbuf_nocrc_uint8_t (struct wbuf *w, uint8_t ch) {
    assert(w->ndone<w->size);
    w->buf[w->ndone++]=ch;
}

static inline void wbuf_char (struct wbuf *w, unsigned char ch) {
    wbuf_nocrc_char (w, ch);
    toku_x1764_add(&w->checksum, &w->buf[w->ndone-1], 1);
}

//Write an int that MUST be in network order regardless of disk order
static void wbuf_network_int (struct wbuf *w, int32_t i) __attribute__((__unused__));
static void wbuf_network_int (struct wbuf *w, int32_t i) {
    assert(w->ndone + 4 <= w->size);
    *(uint32_t*)(&w->buf[w->ndone]) = toku_htonl(i);
    toku_x1764_add(&w->checksum, &w->buf[w->ndone], 4);
    w->ndone += 4;
}

static inline void wbuf_nocrc_int (struct wbuf *w, int32_t i) {
#if 0
    wbuf_nocrc_char(w, i>>24);
    wbuf_nocrc_char(w, i>>16);
    wbuf_nocrc_char(w, i>>8);
    wbuf_nocrc_char(w, i>>0);
#else
    assert(w->ndone + 4 <= w->size);
 #if 0
    w->buf[w->ndone+0] = i>>24;
    w->buf[w->ndone+1] = i>>16;
    w->buf[w->ndone+2] = i>>8;
    w->buf[w->ndone+3] = i>>0;
 #else
    *(uint32_t*)(&w->buf[w->ndone]) = toku_htod32(i);
 #endif
    w->ndone += 4;
#endif
}

static inline void wbuf_int (struct wbuf *w, int32_t i) {
    wbuf_nocrc_int(w, i);
    toku_x1764_add(&w->checksum, &w->buf[w->ndone-4], 4);
}

static inline void wbuf_nocrc_uint (struct wbuf *w, uint32_t i) {
    wbuf_nocrc_int(w, (int32_t)i);
}

static inline void wbuf_uint (struct wbuf *w, uint32_t i) {
    wbuf_int(w, (int32_t)i);
}

static inline uint8_t* wbuf_nocrc_reserve_literal_bytes(struct wbuf *w, uint32_t nbytes) {
    assert(w->ndone + nbytes <= w->size);
    uint8_t * dest = w->buf + w->ndone;
    w->ndone += nbytes;
    return dest;
}

static inline void wbuf_nocrc_literal_bytes(struct wbuf *w, const void *bytes_bv, uint32_t nbytes) {
    const unsigned char *bytes = (const unsigned char *) bytes_bv;
#if 0
    { int i; for (i=0; i<nbytes; i++) wbuf_nocrc_char(w, bytes[i]); }
#else
    assert(w->ndone + nbytes <= w->size);
    memcpy(w->buf + w->ndone, bytes, (size_t)nbytes);
    w->ndone += nbytes;
#endif
}

static inline void wbuf_literal_bytes(struct wbuf *w, const void *bytes_bv, uint32_t nbytes) {
    wbuf_nocrc_literal_bytes(w, bytes_bv, nbytes);
    toku_x1764_add(&w->checksum, &w->buf[w->ndone-nbytes], nbytes);
}

static void wbuf_nocrc_bytes (struct wbuf *w, const void *bytes_bv, uint32_t nbytes) {
    wbuf_nocrc_uint(w, nbytes);
    wbuf_nocrc_literal_bytes(w, bytes_bv, nbytes);
}

static void wbuf_bytes (struct wbuf *w, const void *bytes_bv, uint32_t nbytes) {
    wbuf_uint(w, nbytes);
    wbuf_literal_bytes(w, bytes_bv, nbytes);
}

static void wbuf_nocrc_ulonglong (struct wbuf *w, uint64_t ull) {
    wbuf_nocrc_uint(w, (uint32_t)(ull>>32));
    wbuf_nocrc_uint(w, (uint32_t)(ull&0xFFFFFFFF));
}

static void wbuf_ulonglong (struct wbuf *w, uint64_t ull) {
    wbuf_uint(w, (uint32_t)(ull>>32));
    wbuf_uint(w, (uint32_t)(ull&0xFFFFFFFF));
}

static inline void wbuf_nocrc_uint64_t(struct wbuf *w, uint64_t ull) {
    wbuf_nocrc_ulonglong(w, ull);
}


static inline void wbuf_uint64_t(struct wbuf *w, uint64_t ull) {
    wbuf_ulonglong(w, ull);
}

static inline void wbuf_nocrc_bool (struct wbuf *w, bool b) {
    wbuf_nocrc_uint8_t(w, (uint8_t)(b ? 1 : 0));
}

static inline void wbuf_nocrc_BYTESTRING (struct wbuf *w, BYTESTRING v) {
    wbuf_nocrc_bytes(w, v.data, v.len);
}

static inline void wbuf_BYTESTRING (struct wbuf *w, BYTESTRING v) {
    wbuf_bytes(w, v.data, v.len);
}

static inline void wbuf_uint8_t (struct wbuf *w, uint8_t v) {
    wbuf_char(w, v);
}

static inline void wbuf_nocrc_uint32_t (struct wbuf *w, uint32_t v) {
    wbuf_nocrc_uint(w, v);
}

static inline void wbuf_uint32_t (struct wbuf *w, uint32_t v) {
    wbuf_uint(w, v);
}

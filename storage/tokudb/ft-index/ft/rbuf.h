/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef RBUF_H
#define RBUF_H

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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "toku_assert.h"
#include "fttypes.h"
#include "memory.h"
#include <toku_htonl.h>

#include <util/memarena.h>

struct rbuf {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
};
#define RBUF_INITIALIZER ((struct rbuf){.buf = NULL, .size=0, .ndone=0})

static inline void rbuf_init(struct rbuf *r, unsigned char *buf, unsigned int size) {
    r->buf = buf;
    r->size = size;
    r->ndone = 0;
}

static inline unsigned int rbuf_get_roffset(struct rbuf *r) {
    return r->ndone;
}

static inline unsigned char rbuf_char (struct rbuf *r) {
    assert(r->ndone<r->size);
    return r->buf[r->ndone++];
}

static inline void rbuf_ma_uint8_t (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), uint8_t *num) {
    *num = rbuf_char(r);
}

static inline void rbuf_ma_bool (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), bool *b) {
    uint8_t n = rbuf_char(r);
    *b = (n!=0);
}

//Read an int that MUST be in network order regardless of disk order
static unsigned int rbuf_network_int (struct rbuf *r) __attribute__((__unused__));
static unsigned int rbuf_network_int (struct rbuf *r) {
    assert(r->ndone+4 <= r->size);
    uint32_t result = toku_ntohl(*(uint32_t*)(r->buf+r->ndone)); // This only works on machines where unaligned loads are OK.
    r->ndone+=4;
    return result;
}

static unsigned int rbuf_int (struct rbuf *r) {
#if 1
    assert(r->ndone+4 <= r->size);
    uint32_t result = toku_dtoh32(*(uint32_t*)(r->buf+r->ndone)); // This only works on machines where unaligned loads are OK.
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

static inline MSN rbuf_msn (struct rbuf *r) {
    MSN msn = {rbuf_ulonglong(r)};
    return msn;
}

static inline BLOCKNUM rbuf_blocknum (struct rbuf *r) {
    BLOCKNUM result = make_blocknum(rbuf_longlong(r));
    return result;
}
static inline void rbuf_ma_BLOCKNUM (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), BLOCKNUM *blocknum) {
    *blocknum = rbuf_blocknum(r);
}

static inline void rbuf_ma_uint32_t (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), uint32_t *num) {
    *num = rbuf_int(r);
}

static inline void rbuf_ma_uint64_t (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), uint64_t *num) {
    *num = rbuf_ulonglong(r);
}


static inline void rbuf_TXNID (struct rbuf *r, TXNID *txnid) {
    *txnid = rbuf_ulonglong(r);
}

static inline void rbuf_TXNID_PAIR (struct rbuf *r, TXNID_PAIR *txnid) {
    txnid->parent_id64 = rbuf_ulonglong(r);
    txnid->child_id64 = rbuf_ulonglong(r);
}

static inline void rbuf_ma_TXNID (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), TXNID *txnid) {
    rbuf_TXNID(r, txnid);
}

static inline void rbuf_ma_TXNID_PAIR (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), TXNID_PAIR *txnid) {
    rbuf_TXNID_PAIR(r, txnid);
}

static inline void rbuf_FILENUM (struct rbuf *r, FILENUM *filenum) {
    filenum->fileid = rbuf_int(r);
}
static inline void rbuf_ma_FILENUM (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), FILENUM *filenum) {
    rbuf_FILENUM(r, filenum);
}

// 2954
// Don't try to use the same space, malloc it
static inline void rbuf_FILENUMS(struct rbuf *r, FILENUMS *filenums) {
    filenums->num = rbuf_int(r);
    filenums->filenums = (FILENUM *) toku_malloc( filenums->num * sizeof(FILENUM) );
    assert(filenums->filenums != NULL);
    for (uint32_t i=0; i < filenums->num; i++) {
        rbuf_FILENUM(r, &(filenums->filenums[i]));
    }
}

// 2954
static inline void rbuf_ma_FILENUMS (struct rbuf *r, MEMARENA ma __attribute__((__unused__)), FILENUMS *filenums) {
    rbuf_ma_uint32_t(r, ma, &(filenums->num));
    filenums->filenums = (FILENUM *) toku_memarena_malloc(ma, filenums->num * sizeof(FILENUM) );
    assert(filenums->filenums != NULL);
    for (uint32_t i=0; i < filenums->num; i++) {
        rbuf_ma_FILENUM(r, ma, &(filenums->filenums[i]));
    }
}

// Don't try to use the same space, malloc it
static inline void rbuf_BYTESTRING (struct rbuf *r, BYTESTRING *bs) {
    bs->len  = rbuf_int(r);
    uint32_t newndone = r->ndone + bs->len;
    assert(newndone <= r->size);
    bs->data = (char *) toku_memdup(&r->buf[r->ndone], (size_t)bs->len);
    assert(bs->data);
    r->ndone = newndone;
}

static inline void rbuf_ma_BYTESTRING  (struct rbuf *r, MEMARENA ma, BYTESTRING *bs) {
    bs->len  = rbuf_int(r);
    uint32_t newndone = r->ndone + bs->len;
    assert(newndone <= r->size);
    bs->data = (char *) toku_memarena_memdup(ma, &r->buf[r->ndone], (size_t)bs->len);
    assert(bs->data);
    r->ndone = newndone;
}


#endif

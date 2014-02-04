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

#include <string.h>
#include <memory.h>
#include <toku_assert.h>
#include "mempool.h"

/* Contract:
 * Caller allocates mempool struct as convenient for caller, but memory used for data storage
 * must be dynamically allocated via toku_malloc().
 * Caller dynamically allocates memory for mempool and initializes mempool by calling toku_mempool_init().
 * Once a buffer is assigned to a mempool (via toku_mempool_init()), the mempool owns it and
 * is responsible for destroying it when the mempool is destroyed.
 * Caller destroys mempool by calling toku_mempool_destroy().
 *
 * Note, toku_mempool_init() does not allocate the memory because sometimes the caller will already have
 * the memory allocated and will assign the pre-allocated memory to the mempool.
 */

/* This is a constructor to be used when the memory for the mempool struct has been
 * allocated by the caller, but no memory has yet been allocatd for the data.
 */
void toku_mempool_zero(struct mempool *mp) {
    // printf("mempool_zero %p\n", mp);
    memset(mp, 0, sizeof(*mp));
}

// TODO 4050 this is dirty, try to replace all uses of this
void toku_mempool_init(struct mempool *mp, void *base, size_t free_offset, size_t size) {
    // printf("mempool_init %p %p %lu\n", mp, base, size);
    paranoid_invariant(base != 0);
    paranoid_invariant(size < (1U<<31)); // used to be assert(size >= 0), but changed to size_t so now let's make sure it's not more than 2GB...
    paranoid_invariant(free_offset <= size);
    mp->base = base;
    mp->size = size;
    mp->free_offset = free_offset;             // address of first available memory
    mp->frag_size = 0;               // byte count of wasted space (formerly used, no longer used or available)
}

/* allocate memory and construct mempool
 */
void toku_mempool_construct(struct mempool *mp, size_t data_size) {
    if (data_size) {
        size_t mpsize = data_size + (data_size/4);     // allow 1/4 room for expansion (would be wasted if read-only)
        mp->base = toku_xmalloc(mpsize);               // allocate buffer for mempool
        mp->size = mpsize;
        mp->free_offset = 0;                     // address of first available memory for new data
        mp->frag_size = 0;                       // all allocated space is now in use
    }
    else {
        toku_mempool_zero(mp);
        //        fprintf(stderr, "Empty mempool created (base constructor)\n");
    }
}


void toku_mempool_destroy(struct mempool *mp) {
    // printf("mempool_destroy %p %p %lu %lu\n", mp, mp->base, mp->size, mp->frag_size);
    if (mp->base)
        toku_free(mp->base);
    toku_mempool_zero(mp);
}

void *toku_mempool_get_base(struct mempool *mp) {
    return mp->base;
}

size_t toku_mempool_get_size(struct mempool *mp) {
    return mp->size;
}

size_t toku_mempool_get_frag_size(struct mempool *mp) {
    return mp->frag_size;
}

size_t toku_mempool_get_used_space(struct mempool *mp) {
    return mp->free_offset - mp->frag_size;
}

size_t toku_mempool_get_free_space(struct mempool *mp) {
    return mp->size - mp->free_offset;
}

size_t toku_mempool_get_allocated_space(struct mempool *mp) {
    return mp->free_offset;
}

void *toku_mempool_malloc(struct mempool *mp, size_t size, int alignment) {
    paranoid_invariant(size < (1U<<31));
    paranoid_invariant(mp->size < (1U<<31));
    paranoid_invariant(mp->free_offset < (1U<<31));
    paranoid_invariant(mp->free_offset <= mp->size);
    void *vp;
    size_t offset = (mp->free_offset + (alignment-1)) & ~(alignment-1);
    //printf("mempool_malloc size=%ld base=%p free_offset=%ld mp->size=%ld offset=%ld\n", size, mp->base, mp->free_offset, mp->size, offset);
    if (offset + size > mp->size) {
        vp = 0;
    } else {
        vp = (char *)mp->base + offset;
        mp->free_offset = offset + size;
    }
    paranoid_invariant(mp->free_offset <= mp->size);
    paranoid_invariant(((long)vp & (alignment-1)) == 0);
    paranoid_invariant(vp == 0 || toku_mempool_inrange(mp, vp, size));
    //printf("mempool returning %p\n", vp);
    return vp;
}

// if vp is null then we are freeing something, but not specifying what.  The data won't be freed until compression is done.
void toku_mempool_mfree(struct mempool *mp, void *vp, size_t size) {
    if (vp) { paranoid_invariant(toku_mempool_inrange(mp, vp, size)); }
    mp->frag_size += size;
    paranoid_invariant(mp->frag_size <= mp->size);
}


/* get memory footprint */
size_t toku_mempool_footprint(struct mempool *mp) {
    void * base = mp->base;
    size_t touched = mp->free_offset;
    size_t rval = toku_memory_footprint(base, touched);
    return rval;
}

void toku_mempool_clone(struct mempool* orig_mp, struct mempool* new_mp) {
    new_mp->frag_size = orig_mp->frag_size;
    new_mp->free_offset = orig_mp->free_offset;
    new_mp->size = orig_mp->free_offset; // only make the cloned mempool store what is needed
    new_mp->base = toku_xmalloc(new_mp->size);
    memcpy(new_mp->base, orig_mp->base, new_mp->size);
}

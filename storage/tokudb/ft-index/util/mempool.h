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

#ifndef UTIL_MEMPOOL_H
#define UTIL_MEMPOOL_H

/* a memory pool is a contiguous region of memory that supports single
   allocations from the pool.  these allocated regions are never recycled.
   when the memory pool no longer has free space, the allocated chunks
   must be relocated by the application to a new memory pool. */

#include <stddef.h>

struct mempool;

  // TODO 4050 Hide mempool struct internals from callers

struct mempool {
    void *base;           /* the base address of the memory */
    size_t free_offset;      /* the offset of the memory pool free space */
    size_t size;             /* the size of the memory */
    size_t frag_size;        /* the size of the fragmented memory */
};

/* This is a constructor to be used when the memory for the mempool struct has been
 * allocated by the caller, but no memory has yet been allocatd for the data.
 */
void toku_mempool_zero(struct mempool *mp);

/* Copy constructor.  Fill in empty mempool struct with new values, allocating
 * a new buffer and filling the buffer with data from from data_source.
 * Any time a new mempool is needed, allocate 1/4 more space
 * than is currently needed.
 */
void toku_mempool_copy_construct(struct mempool *mp, const void * const data_source, const size_t data_size);

/* initialize the memory pool with the base address and size of a
   contiguous chunk of memory */
void toku_mempool_init(struct mempool *mp, void *base, size_t free_offset, size_t size);

/* allocate memory and construct mempool
 */
void toku_mempool_construct(struct mempool *mp, size_t data_size);

/* destroy the memory pool */
void toku_mempool_destroy(struct mempool *mp);

/* get the base address of the memory pool */
void *toku_mempool_get_base(struct mempool *mp);

/* get the size of the memory pool */
size_t toku_mempool_get_size(struct mempool *mp);

/* get the amount of fragmented (wasted) space in the memory pool */
size_t toku_mempool_get_frag_size(struct mempool *mp);

/* get the amount of space that is holding useful data */
size_t toku_mempool_get_used_space(struct mempool *mp);

/* get the amount of space that is available for new data */
size_t toku_mempool_get_free_space(struct mempool *mp);

/* get the amount of space that has been allocated for use (wasted or not) */
size_t toku_mempool_get_allocated_space(struct mempool *mp);

/* allocate a chunk of memory from the memory pool suitably aligned */
void *toku_mempool_malloc(struct mempool *mp, size_t size, int alignment);

/* free a previously allocated chunk of memory.  the free only updates
   a count of the amount of free space in the memory pool.  the memory
   pool does not keep track of the locations of the free chunks */
void toku_mempool_mfree(struct mempool *mp, void *vp, size_t size);

/* verify that a memory range is contained within a mempool */
static inline int toku_mempool_inrange(struct mempool *mp, void *vp, size_t size) {
    return (mp->base <= vp) && ((char *)vp + size <= (char *)mp->base + mp->size);
}

/* get memory footprint */
size_t toku_mempool_footprint(struct mempool *mp);

void toku_mempool_clone(struct mempool* orig_mp, struct mempool* new_mp);

#endif // UTIL_MEMPOOL_H

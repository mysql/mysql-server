/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ifndef MEMORY_H
#define MEMORY_H

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

#include <stdlib.h>
#include <toku_portability.h>


/* Tokutek memory allocation functions and macros.
 * These are functions for malloc and free */

int toku_memory_startup(void) __attribute__((constructor));
void toku_memory_shutdown(void) __attribute__((destructor));

/* Generally: errno is set to 0 or a value to indicate problems. */

// Everything should call toku_malloc() instead of malloc(), and toku_calloc() instead of calloc()
// That way the tests can can, e.g.,  replace the malloc function using toku_set_func_malloc().
void *toku_calloc(size_t nmemb, size_t size)  __attribute__((__visibility__("default")));
void *toku_xcalloc(size_t nmemb, size_t size)  __attribute__((__visibility__("default")));
void *toku_malloc(size_t size)  __attribute__((__visibility__("default")));
void *toku_malloc_aligned(size_t alignment, size_t size)  __attribute__((__visibility__("default")));

// xmalloc aborts instead of return NULL if we run out of memory
void *toku_xmalloc(size_t size)  __attribute__((__visibility__("default")));
void *toku_xrealloc(void*, size_t size) __attribute__((__visibility__("default")));
void *toku_xmalloc_aligned(size_t alignment, size_t size) __attribute__((__visibility__("default")));
// Effect: Perform a os_malloc_aligned(size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
//  Fail with a resource_assert if the allocation fails (don't return an error code).
//  If the alloc_aligned function has been set then call it instead.
// Requires: alignment is a power of two.

void toku_free(void*) __attribute__((__visibility__("default")));
void *toku_realloc(void *, size_t size)  __attribute__((__visibility__("default")));
void *toku_realloc_aligned(size_t alignment, void *p, size_t size) __attribute__((__visibility__("default")));
// Effect: Perform a os_realloc_aligned(alignment, p, size) which has the additional property that the returned pointer is a multiple of ALIGNMENT.
//  If the malloc_aligned function has been set then call it instead.
// Requires: alignment is a power of two.

size_t toku_malloc_usable_size(void *p) __attribute__((__visibility__("default")));

/* MALLOC is a macro that helps avoid a common error:
 * Suppose I write
 *    struct foo *x = malloc(sizeof(struct foo));
 * That works fine.  But if I change it to this, I've probably made an mistake:
 *    struct foo *x = malloc(sizeof(struct bar));
 * It can get worse, since one might have something like
 *    struct foo *x = malloc(sizeof(struct foo *))
 * which looks reasonable, but it allocoates enough to hold a pointer instead of the amount needed for the struct.
 * So instead, write
 *    struct foo *MALLOC(x);
 * and you cannot go wrong.
 */
#define MALLOC(v) CAST_FROM_VOIDP(v, toku_malloc(sizeof(*v)))
/* MALLOC_N is like calloc(Except no 0ing of data):  It makes an array.  Write
 *   int *MALLOC_N(5,x);
 * to make an array of 5 integers.
 */
#define MALLOC_N(n,v) CAST_FROM_VOIDP(v, toku_malloc((n)*sizeof(*v)))
#define MALLOC_N_ALIGNED(align, n, v) CAST_FROM_VOIDP(v, toku_malloc_aligned((align), (n)*sizeof(*v)))


//CALLOC_N is like calloc with auto-figuring out size of members
#define CALLOC_N(n,v) CAST_FROM_VOIDP(v, toku_calloc((n), sizeof(*v)))

#define CALLOC(v) CALLOC_N(1,v)

#define REALLOC_N(n,v) CAST_FROM_VOIDP(v, toku_realloc(v, (n)*sizeof(*v)))
#define REALLOC_N_ALIGNED(align, n,v) CAST_FROM_VOIDP(v, toku_realloc_aligned((align), v, (n)*sizeof(*v)))

// XMALLOC macros are like MALLOC except they abort if the operation fails
#define XMALLOC(v) CAST_FROM_VOIDP(v, toku_xmalloc(sizeof(*v)))
#define XMALLOC_N(n,v) CAST_FROM_VOIDP(v, toku_xmalloc((n)*sizeof(*v)))
#define XCALLOC_N(n,v) CAST_FROM_VOIDP(v, toku_xcalloc((n), (sizeof(*v))))
#define XCALLOC(v) XCALLOC_N(1,(v))
#define XREALLOC(v,s) CAST_FROM_VOIDP(v, toku_xrealloc(v, s))
#define XREALLOC_N(n,v) CAST_FROM_VOIDP(v, toku_xrealloc(v, (n)*sizeof(*v)))

#define XMALLOC_N_ALIGNED(align, n, v) CAST_FROM_VOIDP(v, toku_xmalloc_aligned((align), (n)*sizeof(*v)))

#define XMEMDUP(dst, src) CAST_FROM_VOIDP(dst, toku_xmemdup(src, sizeof(*src)))
#define XMEMDUP_N(dst, src, len) CAST_FROM_VOIDP(dst, toku_xmemdup(src, len))

// ZERO_ARRAY writes zeroes to a stack-allocated array
#define ZERO_ARRAY(o) do { memset((o), 0, sizeof (o)); } while (0)
// ZERO_STRUCT writes zeroes to a stack-allocated struct
#define ZERO_STRUCT(o) do { memset(&(o), 0, sizeof (o)); } while (0)

/* Copy memory.  Analogous to strdup() */
void *toku_memdup (const void *v, size_t len);
/* Toku-version of strdup.  Use this so that it calls toku_malloc() */
char *toku_strdup (const char *s)   __attribute__((__visibility__("default")));

/* Copy memory.  Analogous to strdup() Crashes instead of returning NULL */
void *toku_xmemdup (const void *v, size_t len) __attribute__((__visibility__("default")));
/* Toku-version of strdup.  Use this so that it calls toku_xmalloc()  Crashes instead of returning NULL */
char *toku_xstrdup (const char *s)   __attribute__((__visibility__("default")));

void toku_malloc_cleanup (void); /* Before exiting, call this function to free up any internal data structures from toku_malloc.  Otherwise valgrind will complain of memory leaks. */

/* Check to see if everything malloc'd was free.  Might be a no-op depending on how memory.c is configured. */
void toku_memory_check_all_free (void);
/* Check to see if memory is "sane".  Might be a no-op.  Probably better to simply use valgrind. */
void toku_do_memory_check(void);

typedef void *(*malloc_fun_t)(size_t);
typedef void  (*free_fun_t)(void*);
typedef void *(*realloc_fun_t)(void*,size_t);
typedef void *(*malloc_aligned_fun_t)(size_t /*alignment*/, size_t /*size*/);
typedef void *(*realloc_aligned_fun_t)(size_t /*alignment*/, void */*pointer*/, size_t /*size*/);

void toku_set_func_malloc(malloc_fun_t f);
void toku_set_func_xmalloc_only(malloc_fun_t f);
void toku_set_func_malloc_only(malloc_fun_t f);
void toku_set_func_realloc(realloc_fun_t f);
void toku_set_func_xrealloc_only(realloc_fun_t f);
void toku_set_func_realloc_only(realloc_fun_t f);
void toku_set_func_free(free_fun_t f);

typedef struct memory_status {
    uint64_t malloc_count;       // number of malloc operations
    uint64_t free_count;         // number of free operations
    uint64_t realloc_count;      // number of realloc operations
    uint64_t malloc_fail;        // number of malloc operations that failed
    uint64_t realloc_fail;       // number of realloc operations that failed
    uint64_t requested;          // number of bytes requested
    uint64_t used;               // number of bytes used (requested + overhead), obtained from malloc_usable_size()
    uint64_t freed;              // number of bytes freed;
    uint64_t max_requested_size; // largest attempted allocation size
    uint64_t last_failed_size;   // size of the last failed allocation attempt
    volatile uint64_t max_in_use;      // maximum memory footprint (used - freed), approximate (not worth threadsafety overhead for exact)
    const char *mallocator_version;
    uint64_t mmap_threshold;
} LOCAL_MEMORY_STATUS_S, *LOCAL_MEMORY_STATUS;

void toku_memory_get_status(LOCAL_MEMORY_STATUS s);

size_t toku_memory_footprint(void * p, size_t touched);

#endif

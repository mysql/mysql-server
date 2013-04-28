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

#include "toku_config.h"

#include <toku_portability.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include <dlfcn.h>
#include <toku_race_tools.h>
#include "memory.h"
#include "toku_assert.h"
#include <portability/toku_atomic.h>

static malloc_fun_t  t_malloc  = 0;
static malloc_aligned_fun_t t_malloc_aligned = 0;
static malloc_fun_t  t_xmalloc = 0;
static malloc_aligned_fun_t t_xmalloc_aligned = 0;
static free_fun_t    t_free    = 0;
static realloc_fun_t t_realloc = 0;
static realloc_aligned_fun_t t_realloc_aligned = 0;
static realloc_fun_t t_xrealloc = 0;

static LOCAL_MEMORY_STATUS_S status;
int toku_memory_do_stats = 0;

static bool memory_startup_complete;

int
toku_memory_startup(void) {
    if (memory_startup_complete) {
        return 0;
    }
    memory_startup_complete = true;

    int result = 0;

#if defined(HAVE_M_MMAP_THRESHOLD)
    // initialize libc malloc
    size_t mmap_threshold = 64 * 1024; // 64K and larger should be malloced with mmap().
    int success = mallopt(M_MMAP_THRESHOLD, mmap_threshold);
    if (success) {
        status.mallocator_version = "libc";
        status.mmap_threshold = mmap_threshold;
    } else
        result = EINVAL;
#else
    // just a guess
    status.mallocator_version = "darwin";
    status.mmap_threshold = 16 * 1024;
#endif

    // jemalloc has a mallctl function, while libc malloc does not.  we can check if jemalloc 
    // is loaded by checking if the mallctl function can be found.  if it can, we call it 
    // to get version and mmap threshold configuration.
    typedef int (*mallctl_fun_t)(const char *, void *, size_t *, void *, size_t);
    mallctl_fun_t mallctl_f;
    mallctl_f = (mallctl_fun_t) dlsym(RTLD_DEFAULT, "mallctl");
    if (mallctl_f) { // jemalloc is loaded
        size_t version_length = sizeof status.mallocator_version;
        result = mallctl_f("version", &status.mallocator_version, &version_length, NULL, 0);
        if (result == 0) {
            size_t lg_chunk; // log2 of the mmap threshold
            size_t lg_chunk_length = sizeof lg_chunk;
            result  = mallctl_f("opt.lg_chunk", &lg_chunk, &lg_chunk_length, NULL, 0);
            if (result == 0)
                status.mmap_threshold = 1 << lg_chunk;
        }
    }

    return result;
}

static bool memory_shutdown_complete;

void
toku_memory_shutdown(void) {
    if (memory_shutdown_complete) {
        return;
    }
    memory_shutdown_complete = true;
}

void 
toku_memory_get_status(LOCAL_MEMORY_STATUS s) {
    *s = status;
}

// jemalloc's malloc_usable_size does not work with a NULL pointer, so we implement a version that works
static size_t
my_malloc_usable_size(void *p) {
    return p == NULL ? 0 : os_malloc_usable_size(p);
}

// Note that max_in_use may be slightly off because use of max_in_use is not thread-safe.
// It is not worth the overhead to make it completely accurate, but
// this logic is intended to guarantee that it increases monotonically.
// Note that status.sum_used and status.sum_freed increase monotonically
// and that status.max_in_use is declared volatile.
static inline void 
set_max(uint64_t sum_used, uint64_t sum_freed) {
    if (sum_used >= sum_freed) {
	uint64_t in_use = sum_used - sum_freed;
	uint64_t old_max;
	do {
	    old_max = status.max_in_use;
	} while (old_max < in_use &&
		 !toku_sync_bool_compare_and_swap(&status.max_in_use, old_max, in_use));
    }
}

size_t 
toku_memory_footprint(void * p, size_t touched) {
    static size_t pagesize = 0;
    size_t rval = 0;
    if (!pagesize)
	pagesize = sysconf(_SC_PAGESIZE);
    if (p) {
	size_t usable = my_malloc_usable_size(p);
	if (usable >= status.mmap_threshold) {
            int num_pages = (touched + pagesize) / pagesize;
            rval = num_pages * pagesize;
	}
	else {
	    rval = usable;
	}
    }
    return rval;
}

void *
toku_malloc(size_t size) {
    void *p = t_malloc ? t_malloc(size) : os_malloc(size);
    if (p) {
	TOKU_ANNOTATE_NEW_MEMORY(p, size); // see #4671 and https://bugs.kde.org/show_bug.cgi?id=297147
        if (toku_memory_do_stats) {
            size_t used = my_malloc_usable_size(p);
            toku_sync_add_and_fetch(&status.malloc_count, 1);
            toku_sync_add_and_fetch(&status.requested,size);
            toku_sync_add_and_fetch(&status.used, used);
            set_max(status.used, status.freed);
        }
    } else {
        toku_sync_add_and_fetch(&status.malloc_fail, 1);
    }
  return p;
}

void *toku_malloc_aligned(size_t alignment, size_t size) {
    void *p = t_malloc_aligned ? t_malloc_aligned(alignment, size) : os_malloc_aligned(alignment, size);
    if (p) {
	TOKU_ANNOTATE_NEW_MEMORY(p, size); // see #4671 and https://bugs.kde.org/show_bug.cgi?id=297147
        if (toku_memory_do_stats) {
            size_t used = my_malloc_usable_size(p);
            toku_sync_add_and_fetch(&status.malloc_count, 1);
            toku_sync_add_and_fetch(&status.requested,size);
            toku_sync_add_and_fetch(&status.used, used);
            set_max(status.used, status.freed);
        }
    } else {
        toku_sync_add_and_fetch(&status.malloc_fail, 1);
    }
  return p;
}

void *
toku_calloc(size_t nmemb, size_t size) {
    size_t newsize = nmemb * size;
    void *p = toku_malloc(newsize);
    if (p) memset(p, 0, newsize);
    return p;
}

void *
toku_realloc(void *p, size_t size) {
    size_t used_orig = p ? my_malloc_usable_size(p) : 0;
    void *q = t_realloc ? t_realloc(p, size) : os_realloc(p, size);
    if (q) {
        if (toku_memory_do_stats) {
            size_t used = my_malloc_usable_size(q);
            toku_sync_add_and_fetch(&status.realloc_count, 1);
            toku_sync_add_and_fetch(&status.requested, size);
            toku_sync_add_and_fetch(&status.used, used);
            toku_sync_add_and_fetch(&status.freed, used_orig);
            set_max(status.used, status.freed);
        }
    } else {
	toku_sync_add_and_fetch(&status.realloc_fail, 1);
    }
    return q;
}

void *toku_realloc_aligned(size_t alignment, void *p, size_t size) {
    size_t used_orig = p ? my_malloc_usable_size(p) : 0;
    void *q = t_realloc_aligned ? t_realloc_aligned(alignment, p, size) : os_realloc_aligned(alignment, p, size);
    if (q) {
        if (toku_memory_do_stats) {
            size_t used = my_malloc_usable_size(q);
            toku_sync_add_and_fetch(&status.realloc_count, 1);
            toku_sync_add_and_fetch(&status.requested, size);
            toku_sync_add_and_fetch(&status.used, used);
            toku_sync_add_and_fetch(&status.freed, used_orig);
            set_max(status.used, status.freed);
        }
    } else {
	toku_sync_add_and_fetch(&status.realloc_fail, 1);
    }
    return q;
}


void *
toku_memdup(const void *v, size_t len) {
    void *p = toku_malloc(len);
    if (p) memcpy(p, v,len);
    return p;
}

char *
toku_strdup(const char *s) {
    return (char *) toku_memdup(s, strlen(s)+1);
}

void
toku_free(void *p) {
    if (p) {
        if (toku_memory_do_stats) {
            size_t used = my_malloc_usable_size(p);
            toku_sync_add_and_fetch(&status.free_count, 1);
            toku_sync_add_and_fetch(&status.freed, used);
        }
	if (t_free)
	    t_free(p);
	else
	    os_free(p);
    }
}

void *
toku_xmalloc(size_t size) {
    void *p = t_xmalloc ? t_xmalloc(size) : os_malloc(size);
    if (p == NULL)  // avoid function call in common case
        resource_assert(p);
    TOKU_ANNOTATE_NEW_MEMORY(p, size); // see #4671 and https://bugs.kde.org/show_bug.cgi?id=297147
    if (toku_memory_do_stats) {
        size_t used = my_malloc_usable_size(p);
        toku_sync_add_and_fetch(&status.malloc_count, 1);
        toku_sync_add_and_fetch(&status.requested, size);
        toku_sync_add_and_fetch(&status.used, used);
        set_max(status.used, status.freed);
    }
    return p;
}

void* toku_xmalloc_aligned(size_t alignment, size_t size)
// Effect: Perform a malloc(size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
//  Fail with a resource_assert if the allocation fails (don't return an error code).
// Requires: alignment is a power of two.
{
    void *p = t_xmalloc_aligned ? t_xmalloc_aligned(alignment, size) : os_malloc_aligned(alignment,size);
    resource_assert(p);
    if (toku_memory_do_stats) {
        size_t used = my_malloc_usable_size(p);
        toku_sync_add_and_fetch(&status.malloc_count, 1);
        toku_sync_add_and_fetch(&status.requested, size);
        toku_sync_add_and_fetch(&status.used, used);
        set_max(status.used, status.freed);
    }
    return p;
}

void *
toku_xcalloc(size_t nmemb, size_t size) {
    size_t newsize = nmemb * size;
    void *vp = toku_xmalloc(newsize);
    if (vp) memset(vp, 0, newsize);
    return vp;
}

void *
toku_xrealloc(void *v, size_t size) {
    size_t used_orig = v ? my_malloc_usable_size(v) : 0;
    void *p = t_xrealloc ? t_xrealloc(v, size) : os_realloc(v, size);
    if (p == 0)  // avoid function call in common case
        resource_assert(p);
    if (toku_memory_do_stats) {
        size_t used = my_malloc_usable_size(p);
        toku_sync_add_and_fetch(&status.realloc_count, 1);
        toku_sync_add_and_fetch(&status.requested, size);
        toku_sync_add_and_fetch(&status.used, used);
        toku_sync_add_and_fetch(&status.freed, used_orig);
        set_max(status.used, status.freed);
    }
    return p;
}

size_t 
toku_malloc_usable_size(void *p) {
    return my_malloc_usable_size(p);
}

void *
toku_xmemdup (const void *v, size_t len) {
    void *p = toku_xmalloc(len);
    memcpy(p, v, len);
    return p;
}

char *
toku_xstrdup (const char *s) {
    return (char *) toku_xmemdup(s, strlen(s)+1);
}

void
toku_set_func_malloc(malloc_fun_t f) {
    t_malloc = f;
    t_xmalloc = f;
}

void
toku_set_func_xmalloc_only(malloc_fun_t f) {
    t_xmalloc = f;
}

void
toku_set_func_malloc_only(malloc_fun_t f) {
    t_malloc = f;
}

void
toku_set_func_realloc(realloc_fun_t f) {
    t_realloc = f;
    t_xrealloc = f;
}

void
toku_set_func_xrealloc_only(realloc_fun_t f) {
    t_xrealloc = f;
}

void
toku_set_func_realloc_only(realloc_fun_t f) {
    t_realloc = f;

}

void
toku_set_func_free(free_fun_t f) {
    t_free = f;
}

#include <toku_race_tools.h>
void __attribute__((constructor)) toku_memory_helgrind_ignore(void);
void
toku_memory_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&status, sizeof status);
}

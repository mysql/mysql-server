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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include <portability/toku_config.h>

#include <toku_portability.h>
#include <stdlib.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include <dlfcn.h>

#include <string.h>

// #define this to use a version of os_malloc that helps to debug certain features.
// This version uses the real malloc (so that valgrind should still work) but it forces things to be slightly
// misaligned (in particular, avoiding 512-byte alignment if possible, to find situations where O_DIRECT will fail.
// #define USE_DEBUGGING_MALLOCS

#ifdef USE_DEBUGGING_MALLOCS
#include <pthread.h>

// Make things misaligned on 512-byte boundaries
static size_t malloced_now_count=0, malloced_now_size=0;
struct malloc_pair {
    void *returned_pointer;
    void *true_pointer;
    size_t requested_size = 0;
};
static struct malloc_pair *malloced_now;
static pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void malloc_lock(void) {
    int r = pthread_mutex_lock(&malloc_mutex);
    assert(r==0);
}
static void malloc_unlock(void) {
    int r = pthread_mutex_unlock(&malloc_mutex);
    assert(r==0);
}

static void push_to_malloced_memory(void *returned_pointer, void *true_pointer, size_t requested_size) {
    malloc_lock();
    if (malloced_now_count == malloced_now_size) {
        malloced_now_size = 2*malloced_now_size + 1;
        malloced_now = (struct malloc_pair *)realloc(malloced_now, malloced_now_size * sizeof(*malloced_now));
    }
    malloced_now[malloced_now_count].returned_pointer = returned_pointer;
    malloced_now[malloced_now_count].true_pointer     = true_pointer;
    malloced_now[malloced_now_count].requested_size   = requested_size;
    malloced_now_count++;
    malloc_unlock();
}

static struct malloc_pair *find_malloced_pair(const void *p)
// Requires: Lock must be held before calling.
{
    for (size_t i=0; i<malloced_now_count; i++) {
        if (malloced_now[i].returned_pointer==p) return &malloced_now[i];
    }
    return 0;
}

void *os_malloc(size_t size) {
    void  *raw_ptr   = malloc(size+16); // allocate 16 extra bytes
    size_t raw_ptr_i = (size_t) raw_ptr; 
    if (raw_ptr_i%512==0) {
        push_to_malloced_memory(16+(char*)raw_ptr, raw_ptr, size);
        return 16+(char*)raw_ptr;
    } else {
        push_to_malloced_memory(raw_ptr,    raw_ptr, size);
        return raw_ptr;
    }
}

void *os_malloc_aligned(size_t alignment, size_t size)
// Effect: Perform a malloc(size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
// Requires: alignment is a power of two.
{
    void *p;
    int r = posix_memalign(&p, alignment, size);
    if (r != 0) {
        errno = r;
        p = nullptr;
    }
    return p;
    if (alignment%512==0) {
        void *raw_ptr;
        int r = posix_memalign(&raw_ptr, alignment, size);
        if (r != 0) {
            errno = r;
            return nullptr;
        }
        push_to_malloced_memory(raw_ptr, raw_ptr, size);
        return raw_ptr;
    } else {
        // Make sure it isn't 512-byte aligned
        void *raw_ptr;
        int r = posix_memalign(&raw_ptr, alignment, size+alignment);
        if (r != 0) {
            errno = r;
            return nullptr;
        }
        size_t raw_ptr_i = (size_t) raw_ptr;
        if (raw_ptr_i%512==0) {
            push_to_malloced_memory(alignment+(char*)raw_ptr, raw_ptr, size);
            return alignment+(char*)raw_ptr;
        } else {
            push_to_malloced_memory(raw_ptr,    raw_ptr, size);
            return raw_ptr;
        }
    }
}

static size_t min(size_t a, size_t b) {
    if (a<b) return a;
    else return b;
}

void *os_realloc(void *p, size_t size) {
    size_t alignment;
    if (size<4) {
        alignment = 1;
    } else if (size<8) {
        alignment = 4;
    } else if (size<16) {
        alignment = 8;
    } else {
        alignment = 16;
    }
    return os_realloc_aligned(alignment, p, size);
}

void * os_realloc_aligned(size_t alignment, void *p, size_t size)
// Effect: Perform a realloc(p, size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
// Requires: alignment is a power of two.
{
    if (p==NULL) {
        return os_malloc_aligned(alignment, size);
    } else {
        void *result = os_malloc_aligned(alignment, size);
        malloc_lock();
        struct malloc_pair *mp = find_malloced_pair(p);
        assert(mp);
        // now copy all the good stuff from p to result
        memcpy(result, p, min(size, mp->requested_size));
        malloc_unlock();
        os_free(p);
        return result;
    }
}


void os_free(void* p) {
    malloc_lock();
    struct malloc_pair *mp = find_malloced_pair(p);
    assert(mp);
    free(mp->true_pointer);
    *mp = malloced_now[--malloced_now_count];
    malloc_unlock();
}

size_t os_malloc_usable_size(const void *p) {
    malloc_lock();
    struct malloc_pair *mp = find_malloced_pair(p);
    assert(mp);
    size_t size = mp->requested_size;
    malloc_unlock();
    return size;
}

#else

void *
os_malloc(size_t size)
{
    return malloc(size);
}

void *os_malloc_aligned(size_t alignment, size_t size)
// Effect: Perform a malloc(size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
// Requires: alignment is a power of two.
{
    void *p;
    int r = posix_memalign(&p, alignment, size);
    if (r != 0) {
        errno = r;
        p = nullptr;
    }
    return p;
}

void *
os_realloc(void *p, size_t size)
{
    return realloc(p, size);
}

void * os_realloc_aligned(size_t alignment, void *p, size_t size)
// Effect: Perform a realloc(p, size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
// Requires: alignment is a power of two.
{
#if 1
    if (p==NULL) {
        return os_malloc_aligned(alignment, size);
    } else {
        void *newp = realloc(p, size);
        if (0!=((long long)newp%alignment)) {
            // it's not aligned, so align it ourselves.
            void *newp2 = os_malloc_aligned(alignment, size);
            memcpy(newp2, newp, size);
            free(newp);
            newp = newp2;
        }
        return newp;
    }
#else
    // THIS STUFF SEEMS TO FAIL VALGRIND
    if (p==NULL) {
        return os_malloc_aligned(alignment, size);
    } else {
        size_t ignore;
        int r = rallocm(&p,        // returned pointer
                        &ignore,   // actual size of returned object.
                        size,      // the size we want
                        0,         // extra bytes to "try" to allocate at the end
                        ALLOCM_ALIGN(alignment));
        if (r!=0) return NULL;
        else return p;
    }
#endif
}


void
os_free(void* p)
{
    free(p);
}

typedef size_t (*malloc_usable_size_fun_t)(const void *);
static malloc_usable_size_fun_t malloc_usable_size_f = NULL;

size_t os_malloc_usable_size(const void *p) {
    if (p==NULL) return 0;
    if (!malloc_usable_size_f) {
        malloc_usable_size_f = (malloc_usable_size_fun_t) dlsym(RTLD_DEFAULT, "malloc_usable_size");
        if (!malloc_usable_size_f) {
            malloc_usable_size_f = (malloc_usable_size_fun_t) dlsym(RTLD_DEFAULT, "malloc_size"); // darwin
            if (!malloc_usable_size_f) {
                abort(); // couldn't find a malloc size function
            }
        }
    }
    return malloc_usable_size_f(p);
}
#endif

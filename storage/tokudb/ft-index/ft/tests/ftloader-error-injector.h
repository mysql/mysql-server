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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <portability/toku_atomic.h>

static toku_mutex_t event_mutex = TOKU_MUTEX_INITIALIZER;
static void lock_events(void) {
    toku_mutex_lock(&event_mutex);
}
static void unlock_events(void) {
    toku_mutex_unlock(&event_mutex);
}
static int event_count, event_count_trigger;

__attribute__((__unused__))
static void reset_event_counts(void) {
    lock_events();
    event_count = event_count_trigger = 0;
    unlock_events();
}

__attribute__((__unused__))
static void event_hit(void) {
}

__attribute__((__unused__))
static int event_add_and_fetch(void) {
    lock_events();
    int r = ++event_count;
    unlock_events();
    return r;
}

static int do_user_errors = 0;

__attribute__((__unused__))
static int loader_poll_callback(void *UU(extra), float UU(progress)) {
    int r;
    if (do_user_errors && event_count_trigger == event_add_and_fetch()) {
        event_hit();
        r = 1;
    } else {
        r = 0;
    }
    return r;
}

static int do_write_errors = 0;

__attribute__((__unused__))
static size_t bad_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t r;
    if (do_write_errors && event_count_trigger == event_add_and_fetch()) {
        event_hit();
	errno = ENOSPC;
	r = (size_t) -1;
    } else {
	r = fwrite(ptr, size, nmemb, stream);
	if (r!=nmemb) {
	    errno = ferror(stream);
	}
    }
    return r;
}

__attribute__((__unused__))
static ssize_t bad_write(int fd, const void * bp, size_t len) {
    ssize_t r;
    if (do_write_errors && event_count_trigger == event_add_and_fetch()) {
        event_hit();
	errno = ENOSPC;
	r = -1;
    } else {
	r = write(fd, bp, len);
    }
    return r;
}

__attribute__((__unused__))
static ssize_t bad_pwrite(int fd, const void * bp, size_t len, toku_off_t off) {
    ssize_t r;
    if (do_write_errors && event_count_trigger == event_add_and_fetch()) {
        event_hit();
	errno = ENOSPC;
	r = -1;
    } else {
	r = pwrite(fd, bp, len, off);
    }
    return r;
}

static int do_malloc_errors = 0;
static int my_malloc_count = 0, my_big_malloc_count = 0;
static int my_realloc_count = 0, my_big_realloc_count = 0;
static size_t my_big_malloc_limit = 64*1024;
   
__attribute__((__unused__))
static void reset_my_malloc_counts(void) {
    my_malloc_count = my_big_malloc_count = 0;
    my_realloc_count = my_big_realloc_count = 0;
}

__attribute__((__unused__))
static void *my_malloc(size_t n) {
    (void) toku_sync_fetch_and_add(&my_malloc_count, 1); // my_malloc_count++;
    if (n >= my_big_malloc_limit) {
        (void) toku_sync_fetch_and_add(&my_big_malloc_count, 1); // my_big_malloc_count++;
        if (do_malloc_errors) {
            if (event_add_and_fetch() == event_count_trigger) {
                event_hit();
                errno = ENOMEM;
                return NULL;
            }
        }
    }
    return malloc(n);
}

static int do_realloc_errors = 0;

__attribute__((__unused__))
static void *my_realloc(void *p, size_t n) {
    (void) toku_sync_fetch_and_add(&my_realloc_count, 1); // my_realloc_count++;
    if (n >= my_big_malloc_limit) {
        (void) toku_sync_fetch_and_add(&my_big_realloc_count, 1); // my_big_realloc_count++;
        if (do_realloc_errors) {
            if (event_add_and_fetch() == event_count_trigger) {
                event_hit();
                errno = ENOMEM;
                return NULL;
            }
        }
    }
    return realloc(p, n);
}

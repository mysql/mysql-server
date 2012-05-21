/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: ft-serialize.c 43686 2012-05-18 23:21:00Z leifwalsh $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "sort.h"
#include "threadpool.h"
#include "ft-pwrite.h"
#include <compress.h>

//TODO(fizzfaldt): determine if this is necessary AT ALL and try to delete
// This mutex protects pwrite from running in parallel, and also protects modifications to the block allocator.
static toku_mutex_t pwrite_mutex = { PTHREAD_MUTEX_INITIALIZER };
static int pwrite_is_locked=0;

void
toku_lock_for_pwrite(void) {
    // Locks the pwrite_mutex.
    toku_mutex_lock(&pwrite_mutex);
    pwrite_is_locked = 1;
}

void
toku_unlock_for_pwrite(void) {
    pwrite_is_locked = 0;
    toku_mutex_unlock(&pwrite_mutex);
}

void
toku_full_pwrite_extend(int fd, const void *buf, size_t count, toku_off_t offset)
// requires that the pwrite has been locked
// On failure, this does not return (an assertion fails or something).
{
    invariant(pwrite_is_locked);
    {
        int r = maybe_preallocate_in_file(fd, offset+count);
        lazy_assert_zero(r);
    }
    toku_os_full_pwrite(fd, buf, count, offset);
}


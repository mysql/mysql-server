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

#ifndef UTIL_CIRCULAR_BUFFER_H
#define UTIL_CIRCULAR_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <portability/toku_pthread.h>

namespace toku {

// The circular buffer manages an array of elements as a thread-safe FIFO queue.
// It does not allocate its own space, or grow the space it manages.
// Access to the circular buffer is managed by a mutex.
// The blocking operations are managed by condition variables.  They are as fairly scheduled as the threading library supports.
//
// Sample usage:
//   int array[2]
//   circular_buffer<int> intbuf;
//   intbuf.init(array, 2);
//
//   // thread A
//   intbuf.push(1);
//   intbuf.push(2);
//   intbuf.push(3);  // <- blocks until thread B runs
//
//   // thread B
//   int a = intbuf.pop();  // <- 1
//   int b = intbuf.pop();  // <- 2
//   int c = intbuf.pop();  // <- 3
//   int d = intbuf.pop();  // <- blocks until more elements are available
template<typename T>
class circular_buffer {
public:

    // Effect:
    //  Initialize the circular buffer with an array of elements to manage.
    // Requires:
    //  array must remain valid until deinit() is called.
    void init(T * const array, size_t cap) __attribute__((nonnull));

    // Effect:
    //  Deinitialize the circular buffer.  Destroys mutex and condition variables, checks for errors.
    // Requires:
    //  Must be empty, use trypop() to drain everything before calling deinit().
    //  Must be free of waiters, no outstanding calls to push() or pop(), trypush() sentinels to flush waiters if necessary.
    void deinit(void);

    // Effect:
    //  Append elt to the end of the queue.
    // Notes:
    //  Blocks until there is room in the array.
    void push(const T &elt);

    // Effect:
    //  Append elt to the end of the queue if there's room and nobody is waiting to push.
    // Notes:
    //  Doesn't block.
    // Returns:
    //  true iff elt was appended
    bool trypush(const T &elt) __attribute__((warn_unused_result));

    // Effect:
    //  Append elt to the end of the queue if there's room before abstime.
    // Notes:
    //  Blocks until at most abstime waiting for room in the queue.  See pthread_cond_timedwait(3) for an example of how to use abstime.
    // Returns:
    //  true iff elt was appended
    bool timedpush(const T &elt, toku_timespec_t *abstime) __attribute__((nonnull, warn_unused_result));

    // Effect:
    //  Remove the first item from the queue and return it.
    // Notes:
    //  Blocks until there is something to return.
    T pop(void) __attribute__((warn_unused_result));

    // Effect:
    //  Remove the first item from the queue and return it, if one exists.
    // Notes:
    //  Doesn't block.
    //  Returns the element in *eltp.
    // Returns:
    //  true iff *eltp was set
    bool trypop(T * const eltp) __attribute__((nonnull, warn_unused_result));

    // Effect:
    //  Remove the first item from the queue and return it, if one exists before abstime
    // Notes:
    //  Blocks until at most abstime waiting for room in the queue.  See pthread_cond_timedwait(3) for an example of how to use abstime.
    //  Returns the element in *eltp.
    // Returns:
    //  true iff *eltp was set
    bool timedpop(T * const eltp, toku_timespec_t *abstime) __attribute__((nonnull, warn_unused_result));

private:
    void lock(void);

    void unlock(void);

    size_t size(void) const;

    bool is_empty(void) const;

    bool is_full(void) const;

    T *get_addr(size_t);

    void push_and_maybe_signal_unlocked(const T &elt);

    T pop_and_maybe_signal_unlocked(void);

    T *m_array;
    size_t m_cap;
    size_t m_begin, m_limit;
    toku_mutex_t m_lock;
    toku_cond_t m_push_cond;
    toku_cond_t m_pop_cond;
    int m_push_waiters, m_pop_waiters;
};

}

#include "circular_buffer.cc"

#endif // UTIL_CIRCULAR_BUFFER_H

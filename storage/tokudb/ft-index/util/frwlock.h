/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef UTIL_FRWLOCK_H
#define UTIL_FRWLOCK_H
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
#include <toku_pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/context.h>

//TODO: update comment, this is from rwlock.h

namespace toku {

class frwlock {
public:

    void init(toku_mutex_t *const mutex);
    void deinit(void);

    void write_lock(bool expensive);
    bool try_write_lock(bool expensive);
    void write_unlock(void);
    // returns true if acquiring a write lock will be expensive
    bool write_lock_is_expensive(void);

    void read_lock(void);
    bool try_read_lock(void);
    void read_unlock(void);
    // returns true if acquiring a read lock will be expensive
    bool read_lock_is_expensive(void);

    uint32_t users(void) const;
    uint32_t blocked_users(void) const;
    uint32_t writers(void) const;
    uint32_t blocked_writers(void) const;
    uint32_t readers(void) const;
    uint32_t blocked_readers(void) const;

private:
    struct queue_item {
        toku_cond_t *cond;
        struct queue_item *next;
    };

    bool queue_is_empty(void) const;
    void enq_item(queue_item *const item);
    toku_cond_t *deq_item(void);
    void maybe_signal_or_broadcast_next(void);
    void maybe_signal_next_writer(void);

    toku_mutex_t *m_mutex;

    uint32_t m_num_readers;
    uint32_t m_num_writers;
    uint32_t m_num_want_write;
    uint32_t m_num_want_read;
    uint32_t m_num_signaled_readers;
    // number of writers waiting that are expensive
    // MUST be < m_num_want_write
    uint32_t m_num_expensive_want_write;
    // bool that states if the current writer is expensive
    // if there is no current writer, then is false
    bool m_current_writer_expensive;
    // bool that states if waiting for a read
    // is expensive
    // if there are currently no waiting readers, then set to false
    bool m_read_wait_expensive;
    // thread-id of the current writer
    int m_current_writer_tid;
    // context id describing the context of the current writer blocking
    // new readers (either because this writer holds the write lock or
    // is the first to want the write lock).
    context_id m_blocking_writer_context_id;
    
    toku_cond_t m_wait_read;
    queue_item m_queue_item_read;
    bool m_wait_read_is_in_queue;

    queue_item *m_wait_head;
    queue_item *m_wait_tail;
};

ENSURE_POD(frwlock);

} // namespace toku

// include the implementation here
// #include "frwlock.cc"

#endif // UTIL_FRWLOCK_H

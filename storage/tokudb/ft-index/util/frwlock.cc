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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>

#include <util/context.h>
#include <util/frwlock.h>

namespace toku {

static __thread int thread_local_tid = -1;
static int get_local_tid() {
    if (thread_local_tid == -1) {
        thread_local_tid = toku_os_gettid();
    }
    return thread_local_tid;
}

void frwlock::init(toku_mutex_t *const mutex) {
    m_mutex = mutex;

    m_num_readers = 0;
    m_num_writers = 0;
    m_num_want_write = 0;
    m_num_want_read = 0;
    m_num_signaled_readers = 0;
    m_num_expensive_want_write = 0;
    
    toku_cond_init(&m_wait_read, nullptr);
    m_queue_item_read.cond = &m_wait_read;
    m_queue_item_read.next = nullptr;
    m_wait_read_is_in_queue = false;
    m_current_writer_expensive = false;
    m_read_wait_expensive = false;
    m_current_writer_tid = -1;
    m_blocking_writer_context_id = CTX_INVALID;

    m_wait_head = nullptr;
    m_wait_tail = nullptr;
}

void frwlock::deinit(void) {
    toku_cond_destroy(&m_wait_read);
}

bool frwlock::queue_is_empty(void) const {
    return m_wait_head == nullptr;
}

void frwlock::enq_item(queue_item *const item) {
    paranoid_invariant_null(item->next);
    if (m_wait_tail != nullptr) {
        m_wait_tail->next = item;
    } else {
        paranoid_invariant_null(m_wait_head);
        m_wait_head = item;
    }
    m_wait_tail = item;
}

toku_cond_t *frwlock::deq_item(void) {
    paranoid_invariant_notnull(m_wait_head);
    paranoid_invariant_notnull(m_wait_tail);
    queue_item *item = m_wait_head;
    m_wait_head = m_wait_head->next;
    if (m_wait_tail == item) {
        m_wait_tail = nullptr;
    }
    return item->cond;
}

// Prerequisite: Holds m_mutex.
void frwlock::write_lock(bool expensive) {
    toku_mutex_assert_locked(m_mutex);
    if (this->try_write_lock(expensive)) {
        return;
    }

    toku_cond_t cond = TOKU_COND_INITIALIZER;
    queue_item item = { .cond = &cond, .next = nullptr };
    this->enq_item(&item);

    // Wait for our turn.
    ++m_num_want_write;
    if (expensive) {
        ++m_num_expensive_want_write;
    }
    if (m_num_writers == 0 && m_num_want_write == 1) {
        // We are the first to want a write lock. No new readers can get the lock.
        // Set our thread id and context for proper instrumentation.
        // see: toku_context_note_frwlock_contention()
        m_current_writer_tid = get_local_tid();
        m_blocking_writer_context_id = toku_thread_get_context()->get_id();
    }
    toku_cond_wait(&cond, m_mutex);
    toku_cond_destroy(&cond);

    // Now it's our turn.
    paranoid_invariant(m_num_want_write > 0);
    paranoid_invariant_zero(m_num_readers);
    paranoid_invariant_zero(m_num_writers);
    paranoid_invariant_zero(m_num_signaled_readers);

    // Not waiting anymore; grab the lock.
    --m_num_want_write;
    if (expensive) {
        --m_num_expensive_want_write;
    }
    m_num_writers = 1;
    m_current_writer_expensive = expensive;
    m_current_writer_tid = get_local_tid();
    m_blocking_writer_context_id = toku_thread_get_context()->get_id();
}

bool frwlock::try_write_lock(bool expensive) {
    toku_mutex_assert_locked(m_mutex);
    if (m_num_readers > 0 || m_num_writers > 0 || m_num_signaled_readers > 0 || m_num_want_write > 0) {
        return false;
    }
    // No one holds the lock.  Grant the write lock.
    paranoid_invariant_zero(m_num_want_write);
    paranoid_invariant_zero(m_num_want_read);
    m_num_writers = 1;
    m_current_writer_expensive = expensive;
    m_current_writer_tid = get_local_tid();
    m_blocking_writer_context_id = toku_thread_get_context()->get_id();
    return true;
}

void frwlock::read_lock(void) {
    toku_mutex_assert_locked(m_mutex);
    if (m_num_writers > 0 || m_num_want_write > 0) {
        if (!m_wait_read_is_in_queue) {
            // Throw the read cond_t onto the queue.
            paranoid_invariant(m_num_signaled_readers == m_num_want_read);
            m_queue_item_read.next = nullptr;
            this->enq_item(&m_queue_item_read);
            m_wait_read_is_in_queue = true;
            paranoid_invariant(!m_read_wait_expensive);
            m_read_wait_expensive = (
                m_current_writer_expensive || 
                (m_num_expensive_want_write > 0)
                );
        }

        // Note this contention event in engine status.
        toku_context_note_frwlock_contention(
            toku_thread_get_context()->get_id(),
            m_blocking_writer_context_id
            );

        // Wait for our turn.
        ++m_num_want_read;
        toku_cond_wait(&m_wait_read, m_mutex);

        // Now it's our turn.
        paranoid_invariant_zero(m_num_writers);
        paranoid_invariant(m_num_want_read > 0);
        paranoid_invariant(m_num_signaled_readers > 0);

        // Not waiting anymore; grab the lock.
        --m_num_want_read;
        --m_num_signaled_readers;
    }
    ++m_num_readers;
}

bool frwlock::try_read_lock(void) {
    toku_mutex_assert_locked(m_mutex);
    if (m_num_writers > 0 || m_num_want_write > 0) {
        return false;
    }
    // No writer holds the lock.
    // No writers are waiting.
    // Grant the read lock.
    ++m_num_readers;
    return true;
}

void frwlock::maybe_signal_next_writer(void) {
    if (m_num_want_write > 0 && m_num_signaled_readers == 0 && m_num_readers == 0) {
        toku_cond_t *cond = this->deq_item();
        paranoid_invariant(cond != &m_wait_read);
        // Grant write lock to waiting writer.
        paranoid_invariant(m_num_want_write > 0);
        toku_cond_signal(cond);
    }
}

void frwlock::read_unlock(void) {
    toku_mutex_assert_locked(m_mutex);
    paranoid_invariant(m_num_writers == 0);
    paranoid_invariant(m_num_readers > 0);
    --m_num_readers;
    this->maybe_signal_next_writer();
}

bool frwlock::read_lock_is_expensive(void) {
    toku_mutex_assert_locked(m_mutex);
    if (m_wait_read_is_in_queue) {
        return m_read_wait_expensive;
    }
    else {
        return m_current_writer_expensive || (m_num_expensive_want_write > 0);
    }
}


void frwlock::maybe_signal_or_broadcast_next(void) {
    paranoid_invariant(m_num_signaled_readers == 0);

    if (this->queue_is_empty()) {
        paranoid_invariant(m_num_want_write == 0);
        paranoid_invariant(m_num_want_read == 0);
        return;
    }
    toku_cond_t *cond = this->deq_item();
    if (cond == &m_wait_read) {
        // Grant read locks to all waiting readers
        paranoid_invariant(m_wait_read_is_in_queue);
        paranoid_invariant(m_num_want_read > 0);
        m_num_signaled_readers = m_num_want_read;
        m_wait_read_is_in_queue = false;
        m_read_wait_expensive = false;
        toku_cond_broadcast(cond);
    }
    else {
        // Grant write lock to waiting writer.
        paranoid_invariant(m_num_want_write > 0);
        toku_cond_signal(cond);
    }
}

void frwlock::write_unlock(void) {
    toku_mutex_assert_locked(m_mutex);
    paranoid_invariant(m_num_writers == 1);
    m_num_writers = 0;
    m_current_writer_expensive = false;
    m_current_writer_tid = -1;
    m_blocking_writer_context_id = CTX_INVALID;
    this->maybe_signal_or_broadcast_next();
}
bool frwlock::write_lock_is_expensive(void) {
    toku_mutex_assert_locked(m_mutex);
    return (m_num_expensive_want_write > 0) || (m_current_writer_expensive);
}


uint32_t frwlock::users(void) const {
    toku_mutex_assert_locked(m_mutex);
    return m_num_readers + m_num_writers + m_num_want_read + m_num_want_write;
}
uint32_t frwlock::blocked_users(void) const {
    toku_mutex_assert_locked(m_mutex);
    return m_num_want_read + m_num_want_write;
}
uint32_t frwlock::writers(void) const {
    // this is sometimes called as "assert(lock->writers())" when we
    // assume we have the write lock.  if that's the assumption, we may
    // not own the mutex, so we don't assert_locked here
    return m_num_writers;
}
uint32_t frwlock::blocked_writers(void) const {
    toku_mutex_assert_locked(m_mutex);
    return m_num_want_write;
}
uint32_t frwlock::readers(void) const {
    toku_mutex_assert_locked(m_mutex);
    return m_num_readers;
}
uint32_t frwlock::blocked_readers(void) const {
    toku_mutex_assert_locked(m_mutex);
    return m_num_want_read;
}

} // namespace toku

/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: frwlock.h 45930 2012-07-19 19:18:35Z zardosht $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>

namespace toku {

void frwlock::init(toku_mutex_t *const mutex) {
    m_mutex = mutex;

    m_num_readers = 0;
    m_num_writers = 0;
    m_num_want_write = 0;
    m_num_want_read = 0;
    m_num_signaled_readers = 0;
    m_num_expensive_want_write = 0;
    
    toku_cond_init(&m_wait_read, nullptr);
    m_queue_item_read = { .cond = &m_wait_read, .next = nullptr };
    m_wait_read_is_in_queue = false;
    m_current_writer_expensive = false;
    m_read_wait_expensive = false;

    m_wait_head = nullptr;
    m_wait_tail = nullptr;
}

void frwlock::deinit(void) {
    toku_cond_destroy(&m_wait_read);
}

inline bool frwlock::queue_is_empty(void) const {
    return m_wait_head == nullptr;
}

inline void frwlock::enq_item(queue_item *const item) {
    invariant_null(item->next);
    if (m_wait_tail != nullptr) {
        m_wait_tail->next = item;
    } else {
        invariant_null(m_wait_head);
        m_wait_head = item;
    }
    m_wait_tail = item;
}

inline toku_cond_t *frwlock::deq_item(void) {
    invariant_notnull(m_wait_head);
    invariant_notnull(m_wait_tail);
    queue_item *item = m_wait_head;
    m_wait_head = m_wait_head->next;
    if (m_wait_tail == item) {
        m_wait_tail = nullptr;
    }
    return item->cond;
}

// Prerequisite: Holds m_mutex.
inline void frwlock::write_lock(bool expensive) {
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
    toku_cond_wait(&cond, m_mutex);
    toku_cond_destroy(&cond);

    // Now it's our turn.
    invariant(m_num_want_write > 0);
    invariant_zero(m_num_readers);
    invariant_zero(m_num_writers);
    invariant_zero(m_num_signaled_readers);

    // Not waiting anymore; grab the lock.
    --m_num_want_write;
    if (expensive) {
        --m_num_expensive_want_write;
    }
    m_num_writers = 1;
    m_current_writer_expensive = expensive;
}

inline bool frwlock::try_write_lock(bool expensive) {
    if (m_num_readers > 0 || m_num_writers > 0 || m_num_signaled_readers > 0 || m_num_want_write > 0) {
        return false;
    }
    // No one holds the lock.  Grant the write lock.
    invariant_zero(m_num_want_write);
    invariant_zero(m_num_want_read);
    m_num_writers = 1;
    m_current_writer_expensive = expensive;
    return true;
}

inline void frwlock::read_lock(void) {
    if (m_num_writers > 0 || m_num_want_write > 0) {
        if (!m_wait_read_is_in_queue) {
            // Throw the read cond_t onto the queue.
            invariant(m_num_signaled_readers == m_num_want_read);
            m_queue_item_read.next = nullptr;
            this->enq_item(&m_queue_item_read);
            m_wait_read_is_in_queue = true;
            invariant(!m_read_wait_expensive);
            m_read_wait_expensive = (
                m_current_writer_expensive || 
                (m_num_expensive_want_write > 0)
                );
        }

        // Wait for our turn.
        ++m_num_want_read;
        toku_cond_wait(&m_wait_read, m_mutex);

        // Now it's our turn.
        invariant_zero(m_num_writers);
        invariant(m_num_want_read > 0);
        invariant(m_num_signaled_readers > 0);

        // Not waiting anymore; grab the lock.
        --m_num_want_read;
        --m_num_signaled_readers;
    }
    ++m_num_readers;
}

inline bool frwlock::try_read_lock(void) {
    if (m_num_writers > 0 || m_num_want_write > 0) {
        return false;
    }
    // No writer holds the lock.
    // No writers are waiting.
    // Grant the read lock.
    ++m_num_readers;
    return true;
}

inline void frwlock::maybe_signal_next_writer(void) {
    if (m_num_want_write > 0 && m_num_signaled_readers == 0 && m_num_readers == 0) {
        toku_cond_t *cond = this->deq_item();
        invariant(cond != &m_wait_read);
        // Grant write lock to waiting writer.
        invariant(m_num_want_write > 0);
        toku_cond_signal(cond);
    }
}

inline void frwlock::read_unlock(void) {
    invariant(m_num_writers == 0);
    invariant(m_num_readers > 0);
    --m_num_readers;
    this->maybe_signal_next_writer();
}

inline bool frwlock::read_lock_is_expensive(void) {
    if (m_wait_read_is_in_queue) {
        return m_read_wait_expensive;
    }
    else {
        return m_current_writer_expensive || (m_num_expensive_want_write > 0);
    }
}


inline void frwlock::maybe_signal_or_broadcast_next(void) {
    invariant(m_num_signaled_readers == 0);

    if (this->queue_is_empty()) {
        invariant(m_num_want_write == 0);
        invariant(m_num_want_read == 0);
        return;
    }
    toku_cond_t *cond = this->deq_item();
    if (cond == &m_wait_read) {
        // Grant read locks to all waiting readers
        invariant(m_wait_read_is_in_queue);
        invariant(m_num_want_read > 0);
        m_num_signaled_readers = m_num_want_read;
        m_wait_read_is_in_queue = false;
        m_read_wait_expensive = false;
        toku_cond_broadcast(cond);
    }
    else {
        // Grant write lock to waiting writer.
        invariant(m_num_want_write > 0);
        toku_cond_signal(cond);
    }
}

inline void frwlock::write_unlock(void) {
    invariant(m_num_writers == 1);
    m_num_writers = 0;
    m_current_writer_expensive = false;
    this->maybe_signal_or_broadcast_next();
}
inline bool frwlock::write_lock_is_expensive(void) {
    return (m_num_expensive_want_write > 0) || (m_current_writer_expensive);
}


inline uint32_t frwlock::users(void) const {
    return m_num_readers + m_num_writers + m_num_want_read + m_num_want_write;
}
inline uint32_t frwlock::blocked_users(void) const {
    return m_num_want_read + m_num_want_write;
}
inline uint32_t frwlock::writers(void) const {
    return m_num_writers;
}
inline uint32_t frwlock::blocked_writers(void) const {
    return m_num_want_write;
}
inline uint32_t frwlock::readers(void) const {
    return m_num_readers;
}
inline uint32_t frwlock::blocked_readers(void) const {
    return m_num_want_read;
}

} // namespace toku

/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef UTIL_FRWLOCK_H
#define UTIL_FRWLOCK_H
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_pthread.h>
#include <stdbool.h>
#include <stdint.h>

//TODO: update comment, this is from rwlock.h

namespace toku {

class frwlock {
public:

    void init(toku_mutex_t *const mutex);
    void deinit(void);

    inline void write_lock(bool expensive);
    inline bool try_write_lock(bool expensive);
    inline void write_unlock(void);
    // returns true if acquiring a write lock will be expensive
    inline bool write_lock_is_expensive(void);

    inline void read_lock(void);
    inline bool try_read_lock(void);
    inline void read_unlock(void);
    // returns true if acquiring a read lock will be expensive
    inline bool read_lock_is_expensive(void);

    inline uint32_t users(void) const;
    inline uint32_t blocked_users(void) const;
    inline uint32_t writers(void) const;
    inline uint32_t blocked_writers(void) const;
    inline uint32_t readers(void) const;
    inline uint32_t blocked_readers(void) const;

private:
    struct queue_item {
        toku_cond_t *cond;
        struct queue_item *next;
    };

    inline bool queue_is_empty(void) const;
    inline void enq_item(queue_item *const item);
    inline toku_cond_t *deq_item(void);
    inline void maybe_signal_or_broadcast_next(void);
    inline void maybe_signal_next_writer(void);

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
    
    toku_cond_t m_wait_read;
    queue_item m_queue_item_read;
    bool m_wait_read_is_in_queue;

    queue_item *m_wait_head;
    queue_item *m_wait_tail;
};

ENSURE_POD(frwlock);

} // namespace toku

// include the implementation here
#include "frwlock.cc"

#endif // UTIL_FRWLOCK_H

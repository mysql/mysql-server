/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

namespace toku {

    template<typename T>
    void circular_buffer<T>::init(T * const array, size_t cap) {
        paranoid_invariant_notnull(array);
        m_array = array;
        m_cap = cap;
        m_begin = 0;
        m_limit = 0;
        toku_mutex_init(&m_lock, nullptr);
        toku_cond_init(&m_push_cond, nullptr);
        toku_cond_init(&m_pop_cond, nullptr);
        m_push_waiters = 0;
        m_pop_waiters = 0;
    }

    template<typename T>
    void circular_buffer<T>::deinit(void) {
        lock();
        paranoid_invariant(is_empty());
        paranoid_invariant_zero(m_push_waiters);
        paranoid_invariant_zero(m_pop_waiters);
        unlock();
        toku_cond_destroy(&m_pop_cond);
        toku_cond_destroy(&m_push_cond);
        toku_mutex_destroy(&m_lock);
    }

    template<typename T>
    void circular_buffer<T>::lock(void) {
        toku_mutex_lock(&m_lock);
    }

    template<typename T>
    void circular_buffer<T>::unlock(void) {
        toku_mutex_unlock(&m_lock);
    }

    template<typename T>
    size_t circular_buffer<T>::size(void) const {
        toku_mutex_assert_locked(&m_lock);
        return m_limit - m_begin;
    }

    template<typename T>
    bool circular_buffer<T>::is_empty(void) const {
        return size() == 0;
    }

    template<typename T>
    bool circular_buffer<T>::is_full(void) const {
        return size() == m_cap;
    }

    template<typename N>
    __attribute__((const))
    static inline N mod(N a, N b) {
        return ((a % b) + a) % b;
    }

    template<typename T>
    T *circular_buffer<T>::get_addr(size_t idx) {
        toku_mutex_assert_locked(&m_lock);
        paranoid_invariant(idx >= m_begin);
        paranoid_invariant(idx < m_limit);
        return &m_array[mod(idx, m_cap)];
    }

    template<typename T>
    void circular_buffer<T>::push_and_maybe_signal_unlocked(const T &elt) {
        toku_mutex_assert_locked(&m_lock);
        paranoid_invariant(!is_full());
        size_t location = m_limit++;
        *get_addr(location) = elt;
        if (m_pop_waiters > 0) {
            toku_cond_signal(&m_pop_cond);
        }
    }

    template<typename T>
    void circular_buffer<T>::push(const T &elt) {
        lock();
        while (is_full()) {
            ++m_push_waiters;
            toku_cond_wait(&m_push_cond, &m_lock);
            --m_push_waiters;
        }
        push_and_maybe_signal_unlocked(elt);
        unlock();
    }

    template<typename T>
    bool circular_buffer<T>::trypush(const T &elt) {
        bool pushed = false;
        lock();
        if (!is_full() && m_push_waiters == 0) {
            push_and_maybe_signal_unlocked(elt);
            pushed = true;
        }
        unlock();
        return pushed;
    }

    template<typename T>
    bool circular_buffer<T>::timedpush(const T &elt, toku_timespec_t *abstime) {
        bool pushed = false;
        paranoid_invariant_notnull(abstime);
        lock();
        if (is_full()) {
            ++m_push_waiters;
            int r = toku_cond_timedwait(&m_push_cond, &m_lock, abstime);
            if (r != 0) {
                invariant(r == ETIMEDOUT);
            }
            --m_push_waiters;
        }
        if (!is_full()) {
            push_and_maybe_signal_unlocked(elt);
            pushed = true;
        }
        unlock();
        return pushed;
    }

    template<typename T>
    T circular_buffer<T>::pop_and_maybe_signal_unlocked(void) {
        toku_mutex_assert_locked(&m_lock);
        paranoid_invariant(!is_empty());
        T ret = *get_addr(m_begin);
        ++m_begin;
        if (m_push_waiters > 0) {
            toku_cond_signal(&m_push_cond);
        }
        return ret;
    }

    template<typename T>
    T circular_buffer<T>::pop(void) {
        lock();
        while (is_empty()) {
            ++m_pop_waiters;
            toku_cond_wait(&m_pop_cond, &m_lock);
            --m_pop_waiters;
        }
        T ret = pop_and_maybe_signal_unlocked();
        unlock();
        return ret;
    }

    template<typename T>
    bool circular_buffer<T>::trypop(T * const eltp) {
        bool popped = false;
        paranoid_invariant_notnull(eltp);
        lock();
        if (!is_empty() && m_pop_waiters == 0) {
            *eltp = pop_and_maybe_signal_unlocked();
            popped = true;
        }
        unlock();
        return popped;
    }

    template<typename T>
    bool circular_buffer<T>::timedpop(T * const eltp, toku_timespec_t *abstime) {
        bool popped = false;
        paranoid_invariant_notnull(eltp);
        paranoid_invariant_notnull(abstime);
        lock();
        if (is_empty()) {
            ++m_pop_waiters;
            int r = toku_cond_timedwait(&m_pop_cond, &m_lock, abstime);
            if (r != 0) {
                invariant(r == ETIMEDOUT);
            }
            --m_pop_waiters;
        }
        if (!is_empty()) {
            *eltp = pop_and_maybe_signal_unlocked();
            popped = true;
        }
        unlock();
        return popped;
    }

}

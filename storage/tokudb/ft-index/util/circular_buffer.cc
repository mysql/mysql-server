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

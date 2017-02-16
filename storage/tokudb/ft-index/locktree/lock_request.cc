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

#include "portability/toku_race_tools.h"

#include "ft/txn/txn.h"
#include "locktree/locktree.h"
#include "locktree/lock_request.h"
#include "util/dbt.h"

namespace toku {

// initialize a lock request's internals
void lock_request::create(void) {
    m_txnid = TXNID_NONE;
    m_conflicting_txnid = TXNID_NONE;
    m_start_time = 0;
    m_left_key = nullptr;
    m_right_key = nullptr;
    toku_init_dbt(&m_left_key_copy);
    toku_init_dbt(&m_right_key_copy);

    m_type = type::UNKNOWN;
    m_lt = nullptr;

    m_complete_r = 0;
    m_state = state::UNINITIALIZED;
    m_info = nullptr;

    toku_cond_init(&m_wait_cond, nullptr);

    m_start_test_callback = nullptr;
    m_retry_test_callback = nullptr;
}

// destroy a lock request.
void lock_request::destroy(void) {
    invariant(m_state != state::PENDING);
    invariant(m_state != state::DESTROYED);
    m_state = state::DESTROYED;
    toku_destroy_dbt(&m_left_key_copy);
    toku_destroy_dbt(&m_right_key_copy);
    toku_cond_destroy(&m_wait_cond);
}

// set the lock request parameters. this API allows a lock request to be reused.
void lock_request::set(locktree *lt, TXNID txnid, const DBT *left_key, const DBT *right_key, lock_request::type lock_type, bool big_txn) {
    invariant(m_state != state::PENDING);
    m_lt = lt;
    m_txnid = txnid;
    m_left_key = left_key;
    m_right_key = right_key;
    toku_destroy_dbt(&m_left_key_copy);
    toku_destroy_dbt(&m_right_key_copy);
    m_type = lock_type;
    m_state = state::INITIALIZED;
    m_info = lt ? lt->get_lock_request_info() : nullptr;
    m_big_txn = big_txn;
}

// get rid of any stored left and right key copies and
// replace them with copies of the given left and right key
void lock_request::copy_keys() {
    if (!toku_dbt_is_infinite(m_left_key)) {
        toku_clone_dbt(&m_left_key_copy, *m_left_key);
        m_left_key = &m_left_key_copy;
    }
    if (!toku_dbt_is_infinite(m_right_key)) {
        toku_clone_dbt(&m_right_key_copy, *m_right_key);
        m_right_key = &m_right_key_copy;
    }
}

// what are the conflicts for this pending lock request?
void lock_request::get_conflicts(txnid_set *conflicts) {
    invariant(m_state == state::PENDING);
    const bool is_write_request = m_type == type::WRITE;
    m_lt->get_conflicts(is_write_request, m_txnid, m_left_key, m_right_key, conflicts);
}

// build a wait-for-graph for this lock request and the given conflict set
// for each transaction B that blocks A's lock request
//     if B is blocked then
//         add (A,T) to the WFG and if B is new, fill in the WFG from B
void lock_request::build_wait_graph(wfg *wait_graph, const txnid_set &conflicts) {
    size_t num_conflicts = conflicts.size();
    for (size_t i = 0; i < num_conflicts; i++) {
        TXNID conflicting_txnid = conflicts.get(i);
        lock_request *conflicting_request = find_lock_request(conflicting_txnid);
        invariant(conflicting_txnid != m_txnid);
        invariant(conflicting_request != this);
        if (conflicting_request) {
            bool already_exists = wait_graph->node_exists(conflicting_txnid);
            wait_graph->add_edge(m_txnid, conflicting_txnid);
            if (!already_exists) {
                // recursively build the wait for graph rooted at the conflicting
                // request, given its set of lock conflicts.
                txnid_set other_conflicts;
                other_conflicts.create();
                conflicting_request->get_conflicts(&other_conflicts);
                conflicting_request->build_wait_graph(wait_graph, other_conflicts);
                other_conflicts.destroy();
            }
        }
    }
}

// returns: true if the current set of lock requests contains
//          a deadlock, false otherwise.
bool lock_request::deadlock_exists(const txnid_set &conflicts) {
    wfg wait_graph;
    wait_graph.create();

    build_wait_graph(&wait_graph, conflicts);
    bool deadlock = wait_graph.cycle_exists_from_txnid(m_txnid);

    wait_graph.destroy();
    return deadlock;
}

// try to acquire a lock described by this lock request. 
int lock_request::start(void) {
    int r;

    txnid_set conflicts;
    conflicts.create();
    if (m_type == type::WRITE) {
        r = m_lt->acquire_write_lock(m_txnid, m_left_key, m_right_key, &conflicts, m_big_txn);
    } else {
        invariant(m_type == type::READ);
        r = m_lt->acquire_read_lock(m_txnid, m_left_key, m_right_key, &conflicts, m_big_txn);
    }

    // if the lock is not granted, save it to the set of lock requests
    // and check for a deadlock. if there is one, complete it as failed
    if (r == DB_LOCK_NOTGRANTED) {
        copy_keys();
        m_state = state::PENDING;
        m_start_time = toku_current_time_microsec() / 1000;
        m_conflicting_txnid = conflicts.get(0);
        toku_mutex_lock(&m_info->mutex);
        insert_into_lock_requests();
        if (deadlock_exists(conflicts)) {
            remove_from_lock_requests();
            r = DB_LOCK_DEADLOCK;
        }
        toku_mutex_unlock(&m_info->mutex);
        if (m_start_test_callback) m_start_test_callback(); // test callback
    }

    if (r != DB_LOCK_NOTGRANTED) {
        complete(r);
    }

    conflicts.destroy();
    return r;
}

// sleep on the lock request until it becomes resolved or the wait time has elapsed.
int lock_request::wait(uint64_t wait_time_ms) {
    return wait(wait_time_ms, 0, nullptr);
}

int lock_request::wait(uint64_t wait_time_ms, uint64_t killed_time_ms, int (*killed_callback)(void)) {
    uint64_t t_now = toku_current_time_microsec();
    uint64_t t_start = t_now;
    uint64_t t_end = t_start + wait_time_ms * 1000;

    toku_mutex_lock(&m_info->mutex);

    while (m_state == state::PENDING) {

        // compute next wait time
        uint64_t t_wait;
        if (killed_time_ms == 0) {
            t_wait = t_end;
        } else {
            t_wait = t_now + killed_time_ms * 1000;
            if (t_wait > t_end)
                t_wait = t_end;
        }
        struct timespec ts = {};
        ts.tv_sec = t_wait / 1000000;
        ts.tv_nsec = (t_wait % 1000000) * 1000;
        int r = toku_cond_timedwait(&m_wait_cond, &m_info->mutex, &ts);
        invariant(r == 0 || r == ETIMEDOUT);

        t_now = toku_current_time_microsec();
        if (m_state == state::PENDING && (t_now >= t_end || (killed_callback && killed_callback()))) {
            m_info->counters.timeout_count += 1;
            
            // if we're still pending and we timed out, then remove our
            // request from the set of lock requests and fail.
            remove_from_lock_requests();
            
            // complete sets m_state to COMPLETE, breaking us out of the loop
            complete(DB_LOCK_NOTGRANTED);
        }
    }

    uint64_t t_real_end = toku_current_time_microsec();
    uint64_t duration = t_real_end - t_start;
    m_info->counters.wait_count += 1;
    m_info->counters.wait_time += duration;
    if (duration >= 1000000) {
        m_info->counters.long_wait_count += 1;
        m_info->counters.long_wait_time += duration;
    }
    toku_mutex_unlock(&m_info->mutex);

    invariant(m_state == state::COMPLETE);
    return m_complete_r;
}

// complete this lock request with the given return value
void lock_request::complete(int complete_r) {
    m_complete_r = complete_r;
    m_state = state::COMPLETE;
}

const DBT *lock_request::get_left_key(void) const {
    return m_left_key;
}

const DBT *lock_request::get_right_key(void) const {
    return m_right_key;
}

TXNID lock_request::get_txnid(void) const {
    return m_txnid;
}

uint64_t lock_request::get_start_time(void) const {
    return m_start_time;
}

TXNID lock_request::get_conflicting_txnid(void) const {
    return m_conflicting_txnid;
}

int lock_request::retry(void) {
    int r;

    invariant(m_state == state::PENDING);
    if (m_type == type::WRITE) {
        r = m_lt->acquire_write_lock(m_txnid, m_left_key, m_right_key, nullptr, m_big_txn);
    } else {
        r = m_lt->acquire_read_lock(m_txnid, m_left_key, m_right_key, nullptr, m_big_txn);
    }

    // if the acquisition succeeded then remove ourselves from the
    // set of lock requests, complete, and signal the waiting thread.
    if (r == 0) {
        remove_from_lock_requests();
        complete(r);
        if (m_retry_test_callback) m_retry_test_callback(); // test callback
        toku_cond_broadcast(&m_wait_cond);
    }

    return r;
}

void lock_request::retry_all_lock_requests(locktree *lt) {
    lt_lock_request_info *info = lt->get_lock_request_info();

    // if a thread reads this bit to be true, then it should go ahead and
    // take the locktree mutex and retry lock requests. we use this bit
    // to prevent every single thread from waiting on the locktree mutex
    // in order to retry requests, especially when no requests actually exist.
    //
    // it is important to note that this bit only provides an optimization.
    // it is not problematic for it to be true when it should be false,
    // but it can be problematic for it to be false when it should be true.
    // therefore, the lock request code must ensures that when lock requests
    // are added to this locktree, the bit is set.
    // see lock_request::insert_into_lock_requests()
    if (!info->should_retry_lock_requests) {
        return;
    }

    toku_mutex_lock(&info->mutex);

    // let other threads know that they need not retry lock requests at this time.
    //
    // the motivation here is that if a bunch of threads have already released
    // their locks in the rangetree, then its probably okay for only one thread
    // to iterate over the list of requests and retry them. otherwise, at high
    // thread counts and a large number of pending lock requests, you could
    // end up wasting a lot of cycles.
    info->should_retry_lock_requests = false;

    size_t i = 0;
    while (i < info->pending_lock_requests.size()) {
        lock_request *request;
        int r = info->pending_lock_requests.fetch(i, &request);
        invariant_zero(r);

        // retry the lock request. if it didn't succeed,
        // move on to the next lock request. otherwise
        // the request is gone from the list so we may
        // read the i'th entry for the next one.
        r = request->retry();
        if (r != 0) {
            i++;
        }
    }

    // future threads should only retry lock requests if some still exist
    info->should_retry_lock_requests = info->pending_lock_requests.size() > 0;

    toku_mutex_unlock(&info->mutex);
}

// find another lock request by txnid. must hold the mutex.
lock_request *lock_request::find_lock_request(const TXNID &txnid) {
    lock_request *request;
    int r = m_info->pending_lock_requests.find_zero<TXNID, find_by_txnid>(txnid, &request, nullptr);
    if (r != 0) {
        request = nullptr;
    }
    return request;
}

// insert this lock request into the locktree's set. must hold the mutex.
void lock_request::insert_into_lock_requests(void) {
    uint32_t idx;
    lock_request *request;
    int r = m_info->pending_lock_requests.find_zero<TXNID, find_by_txnid>(m_txnid, &request, &idx);
    invariant(r == DB_NOTFOUND);
    r = m_info->pending_lock_requests.insert_at(this, idx);
    invariant_zero(r);

    // ensure that this bit is true, now that at least one lock request is in the set
    m_info->should_retry_lock_requests = true;
}

// remove this lock request from the locktree's set. must hold the mutex.
void lock_request::remove_from_lock_requests(void) {
    uint32_t idx;
    lock_request *request;
    int r = m_info->pending_lock_requests.find_zero<TXNID, find_by_txnid>(m_txnid, &request, &idx);
    invariant_zero(r);
    invariant(request == this);
    r = m_info->pending_lock_requests.delete_at(idx);
    invariant_zero(r);
}

int lock_request::find_by_txnid(lock_request * const &request, const TXNID &txnid) {
    TXNID request_txnid = request->m_txnid;
    if (request_txnid < txnid) {
        return -1;
    } else if (request_txnid == txnid) {
        return 0;
    } else {
        return 1;
    }
}

void lock_request::set_start_test_callback(void (*f)(void)) {
    m_start_test_callback = f;
}

void lock_request::set_retry_test_callback(void (*f)(void)) {
    m_retry_test_callback = f;
}

} /* namespace toku */

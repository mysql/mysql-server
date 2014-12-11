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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db.h>

#include "portability/toku_pthread.h"

#include "locktree/locktree.h"
#include "locktree/txnid_set.h"
#include "locktree/wfg.h"
#include "ft/comparator.h"

namespace toku {

// A lock request contains the db, the key range, the lock type, and
// the transaction id that describes a potential row range lock.
//
// the typical use case is:
// - initialize a lock request
// - start to try to acquire the lock
// - do something else
// - wait for the lock request to be resolved on a timed condition
// - destroy the lock request
// a lock request is resolved when its state is no longer pending, or
// when it becomes granted, or timedout, or deadlocked. when resolved, the
// state of the lock request is changed and any waiting threads are awakened.

class lock_request {
public:
    enum type {
        UNKNOWN,
        READ,
        WRITE
    };

    // effect: Initializes a lock request.
    void create(void);

    // effect: Destroys a lock request.
    void destroy(void);

    // effect: Resets the lock request parameters, allowing it to be reused.
    // requires: Lock request was already created at some point
    void set(locktree *lt, TXNID txnid, const DBT *left_key, const DBT *right_key, type lock_type, bool big_txn);

    // effect: Tries to acquire a lock described by this lock request.
    // returns: The return code of locktree::acquire_[write,read]_lock()
    //          or DB_LOCK_DEADLOCK if this request would end up deadlocked.
    int start(void);

    // effect: Sleeps until either the request is granted or the wait time expires.
    // returns: The return code of locktree::acquire_[write,read]_lock()
    //          or simply DB_LOCK_NOTGRANTED if the wait time expired.
    int wait(uint64_t wait_time_ms);
    int wait(uint64_t wait_time_ms, uint64_t killed_time_ms, int (*killed_callback)(void));

    // return: left end-point of the lock range
    const DBT *get_left_key(void) const;

    // return: right end-point of the lock range
    const DBT *get_right_key(void) const;

    // return: the txnid waiting for a lock
    TXNID get_txnid(void) const;

    // return: when this lock request started, as milliseconds from epoch
    uint64_t get_start_time(void) const;

    // return: which txnid is blocking this request (there may be more, though)
    TXNID get_conflicting_txnid(void) const;

    // effect: Retries all of the lock requests for the given locktree.
    //         Any lock requests successfully restarted is completed and woken up.
    //         The rest remain pending.
    static void retry_all_lock_requests(locktree *lt);

    void set_start_test_callback(void (*f)(void));
    void set_retry_test_callback(void (*f)(void));
private:

    enum state {
        UNINITIALIZED,
        INITIALIZED,
        PENDING,
        COMPLETE,
        DESTROYED,
    };

    // The keys for a lock request are stored "unowned" in m_left_key
    // and m_right_key. When the request is about to go to sleep, it
    // copies these keys and stores them in m_left_key_copy etc and
    // sets the temporary pointers to null.
    TXNID m_txnid;
    TXNID m_conflicting_txnid;
    uint64_t m_start_time;
    const DBT *m_left_key;
    const DBT *m_right_key;
    DBT m_left_key_copy;
    DBT m_right_key_copy;

    // The lock request type and associated locktree
    type m_type;
    locktree *m_lt;

    // If the lock request is in the completed state, then its
    // final return value is stored in m_complete_r
    int m_complete_r;
    state m_state;

    toku_cond_t m_wait_cond;

    bool m_big_txn;

    // the lock request info state stored in the
    // locktree that this lock request is for.
    struct lt_lock_request_info *m_info;

    // effect: tries again to acquire the lock described by this lock request
    // returns: 0 if retrying the request succeeded and is now complete
    int retry(void);

    void complete(int complete_r);

    // effect: Finds another lock request by txnid.
    // requires: The lock request info mutex is held
    lock_request *find_lock_request(const TXNID &txnid);

    // effect: Insert this lock request into the locktree's set.
    // requires: the locktree's mutex is held
    void insert_into_lock_requests(void);

    // effect: Removes this lock request from the locktree's set.
    // requires: The lock request info mutex is held
    void remove_from_lock_requests(void);

    // effect: Asks this request's locktree which txnids are preventing
    //         us from getting the lock described by this request.
    // returns: conflicts is populated with the txnid's that this request
    //          is blocked on
    void get_conflicts(txnid_set *conflicts);

    // effect: Builds a wait-for-graph for this lock request and the given conflict set
    void build_wait_graph(wfg *wait_graph, const txnid_set &conflicts);

    // returns: True if this lock request is in deadlock with the given conflicts set
    bool deadlock_exists(const txnid_set &conflicts);

    void copy_keys(void);

    static int find_by_txnid(lock_request * const &request, const TXNID &txnid);

    void (*m_start_test_callback)(void);
    void (*m_retry_test_callback)(void);

    friend class lock_request_unit_test;
};
ENSURE_POD(lock_request);

} /* namespace toku */

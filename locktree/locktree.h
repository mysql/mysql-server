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

#ifndef TOKU_LOCKTREE_H
#define TOKU_LOCKTREE_H

#include <db.h>
#include <toku_time.h>
#include <toku_pthread.h>

#include <ft/fttypes.h>
#include <ft/comparator.h>

#include <util/omt.h>

#include "txnid_set.h"
#include "wfg.h"
#include "range_buffer.h"

#define TOKU_LOCKTREE_ESCALATOR_LAMBDA 0
#if TOKU_LOCKTREE_ESCALATOR_LAMBDA
#include <functional>
#endif

enum {
    LTM_SIZE_CURRENT = 0,
    LTM_SIZE_LIMIT,
    LTM_ESCALATION_COUNT,
    LTM_ESCALATION_TIME,
    LTM_ESCALATION_LATEST_RESULT,
    LTM_NUM_LOCKTREES,
    LTM_LOCK_REQUESTS_PENDING,
    LTM_STO_NUM_ELIGIBLE,
    LTM_STO_END_EARLY_COUNT,
    LTM_STO_END_EARLY_TIME,
    LTM_WAIT_COUNT,
    LTM_WAIT_TIME,
    LTM_LONG_WAIT_COUNT,
    LTM_LONG_WAIT_TIME,
    LTM_TIMEOUT_COUNT,
    LTM_WAIT_ESCALATION_COUNT,
    LTM_WAIT_ESCALATION_TIME,
    LTM_LONG_WAIT_ESCALATION_COUNT,
    LTM_LONG_WAIT_ESCALATION_TIME,
    LTM_STATUS_NUM_ROWS // must be last
};

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[LTM_STATUS_NUM_ROWS];
} LTM_STATUS_S, *LTM_STATUS;

namespace toku {

class lock_request;
class concurrent_tree;

// A locktree represents the set of row locks owned by all transactions
// over an open dictionary. Read and write ranges are represented as
// a left and right key which are compared with the given descriptor
// and comparison fn.
//
// Locktrees are not created and destroyed by the user. Instead, they are
// referenced and released using the locktree manager.
//
// A sample workflow looks like this:
// - Create a manager.
// - Get a locktree by dictionaroy id from the manager.
// - Perform read/write lock acquision on the locktree, add references to
//   the locktree using the manager, release locks, release references, etc.
// - ...
// - Release the final reference to the locktree. It will be destroyed.
// - Destroy the manager.

class locktree {
public:

    // effect: Attempts to grant a read lock for the range of keys between [left_key, right_key].
    // returns: If the lock cannot be granted, return DB_LOCK_NOTGRANTED, and populate the
    //          given conflicts set with the txnids that hold conflicting locks in the range.
    //          If the locktree cannot create more locks, return TOKUDB_OUT_OF_LOCKS.
    // note: Read locks cannot be shared between txnids, as one would expect.
    //       This is for simplicity since read locks are rare in MySQL.
    int acquire_read_lock(TXNID txnid, const DBT *left_key, const DBT *right_key, txnid_set *conflicts, bool big_txn);

    // effect: Attempts to grant a write lock for the range of keys between [left_key, right_key].
    // returns: If the lock cannot be granted, return DB_LOCK_NOTGRANTED, and populate the
    //          given conflicts set with the txnids that hold conflicting locks in the range.
    //          If the locktree cannot create more locks, return TOKUDB_OUT_OF_LOCKS.
    int acquire_write_lock(TXNID txnid, const DBT *left_key, const DBT *right_key, txnid_set *conflicts, bool big_txn);

    // effect: populate the conflicts set with the txnids that would preventing
    //         the given txnid from getting a lock on [left_key, right_key]
    void get_conflicts(bool is_write_request, TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

    // effect: Release all of the lock ranges represented by the range buffer for a txnid.
    void release_locks(TXNID txnid, const range_buffer *ranges);

    // returns: The userdata associated with this locktree, or null if it has not been set.
    void *get_userdata(void);

    void set_userdata(void *userdata);

    void set_descriptor(DESCRIPTOR desc);

    int compare(const locktree *lt);

    DICTIONARY_ID get_dict_id() const;

    struct lt_counters {
        uint64_t wait_count, wait_time;
        uint64_t long_wait_count, long_wait_time;
        uint64_t timeout_count;
    };

    // The locktree stores some data for lock requests. It doesn't have to know
    // how they work or even what a lock request object looks like.
    struct lt_lock_request_info {
        omt<lock_request *> pending_lock_requests;
        toku_mutex_t mutex;
        bool should_retry_lock_requests;
        lt_counters counters;
    };

    // Private info struct for storing pending lock request state.
    // Only to be used by lock requests. We store it here as
    // something less opaque than usual to strike a tradeoff between
    // abstraction and code complexity. It is still fairly abstract
    // since the lock_request object is opaque 
    struct lt_lock_request_info *get_lock_request_info(void);

    class manager;

    // the escalator coordinates escalation on a set of locktrees for a bunch of threads
    class escalator {
    public:
        void create(void);
        void destroy(void);
#if TOKU_LOCKTREE_ESCALATOR_LAMBDA
        void run(manager *mgr, std::function<void (void)> escalate_locktrees_fun);
#else
        void run(manager *mgr, void (*escalate_locktrees_fun)(void *extra), void *extra);
#endif
    private:
        toku_mutex_t m_escalator_mutex;
        toku_cond_t m_escalator_done;
        bool m_escalator_running;
    };
    ENSURE_POD(escalator);

    // The locktree manager manages a set of locktrees,
    // one for each open dictionary. Locktrees are accessed through
    // the manager, and when they are no longer needed, they can
    // be released by the user.

    class manager {
    public:
        typedef int  (*lt_create_cb)(locktree *lt, void *extra);
        typedef void (*lt_destroy_cb)(locktree *lt);
        typedef void (*lt_escalate_cb)(TXNID txnid, const locktree *lt, const range_buffer &buffer, void *extra);

        // note: create_cb is called just after a locktree is first created.
        //       destroy_cb is called just before a locktree is destroyed.
        void create(lt_create_cb create_cb, lt_destroy_cb destroy_cb, lt_escalate_cb, void *extra);

        void destroy(void);

        size_t get_max_lock_memory(void);

        int set_max_lock_memory(size_t max_lock_memory);

        // effect: Get a locktree from the manager. If a locktree exists with the given
        //         dict_id, it is referenced and then returned. If one did not exist, it
        //         is created. It will use the given descriptor and comparison function
        //         for comparing keys, and the on_create callback passed to manager::create()
        //         will be called with the given extra parameter.
        locktree *get_lt(DICTIONARY_ID dict_id, DESCRIPTOR desc, ft_compare_func cmp,
                void *on_create_extra);

        void reference_lt(locktree *lt);

        // effect: Releases one reference on a locktree. If the reference count transitions
        //         to zero, the on_destroy callback is called before it gets destroyed.
        void release_lt(locktree *lt);

        // The memory tracker is employed by the manager to take care of
        // maintaining the current number of locks and lock memory and run
        // escalation if necessary.
        //
        // To do this, the manager hands out a memory tracker reference to each
        // locktree it creates, so that the locktrees can notify the memory
        // tracker when locks are acquired and released.

        class memory_tracker {
        public:
            void set_manager(manager *mgr);
            manager *get_manager(void);

            // effect: Determines if too many locks or too much memory is being used,
            //         Runs escalation on the manager if so.
            // returns: 0 if there enough resources to create a new lock, or TOKUDB_OUT_OF_LOCKS 
            //          if there are not enough resources and lock escalation failed to free up
            //          enough resources for a new lock.
            int check_current_lock_constraints(void);

            bool over_big_threshold(void);

            void note_mem_used(uint64_t mem_used);

            void note_mem_released(uint64_t mem_freed);

        private:
            manager *m_mgr;

            // returns: true if the manager of this memory tracker currently
            //          has more locks or lock memory than it is allowed.
            // note: this is a lock-less read, and it is ok for the caller to
            //       get false when they should have gotten true as long as
            //       a subsequent call gives the correct answer.
            //
            //       in general, if the tracker says the manager is not out of
            //       locks, you are clear to add O(1) locks to the system.
            bool out_of_locks(void) const;
        };
        ENSURE_POD(memory_tracker);

        // effect: calls the private function run_escalation(), only ok to
        //         do for tests.
        // rationale: to get better stress test coverage, we want a way to
        //            deterministicly trigger lock escalation.
        void run_escalation_for_test(void);
        void run_escalation(void);

        void get_status(LTM_STATUS status);

        // effect: calls the iterate function on each pending lock request
        // note: holds the manager's mutex
        typedef int (*lock_request_iterate_callback)(DICTIONARY_ID dict_id,
                                                     TXNID txnid,
                                                     const DBT *left_key,
                                                     const DBT *right_key,
                                                     TXNID blocking_txnid,
                                                     uint64_t start_time,
                                                     void *extra);
        int iterate_pending_lock_requests(lock_request_iterate_callback cb, void *extra);

        int check_current_lock_constraints(bool big_txn);

        // Escalate locktrees touched by a txn
        void escalate_lock_trees_for_txn(TXNID, locktree *lt);

        // Escalate all locktrees
        void escalate_all_locktrees(void);

        // Escalate a set of locktrees
        void escalate_locktrees(locktree **locktrees, int num_locktrees);

        // Add time t to the escalator's wait time statistics
        void add_escalator_wait_time(uint64_t t);

    private:
        static const uint64_t DEFAULT_MAX_LOCK_MEMORY = 64L * 1024 * 1024;

        // tracks the current number of locks and lock memory
        uint64_t m_max_lock_memory;
        uint64_t m_current_lock_memory;
        memory_tracker m_mem_tracker;

        struct lt_counters m_lt_counters;

        // the create and destroy callbacks for the locktrees
        lt_create_cb m_lt_create_callback;
        lt_destroy_cb m_lt_destroy_callback;
        lt_escalate_cb m_lt_escalate_callback;
        void *m_lt_escalate_callback_extra;

        LTM_STATUS_S status;

        omt<locktree *> m_locktree_map;

        // the manager's mutex protects the locktree map
        toku_mutex_t m_mutex;

        void mutex_lock(void);

        void mutex_unlock(void);

        void status_init(void);

        // effect: Gets a locktree from the map.
        // requires: Manager's mutex is held
        locktree *locktree_map_find(const DICTIONARY_ID &dict_id);

        // effect: Puts a locktree into the map.
        // requires: Manager's mutex is held
        void locktree_map_put(locktree *lt);

        // effect: Removes a locktree from the map.
        // requires: Manager's mutex is held
        void locktree_map_remove(locktree *lt);

        static int find_by_dict_id(locktree *const &lt, const DICTIONARY_ID &dict_id);

        void escalator_init(void);

        void escalator_destroy(void);

        // statistics about lock escalation.
        toku_mutex_t m_escalation_mutex;
        uint64_t m_escalation_count;
        tokutime_t m_escalation_time;
        uint64_t m_escalation_latest_result;
        uint64_t m_wait_escalation_count;
        uint64_t m_wait_escalation_time;
        uint64_t m_long_wait_escalation_count;
        uint64_t m_long_wait_escalation_time;

        escalator m_escalator;

        friend class manager_unit_test;
    };
    ENSURE_POD(manager);

    manager::memory_tracker *get_mem_tracker(void) const;

private:
    manager *m_mgr;
    manager::memory_tracker *m_mem_tracker;

    DICTIONARY_ID m_dict_id;

    // use a comparator object that encapsulates an ft compare
    // function and a descriptor in a fake db. this way we can
    // pass it around for easy key comparisons.
    //
    // since this comparator will store a pointer to a descriptor,
    // the user of the locktree needs to make sure that the descriptor
    // is valid for as long as the locktree. this is currently
    // implemented by opening an ft_handle for this locktree and
    // storing it as userdata below.
    comparator *m_cmp;

    uint32_t m_reference_count;

    concurrent_tree *m_rangetree;

    void *m_userdata;

    struct lt_lock_request_info m_lock_request_info;

    // The following fields and members prefixed with "sto_" are for
    // the single txnid optimization, intended to speed up the case
    // when only one transaction is using the locktree. If we know
    // the locktree has only one transaction, then acquiring locks
    // takes O(1) work and releasing all locks takes O(1) work.
    //
    // How do we know that the locktree only has a single txnid?
    // What do we do if it does?
    //
    // When a txn with txnid T requests a lock:
    // - If the tree is empty, the optimization is possible. Set the single
    // txnid to T, and insert the lock range into the buffer.
    // - If the tree is not empty, check if the single txnid is T. If so,
    // append the lock range to the buffer. Otherwise, migrate all of
    // the locks in the buffer into the rangetree on behalf of txnid T,
    // and invalid the single txnid.
    //
    // When a txn with txnid T releases its locks:
    // - If the single txnid is valid, it must be for T. Destroy the buffer.
    // - If it's not valid, release locks the normal way in the rangetree.
    //
    // To carry out the optimization we need to record a single txnid
    // and a range buffer for each locktree, each protected by the root
    // lock of the locktree's rangetree. The root lock for a rangetree
    // is grabbed by preparing a locked keyrange on the rangetree.
    TXNID m_sto_txnid;
    range_buffer m_sto_buffer;

    // The single txnid optimization speeds up the case when only one
    // transaction is using the locktree. But it has the potential to
    // hurt the case when more than one txnid exists.
    //
    // There are two things we need to do to make the optimization only
    // optimize the case we care about, and not hurt the general case.
    //
    // Bound the worst-case latency for lock migration when the
    // optimization stops working:
    // - Idea: Stop the optimization and migrate immediate if we notice
    // the single txnid has takes many locks in the range buffer.
    // - Implementation: Enforce a max size on the single txnid range buffer.
    // - Analysis: Choosing the perfect max value, M, is difficult to do
    // without some feedback from the field. Intuition tells us that M should
    // not be so small that the optimization is worthless, and it should not
    // be so big that it's unreasonable to have to wait behind a thread doing
    // the work of converting M buffer locks into rangetree locks.
    //
    // Prevent concurrent-transaction workloads from trying the optimization
    // in vain:
    // - Idea: Don't even bother trying the optimization if we think the
    // system is in a concurrent-transaction state.
    // - Implementation: Do something even simpler than detecting whether the
    // system is in a concurent-transaction state. Just keep a "score" value
    // and some threshold. If at any time the locktree is eligible for the
    // optimization, only do it if the score is at this threshold. When you
    // actually do the optimization but someone has to migrate locks in the buffer
    // (expensive), then reset the score back to zero. Each time a txn
    // releases locks, the score is incremented by 1.
    // - Analysis: If you let the threshold be "C", then at most 1 / C txns will
    // do the optimization in a concurrent-transaction system. Similarly, it
    // takes at most C txns to start using the single txnid optimzation, which
    // is good when the system transitions from multithreaded to single threaded.
    //
    // STO_BUFFER_MAX_SIZE:
    //
    // We choose the max value to be 1 million since most transactions are smaller
    // than 1 million and we can create a rangetree of 1 million elements in
    // less than a second. So we can be pretty confident that this threshold
    // enables the optimization almost always, and prevents super pathological
    // latency issues for the first lock taken by a second thread.
    //
    // STO_SCORE_THRESHOLD:
    //
    // A simple first guess at a good value for the score threshold is 100.
    // By our analysis, we'd end up doing the optimization in vain for
    // around 1% of all transactions, which seems reasonable. Further,
    // if the system goes single threaded, it ought to be pretty quick
    // for 100 transactions to go by, so we won't have to wait long before
    // we start doing the single txind optimzation again.
    static const int STO_BUFFER_MAX_SIZE = 50 * 1024;
    static const int STO_SCORE_THRESHOLD = 100;
    int m_sto_score;

    // statistics about time spent ending the STO early
    uint64_t m_sto_end_early_count;
    tokutime_t m_sto_end_early_time;

    // effect: begins the single txnid optimizaiton, setting m_sto_txnid
    //         to the given txnid.
    // requires: m_sto_txnid is invalid
    void sto_begin(TXNID txnid);

    // effect: append a range to the sto buffer
    // requires: m_sto_txnid is valid
    void sto_append(const DBT *left_key, const DBT *right_key);

    // effect: ends the single txnid optimization, releaseing any memory
    //         stored in the sto buffer, notifying the tracker, and
    //         invalidating m_sto_txnid.
    // requires: m_sto_txnid is valid
    void sto_end(void);

    // params: prepared_lkr is a void * to a prepared locked keyrange. see below.
    // effect: ends the single txnid optimization early, migrating buffer locks
    //         into the rangetree, calling sto_end(), and then setting the
    //         sto_score back to zero.
    // requires: m_sto_txnid is valid
    void sto_end_early(void *prepared_lkr);
    void sto_end_early_no_accounting(void *prepared_lkr);

    // params: prepared_lkr is a void * to a prepared locked keyrange. we can't use
    //         the real type because the compiler won't allow us to forward declare
    //         concurrent_tree::locked_keyrange without including concurrent_tree.h,
    //         which we cannot do here because it is a template implementation.
    // requires: the prepared locked keyrange is for the locktree's rangetree
    // requires: m_sto_txnid is valid
    // effect: migrates each lock in the single txnid buffer into the locktree's
    //         rangetree, notifying the memory tracker as necessary.
    void sto_migrate_buffer_ranges_to_tree(void *prepared_lkr);

    // effect: If m_sto_txnid is valid, then release the txnid's locks
    //         by ending the optimization.
    // requires: If m_sto_txnid is valid, it is equal to the given txnid
    // returns: True if locks were released for this txnid
    bool sto_try_release(TXNID txnid);

    // params: prepared_lkr is a void * to a prepared locked keyrange. see above.
    // requires: the prepared locked keyrange is for the locktree's rangetree
    // effect: If m_sto_txnid is valid and equal to the given txnid, then
    // append a range onto the buffer. Otherwise, if m_sto_txnid is valid
    //        but not equal to this txnid, then migrate the buffer's locks
    //        into the rangetree and end the optimization, setting the score
    //        back to zero.
    // returns: true if the lock was acquired for this txnid
    bool sto_try_acquire(void *prepared_lkr, TXNID txnid,
            const DBT *left_key, const DBT *right_key);

    // Effect:
    //  Provides a hook for a helgrind suppression.
    // Returns:
    //  true if m_sto_txnid is not TXNID_NONE
    bool sto_txnid_is_valid_unsafe(void) const;

    // Effect:
    //  Provides a hook for a helgrind suppression.
    // Returns:
    //  m_sto_score
    int sto_get_score_unsafe(void )const;

    // effect: Creates a locktree that uses the given memory tracker
    //         to report memory usage and honor memory constraints.
    void create(manager::memory_tracker *mem_tracker, DICTIONARY_ID dict_id,
            DESCRIPTOR desc, ft_compare_func cmp);

    void destroy(void);

    void remove_overlapping_locks_for_txnid(TXNID txnid,
            const DBT *left_key, const DBT *right_key);

    int acquire_lock_consolidated(void *prepared_lkr, TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

    int acquire_lock(bool is_write_request, TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

    int try_acquire_lock(bool is_write_request, TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts, bool big_txn);

    void escalate(manager::lt_escalate_cb after_escalate_callback, void *extra);

    friend class locktree_unit_test;
    friend class manager_unit_test;
    friend class lock_request_unit_test;
};
ENSURE_POD(locktree);

} /* namespace toku */

#endif /* TOKU_LOCKTREE_H */

/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_LOCKTREE_H
#define TOKU_LOCKTREE_H

#include <db.h>
#include <toku_pthread.h>

#include <ft/fttypes.h>
#include <ft/comparator.h>

#include <util/omt.h>

#include "txnid_set.h"
#include "wfg.h"
#include "range_buffer.h"

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
    int acquire_read_lock(TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

    // effect: Attempts to grant a write lock for the range of keys between [left_key, right_key].
    // returns: If the lock cannot be granted, return DB_LOCK_NOTGRANTED, and populate the
    //          given conflicts set with the txnids that hold conflicting locks in the range.
    //          If the locktree cannot create more locks, return TOKUDB_OUT_OF_LOCKS.
    int acquire_write_lock(TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

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

    int compare(locktree *lt);

    // The locktree stores some data for lock requests. It doesn't have to know
    // how they work or even what a lock request object looks like.
    struct lt_lock_request_info {
        omt<lock_request *> pending_lock_requests;
        toku_mutex_t mutex;
        bool should_retry_lock_requests;
    };

    // Private info struct for storing pending lock request state.
    // Only to be used by lock requests. We store it here as
    // something less opaque than usual to strike a tradeoff between
    // abstraction and code complexity. It is still fairly abstract
    // since the lock_request object is opaque 
    struct lt_lock_request_info *get_lock_request_info(void);

    // The locktree manager manages a set of locktrees,
    // one for each open dictionary. Locktrees are accessed through
    // the manager, and when they are no longer needed, they can
    // be released by the user.

    class manager {
    public:
        typedef void (*lt_create_cb)(locktree *lt, void *extra);
        typedef void (*lt_destroy_cb)(locktree *lt);

        // note: create_cb is called just after a locktree is first created.
        //       destroy_cb is called just before a locktree is destroyed.
        void create(lt_create_cb create_cb, lt_destroy_cb destroy_cb);

        void destroy(void);

        size_t get_max_lock_memory(void);

        int set_max_lock_memory(size_t max_lock_memory);

        uint64_t get_lock_wait_time(void);

        void set_lock_wait_time(uint64_t lock_wait_time);

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

            // effect: Determines if too many locks or too much memory is being used,
            //         Runs escalation on the manager if so.
            // returns: 0 if there enough resources to create a new lock, or TOKUDB_OUT_OF_LOCKS 
            //          if there are not enough resources and lock escalation failed to free up
            //          enough resources for a new lock.
            int check_current_lock_constraints(void);

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

    private:
        static const uint64_t DEFAULT_MAX_LOCK_MEMORY = 64L * 1024 * 1024;
        static const uint64_t DEFAULT_LOCK_WAIT_TIME = 0;

        // tracks the current number of locks and lock memory
        uint64_t m_max_lock_memory;
        uint64_t m_current_lock_memory;
        memory_tracker m_mem_tracker;

        // lock wait time for blocking row locks, in ms
        uint64_t m_lock_wait_time_ms;

        // the create and destroy callbacks for the locktrees
        lt_create_cb m_lt_create_callback;
        lt_destroy_cb m_lt_destroy_callback;

        omt<locktree *> m_locktree_map;

        // the manager's mutex protects the locktree map
        toku_mutex_t m_mutex;

        void mutex_lock(void);

        void mutex_unlock(void);

        // effect: Gets a locktree from the map.
        // requires: Manager's mutex is held
        locktree *locktree_map_find(const DICTIONARY_ID &dict_id);

        // effect: Puts a locktree into the map.
        // requires: Manager's mutex is held
        void locktree_map_put(locktree *lt);

        // effect: Removes a locktree from the map.
        // requires: Manager's mutex is held
        void locktree_map_remove(locktree *lt);

        // effect: Runs escalation on all locktrees.
        // requires: Manager's mutex is held
        void run_escalation(void);

        static int find_by_dict_id(locktree *const &lt, const DICTIONARY_ID &dict_id);

        friend class manager_unit_test;
    };
    ENSURE_POD(manager);

private:
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

    // the locktree stores locks in a concurrent, non-overlapping rangetree
    concurrent_tree *m_rangetree;

    void *m_userdata;

    struct lt_lock_request_info m_lock_request_info;

    // the following is an optimization for locktrees that contain
    // locks for only a single txnid. in this case, we can just
    // delete everything from the locktree when that txnid unlocks.
    //
    // how do we know that this locktree only has a single txnid?
    //
    // when a txn requests a lock:
    // - if the tree is empty, set the single txnid to that txn, set
    // the optimization to true.
    // - if the tree is not empty, then some txnid has inserted into
    // the tree before and its txnid is m_single_txnid. set the bit
    // to false if that txnid is different than the one about to insert.
    // - if the txnid never changes (ie: only one txnid inserts into
    // a locktree) then the bit stays true and the optimization happens.
    //
    // when a txn releases its locks
    // - check the optimization bit. if it is set, take the locktree's mutex
    // and then check it agian. if it is still set, then perform the optimizaiton.
    // - if the bit was not set, then carry out release locks as usual.
    //
    // the single txnid and the optimizable possible bit are both protected
    // by the root lock on the concurrent tree. the way this is implemented
    // is by a locked keyrange function called prepare(), which grabs
    // the root lock and returns. once acquire/release() is called, the root
    // lock is unlocked if necessary. so prepare() acts as a serialization
    // point where we can safely read and modify these bits.
    TXNID m_single_txnid;
    bool m_single_txnid_optimization_possible;

    // effect: If the single txnid is possible, assert that it
    //         is for the given txnid and then release all of
    //         the locks in the locktree.
    // returns: True if locks were released, false otherwise
    bool try_single_txnid_release_optimization(TXNID txnid);

    // effect: Checks if the single txnid bit is set and, if so,
    //         sets it to false iff the given txnid differs
    //         from the current known single txnid.
    void update_single_txnid_optimization(TXNID txnid);

    // effect: Sets the single txnid bit to be true for the given txnid
    void reset_single_txnid_optimization(TXNID txnid);

    // Effect:
    //  Provides a hook for a helgrind suppression.
    // Returns:
    //  m_single_txnid_optimization_possible
    bool unsafe_read_single_txnid_optimization_possible(void) const;

    // effect: Creates a locktree that uses the given memory tracker
    //         to report memory usage and honor memory constraints.
    void create(manager::memory_tracker *mem_tracker, DICTIONARY_ID dict_id,
            DESCRIPTOR desc, ft_compare_func cmp);

    void destroy(void);

    void remove_overlapping_locks_for_txnid(TXNID txnid,
            const DBT *left_key, const DBT *right_key);

    int try_acquire_lock(bool is_write_request, TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

    int acquire_lock(bool is_write_request, TXNID txnid,
            const DBT *left_key, const DBT *right_key, txnid_set *conflicts);

    void escalate();

    friend class locktree_unit_test;
    friend class manager_unit_test;
    friend class lock_request_unit_test;
};
ENSURE_POD(locktree);

} /* namespace toku */

#endif /* TOKU_LOCKTREE_H */

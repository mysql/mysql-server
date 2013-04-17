/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <memory.h>

#include <util/growable_array.h>

#include <portability/toku_time.h>

#include "locktree.h"

// including the concurrent_tree here expands the templates
// and "defines" the implementation, so we do it here in
// the locktree source file instead of the header.
#include "concurrent_tree.h"

namespace toku {

// A locktree represents the set of row locks owned by all transactions
// over an open dictionary. Read and write ranges are represented as
// a left and right key which are compared with the given descriptor 
// and comparison fn.
//
// Each locktree has a reference count which it manages
// but does nothing based on the value of the reference count - it is
// up to the user of the locktree to destroy it when it sees fit.

void locktree::create(manager::memory_tracker *mem_tracker, DICTIONARY_ID dict_id,
        DESCRIPTOR desc, ft_compare_func cmp) {
    m_mem_tracker = mem_tracker;
    m_dict_id = dict_id;

    // the only reason m_cmp is malloc'd here is to prevent gdb from printing
    // out an entire DB struct every time you inspect a locktree.
    XCALLOC(m_cmp);
    m_cmp->create(cmp, desc);
    m_reference_count = 1;
    m_userdata = nullptr;
    XCALLOC(m_rangetree);
    m_rangetree->create(m_cmp);
    reset_single_txnid_optimization(TXNID_NONE);

    m_lock_request_info.pending_lock_requests.create();
    m_lock_request_info.mutex = TOKU_MUTEX_INITIALIZER;
    m_lock_request_info.should_retry_lock_requests = false;

    // Threads read the should retry bit without a lock
    // for performance. It's ok to read the wrong value.
    // - If you think you should but you shouldn't, you waste a little time.
    // - If you think you shouldn't but you should, then some other thread
    // will come around to do the work of retrying requests instead of you.
    TOKU_VALGRIND_HG_DISABLE_CHECKING(
            &m_lock_request_info.should_retry_lock_requests,
            sizeof(m_lock_request_info.should_retry_lock_requests));
    TOKU_DRD_IGNORE_VAR(m_lock_request_info.should_retry_lock_requests);
}

void locktree::destroy(void) {
    invariant(m_reference_count == 0);
    m_rangetree->destroy();
    toku_free(m_cmp);
    toku_free(m_rangetree);

    m_lock_request_info.pending_lock_requests.destroy();
}

// a container for a range/txnid pair
struct row_lock {
    keyrange range;
    TXNID txnid;
};

// iterate over a locked keyrange and copy out all of the data,
// storing each row lock into the given growable array. the
// caller does not own the range inside the returned row locks,
// so remove from the tree with care using them as keys.
static void iterate_and_get_overlapping_row_locks(
        const concurrent_tree::locked_keyrange &lkr,
        GrowableArray<row_lock> *row_locks) {
    struct copy_fn_obj {
        GrowableArray<row_lock> *row_locks;
        bool fn(const keyrange &range, TXNID txnid) {
            row_lock lock = { .range = range, .txnid = txnid };
            row_locks->push(lock);
            return true;
        }
    } copy_fn;
    copy_fn.row_locks = row_locks;
    lkr.iterate(&copy_fn);
}

// given a txnid and a set of overlapping row locks, determine
// which txnids are conflicting, and store them in the conflicts
// set, if given.
static bool determine_conflicting_txnids(const GrowableArray<row_lock> &row_locks,
        const TXNID &txnid, txnid_set *conflicts) {
    bool conflicts_exist = false;
    const size_t num_overlaps = row_locks.get_size();
    for (size_t i = 0; i < num_overlaps; i++) {
        const row_lock lock = row_locks.fetch_unchecked(i);
        const TXNID other_txnid = lock.txnid;
        if (other_txnid != txnid) {
            if (conflicts) {
                conflicts->add(other_txnid);
            }
            conflicts_exist = true;
        }
    }
    return conflicts_exist;
}

// given a row lock, what is the effective memory size?
// that is, how much memory does it take when stored in a tree?
static uint64_t effective_row_lock_memory_size(const row_lock &lock) {
    const uint64_t overhead = concurrent_tree::get_insertion_memory_overhead();
    return lock.range.get_memory_size() + overhead;
}

// remove all row locks from the given lkr, then notify the memory tracker.
static void remove_all_row_locks(concurrent_tree::locked_keyrange *lkr,
        locktree::manager::memory_tracker *mem_tracker) {
    uint64_t mem_released = 0;
    uint64_t num_removed = lkr->remove_all(&mem_released);
    mem_released += num_removed * concurrent_tree::get_insertion_memory_overhead();
    mem_tracker->note_mem_released(mem_released);
}

// remove and destroy the given row lock from the locked keyrange,
// then notify the memory tracker of the newly freed lock.
static void remove_row_lock(concurrent_tree::locked_keyrange *lkr,
        const row_lock &lock, locktree::manager::memory_tracker *mem_tracker) {
    const uint64_t mem_released = effective_row_lock_memory_size(lock);
    lkr->remove(lock.range);
    mem_tracker->note_mem_released(mem_released);
}

// insert a row lock into the locked keyrange, then notify
// the memory tracker of this newly acquired lock.
static void insert_row_lock(concurrent_tree::locked_keyrange *lkr,
        const row_lock &lock, locktree::manager::memory_tracker *mem_tracker) {
    uint64_t mem_used = effective_row_lock_memory_size(lock);
    lkr->insert(lock.range, lock.txnid);
    mem_tracker->note_mem_used(mem_used);
}

void locktree::update_single_txnid_optimization(TXNID txnid) {
    if (m_rangetree->is_empty()) {
        // the optimization becomes possible for this txnid if the
        // tree was previously empy before we touched it. the idea
        // here is that if we are still the only one to have touched
        // it by the time we commit, the optimization holds.
        reset_single_txnid_optimization(txnid);
    } else {
        // the tree is not empty, so some txnid must have touched it.
        invariant(m_single_txnid != TXNID_NONE);
        // the optimization is not possible if the txnid has changed 
        if (m_single_txnid_optimization_possible && m_single_txnid != txnid) {
            m_single_txnid_optimization_possible = false;
        }
    }
}

// acquire a lock in the given key range, inclusive. if successful,
// return 0. otherwise, populate the conflicts txnid_set with the set of
// transactions that conflict with this request.
int locktree::acquire_lock(bool is_write_request, TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    keyrange requested_range;
    requested_range.create(left_key, right_key);

    // we are only supporting write locks for simplicity
    invariant(is_write_request);

    // acquire and prepare a locked keyrange over the requested range.
    // prepare is a serialzation point, so we take the opportunity to
    // update the single txnid optimization bits.
    concurrent_tree::locked_keyrange lkr;
    lkr.prepare(m_rangetree);
    update_single_txnid_optimization(txnid);
    lkr.acquire(requested_range);

    // copy out the set of overlapping row locks.
    GrowableArray<row_lock> overlapping_row_locks;
    overlapping_row_locks.init();
    iterate_and_get_overlapping_row_locks(lkr, &overlapping_row_locks);
    size_t num_overlapping_row_locks = overlapping_row_locks.get_size();

    // if any overlapping row locks conflict with this request, bail out.
    bool conflicts_exist = determine_conflicting_txnids(overlapping_row_locks, txnid, conflicts);
    if (!conflicts_exist) {
        // there are no conflicts, so all of the overlaps are for the requesting txnid.
        // so, we must consolidate all existing overlapping ranges and the requested
        // range into one dominating range. then we insert the dominating range.
        for (size_t i = 0; i < num_overlapping_row_locks; i++) {
            row_lock overlapping_lock = overlapping_row_locks.fetch_unchecked(i);
            invariant(overlapping_lock.txnid == txnid);
            requested_range.extend(m_cmp, overlapping_lock.range);
            remove_row_lock(&lkr, overlapping_lock, m_mem_tracker);
        }

        row_lock new_lock = { .range = requested_range, .txnid = txnid };
        insert_row_lock(&lkr, new_lock, m_mem_tracker);
    }

    lkr.release();
    overlapping_row_locks.deinit();
    requested_range.destroy();

    // if there were conflicts, the lock is not granted.
    if (conflicts_exist) {
        return DB_LOCK_NOTGRANTED;
    } else {
        return 0;
    }
}

int locktree::try_acquire_lock(bool is_write_request, TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    int r = m_mem_tracker->check_current_lock_constraints();
    if (r == 0) {
        r = acquire_lock(is_write_request, txnid, left_key, right_key, conflicts);
    }
    return r;
}

// the locktree silently upgrades read locks to write locks for simplicity
int locktree::acquire_read_lock(TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    return acquire_write_lock(txnid, left_key, right_key, conflicts);
}

int locktree::acquire_write_lock(TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    return try_acquire_lock(true, txnid, left_key, right_key, conflicts);
}

void locktree::get_conflicts(bool is_write_request, TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    // because we only support write locks, ignore this bit for now.
    (void) is_write_request;

    // preparing and acquire a locked keyrange over the range
    keyrange range;
    range.create(left_key, right_key);
    concurrent_tree::locked_keyrange lkr;
    lkr.prepare(m_rangetree);
    lkr.acquire(range);

    // copy out the set of overlapping row locks and determine the conflicts
    GrowableArray<row_lock> overlapping_row_locks;
    overlapping_row_locks.init();
    iterate_and_get_overlapping_row_locks(lkr, &overlapping_row_locks);

    // we don't care if conflicts exist. we just want the conflicts set populated.
    (void) determine_conflicting_txnids(overlapping_row_locks, txnid, conflicts);

    lkr.release();
    overlapping_row_locks.deinit();
    range.destroy();
}

// Effect:
//  For each range in the lock tree that overlaps the given range and has
//  the given txnid, remove it.
// Rationale:
//  In the common case, there is only the range [left_key, right_key] and
//  it is associated with txnid, so this is a single tree delete.
//
//  However, consolidation and escalation change the objects in the tree
//  without telling the txn anything.  In this case, the txn may own a
//  large range lock that represents its ownership of many smaller range
//  locks.  For example, the txn may think it owns point locks on keys 1,
//  2, and 3, but due to escalation, only the object [1,3] exists in the
//  tree.
//
//  The first call for a small lock will remove the large range lock, and
//  the rest of the calls should do nothing.  After the first release,
//  another thread can acquire one of the locks that the txn thinks it
//  still owns.  That's ok, because the txn doesn't want it anymore (it
//  unlocks everything at once), but it may find a lock that it does not
//  own.
//
//  In our example, the txn unlocks key 1, which actually removes the
//  whole lock [1,3].  Now, someone else can lock 2 before our txn gets
//  around to unlocking 2, so we should not remove that lock.
void locktree::remove_overlapping_locks_for_txnid(TXNID txnid,
        const DBT *left_key, const DBT *right_key) {

    keyrange release_range;
    release_range.create(left_key, right_key);

    // acquire and prepare a locked keyrange over the release range
    concurrent_tree::locked_keyrange lkr;
    lkr.prepare(m_rangetree);
    lkr.acquire(release_range);

    // copy out the set of overlapping row locks.
    GrowableArray<row_lock> overlapping_row_locks;
    overlapping_row_locks.init();
    iterate_and_get_overlapping_row_locks(lkr, &overlapping_row_locks);
    size_t num_overlapping_row_locks = overlapping_row_locks.get_size();

    for (size_t i = 0; i < num_overlapping_row_locks; i++) {
        row_lock lock = overlapping_row_locks.fetch_unchecked(i);
        // If this isn't our lock, that's ok, just don't remove it.
        // See rationale above.
        if (lock.txnid == txnid) {
            remove_row_lock(&lkr, lock, m_mem_tracker);
        }
    }

    lkr.release();
    overlapping_row_locks.deinit();
    release_range.destroy();
}

// reset the optimization bit to possible for the given txnid
void locktree::reset_single_txnid_optimization(TXNID txnid) {
    m_single_txnid = txnid;
    m_single_txnid_optimization_possible = true;
}

bool locktree::try_single_txnid_release_optimization(TXNID txnid) {
    bool released = false;
    TOKU_DRD_IGNORE_VAR(m_single_txnid_optimization_possible);
    bool optimization_possible = m_single_txnid_optimization_possible;
    TOKU_DRD_STOP_IGNORING_VAR(m_single_txnid_optimization_possible);
    if (optimization_possible) {
        // check the bit again with a prepared locked keyrange,
        // which protects the optimization bits and rangetree data
        concurrent_tree::locked_keyrange lkr;
        lkr.prepare(m_rangetree);
        if (m_single_txnid_optimization_possible) {
            // this txnid better be the single txnid on this locktree,
            // or else we are in big trouble (meaning the logic is broken)
            invariant(m_single_txnid == txnid);

            // acquire a locked range on -inf, +inf. this is just for 
            // readability's sake, since the prepared lkr already has
            // the root locked, but the API says to do this so we do.
            keyrange infinite_range = keyrange::get_infinite_range();
            lkr.acquire(infinite_range);

            // knowing that only our row locks exist in the locktree
            // and that we have the entire thing locked, remove everything.
            remove_all_row_locks(&lkr, m_mem_tracker);

            // reset the optimization back to possible, with no txnid
            // we set txnid to TXNID_NONE for invariant purposes.
            reset_single_txnid_optimization(TXNID_NONE);
            released = true;
        }
        lkr.release();
    }
    return released;
}

// release all of the locks for a txnid whose endpoints are pairs
// in the given range buffer.
void locktree::release_locks(TXNID txnid, const range_buffer *ranges) {
    // try the single txn optimization. if it worked, then all of the
    // locks are already released, otherwise we need to do it here.
    bool released = try_single_txnid_release_optimization(txnid);
    if (!released) {
        range_buffer::iterator iter;
        range_buffer::iterator::record rec;
        iter.create(ranges);
        while (iter.current(&rec)) {
            const DBT *left_key = rec.get_left_key();
            const DBT *right_key = rec.get_right_key();
            remove_overlapping_locks_for_txnid(txnid, left_key, right_key);
            iter.next();
        }
    }
}

// iterate over a locked keyrange and extract copies of the first N
// row locks, storing each one into the given array of size N,
// then removing each extracted lock from the locked keyrange.
static int extract_first_n_row_locks(concurrent_tree::locked_keyrange *lkr,
        locktree::manager::memory_tracker *mem_tracker,
        row_lock *row_locks, int num_to_extract) { 

    struct extract_fn_obj {
        int num_extracted;
        int num_to_extract;
        row_lock *row_locks;
        bool fn(const keyrange &range, TXNID txnid) {
            if (num_extracted < num_to_extract) {
                row_lock lock;
                lock.range.create_copy(range);
                lock.txnid = txnid;
                row_locks[num_extracted++] = lock;
                return true;
            } else {
                return false;
            }
        }
    } extract_fn;

    extract_fn.row_locks = row_locks;
    extract_fn.num_to_extract = num_to_extract;
    extract_fn.num_extracted = 0;
    lkr->iterate(&extract_fn);

    // now that the ranges have been copied out, complete
    // the extraction by removing the ranges from the tree.
    // use remove_row_lock() so we properly track the
    // amount of memory and number of locks freed.
    int num_extracted = extract_fn.num_extracted;
    invariant(num_extracted <= num_to_extract);
    for (int i = 0; i < num_extracted; i++) {
        remove_row_lock(lkr, row_locks[i], mem_tracker);
    }

    return num_extracted;
}

// escalate the locks in the locktree by merging adjacent
// locks that have the same txnid into one larger lock.
//
// if there's only one txnid in the locktree then this
// approach works well. if there are many txnids and each
// has locks in a random/alternating order, then this does
// not work so well.
void locktree::escalate(void) {
    GrowableArray<row_lock> escalated_locks;
    escalated_locks.init();

    // prepare and acquire a locked keyrange on the entire locktree
    concurrent_tree::locked_keyrange lkr;
    keyrange infinite_range = keyrange::get_infinite_range();
    lkr.prepare(m_rangetree);
    lkr.acquire(infinite_range);

    // extract and remove batches of row locks from the locktree
    int num_extracted;
    static const int num_row_locks_per_batch = 128;
    row_lock *XCALLOC_N(num_row_locks_per_batch, extracted_buf);

    // we always remove the "first" n because we are removing n
    // each time we do an extraction. so this loops until its empty.
    while ((num_extracted = extract_first_n_row_locks(&lkr, m_mem_tracker,
                    extracted_buf, num_row_locks_per_batch)) > 0) {
        int current_index = 0;
        while (current_index < num_extracted) {
            // every batch of extracted locks is in range-sorted order. search
            // through them and merge adjacent locks with the same txnid into
            // one dominating lock and save it to a set of escalated locks.
            //
            // first, find the index of the next row lock with a different txnid
            int next_txnid_index = current_index + 1;
            while (next_txnid_index < num_extracted &&
                    extracted_buf[current_index].txnid == extracted_buf[next_txnid_index].txnid) {
                next_txnid_index++;
            }

            // create a range which dominates the ranges between 
            // the current index and the next txnid's index (excle).
            keyrange merged_range;
            merged_range.create(
                    extracted_buf[current_index].range.get_left_key(), 
                    extracted_buf[next_txnid_index - 1].range.get_right_key());

            // save the new lock, continue from the next txnid's index
            row_lock merged_row_lock;
            merged_row_lock.range.create_copy(merged_range);
            merged_row_lock.txnid = extracted_buf[current_index].txnid;
            escalated_locks.push(merged_row_lock);
            current_index = next_txnid_index;
        }

        // destroy the ranges copied during the extraction
        for (int i = 0; i < num_extracted; i++) {
            extracted_buf[i].range.destroy();
        }
    }
    toku_free(extracted_buf);

    // we should have extracted every lock from the old rangetree.
    // now it is time to repopulate it with the escalated locks.
    invariant(m_rangetree->is_empty());
    size_t new_num_locks = escalated_locks.get_size();
    for (size_t i = 0; i < new_num_locks; i++) {
        row_lock lock = escalated_locks.fetch_unchecked(i);
        insert_row_lock(&lkr, lock, m_mem_tracker);
        lock.range.destroy();
    }

    escalated_locks.deinit();
    lkr.release();
}

void *locktree::get_userdata(void) {
    return m_userdata;
}

void locktree::set_userdata(void *userdata) {
    m_userdata = userdata;
}

struct locktree::lt_lock_request_info *locktree::get_lock_request_info(void) {
    return &m_lock_request_info;
}

void locktree::set_descriptor(DESCRIPTOR desc) {
    m_cmp->set_descriptor(desc);
}

int locktree::compare(locktree *lt) {
    if (m_dict_id.dictid < lt->m_dict_id.dictid) {
        return -1;
    } else if (m_dict_id.dictid == lt->m_dict_id.dictid) {
        return 0;
    } else {
        return 1;
    }
}

} /* namespace toku */

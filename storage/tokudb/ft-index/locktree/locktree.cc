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

#include <memory.h>

#include <util/growable_array.h>

#include <portability/toku_pthread.h>
#include <portability/toku_time.h>

#include "locktree.h"
#include "range_buffer.h"

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

    m_sto_txnid = TXNID_NONE;
    m_sto_buffer.create();
    m_sto_score = STO_SCORE_THRESHOLD;
    m_sto_end_early_count = 0;
    m_sto_end_early_time = 0;

    m_lock_request_info.pending_lock_requests.create();
    ZERO_STRUCT(m_lock_request_info.mutex);
    toku_mutex_init(&m_lock_request_info.mutex, nullptr);
    m_lock_request_info.should_retry_lock_requests = false;
    ZERO_STRUCT(m_lock_request_info.counters);

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
    m_sto_buffer.destroy();

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
        const concurrent_tree::locked_keyrange *lkr,
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
    lkr->iterate(&copy_fn);
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

// how much memory does a row lock take up in a concurrent tree?
static uint64_t row_lock_size_in_tree(const row_lock &lock) {
    const uint64_t overhead = concurrent_tree::get_insertion_memory_overhead();
    return lock.range.get_memory_size() + overhead;
}

// remove and destroy the given row lock from the locked keyrange,
// then notify the memory tracker of the newly freed lock.
static void remove_row_lock_from_tree(concurrent_tree::locked_keyrange *lkr,
        const row_lock &lock, locktree::manager::memory_tracker *mem_tracker) {
    const uint64_t mem_released = row_lock_size_in_tree(lock);
    lkr->remove(lock.range);
    mem_tracker->note_mem_released(mem_released);
}

// insert a row lock into the locked keyrange, then notify
// the memory tracker of this newly acquired lock.
static void insert_row_lock_into_tree(concurrent_tree::locked_keyrange *lkr,
        const row_lock &lock, locktree::manager::memory_tracker *mem_tracker) {
    uint64_t mem_used = row_lock_size_in_tree(lock);
    lkr->insert(lock.range, lock.txnid);
    mem_tracker->note_mem_used(mem_used);
}

void locktree::sto_begin(TXNID txnid) {
    invariant(m_sto_txnid == TXNID_NONE);
    invariant(m_sto_buffer.is_empty());
    m_sto_txnid = txnid;
}

void locktree::sto_append(const DBT *left_key, const DBT *right_key) {
    uint64_t buffer_mem, delta;
    keyrange range;
    range.create(left_key, right_key);

    buffer_mem = m_sto_buffer.get_num_bytes();
    m_sto_buffer.append(left_key, right_key);
    delta = m_sto_buffer.get_num_bytes() - buffer_mem;
    m_mem_tracker->note_mem_used(delta);
}

void locktree::sto_end(void) {
    uint64_t num_bytes = m_sto_buffer.get_num_bytes();
    m_mem_tracker->note_mem_released(num_bytes);
    m_sto_buffer.destroy();
    m_sto_buffer.create();
    m_sto_txnid = TXNID_NONE;
}

void locktree::sto_end_early_no_accounting(void *prepared_lkr) {
    sto_migrate_buffer_ranges_to_tree(prepared_lkr);
    sto_end();
    m_sto_score = 0;
}

void locktree::sto_end_early(void *prepared_lkr) {
    m_sto_end_early_count++;

    tokutime_t t0 = toku_time_now();
    sto_end_early_no_accounting(prepared_lkr);
    tokutime_t t1 = toku_time_now();

    m_sto_end_early_time += (t1 - t0);
}

void locktree::sto_migrate_buffer_ranges_to_tree(void *prepared_lkr) {
    // There should be something to migrate, and nothing in the rangetree.
    invariant(!m_sto_buffer.is_empty());
    invariant(m_rangetree->is_empty());

    concurrent_tree sto_rangetree;
    concurrent_tree::locked_keyrange sto_lkr;
    sto_rangetree.create(m_cmp);

    // insert all of the ranges from the single txnid buffer into a new rangtree
    range_buffer::iterator iter;
    range_buffer::iterator::record rec;
    iter.create(&m_sto_buffer);
    while (iter.current(&rec)) {
        sto_lkr.prepare(&sto_rangetree);
        int r = acquire_lock_consolidated(&sto_lkr,
                m_sto_txnid, rec.get_left_key(), rec.get_right_key(), nullptr);
        invariant_zero(r);
        sto_lkr.release();
        iter.next();
    }

    // Iterate the newly created rangetree and insert each range into the
    // locktree's rangetree, on behalf of the old single txnid.
    struct migrate_fn_obj {
        concurrent_tree::locked_keyrange *dst_lkr;
        bool fn(const keyrange &range, TXNID txnid) {
            dst_lkr->insert(range, txnid);
            return true;
        }
    } migrate_fn;
    migrate_fn.dst_lkr = static_cast<concurrent_tree::locked_keyrange *>(prepared_lkr);
    sto_lkr.prepare(&sto_rangetree);
    sto_lkr.iterate(&migrate_fn);
    sto_lkr.remove_all();
    sto_lkr.release();
    sto_rangetree.destroy();
    invariant(!m_rangetree->is_empty());
}

bool locktree::sto_try_acquire(void *prepared_lkr, TXNID txnid,
        const DBT *left_key, const DBT *right_key) {
    if (m_rangetree->is_empty() && m_sto_buffer.is_empty() && m_sto_score >= STO_SCORE_THRESHOLD) {
        // We can do the optimization because the rangetree is empty, and
        // we know its worth trying because the sto score is big enough.
        sto_begin(txnid);
    } else if (m_sto_txnid != TXNID_NONE) {
        // We are currently doing the optimization. Check if we need to cancel
        // it because a new txnid appeared, or if the current single txnid has
        // taken too many locks already.
        if (m_sto_txnid != txnid || m_sto_buffer.get_num_ranges() > STO_BUFFER_MAX_SIZE) {
            sto_end_early(prepared_lkr);
        }
    }

    // At this point the sto txnid is properly set. If it is valid, then
    // this txnid can append its lock to the sto buffer successfully.
    if (m_sto_txnid != TXNID_NONE) {
        invariant(m_sto_txnid == txnid);
        sto_append(left_key, right_key);
        return true;
    } else {
        invariant(m_sto_buffer.is_empty());
        return false;
    }
}

// try to acquire a lock and consolidate it with existing locks if possible
// param: lkr, a prepared locked keyrange
// return: 0 on success, DB_LOCK_NOTGRANTED if conflicting locks exist.
int locktree::acquire_lock_consolidated(void *prepared_lkr, TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    int r = 0;
    concurrent_tree::locked_keyrange *lkr;

    keyrange requested_range;
    requested_range.create(left_key, right_key);
    lkr = static_cast<concurrent_tree::locked_keyrange *>(prepared_lkr); 
    lkr->acquire(requested_range);

    // copy out the set of overlapping row locks.
    GrowableArray<row_lock> overlapping_row_locks;
    overlapping_row_locks.init();
    iterate_and_get_overlapping_row_locks(lkr, &overlapping_row_locks);
    size_t num_overlapping_row_locks = overlapping_row_locks.get_size();

    // if any overlapping row locks conflict with this request, bail out.
    bool conflicts_exist = determine_conflicting_txnids(
            overlapping_row_locks, txnid, conflicts);
    if (!conflicts_exist) {
        // there are no conflicts, so all of the overlaps are for the requesting txnid.
        // so, we must consolidate all existing overlapping ranges and the requested
        // range into one dominating range. then we insert the dominating range.
        for (size_t i = 0; i < num_overlapping_row_locks; i++) {
            row_lock overlapping_lock = overlapping_row_locks.fetch_unchecked(i);
            invariant(overlapping_lock.txnid == txnid);
            requested_range.extend(m_cmp, overlapping_lock.range);
            remove_row_lock_from_tree(lkr, overlapping_lock, m_mem_tracker);
        }

        row_lock new_lock = { .range = requested_range, .txnid = txnid };
        insert_row_lock_into_tree(lkr, new_lock, m_mem_tracker);
    } else {
        r = DB_LOCK_NOTGRANTED;
    }

    requested_range.destroy();
    overlapping_row_locks.deinit();
    return r;
}

// acquire a lock in the given key range, inclusive. if successful,
// return 0. otherwise, populate the conflicts txnid_set with the set of
// transactions that conflict with this request.
int locktree::acquire_lock(bool is_write_request, TXNID txnid,
        const DBT *left_key, const DBT *right_key, txnid_set *conflicts) {
    int r = 0;

    // we are only supporting write locks for simplicity
    invariant(is_write_request);

    // acquire and prepare a locked keyrange over the requested range.
    // prepare is a serialzation point, so we take the opportunity to
    // try the single txnid optimization first.
    concurrent_tree::locked_keyrange lkr;
    lkr.prepare(m_rangetree);

    bool acquired = sto_try_acquire(&lkr, txnid, left_key, right_key);
    if (!acquired) {
        r = acquire_lock_consolidated(&lkr, txnid, left_key, right_key, conflicts);
    }

    lkr.release();
    return r;
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
    iterate_and_get_overlapping_row_locks(&lkr, &overlapping_row_locks);

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
    iterate_and_get_overlapping_row_locks(&lkr, &overlapping_row_locks);
    size_t num_overlapping_row_locks = overlapping_row_locks.get_size();

    for (size_t i = 0; i < num_overlapping_row_locks; i++) {
        row_lock lock = overlapping_row_locks.fetch_unchecked(i);
        // If this isn't our lock, that's ok, just don't remove it.
        // See rationale above.
        if (lock.txnid == txnid) {
            remove_row_lock_from_tree(&lkr, lock, m_mem_tracker);
        }
    }

    lkr.release();
    overlapping_row_locks.deinit();
    release_range.destroy();
}

bool locktree::sto_txnid_is_valid_unsafe(void) const {
    return m_sto_txnid != TXNID_NONE;
}

int locktree::sto_get_score_unsafe(void) const {
    return m_sto_score;
}

bool locktree::sto_try_release(TXNID txnid) {
    bool released = false;
    if (sto_txnid_is_valid_unsafe()) {
        // check the bit again with a prepared locked keyrange,
        // which protects the optimization bits and rangetree data
        concurrent_tree::locked_keyrange lkr;
        lkr.prepare(m_rangetree);
        if (m_sto_txnid != TXNID_NONE) {
            // this txnid better be the single txnid on this locktree,
            // or else we are in big trouble (meaning the logic is broken)
            invariant(m_sto_txnid == txnid);
            invariant(m_rangetree->is_empty());
            sto_end();
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
    bool released = sto_try_release(txnid);
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
        // Increase the sto score slightly. Eventually it will hit
        // the threshold and we'll try the optimization again. This
        // is how a previously multithreaded system transitions into
        // a single threaded system that benefits from the optimization.
        if (sto_get_score_unsafe() < STO_SCORE_THRESHOLD) {
            toku_sync_fetch_and_add(&m_sto_score, 1);
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
    // use remove_row_lock_from_tree() so we properly track the
    // amount of memory and number of locks freed.
    int num_extracted = extract_fn.num_extracted;
    invariant(num_extracted <= num_to_extract);
    for (int i = 0; i < num_extracted; i++) {
        remove_row_lock_from_tree(lkr, row_locks[i], mem_tracker);
    }

    return num_extracted;
}

// Store each newly escalated lock in a range buffer for appropriate txnid.
// We'll rebuild the locktree by iterating over these ranges, and then we
// can pass back each txnid/buffer pair individually through a callback
// to notify higher layers that locks have changed.
struct txnid_range_buffer {
    TXNID txnid;
    range_buffer buffer;

    static int find_by_txnid(const struct txnid_range_buffer &other_buffer, const TXNID &txnid) {
        if (txnid < other_buffer.txnid) {
            return -1;
        } else if (other_buffer.txnid == txnid) {
            return 0;
        } else {
            return 1;
        }
    }
};

// escalate the locks in the locktree by merging adjacent
// locks that have the same txnid into one larger lock.
//
// if there's only one txnid in the locktree then this
// approach works well. if there are many txnids and each
// has locks in a random/alternating order, then this does
// not work so well.
void locktree::escalate(manager::lt_escalate_cb after_escalate_callback, void *after_escalate_callback_extra) {
    omt<struct txnid_range_buffer, struct txnid_range_buffer *> range_buffers;
    range_buffers.create();

    // prepare and acquire a locked keyrange on the entire locktree
    concurrent_tree::locked_keyrange lkr;
    keyrange infinite_range = keyrange::get_infinite_range();
    lkr.prepare(m_rangetree);
    lkr.acquire(infinite_range);

    // if we're in the single txnid optimization, simply call it off.
    // if you have to run escalation, you probably don't care about
    // the optimization anyway, and this makes things easier.
    if (m_sto_txnid != TXNID_NONE) {
        // We are already accounting for this escalation time and
        // count, so don't do it for sto_end_early too.
        sto_end_early_no_accounting(&lkr);
    }

    // extract and remove batches of row locks from the locktree
    int num_extracted;
    const int num_row_locks_per_batch = 128;
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

            // Create an escalated range for the current txnid that dominates
            // each range between the current indext and the next txnid's index.
            const TXNID current_txnid = extracted_buf[current_index].txnid;
            const DBT *escalated_left_key = extracted_buf[current_index].range.get_left_key(); 
            const DBT *escalated_right_key = extracted_buf[next_txnid_index - 1].range.get_right_key();

            // Try to find a range buffer for the current txnid. Create one if it doesn't exist.
            // Then, append the new escalated range to the buffer.
            uint32_t idx;
            struct txnid_range_buffer new_range_buffer;
            struct txnid_range_buffer *existing_range_buffer;
            int r = range_buffers.find_zero<TXNID, txnid_range_buffer::find_by_txnid>(
                    current_txnid,
                    &existing_range_buffer,
                    &idx
                    );
            if (r == DB_NOTFOUND) {
                new_range_buffer.txnid = current_txnid;
                new_range_buffer.buffer.create();
                new_range_buffer.buffer.append(escalated_left_key, escalated_right_key);
                range_buffers.insert_at(new_range_buffer, idx);
            } else {
                invariant_zero(r);
                invariant(existing_range_buffer->txnid == current_txnid);
                existing_range_buffer->buffer.append(escalated_left_key, escalated_right_key);
            }

            current_index = next_txnid_index;
        }

        // destroy the ranges copied during the extraction
        for (int i = 0; i < num_extracted; i++) {
            extracted_buf[i].range.destroy();
        }
    }
    toku_free(extracted_buf);

    // Rebuild the locktree from each range in each range buffer,
    // then notify higher layers that the txnid's locks have changed.
    invariant(m_rangetree->is_empty());
    const size_t num_range_buffers = range_buffers.size();
    for (size_t i = 0; i < num_range_buffers; i++) {
        struct txnid_range_buffer *current_range_buffer;
        int r = range_buffers.fetch(i, &current_range_buffer);
        invariant_zero(r);

        const TXNID current_txnid = current_range_buffer->txnid;
        range_buffer::iterator iter;
        range_buffer::iterator::record rec;
        iter.create(&current_range_buffer->buffer);
        while (iter.current(&rec)) {
            keyrange range;
            range.create(rec.get_left_key(), rec.get_right_key());
            row_lock lock = { .range = range, .txnid = current_txnid };
            insert_row_lock_into_tree(&lkr, lock, m_mem_tracker);
            iter.next();
        }

        // Notify higher layers that locks have changed for the current txnid
        if (after_escalate_callback) {
            after_escalate_callback(current_txnid, this, current_range_buffer->buffer, after_escalate_callback_extra);
        }
        current_range_buffer->buffer.destroy();
    }
    range_buffers.destroy();

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

locktree::manager::memory_tracker *locktree::get_mem_tracker(void) const {
    return m_mem_tracker;
}

int locktree::compare(const locktree *lt) {
    if (m_dict_id.dictid < lt->m_dict_id.dictid) {
        return -1;
    } else if (m_dict_id.dictid == lt->m_dict_id.dictid) {
        return 0;
    } else {
        return 1;
    }
}

DICTIONARY_ID locktree::get_dict_id() const {
    return m_dict_id;
}

} /* namespace toku */

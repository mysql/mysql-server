/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdlib.h>
#include <string.h>

#include "locktree.h"

namespace toku {

void locktree::manager::create(lt_create_cb create_cb, lt_destroy_cb destroy_cb) {
    m_max_lock_memory = DEFAULT_MAX_LOCK_MEMORY;
    m_current_lock_memory = 0;
    m_lock_wait_time_ms = DEFAULT_LOCK_WAIT_TIME;
    m_mem_tracker.set_manager(this);

    m_locktree_map.create();
    m_lt_create_callback = create_cb;
    m_lt_destroy_callback = destroy_cb;
    m_mutex = TOKU_MUTEX_INITIALIZER;
}

void locktree::manager::destroy(void) {
    invariant(m_current_lock_memory == 0);
    invariant(m_locktree_map.size() == 0);
    m_locktree_map.destroy();
}

void locktree::manager::mutex_lock(void) {
    toku_mutex_lock(&m_mutex);
}

void locktree::manager::mutex_unlock(void) {
    toku_mutex_unlock(&m_mutex);
}

size_t locktree::manager::get_max_lock_memory(void) {
    return m_max_lock_memory;
}

int locktree::manager::set_max_lock_memory(size_t max_lock_memory) {
    int r = 0;
    mutex_lock();
    if (max_lock_memory < m_current_lock_memory) {
        r = EDOM;
    } else {
        m_max_lock_memory = max_lock_memory;
    }
    mutex_unlock();
    return r;
}

uint64_t locktree::manager::get_lock_wait_time(void) {
    return m_lock_wait_time_ms;
}

void locktree::manager::set_lock_wait_time(uint64_t lock_wait_time_ms) {
    m_lock_wait_time_ms = lock_wait_time_ms;
}

int locktree::manager::find_by_dict_id(locktree *const &lt, const DICTIONARY_ID &dict_id) {
    if (lt->m_dict_id.dictid < dict_id.dictid) {
        return -1;
    } else if (lt->m_dict_id.dictid == dict_id.dictid) {
        return 0;
    } else {
        return 1;
    }
}

locktree *locktree::manager::locktree_map_find(const DICTIONARY_ID &dict_id) {
    locktree *lt;
    int r = m_locktree_map.find_zero<DICTIONARY_ID, find_by_dict_id>(dict_id, &lt, nullptr);
    return r == 0 ? lt : nullptr;
}

void locktree::manager::locktree_map_put(locktree *lt) {
    int r = m_locktree_map.insert<DICTIONARY_ID, find_by_dict_id>(lt, lt->m_dict_id, nullptr);
    invariant_zero(r);
}

void locktree::manager::locktree_map_remove(locktree *lt) {
    uint32_t idx;
    locktree *found_lt;
    int r = m_locktree_map.find_zero<DICTIONARY_ID, find_by_dict_id>(
            lt->m_dict_id, &found_lt, &idx);
    invariant_zero(r);
    invariant(found_lt == lt);
    r = m_locktree_map.delete_at(idx);
    invariant_zero(r);
}

locktree *locktree::manager::get_lt(DICTIONARY_ID dict_id, DESCRIPTOR desc,
        ft_compare_func cmp, void *on_create_extra) {

    // hold the mutex around searching and maybe
    // inserting into the locktree map
    mutex_lock();

    locktree *lt = locktree_map_find(dict_id);
    if (lt == nullptr) {
        XCALLOC(lt);
        lt->create(&m_mem_tracker, dict_id, desc, cmp);
        invariant(lt->m_reference_count == 1);

        // new locktree created - call the on_create callback
        // and put it in the locktree map
        if (m_lt_create_callback) {
            m_lt_create_callback(lt, on_create_extra);
        }

        locktree_map_put(lt);
    } else {
        reference_lt(lt);
    }

    mutex_unlock();

    return lt;
}

void locktree::manager::reference_lt(locktree *lt) {
    // increment using a sync fetch and add.
    // the caller guarantees that the lt won't be
    // destroyed while we increment the count here.
    //
    // the caller can do this by already having an lt
    // reference or by holding the manager mutex.
    //
    // if the manager's mutex is held, it is ok for the
    // reference count to transition from 0 to 1 (no race),
    // since we're serialized with other opens and closes.
    toku_sync_fetch_and_add(&lt->m_reference_count, 1);
}

void locktree::manager::release_lt(locktree *lt) {
    bool do_destroy = false;
    DICTIONARY_ID dict_id = lt->m_dict_id;

    // Release a reference on the locktree. If the count transitions to zero,
    // then we *may* need to do the cleanup.
    //
    // Grab the manager's mutex and look for a locktree with this locktree's
    // dictionary id. Since dictionary id's never get reused, any locktree 
    // found must be the one we just released a reference on.
    //
    // At least two things could have happened since we got the mutex:
    // - Another thread gets a locktree with the same dict_id, increments
    // the reference count. In this case, we shouldn't destroy it.
    // - Another thread gets a locktree with the same dict_id and then
    // releases it quickly, transitioning the reference count from zero to
    // one and back to zero. In this case, only one of us should destroy it.
    // It doesn't matter which. We originally missed this case, see #5776.
    //
    // After 5776, the high level rule for release is described below.
    //
    // If a thread releases a locktree and notices the reference count transition
    // to zero, then that thread must immediately:
    // - assume the locktree object is invalid
    // - grab the manager's mutex
    // - search the locktree map for a locktree with the same dict_id and remove
    // it, if it exists. the destroy may be deferred.
    // - release the manager's mutex
    //
    // This way, if many threads transition the same locktree's reference count
    // from 1 to zero and wait behind the manager's mutex, only one of them will
    // do the actual destroy and the others will happily do nothing.
    uint32_t refs = toku_sync_sub_and_fetch(&lt->m_reference_count, 1);
    if (refs == 0) {
        mutex_lock();
        locktree *find_lt = locktree_map_find(dict_id);
        if (find_lt != nullptr) {
            // A locktree is still in the map with that dict_id, so it must be
            // equal to lt. This is true because dictionary ids are never reused.
            // If the reference count is zero, it's our responsibility to remove
            // it and do the destroy. Otherwise, someone still wants it.
            invariant(find_lt == lt);
            if (lt->m_reference_count == 0) {
                locktree_map_remove(lt);
                do_destroy = true;
            }
        }
        mutex_unlock();
    }

    // if necessary, do the destroy without holding the mutex
    if (do_destroy) {
        if (m_lt_destroy_callback) {
            m_lt_destroy_callback(lt);
        }
        lt->destroy();
        toku_free(lt);
    }
}

// effect: escalate's the locks in each locktree
// requires: manager's mutex is held
void locktree::manager::run_escalation(void) {
    // there are too many row locks in the system and we need to tidy up.
    //
    // a simple implementation of escalation does not attempt
    // to reduce the memory foot print of each txn's range buffer.
    // doing so would require some layering hackery (or a callback)
    // and more complicated locking. for now, just escalate each
    // locktree individually, in-place.
    size_t num_locktrees = m_locktree_map.size();
    for (size_t i = 0; i < num_locktrees; i++) {
        locktree *lt;
        int r = m_locktree_map.fetch(i, &lt);
        invariant_zero(r);
        lt->escalate();
    }
}

void locktree::manager::memory_tracker::set_manager(manager *mgr) {
    m_mgr = mgr;
}

int locktree::manager::memory_tracker::check_current_lock_constraints(void) {
    int r = 0;
    // check if we're out of locks without the mutex first. then, grab the
    // mutex and check again. if we're still out of locks, run escalation.
    // return an error if we're still out of locks after escalation.
    if (out_of_locks()) {
        m_mgr->mutex_lock();
        if (out_of_locks()) {
            m_mgr->run_escalation();
            if (out_of_locks()) {
                r = TOKUDB_OUT_OF_LOCKS;
            }
        }
        m_mgr->mutex_unlock();
    }
    return r;
}

void locktree::manager::memory_tracker::note_mem_used(uint64_t mem_used) {
    (void) toku_sync_fetch_and_add(&m_mgr->m_current_lock_memory, mem_used);
}

void locktree::manager::memory_tracker::note_mem_released(uint64_t mem_released) {
    uint64_t old_mem_used = toku_sync_fetch_and_sub(&m_mgr->m_current_lock_memory, mem_released);
    invariant(old_mem_used >= mem_released);
}

bool locktree::manager::memory_tracker::out_of_locks(void) const {
    return m_mgr->m_current_lock_memory >= m_mgr->m_max_lock_memory;
}

} /* namespace toku */

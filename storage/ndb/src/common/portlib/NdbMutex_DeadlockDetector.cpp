/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NdbMutex.h>
#include <Bitmask.hpp>
#include <cstring>
#include <vector>

#include "my_stacktrace.h"

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR

#include "NdbMutex_DeadlockDetector.h"

/**
 * ndb_mutex_state and ndb_mutex_thr_state represents the lock-graph
 * of locks being held and waited for by each thread.
 */

class nmdd_mask {
 public:
  nmdd_mask() : m_data() {}

  unsigned size() const {  // Size in number of bits
    return m_data.size() * 32;
  }
  void set(unsigned no);
  void clear(unsigned no);
  bool check(unsigned no) const;
  bool equal(const nmdd_mask *mask) const;
  bool overlaps(const nmdd_mask *mask) const;
  unsigned first_zero_bit() const;

 private:
  std::vector<Uint32> m_data;
};

typedef std::vector<ndb_mutex_state *> nmdd_mutex_array;
typedef std::vector<nmdd_mask> nmdd_mutex_combinations;

/**
 * mutex_set represent a set of locks being being held.
 * The locks being held is registered both by its mutex-no in 'mask',
 * as well with its 'mutex_state' in a list.
 * - The mask provide a quick way of checking if lock(s) are held.
 * - The list makes it possible to traverse the full set of locks
 *   being in effect.
 */
struct nmdd_mutex_set {
  nmdd_mutex_array m_list;
  nmdd_mask m_mask;

  void add(ndb_mutex_state *);
  void remove(ndb_mutex_state *);
};

struct ndb_mutex_state {
  const unsigned m_no; /* my mutex "id" (for access in masks) */

  /**
   * The locked_before_combinations contains the different set of locks
   * we have seen being held before the lock represented by this mutex_state.
   * This is the structure used to predict possible deadlocks.
   */
  nmdd_mutex_combinations m_locked_before_combinations;

  /**
   * The locked_before / after mutex_sets are required to keep track of
   * which mutex_states referring to a mutex-no.
   * Not used for deadlock prediction, needed in order to cleanup and
   * recycle mutex-no's when NdbMutex's are destructed.
   */
  nmdd_mutex_set m_locked_before; /* Mutexes held when locking this mutex */
  nmdd_mutex_set m_locked_after;  /* Mutexes locked when holding this mutex */

  ndb_mutex_state(unsigned no) : m_no(no) {}
};

struct ndb_mutex_thr_state {
  nmdd_mutex_set m_locked; /* Mutexes held by this thread */

  void add_lock(ndb_mutex_state *m, bool is_blocking);
  void remove(ndb_mutex_state *);
};

static thread_local ndb_mutex_thr_state *NDB_THREAD_TLS_SELF = nullptr;

/* Protects the lock graph structures. */
static native_mutex_t g_serialize_mutex;

/* Global map of used mutex_no's */
static nmdd_mask g_mutex_no_mask;

static unsigned alloc_mutex_no();
static void release_mutex_no(unsigned no);

/* Detected a deadlock, print stack trace and crash */
static void dump_stack(std::string msg) {
  my_safe_printf_stderr("Deadlock detected: %s\n", msg.c_str());
  my_print_stacktrace(nullptr, 0);
  assert(false);
  abort();
}

void NdbMutex_DeadlockDetectorInit() {
  const int result [[maybe_unused]] =
      native_mutex_init(&g_serialize_mutex, nullptr);
  assert(result == 0);
}

void NdbMutex_DeadlockDetectorEnd() {
  native_mutex_destroy(&g_serialize_mutex);
}

static ndb_mutex_thr_state *get_thr() { return NDB_THREAD_TLS_SELF; }

void ndb_mutex_thread_init(struct ndb_mutex_thr_state *&mutex_thr_state) {
  mutex_thr_state = new (std::nothrow) ndb_mutex_thr_state();
  NDB_THREAD_TLS_SELF = mutex_thr_state;
}

void ndb_mutex_thread_exit(struct ndb_mutex_thr_state *&mutex_thr_state) {
  delete mutex_thr_state;
  mutex_thr_state = nullptr;
}

void ndb_mutex_created(ndb_mutex_state *&mutex_state) {
  /**
   * Assign mutex no
   */
  native_mutex_lock(&g_serialize_mutex);
  const unsigned no = alloc_mutex_no();
  native_mutex_unlock(&g_serialize_mutex);

  mutex_state = new (std::nothrow) ndb_mutex_state(no);
}

void ndb_mutex_destroyed(ndb_mutex_state *&mutex_state) {
  native_mutex_lock(&g_serialize_mutex);
  const unsigned no = mutex_state->m_no;

  /**
   * In order to be able to reuse mutex_no,
   *   we need to clear this no from all mutex_state's referring to it
   *   this is all mutexes in after map
   */
  for (ndb_mutex_state *&after : mutex_state->m_locked_after.m_list) {
    after->m_locked_before.remove(mutex_state);
    for (nmdd_mask &mask : after->m_locked_before_combinations) {
      mask.clear(no);
    }
  }
  /**
   * And we need to remove ourselves from after list of mutexes in our before
   * list
   */
  for (ndb_mutex_state *&before : mutex_state->m_locked_before.m_list) {
    before->m_locked_after.remove(mutex_state);
    for (nmdd_mask &mask : before->m_locked_before_combinations) {
      mask.clear(no);
    }
  }

  // Release the mutex 'm_no' for reuse
  release_mutex_no(no);
  delete mutex_state;
  mutex_state = nullptr;
  native_mutex_unlock(&g_serialize_mutex);
}

void ndb_mutex_locked(ndb_mutex_state *mutex_state, bool is_blocking) {
  ndb_mutex_thr_state *thr = get_thr();
  if (thr == nullptr) {
    /**
     * These are threads not started with NdbThread_Create(...)
     *   e.g mysql-server threads...ignore these for now
     */
    return;
  }

  native_mutex_lock(&g_serialize_mutex);

  /**
   * Predict possible deadlocks if different lock order is found.
   */
  if (is_blocking) {
    for (ndb_mutex_state *&other : thr->m_locked.m_list) {
      /**
       * We want to lock mutex having 'mutex_state', check that against
       * all 'other' locks already held by this thread.
       *  1) If any 'other' lock being held has ever seen this lock being
       *     taken before itself in some cases, it as a *candidate* for
       *     creating a deadlock -> need further DD-analysis
       *  2) Check all the lock combinations for this 'other' lock.
       *     It is a combination possibly creating a deadlock if:
       *     2a) The before-lock combination involve 'this' lock  -- AND--
       *     2b) The before-lock combination did not also hold any of the
       *         other locks currently held by this thread. In such case
       *         the common before-locks will prevent that the different
       *         lock combinations can be held at the same time -> no deadlocks.
       */
      if (other->m_locked_before.m_mask.check(mutex_state->m_no)) {  // 1)
        // A candidate for possible deadlock, need further check to conclude
        for (nmdd_mask &mask : other->m_locked_before_combinations) {  // 2)
          if (mask.check(mutex_state->m_no) &&                         // 2a)
              !mask.overlaps(&thr->m_locked.m_mask)) {                 // 2b)
            dump_stack("Predicted deadlock due to different lock order");
          }
        }
      }
    }
  }
  /**
   * Register this mutex_state 'lock'.
   */
  for (ndb_mutex_state *&other : thr->m_locked.m_list) {
    /**
     * We want to lock mutex having 'mutex_state'
     * Check that none of the other mutexes we curreny have locked
     *   have this 'mutex_state' in their *before* list
     */
    /**
     * Add other-mutex to this mutex_state's list and mask of before-locks
     */
    mutex_state->m_locked_before.add(other);
    /**
     * Add this mutex_state to other-mutex's list and mask of after-locks
     */
    other->m_locked_after.add(mutex_state);
  }
  thr->add_lock(mutex_state, is_blocking);
  native_mutex_unlock(&g_serialize_mutex);
}

void ndb_mutex_unlocked(ndb_mutex_state *mutex_state) {
  ndb_mutex_thr_state *thr = get_thr();
  if (thr == nullptr) {
    /**
     * These are threads not started with NdbThread_Create(...)
     *   e.g mysql-server threads...ignore these for now
     */
    return;
  }
  native_mutex_lock(&g_serialize_mutex);
  thr->remove(mutex_state);
  native_mutex_unlock(&g_serialize_mutex);
}

/**
 * util
 */
void nmdd_mask::set(unsigned no) {
  const unsigned need_len = (no / 32) + 1;
  const unsigned old_len = m_data.size();
  if (need_len > old_len) {
    m_data.reserve(need_len);
    for (unsigned i = old_len; i < need_len; i++) {
      m_data.push_back(0);
    }
  }
  BitmaskImpl::set(m_data.size(), m_data.data(), no);
}

void nmdd_mask::clear(unsigned no) {
  if (no >= size()) return;
  BitmaskImpl::clear(m_data.size(), m_data.data(), no);
}

bool nmdd_mask::check(unsigned no) const {
  if (no >= size()) return false;
  return BitmaskImpl::get(m_data.size(), m_data.data(), no);
}

bool nmdd_mask::overlaps(const nmdd_mask *mask) const {
  const unsigned len = std::min(m_data.size(), mask->m_data.size());
  return BitmaskImpl::overlaps(len, m_data.data(), mask->m_data.data());
}

bool nmdd_mask::equal(const nmdd_mask *mask) const {
  const unsigned len = std::min(m_data.size(), mask->m_data.size());
  return BitmaskImpl::equal(len, m_data.data(), mask->m_data.data()) &&
         BitmaskImpl::isclear(m_data.size() - len, m_data.data() + len) &&
         BitmaskImpl::isclear(mask->m_data.size() - len,
                              mask->m_data.data() + len);
}

unsigned nmdd_mask::first_zero_bit() const {
  for (unsigned i = 0; i < m_data.size(); i++) {
    const Uint32 word = m_data[i];
    if (word != 0xffffffff) {
      unsigned bit;
      for (bit = 0; bit < 32; bit++) {
        if ((word & (1 << bit)) == 0) break;
      }
      assert(bit < 32);
      return (32 * i) + bit;
    }
  }
  // First zero_bit is after all existing data[]
  return m_data.size() * 32;
}

void nmdd_mutex_set::add(ndb_mutex_state *mutex_state) {
  const unsigned no = mutex_state->m_no;
  if (!m_mask.check(no)) {
    m_mask.set(no);
    m_list.push_back(mutex_state);
  }
}

void nmdd_mutex_set::remove(ndb_mutex_state *mutex_state) {
  const unsigned no = mutex_state->m_no;
  // Set lock as not being held in this mutex_set' any longer
  assert(m_mask.check(no));
  m_mask.clear(no);

  // Find 'mutex_state' in list and erase it
  for (nmdd_mutex_array::iterator it = m_list.begin(); it != m_list.end();
       it++) {
    if (*it == mutex_state) {
      m_list.erase(it);
      return;
    }
  }
  assert(false);
}

static void add_mutex_combination(nmdd_mutex_combinations *comb,
                                  nmdd_mask *locks) {
  for (nmdd_mask &mask : *comb) {
    if (mask.equal(locks)) {
      // Skip if already existing
      return;
    }
  }
  comb->push_back(*locks);
}

void ndb_mutex_thr_state::add_lock(ndb_mutex_state *m, bool is_blocking) {
  if (is_blocking && m_locked.m_list.size() > 0) {
    // Enlist current locks as a 'blocking' combination
    add_mutex_combination(&m->m_locked_before_combinations, &m_locked.m_mask);
  }
  m_locked.add(m);  // Set lock to be held by this thread
}

void ndb_mutex_thr_state::remove(ndb_mutex_state *mutex_state) {
  m_locked.remove(mutex_state);
}

static unsigned alloc_mutex_no() {
  const unsigned no = g_mutex_no_mask.first_zero_bit();
  g_mutex_no_mask.set(no);
  assert(g_mutex_no_mask.check(no));
  return no;
}

static void release_mutex_no(unsigned no) { g_mutex_no_mask.clear(no); }

#endif

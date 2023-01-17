/*****************************************************************************

Copyright (c) 2015, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0lock_free_hash.h
 Lock free hash implementation

 Created Mar 16, 2015 Vasil Dimov
 *******************************************************/

#ifndef ut0lock_free_hash_h
#define ut0lock_free_hash_h

#define BOOST_ATOMIC_NO_LIB

#include "univ.i"

#include <atomic>
#include <list>

#include "os0numa.h"      /* os_getcpu() */
#include "os0thread.h"    /* ut::this_thread_hash */
#include "ut0cpu_cache.h" /* Cache_aligned<T> */
#include "ut0mutex.h"     /* ib_mutex_t */
#include "ut0new.h"       /* UT_NEW*(), ut::delete_*() */
#include "ut0rnd.h"       /* ut::hash_uint64() */

/** An interface class to a basic hash table, that ut_lock_free_hash_t is. */
class ut_hash_interface_t {
 public:
  /** The value that is returned when the searched for key is not
  found. */
  static const int64_t NOT_FOUND = INT64_MAX;

  /** Destructor. */
  virtual ~ut_hash_interface_t() = default;

  /** Get the value mapped to a given key.
  @param[in]    key     key to look for
  @return the value that corresponds to key or NOT_FOUND. */
  virtual int64_t get(uint64_t key) const = 0;

  /** Set the value for a given key, either inserting a new (key, val)
  tuple or overwriting an existent value.
  @param[in]    key     key whose value to set
  @param[in]    val     value to be set */
  virtual void set(uint64_t key, int64_t val) = 0;

  /** Delete a (key, val) pair from the hash.
  @param[in]    key     key whose pair to delete */
  virtual void del(uint64_t key) = 0;

  /** Increment the value for a given key with 1 or insert a new tuple
  (key, 1).
  @param[in]    key     key whose value to increment or insert as 1 */
  virtual void inc(uint64_t key) = 0;

  /** Decrement the value of a given key with 1 or insert a new tuple
  (key, -1).
  @param[in]    key     key whose value to decrement */
  virtual void dec(uint64_t key) = 0;

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
  /** Print statistics about how many searches have been done on the hash
  and how many collisions. */
  virtual void print_stats() = 0;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
};

/** Lock free ref counter. It uses a few counter variables internally to improve
performance on machines with lots of CPUs.  */
class ut_lock_free_cnt_t {
 public:
  /** Constructor. */
  ut_lock_free_cnt_t() {
    /* The initial value of std::atomic depends on C++ standard and the way
    the containing object was initialized, so make sure it's always zero. */
    for (size_t i = 0; i < m_cnt.size(); i++) {
      m_cnt[i].store(0);
    }
  }

  class handle_t {
   public:
    handle_t() : m_counter{nullptr} {}

    handle_t(std::atomic<uint64_t> *counter) : m_counter{counter} {
      m_counter->fetch_add(1);
    }

    handle_t(handle_t &&other) noexcept : m_counter{other.m_counter} {
      other.m_counter = nullptr;
    }

    explicit operator bool() const noexcept { return m_counter != nullptr; }

    ~handle_t() {
      if (m_counter != nullptr) {
        m_counter->fetch_sub(1);
      }
    }

   private:
    std::atomic<uint64_t> *m_counter;
  };

  /** Increment the counter. */
  handle_t reference() { return handle_t{&m_cnt[n_cnt_index()]}; }

  /** Wait until all previously existing references get released.
  This function assumes that the caller ensured that no new references
  should appear (or rather: no long-lived references - there can be treads which
  call reference(), realize the object should no longer be referenced and
  immediately release it)
  */
  void await_release_of_old_references() const {
    for (size_t i = 0; i < m_cnt.size(); i++) {
      while (m_cnt[i].load()) {
        std::this_thread::yield();
      }
    }
  }

 private:
  /** Derive an appropriate index in m_cnt[] for the current thread.
  @return index in m_cnt[] for this thread to use */
  size_t n_cnt_index() const {
    size_t cpu;

#ifdef HAVE_OS_GETCPU
    cpu = static_cast<size_t>(os_getcpu());
#else  /* HAVE_OS_GETCPU */
    cpu = ut::this_thread_hash;
#endif /* HAVE_OS_GETCPU */

    return cpu % m_cnt.size();
  }

  /** The shards of the counter.
  We've just picked up some number that is supposedly larger than the number of
  CPUs on the system or close to it, but small enough that
  await_release_of_old_references() finishes in reasonable time, and that the
  size (256 * 64B = 16 KiB) is not too large.
  We pad the atomics to avoid false sharing. In particular, we hope that on
  platforms which HAVE_OS_GETCPU the same CPU will always fetch the same counter
  and thus will store it in its local cache. This should also help on NUMA
  architectures by avoiding the cost of synchronizing caches between CPUs.*/
  std::array<ut::Cacheline_aligned<std::atomic<uint64_t>>, 256> m_cnt;
};

/** A node in a linked list of arrays. The pointer to the next node is
atomically set (CAS) when a next element is allocated. */
template <typename T>
class ut_lock_free_list_node_t {
 public:
  typedef ut_lock_free_list_node_t<T> *next_t;

  /** Constructor.
  @param[in]    n_elements      number of elements to create */
  explicit ut_lock_free_list_node_t(size_t n_elements)
      : m_base{ut::make_unique<T[]>(
            ut::make_psi_memory_key(mem_key_ut_lock_free_hash_t), n_elements)},
        m_n_base_elements{n_elements},
        m_pending_free{false},
        m_next{nullptr} {
    ut_ad(n_elements > 0);
  }

  static ut_lock_free_list_node_t *alloc(size_t n_elements) {
    return ut::aligned_new_withkey<ut_lock_free_list_node_t<T>>(
        ut::make_psi_memory_key(mem_key_ut_lock_free_hash_t),
        alignof(ut_lock_free_list_node_t<T>), n_elements);
  }

  static void dealloc(ut_lock_free_list_node_t *ptr) {
    ut::aligned_delete(ptr);
  }

  /** Create and append a new array to this one and store a pointer
  to it in 'm_next'. This is done in a way that multiple threads can
  attempt this at the same time and only one will succeed. When this
  method returns, the caller can be sure that the job is done (either
  by this or another thread).
  @param[in]    deleted_val     the constant that designates that
  a value is deleted
  @param[out]   grown_by_this_thread    set to true if the next
  array was created and appended by this thread; set to false if
  created and appended by another thread.
  @return the next array, appended by this or another thread */
  next_t grow(int64_t deleted_val, bool *grown_by_this_thread) {
    size_t new_size;

    if (m_n_base_elements > 1024 &&
        n_deleted(deleted_val) > m_n_base_elements * 3 / 4) {
      /* If there are too many deleted elements (more than
      75%), then do not double the size. */
      new_size = m_n_base_elements;
    } else {
      new_size = m_n_base_elements * 2;
    }

    next_t new_arr = alloc(new_size);

    /* Publish the allocated entry. If somebody did this in the
    meantime then just discard the allocated entry and do
    nothing. */
    next_t expected = nullptr;
    if (!m_next.compare_exchange_strong(expected, new_arr)) {
      /* Somebody just did that. */
      dealloc(new_arr);

      /* 'expected' has the current value which
      must be != NULL because the CAS failed. */
      ut_ad(expected != nullptr);

      *grown_by_this_thread = false;

      return (expected);
    } else {
      *grown_by_this_thread = true;
      return (new_arr);
    }
  }

  /* This object is only ever destroyed after it is removed from the
  list of arrays in the hash table (which means that new threads cannot
  start using it) and the number of threads that use it has decreased
  to zero. */

  /** Mark the beginning of an access to this object. Used to prevent a
  destruction of an array pointed by m_base while our thread is accessing it.
  @return A handle which protects the m_base as long as the handle is not
  destructed. If the handle is {} (==false), the access was denied, this object
  is to be removed from the list and thus new access to it is not allowed.
  The caller should retry from the head of the list. */
  ut_lock_free_cnt_t::handle_t begin_access() {
    auto handle = m_n_ref.reference();

    if (m_pending_free.load()) {
      /* Don't allow access if freeing is pending. Ie if
      another thread is waiting for readers to go away
      before it can free the m_base's member of this
      object. */
      return {};
    }

    return handle;
  }

  /** Wait until all previously held references are released */
  void await_release_of_old_references() {
    m_n_ref.await_release_of_old_references();
  }

  /** Base array. */
  ut::unique_ptr<T[]> m_base;

  /** Number of elements in 'm_base'. */
  size_t m_n_base_elements;

  /** Indicate whether some thread is waiting for readers to go away
  before it can free the memory occupied by the m_base member. */
  std::atomic_bool m_pending_free;

  /** Pointer to the next node if any or NULL. This begins its life
  as NULL and is only changed once to some real value. Never changed
  to another value after that. */
  std::atomic<next_t> m_next;

 private:
  /** Count the number of deleted elements. The value returned could
  be inaccurate because it is obtained without any locks.
  @param[in]    deleted_val     the constant that designates that
  a value is deleted
  @return the number of deleted elements */
  size_t n_deleted(int64_t deleted_val) const {
    size_t ret = 0;

    for (size_t i = 0; i < m_n_base_elements; i++) {
      const int64_t val = m_base[i].m_val.load(std::memory_order_relaxed);

      if (val == deleted_val) {
        ret++;
      }
    }

    return (ret);
  }

  /** Counter for the current number of readers and writers to this
  object. This object is destroyed only after it is removed from the
  list, so that no new readers or writers may arrive, and after this
  counter has dropped to zero. */
  ut_lock_free_cnt_t m_n_ref;
};

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of integer type.
* The possible keys are:
  * UNUSED: a (key = UNUSED, val) tuple means that it is empty/unused. This is
    the initial state of all keys.
  * AVOID: a (key = AVOID, val = any) tuple means that this tuple is disabled,
    does not contain a real data and should be avoided by searches and
    inserts. This is used when migrating all elements of an array to the
    next array.
  * real key: anything other than the above means this is a real key,
    specified by the user.

* The possible values are:
  * NOT_FOUND: a (key, val = NOT_FOUND) tuple means that it is just being
    inserted and returning "not found" is ok. This is the initial state of all
    values.
  * DELETED: a (key, val = DELETED) tuple means that a tuple with this key
    existed before but was deleted at some point. Searches for this key return
    "not found" and inserts reuse the tuple, replacing the DELETED value with
    something else.
  * GOTO_NEXT_ARRAY: a (key, val = GOTO_NEXT_ARRAY) tuple means that the
    searches for this tuple (get and insert) should go to the next array and
    repeat the search there. Used when migrating all tuples from one array to
    the next.
  * real value: anything other than the above means this is a real value,
    specified by the user.

* Transitions for keys (a real key is anything other than UNUSED and AVOID):
  * UNUSED -> real key -- allowed
  * UNUSED -> AVOID -- allowed
  anything else is not allowed:
  * real key -> UNUSED -- not allowed
  * real key -> AVOID -- not allowed
  * real key -> another real key -- not allowed
  * AVOID -> UNUSED -- not allowed
  * AVOID -> real key -- not allowed

* Transitions for values (a real value is anything other than NOT_FOUND,
  DELETED and GOTO_NEXT_ARRAY):
  * NOT_FOUND -> real value -- allowed
  * NOT_FOUND -> DELETED -- allowed
  * real value -> another real value -- allowed
  * real value -> DELETED -- allowed
  * real value -> GOTO_NEXT_ARRAY -- allowed
  * DELETED -> real value -- allowed
  * DELETED -> GOTO_NEXT_ARRAY -- allowed
  anything else is not allowed:
  * NOT_FOUND -> GOTO_NEXT_ARRAY -- not allowed
  * real value -> NOT_FOUND -- not allowed
  * DELETED -> NOT_FOUND -- not allowed
  * GOTO_NEXT_ARRAY -> real value -- not allowed
  * GOTO_NEXT_ARRAY -> NOT_FOUND -- not allowed
  * GOTO_NEXT_ARRAY -> DELETED -- not allowed
*/
class ut_lock_free_hash_t : public ut_hash_interface_t {
 public:
  /** Constructor. Not thread safe.
  @param[in]    initial_size    number of elements to allocate
  initially. Must be a power of 2, greater than 0.
  @param[in]    del_when_zero   if true then automatically delete a
  tuple from the hash if due to increment or decrement its value becomes
  zero. */
  explicit ut_lock_free_hash_t(size_t initial_size, bool del_when_zero)
      : m_del_when_zero(del_when_zero)
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
        ,
        m_n_search(0),
        m_n_search_iterations(0)
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
  {
    ut_a(initial_size > 0);
    ut_a(ut_is_2pow(initial_size));

    m_data.store(arr_node_t::alloc(initial_size));

    mutex_create(LATCH_ID_LOCK_FREE_HASH, &m_optimize_latch);

    m_hollow_objects = ut::new_withkey<hollow_t>(
        ut::make_psi_memory_key(mem_key_ut_lock_free_hash_t),
        hollow_alloc_t(mem_key_ut_lock_free_hash_t));
  }

  /** Destructor. Not thread safe. */
  ~ut_lock_free_hash_t() override {
    mutex_destroy(&m_optimize_latch);

    arr_node_t *arr = m_data.load();

    do {
      arr_node_t *next = arr->m_next.load();

      arr_node_t::dealloc(arr);

      arr = next;
    } while (arr != nullptr);

    while (!m_hollow_objects->empty()) {
      arr_node_t::dealloc(m_hollow_objects->front());
      m_hollow_objects->pop_front();
    }
    ut::delete_(m_hollow_objects);
  }

  /** Get the value mapped to a given key.
  @param[in]    key     key to look for
  @return the value that corresponds to key or NOT_FOUND. */
  int64_t get(uint64_t key) const override {
    ut_ad(key != UNUSED);
    ut_ad(key != AVOID);

    arr_node_t *arr = m_data.load();

    for (;;) {
      auto handle_and_tuple{get_tuple(key, &arr)};
      const auto tuple = handle_and_tuple.second;

      if (tuple == nullptr) {
        return (NOT_FOUND);
      }

      /* Here if another thread is just setting this key
      for the first time, then the tuple could be
      (key, NOT_FOUND) (remember all vals are initialized
      to NOT_FOUND initially) in which case we will return
      NOT_FOUND below which is fine. */

      int64_t v = tuple->m_val.load(std::memory_order_relaxed);

      if (v == DELETED) {
        return (NOT_FOUND);
      } else if (v != GOTO_NEXT_ARRAY) {
        return (v);
      }

      /* Prevent reorder of the below m_next.load() with
      the above m_val.load().
      We want to be sure that if m_val is GOTO_NEXT_ARRAY,
      then the next array exists. */

      arr = arr->m_next.load();
    }
  }

  /** Set the value for a given key, either inserting a new (key, val)
  tuple or overwriting an existent value. If two threads call this
  method at the same time with the key, but different val, then when
  both methods have finished executing the value will be one of the
  two ones, but undeterministic which one. E.g.
  Thread 1: set(key, val_a)
  Thread 2: set(key, val_b)
  when both have finished, then a tuple with the given key will be
  present with value either val_a or val_b.
  @param[in]    key     key whose value to set
  @param[in]    val     value to be set */
  void set(uint64_t key, int64_t val) override {
    ut_ad(key != UNUSED);
    ut_ad(key != AVOID);
    ut_ad(val != NOT_FOUND);
    ut_ad(val != DELETED);
    ut_ad(val != GOTO_NEXT_ARRAY);

    insert_or_update(key, val, false, m_data.load());
  }

  /** Delete a (key, val) pair from the hash.
  If this gets called concurrently with get(), inc(), dec() or set(),
  then to the caller it will look like the calls executed in isolation,
  the hash structure itself will not be damaged, but it is undefined in
  what order the calls will be executed. For example:
  Let this tuple exist in the hash: (key == 5, val == 10)
  Thread 1: inc(key == 5)
  Thread 2: del(key == 5)
  [1] If inc() executes first then the tuple will become
  (key == 5, val == 11) and then del() will make it
  (key == 5, val == DELETED), which get()s for key == 5 will return as
  NOT_FOUND.
  [2] If del() executes first then the tuple will become
  (key == 5, val == DELETED) and then inc() will change it to
  (key == 5, value == 1).
  It is undefined which one of [1] or [2] will happen. It is up to the
  caller to accept this behavior or prevent it at a higher level.
  @param[in]    key     key whose pair to delete */
  void del(uint64_t key) override {
    ut_ad(key != UNUSED);
    ut_ad(key != AVOID);

    arr_node_t *arr = m_data.load();

    for (;;) {
      auto handle_and_tuple{get_tuple(key, &arr)};
      const auto tuple = handle_and_tuple.second;

      if (tuple == nullptr) {
        /* Nothing to delete. */
        return;
      }

      int64_t v = tuple->m_val.load(std::memory_order_relaxed);

      for (;;) {
        if (v == GOTO_NEXT_ARRAY) {
          break;
        }

        if (tuple->m_val.compare_exchange_strong(v, DELETED)) {
          return;
        }

        /* CAS stored the most recent value of 'm_val'
        into 'v'. */
      }

      /* Prevent reorder of the below m_next.load() with the
      above m_val.load() or the load from
      m_val.compare_exchange_strong().
      We want to be sure that if m_val is GOTO_NEXT_ARRAY,
      then the next array exists. */

      arr = arr->m_next.load();
    }
  }

  /** Increment the value for a given key with 1 or insert a new tuple
  (key, 1).
  If two threads call this method at the same time with the same key,
  then it is guaranteed that when both calls have finished, the value
  will be incremented with 2.
  If two threads call this method and set() at the same time with the
  same key it is undeterministic whether the value will be what was
  given to set() or what was given to set() + 1. E.g.
  Thread 1: set(key, val)
  Thread 2: inc(key)
  or
  Thread 1: inc(key)
  Thread 2: set(key, val)
  when both have finished the value will be either val or val + 1.
  @param[in]    key     key whose value to increment or insert as 1 */
  void inc(uint64_t key) override {
    ut_ad(key != UNUSED);
    ut_ad(key != AVOID);

    insert_or_update(key, 1, true, m_data.load());
  }

  /** Decrement the value of a given key with 1 or insert a new tuple
  (key, -1).
  With respect to calling this together with set(), inc() or dec() the
  same applies as with inc(), see its comment. The only guarantee is
  that the calls will execute in isolation, but the order in which they
  will execute is undeterministic.
  @param[in]    key     key whose value to decrement */
  void dec(uint64_t key) override {
    ut_ad(key != UNUSED);
    ut_ad(key != AVOID);

    insert_or_update(key, -1, true, m_data.load());
  }

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
  /** Print statistics about how many searches have been done on the hash
  and how many collisions. */
  void print_stats() {
    const ulint n_search = m_n_search;
    const ulint n_search_iterations = m_n_search_iterations;

    ib::info info(ER_IB_MSG_LOCK_FREE_HASH_USAGE_STATS);
    info << "Lock free hash usage stats: number of searches=" << n_search
         << ", number of search iterations=" << n_search_iterations;
    if (n_search != 0) {
      info << "average iterations per search: "
           << (double)n_search_iterations / n_search;
    }
  }
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

 private:
  /** A key == UNUSED designates that this cell in the array is empty. */
  static const uint64_t UNUSED = UINT64_MAX;

  /** A key == AVOID designates an unusable cell. This cell of the array
  has been empty (key == UNUSED), but was then marked as AVOID in order
  to prevent new inserts into it. Searches should treat this like
  UNUSED (ie if they encounter it before the key they are searching for
  then stop the search and declare 'not found'). */
  static const uint64_t AVOID = UNUSED - 1;

  /** A val == DELETED designates that this cell in the array has been
  used in the past, but it was deleted later. Searches should return
  NOT_FOUND when they encounter it. */
  static const int64_t DELETED = NOT_FOUND - 1;

  /** A val == GOTO_NEXT_ARRAY designates that this tuple (key, whatever)
  has been moved to the next array. The search for it should continue
  there. */
  static const int64_t GOTO_NEXT_ARRAY = DELETED - 1;

  /** (key, val) tuple type. */
  struct key_val_t {
    key_val_t() : m_key(UNUSED), m_val(NOT_FOUND) {}

    /** Key. */
    std::atomic<uint64_t> m_key;

    /** Value. */
    std::atomic<int64_t> m_val;
  };

  /** An array node in the hash. The hash table consists of a linked
  list of such nodes. */
  typedef ut_lock_free_list_node_t<key_val_t> arr_node_t;

  /** A hash function used to map a key to its suggested position in the
  array. A linear search to the right is done after this position to find
  the tuple with the given key or find a tuple with key == UNUSED or
  AVOID which means that the key is not present in the array.
  @param[in]    key             key to map into a position
  @param[in]    arr_size        number of elements in the array
  @return a position (index) in the array where the tuple is guessed
  to be */
  size_t guess_position(uint64_t key, size_t arr_size) const {
    /* Implement a better hashing function to map
    [0, UINT64_MAX] -> [0, arr_size - 1] if this one turns
    out to generate too many collisions. */

    /* arr_size is a power of 2. */
    return (static_cast<size_t>(ut::hash_uint64(key) & (arr_size - 1)));
  }

  /** Get the array cell of a key from a given array.
  @param[in]    arr             array to search into
  @param[in]    arr_size        number of elements in the array
  @param[in]    key             search for a tuple with this key
  @return pointer to the array cell or NULL if not found */
  key_val_t *get_tuple_from_array(key_val_t *arr, size_t arr_size,
                                  uint64_t key) const {
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
    ++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

    const size_t start = guess_position(key, arr_size);
    const size_t end = start + arr_size;

    for (size_t i = start; i < end; i++) {
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
      ++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

      /* arr_size is a power of 2. */
      const size_t cur_pos = i & (arr_size - 1);

      key_val_t *cur_tuple = &arr[cur_pos];

      const uint64_t cur_key = cur_tuple->m_key.load(std::memory_order_relaxed);

      if (cur_key == key) {
        return (cur_tuple);
      } else if (cur_key == UNUSED || cur_key == AVOID) {
        return (nullptr);
      }
    }

    return (nullptr);
  }

  /** Get the array cell of a key.
  @param[in]    key     key to search for
  @param[in,out]        arr     start the search from this array; when this
  method ends, *arr will point to the array in which the search
  ended (in which the returned key_val resides)
  @return If key was found: handle to the (updated) arr which contains the tuple
  and pointer to the array cell with the tuple. Otherwise an empty handle, and
  nullptr. */
  std::pair<ut_lock_free_cnt_t::handle_t, key_val_t *> get_tuple(
      uint64_t key, arr_node_t **arr) const {
    for (;;) {
      auto handle = (*arr)->begin_access();
      if (!handle) {
        /* The array has been garbaged, restart the search from the beginning.*/
        *arr = m_data.load();
        continue;
      }

      key_val_t *t = get_tuple_from_array((*arr)->m_base.get(),
                                          (*arr)->m_n_base_elements, key);

      if (t != nullptr) {
        return {std::move(handle), t};
      }

      *arr = (*arr)->m_next.load();

      if (*arr == nullptr) {
        return {};
      }
    }
  }

  /** Insert the given key into a given array or return its cell if
  already present.
  @param[in]    arr             array into which to search and insert
  @param[in]    arr_size        number of elements in the array
  @param[in]    key             key to insert or whose cell to retrieve
  @return a pointer to the inserted or previously existent tuple or NULL
  if a tuple with this key is not present in the array and the array is
  full, without any unused cells and thus insertion cannot be done into
  it. */
  key_val_t *insert_or_get_position_in_array(key_val_t *arr, size_t arr_size,
                                             uint64_t key) {
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
    ++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

    const size_t start = guess_position(key, arr_size);
    const size_t end = start + arr_size;

    for (size_t i = start; i < end; i++) {
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
      ++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

      /* arr_size is a power of 2. */
      const size_t cur_pos = i & (arr_size - 1);

      key_val_t *cur_tuple = &arr[cur_pos];

      const uint64_t cur_key = cur_tuple->m_key.load(std::memory_order_relaxed);

      if (cur_key == key) {
        return (cur_tuple);
      }

      if (cur_key == UNUSED) {
        uint64_t expected = UNUSED;
        if (cur_tuple->m_key.compare_exchange_strong(expected, key)) {
          return (cur_tuple);
        }

        /* CAS failed, which means that some other
        thread just changed the current key from UNUSED
        to something else (which is stored in
        'expected'). See if the new value is 'key'. */
        if (expected == key) {
          return (cur_tuple);
        }

        /* The current key, which was UNUSED, has been
        replaced with something else (!= key) by a
        concurrently executing insert. Keep searching
        for a free slot. */
      }

      /* Skip through tuples with key == AVOID. */
    }

    return (nullptr);
  }

  /** Copy all used elements from one array to another. Flag the ones
  in the old array as 'go to the next array'.
  @param[in,out]        src_arr array to copy from
  @param[in,out]        dst_arr array to copy to */
  void copy_to_another_array(arr_node_t *src_arr, arr_node_t *dst_arr) {
    for (size_t i = 0; i < src_arr->m_n_base_elements; i++) {
      key_val_t *t = &src_arr->m_base[i];

      uint64_t k = t->m_key.load(std::memory_order_relaxed);

      /* Prevent further inserts into empty cells. */
      if (k == UNUSED && t->m_key.compare_exchange_strong(k, AVOID)) {
        continue;
      }

      int64_t v = t->m_val.load(std::memory_order_relaxed);

      /* Insert (k, v) into the destination array. We know
      that nobody else will try this concurrently with this
      thread because:
      * this code is being executed by just one thread (the
        thread that managed to grow the list of arrays) and
      * other normal inserts/updates with (key == k) will
        pick the entry in src_arr. */

      /* If we copied the tuple to dst_arr once then we must
      keep trying to transfer its most recent value to
      dst_arr, even if its value becomes DELETED. Otherwise
      a delete operation on this tuple which executes
      concurrently with the loop below may be lost. */
      bool copied = false;

      for (;;) {
        if (v != DELETED || copied) {
          insert_or_update(k, v, false, dst_arr, false);
          copied = true;
        }

        /* Prevent any preceding memory operations (the
        stores from insert_or_update() in particular)
        to be reordered past the store from
        m_val.compare_exchange_strong() below. We want
        to be sure that if m_val is GOTO_NEXT_ARRAY,
        then the entry is indeed present in some of the
        next arrays (ie that insert_or_update() has
        completed and that its effects are visible to
        other threads). */

        /* Now that we know (k, v) is present in some
        of the next arrays, try to CAS the tuple
        (k, v) to (k, GOTO_NEXT_ARRAY) in the current
        array. */

        if (t->m_val.compare_exchange_strong(v, GOTO_NEXT_ARRAY)) {
          break;
        }

        /* If CAS fails, this means that m_val has been
        changed in the meantime and the CAS will store
        m_val's most recent value in 'v'. Retry both
        operations (this time the insert_or_update()
        call will be an update, rather than an
        insert). */
      }
    }
  }

  /** Update the value of a given tuple.
  @param[in,out]        t               tuple whose value to update
  @param[in]    val_to_set      value to set or delta to apply
  @param[in]    is_delta        if true then set the new value to
  old + val, otherwise just set to val
  @retval               true            update succeeded
  @retval               false           update failed due to GOTO_NEXT_ARRAY
  @return whether the update succeeded or not */
  bool update_tuple(key_val_t *t, int64_t val_to_set, bool is_delta) {
    int64_t cur_val = t->m_val.load(std::memory_order_relaxed);

    for (;;) {
      if (cur_val == GOTO_NEXT_ARRAY) {
        return (false);
      }

      int64_t new_val;

      if (is_delta && cur_val != NOT_FOUND && cur_val != DELETED) {
        if (m_del_when_zero && cur_val + val_to_set == 0) {
          new_val = DELETED;
        } else {
          new_val = cur_val + val_to_set;
        }
      } else {
        new_val = val_to_set;
      }

      if (t->m_val.compare_exchange_strong(cur_val, new_val)) {
        return (true);
      }

      /* When CAS fails it sets the most recent value of
      m_val into cur_val. */
    }
  }

  /** Optimize the hash table. Called after a new array is appended to
  the list of arrays (grow). Having more than one array in the list
  of arrays works, but is suboptimal because searches
  (for get/insert/update) have to search in more than one array. This
  method starts from the head of the list and migrates all tuples from
  this array to the next arrays. Then it removes the array from the
  head of the list, waits for readers to go away and frees it's m_base
  member. */
  void optimize() {
    mutex_enter(&m_optimize_latch);

    for (;;) {
      arr_node_t *arr = m_data.load();

      arr_node_t *next = arr->m_next.load();

      if (next == nullptr) {
        break;
      }

      /* begin_access() (ref counting) for 'arr' and 'next'
      around copy_to_another_array() is not needed here
      because the only code that frees memory is below,
      serialized with a mutex. */

      copy_to_another_array(arr, next);

      arr->m_pending_free.store(true);

      arr_node_t *expected = arr;

      /* Detach 'arr' from the list. Ie move the head of the
      list 'm_data' from 'arr' to 'arr->m_next'. */
      ut_a(m_data.compare_exchange_strong(expected, next));

      /* Spin/wait for all threads to stop looking at
      this array. If at some point this turns out to be
      sub-optimal (ie too long busy wait), then 'arr' could
      be added to some lazy deletion list
      arrays-awaiting-destruction-once-no-readers. */
      arr->await_release_of_old_references();

      arr->m_base.reset();

      m_hollow_objects->push_back(arr);
    }

    mutex_exit(&m_optimize_latch);
  }

  /** Insert a new tuple or update an existent one. If a tuple with this
  key does not exist then a new one is inserted (key, val) and is_delta
  is ignored. If a tuple with this key exists and is_delta is true, then
  the current value is changed to be current value + val, otherwise it
  is overwritten to be val.
  @param[in]    key                     key to insert or whose value
  to update
  @param[in]    val                     value to set; if the tuple
  does not exist or if is_delta is false, then the new value is set
  to val, otherwise it is set to old + val
  @param[in]    is_delta                if true then set the new
  value to old + val, otherwise just set to val.
  @param[in]    arr                     array to start the search from
  @param[in]    optimize_allowed        if true then call optimize()
  after an eventual grow(), if false, then never call optimize(). Used
  to prevent recursive optimize() call by insert_or_update() ->
  optimize() -> copy_to_another_array() -> insert_or_update() ->
  optimize(). */
  void insert_or_update(uint64_t key, int64_t val, bool is_delta,
                        arr_node_t *arr, bool optimize_allowed = true) {
    bool call_optimize = false;

    /* Loop through the arrays until we find a free slot to insert
    or until we find a tuple with the specified key and manage to
    update it. */
    for (;;) {
      auto handle = arr->begin_access();
      if (!handle) {
        /* The array has been garbaged, try the next one. */
        arr = arr->m_next.load();
        continue;
      }

      key_val_t *t = insert_or_get_position_in_array(
          arr->m_base.get(), arr->m_n_base_elements, key);

      /* t == NULL means that the array is full, must expand
      and go to the next array. */

      /* update_tuple() returning false means that the value
      of the tuple is GOTO_NEXT_ARRAY, so we must go to the
      next array. */

      if (t != nullptr && update_tuple(t, val, is_delta)) {
        break;
      }

      arr_node_t *next = arr->m_next.load();

      if (next != nullptr) {
        arr = next;
        /* Prevent any subsequent memory operations
        (the reads from the next array in particular)
        to be reordered before the m_next.load()
        above. */
        continue;
      }

      bool grown_by_this_thread;

      arr = arr->grow(DELETED, &grown_by_this_thread);

      if (grown_by_this_thread) {
        call_optimize = true;
      }
    }

    if (optimize_allowed && call_optimize) {
      optimize();
    }
  }

  /** Storage for the (key, val) tuples. */
  std::atomic<arr_node_t *> m_data;

  typedef ut::allocator<arr_node_t *> hollow_alloc_t;
  typedef std::list<arr_node_t *, hollow_alloc_t> hollow_t;

  /** Container for hollow (semi-destroyed) objects that have been
  removed from the list that starts at m_data. Those objects have their
  m_base member freed and are entirely destroyed at the end of the hash
  table life time. The access to this member is protected by
  m_optimize_latch (adding of new elements) and the removal of elements
  is done in the destructor of the hash table. */
  hollow_t *m_hollow_objects;

  /** Concurrent copy-all-elements-to-the-next-array, removal of the
  head of the list and freeing of its m_base member are serialized with
  this latch. Those operations could be implemented without serialization,
  but this immensely increases the complexity of the code. Growing of the
  hash table is not too hot operation and thus we chose simplicity and
  maintainability instead of top performance in this case. "Get"
  operations and "insert/update" ones that do not cause grow still
  run concurrently even if this latch is locked. */
  ib_mutex_t m_optimize_latch;

  /** True if a tuple should be automatically deleted from the hash
  if its value becomes 0 after an increment or decrement. */
  bool m_del_when_zero;

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
  /* The atomic type gives correct results, but has a _huge_
  performance impact. The unprotected operation gives a significant
  skew, but has almost no performance impact. */

  /** Number of searches performed in this hash. */
  mutable std::atomic<uint64_t> m_n_search;

  /** Number of elements processed for all searches. */
  mutable std::atomic<uint64_t> m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
};

#endif /* ut0lock_free_hash_h */

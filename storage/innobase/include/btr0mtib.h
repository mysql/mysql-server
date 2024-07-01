/*****************************************************************************

Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/btr0mtib.h

 Multi Threaded Index Build (MTIB) using BUF_BLOCK_MEMORY and dedicated
 Bulk_flusher threads.

 Created 09/Feb/2023 Annamalai Gurusami
 *************************************************************************/

#ifndef btr0mtib_h
#define btr0mtib_h

#include <stddef.h>
#include <vector>

#include "btr0load.h"
#include "ddl0impl-compare.h"
#include "dict0dict.h"
#include "lob0bulk.h"
#include "lob0lob.h"
#include "page0cur.h"
#include "ut0class_life_cycle.h"
#include "ut0new.h"
#include "ut0object_cache.h"

/* The Btree_multi namespace is used for multi-threaded parallel index build. */
namespace Btree_multi {

// Forward declaration.
class Page_load;
class Btree_load;
struct Page_stat;

using Blob_context = void *;

namespace bulk {
class Blob_inserter;
}  // namespace bulk

/** Allocate, use, manage and flush one extent pages (FSP_EXTENT_SIZE). */
struct Page_extent {
  using Page_range_t = std::pair<page_no_t, page_no_t>;

  /** Constructor.
  @param[in]  btree_load  B-tree loader object.
  @param[in]  is_leaf  true if this is part of leaf segment, false if this is
   part of non-leaf (or top) segment. */
  Page_extent(Btree_load *btree_load, const bool is_leaf);

  /** Destructor. */
  ~Page_extent();

  /** Next page number to be used. */
  page_no_t m_page_no{FIL_NULL};

  /** Page numbers of the pages that has been allocated in this extent.
  The page range is [p1, p2), where p2 is not included. */
  Page_range_t m_range{FIL_NULL, FIL_NULL};

  /** All the page loaders of the used pages. */
  std::vector<Page_load *> m_page_loads;

  bool is_btree_load_nullptr() const { return m_btree_load == nullptr; }

 public:
  /** Create an object of type Page_extent in the heap. */
  static Page_extent *create(Btree_load *btree_load, const bool is_leaf,
                             const bool is_blob);

  /** Release the page extent. Delete if not cached.
  @param[in] extent extent to release */
  static void drop(Page_extent *extent);

  /** Number of pages in this extent. */
  page_no_t page_count() const;

  /** Reset the range with the given value.
  @param[in]  range  new range value to be used. */
  void reset_range(const Page_range_t &range);

  /** Calculate the number of used pages.
  return the number of used pages. */
  size_t used_pages() const { return m_page_no - m_range.first; }

  void get_page_numbers(std::vector<page_no_t> &page_numbers) const;

  /** Get the index of the first unused page load.
  @return index of the first unused page load. */
  size_t last() const { return m_page_no - m_range.first; }

  /** Check if the range is valid.
  @return true if the range is valid, false otherwise. */
  bool is_valid() const;

  bool is_null() const {
    return (m_range.first == FIL_NULL) && (m_range.second == FIL_NULL);
  }

 public:
  /** Member of Page_extent. The index of page_load objects in the m_page_loads
  corresponds to the page_no in the m_range.  Here, check if a page_no already
  has a Page_load object.
  @param[in]  page_no  page_no for which we are looking for Page_load obj.
  @return Page_load object if available, nullptr otherwise. */
  Page_load *get_page_load(page_no_t page_no);

  /** Member of Page_extent. Associate the given page_no and the page load
  object.
  @param[in]  page_no  page number to associate.
  @param[in]  page_load  page load object to associate. */
  void set_page_load(page_no_t page_no, Page_load *page_load);

  Page_range_t pages_to_free() const;

  /** Initialize the next page number to be allocated. The page range should
  have been already initialized. */
  void init();

  /** Check if no more pages are there to be used.
  @return true if the page extent is completely used.
  @return false if the page extent has more pages to be used. */
  bool is_fully_used() const { return m_page_no == m_range.second; }

  /** Check if there are any pages used.
  @return true if at least one page is used.
  @return false if no pages are used in this extent.*/
  bool is_any_used() const {
    ut_ad(m_page_no == m_range.first || m_page_loads.size() > 0);
    return m_page_no > m_range.first;
  }

 public:
  /** Allocate a page number. */
  page_no_t alloc();

  /** Save a page_load. */
  void append(Page_load *page_load);

  /** Flush the used pages to disk. It also frees the unused pages back to the
  segment.
  @param[in,out] node space file node
  @param[in,out] iov vector IO array
  @param[in] iov_size vector IO array size
  @return On success, return DB_SUCCESS. */
  dberr_t flush(fil_node_t *node, void *iov, size_t iov_size);

  /** Flush one page at a time.  This can be used when scatter/gather i/o is
  not available for use.
  @param[in,out] node space file node
  @return On success, return DB_SUCCESS. */
  dberr_t flush_one_by_one(fil_node_t *node);

  /** Flush 1 extent pages at a time. Internally it will call OS dependent
  API (either bulk_flush_win() on Windows or bulk_flush_linux() on other
  operating systems.
  @param[in,out] node space file node
  @param[in,out] iov vector IO array
  @param[in] iov_size vector IO array size
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t bulk_flush(fil_node_t *node, void *iov [[maybe_unused]],
                     size_t iov_size [[maybe_unused]]);

#ifdef UNIV_LINUX
  /** Flush 1 extent pages at a time. Uses pwritev() i/o API.
  @param[in,out] node space file node
  @param[in,out] iov vector IO array
  @param[in] iov_size vector IO array size
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t bulk_flush_linux(fil_node_t *node, struct iovec *iov,
                           size_t iov_size);
#endif /* UNIV_LINUX */

  /** Free all resources. */
  dberr_t destroy();

  /** Free any cached page load entries. */
  void destroy_cached();

  space_id_t space() const;

  /** Mark the extent as cached. Flush thread should not free this extent. */
  void set_cached() { m_is_cached.store(true); }

  /** Set and unset free state of a cached extent.
  @param[in] free state to be set */
  void set_state(bool free) { m_is_free.store(free); }

  /** @return true iff the cached element is in free state. */
  bool is_free() const { return m_is_free.load(); }

  /** @return true iff it is a cached extent. */
  bool is_cached() const { return m_is_cached.load(); }

  /** Reset page load cache to free all. */
  void reset_cached_page_loads() { m_next_cached_page_load_index = 0; }

 public:
  std::ostream &print(std::ostream &out) const;

  /** Mark that this extent is used for blobs. */
  void set_blob() { m_is_blob = true; }

  /** Check if this is a blob extent.
  @return true if it is a blob extent. */
  bool is_blob() const { return m_is_blob; }

  /** Free the BUF_BLOCK_MEMORY blocks used by this extent. */
  void free_memory_blocks();

#ifdef UNIV_DEBUG
  /** True if this extent has been handed over to the bulk flusher. */
  std::atomic_bool m_is_owned_by_bulk_flusher{false};
#endif /* UNIV_DEBUG */

 private:
  Btree_load *m_btree_load{nullptr};

  /** true if this extent belongs to leaf segment. */
  bool m_is_leaf{true};

  /** true iff the the extent is cached. */
  std::atomic_bool m_is_cached{false};
  /** true if the cached entry is free to be used. */
  std::atomic_bool m_is_free{true};
  /** Cached page loads. */
  std::vector<Page_load *> m_cached_page_loads;
  /** Next cached page load index. */
  size_t m_next_cached_page_load_index{0};

  /** True if this extent is used for blobs. */
  bool m_is_blob{false};

  friend struct Level_ctx;
};

inline void Page_extent::get_page_numbers(
    std::vector<page_no_t> &page_numbers) const {
  for (page_no_t i = m_range.first; i < m_page_no; ++i) {
    page_numbers.push_back(i);
  }
}

inline void Page_extent::set_page_load(page_no_t page_no,
                                       Page_load *page_load) {
  ut_ad(page_no >= m_range.first);
  ut_ad(page_no < m_range.second);
  const size_t idx = page_no - m_range.first;
  if (idx == m_page_loads.size()) {
    m_page_loads.push_back(page_load);
  } else {
    ut_ad(idx <= m_page_loads.size());
    ut_ad(m_page_loads[idx] == nullptr);
    m_page_loads[idx] = page_load;
  }
  ut_ad(m_page_loads.size() > 0);
}

inline Page_load *Page_extent::get_page_load(page_no_t page_no) {
  ut_ad(page_no >= m_range.first);
  ut_ad(page_no < m_range.second);
  const size_t idx = page_no - m_range.first;
  if (m_page_loads.empty() || m_page_loads.size() <= idx) {
    return nullptr;
  }
  return m_page_loads[idx];
}

inline Page_extent::~Page_extent() {
  ut_ad(!m_is_owned_by_bulk_flusher.load());
  m_page_no = FIL_NULL;
  m_range.first = FIL_NULL;
  m_range.second = FIL_NULL;
  m_btree_load = nullptr;
}

inline bool Page_extent::is_valid() const {
  ut_ad(m_range.first != 0);
  ut_ad(m_range.second != 0);
  if (is_null()) {
    return true;
  }
  ut_ad(m_range.first < m_range.second);
  ut_ad((m_range.second - m_range.first) <= FSP_EXTENT_SIZE);
  return m_range.first < m_range.second;
}

inline std::ostream &Page_extent::print(std::ostream &out) const {
  out << "[Page_extent: this=" << (void *)this
      << ", m_range.first=" << m_range.first
      << ", m_range.second=" << m_range.second
      << ", page_loads=" << m_page_loads.size() << "]" << std::endl;
  return out;
}

inline std::ostream &operator<<(std::ostream &out, const Page_extent &obj) {
  return obj.print(out);
}

inline void Page_extent::reset_range(const Page_range_t &range) {
  ut_ad(range.first != 0);
  ut_ad(range.second != 0);
  ut_ad(range.first != FIL_NULL);
  ut_ad(range.second != FIL_NULL);
  m_range = range;
  m_page_no = m_range.first;
}

inline page_no_t Page_extent::alloc() {
  ut_ad(is_valid());
  ut_ad(!m_is_owned_by_bulk_flusher.load());

  if (m_page_no == m_range.second) {
    return FIL_NULL;
  }
  return m_page_no++;
}

inline void Page_extent::init() {
  ut_ad(m_range.first != 0);
  ut_ad(m_range.second != 0);
  ut_ad(m_range.first != FIL_NULL);
  ut_ad(m_range.second != FIL_NULL);
  m_page_no = m_range.first;
  m_page_loads.reserve(page_count());
}

inline page_no_t Page_extent::page_count() const {
  return m_range.second - m_range.first;
}

/** Context information for each level. */
struct Level_ctx {
  /** Static member function construct a Level_ctx object.
  @param[in]  index  dictionary index object.
  @param[in]  level  the B-tree level of this context object.
  @param[in]  btree_load a back pointer to the Btree_load object to which this
   Level_ctx object is a part of.
  @return level context object on success, nullptr on error. */
  static Level_ctx *create(dict_index_t *index, size_t level,
                           Btree_load *btree_load);

  /** Static member function to destroy a Level_ctx object.
  @param[in]  obj  the Level_ctx object to destroy. */
  static void destroy(Level_ctx *obj);

  /** Constructor
  @param[in]  index  dictionary index object.
  @param[in]  level  the B-tree level of this context object.
  @param[in]  btree_load a back pointer to the Btree_load object to which this
   Level_ctx object is a part of.*/
  Level_ctx(dict_index_t *index, size_t level, Btree_load *btree_load)
      : m_index(index),
        m_level(level),
        m_page_load(nullptr),
        m_btree_load(btree_load) {}

  /** Destructor. */
  ~Level_ctx();

  /** Initialize.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t init();

  /** Check if this is leaf level.
  @return true if this is leaf level, false otherwise. */
  bool is_leaf() const { return m_level == 0; }

  Page_load *create_page_load();

  /** Free the current page load. */
  void free_page_load();

  /** Allocate a page number. Subsequently a Page_load will be created with the
  allocated page number.
  @param[out] page_no page number that was allocated.
  @return DB_SUCCESS on success, error code on failure.*/
  dberr_t alloc_page_num(page_no_t &page_no);

  /** Allocate one extent in the relevant file segment. No associated buffer
  blocks are allocated.
  @return DB_SUCCESS on success, error code on failure.*/
  dberr_t alloc_extent();

  /** Allocate private memory buffer (BUF_BLOCK_MEMORY) block for given page
  number. */
  [[nodiscard]] buf_block_t *alloc(const page_no_t new_page_no) noexcept;

  void set_current_page_load(Page_load *sibling);

  Page_load *get_page_load() const;

  trx_id_t get_trx_id() const;

  /** The current extent that is being loaded. */
  Page_extent *m_page_extent{};

  /** Build the extent cache. */
  void build_extent_cache();

  /** Load one extent from extent cache.
  @return true iff successful. */
  bool load_extent_from_cache();

  /** Build page loader cache for current exent. */
  void build_page_cache();

  /** Get a free page loader from cache
  @return page loader or nullptr if not found. */
  Page_load *get_page_load_from_cache();

  /** Pre allocated extents to prevent repeated allocation and free. */
  std::vector<Page_extent *> m_cached_extents;

  /** The page_no of the first page in this level. */
  page_no_t m_first_page{FIL_NULL};

  /** The page_no of the last page in this level. */
  page_no_t m_last_page{FIL_NULL};

  /** The index which is being built. */
  dict_index_t *m_index{};

  /** The B-tree level whose context information is stored in this obj. */
  const size_t m_level{};

  /** The Page_load of the current page being loaded. */
  Page_load *m_page_load{};

  /** A back pointer to conceptually higher level btree load object. */
  Btree_load *m_btree_load;

  /** Number of pages allocated at this level. */
  size_t m_stat_n_pages{};

  /** Number of extents allocated at this level. */
  size_t m_stat_n_extents{};

  /** True if the current extent is full. */
  bool m_extent_full{true};

#ifdef UNIV_DEBUG
  bool is_page_tracked(const page_no_t &page_no) const;
  std::vector<page_no_t> m_pages_allocated;
#endif /* UNIV_DEBUG */
};

inline Page_load *Level_ctx::get_page_load() const { return m_page_load; }

inline void Level_ctx::set_current_page_load(Page_load *sibling) {
  m_page_load = sibling;
}

class Bulk_extent_allocator {
 public:
  enum class Type {
    /** Allocate by Page */
    PAGE,
    /** Allocate by extent. */
    EXTENT
  };

  /** Destructor to ensure thread stop. */
  ~Bulk_extent_allocator() { stop(); }

  /** Check size and set extent allocator size parameters
  @param[in] table Innodb dictionary table object
  @param[in] trx transaction performing bulk load
  @param[in] size total data size to be loaded
  @param[in] num_threads number of concurrent threads
  @param[in] in_pages if true, allocate in pages
  @return tablespace extend size in bytes. */
  uint64_t init(dict_table_t *table, trx_t *trx, size_t size,
                size_t num_threads, bool in_pages);

  /* Start extent allocator thread. */
  void start();

  /** Stop extent allocator thread, if active. */
  void stop();

  /** Allocate a page range - currently ans Extent.
  @param[in] is_leaf true if leaf segment, otherwise non-leaf segment
  @param[in] alloc_page if true, allocate in pages otherwise allocate extent
  @param[out] range page range
  @param[in,out] fn_wait_begin begin callback if wait is needed
  @param[in,out] fn_wait_end end callback if wait is needed
  @return Innodb error code. */
  dberr_t allocate(bool is_leaf, bool alloc_page, Page_range_t &range,
                   std::function<void()> &fn_wait_begin,
                   std::function<void()> &fn_wait_end);

 private:
  /** Upper bound for max ranges. */
  static constexpr size_t S_MAX_RANGES = 2 * 1024;

  /** Maximum size by which the tablespace is extended each time. */
  static constexpr size_t S_BULK_EXTEND_SIZE_MAX = 64;

  struct Extent_cache {
    /** Initialize cache.
    @param[in] max_range maximum number of extents to cache. */
    void init(size_t max_range);

    /** @return true if no available extent to consume. */
    inline bool is_empty() const { return (m_num_allocated == m_num_consumed); }

    /** @return true if cache is full and no more extents can be added. */
    inline bool is_full() const {
      return (m_num_allocated >= m_max_range + m_num_consumed);
    }

    /** Check for number of extents to be allocated and cached.
    @param[out] num_alloc number of extents to allocate
    @param[out] num_free number of free extents
    @return true if succesful. */
    bool check(size_t &num_alloc, size_t &num_free) const;

    /** Get one page range from the cache.
    @param[out] range the allocated page range
    @param[out] alloc_trigger true, if need to trigger allocator
    @return true if extent is successfully returned from cache. */
    bool get_range(Page_range_t &range, bool &alloc_trigger);

    /** Set allocated range(extent) in cache.
    @param[in] index position of the range
    @param[in] range page range to be set */
    void set_range(size_t index, Page_range_t &range);

    /** Cached page ranges already allocated to the segment. */
    std::array<Page_range_t, S_MAX_RANGES> m_ranges;

    /** Maximum number of ranges to pre-allocate. */
    size_t m_max_range = S_MAX_RANGES;

    /** Total number of ranges allocated. */
    std::atomic<size_t> m_num_allocated{0};

    /** Total number of ranges allocated. */
    std::atomic<size_t> m_num_consumed{0};
  };

  /** Extent thread executor.
  @return innodb error code. */
  dberr_t run();

  /** Allocate extents and fill the cache.
  @param[in] is_leaf true if leaf segment, otherwise non-leaf segment
  @param[in] num_extents number of extents to allocate
  @return innodb error code. */
  dberr_t allocate_extents(bool is_leaf, size_t num_extents);

  /** Allocator wait function. */
  void allocator_wait() const;

  /** Check if leaf and non-leaf extent cache needs to be filled.
  @param[out] n_leaf number of leaf extents to allocate
  @param[out] n_non_leaf number of non-leaf extents to allocate
  @param[out] trigger true if consumers should be triggered
  @return true if allocator should stop. */
  bool check(size_t &n_leaf, size_t &n_non_leaf, bool &trigger);

  /** Allocate one extent.
  @param[in] is_leaf true if leaf segment, otherwise non-leaf segment
  @param[in,out] mtr mini tranaction to be used for allocation
  @param[out] range page rannge for the extent
  @return innodb error code. */
  dberr_t allocate_extent(bool is_leaf, mtr_t &mtr, Page_range_t &range);

  /** Allocate one page.
  @param[in] is_leaf true if leaf segment, otherwise non-leaf segment
  @param[out] range page rannge for the page
  @return innodb error code. */
  dberr_t allocate_page(bool is_leaf, Page_range_t &range);

  /** @return true if operation is interrupted. */
  bool is_interrupted();

 private:
  /** Bulk extent allocator. */
  std::thread m_thread;

  /** Number of times consumer(s) had to wait. */
  mutable size_t m_consumer_wait_count{};

  /** Number of times allocator had to wait. */
  mutable size_t m_allocator_wait_count{};

  /** Total consumer wait time in micro seconds. */
  mutable std::chrono::microseconds m_consumer_wait_time;

  /** Total allocator wait time in micro seconds. */
  mutable std::chrono::microseconds m_allocator_wait_time;

  /** Page range type. */
  Type m_type = Type::EXTENT;

  /** Cached leaf extents. */
  Extent_cache m_leaf_extents;

  /** Cached non-leaf extents. */
  Extent_cache m_non_leaf_extents;

  /** This mutex protects the m_queue. */
  mutable std::mutex m_mutex;

  /** Condition variable for allocator thread. */
  mutable std::condition_variable m_allocator_condition;

  /** Condition variable for extent consumer threads. */
  mutable std::condition_variable m_consumer_condition;

  /** Flag to indicate if the bulk allocator thread should stop. */
  bool m_stop{false};

  /** Error code, protected by m_mutex */
  dberr_t m_error = DB_SUCCESS;

  /** Innodb dictionary table object. */
  dict_table_t *m_table{};

  /** Innodb transaction - used for checking interrupt. */
  trx_t *m_trx{};

  /** Number of concurrent consumers. */
  size_t m_concurrency{};
};

class Bulk_flusher {
 public:
  /** Thread main function.
  @return innodb error code. */
  dberr_t run();

  /** Check if work is available for the bulk flusher thread.
  @return true if work is available. */
  bool is_work_available();

  /** Start a new thread to do the flush work.
  @param[in] space_id space for flushing pages to
  @param[in] index loader index
  @param[in] queue_size flusher queue size */
  void start(space_id_t space_id, size_t index, size_t queue_size);

  /** Add a page extent to the bulk flush queue.
  @param[in,out] page_extent extent to be added to the queue
  @param[in,out] fn_wait_begin begin callback if wait is needed
  @param[in,out] fn_wait_end end callback if wait is needed */
  void add(Page_extent *page_extent, std::function<void()> &fn_wait_begin,
           std::function<void()> &fn_wait_end);

  /** Check for flusher error and wake up flusher thread.
  @return Innodb error code. */
  dberr_t check_and_notify() const;

  /** Wait till the bulk flush thread stops. */
  void wait_to_stop();

  /** Get the maximum allowed queue size.
  @return the maximum allowed queue size. */
  size_t get_max_queue_size() const { return m_max_queue_size; }

  /** Destructor. */
  ~Bulk_flusher();

  /** @return true iff error has occurred. */
  bool is_error() const { return m_is_error.load(); }

  /** @return error code */
  dberr_t get_error() const;

  void add_to_free_queue(Page_extent *page_extent);

  Page_extent *get_free_extent();

 private:
  /** Do the actual work of flushing.
  @param[in,out] node space file node
  @param[in,out] iov vector IO array
  @param[in] iov_size vector IO array size */
  void do_work(fil_node_t *node, void *iov, size_t iov_size);

  /** Check if the bulk flush thread should stop working. */
  bool should_i_stop() const { return m_stop.load(); }

  /** When no work is available, put the thread to sleep. */
  void wait();

  /** Print useful information to the server log file while exiting. */
  void info();

  /** This queue is protected by the m_mutex. */
  std::vector<Page_extent *> m_queue;

  /** This mutex protects the m_queue. */
  mutable std::mutex m_mutex;

  /** Condition variable to wait upon. */
  mutable std::condition_variable m_condition;

  /** This queue is protected by the m_free_mutex. It is used to cache the
  Page_extent objects that have been flushed and ready for re-use. */
  std::vector<Page_extent *> m_free_queue;

  /** This mutex protects the m_free_queue. */
  mutable std::mutex m_free_mutex;

  /** Flag to indicate if the bulk flusher thread should stop. If true, the
  bulk flusher thread will stop after emptying the queue.  If false, the
  bulk flusher thread will go to sleep after emptying the queue. */
  std::atomic<bool> m_stop{false};

  /** Set if error is encountered during flush. */
  std::atomic<bool> m_is_error{false};

  /** Error code, protected by m_mutex */
  dberr_t m_error = DB_SUCCESS;

  /** Set error code.
  @param[in] error_code error code to set. It could be DB_SUCCESS.*/
  void set_error(dberr_t error_code);

  /** Private queue (private to the bulk flush thread) containing the extents to
  flush. */
  std::vector<Page_extent *> m_priv_queue;

  /** Bulk flusher thread. */
  std::thread m_flush_thread;

  /** Number of times slept */
  size_t m_n_sleep{};

  /** Total sleep time in micro seconds. */
  std::chrono::microseconds m_wait_time;

  /** The sleep duration in milliseconds. */
  static constexpr std::chrono::milliseconds s_sleep_duration{100};

  /** Maximum queue size, defaults to 4 */
  size_t m_max_queue_size{4};

  /** Number of pages flushed. */
  size_t m_pages_flushed{};

  /** Bulk flusher is specific to a tablespace for now. */
  space_id_t m_space_id{};

  /** Flusher ID. */
  size_t m_id{};

#ifdef UNIV_DEBUG
 public:
  /** Vector of page numbers that are flushed by this Bulk_flusher object. */
  std::vector<page_no_t> m_flushed_page_nos;
#endif /* UNIV_DEBUG */
};

namespace bulk {

class Blob_handle;

/** Used to insert many blobs into InnoDB. */
class Blob_inserter {
 public:
  /** Constructor.
  @param[in]  btree_load  the B-tree into which blobs are inserted. */
  Blob_inserter(Btree_load &btree_load);

  ~Blob_inserter();

  /** Initialize by allocating necessary resources.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t init();

  void finish();

  dberr_t insert_blob(lob::ref_t &ref, const dfield_t *dfield) {
    Blob_context blob_ctx;
    dberr_t err = open_blob(blob_ctx, ref);
    if (err != DB_SUCCESS) {
      return err;
    }
    const byte *data = (const byte *)dfield->data;
    err = write_blob(blob_ctx, ref, data, dfield->len);
    if (err != DB_SUCCESS) {
      return err;
    }
    return close_blob(blob_ctx, ref);
  }

  /** Create a blob.
  @param[out]  blob_ctx  pointer to an opaque object representing a blob.
  @param[out]  ref       blob reference to be placed in the record.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t open_blob(Blob_context &blob_ctx, lob::ref_t &ref);

  /** Write data into the blob.
  @param[in]  blob_ctx  pointer to blob into which data is written.
  @param[out] ref       blob reference to be placed in the record.
  @param[in]  data      buffer containing data to be written
  @param[in]  len       length of the data to be written.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t write_blob(Blob_context blob_ctx, lob::ref_t &ref, const byte *data,
                     size_t len);

  /** Indicate that the blob has been completed, so that resources can be
  removed, and as necessary flushing can be done.
  @param[in]  blob_ctx  pointer to blob which has been completely written.
  @param[out] ref       a blob ref object.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t close_blob(Blob_context blob_ctx, lob::ref_t &ref);

  /** Allocate a LOB first page
  @return a LOB first page. */
  Page_load *alloc_first_page();

  /** Allocate a data page
  @return a LOB data page. */
  Page_load *alloc_data_page();

  /** Allocate a LOB index page.
  @return a LOB index page. */
  Page_load *alloc_index_page();

  /** Get the current transaction id.
  @return the current transaction id. */
  trx_id_t get_trx_id() const;

 private:
  Page_load *alloc_page_from_extent(Page_extent *&m_page_extent);

  Page_extent *alloc_free_extent();

  Btree_load &m_btree_load;

  /** Page extent from which to allocate first pages of blobs.
  @ref lob::bulk::first_page_t. */
  Page_extent *m_page_extent_first{nullptr};

  Page_range_t m_page_range_first;

  /** Page extent from which to allocate data pages of blobs.
  @ref lob::bulk::data_page_t. */
  Page_extent *m_page_extent_data{nullptr};

  /** Page extent from which to allocate index pages of blobs.
  @ref lob::bulk::node_page_t. */
  std::list<Page_extent *> m_index_extents;

  /** The current blob being inserted. */
  Blob_context m_blob{nullptr};

  /** Cache of Page_load objects. */
  ut::Object_cache<Page_load> m_page_load_cache;

  /** Cache of Page_extent objects. */
  ut::Object_cache<Page_extent> m_page_extent_cache;

  /** Only one blob handle per sub-tree */
  ut::unique_ptr<Blob_handle> m_blob_handle;
};

} /* namespace bulk */

/** @note We should call commit(false) for a Page_load object, which is not in
m_page_loaders after page_commit, and we will commit or abort Page_load
objects in function "finish". */
class Btree_load : private ut::Non_copyable {
 public:
  /** Merge multiple Btree_load sub-trees together. */
  class Merger;

  dberr_t insert_blob(lob::ref_t &ref, const dfield_t *dfield) {
    return m_blob_inserter.insert_blob(ref, dfield);
  }

  /** Create a blob.
  @param[out]  blob_ctx  pointer to an opaque object representing a blob.
  @param[out]  ref       blob reference to be placed in the record.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t open_blob(Blob_context &blob_ctx, lob::ref_t &ref) {
    return m_blob_inserter.open_blob(blob_ctx, ref);
  }

  /** Write data into the blob.
  @param[in]  blob_ctx  pointer to blob into which data is written.
  @param[in,out] ref    blob reference of the current blob
  @param[in]  data      buffer containing data to be written
  @param[in]  len       length of the data to be written.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t write_blob(Blob_context blob_ctx, lob::ref_t &ref, const byte *data,
                     size_t len) {
    return m_blob_inserter.write_blob(blob_ctx, ref, data, len);
  }

  /** Indicate that the blob has been completed, so that resources can be
  removed, and as necessary flushing can be done.
  @param[in]  blob_ctx  pointer to blob which has been completely written.
  @param[out] ref   blob reference of the closed blob.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t close_blob(Blob_context blob_ctx, lob::ref_t &ref) {
    return m_blob_inserter.close_blob(blob_ctx, ref);
  }

 public:
  using Page_loaders = std::vector<Page_load *, ut::allocator<Page_load *>>;
  using Level_ctxs = std::vector<Level_ctx *, ut::allocator<Level_ctx *>>;

  /** Helper to set wait callbacks for the current scope. */
  class Wait_callbacks {
   public:
    using Function = std::function<void()>;
    friend class Btree_load;

    Wait_callbacks(Btree_load *btree_load, Function &begin, Function &end)
        : m_btree_load(btree_load) {
      m_btree_load->m_fn_wait_begin = begin;
      m_btree_load->m_fn_wait_end = end;
    }

    ~Wait_callbacks() {
      m_btree_load->m_fn_wait_begin = nullptr;
      m_btree_load->m_fn_wait_end = nullptr;
    }

   private:
    /** Btree Load for the wait callbacks. */
    Btree_load *m_btree_load;
  };

  /** Constructor
  @param[in]  index  B-tree index.
  @param[in]  trx  Transaction object.
  @param[in]  loader_num loader index
  @param[in]  flush_queue_size bulk flusher queue size
  @param[in]  allocator extent allocator */
  Btree_load(dict_index_t *index, trx_t *trx, size_t loader_num,
             size_t flush_queue_size,
             Bulk_extent_allocator &allocator) noexcept;

  /** Destructor */
  ~Btree_load() noexcept;

  /** Initialize.  Allocates the m_heap_order memory heap.
  @return DB_SUCCESS on success or an error code on failure. */
  dberr_t init();

#ifdef UNIV_DEBUG
  /** Save flushed page numbers for debugging purposes.
  @param[in]  page_no  page number of the page that is flushed. */
  void track_page_flush(page_no_t page_no) {
    m_bulk_flusher.m_flushed_page_nos.push_back(page_no);
  }
#endif /* UNIV_DEBUG */

  /** Check if the index build operation has been interrupted.
  @return true if the index build operation is interrupted, false otherwise.*/
  bool is_interrupted() const;

  /** Trigger flusher thread and check for error.
  @return Innodb error code. */
  dberr_t trigger_flusher() const { return m_bulk_flusher.check_and_notify(); }

  /** Get the index object.
  @return index object. */
  dict_index_t *index() const { return m_index; }

  const char *get_table_name() const { return m_index->table->name.m_name; }

  /** Get the root page number of this tree/subtree.
  @return the root page number of this tree/subtree. */
  page_no_t get_subtree_root() const { return m_first_page_nos.back(); }

  /** Get the level of the root page.
  @return the level of the root page. */
  size_t get_root_level() const { return m_root_level; }

  /** Get information about root page. */
  void get_root_page_stat(Page_stat &stat);

  /** Get the transaction id.
  @return the transaction id. */
  trx_id_t get_trx_id() const;

  /** Btree bulk load finish. We commit the last page in each level
  and copy the last page in top level to the root page of the index
  if no error occurs.
  @param[in]    is_err    Whether bulk load was successful until now
  @param[in]    subtree	  true if a subtree is being built, false otherwise.
  @return error code  */
  [[nodiscard]] dberr_t finish(bool is_err, const bool subtree) noexcept;

  /** Insert a tuple to a page in a level
  @param[in] dtuple             Tuple to insert
  @param[in] level                  B-tree level
  @return error code */
  [[nodiscard]] dberr_t insert(dtuple_t *dtuple, size_t level) noexcept;

  /** Split the right most block of the tree at the given level.
  @param[in,out]  block  the right most block at the given level.
  @param[in]  level  level of the given block.
  @param[in]  node_ptr  node pointer to be inserted in the block after
                        splitting.
  @param[in]  mtr  mini transaction context.
  @param[in,out]  highest_level  highest level among all the subtrees.*/
  void split_rightmost(buf_block_t *block, size_t level, dtuple_t *node_ptr,
                       mtr_t *mtr, size_t &highest_level);

  /** Split the left most block of the tree at the given level.
  @param[in,out]  block  the left most block at the given level. it will be
                         updated with the new left most block.
  @param[in]  level  level of the given block.
  @param[in]  node_ptr  node pointer to be inserted in the block after
                        splitting.
  @param[in]  mtr  mini transaction context.
  @param[in,out]  highest_level  highest level among all the subtrees.*/
  void split_leftmost(buf_block_t *&block, size_t level, dtuple_t *node_ptr,
                      mtr_t *mtr, size_t &highest_level);

 private:
  /** Set the root page on completion.
  @param[in] last_page_no       Last page number (the new root).
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t load_root_page(page_no_t last_page_no) noexcept;

 public:
  /** Commit(finish) a page. We set next/prev page no, insert a node pointer to
  father page if needed, and commit mini-transaction.
  @param[in]    page_load               Page to commit
  @param[in]    next_page_load    Next page
  @param[in]    insert_father       Flag whether need to insert node ptr
  @return       error code */
  [[nodiscard]] dberr_t page_commit(Page_load *page_load,
                                    Page_load *next_page_load,
                                    bool insert_father) noexcept;

  /** Prepare space to insert a tuple.
  @param[in,out] page_load      Page bulk that will be used to store the record.
                                It may be replaced if there is not enough space
                                to hold the record.
  @param[in]  level             B-tree level
  @param[in]  rec_size          Record size
  @return error code */
  [[nodiscard]] dberr_t prepare_space(Page_load *&page_load, size_t level,
                                      size_t rec_size) noexcept;

  /** Insert a tuple to a page.
  @param[in]  page_load         Page bulk object
  @param[in]  tuple             Tuple to insert
  @param[in]  big_rec           Big record vector, maybe NULL if there is no
                                Data to be stored externally.
  @param[in]  rec_size          Record size
  @return error code */
  [[nodiscard]] dberr_t insert(Page_load *page_load, dtuple_t *tuple,
                               big_rec_t *big_rec, size_t rec_size) noexcept;

  /** Btree page bulk load finish. Commits the last page in each level
  if no error occurs. Also releases all page bulks.
  @param[in]  is_err       Whether bulk load was successful until now
  @param[out] last_page_no Last page number
  @return error code  */
  [[nodiscard]] dberr_t finalize_page_loads(bool is_err,
                                            page_no_t &last_page_no) noexcept;

 public:
  /** Allocate an extent.
  @param[in,out]  page_range  the range of pages allocated.
  @param[in]  level  btree level for which pages are allocated.
  @return status code. */
  dberr_t alloc_extent(Page_range_t &page_range, size_t level);

  /** Initiate a direct file write  operation.
  @param[in]  block  block to be written to disk.
  @return error code. */
  [[nodiscard]] dberr_t fil_io(buf_block_t *block) noexcept;

  /** Flush the blob pages.
  @return status code. */
  [[nodiscard]] dberr_t flush_blobs() noexcept;

  /** Add the given block the internal cache of blocks.
  @param[in]  block  the block to be cached. */
  inline void block_put(buf_block_t *block);

  /** Remove the given block from the internal cache of blocks.
  @param[in]  page_no  the page number of block to be removed from cache. */
  inline void block_remove(const page_no_t page_no);

  /** Search for a BUF_BLOCK_MEMORY block with given page number in the local
  cache.
  @param[in]  page_no  the page number of block to be fetched.
  @return buffer block with given page number. */
  [[nodiscard]] inline buf_block_t *block_get(page_no_t page_no) const noexcept;

  /** Evict all the pages in the given range from the buffer pool.
  @param[in]  range  range of page numbers.
  @param[in]  dirty_is_ok  it is OK for a page to be dirty. */
  void force_evict(const Page_range_t &range, const bool dirty_is_ok = true);

 public:
  /** Check if a new level is needed. */
  bool is_new_level(size_t level) const { return level >= m_level_ctxs.size(); }

  /** Last page numbers of each level. */
  std::vector<page_no_t, ut::allocator<page_no_t>> m_last_page_nos{};

  /** First page numbers of each level. */
  std::vector<page_no_t, ut::allocator<page_no_t>> m_first_page_nos{};

  /** Get the level context object.
  @param[in]  level  the level number. level 0 is leaf level.
  @return the level context object. */
  Level_ctx *get_level(size_t level) const;

  /** Page numbers of the pages that has been allocated in the leaf level.
  The page range is [p1, p2), where p2 is not included. */
  Page_range_t m_page_range_leaf{};

  /** Page numbers of the pages that has been allocated in the non-leaf level.
  The page range is [p1, p2), where p2 is not included. */
  Page_range_t m_page_range_top{};

  byte m_fseg_hdr_leaf[FSEG_HEADER_SIZE];
  byte m_fseg_hdr_top[FSEG_HEADER_SIZE];

  /** State of the index. Used for asserting at the end of a
  bulk load operation to ensure that the online status of the
  index does not change */
  IF_DEBUG(unsigned m_index_online{};)

  /** Number of extents allocated for this B-tree. */
  size_t m_stat_n_extents{0};

  /** Number of pages allocated for this B-tree. */
  size_t m_stat_n_pages{0};

 public:
  std::ostream &print_left_pages(std::ostream &out) const;
  std::ostream &print_right_pages(std::ostream &out) const;

  dberr_t check_key_overlap(const Btree_load *r_btree) const;

#ifdef UNIV_DEBUG
  void print_tree_pages() const;
  std::string print_pages_in_level(const size_t level) const;
  /** Check size and validate index of limited size.
  @param[in] index Index to validate
  @return true if successful. */
  static bool validate_index(dict_index_t *index);
#endif /* UNIV_DEBUG */

  /** All allocated extents registers with Btree_load.  */
  void track_extent(Page_extent *page_extent);

  /** Add fully used extents to the bulk flusher. Call this whenever a new
  Page_load is allocated, with finish set to false.  Only in
  Btree_load::finish(), the finish argument will be true.
  @param[in]  finish  if true, add all the tracked extents to the bulk flusher,
                      irrespective of whether it is fully used or not. */
  void add_to_bulk_flusher(bool finish = false);

  /** Add the given page extent object to the bulk flusher.
  @param[in]  page_extent the extent to be flushed. */
  void add_to_bulk_flusher(Page_extent *page_extent);

  /** Check if transparent page compression (TPC) is enabled.
  @return true if TPC is enabled. */
  bool is_tpc_enabled() const;

  /** Check if transparent page encryption (TPE) is enabled.
  @return true if TPE is enabled. */
  bool is_tpe_enabled() const;

  /** @return get flush queue size limit. */
  size_t get_max_flush_queue_size() const {
    return m_bulk_flusher.get_max_queue_size();
  }

  /** If the data is already sorted and checked for duplicates, then we can
  disable doing it again. */
  void disable_check_order() { m_check_order = false; }

 private:
  /** Page allocation type. We allocate in extents by default. */
  Bulk_extent_allocator::Type m_alloc_type =
      Bulk_extent_allocator::Type::EXTENT;

  /** Number of records inserted. */
  uint64_t m_n_recs{};

  /** B-tree index */
  dict_index_t *m_index{};

  fil_space_t *m_space{};

  /** Transaction id */
  trx_t *m_trx{};

  /** Root page level */
  size_t m_root_level{};

 private:
  /** Context information for each level of the B-tree.  The leaf level is at
  m_level_ctxs[0]. */
  Level_ctxs m_level_ctxs{};

  /** Reference to global extent allocator. */
  Bulk_extent_allocator &m_allocator;

  /** Extents that are being tracked. */
  std::list<Page_extent *> m_extents_tracked;

  /** If true, check if data is inserted in sorted order. */
  bool m_check_order{true};

  /** Memory heap to be used for sort order checks. */
  mem_heap_t *m_heap_order{};

  /** Function object to compare two tuples. */
  ddl::Compare_key m_compare_key;

  /** The previous tuple that has been inserted. */
  dtuple_t *m_prev_tuple{};

  bool is_extent_tracked(const Page_extent *page_extent) const;

  /** Loader number. */
  size_t m_loader_num{};

  const page_size_t m_page_size;

  /* Begin wait callback function. */
  Wait_callbacks::Function m_fn_wait_begin;

  /* End wait callback function. */
  Wait_callbacks::Function m_fn_wait_end;

  /** Blob inserter that will be used to handle all the externally stored
  fields of InnoDB. */
  bulk::Blob_inserter m_blob_inserter;

  /* Dedicated thread to flush pages. */
  Bulk_flusher m_bulk_flusher;

  friend class bulk::Blob_inserter;
};

class Btree_load::Merger {
 public:
  using Btree_loads = std::vector<Btree_load *, ut::allocator<Btree_load *>>;

  Merger(Btree_loads &loads, dict_index_t *index, trx_t *trx)
      : m_btree_loads(loads),
        m_index(index),
        m_trx(trx),
        m_tuple_heap(2048, UT_LOCATION_HERE) {}

  dberr_t merge(bool sort);

 private:
  /** Get the maximum free space available in an empty page in bytes.
  @return the maximum free space available in an empty page. */
  size_t get_max_free() const {
    return page_get_free_space_of_empty(dict_table_is_comp(m_index->table));
  }

  /** Remove any empty sub-trees with no records. */
  void remove_empty_subtrees();

#ifdef UNIV_DEBUG
  /** Validate sub-tree boundaries. */
  void validate_boundaries();

#endif /* UNIV_DEBUG */

  /** Stich sub-trees together to form a tree with one or multiple
  nodes at highest leve.
  @param[out]  highest_level  highest level of the merged tree.
  @return innodb error code. */
  dberr_t subtree_link_levels(size_t &highest_level);

  /** Create root node for the stiched sub-trees by combining the nodes
  at highest level creating another level if required.
  @param[in]  highest_level  highest level of the merged tree.
  @return innodb error code. */
  dberr_t add_root_for_subtrees(const size_t highest_level);

  /** Insert the given list of node pointers into pages at the given level.
  @param[in,out]  all_node_ptrs  list of node pointers
  @param[in,out]  total_node_ptrs_size  total space in bytes needed to insert
                                        all the node pointers.
  @param[in]  level  the level at which the node pointers are inserted.
  @return DB_SUCCESS if successful.
  @return error code on failure. */
  dberr_t insert_node_ptrs(std::vector<dtuple_t *> &all_node_ptrs,
                           size_t &total_node_ptrs_size, size_t level);

  /** Load the left page and update its FIL_PAGE_NEXT.
  @param[in]  l_page_no  left page number
  @param[in]  r_page_no  right page number. */
  void link_right_sibling(const page_no_t l_page_no, const page_no_t r_page_no);

 private:
  /** Refernce to the subtrees to be merged. */
  Btree_loads &m_btree_loads;

  /** Index which is being built. */
  dict_index_t *m_index{};

  /** Transaction making the changes. */
  trx_t *m_trx{};

  /** Memory heap to store node pointers. */
  Scoped_heap m_tuple_heap{};
};

inline bool Btree_load::is_extent_tracked(
    const Page_extent *page_extent) const {
  for (auto e : m_extents_tracked) {
    if (page_extent == e) {
      return true;
    }
  }
  return false;
}

/** The proper function call sequence of Page_load is as below:
-- Page_load::init
-- Page_load::insert
-- Page_load::finish
-- Page_load::commit */
class Page_load : private ut::Non_copyable {
 public:
  using Rec_offsets = ulint *;

  /** Ctor.
  @param[in]	index	    B-tree index
  @param[in]	btree_load  btree object to which this page belongs. */
  Page_load(dict_index_t *index, Btree_load *btree_load);

  /** Destructor. */
  ~Page_load() noexcept;

  /** Check if page is corrupted.
  @return true if corrupted, false otherwise. */
  bool is_corrupted() const;

  /** Print the child page numbers. */
  void print_child_page_nos() noexcept;

  /** Check if state of this page is BUF_BLOCK_MEMORY.
  @return true if page state is BUF_BLOCK_MEMORY, false otherwise.*/
  bool is_memory() const { return m_block->is_memory(); }

  /** A static member function to create this object.
  @param[in]  btree_load  the bulk load object to which this Page_load belongs.
  @param[in]  page_extent page extent to which this page belongs. */
  static Page_load *create(Btree_load *btree_load, Page_extent *page_extent);

  /** Release the page loader. Delete if not cached.
  @param[in] page_load page loader to delete. */
  static void drop(Page_load *page_load);

  /** Constructor
  @param[in]	index	    B-tree index
  @param[in]	trx_id	    Transaction id
  @param[in]	page_no	    Page number
  @param[in]	level	    Page level
  @param[in]	observer    Flush observer
  @param[in]	btree_load  btree object to which this page belongs. */
  Page_load(dict_index_t *index, trx_id_t trx_id, page_no_t page_no,
            size_t level, Flush_observer *observer,
            Btree_load *btree_load = nullptr) noexcept
      : m_index(index),
        m_trx_id(trx_id),
        m_page_no(page_no),
        m_level(level),
        m_is_comp(dict_table_is_comp(index->table)),
        m_flush_observer(observer),
        m_btree_load(btree_load) {
    ut_ad(!dict_index_is_spatial(m_index));
  }

  /** Set the transaction id.
  @param[in]  trx_id  the transaction id to used. */
  void set_trx_id(const trx_id_t trx_id) { m_trx_id = trx_id; }

  /** Get the current transaction identifier.
  @return the current transaction identifier.*/
  trx_id_t get_trx_id() const { return m_trx_id; }

  /** Set the flush observer.
  @param[in]  observer the flush observer object to use. */
  void set_flush_observer(Flush_observer *observer) {
    m_flush_observer = observer;
  }

  bool is_leaf() const { return m_level == 0; }

  /** Set the page number of this object. */
  void set_page_no(const page_no_t page_no);

  void set_leaf_seg(const fseg_header_t *hdr) {
    memcpy(m_page + PAGE_HEADER + PAGE_BTR_SEG_LEAF, hdr, FSEG_HEADER_SIZE);
  }
  void set_top_seg(const fseg_header_t *hdr) {
    memcpy(m_page + PAGE_HEADER + PAGE_BTR_SEG_TOP, hdr, FSEG_HEADER_SIZE);
  }

  /** Initialize members and allocate page if needed and start mtr.
  @note Must be called and only once right after constructor.
  @return error code */
  [[nodiscard]] dberr_t init() noexcept;
  [[nodiscard]] dberr_t init_mem(const page_no_t new_page_no,
                                 Page_extent *page_extent) noexcept;

  /** Initialize a memory block to be used for storing blobs.
  @param[in]  page_no  the page number to be set in the memory block.
  @param[in]  page_extent  extent to which this page belongs.
  @return DB_SUCCESS on success, error code on failure.*/
  [[nodiscard]] dberr_t init_mem_blob(const page_no_t page_no,
                                      Page_extent *page_extent) noexcept;

  /** Allocate a page for this Page_load object.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t alloc() noexcept;

  /** Re-initialize this page. */
  [[nodiscard]] dberr_t reinit() noexcept;

  /** Reset this object so that Page_load::init() can be called again on this
  object. */
  void reset() noexcept;

  /** Insert a tuple in the page.
  @param[in]  tuple             Tuple to insert
  @param[in]  big_rec           External record
  @param[in]  rec_size          Record size
  @return error code */
  [[nodiscard]] dberr_t insert(const dtuple_t *tuple, const big_rec_t *big_rec,
                               size_t rec_size) noexcept;

  /** Mark end of insertion to the page. Scan records to set page dirs,
  and set page header members. The scan is incremental (slots and records
  which assignment could be "finalized" are not checked again. Check the
  m_slotted_rec_no usage, note it could be reset in some cases like
  during split.
  Note: we refer to page_copy_rec_list_end_to_created_page.*/
  void finish() noexcept;

  /** Commit mtr for a page
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t commit() noexcept;

  /** Commit mtr for a page */
  void rollback() noexcept;

  /** Check whether the record needs to be stored externally.
  @return false if the entire record can be stored locally on the page */
  [[nodiscard]] bool need_ext(const dtuple_t *tuple,
                              size_t rec_size) const noexcept;

  /** Store externally the first possible field of the given tuple.
  @return true if a field was stored externally, false if it was not possible
  to store any of the fields externally. */
  [[nodiscard]] bool make_ext(dtuple_t *tuple);

  /** Get node pointer
  @return node pointer */
  [[nodiscard]] dtuple_t *get_node_ptr() noexcept;

  /** Get node pointer
  @param[in] heap allocate node pointer in the given heap.
  @return node pointer */
  [[nodiscard]] dtuple_t *get_node_ptr(mem_heap_t *heap) noexcept;

  /** Copy all records from page.
  @param[in]  src_page          Page with records to copy. */
  size_t copy_all(const page_t *src_page) noexcept;

  /** Distribute all records from this page to the given pages.
  @param[in,out]  to_pages array of Page_load objects.
  return total number of records processed. */
  size_t copy_to(std::vector<Page_load *> &to_pages);

  /** Set next page
  @param[in]	next_page_no	    Next page no */
  void set_next(page_no_t next_page_no) noexcept;

  /** Set previous page
  @param[in]	prev_page_no	    Previous page no */
  void set_prev(page_no_t prev_page_no) noexcept;

  /** Get previous page (FIL_PAGE_PREV). */
  page_no_t get_prev() noexcept;

  /** Start mtr and latch block */
  void latch() noexcept;

  /** Check if required space is available in the page for the rec
  to be inserted.	We check fill factor & padding here.
  @param[in]	rec_size	        Required space
  @return true	if space is available */
  [[nodiscard]] inline bool is_space_available(size_t rec_size) const noexcept;

  /** Get the page number of this page load object.
  @return the page number of this page load object. */
  [[nodiscard]] page_no_t get_page_no() const noexcept { return m_page_no; }

  [[nodiscard]] page_id_t get_page_id() const noexcept {
    return m_block->page.id;
  }

  /** Get the physical page size of the underlying tablespace.
  @return the physical page size of the tablespace. */
  size_t get_page_size() const noexcept;

  /** Get the table space ID.
  @return the table space ID. */
  space_id_t space() const noexcept;

#ifdef UNIV_DEBUG
  /** Obtain tablespace id from the frame and the buffer block and ensure that
  they are the same.
  @return true if space id is same in both places. */
  bool verify_space_id() const;
#endif /* UNIV_DEBUG */

  /** Get page level */
  [[nodiscard]] size_t get_level() const noexcept { return m_level; }

  /** Set the level of this page. */
  void set_level(size_t level) noexcept { m_level = level; }

  /** Get record no */
  [[nodiscard]] size_t get_rec_no() const { return m_rec_no; }

  /** Get page */
  [[nodiscard]] const page_t *get_page() const noexcept {
    return buf_block_get_frame(m_block);
  }

  [[nodiscard]] page_t *get_page() noexcept {
    return buf_block_get_frame(m_block);
  }

 public:
  void init_for_writing();
  size_t get_data_size() const { return page_get_data_size(m_page); }

#ifdef UNIV_DEBUG
  /** Check if index is X locked
  @return true if index is locked. */
  bool is_index_locked() noexcept;
#endif /* UNIV_DEBUG */

  /** Copy given and all following records.
  @param[in]  first_rec         First record to copy */
  size_t copy_records(const rec_t *first_rec) noexcept;

  /** Insert a record in the page, check for duplicates too.
  @param[in]  rec               Record
  @param[in]  offsets           Record offsets
  @return DB_SUCCESS or error code. */
  dberr_t insert(const rec_t *rec, Rec_offsets offsets) noexcept;

 public:
  /** Store external record
  Since the record is not logged yet, so we don't log update to the record.
  the blob data is logged first, then the record is logged in bulk mode.
  @param[in]  big_rec           External record
  @param[in]  offsets           Record offsets
  @return error code */
  [[nodiscard]] dberr_t store_ext(const big_rec_t *big_rec,
                                  Rec_offsets offsets) noexcept;

  /** Set the REC_INFO_MIN_REC_FLAG on the first user record in this page.
  @param[in]  mtr  mini transaction context. */
  void set_min_rec_flag(mtr_t *mtr);

  /** Set the REC_INFO_MIN_REC_FLAG on the first user record in this page. */
  void set_min_rec_flag();
  bool is_min_rec_flag() const;

  /** Set the level context object for this page load
  @param[in]  level_ctx  the level context object. */
  void set_level_ctx(Level_ctx *level_ctx) { m_level_ctx = level_ctx; }

  /** Check if this page load object contains a level context object.
  @return true if the page load contains a level context object.
  @return false if the page load does NOT contain a level context object.*/
  bool has_level_ctx() const { return m_level_ctx != nullptr; }

  /** Free the memory block. */
  void free();

  dict_index_t *index() { return m_index; }

  buf_block_t *get_block() { return m_block; }

  void set_page_extent(Page_extent *page_extent) {
    m_page_extent = page_extent;
  }

  /** Mark the Page load as cached. Flush thread should not free this Page. */
  void set_cached() { m_is_cached.store(true); }

  /** @return true iff it is a cached Page Load. */
  bool is_cached() const { return m_is_cached.load(); }

 private:
  /** Memory heap for internal allocation */
  mem_heap_t *m_heap{};

  /** The index B-tree */
  dict_index_t *m_index{};

  /** The min-transaction */
  mtr_t *m_mtr{};

  /** The transaction id */
  trx_id_t m_trx_id{};

  /** The buffer block */
  buf_block_t *m_block{};

  /** The page */
  page_t *m_page{};

  /** The current rec, just before the next insert rec */
  rec_t *m_cur_rec{};

  /** The page no */
  page_no_t m_page_no{};

  /** The page level in B-tree */
  size_t m_level{};

  /** Flag: is page in compact format */
  const bool m_is_comp{};

  /** The heap top in page for next insert */
  byte *m_heap_top{};

  /** User record no */
  size_t m_rec_no{};

  /** The free space left in the page */
  size_t m_free_space{};

  /** The reserved space for fill factor */
  size_t m_reserved_space{};

  /** Total data in the page */
  IF_DEBUG(size_t m_total_data{};)

  /** The modify clock value of the buffer block
  when the block is re-pinned */
  uint64_t m_modify_clock{};

  /** Flush observer */
  Flush_observer *m_flush_observer{};

  /** Last record assigned to a slot. */
  rec_t *m_last_slotted_rec{};

  /** Number of records assigned to slots. */
  size_t m_slotted_rec_no{};

  /** Page modified flag. */
  bool m_modified{};

  Btree_load *m_btree_load{};

  Level_ctx *m_level_ctx{};

  Page_extent *m_page_extent{};

  /** true iff the the Page load is cached. */
  std::atomic_bool m_is_cached{false};

  friend class Btree_load;
};

inline dtuple_t *Page_load::get_node_ptr() noexcept {
  return get_node_ptr(m_heap);
}

inline space_id_t Page_load::space() const noexcept { return m_index->space; }

inline size_t Page_load::get_page_size() const noexcept {
  const page_size_t page_size = m_index->get_page_size();
  return page_size.physical();
}

inline Level_ctx *Btree_load::get_level(size_t level) const {
  ut_a(m_level_ctxs.size() > level);
  return m_level_ctxs[level];
}

/** Information about a buffer page. */
struct Page_stat {
  /** Number of user records in the page. */
  size_t m_n_recs;

  /** Number of bytes of data. */
  size_t m_data_size;
};

inline void Page_extent::append(Page_load *page_load) {
  ut_ad(page_load->get_block() != nullptr);
  ut_ad(page_load->is_memory());
  ut_ad(page_load->get_page_no() >= m_range.first);
  ut_ad(page_load->get_page_no() < m_range.second);
  for (auto &iter : m_page_loads) {
    if (iter->get_page_no() == page_load->get_page_no()) {
      /* Page already appended. Don't append again. */
      return;
    }
  }
  ut_ad(m_page_loads.size() < FSP_EXTENT_SIZE);
  m_page_loads.push_back(page_load);
}

inline trx_id_t Level_ctx::get_trx_id() const {
  return m_btree_load->get_trx_id();
}

inline space_id_t Page_extent::space() const {
  return m_btree_load->index()->space;
}

inline Page_extent::Page_extent(Btree_load *btree_load, const bool is_leaf)
    : m_page_no(FIL_NULL),
      m_range(FIL_NULL, FIL_NULL),
      m_btree_load(btree_load),
      m_is_leaf(is_leaf) {
  IF_DEBUG(m_is_owned_by_bulk_flusher.store(false));
}

inline Page_extent *Page_extent::create(Btree_load *btree_load,
                                        const bool is_leaf, bool skip_track) {
  Page_extent *p = ut::new_withkey<Page_extent>(UT_NEW_THIS_FILE_PSI_KEY,
                                                btree_load, is_leaf);
  if (!skip_track) {
    btree_load->track_extent(p);
  }
  p->m_is_cached.store(false);
  return p;
}

inline void Page_extent::drop(Page_extent *extent) {
  if (extent == nullptr) {
    return;
  }
  if (extent->is_cached()) {
    ut_a(!extent->is_free());
    bool free = true;
    extent->set_state(free);
    return;
  }
  ut::delete_(extent);
}

/** Function object to compare two Btree_load objects. */
struct Btree_load_compare {
  Btree_load_compare(dict_index_t *index) : m_index(index) {}
  bool operator()(const Btree_load *l_btree, const Btree_load *r_btree);
  dict_index_t *m_index;
};

#ifdef UNIV_DEBUG
void bulk_load_enable_slow_io_debug();
void bulk_load_disable_slow_io_debug();
#endif /* UNIV_DEBUG */

inline void Page_extent::free_memory_blocks() {
  for (auto page_load : m_page_loads) {
    page_load->free();
  }
}

namespace bulk {
inline trx_id_t Blob_inserter::get_trx_id() const {
  return m_btree_load.get_trx_id();
}
} /* namespace bulk */

} /* namespace Btree_multi */

#endif /* btr0mtib_h */

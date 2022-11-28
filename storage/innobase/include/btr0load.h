/*****************************************************************************

Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

/** @file include/btr0load.h
 The B-tree bulk load

 Created 03/11/2014 Shaohua Wang
 *************************************************************************/

#ifndef btr0load_h
#define btr0load_h

#include <stddef.h>
#include <vector>

#include "ddl0impl-compare.h"
#include "dict0dict.h"
#include "page0cur.h"
#include "ut0class_life_cycle.h"
#include "ut0new.h"

// Forward declaration.
class Page_load;
class Btree_load;
struct Page_stat;

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
  size_t used_pages() const { return m_page_loads.size(); }

  /** Check if the range is valid.
  @return true if the range is valid, false otherwise. */
  bool is_valid() const;

  bool is_null() const {
    return (m_range.first == FIL_NULL) && (m_range.second == FIL_NULL);
  }

  Page_range_t pages_to_free() const;

  /** Initialize the next page number to be allocated. The page range should
  have been already initialized. */
  void init();

  /** Check if no more pages are there to be used.
  @return true if the page extent is completed used.
  @return false if the page extent has more pages to be used. */
  bool is_fully_used() const { return m_page_no == m_range.second; }
  bool is_page_loads_full() const {
    return m_page_loads.size() == (m_range.second - m_range.first);
  }

 public:
  /** Allocate a page number. */
  page_no_t alloc();

  /** Save a page_load. */
  void append(Page_load *page_load);

  /** Flush the used pages to disk. It also frees the unused pages back to the
  segment.
  @return On success, return DB_SUCCESS. */
  dberr_t flush();

  /** Flush one page at a time.  This can be used when scatter/gather i/o is
  not available for use. */
  dberr_t flush_one_by_one();

  /** Flush 1 extent pages at a time. Internally it will call OS dependent
  API (either bulk_flush_win() on Windows or bulk_flush_linux() on other
  operating systems.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t bulk_flush();

#ifdef _WIN32
  /** Flush 1 extent pages at a time. This is Windows specific implementation.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t bulk_flush_win();
#else  /* _WIN32 */
  /** Flush 1 extent pages at a time. Uses pwritev() i/o API.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t bulk_flush_linux();
#endif /* _WIN32 */

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

  /** Reaset page load cache to free all. */
  void reset_cached_page_loads() { m_next_cached_page_load_index = 0; }

 public:
  std::ostream &print(std::ostream &out) const;

 private:
  Btree_load *m_btree_load{};
  const bool m_is_leaf; /* true if this extent belongs to leaf segment. */
  /** true iff the the extent is cached. */
  std::atomic_bool m_is_cached = ATOMIC_VAR_INIT(false);
  /** true if the cached entry is free to be used. */
  std::atomic_bool m_is_free = ATOMIC_VAR_INIT(true);
  /** Cached page loads. */
  std::vector<Page_load *> m_cached_page_loads;
  /** Next cached page load index. */
  size_t m_next_cached_page_load_index{0};

  friend struct Level_ctx;
};

inline Page_extent::~Page_extent() {
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

inline Page_range_t Page_extent::pages_to_free() const {
  return std::make_pair(m_page_no, m_range.second);
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

  /** Allocate a page number.  Subsequently a Page_load will be created with the
  allocated page number.
  @return page number that was allocated. */
  page_no_t alloc_page_num();

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
};

inline Page_load *Level_ctx::get_page_load() const { return m_page_load; }

inline void Level_ctx::set_current_page_load(Page_load *sibling) {
  m_page_load = sibling;
}

/** The page allocations for all blobs of the index is taken care of by this
class.  This means that one extent allocated here can be used for different
blobs.  The blob index extents and data extents are kept separate, because their
flushing needs are differnet. */
class Blob_load {
 public:
  /* Constructor. */
  Blob_load(Btree_load *btree_load) : m_btree_load(btree_load) {}

  /** Create an instance of Blob_load.
  @param[in]  btree_load  bulk load object to which this obj belongs.*/
  static Blob_load *create(Btree_load *btree_load);

  /** Destroy an instance of Blob_load.
  @param[in]  blob_load  the instance to be destroyed.*/
  static void destroy(Blob_load *blob_load);

  /** Flush all the remaining extents.  Normally called at the end of
  Btree_load. */
  dberr_t finish();

  /** Flush full index extents. Called at the end of each blob insertion.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t flush_index_extents();
  dberr_t flush_data_extents();

  dberr_t add_index_extent();

  /** Allocate a page that is to be used as first page of blob. */
  buf_block_t *alloc_first_page();
  buf_block_t *alloc_index_page();
  buf_block_t *alloc_data_page();
  bool validate();
  void clear_cache() noexcept { m_block_cache.clear(); }

  /** Add the given block to the local cache of BUF_BLOCK_MEMORY blocks
  maintained in this object.
  @param[in]  block  the buffer block to be added to local cache. */
  void block_put(buf_block_t *block);
  void block_remove(const page_no_t page_no);
  [[nodiscard]] buf_block_t *block_get(page_no_t page_no) const noexcept;

 private:
  /** Get the last allocated index extent. */
  Page_extent *&get_index_extent() { return m_index_extents.back(); }

  /* Blob data pages are managed here.  In the case of uncompressed data, all
  complete extents are flushed as soon as they are full. */
  std::list<Page_extent *> m_data_extents;

  /* Blob index pages are managed here.  All complete extents are flushed at
  the end of a blob insert. */
  std::list<Page_extent *> m_index_extents;

  /** This Blob_load object belongs to the higher level Btree_load. */
  Btree_load *m_btree_load;

  using Block_cache =
      std::map<page_no_t, buf_block_t *, std::less<page_no_t>,
               ut::allocator<std::pair<const page_no_t, buf_block_t *>>>;

  /** Cache of BUF_BLOCK_MEMORY blocks containing the LOB index pages. */
  Block_cache m_block_cache;

#ifdef UNIV_DEBUG
  /** Total blob pages allocated. */
  size_t m_page_count{};

  /** Print collected statistics information. */
  std::ostream &print_stats(std::ostream &out) const;
#endif /* UNIV_DEBUG */
};

class Bulk_flusher {
 public:
  /** Thread main function. */
  void run();

  /** Check if work is available for the bulk flusher thread.
  @return true if work is available. */
  bool is_work_available();

  /** Start a new thread to do the flush work. */
  void start();

  /** Add a page extent to the bulk flush queue. */
  void add(Page_extent *page_extent);

  /** Wait till the bulk flush thread stops. */
  void wait_to_stop();

  /** Check if the bulk flusher queue is full.
  @return true if queue is full, false otherwise. */
  bool is_full() const { return m_queue_full; }

  /** Get the current queue size.
  @return the current queue size. */
  size_t get_queue_size() const;

  /** Get the maximum allowed queue size.
  @return the maximum allowed queue size. */
  size_t get_max_queue_size() const { return m_max_queue_size; }

  /** Destructor. */
  ~Bulk_flusher();

 private:
  /** Calculate and set the value of maximum queue size. */
  void set_max_queue_size();

 private:
  /** Do the actual work of flushing. */
  void do_work();

  /** Check if the bulk flush thread should stop working. */
  bool should_i_stop() const { return m_stop.load(); }

  /** When no work is available, put the thread to sleep. */
  void sleep() {
    m_n_sleep++;
    std::this_thread::sleep_for(s_sleep_duration);
  }

  /** Print useful information to the server log file while exiting. */
  void info();

  /** This queue is protected by the m_mutex. */
  std::vector<Page_extent *> m_queue;

  /** This mutex protects the m_queue. */
  mutable std::mutex m_mutex;

  /** Flag to indicate if the bulk flusher thread should stop. If true, the
  bulk flusher thread will stop after emptying the queue.  If false, the
  bulk flusher thread will go to sleep after emptying the queue. */
  std::atomic<bool> m_stop{false};

  /** Private queue (private to the bulk flush thread) containing the extents to
  flush. */
  std::vector<Page_extent *> m_priv_queue;

  /** Bulk flusher thread. */
  std::thread m_flush_thread;

  /** Number of times slept */
  size_t m_n_sleep{};

  /** The sleep duration in milliseconds. */
  static constexpr std::chrono::milliseconds s_sleep_duration{10};

  /** Maximum queue size, defaults to 4 */
  size_t m_max_queue_size{4};

  /** A flag to indicate the flush queue is full. */
  std::atomic<bool> m_queue_full{false};

  /** Number of pages flushed. */
  size_t m_pages_flushed{};
};

/** @note We should call commit(false) for a Page_load object, which is not in
m_page_loaders after page_commit, and we will commit or abort Page_load
objects in function "finish". */
class Btree_load : private ut::Non_copyable {
 public:
  /** Merge multiple Btree_load sub-trees together. */
  class Merger;

  /** Interface to consume from. */
  struct Cursor {
    /** Constructor. */
    Cursor() = default;

    /** Destructor. */
    virtual ~Cursor() = default;

    /** Fetch the current row as a tuple.
    @param[out] dtuple          Row represented as a tuple.
    @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
    [[nodiscard]] virtual dberr_t fetch(dtuple_t *&dtuple) noexcept = 0;

    /** @return true if duplicates detected. */
    virtual bool duplicates_detected() const noexcept = 0;

    /** Move to the next record.
    @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
    [[nodiscard]] virtual dberr_t next() noexcept = 0;
  };

 public:
  using Page_loaders = std::vector<Page_load *, ut::allocator<Page_load *>>;
  using Level_ctxs = std::vector<Level_ctx *, ut::allocator<Level_ctx *>>;

  /** Constructor
  @param[in]  index  B-tree index.
  @param[in]  trx  Transaction object.
  @param[in]  observer  Flush observer */
  Btree_load(dict_index_t *index, trx_t *trx,
             Flush_observer *observer) noexcept;

  /** Destructor */
  ~Btree_load() noexcept;

  /** Initialize.  Allocates the m_heap_order memory heap.
  @return DB_SUCCESS on success or an error code on failure. */
  dberr_t init();

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

  /** Load the btree from the cursor.
  @param[in,out] cursor         Cursor to read tuples from.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build(Cursor &cursor) noexcept;

  /** Btree bulk load finish. We commit the last page in each level
  and copy the last page in top level to the root page of the index
  if no error occurs.
  @param[in]    err    Whether bulk load was successful until now
  @param[in]    subtree	  true if a subtree is being built, false otherwise.
  @return error code  */
  [[nodiscard]] dberr_t finish(dberr_t err, const bool subtree) noexcept;

  /** Release latch on the rightmost leaf page in the index tree.
  @note It does not do anything now. */
  void release() noexcept;

  /** Re-latch latch on the rightmost leaf page in the index tree.
  @note It does not do anything now. */
  void latch() noexcept;

  /** Insert a tuple to a page in a level
  @param[in] dtuple             Tuple to insert
  @param[in] level                  B-tree level
  @return error code */
  [[nodiscard]] dberr_t insert(dtuple_t *dtuple, size_t level) noexcept;

 private:
  /** Set the root page on completion.
  @param[in] last_page_no       Last page number (the new root).
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t load_root_page(page_no_t last_page_no) noexcept;

  /** Split a page
  @param[in]  page_loader  Page to split
  @param[in]  next_page_load  Next page
  @return  error code */
  [[nodiscard]] dberr_t page_split(Page_load *page_loader,
                                   Page_load *next_page_load) noexcept;

 public:
  /** Commit(finish) a page. We set next/prev page no, compress a page of
  compressed table and split the page if compression fails, insert a node
  pointer to father page if needed, and commit mini-transaction.
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

  /** Log free check */
  void log_free_check() noexcept;

  /** Btree page bulk load finish. Commits the last page in each level
  if no error occurs. Also releases all page bulks.
  @param[in]  err               Whether bulk load was successful until now
  @param[out] last_page_no      Last page number
  @return error code  */
  [[nodiscard]] dberr_t finalize_page_loads(dberr_t err,
                                            page_no_t &last_page_no) noexcept;

 public:
  /** Check if a leaf page is available.
  @return true if leaf page is available, false otherwise. */
  bool is_leaf_page_available() const {
    return m_page_range_leaf.first < m_page_range_leaf.second;
  }

  /** Check if a top page (aka non-leaf page) is available.
  @return true if top page is available, false otherwise. */
  bool is_top_page_available() const {
    return m_page_range_top.first < m_page_range_top.second;
  }

  /** Get the next available leaf page number that can be used.
  @return leaf page number. */
  [[nodiscard]] page_no_t get_leaf_page() noexcept;

  /** Get the next available top page number that can be used.
  @return top page number. */
  [[nodiscard]] page_no_t get_top_page() noexcept;

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
  @param[in]  range  range of page numbers. */
  void force_evict(const Page_range_t &range);

  /** Free the pages in the page range.
  @param[in]  range  page numbers to be freed [range.first, range.second).
  @param[in]  fseg_hdr  the segment header to which pages belong. */
  void free_pages(const Page_range_t &range, fseg_header_t *fseg_hdr);
  void free_pages_leaf(const Page_range_t &range);
  void free_pages_top(const Page_range_t &range);

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

  bool does_keys_overlap(const Btree_load *r_btree) const;

  /** Based on current usage of the buffer pool, decide if allocation needs to
  be in pages or extents.
  @return if true allocate in pages, if false allocate in extents.*/
  bool allocate_in_pages() const;

#ifdef UNIV_DEBUG
  void print_tree_pages() const;
  void print_pages_in_level(const size_t level) const;
#endif /* UNIV_DEBUG */

  bool is_compressed() const { return m_page_size.is_compressed(); }

  /** Get a reference to the Blob_load object. */
  Blob_load *blob() { return m_blob_load; }

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

  /** Set number of page ranges to cache. Optimization to avoid acquiring X
  lock frequently.
  @param[in] num_ranges ranges to cache for leaf and non-leaf segments */
  void set_cached_range(size_t num_ranges) {
    if (num_ranges > S_MAX_CACHED_RANGES) {
      num_ranges = S_MAX_CACHED_RANGES;
    }
    m_max_cached = num_ranges;
  }

  /** @return true iff page ranges should be cached. */
  bool cache_ranges() const { return m_max_cached > 0; }

  /** Fill cached ranges and get the first range.
  @param[out]    page_range cached page range
  @param[in]     level      B-tree level
  @param[in,out] mtr        mini-transaction
  @return innodb error code. */
  dberr_t fill_cached_range(Page_range_t &page_range, size_t level, mtr_t *mtr);

  /** Get page range from cached ranges.
  @param[out] page_range cached page range
  @param[in]  level      B-tree level
  @return true iff cached range is found. */
  bool get_cached_range(Page_range_t &page_range, size_t level);

  /** @return get flush queue size limit. */
  size_t get_max_flush_queue_size() const {
    return m_bulk_flusher.get_max_queue_size();
  }

 private:
  /** Number of ranges to cache. */
  size_t m_max_cached{};

  /** Maximum number of page ranges to cache. */
  const static size_t S_MAX_CACHED_RANGES = 16;

  /** Cached non-leaf page ranges. */
  Page_range_t m_cached_ranges_top[S_MAX_CACHED_RANGES];

  /** Current number of cached non-leaf ranges. */
  size_t m_num_top_cached{};

  /** Cached leaf page ranges. */
  Page_range_t m_cached_ranges_leaf[S_MAX_CACHED_RANGES];

  /** Current number of cached leaf ranges. */
  size_t m_num_leaf_cached{};

  /** Number of records inserted. */
  uint64_t m_n_recs{};

  /** B-tree index */
  dict_index_t *m_index{};

  /** Transaction id */
  trx_t *m_trx{};

  /** Root page level */
  size_t m_root_level{};

 private:
  /** Flush observer */
  Flush_observer *m_flush_observer{};

  /** Context information for each level of the B-tree.  The leaf level is at
  m_level_ctxs[0]. */
  Level_ctxs m_level_ctxs{};

  /** For blob operations. */
  Blob_load *m_blob_load{};

  /* Dedicated thread to flush pages. */
  Bulk_flusher m_bulk_flusher;

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

  size_t m_n_threads{};

  const page_size_t m_page_size;
};

class Btree_load::Merger {
 public:
  using Btree_loads = std::vector<Btree_load *, ut::allocator<Btree_load *>>;

  Merger(Btree_loads &loads, dict_index_t *index, trx_t *trx)
      : m_btree_loads(loads), m_index(index), m_trx(trx) {}

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

  /* Commit the root page of the combined final B-tree.
  @param[in]  root_load   the page load object of the root page.
  @return DB_SUCCESS if root page committed successfully.
  @return error code on failure. */
  dberr_t root_page_commit(Page_load *root_load);

  /* If the compression of the root page failed, then it needs to be split into
  multiple pages and compressed again.
  @param[in]  root_load   the page load object of the root page.
  @param[in]  n_pages  the number of pages the root page will be split into.
  @return DB_SUCCESS if root page committed successfully.
  @return error code on failure. */
  dberr_t root_split(Page_load *root_load, const size_t n_pages);

 private:
  /** Refernce to the subtrees to be merged. */
  Btree_loads &m_btree_loads;

  /** Index which is being built. */
  dict_index_t *m_index{};

  /** Transaction making the changes. */
  trx_t *m_trx{};
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
-- Page_load::compress(COMPRESSED table only)
-- Page_load::page_split(COMPRESSED table only)
-- Page_load::commit */
class Page_load : private ut::Non_copyable {
  using Rec_offsets = ulint *;

  /** Page split point descriptor. */
  struct Split_point {
    /** Record being the point of split.
    All records before this record should stay on current on page.
    This record and all following records should be moved to new page. */
    rec_t *m_rec{};

    /** Number of records before this record. */
    size_t m_n_rec_before{};
  };

 public:
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

  /** Set the flush observer.
  @param[in]  observer the flush observer object to use. */
  void set_flush_observer(Flush_observer *observer) {
    m_flush_observer = observer;
  }

  bool is_leaf() const { return m_level == 0; }

  page_no_t get_new_page_no() {
    page_no_t page_no;
    if (is_leaf()) {
      page_no = m_btree_load->get_leaf_page();
    } else {
      page_no = m_btree_load->get_top_page();
    }
    return page_no;
  }

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

  /** Allocate a page for this Page_load object.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t alloc() noexcept;

  /** Re-initialize this page. */
  [[nodiscard]] dberr_t reinit() noexcept;

  /** Initialize members and allocate page for blob. */
  dberr_t init_blob(const page_no_t new_page_no) noexcept;

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

  /** Compress if it is compressed table
  @return	true	compress successfully or no need to compress
  @return	false	compress failed. */
  [[nodiscard]] bool compress() noexcept;

  /** Check whether the record needs to be stored externally.
  @return false if the entire record can be stored locally on the page */
  [[nodiscard]] bool need_ext(const dtuple_t *tuple,
                              size_t rec_size) const noexcept;

  /** Get node pointer
  @return node pointer */
  [[nodiscard]] dtuple_t *get_node_ptr() noexcept;

  /** Split the page records between this and given bulk.
  @param new_page_load  The new bulk to store split records. */
  void split(Page_load &new_page_load) noexcept;

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

  /** Release block by committing mtr */
  inline void release() noexcept;

  /** Start mtr and latch block */
  void latch() noexcept;

  /** Check if required space is available in the page for the rec
  to be inserted.	We check fill factor & padding here.
  @param[in]	rec_size	        Required space
  @return true	if space is available */
  [[nodiscard]] inline bool is_space_available(size_t rec_size) const noexcept;

  /** Get page no */
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
  [[nodiscard]] page_zip_des_t *get_page_zip() noexcept {
    return m_block->get_page_zip();
  }

  void init_for_writing();

  /** Check if table is compressed.
  @return true if table is compressed, false otherwise. */
  [[nodiscard]] bool is_table_compressed() const noexcept {
    return m_page_zip != nullptr;
  }

  size_t get_data_size() const { return page_get_data_size(m_page); }

#ifdef UNIV_DEBUG
  /** Check if index is X locked
  @return true if index is locked. */
  bool is_index_locked() noexcept;
#endif /* UNIV_DEBUG */

  /** Get page split point. We split a page in half when compression
  fails, and the split record and all following records should be copied
  to the new page.
  @return split record descriptor */
  [[nodiscard]] Split_point get_split_rec() noexcept;

  /** Copy given and all following records.
  @param[in]  first_rec         First record to copy */
  size_t copy_records(const rec_t *first_rec) noexcept;

  /** Remove all records after split rec including itself.
  @param[in]  split_point       Split point descriptor */
  void split_trim(const Split_point &split_point) noexcept;

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

  /** Check if the current page is a compressed blob page.
  @return true if the current page is a compressed blob page. */
  [[nodiscard]] bool is_zblob() const;

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

  /** The page zip descriptor */
  page_zip_des_t *m_page_zip{};

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

  /** The padding space for compressed page */
  size_t m_padding_space{};

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
  std::atomic_bool m_is_cached = ATOMIC_VAR_INIT(false);

  friend class Btree_load;
};

inline space_id_t Page_load::space() const noexcept { return m_index->space; }

inline size_t Page_load::get_page_size() const noexcept {
  const page_size_t page_size = m_index->get_page_size();
  return page_size.physical();
}

inline void Btree_load::block_put(buf_block_t *block) {
  m_blob_load->block_put(block);
}

inline void Blob_load::block_put(buf_block_t *block) {
  ut_ad(block->is_memory());
  const page_no_t page_no = block->get_page_no();
  ut_ad(page_get_page_no(buf_block_get_frame(block)) == page_no);
  m_block_cache[page_no] = block;
}

inline void Blob_load::block_remove(const page_no_t page_no) {
  m_block_cache.erase(page_no);
}

inline void Btree_load::block_remove(const page_no_t page_no) {
  m_blob_load->block_remove(page_no);
}

[[nodiscard]] inline buf_block_t *Btree_load::block_get(
    page_no_t page_no) const noexcept {
  return m_blob_load->block_get(page_no);
}

[[nodiscard]] inline buf_block_t *Blob_load::block_get(
    page_no_t page_no) const noexcept {
  auto iter = m_block_cache.find(page_no);
  if (iter == m_block_cache.end()) {
    return nullptr;
  }
  return iter->second;
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
  ut_ad(m_page_loads.size() < FSP_EXTENT_SIZE);
  for (auto &iter : m_page_loads) {
    if (iter->get_page_no() == page_load->get_page_no()) {
      /* Page already appended. Don't append again. */
      return;
    }
  }
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
      m_is_leaf(is_leaf) {}

inline Page_extent *Page_extent::create(Btree_load *btree_load,
                                        const bool is_leaf, bool skip_track) {
  Page_extent *p = ut::new_withkey<Page_extent>(UT_NEW_THIS_FILE_PSI_KEY,
                                                btree_load, is_leaf);
  if (!skip_track) {
    /* blob extents are tracked by Blob_load */
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

inline Blob_load *Blob_load::create(Btree_load *btree_load) {
  Blob_load *x =
      ut::new_withkey<Blob_load>(UT_NEW_THIS_FILE_PSI_KEY, btree_load);
  return x;
}

inline void Blob_load::destroy(Blob_load *blob_load) {
  if (blob_load != nullptr) {
    ut::delete_(blob_load);
  }
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

#endif /* btr0load_h */

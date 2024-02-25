/*****************************************************************************

Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

/** @file include/row0pread.h
Parallel read interface.

Created 2018-01-27 by Sunny Bains. */

#ifndef row0par_read_h
#define row0par_read_h

#include <functional>
#include <vector>

#include "os0thread-create.h"
#include "row0sel.h"

// Forward declarations
struct trx_t;
struct mtr_t;
class PCursor;
struct btr_pcur_t;
struct buf_block_t;
struct dict_table_t;

#include "btr0cur.h"
#include "db0err.h"
#include "fil0fil.h"
#include "os0event.h"
#include "page0size.h"
#include "rem0types.h"
#include "ut0mpmcbq.h"

/** The core idea is to find the left and right paths down the B+Tree.These
paths correspond to the scan start and scan end search. Follow the links
at the appropriate btree level from the left to right and split the scan
on each of these sub-tree root nodes.

If the user has set the maximum number of threads to use at say 4 threads
and there are 5 sub-trees at the selected level then we will split the 5th
sub-tree dynamically when it is ready for scan.

We want to allow multiple parallel range scans on different indexes at the
same time. To achieve this split out the scan  context (Scan_ctx) from the
execution context (Ctx). The Scan_ctx has the index  and transaction
information and the Ctx keeps track of the cursor for a specific thread
during the scan.

To start a scan we need to instantiate a Parallel_reader. A parallel reader
can contain several Scan_ctx instances and a Scan_ctx can contain several
Ctx instances. Its' the Ctx instances that are eventually executed.

This design allows for a single Parallel_reader to scan multiple indexes
at once.  Each index range scan has to be added via its add_scan() method.
This functionality is required to handle parallel partition scans because
partitions are separate indexes. This can be used to scan completely
different indexes and tables by one instance of a Parallel_reader.

To solve the imbalance problem we dynamically split the sub-trees as and
when required. e.g., If you have 5 sub-trees to scan and 4 threads then
it will tag the 5th sub-tree as "to_be_split" during phase I (add_scan()),
the first thread that finishes scanning the first set of 4 partitions will
then dynamically split the 5th sub-tree and add the newly created sub-trees
to the execution context (Ctx) run queue in the Parallel_reader. As the
other threads complete their sub-tree scans they will pick up more execution
contexts (Ctx) from the Parallel_reader run queue and start scanning the
sub-partitions as normal.

Note: The Ctx instances are in a virtual list. Each Ctx instance has a
range to scan. The start point of this range instance is the end point
of the Ctx instance scanning values less than its start point. A Ctx
will scan from [Start, End) rows. We use std::shared_ptr to manage the
reference counting, this allows us to dispose of the Ctx instances
without worrying about dangling pointers.

NOTE: Secondary index scans are not supported currently. */
class Parallel_reader {
 public:
  /** Maximum value for innodb-parallel-read-threads. */
  constexpr static size_t MAX_THREADS{256};

  /** Maximum value for reserved parallel read threads for data load so that
  at least this many threads are always available for data load. */
  constexpr static size_t MAX_RESERVED_THREADS{16};

  /** Maximum value for at most number of parallel read threads that can be
  spawned. */
  constexpr static size_t MAX_TOTAL_THREADS{MAX_THREADS + MAX_RESERVED_THREADS};

  using Links = std::vector<page_no_t, ut::allocator<page_no_t>>;

  // Forward declaration.
  class Ctx;
  class Scan_ctx;
  struct Thread_ctx;

  /** Scan state. */
  enum class State : uint8_t {
    /** Unknown state. */
    UNKNOWN,

    /** Start/Finish thread state. */
    THREAD,

    /** Start/Finish Ctx state. */
    CTX,

    /** Start/Finish page read. */
    PAGE
  };

  /** Callback to initialise callers state. */
  using Start = std::function<dberr_t(Thread_ctx *thread_ctx)>;

  /** Callback to finalise callers state. */
  using Finish = std::function<dberr_t(Thread_ctx *thread_ctx)>;

  /** Callback to process the rows. */
  using F = std::function<dberr_t(const Ctx *)>;

  /** Specifies the range from where to start the scan and where to end it. */
  struct Scan_range {
    /** Default constructor. */
    Scan_range() : m_start(), m_end() {}

    /** Copy constructor.
    @param[in] scan_range       Instance to copy from. */
    Scan_range(const Scan_range &scan_range) = default;

    /** Constructor.
    @param[in] start            Start key
    @param[in] end              End key. */
    Scan_range(const dtuple_t *start, const dtuple_t *end)
        : m_start(start), m_end(end) {}

    /** Start of the scan, can be nullptr for -infinity. */
    const dtuple_t *m_start{};

    /** End of the scan, can be null for +infinity. */
    const dtuple_t *m_end{};

    /** Convert the instance to a string representation. */
    [[nodiscard]] std::string to_string() const;
  };

  /** Scan (Scan_ctx) configuration. */
  struct Config {
    /** Constructor.
    @param[in] scan_range     Range to scan.
    @param[in] index          Cluster index to scan.
    @param[in] read_level     Btree level from which records need to be read.
    @param[in] partition_id   Partition id if the index to be scanned.
                              belongs to a partitioned table. */
    Config(const Scan_range &scan_range, dict_index_t *index,
           size_t read_level = 0,
           size_t partition_id = std::numeric_limits<size_t>::max())
        : m_scan_range(scan_range),
          m_index(index),
          m_is_compact(dict_table_is_comp(index->table)),
          m_page_size(dict_tf_to_fsp_flags(index->table->flags)),
          m_read_level(read_level),
          m_partition_id(partition_id) {}

    /** Copy constructor.
    @param[in] config           Instance to copy from. */
    Config(const Config &config)

        = default;

    /** Range to scan. */
    const Scan_range m_scan_range;

    /** (Cluster) Index in table to scan. */
    dict_index_t *m_index{};

    /** Row format of table. */
    const bool m_is_compact{};

    /** Tablespace page size. */
    const page_size_t m_page_size;

    /** Btree level from which records need to be read. */
    size_t m_read_level{0};

    /** Partition id if the index to be scanned belongs to a partitioned table,
    else std::numeric_limits<size_t>::max(). */
    size_t m_partition_id{std::numeric_limits<size_t>::max()};
  };

  /** Thread related context information. */
  struct Thread_ctx {
    /** Constructor.
    @param[in]  id  Thread ID */
    explicit Thread_ctx(size_t id) noexcept : m_thread_id(id) {}

    /** Destructor. */
    ~Thread_ctx() noexcept {
      ut_a(m_callback_ctx == nullptr);

      if (m_blob_heap != nullptr) {
        mem_heap_free(m_blob_heap);
      }
    }

    /** Set thread related callback information.
    @param[in]  ctx callback context */
    template <typename T>
    void set_callback_ctx(T *ctx) noexcept {
      ut_ad(m_callback_ctx == nullptr || ctx == nullptr);
      m_callback_ctx = ctx;
    }

    /** Get the thread related callback information/
    @return return context. */
    template <typename T>
    T *get_callback_ctx() noexcept {
      return static_cast<T *>(m_callback_ctx);
    }

    /** Create BLOB heap. */
    void create_blob_heap() noexcept {
      ut_a(m_blob_heap == nullptr);
      m_blob_heap = mem_heap_create(UNIV_PAGE_SIZE, UT_LOCATION_HERE);
    }

    /** @return the worker thread state. */
    State get_state() const noexcept { return m_state; }

    /** Save current position, commit any active mtr. */
    void savepoint() noexcept;

    /** Restore saved position and resume scan.
    @return DB_SUCCESS or error code. */
    [[nodiscard]] dberr_t restore_from_savepoint() noexcept;

    /** Thread ID. */
    size_t m_thread_id{std::numeric_limits<size_t>::max()};

    /** Callback information related to the thread.
    @note Needs to be created and destroyed by the callback itself. */
    void *m_callback_ctx{};

    /** BLOB heap per thread. */
    mem_heap_t *m_blob_heap{};

    /** Worker thread state. */
    State m_state{State::UNKNOWN};

    /** Current persistent cursor. */
    PCursor *m_pcursor{};

    Thread_ctx(Thread_ctx &&) = delete;
    Thread_ctx(const Thread_ctx &) = delete;
    Thread_ctx &operator=(Thread_ctx &&) = delete;
    Thread_ctx &operator=(const Thread_ctx &) = delete;
  };

  /** Constructor.
  @param[in]  max_threads Maximum number of threads to use. */
  explicit Parallel_reader(size_t max_threads);

  /** Destructor. */
  ~Parallel_reader();

  /** Check how many threads are available for parallel reads.
  @param[in] n_required   Number of threads required.
  @param[in] use_reserved true if reserved threads needs to be considered
  while checking for availability of threads
  @return number of threads available. */
  [[nodiscard]] static size_t available_threads(size_t n_required,
                                                bool use_reserved);

  /** Release the parallel read threads. */
  static void release_threads(size_t n_threads) {
    const auto SEQ_CST = std::memory_order_seq_cst;
    auto active = s_active_threads.fetch_sub(n_threads, SEQ_CST);
    ut_a(active >= n_threads);
  }

  /** Add scan context.
  @param[in,out]  trx         Covering transaction.
  @param[in]      config      Scan condfiguration.
  @param[in]      f           Callback function.
  (default is 0 which is leaf level)
  @return error. */
  [[nodiscard]] dberr_t add_scan(trx_t *trx, const Config &config, F &&f);

  /** Wait for the join of threads spawned by the parallel reader. */
  void join() {
    for (auto &t : m_parallel_read_threads) {
      t.wait();
    }
  }

  /** Get the error stored in the global error state.
  @return global error state. */
  [[nodiscard]] dberr_t get_error_state() const { return m_err; }

  /** @return true if the tree is empty, else false. */
  [[nodiscard]] bool is_tree_empty() const {
    return m_ctx_id.load(std::memory_order_relaxed) == 0;
  }

  /** Set the callback that must be called before any processing.
  @param[in] f                  Call before first row is processed.*/
  void set_start_callback(Start &&f) { m_start_callback = std::move(f); }

  /** Set the callback that must be called after all processing.
  @param[in] f                  Call after last row is processed.*/
  void set_finish_callback(Finish &&f) { m_finish_callback = std::move(f); }

  /** Spawn the threads to do the parallel read for the specified range.
  Don't wait for the spawned to threads to complete.
  @param[in]  n_threads number of threads that *need* to be spawned
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t spawn(size_t n_threads) noexcept;

  /** Start the threads to do the parallel read for the specified range.
  @param[in] n_threads          Number of threads to use for the scan.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t run(size_t n_threads);

  /** @return the configured max threads size. */
  [[nodiscard]] size_t max_threads() const { return m_max_threads; }

  /** @return true if in error state. */
  [[nodiscard]] bool is_error_set() const {
    return m_err.load(std::memory_order_relaxed) != DB_SUCCESS;
  }

  /** Set the error state.
  @param[in] err                Error state to set to. */
  void set_error_state(dberr_t err) {
    m_err.store(err, std::memory_order_relaxed);
  }

  /** Set the number of threads to be spawned.
  @param[in]  n_threads number of threads to be spawned. */
  void set_n_threads(size_t n_threads) {
    ut_ad(n_threads <= m_max_threads);
    m_n_threads = n_threads;
  }

  // Disable copying.
  Parallel_reader(const Parallel_reader &) = delete;
  Parallel_reader(const Parallel_reader &&) = delete;
  Parallel_reader &operator=(Parallel_reader &&) = delete;
  Parallel_reader &operator=(const Parallel_reader &) = delete;

 private:
  /** Release unused threads back to the pool.
  @param[in] unused_threads     Number of threads to "release". */
  void release_unused_threads(size_t unused_threads) {
    ut_a(m_max_threads >= unused_threads);
    release_threads(unused_threads);
  }

  /** Add an execution context to the run queue.
  @param[in] ctx                Execution context to add to the queue. */
  void enqueue(std::shared_ptr<Ctx> ctx);

  /** Fetch the next job execute.
  @return job to execute or nullptr. */
  [[nodiscard]] std::shared_ptr<Ctx> dequeue();

  /** @return true if job queue is empty. */
  [[nodiscard]] bool is_queue_empty() const;

  /** Poll for requests and execute.
  @param[in]  thread_ctx  thread related context information */
  void worker(Thread_ctx *thread_ctx);

  /** Create the threads and do a parallel read across the partitions. */
  void parallel_read();

  /** @return true if tasks are still executing. */
  [[nodiscard]] bool is_active() const {
    return m_n_completed.load(std::memory_order_relaxed) <
           m_ctx_id.load(std::memory_order_relaxed);
  }

 private:
  // clang-format off
  using Ctxs =
      std::list<std::shared_ptr<Ctx>,
                ut::allocator<std::shared_ptr<Ctx>>>;

  using Scan_ctxs =
      std::list<std::shared_ptr<Scan_ctx>,
                ut::allocator<std::shared_ptr<Scan_ctx>>>;

  // clang-format on

  /** Maximum number of worker threads to use. */
  size_t m_max_threads{};

  /** Number of worker threads that will be spawned. */
  size_t m_n_threads{0};

  /** Mutex protecting m_ctxs. */
  mutable ib_mutex_t m_mutex;

  /** Contexts that must be executed. */
  Ctxs m_ctxs{};

  /** Scan contexts. */
  Scan_ctxs m_scan_ctxs{};

  /** For signalling worker threads about events. */
  os_event_t m_event{};

  /** Value returned by previous call of os_event_reset() on m_event. */
  uint64_t m_sig_count;

  /** Counter for allocating scan context IDs. */
  size_t m_scan_ctx_id{};

  /** Context ID. Monotonically increasing ID. */
  std::atomic_size_t m_ctx_id{};

  /** Total tasks executed so far. */
  std::atomic_size_t m_n_completed{};

  /** Callback at start (before processing any rows). */
  Start m_start_callback{};

  /** Callback at end (adter processing all rows). */
  Finish m_finish_callback{};

  /** Error during parallel read. */
  std::atomic<dberr_t> m_err{DB_SUCCESS};

  /** List of threads used for paralle_read purpose. */
  std::vector<IB_thread, ut::allocator<IB_thread>> m_parallel_read_threads;

  /** Number of threads currently doing parallel reads. */
  static std::atomic_size_t s_active_threads;

  /** If the caller wants to wait for the parallel_read to finish it's run */
  bool m_sync;

  /** Context information related to each parallel reader thread. */
  std::vector<Thread_ctx *, ut::allocator<Thread_ctx *>> m_thread_ctxs;
};

/** Parallel reader context. */
class Parallel_reader::Scan_ctx {
 public:
  /** Constructor.
  @param[in]  reader          Parallel reader that owns this context.
  @param[in]  id              ID of this scan context.
  @param[in]  trx             Transaction covering the scan.
  @param[in]  config          Range scan config.
  @param[in]  f               Callback function. */
  Scan_ctx(Parallel_reader *reader, size_t id, trx_t *trx,
           const Parallel_reader::Config &config, F &&f);

  /** Destructor. */
  ~Scan_ctx() = default;

 private:
  /** Boundary of the range to scan. */
  struct Iter {
    /** Destructor. */
    ~Iter();

    /** Heap used to allocate m_rec, m_tuple and m_pcur. */
    mem_heap_t *m_heap{};

    /** m_rec column offsets. */
    const ulint *m_offsets{};

    /** Start scanning from this key. Raw data of the row. */
    const rec_t *m_rec{};

    /** Tuple representation inside m_rec, for two Iter instances in a range
    m_tuple will be [first->m_tuple, second->m_tuple). */
    const dtuple_t *m_tuple{};

    /** Persistent cursor.*/
    btr_pcur_t *m_pcur{};
  };

  /** mtr_t savepoint. */
  using Savepoint = std::pair<ulint, buf_block_t *>;

  /** For releasing the S latches after processing the blocks. */
  using Savepoints = std::vector<Savepoint, ut::allocator<Savepoint>>;

  /** The first cursor should read up to the second cursor [f, s). */
  using Range = std::pair<std::shared_ptr<Iter>, std::shared_ptr<Iter>>;

  using Ranges = std::vector<Range, ut::allocator<Range>>;

  /** @return the scan context ID. */
  [[nodiscard]] size_t id() const { return m_id; }

  /** Set the error state.
  @param[in] err                Error state to set to. */
  void set_error_state(dberr_t err) {
    m_err.store(err, std::memory_order_relaxed);
  }

  /** @return true if in error state. */
  [[nodiscard]] bool is_error_set() const {
    return m_err.load(std::memory_order_relaxed) != DB_SUCCESS;
  }

  /** Fetch a block from the buffer pool and acquire an S latch on it.
  @param[in]      page_id       Page ID.
  @param[in,out]  mtr           Mini-transaction covering the fetch.
  @param[in]      line          Line from where called.
  @return the block fetched from the buffer pool. */
  [[nodiscard]] buf_block_t *block_get_s_latched(const page_id_t &page_id,
                                                 mtr_t *mtr, size_t line) const;

  /** Partition the B+Tree for parallel read.
  @param[in] scan_range Range for partitioning.
  @param[in,out]  ranges        Ranges to scan.
  @param[in] split_level  Sub-range required level (0 == root).
  @return the partition scan ranges. */
  dberr_t partition(const Scan_range &scan_range, Ranges &ranges,
                    size_t split_level);

  /** Find the page number of the node that contains the search key. If the
  key is null then we assume -infinity.
  @param[in]  block             Page to look in.
  @param[in] key                Key of the first record in the range.
  @return the left child page number. */
  [[nodiscard]] page_no_t search(const buf_block_t *block,
                                 const dtuple_t *key) const;

  /** Traverse from given sub-tree page number to start of the scan range
  from the given page number.
  @param[in]      page_no       Page number of sub-tree.
  @param[in,out]  mtr           Mini-transaction.
  @param[in]      key           Key of the first record in the range.
  @param[in,out]  savepoints    Blocks S latched and accessed.
  @return the leaf node page cursor. */
  [[nodiscard]] page_cur_t start_range(page_no_t page_no, mtr_t *mtr,
                                       const dtuple_t *key,
                                       Savepoints &savepoints) const;

  /** Create and add the range to the scan ranges.
  @param[in,out]  ranges        Ranges to scan.
  @param[in,out]  leaf_page_cursor Leaf page cursor on which to create the
                                persistent cursor.
  @param[in,out]  mtr           Mini-transaction */
  void create_range(Ranges &ranges, page_cur_t &leaf_page_cursor,
                    mtr_t *mtr) const;

  /** Find the subtrees to scan in a block.
  @param[in]      scan_range    Partition based on this scan range.
  @param[in]      page_no       Page to partition at if at required level.
  @param[in]      depth         Sub-range current level.
  @param[in]      split_level   Sub-range starting level (0 == root).
  @param[in,out]  ranges        Ranges to scan.
  @param[in,out]  mtr           Mini-transaction */
  dberr_t create_ranges(const Scan_range &scan_range, page_no_t page_no,
                        size_t depth, const size_t split_level, Ranges &ranges,
                        mtr_t *mtr);

  /** Build a dtuple_t from rec_t.
  @param[in]      rec           Build the dtuple from this record.
  @param[in,out]  iter          Build in this iterator. */
  void copy_row(const rec_t *rec, Iter *iter) const;

  /** Create the persistent cursor that will be used to traverse the
  partition and position on the the start row.
  @param[in]      page_cursor   Current page cursor
  @param[in]      mtr           Mini-transaction covering the read.
  @return Start iterator. */
  [[nodiscard]] std::shared_ptr<Iter> create_persistent_cursor(
      const page_cur_t &page_cursor, mtr_t *mtr) const;

  /** Build an old version of the row if required.
  @param[in,out]  rec           Current row read from the index. This can
                                be modified by this method if an older version
                                needs to be built.
  @param[in,out]  offsets       Same as above but pertains to the rec offsets
  @param[in,out]  heap          Heap to use if a previous version needs to be
                                built from the undo log.
  @param[in,out]  mtr           Mini-transaction covering the read.
  @return true if row is visible to the transaction. */
  [[nodiscard]] bool check_visibility(const rec_t *&rec, ulint *&offsets,
                                      mem_heap_t *&heap, mtr_t *mtr);

  /** Create an execution context for a range and add it to
  the Parallel_reader's run queue.
  @param[in] range              Range for which to create the context.
  @param[in] split              true if the sub-tree should be split further.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t create_context(const Range &range, bool split);

  /** Create the execution contexts based on the ranges.
  @param[in]  ranges            Ranges for which to create the contexts.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t create_contexts(const Ranges &ranges);

  /** @return the maximum number of threads configured. */
  [[nodiscard]] size_t max_threads() const { return m_reader->max_threads(); }

  /** Release unused threads back to the pool.
  @param[in] unused_threads     Number of threads to "release". */
  void release_threads(size_t unused_threads) {
    m_reader->release_threads(unused_threads);
  }

  /** S lock the index. */
  void index_s_lock();

  /** S unlock the index. */
  void index_s_unlock();

  /** @return true if at least one thread owns the S latch on the index. */
  bool index_s_own() const {
    return m_s_locks.load(std::memory_order_acquire) > 0;
  }

 private:
  using Config = Parallel_reader::Config;

  /** Context ID. */
  size_t m_id{std::numeric_limits<size_t>::max()};

  /** Parallel scan configuration. */
  Config m_config;

  /** Covering transaction. */
  const trx_t *m_trx{};

  /** Callback function. */
  F m_f;

  /** Depth of the Btree. */
  size_t m_depth{};

  /** The parallel reader. */
  Parallel_reader *m_reader{};

  /** Error during parallel read. */
  mutable std::atomic<dberr_t> m_err{DB_SUCCESS};

  /** Number of threads that have S locked the index. */
  std::atomic_size_t m_s_locks{};

  friend class Parallel_reader;

  Scan_ctx(Scan_ctx &&) = delete;
  Scan_ctx(const Scan_ctx &) = delete;
  Scan_ctx &operator=(Scan_ctx &&) = delete;
  Scan_ctx &operator=(const Scan_ctx &) = delete;
};

/** Parallel reader execution context. */
class Parallel_reader::Ctx {
 public:
  /** Constructor.
  @param[in]    id              Thread ID.
  @param[in]    scan_ctx        Scan context.
  @param[in]    range           Range that the thread has to read. */
  Ctx(size_t id, Scan_ctx *scan_ctx, const Scan_ctx::Range &range)
      : m_id(id), m_range(range), m_scan_ctx(scan_ctx) {}

  /** Destructor. */
  ~Ctx() = default;

 public:
  /** @return the context ID. */
  [[nodiscard]] size_t id() const { return m_id; }

  /** The scan ID of the scan context this belongs to. */
  [[nodiscard]] size_t scan_id() const { return m_scan_ctx->id(); }

  /** @return the covering transaction. */
  [[nodiscard]] const trx_t *trx() const { return m_scan_ctx->m_trx; }

  /** @return the index being scanned. */
  [[nodiscard]] const dict_index_t *index() const {
    return m_scan_ctx->m_config.m_index;
  }

  /** @return ID of the thread processing this context */
  [[nodiscard]] size_t thread_id() const { return m_thread_ctx->m_thread_id; }

  /** @return the thread context of the reader thread. */
  [[nodiscard]] Thread_ctx *thread_ctx() const { return m_thread_ctx; }

  /** @return the partition id of the index.
  @note this is std::numeric_limits<size_t>::max() if the index does not
  belong to a partition. */
  [[nodiscard]] size_t partition_id() const {
    return m_scan_ctx->m_config.m_partition_id;
  }

  /** Build an old version of the row if required.
  @param[in,out]  rec           Current row read from the index. This can
                                be modified by this method if an older version
                                needs to be built.
  @param[in,out]  offsets       Same as above but pertains to the rec offsets
  @param[in,out]  heap          Heap to use if a previous version needs to be
                                built from the undo log.
  @param[in,out]  mtr           Mini-transaction covering the read.
  @return true if row is visible to the transaction. */
  bool is_rec_visible(const rec_t *&rec, ulint *&offsets, mem_heap_t *&heap,
                      mtr_t *mtr) {
    return m_scan_ctx->check_visibility(rec, offsets, heap, mtr);
  }

 private:
  /** Traverse the pages by key order.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t traverse();

  /** Traverse the records in a node.
  @param[in]  pcursor persistent b-tree cursor
  @param[in]  mtr mtr
  @return error */
  dberr_t traverse_recs(PCursor *pcursor, mtr_t *mtr);

  /** Move to the next node in the specified level.
  @param[in]  pcursor persistent b-tree cursor
  @return success */
  bool move_to_next_node(PCursor *pcursor);

  /** Split the context into sub-ranges and add them to the execution queue.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t split();

  /** @return true if in error state. */
  [[nodiscard]] bool is_error_set() const {
    return m_scan_ctx->m_reader->is_error_set() || m_scan_ctx->is_error_set();
  }

 private:
  /** Context ID. */
  size_t m_id{std::numeric_limits<size_t>::max()};

  /** If true then split the context at the block level. */
  bool m_split{};

  /** Range to read in this context. */
  Scan_ctx::Range m_range{};

  /** Scanner context. */
  Scan_ctx *m_scan_ctx{};

 public:
  /** Context information related to executing thread ID. */
  Thread_ctx *m_thread_ctx{};

  /** Current block. */
  const buf_block_t *m_block{};

  /** Current row. */
  const rec_t *m_rec{};

  /** Number of pages traversed by the context. */
  size_t m_n_pages{};

  /** True if m_rec is the first record in the page. */
  bool m_first_rec{true};

  ulint *m_offsets{};

  /** Start of a new range to scan. */
  bool m_start{};

  friend class Parallel_reader;
};

#endif /* !row0par_read_h */

/*****************************************************************************

Copyright (c) 2018, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

#include "univ.i"

// Forward declarations
struct trx_t;
struct mtr_t;
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

We want to allow multiple parallel range scans on diffrent indexes at the
same time. To achieve this split out the scan  context (Scan_ctx) from the
execution context (Ctx). The Scan_ctx has the index  and transaction
information and the Ctx keeps track of the cursor for a specific thread
during the scan.

To start a scan we need to instantiate a Parallel_reader. A parallel reader
can contain several Scan_ctx instances and a Scan_ctx can contain several
Ctx instances. Its' the Ctx instances that are eventually executed.

The Parallel_reader will start one thread per Scan_ctx to service read ahead
requests. Currently, the read ahead is a physical read ahead ie. it will read
one extent at a time.

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

  using Links = std::vector<page_no_t, ut_allocator<page_no_t>>;

  // Forward declaration.
  class Ctx;
  class Scan_ctx;

  /** Callback to initialise callers state. */
  using Start = std::function<dberr_t(size_t thread_id)>;

  /** Callback to finalise callers state. */
  using Finish = std::function<dberr_t(size_t thread_id)>;

  /** Callback to process the rows. */
  using F = std::function<dberr_t(const Ctx *)>;

  /** Specifies the range from where to start the scan and where to end it. */
  struct Scan_range {
    /** Default constructor. */
    Scan_range() : m_start(), m_end() {}

    /** Copy constructor.
    @param[in] scan_range       Instance to copy from. */
    Scan_range(const Scan_range &scan_range)
        : m_start(scan_range.m_start), m_end(scan_range.m_end) {}

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
    std::string to_string() const MY_ATTRIBUTE((warn_unused_result));
  };

  /** Scan (Scan_ctx) configuration. */
  struct Config {
    /** Constructor.
    @param[in] scan_range       Range to scan.
    @param[in] index            Cluster index to scan. */
    Config(const Scan_range &scan_range, dict_index_t *index)
        : m_scan_range(scan_range),
          m_index(index),
          m_is_compact(dict_table_is_comp(index->table)),
          m_page_size(dict_tf_to_fsp_flags(index->table->flags)) {}

    /** Copy constructor.
    @param[in] config           Instance to copy from. */
    Config(const Config &config)
        : m_scan_range(config.m_scan_range),
          m_index(config.m_index),
          m_is_compact(config.m_is_compact),
          m_page_size(config.m_page_size) {}

    /** Range to scan. */
    const Scan_range m_scan_range;

    /** (Cluster) Index in table to scan. */
    dict_index_t *m_index{};

    /** Row format of table. */
    const bool m_is_compact{};

    /** Tablespace page size. */
    const page_size_t m_page_size;

    /** if true then enable separate read ahead threads. */
    bool m_read_ahead{true};
  };

  /** Constructor.
  @param[in]  max_threads       Maximum number of threads to use. */
  explicit Parallel_reader(size_t max_threads);

  /** Destructor. */
  ~Parallel_reader();

  /** Check how many threads are available for parallel reads.
  @param[in] n_required         Number of threads required.
  @return number of threads available. */
  static size_t available_threads(size_t n_required)
      MY_ATTRIBUTE((warn_unused_result));

  /** Release the parallel read threads. */
  static void release_threads(size_t n_threads) {
    const auto RELAXED = std::memory_order_relaxed;
    auto active = s_active_threads.fetch_sub(n_threads, RELAXED);
    ut_a(active >= n_threads);
  }

  /** Add scan context.
  @param[in,out]  trx           Covering transaction.
  @param[in] config             Scan condfiguration.
  @param[in] f                  Callback function.
  @return true on success. */
  bool add_scan(trx_t *trx, const Config &config, F &&f)
      MY_ATTRIBUTE((warn_unused_result));

  /** Set the callback that must be called before any processing.
  @param[in] f                  Call before first row is processed.*/
  void set_start_callback(Start &&f) { m_start_callback = std::move(f); }

  /** Set the callback that must be called after all processing.
  @param[in] f                  Call after last row is processed.*/
  void set_finish_callback(Finish &&f) { m_finish_callback = std::move(f); }

  /** Start the threads to do the parallel read for the specified range.
  @return DB_SUCCESS or error code. */
  dberr_t run() MY_ATTRIBUTE((warn_unused_result));

  /** @return the configured max threads size. */
  size_t max_threads() const MY_ATTRIBUTE((warn_unused_result));

  // Disable copying.
  Parallel_reader(const Parallel_reader &) = delete;
  Parallel_reader(const Parallel_reader &&) = delete;
  Parallel_reader &operator=(Parallel_reader &&) = delete;
  Parallel_reader &operator=(const Parallel_reader &) = delete;

 private:
  /** Set the error state.
  @param[in] err                Error state to set to. */
  void set_error_state(dberr_t err) {
    m_err.store(err, std::memory_order_relaxed);
  }

  /** Release unused threads back to the pool.
  @param[in] unused_threads     Number of threads to "release". */
  void release_unused_threads(size_t unused_threads) {
    ut_a(m_max_threads >= unused_threads);
    release_threads(unused_threads);
    m_max_threads -= unused_threads;
  }

  /** @return true if in error state. */
  bool is_error_set() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_err.load(std::memory_order_relaxed) != DB_SUCCESS);
  }

  /** Add an execution context to the run queue.
  @param[in] ctx                Execution context to add to the queue. */
  void enqueue(std::shared_ptr<Ctx> ctx);

  /** Fetch the next job execute.
  @return job to execute or nullptr. */
  std::shared_ptr<Ctx> dequeue() MY_ATTRIBUTE((warn_unused_result));

  /** @return true if job queue is empty. */
  bool is_queue_empty() const MY_ATTRIBUTE((warn_unused_result));

  /** Poll for requests and execute.
  @param[in]      id            Thread ID */
  void worker(size_t id);

  /** Create the threads and do a parallel read across the partitions. */
  void parallel_read();

  /** @return true if tasks are still executing. */
  bool is_active() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_n_completed.load(std::memory_order_relaxed) <
            m_ctx_id.load(std::memory_order_relaxed));
  }

  /** @return true if the read-ahead request queue is empty. */
  bool read_ahead_queue_empty() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_submitted.load(std::memory_order_relaxed) ==
            m_consumed.load(std::memory_order_relaxed));
  }

  /** Read ahead thread.
  @param[in] n_pages            Read ahead batch size. */
  void read_ahead_worker(page_no_t n_pages);

  /** Start the read ahead worker threads. */
  void read_ahead();

 private:
  /** Read ahead request. */
  struct Read_ahead_request {
    /** Default constructor. */
    Read_ahead_request() : m_scan_ctx(), m_page_no(FIL_NULL) {}
    /** Constructor.
    @param[in] scan_ctx         Scan context requesting the read ahead.
    @param[in] page_no          Start page number of read ahead. */
    Read_ahead_request(Scan_ctx *scan_ctx, page_no_t page_no)
        : m_scan_ctx(scan_ctx), m_page_no(page_no) {}

    /** Scan context requesting the read ahead. */
    const Scan_ctx *m_scan_ctx{};

    /** Starting page number. */
    page_no_t m_page_no{};
  };

  // clang-format off
  using Ctxs =
      std::list<std::shared_ptr<Ctx>,
                ut_allocator<std::shared_ptr<Ctx>>>;

  using Scan_ctxs =
      std::list<std::shared_ptr<Scan_ctx>,
                ut_allocator<std::shared_ptr<Scan_ctx>>>;

  /** Read ahead queue. */
  using Read_ahead_queue = mpmc_bq<Read_ahead_request>;
  // clang-format on

  /** Maximum number of worker threads to use. */
  size_t m_max_threads{};

  /** Mutex protecting m_ctxs. */
  mutable ib_mutex_t m_mutex;

  /** Contexts that must be executed. */
  Ctxs m_ctxs{};

  /** Scan contexts. */
  Scan_ctxs m_scan_ctxs{};

  /** For signalling worker threads about events. */
  os_event_t m_event{};

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

  /** Read ahead queue. */
  Read_ahead_queue m_read_aheadq;

  /** Number of read ahead requests submitted. */
  std::atomic<uint64_t> m_submitted{};

  /** Number of read ahead requests processed. */
  std::atomic<uint64_t> m_consumed{};

  /** Error during parallel read. */
  std::atomic<dberr_t> m_err{DB_SUCCESS};

  /** Number of threads currently doing parallel reads. */
  static std::atomic_size_t s_active_threads;

  friend class Ctx;
  friend class Scan_ctx;
};

/** Parallel reader context. */
class Parallel_reader::Scan_ctx {
 private:
  /** Constructor.
  @param[in]  reader          Parallel reader that owns this context.
  @param[in]  id              ID of this scan context.
  @param[in]  config          Range scan config.
  @param[in]  trx             Transaction covering the scan.
  @param[in]  f               Callback function. */
  Scan_ctx(Parallel_reader *reader, size_t id, trx_t *trx,
           const Parallel_reader::Config &config, F &&f);

 public:
  /** Destructor. */
  ~Scan_ctx();

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

    /** Number of externally stored columns. */
    ulint m_n_ext{ULINT_UNDEFINED};

    /** Persistent cursor.*/
    btr_pcur_t *m_pcur{};
  };

  /** mtr_t savepoint. */
  using Savepoint = std::pair<ulint, buf_block_t *>;

  /** For releasing the S latches after processing the blocks. */
  using Savepoints = std::vector<Savepoint, ut_allocator<Savepoint>>;

  /** The first cursor should read up to the second cursor [f, s). */
  using Range = std::pair<std::shared_ptr<Iter>, std::shared_ptr<Iter>>;

  using Ranges = std::vector<Range, ut_allocator<Range>>;

  /** @return the scan context ID. */
  size_t id() const MY_ATTRIBUTE((warn_unused_result)) { return (m_id); }

  /** Set the error state.
  @param[in] err                Error state to set to. */
  void set_error_state(dberr_t err) {
    m_err.store(err, std::memory_order_relaxed);
  }

  /** @return true if in error state. */
  bool is_error_set() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_err.load(std::memory_order_relaxed) != DB_SUCCESS);
  }

  /** Fetch a block from the buffer pool and acquire an S latch on it.
  @param[in]      page_id       Page ID.
  @param[in,out]  mtr           Mini transaction covering the fetch.
  @param[in]      line          Line from where called.
  @return the block fetched from the buffer pool. */
  buf_block_t *block_get_s_latched(const page_id_t &page_id, mtr_t *mtr,
                                   int line) const
      MY_ATTRIBUTE((warn_unused_result));

  /** Partition the B+Tree for parallel read.
  @param[in] scan_range         Range for partitioning.
  @param[in] level              Sub-range required level (0 == root).
  @return the partition scan ranges. */
  Ranges partition(const Scan_range &scan_range, size_t level)
      MY_ATTRIBUTE((warn_unused_result));

  /** Find the page number of the node that contains the search key. If the
  key is null then we assume -infinity.
  @param[in]  block             Page to look in.
  @param[in] key                Key of the first record in the range.
  @return the left child page number. */
  page_no_t search(const buf_block_t *block, const dtuple_t *key) const
      MY_ATTRIBUTE((warn_unused_result));

  /** Traverse from given sub-tree page number to start of the scan range
  from the given page number.
  @param[in]  page_no           Page number of sub-tree.
  @param[in,out]  mtr           Mini-transaction.
  @param[in] key                Key of the first record in the range.
  @param[in,out] savepoints     Blocks S latched and accessed.
  @return the leaf node page cursor. */
  page_cur_t start_range(page_no_t page_no, mtr_t *mtr, const dtuple_t *key,
                         Savepoints &savepoints) const
      MY_ATTRIBUTE((warn_unused_result));

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
  @param[in]      level         Sub-range starting level (0 == root).
  @param[in,out]  ranges        Ranges to scan.
  @param[in,out]  mtr           Mini-transaction */
  void create_ranges(const Scan_range &scan_range, page_no_t page_no,
                     size_t depth, const size_t level, Ranges &ranges,
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
  std::shared_ptr<Iter> create_persistent_cursor(const page_cur_t &page_cursor,
                                                 mtr_t *mtr) const
      MY_ATTRIBUTE((warn_unused_result));

  /** Build an old version of the row if required.
  @param[in,out]  rec           Current row read from the index. This can
                                be modified by this method if an older version
                                needs to be built.
  @param[in,out]  offsets       Same as above but pertains to the rec offsets
  @param[in,out]  heap          Heap to use if a previous version needs to be
                                built from the undo log.
  @param[in,out]  mtr           Mini transaction covering the read.
  @return true if row is visible to the transaction. */
  bool check_visibility(const rec_t *&rec, ulint *&offsets, mem_heap_t *&heap,
                        mtr_t *mtr) MY_ATTRIBUTE((warn_unused_result));

  /** Read ahead from this page number.
  @param[in]  page_no           Start read ahead page number. */
  void submit_read_ahead(page_no_t page_no) {
    ut_ad(page_no != FIL_NULL);
    ut_ad(m_config.m_read_ahead);

    Read_ahead_request read_ahead_request(this, page_no);

    while (!m_reader->m_read_aheadq.enqueue(read_ahead_request)) {
      UT_RELAX_CPU();
    }

    m_reader->m_submitted.fetch_add(1, std::memory_order_relaxed);
  }

  /** Create an execution context for a range and add it to
  the Parallel_reader's run queue.
  @param[in] range              Range for which to create the context.
  @param[in] split              true if the sub-tree should be split further.
  @return DB_SUCCESS or error code. */
  dberr_t create_context(const Range &range, bool split)
      MY_ATTRIBUTE((warn_unused_result));

  /** Create the execution contexts based on the ranges.
  @param[in]  ranges            Ranges for which to create the contexts.
  @return DB_SUCCESS or error code. */
  dberr_t create_contexts(const Ranges &ranges)
      MY_ATTRIBUTE((warn_unused_result));

  /** @return the maximum number of threads configured. */
  size_t max_threads() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_reader->m_max_threads);
  }

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
    return (m_s_locks.load(std::memory_order_acquire) > 0);
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
  friend class Parallel_reader::Ctx;

  Scan_ctx(Scan_ctx &&) = delete;
  Scan_ctx(const Scan_ctx &) = delete;
  Scan_ctx &operator=(Scan_ctx &&) = delete;
  Scan_ctx &operator=(const Scan_ctx &) = delete;
};

/** Parallel reader execution context. */
class Parallel_reader::Ctx {
 private:
  /** Constructor.
  @param[in]    id              Thread ID.
  @param[in]    scan_ctx        Scan context.
  @param[in]    range           Range that the thread has to read. */
  Ctx(size_t id, Scan_ctx *scan_ctx, const Scan_ctx::Range &range)
      : m_id(id), m_range(range), m_scan_ctx(scan_ctx) {}

 public:
  /** Destructor. */
  ~Ctx();

 public:
  /** @return the context ID. */
  size_t id() const MY_ATTRIBUTE((warn_unused_result)) { return (m_id); }

  /** The scan ID of the scan context this belongs to. */
  size_t scan_id() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_scan_ctx->id());
  }

  /** @return the covering transaction. */
  const trx_t *trx() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_scan_ctx->m_trx);
  }

  /** @return the index being scanned. */
  const dict_index_t *index() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_scan_ctx->m_config.m_index);
  }

 private:
  /** Traverse the pages by key order.
  @return DB_SUCCESS or error code. */
  dberr_t traverse() MY_ATTRIBUTE((warn_unused_result));

  /** Split the context into sub-ranges and add them to the execution queue.
  @return DB_SUCCESS or error code. */
  dberr_t split() MY_ATTRIBUTE((warn_unused_result));

 private:
  /** Context ID. */
  size_t m_id{std::numeric_limits<size_t>::max()};

  /** If true the split the context at the block level. */
  bool m_split{};

  /** Range to read in this context. */
  Scan_ctx::Range m_range{};

  /** Scanner context. */
  Scan_ctx *m_scan_ctx{};

 public:
  /** Current executing thread ID. */
  size_t m_thread_id{std::numeric_limits<size_t>::max()};

  /** Current block. */
  const buf_block_t *m_block{};

  /** Current row. */
  const rec_t *m_rec{};

  /** Start of a new range to scan. */
  bool m_start{};

  friend class Parallel_reader;
};

#endif /* !row0par_read_h */

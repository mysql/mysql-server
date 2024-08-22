/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

/** @file include/ddl0impl-builder.h
 DDL index builder data interface.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_builder_h
#define ddl0impl_builder_h

#include "btr0load.h"
#include "ddl0impl-buffer.h"
#include "ddl0impl-file-reader.h"
#include "row0pread.h"

namespace ddl {

// Forward declaration.
struct Copy_ctx;
struct File_cursor;
class RTree_inserter;

/** For loading indexes. */
struct Builder {
  /** Build phase/states. */
  enum class State : uint8_t {
    /** Initial phase. */
    INIT,

    /** Collect the rows for the index to build. */
    ADD,

    /** Setup the merge sort and add the tasks to the task queue. */
    SETUP_SORT,

    /** Sort the collected rows, if required. The builder moves to state
    BTREE_BUILD after all sort tasks are completed successfully or there
    was an error during the sort phase. */
    SORT,

    /** Build the btree. */
    BTREE_BUILD,

    /** FTS sort and build, this is done in one "step" */
    FTS_SORT_AND_BUILD,

    /** Finish the loading of the index. */
    FINISH,

    /** Stop on success. */
    STOP,

    /** Stop on error. */
    ERROR
  };

  /** Constructor.
  @param[in,out] ctx            DDL context.
  @param[in,out] loader         Owner of the instance.
  @param[in] i                  Index ordinal value. */
  Builder(ddl::Context &ctx, Loader &loader, size_t i) noexcept;

  /** Destructor/ */
  ~Builder() noexcept;

  /** @return the error status. */
  dberr_t get_error() const noexcept { return m_ctx.get_error(); }

  /** Set the error code.
  @param[in] err                Error code to set. */
  void set_error(dberr_t err) noexcept { m_ctx.set_error(err, m_id); }

  /** @return the instance ID. */
  [[nodiscard]] size_t id() const noexcept { return m_id; }

  /** @return the index being built. */
  [[nodiscard]] dict_index_t *index() noexcept { return m_sort_index; }

  /** @return the DDL context. */
  Context &ctx() noexcept { return m_ctx; }

  /** Parallel scan thread spawn failed, release the extra thread states. */
  void fallback_to_single_thread() noexcept;

  /** @return true if the index is a spatial index. */
  [[nodiscard]] bool is_spatial_index() const noexcept {
    return dict_index_is_spatial(m_index);
  }

  /** @return true if the index is an FTS index. */
  [[nodiscard]] bool is_fts_index() const noexcept {
    return m_index->type & DICT_FTS;
  }

  /** @return true if the index is a unique index. */
  [[nodiscard]] bool is_unique_index() const noexcept {
    ut_a(!is_fts_index());
    return dict_index_is_unique(m_sort_index);
  }

  /** @return the current builder state. */
  [[nodiscard]] State get_state() const noexcept {
    return m_state.load(std::memory_order_seq_cst);
  }

  /** Set the next state.
  @param[in] state              State to set. */
  void set_state(State state) noexcept {
    m_state.store(state, std::memory_order_seq_cst);
  }

  /** @return the PFS instance that is used to report progress (or nullptr). */
  Alter_stage *stage() noexcept { return m_local_stage; }

  /** Set the next state. */
  void set_next_state() noexcept;

  /** Initialize the cursor.
  @param[in,out] cursor         Cursor to initialize.
  @param[in] n_threads          Number of threads used for reading.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t init(Cursor &cursor, size_t n_threads) noexcept;

  /** Add a row to the merge buffer.
  @param[in,out]        cursor        Current scan cursor.
  @param[in,out] row            Row to add.
  @param[in] thread_id          ID of current thread.
  @param[in,out] latch_release  Called when a log free check is required.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t add_row(Cursor &cursor, Row &row, size_t thread_id,
                                Latch_release &&latch_release) noexcept;

  /** @return true if file sorting can be skipped. */
  bool is_skip_file_sort() const noexcept {
    return m_ctx.m_skip_pk_sort && m_sort_index->is_clustered();
  }

  /** FTS: Sort and insert the rows read.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t fts_sort_and_build() noexcept;

  /** Non-FTS: Sort the rows read.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t setup_sort() noexcept;

  /** Non-FTS: Sort the rows read.
  @param[in] thread_id           Thread state ID.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t merge_sort(size_t thread_id) noexcept;

  /** Load the sorted data into the B+Tree.
  @return DB_SUCESS or error code. */
  [[nodiscard]] dberr_t btree_build() noexcept;

  /** Close temporary files, Flush all dirty pages, apply the row log
  and write the redo log record.
  @return DB_SUCCESS or error code. */
  dberr_t finish() noexcept;

  /** Copy blobs to the tuple.
  @param[out] dtuple            Tuple to copy to.
  @param[in,out] offsets        Column offsets in the row.
  @param[in] mrec               Current row.
  @param[in,out] heap           Heap for the allocating tuple memory.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t dtuple_copy_blobs(dtuple_t *dtuple, ulint *offsets,
                                          const mrec_t *mrec,
                                          mem_heap_t *heap) noexcept;

  /** Write data to disk - in append mode. Increment the file size.
  @param[in,out] file           File handle.
  @param[in] file_buffer        Write the buffer contents to disk.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t append(ddl::file_t &file,
                               IO_buffer file_buffer) noexcept;

  /** @return the path for temporary files. */
  const char *tmpdir() const noexcept { return m_tmpdir; }

  /** Insert cached rows.
  @param[in] thread_id          Insert cached rows for this thread ID.
  @param[in,out] latch_release  Called when a log free check is required.
  @return DB_SUCCESS or error number */
  [[nodiscard]] dberr_t batch_insert(size_t thread_id,
                                     Latch_release &&latch_release) noexcept;

  /** Note that the latches are going to be released. Do a deep copy of the
  tuples that are being inserted in batches by batch_insert
  @param[in] thread_id          Deep copy cached rows for this thread ID. */
  void batch_insert_deep_copy_tuples(size_t thread_id) noexcept;

  /** Check the state of the online build log for the index.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t check_state_of_online_build_log() noexcept;

  /** Write an MLOG_INDEX_LOAD record to indicate in the redo-log
  that redo-logging of individual index pages was disabled, and
  the flushing of such pages to the data files was completed.
  @param[in] index              Index on which redo logging was disabled */
  static void write_redo(const dict_index_t *index) noexcept;

 private:
  /** State of a cluster index reader thread. */
  struct Thread_ctx {
    /** Constructor.
    @param[in] id               Thread state ID.
    @param[in,out] key_buffer   Buffer for building the target index. Note, the
                                thread state will own the key buffer and is
                                responsible for deleting it. */
    explicit Thread_ctx(size_t id, Key_sort_buffer *key_buffer) noexcept;

    /** Destructor. */
    ~Thread_ctx() noexcept;

    /** Thread ID. */
    size_t m_id{};

    /** Key sort buffer. */
    Key_sort_buffer *m_key_buffer{};

    /** Total number of records added to the key sort buffer. */
    size_t m_n_recs{};

    /** Merge file handle. */
    ddl::file_t m_file{};

    /** Buffer to use for file writes. */
    ut::unique_ptr_aligned<byte[]> m_aligned_buffer{};

    /** Buffer to use for file writes. */
    IO_buffer m_io_buffer;

    /** Record list starting offset in the output file. */
    Merge_offsets m_offsets{};

    /** For spatial/Rtree rows handling. */
    RTree_inserter *m_rtree_inserter{};
  };

  using Allocator = ut::allocator<Thread_ctx *>;
  using Thread_ctxs = std::vector<Thread_ctx *, Allocator>;

  /** Create the tasks to merge Sort the file before we load the file into
  the Btree index.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t create_merge_sort_tasks() noexcept;

  /** Flush all dirty pages, apply the row log and write the redo log record.
  @return DB_SUCCESS or error code. */
  dberr_t finalize() noexcept;

  /** Convert the field data from compact to redundant format.
  @param[in]    clust_index           Clustered index being built
  @param[in]    row_field               Field to copy from
  @param[out]   field                     Field to copy to
  @param[in]    len                         Length of the field data
  @param[in]    page_size               Compressed BLOB page size
  @param[in]    is_sdi                  true for SDI Indexes
  @param[in,out]        heap                  Memory heap where to allocate
                                data when converting to ROW_FORMAT=REDUNDANT,
                                or nullptr */
  static void convert(const dict_index_t *clust_index,
                      const dfield_t *row_field, dfield_t *field, ulint len,
                      const page_size_t &page_size,
                      IF_DEBUG(bool is_sdi, ) mem_heap_t *heap) noexcept;

  /** Copy externally stored columns to the data tuple.
  @param[in] index              Index dictionary object.
  @param[in] mrec               Record containing BLOB pointers, or
                                nullptr to use tuple instead.
  @param[in] offsets            Offsets of mrec.
  @param[in] page_size          Compressed page size in bytes, or 0
  @param[in,out] tuple          Data tuple.
  @param[in] is_sdi             True for SDI Indexes
  @param[in,out] heap           Memory heap */
  static void copy_blobs(const dict_index_t *index, const mrec_t *mrec,
                         const ulint *offsets, const page_size_t &page_size,
                         dtuple_t *tuple,
                         IF_DEBUG(bool is_sdi, ) mem_heap_t *heap) noexcept;

  /** Cache a row for batch inserts. Currently used by spatial indexes.
  @param[in,out] row            Row to add.
  @param[in] thread_id          ID of current thread.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t batch_add_row(Row &row, size_t thread_id) noexcept;

  /** Add a row to the merge buffer.
  @param[in,out]        cursor        Current scan cursor.
  @param[in,out] row            Row to add.
  @param[in] thread_id          ID of current thread.
  @param[in,out] latch_release  Called when a log free check is required.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t bulk_add_row(Cursor &cursor, Row &row, size_t thread_id,
                                     Latch_release &&latch_release) noexcept;

  /** Clear the heap used for virtual columns. */
  void clear_virtual_heap() noexcept { m_v_heap.clear(); }

  /** Add the FTS document ID to the destination field.
  @param[in,out] dst            Field to write to.
  @param[in] src                Field to copy meta data from.
  @param[out] write_doc_id      Buffer for copying the doc id. */
  void fts_add_doc_id(dfield_t *dst, const dict_field_t *src,
                      doc_id_t &write_doc_id) noexcept;

  /** Add a row to the write buffer.
  @param[in,out] ctx             Copy context.
  @param[in,out] mv_rows_added   Number of multi-value rows added.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t copy_row(Copy_ctx &ctx, size_t &mv_rows_added) noexcept;

  /** Setup the virtual column src column.
  @param[in,out] ctx            Copy context.
  @param[in] ifield             Index field.
  @param[in] col                Table column.
  @param[out] src_field         Computed value.
  @param[in,out] mv_rows_added  Number of multi-value rows added.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t get_virtual_column(Copy_ctx &ctx,
                                           const dict_field_t *ifield,
                                           dict_col_t *col,
                                           dfield_t *&src_field,
                                           size_t &mv_rows_added) noexcept;

  /** Copy the FTS columns.
  @param[in,out] ctx            Copy context.
  @param[in,out] field          Field to write to.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t copy_fts_column(Copy_ctx &ctx,
                                        dfield_t *field) noexcept;

  /** Copy the columns to the temporary file buffer.
  @param[in,out] ctx            Copy context.
  @param[in,out] mv_rows_added  Multi value rows added.
  @param[in,out] write_doc_id   Buffer for storing the FTS doc ID.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t copy_columns(Copy_ctx &ctx, size_t &mv_rows_added,
                                     doc_id_t &write_doc_id) noexcept;

  /** Add row to the key buffer.
  @param[in,out] ctx            Copy context.
  @param[in,out] mv_rows_added  Number of multi-value index rows added.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t add_to_key_buffer(Copy_ctx &ctx,
                                          size_t &mv_rows_added) noexcept;

  /** Wait for FTS completion.
  @param[in] index             Index being built. */
  void fts_wait_for_completion(const dict_index_t *index) noexcept;

  /** Sort the data in the key buffer.
  @param[in] thread_id          Thread ID of current thread.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t key_buffer_sort(size_t thread_id) noexcept;

  /** Sort the buffer in memory and insert directly in the BTree loader,
  don't write to a temporary file.
  @param[in,out]        cursor        Current scan cursor.
  @param[in] thread_id          ID of current thread.
  @return DB_SUCCESS or error code. */
  dberr_t insert_direct(Cursor &cursor, size_t thread_id) noexcept;

  /** Create the merge file, if needed.
  @param[in,out] file           File handle.
  @return true if file was opened successfully . */
  [[nodiscard]] bool create_file(ddl::file_t &file) noexcept;

  /** Check for duplicates in the first block
  @param[in] dupcheck           Files to check for duplicates.
  @param[in,out] dup            For collecting duplicate key information.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t check_duplicates(Thread_ctxs &dupcheck,
                                         Dup *dup) noexcept;

  /** Cleanup DDL after error in online build
  Note: To be called if DDL must cleanup due to error in online build. Pages
  which are buffer-fixed (in Page_load::release) until the next iteration, must
  be unfixed (with Page_load::latch) before returning the error.
  @note: Assumes that either m_btr_load->release is called before or
  m_n_recs is 0 (no records are inserted yet).
  @param[in]  err    Error hit in online build
  @return the cursor error status. */
  [[nodiscard]] dberr_t online_build_handle_error(dberr_t err) noexcept;

 private:
  /** Buffer ID. */
  size_t m_id{};

  /** Initial phase. */
  std::atomic<State> m_state{State::INIT};

  /** DDL Context. */
  ddl::Context &m_ctx;

  /** Loader that owns the instance. */
  Loader &m_loader;

  /** Index to create (if not FTS index). */
  dict_index_t *m_index{};

  /** Temporary file path. */
  const char *m_tmpdir{};

  /** Per thread context. */
  Thread_ctxs m_thread_ctxs{};

  /** For tracking duplicates. */
  dfield_t *m_prev_fields{};

  /** For collecting duplicate entries (error reporting). */
  Dup m_clust_dup{};

  /** Scoped virtual column heap. */
  Scoped_heap m_v_heap{};

  /** Scoped conversion heap. */
  Scoped_heap m_conv_heap{};

  /** The index to be built, FTS or non-FTS. */
  dict_index_t *m_sort_index{};

  /** Number of active sort tasks. */
  std::atomic<size_t> m_n_sort_tasks{};

  /** Cluster index bulk load instance to use, direct insert without
  a file sort. */
  Btree_load *m_btr_load{};

  /** Stage per builder. */
  Alter_stage *m_local_stage{};
};

struct Load_cursor : Btree_load::Cursor {
  /** Default constructor. */
  explicit Load_cursor(Builder *builder, Dup *dup) noexcept
      : m_dup(dup), m_builder(builder) {}

  /** Default destructor. */
  virtual ~Load_cursor() override = default;

  /** @return the cursor error status. */
  [[nodiscard]] dberr_t get_err() const noexcept { return m_err; }

  /** @return true if duplicates detected. */
  [[nodiscard]] bool duplicates_detected() const noexcept override;

  /** Duplicate checking and reporting. */
  Dup *m_dup{};

  /** Operation error code. */
  dberr_t m_err{DB_SUCCESS};

  /** Index meta data. */
  Builder *m_builder{};

  /** Heap for the raw row to dtuple_t conversion. */
  Scoped_heap m_tuple_heap{};
};

/** Merge the sorted files. */
struct Merge_cursor : public Load_cursor {
  /** File cursors to use for the scan. */
  using File_readers = std::vector<File_reader *, ut::allocator<File_reader *>>;

  /** Constructor.
  @param[in,out] builder        Index builder.
  @param[in,out] dup            If not nullptr, then report duplicates.
  @param[in,out] stage          PFS stage monitoring. */
  explicit Merge_cursor(Builder *builder, Dup *dup,
                        Alter_stage *stage) noexcept;

  /** Destructor. */
  ~Merge_cursor() noexcept override;

  /** Add the cursor to use for merge load.
  @param[in] file               File to merge from.
  @param[in] buffer_size        IO buffer size to use for reading.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t add_file(const ddl::file_t &file,
                                 size_t buffer_size) noexcept;

  /** Add the cursor to use for merge load.
  @param[in] file               File file to read.
  @param[in] buffer_size        IO buffer size to use for reading.
  @param[in] range              Range to read from
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t add_file(const ddl::file_t &file, size_t buffer_size,
                                 const Range &range) noexcept;

  /** Open the cursor.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t open() noexcept;

  /** Fetch the current row as a tuple. Note: Tuple columns are shallow copies.
  @param[out] dtuple            Row represented as a tuple.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t fetch(dtuple_t *&dtuple) noexcept override;

  /** Fetch the current row.
  @param[out] mrec              Current merge record.
  @param[out] offsets           Columns offsets inside mrec.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t fetch(const mrec_t *&mrec, ulint *&offsets) noexcept;

  /** Move to the next record.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t next() noexcept override;

  /** @return the file reader instances. */
  [[nodiscard]] File_readers file_readers() noexcept;

  /** Add the active cursors to the priority queue. */
  void clear_eof() noexcept;

  /** @return the number of active readers. */
  [[nodiscard]] size_t size() const noexcept { return m_pq.size(); }

  /** @return the number of rows read from the files. */
  [[nodiscard]] uint64_t get_n_rows() const noexcept;

  /** @return the number of cursors being merged. */
  [[nodiscard]] size_t number_of_cursors() const noexcept {
    return m_cursors.size();
  }

 private:
  /** @return the current cursor at the head of the queue. */
  [[nodiscard]] File_cursor *pop() noexcept;

 private:
  /** Comparator. */
  struct Compare {
    /** Constructor.
    @param[in] index            Index that the rows belong to.
    @param[in,out] dup          For reporting duplicates, can be nullptr.  */
    explicit Compare(const dict_index_t *index, Dup *dup)
        : m_dup(dup), m_index(index) {}

    /** Destructor. */
    Compare() = default;

    /** Compare the keys of two cursors.
    @param[in] lhs              Left hand side.
    @param[in] rhs              Right hand side.
    @return true if lhs strictly less than rhs. */
    bool operator()(const File_cursor *lhs,
                    const File_cursor *rhs) const noexcept;

    /** For reporting duplicates. */
    Dup *m_dup{};

    /** Index being built. */
    const dict_index_t *m_index{};
  };

  /** File cursors to use for the scan. */
  using File_cursors = std::vector<File_cursor *, ut::allocator<File_cursor *>>;

  /** Priority queue for ordering the rows. */
  using Queue = std::priority_queue<File_cursor *, File_cursors, Compare>;

  /** Priority queue for merging the file cursors. */
  Queue m_pq{};

  /** Cursors to use for parallel loading of the index. */
  File_cursors m_cursors{};

  /** Current cursor. */
  File_cursor *m_cursor{};

  /** PFS stage monitoring. */
  Alter_stage *m_stage{};
};

}  // namespace ddl

#endif /* !ddl0impl_builder_h */

/*****************************************************************************

Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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

/** @file include/ddl0ddl.h
 DDL context */

#ifndef ddl0ddl_h
#define ddl0ddl_h

#include "fts0fts.h"
#include "lock0types.h"
#include "os0file.h"
#include "ut0class_life_cycle.h"

// Forward declaration
class Flush_observer;
class Alter_stage;

namespace ddl {
// Forward declaration
struct Dup;
struct Row;
struct FTS;
class Loader;
struct Cursor;
struct Context;
struct Builder;
struct Sequence;
struct Merge_file_sort;
struct Load_cursor;
struct Btree_cursor;
struct Parallel_cursor;

/** Innodb B-tree index fill factor for bulk load. */
extern long fill_factor;

/** Variable specifying the number of FTS parser threads to use. */
extern ulong fts_parser_threads;

/** Minimum IO buffer size. */
constexpr size_t IO_BLOCK_SIZE = 4 * 1024;

/** @brief Secondary buffer for I/O operations of merge records.

This buffer is used for writing or reading a record that spans two
Aligned_buffer.  Thus, it must be able to hold one merge record,
whose maximum size is the same as the minimum size of Aligned_buffer. */
using mrec_buf_t = byte[UNIV_PAGE_SIZE_MAX];

/** @brief Merge record in Aligned_buffer.

The format is the same as a record in ROW_FORMAT=COMPACT with the
exception that the REC_N_NEW_EXTRA_BYTES are omitted. */
using mrec_t = byte;

/** Index field definition */
struct Index_field {
  /** Column offset */
  size_t m_col_no{};

  /** Column prefix length, or 0 if indexing the whole column */
  size_t m_prefix_len{};

  /** Whether this is a virtual column */
  bool m_is_v_col{};

  /** Whether it has multi-value */
  bool m_is_multi_value{};

  /** true=ASC, false=DESC */
  bool m_is_ascending{};
};

/** Definition of an index being created */
struct Index_defn {
  /** Index name */
  const char *m_name{};

  /** Whether the table is rebuilt */
  bool m_rebuild{};

  /** 0, DICT_UNIQUE, or DICT_CLUSTERED */
  size_t m_ind_type{};

  /** MySQL key number, or ULINT_UNDEFINED if none */
  size_t m_key_number{ULINT_UNDEFINED};

  /** Number of fields in index */
  size_t m_n_fields{};

  /** Field definitions */
  Index_field *m_fields{};

  /** Fulltext parser plugin */
  st_mysql_ftparser *m_parser{};

  /** true if it's ngram parser */
  bool m_is_ngram{};

  /** true if we want to check SRID while inserting to index */
  bool m_srid_is_valid{};

  /** SRID obtained from dd column */
  uint32_t m_srid{};
};

/** Structure for reporting duplicate records. */
struct Dup {
  /** Report a duplicate key.
  @param[in] entry              For reporting duplicate key. */
  void report(const dfield_t *entry) noexcept;

  /** Report a duplicate key.
  @param[in] entry              For reporting duplicate key.
  @param[in] offsets            Row offsets */
  void report(const mrec_t *entry, const ulint *offsets) noexcept;

  /** @return true if no duplicates reported yet. */
  [[nodiscard]] bool empty() const noexcept { return m_n_dup == 0; }

  /** Index being sorted */
  dict_index_t *m_index{};

  /** MySQL table object */
  TABLE *m_table{};

  /** Mapping of column numbers in table to the rebuilt table
  (index->table), or NULL if not rebuilding table */
  const ulint *m_col_map{};

  /** Number of duplicates */
  size_t m_n_dup{};
};

/** Captures ownership and manages lifetime of an already opened OS file
descriptor. Closes the file on object destruction. */
class Unique_os_file_descriptor : private ut::Non_copyable {
 public:
  /** Default constructor, does not hold any file, does not close any on
  destruction. */
  Unique_os_file_descriptor() = default;
  /** Main constructor capturing an already opened OS file descriptor. */
  Unique_os_file_descriptor(os_fd_t fd) : m_fd(fd) {}

  Unique_os_file_descriptor(Unique_os_file_descriptor &&other) { swap(other); }

  ~Unique_os_file_descriptor() { close(); }

  /** Returns the managed OS file descriptor for use with OS functions that
  operate on file. Do not close this file. */
  os_fd_t get() const {
    ut_a(is_open());
    return m_fd;
  }
  bool is_open() const { return m_fd != OS_FD_CLOSED; }

  Unique_os_file_descriptor &operator=(Unique_os_file_descriptor &&other) {
    close();
    swap(other);
    return *this;
  }

  /** Swaps the underlying managed file descriptors between two instances of
  Unique_os_file_descriptor. No files are closed. */
  void swap(Unique_os_file_descriptor &other) { std::swap(m_fd, other.m_fd); }

  /** Closes the managed file. Leaves the instance in the same state as default
  constructed instance. */
  void close() {
#ifdef UNIV_PFS_IO
    struct PSI_file_locker *locker = nullptr;
    PSI_file_locker_state state;
    locker = PSI_FILE_CALL(get_thread_file_descriptor_locker)(&state, m_fd,
                                                              PSI_FILE_CLOSE);
    if (locker != nullptr) {
      PSI_FILE_CALL(start_file_wait)
      (locker, 0, __FILE__, __LINE__);
    }
#endif /* UNIV_PFS_IO */
    if (m_fd != OS_FD_CLOSED) {
      ::close(m_fd);
      m_fd = OS_FD_CLOSED;
    }
#ifdef UNIV_PFS_IO
    if (locker != nullptr) {
      PSI_FILE_CALL(end_file_wait)(locker, 0);
    }
#endif /* UNIV_PFS_IO */
  }

 private:
  os_fd_t m_fd{OS_FD_CLOSED};
};

/** Sets an exclusive lock on a table, for the duration of creating indexes.
@param[in,out] trx              Transaction
@param[in] table                Table to lock.
@param[in] mode                 Lock mode LOCK_X or LOCK_S
@return error code or DB_SUCCESS */
[[nodiscard]] dberr_t lock_table(trx_t *trx, dict_table_t *table,
                                 lock_mode mode) noexcept;

/** Drop those indexes which were created before an error occurred.
The data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed.
@param[in,out] trx              Transaction
@param[in] table                Table to lock.
@param[in] locked               true=table locked, false=may need to do a lazy
                                drop */
void drop_indexes(trx_t *trx, dict_table_t *table, bool locked) noexcept;

/**Create temporary merge files in the given parameter path, and if
UNIV_PFS_IO defined, register the file descriptor with Performance Schema.
@param[in] path                 Location for creating temporary merge files.
@return File descriptor */
[[nodiscard]] Unique_os_file_descriptor file_create_low(
    const char *path) noexcept;

/** Create the index and load in to the dictionary.
@param[in,out] trx              Trx (sets error_state)
@param[in,out] table            The index is on this table
@param[in] index_def            The index definition
@param[in] add_v                New virtual columns added along with add
                                index call
@return index, or nullptr on error */
[[nodiscard]] dict_index_t *create_index(
    trx_t *trx, dict_table_t *table, const Index_defn *index_def,
    const dict_add_v_col_t *add_v) noexcept;

/** Drop a table. The caller must have ensured that the background stats
thread is not processing the table. This can be done by calling
dict_stats_wait_bg_to_stop_using_table() after locking the dictionary and
before calling this function.
@param[in,out] trx              Transaction
@param[in,out] table            Table to drop.
@return DB_SUCCESS or error code */
dberr_t drop_table(trx_t *trx, dict_table_t *table) noexcept;

/** Generate the next autoinc based on a snapshot of the session
auto_increment_increment and auto_increment_offset variables.
Assignment operator would be used during the inplace_alter_table()
phase only **/
struct Sequence {
  /** Constructor.
  @param[in,out]  thd           The session
  @param[in] start_value        The lower bound
  @param[in] max_value          The upper bound (inclusive) */
  Sequence(THD *thd, ulonglong start_value, ulonglong max_value) noexcept;

  /** Destructor. */
  ~Sequence() = default;

  /** Postfix increment
  @return the value to insert */
  ulonglong operator++(int) noexcept;

  /** Check if the autoinc "sequence" is exhausted.
  @return true if the sequence is exhausted */
  bool eof() const noexcept { return m_eof; }

  /** Assignment operator to copy the sequence values
  @param[in] rhs                Sequence to copy from */
  ddl::Sequence &operator=(const ddl::Sequence &rhs) noexcept {
    ut_ad(rhs.m_next_value > 0);
    ut_ad(rhs.m_max_value == m_max_value);
    m_next_value = rhs.m_next_value;
    m_increment = rhs.m_increment;
    m_offset = rhs.m_offset;
    m_eof = rhs.m_eof;
    return *this;
  }

  /** @return the next value in the sequence */
  ulonglong last() const noexcept {
    ut_ad(m_next_value > 0);
    return m_next_value;
  }

  /** Maximum column value if adding an AUTOINC column else 0. Once
  we reach the end of the sequence it will be set to ~0. */
  const ulonglong m_max_value{};

  /** Value of auto_increment_increment */
  ulong m_increment{};

  /** Value of auto_increment_offset */
  ulong m_offset{};

  /** Next value in the sequence */
  ulonglong m_next_value{};

  /** true if no more values left in the sequence */
  bool m_eof{};
};

/** DDL context/configuration. */
struct Context {
  /** Full text search context information and state. */
  struct FTS {
    /** Document ID sequence */
    struct Sequence {
      /** Destructor. */
      virtual ~Sequence() noexcept;

      /** Get the next document ID.
      @param[in] dtuple         Row from which to fetch ID.
      @return the next document ID. */
      virtual doc_id_t fetch(const dtuple_t *dtuple = nullptr) noexcept = 0;

      /** Get the current document ID.
      @return the current document ID. */
      virtual doc_id_t current() noexcept = 0;

      /** @return the number of document IDs generated. */
      virtual doc_id_t generated_count() const noexcept = 0;

      /** @return the maximum document ID seen so far. */
      virtual doc_id_t max_doc_id() const noexcept = 0;

      /** @return true if the document ID is generated, instead of fetched
                  from a column from the row. */
      virtual bool is_generated() const noexcept = 0;

      /** Advance the document ID. */
      virtual void increment() noexcept = 0;

      /** Current document ID. */
      doc_id_t m_doc_id{};
    };

    /** Constructor.
    @param[in] n_parser_threads Number of FTS parser threads. */
    explicit FTS(size_t n_parser_threads) noexcept
        : m_n_parser_threads(n_parser_threads) {}

    /** Destructor. */
    ~FTS() noexcept { ut::delete_(m_doc_id); }

    /** FTS index. */
    dict_index_t *m_index{};

    /** Maximum number of FTS parser and sort threads to use. */
    const size_t m_n_parser_threads{};

    /** Document ID sequence generator. */
    Sequence *m_doc_id{};

    /** FTS instance. */
    ddl::FTS *m_ptr{};
  };

  /** Scan sort and IO buffer size. */
  using Scan_buffer_size = std::pair<size_t, size_t>;

  /** Build indexes on a table by reading a clustered index, creating a
  temporary file containing index entries, merge sorting these index entries and
  inserting sorted index entries to indexes.
  @param[in] trx                Transaction.
  @param[in] old_table          Table where rows are read from
  @param[in] new_table          Table where indexes are created; identical to
                                old_table unless creating a PRIMARY KEY
  @param[in] online             True if creating indexes online
  @param[in] indexes            Indexes to be created
  @param[in] key_numbers        MySQL key numbers
  @param[in] n_indexes          Size of indexes[]
  @param[in,out] table          MySQL table, for reporting erroneous key
                                value if applicable
  @param[in] add_cols           Default values of added columns, or NULL
  @param[in] col_map            Mapping of old column numbers to new
                                ones, or nullptr if old_table == new_table
  @param[in] add_autoinc        Number of added AUTO_INCREMENT columns, or
                                ULINT_UNDEFINED if none is added
  @param[in,out] sequence       Autoinc sequence
  @param[in] skip_pk_sort       Whether the new PRIMARY KEY will follow
                                existing order
  @param[in,out] stage          Performance schema accounting object,
                                used by ALTER TABLE.
                                stage->begin_phase_read_pk() will be called
                                at the beginning of this function and it will
                                be passed to other functions for further
                                accounting.
  @param[in] add_v              New virtual columns added along with indexes
  @param[in] eval_table         MySQL table used to evaluate virtual column
                                value, see innobase_get_computed_value().
  @param[in] max_buffer_size    Memory use upper limit.
  @param[in] max_threads        true if DDL should use multiple threads. */
  Context(trx_t *trx, dict_table_t *old_table, dict_table_t *new_table,
          bool online, dict_index_t **indexes, const ulint *key_numbers,
          size_t n_indexes, TABLE *table, const dtuple_t *add_cols,
          const ulint *col_map, size_t add_autoinc, ddl::Sequence &sequence,
          bool skip_pk_sort, Alter_stage *stage, const dict_add_v_col_t *add_v,
          TABLE *eval_table, size_t max_buffer_size,
          size_t max_threads) noexcept;

  /** Destructor. */
  ~Context() noexcept;

  /** @return the DDL error status. */
  dberr_t get_error() const noexcept { return m_err; }

  /** Set the error code, when it's not specific to an index.
  @param[in] err                Error code. */
  void set_error(dberr_t err) noexcept {
    ut_a(err != DB_SUCCESS && err != DB_END_OF_INDEX);

    /* This should only be settable by the the thread that encounters the
    first error, therefore try only once. */

    dberr_t expected{DB_SUCCESS};
    m_err.compare_exchange_strong(expected, err);
  }

  /** Set the error code and index number where the error occurred.
  @param[in] err                Error code.
  @param[in] id                 Index ordinal value where error occurred. */
  void set_error(dberr_t err, size_t id) noexcept {
    /* This should only be settable by the the thread that encounters the
    first error, therefore try only once. */

    ut_a(err != DB_SUCCESS);

    dberr_t expected{DB_SUCCESS};

    if (m_err.compare_exchange_strong(expected, err)) {
      ut_a(m_err_key_number == std::numeric_limits<size_t>::max());
      m_err_key_number = m_key_numbers[id];
    }
  }

  /** Build the indexes.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build() noexcept;

  /** @return the flush observer to use for flushing. */
  [[nodiscard]] Flush_observer *flush_observer() noexcept;

  /** @return the old table. */
  [[nodiscard]] dict_table_t *old_table() noexcept { return m_old_table; }

  /** @return the new table. */
  [[nodiscard]] dict_table_t *new_table() noexcept { return m_new_table; }

  /** Calculate the sort and  buffer size per thread.
  @param[in] n_threads          Total number of threads used for scanning.
  @return the sort and IO buffer size per thread. */
  [[nodiscard]] Scan_buffer_size scan_buffer_size(
      size_t n_threads) const noexcept;

  /** Calculate the io buffer size per file for the sort phase.
  @param[in] n_buffers          Total number of buffers to use for the merge.
  @return the sort buffer size for one instance. */
  [[nodiscard]] size_t merge_io_buffer_size(size_t n_buffers) const noexcept;

  /** Calculate the io buffer size per file for the load phase.
  @param[in] n_buffers          Total number of buffers to use for the loading.
  @return the per thread io buffer size. */
  [[nodiscard]] size_t load_io_buffer_size(size_t n_buffers) const noexcept;

  /** Request number of bytes for a buffer.
  @param[in] n                  Number of bytes requested.
  @return the number of bytes available. */
  [[nodiscard]] size_t allocate(size_t n) const;

  /** @return the server session/connection context. */
  [[nodiscard]] THD *thd() noexcept;

  /** Copy the added columns dtuples so that we don't use the same
  column data buffer for the added column across multiple threads.
  @return new instance or nullptr if out of memory. */
  [[nodiscard]] dtuple_t *create_add_cols() noexcept;

 private:
  /** @return the cluster index read cursor. */
  [[nodiscard]] Cursor *cursor() noexcept { return m_cursor; }

  /** @return the original table cluster index. */
  [[nodiscard]] const dict_index_t *index() const noexcept;

  /** Initialize the context for a cluster index scan.
  @param[in,out] cursor         Cursor used for the cluster index read. */
  [[nodiscard]] dberr_t read_init(Cursor *cursor) noexcept;

  /** Initialize the FTS build infrastructure.
  @param[in,out] index          Index prototype to build.
  @return DB_SUCCESS or error code. */
  dberr_t fts_create(dict_index_t *index) noexcept;

  /** Setup the FTS index build data structures.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t setup_fts_build() noexcept;

  /** Get the next Doc ID and increment the current value.
  @return a document ID. */
  [[nodiscard]] doc_id_t next_doc_id() noexcept;

  /** Update the FTS document ID. */
  void update_fts_doc_id() noexcept;

  /** Check the state of the online build log for the index.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t check_state_of_online_build_log() noexcept;

  /** Track the highest TxID that modified this index when the scan
  was completed. We prevent older readers from accessing this index, to
  ensure read consistency.
  @param[in,out] index          Index to track. */
  void note_max_trx_id(dict_index_t *index) noexcept;

  /** Setup the primary key sort.
  @param[in,out] cursor         Setup the primary key data structures.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t setup_pk_sort(Cursor *cursor) noexcept;

  /** Init the non-null column constraints checks (if required). */
  void setup_nonnull() noexcept;

  /** Check if the nonnull columns satisfy the constraint.
  @param[in] row                Row to check.
  @return true on success. */
  [[nodiscard]] bool check_null_constraints(const dtuple_t *row) const noexcept;

  /** Clean up the data structures at the end of the DDL.
  @param[in] err                Status of the DDL.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t cleanup(dberr_t err) noexcept;

  /** Handle auto increment.
  @param[in] row                Row with autoinc column.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t handle_autoinc(const dtuple_t *row) noexcept;

  /** @return true if any virtual columns are involved. */
  [[nodiscard]] bool has_virtual_columns() const noexcept;

  /** @return true if any FTS indexes are involved. */
  [[nodiscard]] bool has_fts_indexes() const noexcept;

  /** @return true if the DDL was interrupted. */
  [[nodiscard]] bool is_interrupted() noexcept;

 private:
  using Key_numbers = std::vector<size_t, ut::allocator<size_t>>;
  using Indexes = std::vector<dict_index_t *, ut::allocator<dict_index_t *>>;

  /** Common error code for all index builders running in parallel. */
  std::atomic<dberr_t> m_err{DB_SUCCESS};

  /** Index where the error occurred. */
  size_t m_err_key_number{std::numeric_limits<size_t>::max()};

  /** Transaction covering the index build. */
  trx_t *m_trx{};

  /** The FTS builder. There is one FTS per table. */
  FTS m_fts;

  /** Source table, read rows from this table. */
  dict_table_t *m_old_table{};

  /** Table where indexes are created; identical to old_table unless creating
  a PRIMARY KEY. */
  dict_table_t *m_new_table{};

  /** True if creating index online. Non-online implies that we have an
  S latch on the table, therefore there can't be concurrent updates to
  the table while we are executing the DDL. We don't log the changes to
  the row log. */
  bool m_online{};

  /** Indexes to be created. */
  Indexes m_indexes{};

  /** MySQL key numbers. */
  Key_numbers m_key_numbers{};

  /** MySQL table for reporting errors/warnings. */
  TABLE *m_table{};

  /** Default value for added columns or null. */
  const dtuple_t *m_add_cols{};

  /** Mapping of old column numbers to new ones, or nullptr if none
  were added. */
  const ulint *m_col_map{};

  /** Number of added AUTO_INCREMENT columns, or ULINT_UNDEFINED if
  none added. */
  size_t m_add_autoinc{ULINT_UNDEFINED};

  /** Autoinc sequence. */
  ddl::Sequence &m_sequence;

  /** Performance schema accounting object, used by ALTER TABLE.
  stage->begin_phase_read_pk() will be called at the beginning of
  this function and it will be passed to other functions for further
  accounting. */
  Alter_stage *m_stage{};

  /** New virtual columns added along with indexes */
  const dict_add_v_col_t *m_add_v{};

  /** MySQL table used to evaluate virtual column value, see
  innobase_get_computed_value(). */
  TABLE *m_eval_table{};

  /** Skip the sorting phase if true. */
  bool m_skip_pk_sort{};

  /** Non null columns. */
  std::vector<size_t, ut::allocator<size_t>> m_nonnull{};

  /** Number of unique columns in the key. */
  size_t m_n_uniq{};

  /** true if need flush observer. */
  bool m_need_observer{};

  /** Cursor for reading the cluster index. */
  Cursor *m_cursor{};

  /** Number of bytes used. */
  size_t m_n_allocated{};

  /** Maximum number of bytes to use. */
  const size_t m_max_buffer_size{};

  /** Maximum number of threads to use. We don't do a parallel scan of the
  clustered index when FTS and/or virtual columns are involved. The build
  phase is parallel though. */
  const size_t m_max_threads{};

  /** For parallel access to the autoincrement generator. */
  ib_mutex_t m_autoinc_mutex;

  /** Heap for copies of m_add_cols. */
  mem_heap_t *m_dtuple_heap{};

  friend struct Row;
  friend class Loader;
  friend struct Cursor;
  friend struct Builder;
  friend struct ddl::FTS;
  friend struct Load_cursor;
  friend struct Btree_cursor;
  friend struct Merge_file_sort;
  friend struct Parallel_cursor;
};

}  // namespace ddl

#endif /* ddl0ddl_h */

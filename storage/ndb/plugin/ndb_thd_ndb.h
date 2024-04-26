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

#ifndef NDB_THD_NDB_H
#define NDB_THD_NDB_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "storage/ndb/plugin/ndb_share.h"
#include "storage/ndb/plugin/ndb_thd.h"

class THD;
class Relay_log_info;
class Ndb_applier;

/*
  Class for ndbcluster thread specific data
*/
class Thd_ndb {
  THD *const m_thd;

  Thd_ndb(THD *thd, const char *name);
  ~Thd_ndb();

  uint32 options;
  uint32 trans_options;
  class Ndb_DDL_transaction_ctx *m_ddl_ctx;
  // Thread name that owns this connection (usually the PFS thread name)
  const char *const m_thread_name;

 public:
  static Thd_ndb *seize(THD *thd, const char *name = nullptr);
  static void release(Thd_ndb *thd_ndb);

  // Keeps track of stats for tables taking part in transaction
  class Trans_tables {
   public:
    struct Stats {
     private:
      static constexpr uint64_t INVALID_TABLE_ROWS = ~0;

     public:
      int uncommitted_rows{0};
      uint64_t table_rows{INVALID_TABLE_ROWS};

      // Check if stats are invalid, i.e has never been updated with number of
      // rows in the table (from NDB or other (potentially cached) source)
      bool invalid() const { return table_rows == INVALID_TABLE_ROWS; }

      void update_uncommitted_rows(int changed_rows) {
        DBUG_TRACE;
        uncommitted_rows += changed_rows;
        DBUG_PRINT("info", ("changed_rows: %d -> new value: %d", changed_rows,
                            uncommitted_rows));
      }
    };
    void clear();
    Stats *register_stats(NDB_SHARE *share);
    void reset_stats();
    void update_cached_stats_with_committed();

   private:
    std::unordered_map<NDB_SHARE *, Stats> m_map;
    void dbug_print_elem(const std::pair<NDB_SHARE *, Stats> &elem,
                         bool check_reset) const;
    void dbug_print(bool check_reset = false) const;
  } trans_tables;

  class Ndb_cluster_connection *connection;
  class Ndb *ndb;
  class ha_ndbcluster *m_handler;

  // Reference counter for external_lock() calls. The counter controls that
  // the handlerton is registered as being part of the MySQL transaction only at
  // the first external_lock() call (when the counter is zero). Also the counter
  // controls that any started NDB transaction is closed when external_lock(..,
  // F_UNLOCK) is called for the last time (i.e when the counter is back to zero
  // again).
  uint external_lock_count{0};

  // Reference counter for start_stmt() calls. The counter controls that the
  // handlerton is registered as being part of the MySQL transaction at the
  // first start_stmt() call (when the counter is zero). When MySQL
  // ends the transaction by calling either ndbcluster_commit() or
  // ndbcluster_rollback(), the counter is then reset back to zero.
  uint start_stmt_count{0};

  uint save_point_count;
  class NdbTransaction *trans;
  bool m_force_send;

  enum Options {
    /*
      Don't distribute schema operations for this thread.
      NOTE! Flag is _only_ set by the binlog injector thread and thus
      any DDL operations it performs are not distributed.
    */
    NO_LOG_SCHEMA_OP = 1 << 0,
    /*
      This Thd_ndb is a participant in a global schema distribution.
      Whenever a GSL lock is required, it is acquired by the coordinator.
      The participant can then assume that the GSL lock is already held
      for the schema operation it is part of. Thus it should not take
      any GSL locks itself.
    */
    IS_SCHEMA_DIST_PARTICIPANT = 1 << 1,

    /*
      Allow Thd_ndb to setup schema distribution and apply status
     */
    ALLOW_BINLOG_SETUP = 1 << 2,

    /*
       Create a ndbcluster util table in DD. The table already exists
       in NDB and thus some functions need to return early in order to hide
       the table. This allows using SQL to install the table definition in DD.
    */
    CREATE_UTIL_TABLE = 1 << 3,

    /*
       Mark the util table as hidden when installing the table definition
       in DD. By marking the table as hidden it will not be available for either
       DML or DDL.
    */
    CREATE_UTIL_TABLE_HIDDEN = 1 << 4
  };

  // Check if given option is set
  bool check_option(Options option) const;
  // Set given option
  void set_option(Options option);

  // Guard class for automatically restoring the state of
  // Thd_ndb::options when the guard goes out of scope
  class Options_guard {
    Thd_ndb *const m_thd_ndb;
    const uint32 m_save_options;

   public:
    Options_guard(Thd_ndb *thd_ndb)
        : m_thd_ndb(thd_ndb), m_save_options(thd_ndb->options) {
      assert(sizeof(m_save_options) == sizeof(thd_ndb->options));
    }
    ~Options_guard() {
      // Restore the saved options
      m_thd_ndb->options = m_save_options;
    }
    void set(Options option) { m_thd_ndb->set_option(option); }
  };

  enum Trans_options {
    /*
       Indicates that no logging is performed by this MySQL Server and thus
       the anyvalue should have the nologging bit turned on
    */
    TRANS_NO_LOGGING = 1 << 1,

    /*
       Turn off transactional behaviour for the duration
       of this transaction/statement
    */
    TRANS_TRANSACTIONS_OFF = 1 << 2
  };

  // Check if given trans option is set
  bool check_trans_option(Trans_options trans_option) const;
  // Set given trans option
  void set_trans_option(Trans_options trans_option);
  // Reset all trans_options
  void reset_trans_options(void);

  // Start of transaction check, to automatically detect which
  // trans options should be enabled
  void transaction_checks(void);

 private:
  // Threshold for when to flush a batch. Configured from @@ndb_batch_size or
  // --ndb-replica-batch-size when transaction starts.
  uint m_batch_size;

  // The size in bytes to use when batching blob writes. Configured from
  // @@ndb_blob_write_batch_bytes or
  // --ndb-replica-blob-write-batch-bytes when transaction starts.
  uint m_blob_write_batch_size;

 public:
  // Return the configured value for blob write batch size
  uint get_blob_write_batch_size() const {
    assert(trans);  // assume trans has been started
    return m_blob_write_batch_size;
  }

 private:
  // Block size for the batch memroot. First block will be allocated
  // with this size, subsequent blocks will be 50% larger each time.
  static constexpr size_t BATCH_MEM_ROOT_BLOCK_SIZE = 8192;

  /*
    Memroot used for batched execution, it contains rows as well as
    other control structures that need to be kept alive until next execute().
    The allocated memory is then reset before next execute().
  */
  MEM_ROOT m_batch_mem_root;

 public:
  /*
    Return a generic buffer that will remain valid until after next execute.

    The memory is freed by the first call to add_row_check_if_batch_full()
    following any execute() call. The intention is that the memory is associated
    with one batch of operations during batched updates.

    Note in particular that using get_buffer() / copy_to_batch_mem() separately
    from add_row_check_if_batch_full() could make memory usage grow without
    limit, and that this sequence:

      execute()
      get_buffer() / copy_to_batch_mem()
      add_row_check_if_batch_full()
      ...
      execute()

    will free the memory already at add_row_check_if_batch_full() time, it
    will not remain valid until the second execute().
  */
  uchar *get_buffer(size_t size) {
    // Allocate buffer memory from batch MEM_ROOT
    return static_cast<uchar *>(m_batch_mem_root.Alloc(size));
  }

  /*
    Copy data of given size into the batch memroot.
    Return pointer to copy
  */
  uchar *copy_to_batch_mem(const void *data, size_t size) {
    uchar *row = get_buffer(size);
    if (unlikely(!row)) {
      return nullptr;
    }
    memcpy(row, data, size);
    return row;
  }

  // Number of unsent bytes in the transaction
  ulong m_unsent_bytes;
  // Flag for unsent blobs in the transaction
  bool m_unsent_blob_ops;

  bool add_row_check_if_batch_full(uint row_size);

  uint m_execute_count;

  uint m_scan_count;
  uint m_pruned_scan_count;

  /** This is the number of sorted scans (via ordered indexes).*/
  uint m_sorted_scan_count;

  /** This is the number of NdbQueryDef objects that the handler has created.*/
  uint m_pushed_queries_defined;
  /**
    This is the number of cases where the handler decided not to use an
    NdbQuery object that it previously created for executing a particular
    instance of a query fragment. This may happen if for examle the optimizer
    decides to use another type of access operation than originally assumed.
  */
  uint m_pushed_queries_dropped;
  /**
    This is the number of times that the handler instantiated an NdbQuery object
    from a NdbQueryDef object and used the former for executing an instance
    of a query fragment.
   */
  uint m_pushed_queries_executed;
  /**
    This is the number of lookup operations (via unique index or primary key)
    that were eliminated by pushing linked operations (NdbQuery) to the data
    nodes.
   */
  uint m_pushed_reads;

  /**
    The number of hinted transactions started by this thread. Using hinted
    transaction is normally more efficient as it tries to select a transaction
    coordinator close to the data, in most cases on the node where the primary
    replica of the data resides.
  */
 private:
  uint m_hinted_trans_count{0};

 public:
  void increment_hinted_trans_count() { m_hinted_trans_count++; }
  uint hinted_trans_count() const { return m_hinted_trans_count; }

  // Number of times that table stats has been fetched from NDB
  uint m_fetch_table_stats{0};

  NdbTransaction *global_schema_lock_trans;
  uint global_schema_lock_count;
  uint global_schema_lock_error;
  uint schema_locks_count;  // Number of global schema locks taken by thread
  bool has_required_global_schema_lock(const char *func) const;

  /**
     Epoch of last committed transaction in this session, 0 if none so far
   */
  Uint64 m_last_commit_epoch_session;

  unsigned m_connect_count;
  bool valid_ndb(void) const;
  bool recycle_ndb(void);

  THD *get_thd() const { return m_thd; }

  int sql_command() const { return thd_sql_command(m_thd); }

  /*
    @brief Push a warning message onto THD's condition stack.
           Using default error code.

    @param[in]  fmt    printf-like format string
    @param[in]  ...    Variable arguments matching format string
  */
  void push_warning(const char *fmt, ...) const
      MY_ATTRIBUTE((format(printf, 2, 3)));

  /*
    @brief Push a warning message onto THD's condition stack.
           Using specified error code.

    @param      code   Error code to use for the warning
    @param[in]  fmt    printf-like format string
    @param[in]  ...    Variable arguments matching format string
  */
  void push_warning(uint code, const char *fmt, ...) const
      MY_ATTRIBUTE((format(printf, 3, 4)));

  /*
    @brief Push an error from NDB as warning message onto THD's condition stack.

    @param      ndberr The NDB error to push as warning
  */
  void push_ndb_error_warning(const NdbError &ndberr) const;

  /*
    @brief Push the NDB error as warning message. Then set an
    error message(with my_error) that describes the operation
    that failed.

    @param      message Description of the operation that failed
  */
  void set_ndb_error(const NdbError &ndberr, const char *message) const;

  /*
    @brief Instantiate the Ndb_DDL_transaction_ctx object if its not
           instantiated already and return the pointer to the object.

    @param   create_if_not_exist      Boolean flag. If true, a new
                                      ddl_transaction_ctx object will be
                                      instantiated if there is none already.
    @return  The pointer to Ndb_DDL_transaction_ctx object stored in Thd_ndb.
  */
  class Ndb_DDL_transaction_ctx *get_ddl_transaction_ctx(
      bool create_if_not_exist = false);

  /*
    @brief Destroy the Ndb_DDL_transaction_ctx instance.
  */
  void clear_ddl_transaction_ctx();

  /*
    @brief  Create a string to identify the owner thread of this Thd_ndb
    @return String identification of the owner thread
  */
  std::string get_info_str() const;

 private:
  std::unique_ptr<Ndb_applier> m_applier;

  /**
     @brief Check if this Thd_ndb will do applier work and need to be
     extended with Ndb_applier state.

     This means:
      1) The SQL thread when not using workers
      2) The SQL worker thread(s)

    @return true this Thd_ndb will do applier work
  */
  bool will_do_applier_work() const;

 public:
  /**
     @brief Initialize the Applier state for Thd_ndb which is replication
     applier.

     @return true for success
   */
  bool init_applier();

  /**
     @brief Get pointer to Applier state

     @return pointer to Ndb_applier for Thd_ndb which is replication applier
   */
  Ndb_applier *get_applier() const { return m_applier.get(); }
};

/**
  @brief RAII style class for seizing and relasing a Thd_ndb
*/
class Thd_ndb_guard {
  THD *const m_thd;
  Thd_ndb *const m_thd_ndb;
  Thd_ndb_guard() = delete;
  Thd_ndb_guard(const Thd_ndb_guard &) = delete;

 public:
  Thd_ndb_guard(THD *thd, const char *name)
      : m_thd(thd), m_thd_ndb(Thd_ndb::seize(m_thd, name)) {
    thd_set_thd_ndb(m_thd, m_thd_ndb);
  }

  ~Thd_ndb_guard() {
    Thd_ndb::release(m_thd_ndb);
    thd_set_thd_ndb(m_thd, nullptr);
  }

  const Thd_ndb *get_thd_ndb() const { return m_thd_ndb; }
};

#endif

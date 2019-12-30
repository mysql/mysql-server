/*
   Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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

#include "map_helpers.h"
#include "storage/ndb/plugin/ndb_share.h"

struct THD_NDB_SHARE;
class THD;

/*
  Class for ndbcluster thread specific data
*/
class Thd_ndb {
  THD *const m_thd;

  Thd_ndb(THD *);
  ~Thd_ndb();
  const bool m_slave_thread;  // cached value of thd->slave_thread

  uint32 options;
  uint32 trans_options;
  class Ndb_DDL_transaction_ctx *m_ddl_ctx;

 public:
  static Thd_ndb *seize(THD *);
  static void release(Thd_ndb *thd_ndb);

  void init_open_tables();

  class Ndb_cluster_connection *connection;
  class Ndb *ndb;
  class ha_ndbcluster *m_handler;
  ulong count;
  uint lock_count;
  uint start_stmt_count;
  uint save_point_count;
  class NdbTransaction *trans;
  bool m_error;
  bool m_slow_path;
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
      Whenver a GSL lock is required, it is acquired by the coordinator.
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
       Remember that statement has written to ndb_apply_status and subsequent
       writes need to do updates
    */
    TRANS_INJECTED_APPLY_STATUS = 1 << 0,

    /*
       Indicator that no looging is performd by this MySQL Server ans thus
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
  malloc_unordered_map<const void *, THD_NDB_SHARE *> open_tables{
      PSI_INSTRUMENT_ME};
  /*
    This is a memroot used to buffer rows for batched execution.
    It is reset after every execute().
  */
  MEM_ROOT m_batch_mem_root;
  /*
    Estimated pending batched execution bytes, once this is > BATCH_FLUSH_SIZE
    we execute() to flush the rows buffered in m_batch_mem_root.
  */
  uint m_unsent_bytes;
  uint m_batch_size;
  bool add_row_check_if_batch_full(uint size);

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

  bool is_slave_thread(void) const { return m_slave_thread; }

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
};

#endif

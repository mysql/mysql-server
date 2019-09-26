/*
  Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"

#include <mysql/psi/mysql_thread.h>
#include <unordered_map>

#include "my_dbug.h"
#include "my_thread.h"
#include "mysql/plugin.h"
#include "sql/auth/acl_change_notification.h"
#include "sql/binlog.h"
#include "sql/dd/types/abstract_table.h"  // dd::enum_table_type
#include "sql/dd/types/tablespace.h"      // dd::Tablespace
#include "sql/derror.h"                   // ER_THD
#include "sql/mysqld.h"                   // opt_bin_log
#include "sql/mysqld_thd_manager.h"       // Global_THD_manager
#include "sql/protocol_classic.h"
#include "sql/rpl_injector.h"
#include "sql/rpl_slave.h"
#include "sql/sql_lex.h"
#include "sql/sql_table.h"  // build_table_filename
#include "sql/sql_thd_internal_api.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/ndb_cluster_connection.hpp"
#include "storage/ndb/plugin/ha_ndbcluster.h"
#include "storage/ndb/plugin/ha_ndbcluster_connection.h"
#include "storage/ndb/plugin/ndb_apply_status_table.h"
#include "storage/ndb/plugin/ndb_binlog_client.h"
#include "storage/ndb/plugin/ndb_bitmap.h"
#include "storage/ndb/plugin/ndb_dd.h"
#include "storage/ndb/plugin/ndb_dd_client.h"
#include "storage/ndb/plugin/ndb_dd_disk_data.h"
#include "storage/ndb/plugin/ndb_dd_schema.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_global_schema_lock.h"
#include "storage/ndb/plugin/ndb_global_schema_lock_guard.h"
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_name_util.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_require.h"
#include "storage/ndb/plugin/ndb_retry.h"
#include "storage/ndb/plugin/ndb_schema_dist_table.h"
#include "storage/ndb/plugin/ndb_schema_result_table.h"
#include "storage/ndb/plugin/ndb_sleep.h"
#include "storage/ndb/plugin/ndb_stored_grants.h"
#include "storage/ndb/plugin/ndb_table_guard.h"
#include "storage/ndb/plugin/ndb_tdc.h"
#include "storage/ndb/plugin/ndb_thd.h"

typedef NdbDictionary::Event NDBEVENT;
typedef NdbDictionary::Object NDBOBJ;
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;

extern bool opt_ndb_log_orig;
extern bool opt_ndb_log_bin;
extern bool opt_ndb_log_update_as_write;
extern bool opt_ndb_log_updated_only;
extern bool opt_ndb_log_update_minimal;
extern bool opt_ndb_log_binlog_index;
extern bool opt_ndb_log_apply_status;
extern st_ndb_slave_state g_ndb_slave_state;
extern bool opt_ndb_log_transaction_id;
extern bool log_bin_use_v1_row_events;
extern bool opt_ndb_log_empty_update;
extern bool opt_ndb_clear_apply_status;
extern bool opt_ndb_schema_dist_upgrade_allowed;
extern int opt_ndb_schema_dist_timeout;
extern ulong opt_ndb_schema_dist_lock_wait_timeout;

bool ndb_log_empty_epochs(void);

void ndb_index_stat_restart();

#include "storage/ndb/plugin/ndb_anyvalue.h"
#include "storage/ndb/plugin/ndb_binlog_extra_row_info.h"
#include "storage/ndb/plugin/ndb_binlog_thread.h"
#include "storage/ndb/plugin/ndb_event_data.h"
#include "storage/ndb/plugin/ndb_repl_tab.h"
#include "storage/ndb/plugin/ndb_schema_dist.h"
#include "storage/ndb/plugin/ndb_schema_object.h"

extern Ndb_cluster_connection *g_ndb_cluster_connection;

/*
  Timeout for syncing schema events between
  mysql servers, and between mysql server and the binlog
*/
static const int DEFAULT_SYNC_TIMEOUT = 120;

/* Column numbers in the ndb_binlog_index table */
enum Ndb_binlog_index_cols {
  NBICOL_START_POS = 0,
  NBICOL_START_FILE = 1,
  NBICOL_EPOCH = 2,
  NBICOL_NUM_INSERTS = 3,
  NBICOL_NUM_UPDATES = 4,
  NBICOL_NUM_DELETES = 5,
  NBICOL_NUM_SCHEMAOPS = 6
  /* Following colums in schema 'v2' */
  ,
  NBICOL_ORIG_SERVERID = 7,
  NBICOL_ORIG_EPOCH = 8,
  NBICOL_GCI = 9
  /* Following columns in schema 'v3' */
  ,
  NBICOL_NEXT_POS = 10,
  NBICOL_NEXT_FILE = 11
};

class Mutex_guard {
 public:
  Mutex_guard(mysql_mutex_t &mutex) : m_mutex(mutex) {
    mysql_mutex_lock(&m_mutex);
  }
  ~Mutex_guard() { mysql_mutex_unlock(&m_mutex); }

 private:
  mysql_mutex_t &m_mutex;
};

/*
  Mutex and condition used for interacting between client sql thread
  and injector thread
   - injector_data_mutex protects global data maintained
     by the injector thread and accessed by any client thread.
   - injector_event_mutex, protects injector thread pollEvents()
     and concurrent create and drop of events from client threads.
     It also protects injector_ndb and schema_ndb which are the Ndb
     objects used for the above create/drop/pollEvents()
  Rational for splitting these into two separate mutexes, is that
  the injector_event_mutex is held for 10ms across pollEvents().
  That could (almost) block access to the shared binlog injector data,
  like ndb_binlog_is_read_only().
*/
static mysql_mutex_t injector_event_mutex;
static mysql_mutex_t injector_data_mutex;
static mysql_cond_t injector_data_cond;

/*
  NOTE:
  Several of the ndb_binlog* variables use a 'relaxed locking' schema.
  Such a variable is only modified by the 'injector_thd' thread,
  but could be read by any 'thd'. Thus:
    - Any update of such a variable need a mutex lock.
    - Reading such a variable outside of the injector_thd need the mutex.
  However, it should be safe to read the variable within the injector_thd
  without holding the mutex! (As there are no other threads updating it)
*/

/**
  ndb_binlog_running
  Changes to NDB tables should be written to the binary log. I.e the
  ndb binlog injector thread subscribes to changes in the cluster
  and when such changes are received, they will be written to the
  binary log
*/
bool ndb_binlog_running = false;

static bool ndb_binlog_tables_inited = false;  // injector_data_mutex, relaxed
static bool ndb_binlog_is_ready = false;       // injector_data_mutex, relaxed

bool ndb_binlog_is_read_only(void) {
  /*
    Could be called from any client thread. Need a mutex to
    protect ndb_binlog_tables_inited and ndb_binlog_is_ready.
  */
  Mutex_guard injector_g(injector_data_mutex);
  if (!ndb_binlog_tables_inited) {
    /* the ndb_* system tables not setup yet */
    return true;
  }

  if (ndb_binlog_running && !ndb_binlog_is_ready) {
    /*
      The binlog thread is supposed to write to binlog
      but not ready (still initializing or has lost connection)
    */
    return true;
  }
  return false;
}

static THD *injector_thd = NULL;

/*
  Global reference to ndb injector thd object.

  Used mainly by the binlog index thread, but exposed to the client sql
  thread for one reason; to setup the events operations for a table
  to enable ndb injector thread receiving events.

  Must therefore always be used with a surrounding
  mysql_mutex_lock(&injector_event_mutex), when create/dropEventOperation
*/
static Ndb *injector_ndb = NULL;  // Need injector_event_mutex
static Ndb *schema_ndb = NULL;    // Need injector_event_mutex

static int ndbcluster_binlog_inited = 0;

/* NDB Injector thread (used for binlog creation) */
static ulonglong ndb_latest_applied_binlog_epoch = 0;
static ulonglong ndb_latest_handled_binlog_epoch = 0;
static ulonglong ndb_latest_received_binlog_epoch = 0;

NDB_SHARE *ndb_apply_status_share = NULL;

extern bool opt_log_slave_updates;
static bool g_ndb_log_slave_updates;

static bool g_injector_v1_warning_emitted = false;

bool Ndb_binlog_client::create_event_data(NDB_SHARE *share,
                                          const dd::Table *table_def,
                                          Ndb_event_data **event_data) const {
  DBUG_TRACE;
  DBUG_ASSERT(table_def);
  DBUG_ASSERT(event_data);

  Ndb_event_data *new_event_data = Ndb_event_data::create_event_data(
      m_thd, share, share->db, share->table_name, share->key_string(),
      injector_thd, table_def);
  if (!new_event_data) return false;

  // Return the newly created event_data to caller
  *event_data = new_event_data;

  return true;
}

static int get_ndb_blobs_value(TABLE *table, NdbValue *value_array,
                               uchar *&buffer, uint &buffer_size,
                               ptrdiff_t ptrdiff) {
  DBUG_TRACE;

  // Field has no field number so cannot use TABLE blob_field
  // Loop twice, first only counting total buffer size
  for (int loop = 0; loop <= 1; loop++) {
    uint32 offset = 0;
    for (uint i = 0; i < table->s->fields; i++) {
      Field *field = table->field[i];
      NdbValue value = value_array[i];
      if (!(field->flags & BLOB_FLAG && field->stored_in_db)) continue;
      if (value.blob == NULL) {
        DBUG_PRINT("info", ("[%u] skipped", i));
        continue;
      }
      Field_blob *field_blob = (Field_blob *)field;
      NdbBlob *ndb_blob = value.blob;
      int isNull;
      if (ndb_blob->getNull(isNull) != 0) return -1;
      if (isNull == 0) {
        Uint64 len64 = 0;
        if (ndb_blob->getLength(len64) != 0) return -1;
        // Align to Uint64
        uint32 size = Uint32(len64);
        if (size % 8 != 0) size += 8 - size % 8;
        if (loop == 1) {
          uchar *buf = buffer + offset;
          uint32 len = buffer_size - offset;  // Size of buf
          if (ndb_blob->readData(buf, len) != 0) return -1;
          DBUG_PRINT("info",
                     ("[%u] offset: %u  buf: 0x%lx  len=%u  [ptrdiff=%d]", i,
                      offset, (long)buf, len, (int)ptrdiff));
          DBUG_ASSERT(len == len64);
          // Ugly hack assumes only ptr needs to be changed
          field_blob->set_ptr_offset(ptrdiff, len, buf);
        }
        offset += size;
      } else if (loop == 1)  // undefined or null
      {
        // have to set length even in this case
        uchar *buf = buffer + offset;  // or maybe NULL
        uint32 len = 0;
        field_blob->set_ptr_offset(ptrdiff, len, buf);
        DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
      }
    }
    if (loop == 0 && offset > buffer_size) {
      my_free(buffer);
      buffer_size = 0;
      DBUG_PRINT("info", ("allocate blobs buffer size %u", offset));
      buffer = (uchar *)my_malloc(PSI_INSTRUMENT_ME, offset, MYF(MY_WME));
      if (buffer == NULL) {
        ndb_log_error("get_ndb_blobs_value, my_malloc(%u) failed", offset);
        return -1;
      }
      buffer_size = offset;
    }
  }
  return 0;
}

/*
  @brief Wait until the last committed epoch from the session enters the
         binlog. Wait a maximum of 30 seconds. This wait is necessary in
         SHOW BINLOG EVENTS so that the user see its own changes. Also
         in RESET MASTER before clearing ndbcluster's binlog index.
  @param thd Thread handle to wait for its changes to enter the binlog.
*/
static void ndbcluster_binlog_wait(THD *thd) {
  DBUG_TRACE;

  if (!ndb_binlog_running) {
    DBUG_PRINT("exit", ("Not writing binlog -> nothing to wait for"));
    return;
  }

  // Assumption is that only these commands will wait
  DBUG_ASSERT(thd_sql_command(thd) == SQLCOM_SHOW_BINLOG_EVENTS ||
              thd_sql_command(thd) == SQLCOM_FLUSH ||
              thd_sql_command(thd) == SQLCOM_RESET);

  if (thd->system_thread == SYSTEM_THREAD_NDBCLUSTER_BINLOG) {
    // Binlog Injector thread should not wait for itself
    DBUG_PRINT("exit", ("binlog injector should not wait for itself"));
    return;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (!thd_ndb) {
    // Thread has not used NDB before, no need for waiting
    DBUG_PRINT("exit", ("Thread has not used NDB, nothing to wait for"));
    return;
  }

  const char *save_info = thd->proc_info;
  thd->proc_info =
      "Waiting for ndbcluster binlog update to reach current position";

  // Highest epoch that a transaction against Ndb has received
  // as part of commit processing *in this thread*. This is a
  // per-session 'most recent change' indicator.
  const Uint64 session_last_committed_epoch =
      thd_ndb->m_last_commit_epoch_session;

  // Wait until the last committed epoch from the session enters Binlog.
  // Break any possible deadlock after 30s.
  int count = 30;  // seconds
  mysql_mutex_lock(&injector_data_mutex);
  const Uint64 start_handled_epoch = ndb_latest_handled_binlog_epoch;
  while (!thd->killed && count && ndb_binlog_running &&
         (ndb_latest_handled_binlog_epoch == 0 ||
          ndb_latest_handled_binlog_epoch < session_last_committed_epoch)) {
    count--;
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&injector_data_cond, &injector_data_mutex, &abstime);
  }
  mysql_mutex_unlock(&injector_data_mutex);

  if (count == 0) {
    ndb_log_warning(
        "Thread id %u timed out (30s) waiting for epoch %u/%u "
        "to be handled.  Progress : %u/%u -> %u/%u.",
        thd->thread_id(),
        Uint32((session_last_committed_epoch >> 32) & 0xffffffff),
        Uint32(session_last_committed_epoch & 0xffffffff),
        Uint32((start_handled_epoch >> 32) & 0xffffffff),
        Uint32(start_handled_epoch & 0xffffffff),
        Uint32((ndb_latest_handled_binlog_epoch >> 32) & 0xffffffff),
        Uint32(ndb_latest_handled_binlog_epoch & 0xffffffff));

    // Fail on wait/deadlock timeout in debug compile
    DBUG_ASSERT(false);
  }

  thd->proc_info = save_info;
}

/*
  Setup THD object
  'Inspired' from ha_ndbcluster.cc : ndb_util_thread_func
*/
THD *ndb_create_thd(char *stackptr) {
  DBUG_TRACE;
  THD *thd = new THD; /* note that constructor of THD uses DBUG_ */
  if (thd == 0) {
    return 0;
  }
  THD_CHECK_SENTRY(thd);

  thd->thread_stack = stackptr; /* remember where our stack is */
  thd->store_globals();

  thd->init_query_mem_roots();
  thd->set_command(COM_DAEMON);
  thd->system_thread = SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->get_protocol_classic()->set_client_capabilities(0);
  thd->lex->start_transaction_opt = 0;
  thd->security_context()->skip_grants();

  CHARSET_INFO *charset_connection =
      get_charset_by_csname("utf8", MY_CS_PRIMARY, MYF(MY_WME));
  thd->variables.character_set_client = charset_connection;
  thd->variables.character_set_results = charset_connection;
  thd->variables.collation_connection = charset_connection;
  thd->update_charset();
  return thd;
}

// Instantiate Ndb_binlog_thread component
static Ndb_binlog_thread ndb_binlog_thread;

// Forward declaration
static bool ndbcluster_binlog_index_remove_file(THD *thd, const char *filename);

/*
  @brief called when a binlog file is purged(i.e the physical
  binlog file is removed by the MySQL Server). ndbcluster need
  to remove any rows in its mysql.ndb_binlog_index table which
  references the removed file.

  @param thd Thread handle
  @param filename Name of the binlog file which has been removed

  @return 0 for success
*/

static int ndbcluster_binlog_index_purge_file(THD *thd, const char *filename) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("filename: %s", filename));

  // Check if the binlog thread can handle the purge.
  // This functionality is initially only implemented for the case when the
  // "server started" state has not yet been reached, but could in the future be
  // extended to handle all purging by the binlog thread(this would most likley
  // eliminate the need to create a separate THD further down in this function)
  if (ndb_binlog_thread.handle_purge(filename)) {
    return 0;  // Ok, purge handled by binlog thread
  }

  if (!ndb_binlog_running) {
    return 0;  // Nothing to do, binlog thread not running
  }

  if (thd_slave_thread(thd)) {
    return 0;  // Nothing to do, slave thread
  }

  // Create a separate temporary THD, primarily in order to isolate from any
  // active transactions in the THD passed by caller. NOTE! This should be
  // revisited
  int stack_base = 0;
  THD *tmp_thd = ndb_create_thd((char *)&stack_base);
  if (!tmp_thd) {
    ndb_log_warning("NDB Binlog: Failed to purge: '%s' (create THD failed)",
                    filename);
    return 0;
  }

  int error = 0;
  if (ndbcluster_binlog_index_remove_file(tmp_thd, filename)) {
    // Failed to delete rows from table
    ndb_log_warning("NDB Binlog: Failed to purge: '%s'", filename);
    error = 1;  // Failed
  }
  delete tmp_thd;

  /* Relink original THD */
  thd->store_globals();

  return error;
}

/*
  ndbcluster_binlog_log_query

   - callback function installed in handlerton->binlog_log_query
   - called by MySQL Server in places where no other handlerton
     function exists which can be used to notify about changes
   - used by ndbcluster to detect when
     -- databases are created or altered
     -- privilege tables have been modified
*/

static void ndbcluster_binlog_log_query(handlerton *, THD *thd,
                                        enum_binlog_command binlog_command,
                                        const char *query, uint query_length,
                                        const char *db, const char *) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("binlog_command: %d, db: '%s', query: '%s'",
                       binlog_command, db, query));

  switch (binlog_command) {
    case LOGCOM_CREATE_DB: {
      DBUG_PRINT("info", ("New database '%s' created", db));

      Ndb_schema_dist_client schema_dist_client(thd);

      if (!schema_dist_client.prepare(db, "")) {
        // Could not prepare the schema distribution client
        // NOTE! As there is no way return error, this may have to be
        // revisited, the prepare should be done
        // much earlier where it can return an error for the query
        return;
      }

      // Generate the id, version
      unsigned int id = schema_dist_client.unique_id();
      unsigned int version = schema_dist_client.unique_version();

      const bool result =
          schema_dist_client.create_db(query, query_length, db, id, version);
      if (result) {
        // Update the schema with the generated id and version but skip
        // committing the change in DD. Commit will be done by the caller.
        ndb_dd_update_schema_version(thd, db, id, version,
                                     true /*skip_commit*/);
      } else {
        // NOTE! There is currently no way to report an error from this
        // function, just log an error and proceed
        ndb_log_error("Failed to distribute 'CREATE DATABASE %s'", db);
      }
    } break;

    case LOGCOM_ALTER_DB: {
      DBUG_PRINT("info", ("The database '%s' was altered", db));

      Ndb_schema_dist_client schema_dist_client(thd);

      if (!schema_dist_client.prepare(db, "")) {
        // Could not prepare the schema distribution client
        // NOTE! As there is no way return error, this may have to be
        // revisited, the prepare should be done
        // much earlier where it can return an error for the query
        return;
      }

      // Generate the id, version
      unsigned int id = schema_dist_client.unique_id();
      unsigned int version = schema_dist_client.unique_version();

      const bool result =
          schema_dist_client.alter_db(query, query_length, db, id, version);
      if (result) {
        // Update the schema with the generated id and version but skip
        // committing the change in DD. Commit will be done by the caller.
        ndb_dd_update_schema_version(thd, db, id, version,
                                     true /*skip_commit*/);
      } else {
        // NOTE! There is currently no way to report an error from this
        // function, just log an error and proceed
        ndb_log_error("Failed to distribute 'ALTER DATABASE %s'", db);
      }
    } break;

    case LOGCOM_CREATE_TABLE:
    case LOGCOM_ALTER_TABLE:
    case LOGCOM_RENAME_TABLE:
    case LOGCOM_DROP_TABLE:
    case LOGCOM_DROP_DB:
      DBUG_PRINT("info", ("Ignoring binlog_log_query notification "
                          "for binlog_command: %d",
                          binlog_command));
      break;
  }
}

static void ndbcluster_acl_notify(THD *thd,
                                  const Acl_change_notification *notice) {
  DBUG_TRACE;
  const std::string &query = notice->get_query();

  if (!check_ndb_in_thd(thd)) {
    ndb_log_error("Privilege distribution failed to seize thd_ndb");
    return;
  }

  /* If this is the binlog thread, the ACL change has arrived via
     schema distribution and requires no further action.
  */
  if (get_thd_ndb(thd)->check_option(Thd_ndb::NO_LOG_SCHEMA_OP)) {
    return;
  }

  {
    ndb_log_verbose(9, "ACL considering: %s", query.c_str());
    std::string user_list;
    bool dist_use_db = false;   // Prepend "use [db];" to statement
    bool dist_refresh = false;  // All participants must refresh their caches
    Ndb_stored_grants::Strategy strategy =
        Ndb_stored_grants::handle_local_acl_change(thd, notice, &user_list,
                                                   &dist_use_db, &dist_refresh);

    Ndb_schema_dist_client schema_dist_client(thd);

    if (strategy == Ndb_stored_grants::Strategy::ERROR) {
      ndb_log_error("Not distributing ACL change after error.");
      return;
    }

    if (strategy == Ndb_stored_grants::Strategy::NONE) {
      ndb_log_verbose(9, "ACL change distribution: NONE");
      return;
    }

    const unsigned int &node_id = g_ndb_cluster_connection->node_id();
    if (!schema_dist_client.prepare_acl_change(node_id)) {
      ndb_log_error("Failed to distribute '%s' (Failed prepare)",
                    query.c_str());
      return;
    }

    if (strategy == Ndb_stored_grants::Strategy::SNAPSHOT) {
      ndb_log_verbose(9, "ACL change distribution: SNAPSHOT");
      if (!schema_dist_client.acl_notify(user_list))
        ndb_log_error("Failed to distribute '%s' (SNAPSHOT)", query.c_str());
      return;
    }

    DBUG_ASSERT(strategy == Ndb_stored_grants::Strategy::STATEMENT);
    ndb_log_verbose(9, "ACL change distribution: STATEMENT");
    if (!schema_dist_client.acl_notify(
            dist_use_db ? notice->get_db().c_str() : nullptr, query.c_str(),
            query.length(), dist_refresh))
      ndb_log_error("Failed to distribute '%s' (STATEMENT)", query.c_str());
  }
}

/*
  End use of the NDB Cluster binlog
   - wait for binlog thread to shutdown
*/

int ndbcluster_binlog_end() {
  DBUG_TRACE;

  if (ndbcluster_binlog_inited) {
    ndbcluster_binlog_inited = 0;

    ndb_binlog_thread.stop();
    ndb_binlog_thread.deinit();

    mysql_mutex_destroy(&injector_event_mutex);
    mysql_mutex_destroy(&injector_data_mutex);
    mysql_cond_destroy(&injector_data_cond);
  }

  return 0;
}

/*****************************************************************
  functions called from slave sql client threads
****************************************************************/
static void ndbcluster_reset_slave(THD *thd) {
  if (!ndb_binlog_running) return;

  DBUG_TRACE;

  /*
    delete all rows from mysql.ndb_apply_status table
    - if table does not exist ignore the error as it
      is a consistent behavior
  */
  if (opt_ndb_clear_apply_status) {
    Ndb_local_connection mysqld(thd);
    const bool ignore_no_such_table = true;
    if (mysqld.delete_rows(Ndb_apply_status_table::DB_NAME,
                           Ndb_apply_status_table::TABLE_NAME,
                           ignore_no_such_table, "1=1")) {
      // Failed to delete rows from table
    }
  }

  g_ndb_slave_state.atResetSlave();

  // pending fix for bug#59844 will make this function return int
}

static int ndbcluster_binlog_func(handlerton *, THD *thd, enum_binlog_func fn,
                                  void *arg) {
  DBUG_TRACE;
  int res = 0;
  switch (fn) {
    case BFN_RESET_LOGS:
      break;
    case BFN_RESET_SLAVE:
      ndbcluster_reset_slave(thd);
      break;
    case BFN_BINLOG_WAIT:
      ndbcluster_binlog_wait(thd);
      break;
    case BFN_BINLOG_END:
      res = ndbcluster_binlog_end();
      break;
    case BFN_BINLOG_PURGE_FILE:
      res = ndbcluster_binlog_index_purge_file(thd, (const char *)arg);
      break;
  }
  return res;
}

void ndbcluster_binlog_init(handlerton *h) {
  h->binlog_func = ndbcluster_binlog_func;
  h->binlog_log_query = ndbcluster_binlog_log_query;
  h->acl_notify = ndbcluster_acl_notify;
}

/*
   ndb_notify_tables_writable

   Called to notify any waiting threads that Ndb tables are
   now writable
*/
static void ndb_notify_tables_writable() {
  mysql_mutex_lock(&ndbcluster_mutex);
  ndb_setup_complete = 1;
  mysql_cond_broadcast(&ndbcluster_cond);
  mysql_mutex_unlock(&ndbcluster_mutex);
}

static bool migrate_table_with_old_extra_metadata(
    THD *thd, Ndb *ndb, const char *schema_name, const char *table_name,
    void *unpacked_data, Uint32 unpacked_len, bool force_overwrite) {
#ifndef BUG27543602
  // Temporary workaround for Bug 27543602
  if (strcmp("mysql", schema_name) == 0 &&
      (strcmp("ndb_index_stat_head", table_name) == 0 ||
       strcmp("ndb_index_stat_sample", table_name) == 0)) {
    ndb_log_info(
        "Skipped installation of the ndb_index_stat table '%s.%s'. "
        "The table can still be accessed using NDB tools",
        schema_name, table_name);
    return true;
  }
#endif

  // Migrate tables that have old metadata to data dictionary
  // using on the fly translation
  ndb_log_info(
      "Table '%s.%s' has obsolete extra metadata. "
      "The table is installed into the data dictionary "
      "by translating the old metadata",
      schema_name, table_name);

  const uchar *frm_data = static_cast<const uchar *>(unpacked_data);

  // Install table in DD
  Ndb_dd_client dd_client(thd);

  // First acquire exclusive MDL lock on schema and table
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
    ndb_log_error("Failed to acquire MDL on table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  const bool migrate_result = dd_client.migrate_table(
      schema_name, table_name, frm_data, unpacked_len, force_overwrite);

  if (!migrate_result) {
    // Failed to create DD entry for table
    ndb_log_error("Failed to create entry in DD for table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  // Check if table need to be setup for binlogging or
  // schema distribution
  const dd::Table *table_def;
  if (!dd_client.get_table(schema_name, table_name, &table_def)) {
    ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                  table_name);
    return false;
  }

  if (ndbcluster_binlog_setup_table(thd, ndb, schema_name, table_name,
                                    table_def) != 0) {
    ndb_log_error("Failed to setup binlog for table '%s.%s'", schema_name,
                  table_name);
    return false;
  }

  return true;
}

/**
  Utility class encapsulating the code which setup the 'ndb binlog thread'
  to be "connected" to the cluster.
  This involves:
   - synchronizing the local mysqld data dictionary with that in NDB
   - subscribing to changes that happen in NDB, thus allowing:
    -- local Data Dictionary to be kept in synch
    -- changes in NDB to be written to binlog

*/

class Ndb_binlog_setup {
  THD *const m_thd;

  // Enum defining database ddl types
  enum Ndb_schema_ddl_type : unsigned short {
    SCHEMA_DDL_CREATE = 0,
    SCHEMA_DDL_ALTER = 1,
    SCHEMA_DDL_DROP = 2
  };

  // A tuple to hold the values read from ndb_schema table
  using Ndb_schema_tuple =
      std::tuple<std::string,         /* db name */
                 std::string,         /* query */
                 Ndb_schema_ddl_type, /* database ddl type */
                 unsigned int,        /* id */
                 unsigned int>;       /* version */

  /**
   * @brief Retrieves all the database DDLs from the mysql.ndb_schema table
   *
   * @note This function is designed to be called through ndb_trans_retry().
   *
   * @param      ndb_transaction  NdbTransaction object to perform the read.
   * @param      ndb_schema_tab   Pointer to ndb_schema table's
   *                              NdbDictionary::Table object.
   * @param[out] database_ddls    Vector of Ndb_schema_tuple consisting DDLs
   *                              read from ndb_schema table
   * @return NdbError On failure
   *         nullptr  On success
   */
  static const NdbError *fetch_database_ddls(
      NdbTransaction *ndb_transaction,
      const NdbDictionary::Table *ndb_schema_tab,
      std::vector<Ndb_schema_tuple> *database_ddls) {
    DBUG_ASSERT(ndb_transaction != nullptr);
    DBUG_TRACE;

    /* Create scan operation and define the read */
    NdbScanOperation *op = ndb_transaction->getNdbScanOperation(ndb_schema_tab);
    if (op == nullptr) {
      return &ndb_transaction->getNdbError();
    }

    if (op->readTuples(NdbScanOperation::LM_Read, NdbScanOperation::SF_TupScan,
                       1) != 0) {
      return &op->getNdbError();
    }

    /* Define the attributes to be fetched */
    NdbRecAttr *ndb_rec_db = op->getValue(Ndb_schema_dist_table::COL_DB);
    NdbRecAttr *ndb_rec_name = op->getValue(Ndb_schema_dist_table::COL_NAME);
    NdbRecAttr *ndb_rec_id = op->getValue(Ndb_schema_dist_table::COL_ID);
    NdbRecAttr *ndb_rec_version =
        op->getValue(Ndb_schema_dist_table::COL_VERSION);
    if (!ndb_rec_db || !ndb_rec_name || !ndb_rec_id || !ndb_rec_version) {
      return &op->getNdbError();
    }

    char query[64000];
    NdbBlob *query_blob_handle =
        op->getBlobHandle(Ndb_schema_dist_table::COL_QUERY);
    if (!query_blob_handle ||
        (query_blob_handle->getValue(query, sizeof(query)) != 0)) {
      return &op->getNdbError();
    }

    /* Start scanning */
    if (ndb_transaction->execute(NdbTransaction::NoCommit)) {
      return &ndb_transaction->getNdbError();
    }

    /* Handle the results and store it in the map */
    while ((op->nextResult()) == 0) {
      std::string db_name = Ndb_util_table::unpack_varbinary(ndb_rec_db);
      std::string table_name = Ndb_util_table::unpack_varbinary(ndb_rec_name);
      /* Database DDLs are entries with no table_name */
      if (table_name.empty()) {
        /* NULL terminate the query string by
           getting the length of the query blob */
        Uint64 query_length = 0;
        if (query_blob_handle->getLength(query_length)) {
          return &query_blob_handle->getNdbError();
        }
        query[query_length] = 0;

        /* Inspect the query string further to find out the DDL type */
        Ndb_schema_ddl_type type;
        if (native_strncasecmp("CREATE", query, 6) == 0) {
          type = SCHEMA_DDL_CREATE;
        } else if (native_strncasecmp("ALTER", query, 5) == 0) {
          type = SCHEMA_DDL_ALTER;
        } else if (native_strncasecmp("DROP", query, 4) == 0) {
          type = SCHEMA_DDL_DROP;
        } else {
          /* Not a database DDL skip this one */
          continue;
        }
        /* Add the database DDL to the map */
        database_ddls->push_back(
            std::make_tuple(db_name, query, type, ndb_rec_id->u_32_value(),
                            ndb_rec_version->u_32_value()));
      }
    }
    /* Successfully read the rows. Return to caller */
    return nullptr;
  }

  /*
    NDB has no representation of the database schema objects, but
    the mysql.ndb_schema table contains the latest schema operations
    done via a mysqld, and thus reflects databases created/dropped/altered.
    This function tries to restore the correct state w.r.t created databases
    using the information in that table, NDB Dictionary and DD.
  */
  bool synchronize_databases() {
    ndb_log_info("Synchronizing databases");
    DBUG_TRACE;

    /*
      Function should only be called while ndbcluster_global_schema_lock
      is held, to ensure that ndb_schema table is not being updated while
      synchronizing the databases.
     */
    Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
    if (!thd_ndb->has_required_global_schema_lock(
            "Ndb_binlog_setup::synchronize_databases"))
      return false;

    /* Open the ndb_schema table for reading */
    Ndb *ndb = thd_ndb->ndb;
    Ndb_schema_dist_table ndb_schema_table(thd_ndb);
    if (!ndb_schema_table.open()) {
      const NdbError &ndb_error = ndb->getDictionary()->getNdbError();
      ndb_log_error("Failed to open ndb_schema table, Error : %u(%s)",
                    ndb_error.code, ndb_error.message);
      return false;
    }
    const NDBTAB *ndbtab = ndb_schema_table.get_table();

    /* Create the std::function instance of fetch_database_ddls()
       to be used with ndb_trans_retry() */
    std::function<const NdbError *(NdbTransaction *,
                                   const NdbDictionary::Table *,
                                   std::vector<Ndb_schema_tuple> *)>
        fetch_db_func = std::bind(&fetch_database_ddls, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);

    /* Read ndb_schema and fetch the database DDLs */
    NdbError last_ndb_err;
    std::vector<Ndb_schema_tuple> database_ddls;
    if (!ndb_trans_retry(ndb, m_thd, last_ndb_err, fetch_db_func, ndbtab,
                         &database_ddls)) {
      ndb_log_error(
          "Failed to fetch database DDL from ndb_schema table. Error : %u(%s)",
          last_ndb_err.code, last_ndb_err.message);
      return false;
    }

    /* Fetch list of databases used in NDB */
    std::unordered_set<std::string> databases_in_NDB;
    if (!ndb_get_database_names_in_dictionary(thd_ndb->ndb->getDictionary(),
                                              &databases_in_NDB)) {
      ndb_log_error("Failed to fetch database names from NDB");
      return false;
    }

    /* Read all the databases from DD */
    Ndb_dd_client dd_client(m_thd);
    std::map<std::string, const dd::Schema *> databases_in_DD;
    if (!dd_client.fetch_all_schemas(databases_in_DD)) {
      ndb_log_error("Failed to fetch schema details from DD");
      return false;
    }

    /* Mark this as a participant so that the any DDLs don't get distributed */
    Thd_ndb::Options_guard thd_ndb_options(thd_ndb);
    thd_ndb_options.set(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT);

    /* Inspect the DDLs obtained from ndb_schema and act upon it based on
       the list of databases available in the DD and NDB */
    Ndb_local_connection mysqld(m_thd);
    for (auto &ddl_tuple : database_ddls) {
      /* Read all the values from tuple */
      std::string db_name, query;
      Ndb_schema_ddl_type schema_ddl_type;
      unsigned int ddl_counter, ddl_node_id;
      std::tie(db_name, query, schema_ddl_type, ddl_counter, ddl_node_id) =
          ddl_tuple;
      DBUG_ASSERT(ddl_counter != 0 && ddl_node_id != 0);
      ndb_log_verbose(5,
                      "ndb_schema query : '%s', db : '%s', "
                      "counter : %u, node_id : %u",
                      query.c_str(), db_name.c_str(), ddl_counter, ddl_node_id);

      /* Check if the database exists in DD and read in its version info */
      bool db_exists_in_DD = false;
      bool tables_exist_in_database = false;
      unsigned int schema_counter = 0, schema_node_id = 0;
      auto it = databases_in_DD.find(db_name);
      if (it != databases_in_DD.end()) {
        db_exists_in_DD = true;

        /* Read se_private_data */
        const dd::Schema *schema = it->second;
        ndb_dd_schema_get_counter_and_nodeid(schema, schema_counter,
                                             schema_node_id);
        ndb_log_verbose(5,
                        "Found schema '%s' in DD with "
                        "counter : %u, node_id : %u",
                        db_name.c_str(), ddl_counter, ddl_node_id);

        /* Check if there are any local tables */
        if (!ndb_dd_has_local_tables_in_schema(m_thd, db_name.c_str(),
                                               tables_exist_in_database)) {
          ndb_log_error(
              "Failed to check if the Schema '%s' has any local tables",
              db_name.c_str());
          return false;
        }
      }

      /* Check if the database has tables in NDB. */
      tables_exist_in_database |=
          (databases_in_NDB.find(db_name) != databases_in_NDB.end());

      /* Handle the relevant DDL based on the
         existence of the database in DD and NDB*/
      switch (schema_ddl_type) {
        case SCHEMA_DDL_CREATE: {
          /* Flags to decide if the database needs to be created */
          bool create_database = !db_exists_in_DD;
          bool update_version = create_database;

          if (db_exists_in_DD && (ddl_node_id != schema_node_id ||
                                  ddl_counter != schema_counter)) {
            /* Database exists in DD but version differs.
               Drop and recreate database iff it is empty */
            if (!tables_exist_in_database) {
              if (mysqld.drop_database(db_name)) {
                ndb_log_error("Failed to update database '%s'",
                              db_name.c_str());
                return false;
              }
              /* Mark that the database needs to be created */
              create_database = true;
            } else {
              /* Database has tables in it. Just update the version later. */
              ndb_log_warning(
                  "Database '%s' exists already with a different version",
                  db_name.c_str());
            }
            /* The version information in the ndb_schema is the right version.
               So, always update the version of schema in DD to that in the
               ndb_schema if they differ. */
            update_version = true;
          }

          if (create_database) {
            /* Create it by running the DDL */
            if (mysqld.execute_database_ddl(query)) {
              ndb_log_error("Failed to create database '%s'.", db_name.c_str());
              return false;
            }
            ndb_log_info("Created database '%s'", db_name.c_str());
          }

          if (update_version) {
            /* Update the schema version */
            if (!ndb_dd_update_schema_version(m_thd, db_name.c_str(),
                                              ddl_counter, ddl_node_id)) {
              ndb_log_error(
                  "Failed to update version in DD for database : '%s'",
                  db_name.c_str());
              return false;
            }
            ndb_log_info(
                "Updated the version of database '%s' to "
                "counter : %u, node_id : %u",
                db_name.c_str(), ddl_counter, ddl_node_id);
          }

          /* Remove the database name from the NDB list */
          databases_in_NDB.erase(db_name);

        } break;
        case SCHEMA_DDL_ALTER: {
          if (!db_exists_in_DD) {
            /* Database doesn't exist. Create it. */
            if (mysqld.create_database(db_name)) {
              ndb_log_error("Failed to create database '%s'", db_name.c_str());
              return false;
            }
            ndb_log_info("Created database '%s'", db_name.c_str());
          }

          /* Compare the versions and run the alter if they differ */
          if (ddl_node_id != schema_node_id || ddl_counter != schema_counter) {
            if (mysqld.execute_database_ddl(query)) {
              ndb_log_error("Failed to alter database '%s'.", db_name.c_str());
              return false;
            }
            /* Update the schema version */
            if (!ndb_dd_update_schema_version(m_thd, db_name.c_str(),
                                              ddl_counter, ddl_node_id)) {
              ndb_log_error(
                  "Failed to update version in DD for database : '%s'",
                  db_name.c_str());
              return false;
            }
            ndb_log_info("Successfully altered database '%s'", db_name.c_str());
          }

          /* Remove the database name from the NDB list */
          databases_in_NDB.erase(db_name);

        } break;
        case SCHEMA_DDL_DROP: {
          if (db_exists_in_DD) {
            /* Database exists in DD */
            if (!tables_exist_in_database) {
              /* drop it if doesn't have any table in it */
              if (mysqld.drop_database(db_name)) {
                ndb_log_error("Failed to drop database '%s'.", db_name.c_str());
                return false;
              }
              ndb_log_info("Dropped database '%s'", db_name.c_str());
            } else {
              /* It has table(s) in it. Skip dropping it */
              ndb_log_warning("Database '%s' has tables. Skipped dropping it.",
                              db_name.c_str());

              /* Update the schema version to drop database DDL's version */
              if (!ndb_dd_update_schema_version(m_thd, db_name.c_str(),
                                                ddl_counter, ddl_node_id)) {
                ndb_log_error(
                    "Failed to update version in DD for database : '%s'",
                    db_name.c_str());
                return false;
              }
            }

            /* Remove the database name from the NDB list */
            databases_in_NDB.erase(db_name);
          }
        } break;
        default:
          DBUG_ASSERT(!"Unknown database DDL type");
      }
    }

    /* Check that all the remaining databases in NDB are in DD.
       Create them if they don't exist in the DD. */
    for (std::string db_name : databases_in_NDB) {
      if (databases_in_DD.find(db_name) == databases_in_DD.end()) {
        /* Create the database with default properties */
        if (mysqld.create_database(db_name)) {
          ndb_log_error("Failed to create database '%s'.", db_name.c_str());
          return false;
        }
        ndb_log_info("Discovered a database : '%s'", db_name.c_str());
      }
    }
    return true;
  }

  bool remove_table_from_dd(const char *schema_name, const char *table_name) {
    Ndb_dd_client dd_client(m_thd);

    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
      return false;
    }

    if (!dd_client.remove_table(schema_name, table_name)) {
      return false;
    }

    dd_client.commit();

    return true;  // OK
  }

  /**
     @brief Remove all deleted NDB tables from DD by comparing them against
            a list of tables in NDB. Optionally during an initial start/restart
            remove all NDB tables from DD.
     @param initial_restart   Boolean flag. When true, it denotes that the NDB
                              has been started/restarted with initial option.
     @return true on success, false otherwise.
   */
  bool remove_deleted_ndb_tables_from_dd(bool initial_restart) {
    DBUG_TRACE;
    ndb_log_verbose(50, "Looking to remove deleted tables from NDB...");

    Ndb_dd_client dd_client(m_thd);
    // Fetch list of schemas in DD
    std::vector<std::string> schema_names;
    if (!dd_client.fetch_schema_names(&schema_names)) {
      ndb_log_error("Failed to fetch schema names from DD");
      return false;
    }

    ndb_log_verbose(50, "Found %zu databases in DD", schema_names.size());

    // Iterate over each schema and
    // remove all or deleted NDB tables from the DD one by one
    for (const auto &name : schema_names) {
      const char *schema_name = name.c_str();
      // Lock the schema in DD
      if (!dd_client.mdl_lock_schema(schema_name)) {
        ndb_log_error("Failed to acquire MDL lock on schema '%s'", schema_name);
        return false;
      }

      ndb_log_verbose(50, "Fetching list of NDB tables");

      // Fetch list of NDB tables in DD, also acquire MDL lock on table names
      std::unordered_set<std::string> ndb_tables_in_DD;
      if (!dd_client.get_ndb_table_names_in_schema(schema_name,
                                                   &ndb_tables_in_DD)) {
        ndb_log_error("Failed to get list of NDB tables in schema '%s' from DD",
                      schema_name);
        return false;
      }
      ndb_log_verbose(50, "Found %zu NDB tables in DD",
                      ndb_tables_in_DD.size());

      if (unlikely(initial_restart)) {
        // This is an initial start/restart
        // Remove all NDB tables in the schema
        for (const auto &ndb_table_name : ndb_tables_in_DD) {
          ndb_log_info("Removing table '%s.%s'", schema_name,
                       ndb_table_name.c_str());
          if (!remove_table_from_dd(schema_name, ndb_table_name.c_str())) {
            ndb_log_error("Failed to remove table '%s.%s' from DD", schema_name,
                          ndb_table_name.c_str());
            return false;
          }
        }
      } else {
        // This is not an initial start/restart
        // Fetch list of NDB tables in NDB
        Ndb *ndb = get_thd_ndb(m_thd)->ndb;
        std::unordered_set<std::string> ndb_tables_in_NDB;
        if (!ndb_get_table_names_in_schema(ndb->getDictionary(), schema_name,
                                           &ndb_tables_in_NDB)) {
          log_NDB_error(ndb->getDictionary()->getNdbError());
          ndb_log_error(
              "Failed to get list of NDB tables in schema '%s' from "
              "NDB",
              schema_name);
          return false;
        }

        ndb_log_verbose(
            50, "Found %zu NDB tables in schema '%s' in the NDB Dictionary",
            ndb_tables_in_NDB.size(), schema_name);

        // Iterate over all NDB tables found in DD. If they don't
        // exist in NDB anymore, then remove the table from DD
        for (const auto &ndb_table_name : ndb_tables_in_DD) {
          if (ndb_tables_in_NDB.find(ndb_table_name) ==
              ndb_tables_in_NDB.end()) {
            ndb_log_info("Removing table '%s.%s'", schema_name,
                         ndb_table_name.c_str());
            if (!remove_table_from_dd(schema_name, ndb_table_name.c_str())) {
              ndb_log_error("Failed to remove table '%s.%s' from DD",
                            schema_name, ndb_table_name.c_str());
              return false;
            }
          }
        }
      }
    }

    ndb_log_verbose(50, "Done removing deleted NDB tables from DD!");

    return true;
  }

  bool install_table_from_NDB(THD *thd, const char *schema_name,
                              const char *table_name,
                              const NdbDictionary::Table *ndbtab,
                              bool force_overwrite = false) {
    DBUG_TRACE;
    DBUG_PRINT("enter",
               ("schema_name: %s, table_name: %s", schema_name, table_name));

    Thd_ndb *thd_ndb = get_thd_ndb(thd);
    Ndb *ndb = thd_ndb->ndb;
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    const std::string tablespace_name = ndb_table_tablespace_name(dict, ndbtab);
    if (!tablespace_name.empty()) {
      // This is a disk data table. Before the table is installed, we check if
      // the tablespace exists in DD since it's possible that the tablespace
      // wasn't successfully installed during the tablespace synchronization
      // step. We try and deal with such scenarios by attempting to install the
      // missing tablespace or erroring out should the installation fail once
      // again
      Ndb_dd_client dd_client(thd);
      if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
        ndb_log_error("Failed to acquire MDL on tablespace '%s'",
                      tablespace_name.c_str());
        return false;
      }
      bool exists_in_DD;
      if (!dd_client.tablespace_exists(tablespace_name.c_str(), exists_in_DD)) {
        ndb_log_info("Failed to determine if tablespace '%s' was present in DD",
                     tablespace_name.c_str());
        return false;
      }
      if (!exists_in_DD) {
        ndb_log_info("Tablespace '%s' does not exist in DD, installing..",
                     tablespace_name.c_str());
        if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name.c_str())) {
          ndb_log_error("Failed to acquire MDL on tablespace '%s'",
                        tablespace_name.c_str());
          return false;
        }
        std::vector<std::string> datafile_names;
        if (!ndb_get_datafile_names(dict, tablespace_name, &datafile_names)) {
          ndb_log_error(
              "Failed to get datafiles assigned to tablespace '%s' from NDB",
              tablespace_name.c_str());
          return false;
        }
        int ndb_id, ndb_version;
        if (!ndb_get_tablespace_id_and_version(dict, tablespace_name, ndb_id,
                                               ndb_version)) {
          ndb_log_error(
              "Failed to get id and version of tablespace '%s' from NDB",
              tablespace_name.c_str());
          return false;
        }
        if (!dd_client.install_tablespace(tablespace_name.c_str(),
                                          datafile_names, ndb_id, ndb_version,
                                          false /* force_overwrite */)) {
          ndb_log_error("Failed to install tablespace '%s' in DD",
                        tablespace_name.c_str());
          return false;
        }
        dd_client.commit();
        ndb_log_info("Tablespace '%s' installed in DD",
                     tablespace_name.c_str());
      }
    }

    dd::sdi_t sdi;
    {
      Uint32 version;
      void *unpacked_data;
      Uint32 unpacked_len;
      const int get_result =
          ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_len);
      if (get_result != 0) {
        DBUG_PRINT("error",
                   ("Could not get extra metadata, error: %d", get_result));
        return false;
      }

      if (version != 1 && version != 2) {
        // Skip install of table which has unsupported extra metadata
        // versions
        ndb_log_info(
            "Skipping setup of table '%s.%s', it has "
            "unsupported extra metadata version %d.",
            schema_name, table_name, version);
        return true;  // Skipped
      }

      if (version == 1) {
        const bool migrate_result = migrate_table_with_old_extra_metadata(
            thd, ndb, schema_name, table_name, unpacked_data, unpacked_len,
            force_overwrite);

        if (!migrate_result) {
          free(unpacked_data);
          return false;
        }

        free(unpacked_data);
        return true;
      }

      sdi.assign(static_cast<const char *>(unpacked_data), unpacked_len);

      free(unpacked_data);
    }

    // Found table, now install it in DD
    Ndb_dd_client dd_client(thd);

    // First acquire exclusive MDL lock on schema and table
    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
      ndb_log_error("Couldn't acquire exclusive metadata locks on '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    if (!tablespace_name.empty()) {
      // Acquire IX MDL on tablespace
      if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
        ndb_log_error("Couldn't acquire metadata lock on tablespace '%s'",
                      tablespace_name.c_str());
        return false;
      }
    }

    if (!dd_client.install_table(
            schema_name, table_name, sdi, ndbtab->getObjectId(),
            ndbtab->getObjectVersion(), ndbtab->getPartitionCount(),
            tablespace_name, force_overwrite)) {
      // Failed to install table
      ndb_log_warning("Failed to install table '%s.%s'", schema_name,
                      table_name);
      return false;
    }

    const dd::Table *table_def;
    if (!dd_client.get_table(schema_name, table_name, &table_def)) {
      ndb_log_error("Couldn't open table '%s.%s' from DD after install",
                    schema_name, table_name);
      return false;
    }

    // Check if binlogging should be setup for this table
    if (ndbcluster_binlog_setup_table(thd, ndb, schema_name, table_name,
                                      table_def)) {
      return false;
    }

    dd_client.commit();

    return true;  // OK
  }

  void log_NDB_error(const NdbError &ndb_error) const {
    // Display error code and message returned by NDB
    ndb_log_error("Got error '%d: %s' from NDB", ndb_error.code,
                  ndb_error.message);
  }

  bool synchronize_table(const char *schema_name, const char *table_name) {
    Ndb *ndb = get_thd_ndb(m_thd)->ndb;

    ndb_log_verbose(1, "Synchronizing table '%s.%s'", schema_name, table_name);

    Ndb_table_guard ndbtab_g(ndb, schema_name, table_name);
    const NDBTAB *ndbtab = ndbtab_g.get_table();
    if (!ndbtab) {
      // Failed to open the table from NDB
      log_NDB_error(ndb->getDictionary()->getNdbError());
      ndb_log_error("Failed to setup table '%s.%s'", schema_name, table_name);

      // Failed, table was listed but could not be opened, retry
      return false;
    }

    if (ndbtab->getFrmLength() == 0) {
      ndb_log_verbose(1,
                      "Skipping setup of table '%s.%s', no extra "
                      "metadata",
                      schema_name, table_name);
      return true;  // Ok, table skipped
    }

    {
      Uint32 version;
      void *unpacked_data;
      Uint32 unpacked_length;
      const int get_result =
          ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_length);

      if (get_result != 0) {
        // Header corrupt or failed to unpack
        ndb_log_error(
            "Failed to setup table '%s.%s', could not "
            "unpack extra metadata, error: %d",
            schema_name, table_name, get_result);
        return false;
      }

      free(unpacked_data);
    }

    Ndb_dd_client dd_client(m_thd);

    // Acquire MDL lock on table
    if (!dd_client.mdl_lock_table(schema_name, table_name)) {
      ndb_log_error("Failed to acquire MDL lock for table '%s.%s'", schema_name,
                    table_name);
      return false;
    }

    const dd::Table *existing;
    if (!dd_client.get_table(schema_name, table_name, &existing)) {
      ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                    table_name);
      return false;
    }

    if (existing == nullptr) {
      ndb_log_info("Table '%s.%s' does not exist in DD, installing...",
                   schema_name, table_name);

      if (!install_table_from_NDB(m_thd, schema_name, table_name, ndbtab,
                                  false /* need overwrite */)) {
        // Failed to install into DD or setup binlogging
        ndb_log_error("Failed to install table '%s.%s'", schema_name,
                      table_name);
        return false;
      }
      return true;  // OK
    }

    // Skip if table exists in DD, but is in other engine
    const dd::String_type engine = ndb_dd_table_get_engine(existing);
    if (engine != "ndbcluster") {
      ndb_log_info(
          "Skipping table '%s.%s' with same name which is in "
          "engine '%s'",
          schema_name, table_name, engine.c_str());
      return true;  // Skipped
    }

    int table_id, table_version;
    if (!ndb_dd_table_get_object_id_and_version(existing, table_id,
                                                table_version)) {
      //
      ndb_log_error(
          "Failed to extract id and version from table definition "
          "for table '%s.%s'",
          schema_name, table_name);
      DBUG_ASSERT(false);
      return false;
    }

    // Check that latest version of table definition for this NDB table
    // is installed in DD
    if (ndbtab->getObjectId() != table_id ||
        ndbtab->getObjectVersion() != table_version) {
      ndb_log_info(
          "Table '%s.%s' have different version in DD, reinstalling...",
          schema_name, table_name);
      if (!install_table_from_NDB(m_thd, schema_name, table_name, ndbtab,
                                  true /* need overwrite */)) {
        // Failed to create table from NDB
        ndb_log_error("Failed to install table '%s.%s' from NDB", schema_name,
                      table_name);
        return false;
      }
    }

    // Check if table need to be setup for binlogging or
    // schema distribution
    const dd::Table *table_def;
    if (!dd_client.get_table(schema_name, table_name, &table_def)) {
      ndb_log_error("Failed to open table '%s.%s' from DD", schema_name,
                    table_name);
      return false;
    }

    if (ndbcluster_binlog_setup_table(m_thd, ndb, schema_name, table_name,
                                      table_def) != 0) {
      ndb_log_error("Failed to setup binlog for table '%s.%s'", schema_name,
                    table_name);
      return false;
    }

    return true;
  }

  bool synchronize_schema(const char *schema_name) {
    Ndb_dd_client dd_client(m_thd);

    ndb_log_info("Synchronizing schema '%s'", schema_name);

    // Lock the schema in DD
    if (!dd_client.mdl_lock_schema(schema_name)) {
      ndb_log_error("Failed to acquire MDL lock on schema '%s'", schema_name);
      return false;
    }

    // Fetch list of NDB tables in NDB
    std::unordered_set<std::string> ndb_tables_in_NDB;
    Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    if (!ndb_get_table_names_in_schema(dict, schema_name, &ndb_tables_in_NDB)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get list of NDB tables in schema '%s' from NDB",
                    schema_name);
      return false;
    }

    // Iterate over each table in NDB and synchronize them to DD
    for (const auto ndb_table_name : ndb_tables_in_NDB) {
      if (!synchronize_table(schema_name, ndb_table_name.c_str())) {
        ndb_log_info("Failed to synchronize table '%s.%s'", schema_name,
                     ndb_table_name.c_str());
        continue;
      }
    }

    return true;
  }

  bool install_logfile_group_into_DD(
      const char *logfile_group_name, NdbDictionary::LogfileGroup ndb_lfg,
      const std::vector<std::string> &undofile_names, bool force_overwrite) {
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_logfile_group_exclusive(logfile_group_name)) {
      ndb_log_error("MDL lock could not be acquired for logfile group '%s'",
                    logfile_group_name);
      return false;
    }

    if (!dd_client.install_logfile_group(
            logfile_group_name, undofile_names, ndb_lfg.getObjectId(),
            ndb_lfg.getObjectVersion(), force_overwrite)) {
      ndb_log_error("Logfile group '%s' could not be stored in DD",
                    logfile_group_name);
      return false;
    }

    dd_client.commit();
    return true;
  }

  bool compare_file_list(const std::vector<std::string> &file_names_in_NDB,
                         const std::vector<std::string> &file_names_in_DD) {
    if (file_names_in_NDB.size() != file_names_in_DD.size()) {
      return false;
    }

    for (const auto file_name : file_names_in_NDB) {
      if (std::find(file_names_in_DD.begin(), file_names_in_DD.end(),
                    file_name) == file_names_in_DD.end()) {
        return false;
      }
    }
    return true;
  }

  bool synchronize_logfile_group(
      const char *logfile_group_name,
      const std::unordered_set<std::string> &lfg_in_DD) {
    ndb_log_verbose(1, "Synchronizing logfile group '%s'", logfile_group_name);

    Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    NdbDictionary::LogfileGroup ndb_lfg =
        dict->getLogfileGroup(logfile_group_name);
    if (ndb_dict_check_NDB_error(dict)) {
      // Failed to open logfile group from NDB
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get logfile group '%s' from NDB",
                    logfile_group_name);
      return false;
    }

    const auto lfg_position = lfg_in_DD.find(logfile_group_name);
    if (lfg_position == lfg_in_DD.end()) {
      // Logfile group exists only in NDB. Install into DD
      ndb_log_info("Logfile group '%s' does not exist in DD, installing..",
                   logfile_group_name);
      std::vector<std::string> undofile_names;
      if (!ndb_get_undofile_names(dict, logfile_group_name, &undofile_names)) {
        log_NDB_error(dict->getNdbError());
        ndb_log_error(
            "Failed to get undofiles assigned to logfile group '%s' "
            "from NDB",
            logfile_group_name);
        return false;
      }
      if (!install_logfile_group_into_DD(logfile_group_name, ndb_lfg,
                                         undofile_names,
                                         false /*force_overwrite*/)) {
        return false;
      }
      return true;
    }

    // Logfile group exists in DD
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_logfile_group(logfile_group_name,
                                          true /* intention_exclusive */)) {
      ndb_log_error("MDL lock could not be acquired for logfile group '%s'",
                    logfile_group_name);
      return false;
    }
    const dd::Tablespace *existing = nullptr;
    if (!dd_client.get_logfile_group(logfile_group_name, &existing)) {
      ndb_log_error("Failed to acquire logfile group '%s' from DD",
                    logfile_group_name);
      return false;
    }

    if (existing == nullptr) {
      ndb_log_error("Logfile group '%s' does not exist in DD",
                    logfile_group_name);
      DBUG_ASSERT(false);
      return false;
    }

    // Check if the DD has the latest definition of the logfile group
    int object_id_in_DD, object_version_in_DD;
    if (!ndb_dd_disk_data_get_object_id_and_version(existing, object_id_in_DD,
                                                    object_version_in_DD)) {
      ndb_log_error(
          "Could not extract id and version from the definition "
          "of logfile group '%s'",
          logfile_group_name);
      DBUG_ASSERT(false);
      return false;
    }

    const int object_id_in_NDB = ndb_lfg.getObjectId();
    const int object_version_in_NDB = ndb_lfg.getObjectVersion();
    std::vector<std::string> undofile_names_in_NDB;
    if (!ndb_get_undofile_names(dict, logfile_group_name,
                                &undofile_names_in_NDB)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error(
          "Failed to get undofiles assigned to logfile group '%s' "
          "from NDB",
          logfile_group_name);
      return false;
    }

    std::vector<std::string> undofile_names_in_DD;
    ndb_dd_disk_data_get_file_names(existing, undofile_names_in_DD);
    if (object_id_in_NDB != object_id_in_DD ||
        object_version_in_NDB != object_version_in_DD ||
        // The object version is not updated after an ALTER, so there
        // exists a possibility that the object ids and versions match
        // but there's a mismatch in the list of undo files assigned to
        // the logfile group. Thus, the list of files assigned to the
        // logfile group in NDB Dictionary and DD are compared as an
        // additional check. This also protects us from the corner case
        // that's possible after an initial cluster restart. In such
        // cases, it's possible the ids and versions match even though
        // they are entirely different objects
        !compare_file_list(undofile_names_in_NDB, undofile_names_in_DD)) {
      ndb_log_info(
          "Logfile group '%s' has outdated version in DD, "
          "reinstalling..",
          logfile_group_name);
      if (!install_logfile_group_into_DD(logfile_group_name, ndb_lfg,
                                         undofile_names_in_NDB,
                                         true /* force_overwrite */)) {
        return false;
      }
    }

    // Same definition of logfile group exists in both DD and NDB Dictionary
    return true;
  }

  bool synchronize_logfile_groups() {
    ndb_log_info("Synchronizing logfile groups");

    // Retrieve list of logfile groups from NDB
    std::unordered_set<std::string> lfg_in_NDB;
    Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    const NdbDictionary::Dictionary *dict = ndb->getDictionary();
    if (!ndb_get_logfile_group_names(dict, lfg_in_NDB)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to fetch logfile group names from NDB");
      return false;
    }

    Ndb_dd_client dd_client(m_thd);

    // Retrieve list of logfile groups from DD
    std::unordered_set<std::string> lfg_in_DD;
    if (!dd_client.fetch_ndb_logfile_group_names(lfg_in_DD)) {
      ndb_log_error("Failed to fetch logfile group names from DD");
      return false;
    }

    for (const auto logfile_group_name : lfg_in_NDB) {
      if (!synchronize_logfile_group(logfile_group_name.c_str(), lfg_in_DD)) {
        ndb_log_info("Failed to synchronize logfile group '%s'",
                     logfile_group_name.c_str());
      }
      lfg_in_DD.erase(logfile_group_name);
    }

    // Any entries left in lfg_in_DD exist in DD alone and not NDB
    // and can be removed
    for (const auto logfile_group_name : lfg_in_DD) {
      ndb_log_info("Logfile group '%s' does not exist in NDB, dropping",
                   logfile_group_name.c_str());
      if (!dd_client.mdl_lock_logfile_group_exclusive(
              logfile_group_name.c_str())) {
        ndb_log_info("MDL lock could not be acquired for logfile group '%s'",
                     logfile_group_name.c_str());
        ndb_log_info("Failed to synchronize logfile group '%s'",
                     logfile_group_name.c_str());
        continue;
      }
      if (!dd_client.drop_logfile_group(logfile_group_name.c_str())) {
        ndb_log_info("Failed to synchronize logfile group '%s'",
                     logfile_group_name.c_str());
      }
    }
    dd_client.commit();
    return true;
  }

  bool install_tablespace_into_DD(
      const char *tablespace_name, NdbDictionary::Tablespace ndb_tablespace,
      const std::vector<std::string> &data_file_names, bool force_overwrite) {
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name)) {
      ndb_log_error("MDL lock could not be acquired for tablespace '%s'",
                    tablespace_name);
      return false;
    }

    if (!dd_client.install_tablespace(
            tablespace_name, data_file_names, ndb_tablespace.getObjectId(),
            ndb_tablespace.getObjectVersion(), force_overwrite)) {
      ndb_log_error("Tablespace '%s' could not be stored in DD",
                    tablespace_name);
      return false;
    }

    dd_client.commit();
    return true;
  }

  bool synchronize_tablespace(
      const char *tablespace_name,
      const std::unordered_set<std::string> &tablespaces_in_DD) {
    ndb_log_verbose(1, "Synchronizing tablespace '%s'", tablespace_name);

    if (DBUG_EVALUATE_IF("ndb_install_tablespace_fail", true, false)) {
      ndb_log_verbose(20, "Skipping synchronization of tablespace '%s'",
                      tablespace_name);
      return false;
    }

    Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    const auto tablespace_position = tablespaces_in_DD.find(tablespace_name);
    NdbDictionary::Tablespace ndb_tablespace =
        dict->getTablespace(tablespace_name);
    if (ndb_dict_check_NDB_error(dict)) {
      // Failed to open tablespace from NDB
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get tablespace '%s' from NDB", tablespace_name);
      return false;
    }

    if (tablespace_position == tablespaces_in_DD.end()) {
      // Tablespace exists only in NDB. Install in DD
      ndb_log_info("Tablespace '%s' does not exist in DD, installing..",
                   tablespace_name);
      std::vector<std::string> datafile_names;
      if (!ndb_get_datafile_names(dict, tablespace_name, &datafile_names)) {
        log_NDB_error(dict->getNdbError());
        ndb_log_error("Failed to get datafiles assigned to tablespace '%s'",
                      tablespace_name);
        return false;
      }
      if (!install_tablespace_into_DD(tablespace_name, ndb_tablespace,
                                      datafile_names,
                                      false /*force_overwrite*/)) {
        return false;
      }
      return true;
    }

    // Tablespace exists in DD
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_tablespace(tablespace_name,
                                       true /* intention_exclusive */)) {
      ndb_log_error("MDL lock could not be acquired on tablespace '%s'",
                    tablespace_name);
      return false;
    }
    const dd::Tablespace *existing = nullptr;
    if (!dd_client.get_tablespace(tablespace_name, &existing)) {
      ndb_log_error("Failed to acquire tablespace '%s' from DD",
                    tablespace_name);
      return false;
    }

    if (existing == nullptr) {
      ndb_log_error("Tablespace '%s' does not exist in DD", tablespace_name);
      DBUG_ASSERT(false);
      return false;
    }

    // Check if the DD has the latest definition of the tablespace
    int object_id_in_DD, object_version_in_DD;
    if (!ndb_dd_disk_data_get_object_id_and_version(existing, object_id_in_DD,
                                                    object_version_in_DD)) {
      ndb_log_error(
          "Could not extract id and version from the definition "
          "of tablespace '%s'",
          tablespace_name);
      DBUG_ASSERT(false);
      return false;
    }

    const int object_id_in_NDB = ndb_tablespace.getObjectId();
    const int object_version_in_NDB = ndb_tablespace.getObjectVersion();
    std::vector<std::string> datafile_names_in_NDB;
    if (!ndb_get_datafile_names(dict, tablespace_name,
                                &datafile_names_in_NDB)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error(
          "Failed to get datafiles assigned to tablespace '%s' from "
          "NDB",
          tablespace_name);
      return false;
    }

    std::vector<std::string> datafile_names_in_DD;
    ndb_dd_disk_data_get_file_names(existing, datafile_names_in_DD);
    if (object_id_in_NDB != object_id_in_DD ||
        object_version_in_NDB != object_version_in_DD ||
        // The object version is not updated after an ALTER, so there
        // exists a possibility that the object ids and versions match
        // but there's a mismatch in the list of data files assigned to
        // the tablespace. Thus, the list of files assigned to the
        // tablespace in NDB Dictionary and DD are compared as an
        // additional check. This also protects us from the corner case
        // that's possible after an initial cluster restart. In such
        // cases, it's possible the ids and versions match even though
        // they are entirely different objects
        !compare_file_list(datafile_names_in_NDB, datafile_names_in_DD)) {
      ndb_log_info(
          "Tablespace '%s' has outdated version in DD, "
          "reinstalling..",
          tablespace_name);
      if (!install_tablespace_into_DD(tablespace_name, ndb_tablespace,
                                      datafile_names_in_NDB,
                                      true /* force_overwrite */)) {
        return false;
      }
    }

    // Same definition of tablespace exists in both DD and NDB Dictionary
    return true;
  }

  bool synchronize_tablespaces() {
    ndb_log_info("Synchronizing tablespaces");

    // Retrieve list of tablespaces from NDB
    std::unordered_set<std::string> tablespaces_in_NDB;
    Ndb *ndb = get_thd_ndb(m_thd)->ndb;
    const NdbDictionary::Dictionary *dict = ndb->getDictionary();
    if (!ndb_get_tablespace_names(dict, tablespaces_in_NDB)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to fetch tablespace names from NDB");
      return false;
    }

    Ndb_dd_client dd_client(m_thd);
    // Retrieve list of tablespaces from DD
    std::unordered_set<std::string> tablespaces_in_DD;
    if (!dd_client.fetch_ndb_tablespace_names(tablespaces_in_DD)) {
      ndb_log_error("Failed to fetch tablespace names from DD");
      return false;
    }

    for (const auto tablespace_name : tablespaces_in_NDB) {
      if (!synchronize_tablespace(tablespace_name.c_str(), tablespaces_in_DD)) {
        ndb_log_warning("Failed to synchronize tablespace '%s'",
                        tablespace_name.c_str());
      }
      tablespaces_in_DD.erase(tablespace_name);
    }

    // Any entries left in tablespaces_in_DD exist in DD alone and not NDB
    // and can be removed
    for (const auto tablespace_name : tablespaces_in_DD) {
      ndb_log_info("Tablespace '%s' does not exist in NDB, dropping",
                   tablespace_name.c_str());
      if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name.c_str())) {
        ndb_log_warning("MDL lock could not be acquired on tablespace '%s'",
                        tablespace_name.c_str());
        ndb_log_warning("Failed to synchronize tablespace '%s'",
                        tablespace_name.c_str());
        continue;
      }
      if (!dd_client.drop_tablespace(tablespace_name.c_str())) {
        ndb_log_warning("Failed to synchronize tablespace '%s'",
                        tablespace_name.c_str());
      }
    }
    dd_client.commit();
    return true;
  }

  bool synchronize_data_dictionary(void) {
    ndb_log_info("Starting metadata synchronization...");

    // Synchronize logfile groups and tablespaces
    if (!synchronize_logfile_groups()) {
      ndb_log_warning("Failed to synchronize logfile groups");
      return false;
    }

    if (!synchronize_tablespaces()) {
      ndb_log_warning("Failed to synchronize tablespaces");
      return false;
    }

    // Synchronize databases
    if (!synchronize_databases()) {
      ndb_log_warning("Failed to synchronize databases");
      return false;
    }

    Ndb_dd_client dd_client(m_thd);

    // Fetch list of schemas in DD
    std::vector<std::string> schema_names;
    if (!dd_client.fetch_schema_names(&schema_names)) {
      ndb_log_verbose(19,
                      "Failed to synchronize metadata, could not "
                      "fetch schema names");
      return false;
    }

    // Iterate over each schema and synchronize it one by one,
    // the assumption is that even large deployments have
    // a manageable number of tables in each schema
    for (const auto name : schema_names) {
      if (!synchronize_schema(name.c_str())) {
        ndb_log_info("Failed to synchronize metadata, schema: '%s'",
                     name.c_str());
        return false;
      }
    }

    ndb_log_info("Completed metadata synchronization");
    return true;
  }

  /**
     @brief Detect if this is an initial restart by comparing
            the schema UUID from DD with the one in NDB.
            If they differ, then this is a initial restart.
     @param dd_schema_uuid       Schema UUID retrieved from the DD.
     @param ndb_schema_uuid      Schema UUID retrieved from NDB.

     @return true if this is a initial start/restart. false otherwise.
   */
  bool detect_initial_restart(const dd::String_type &dd_schema_uuid,
                              const std::string &ndb_schema_uuid) {
    DBUG_TRACE;

    if (dd_schema_uuid.empty()) {
      // DD didn't have any schema UUID previously.
      // This is either an initial start (or) an upgrade from
      // a version which does not have the schema UUID implemented.
      // Such upgrades are considered as initial starts to keep this code
      // simple and due to the fact that the upgrade is probably being done
      // from a 5.x or a non GA 8.0.x versions to a 8.0.x cluster GA version.
      ndb_log_info("Detected an initial system start");
      return true;
    }

    if (dd_schema_uuid.compare(ndb_schema_uuid.c_str()) == 0) {
      // Schema UUID is the same.
      // This is either a normal system restart or an upgrade.
      // Any upgrade from versions having schema UUID to another
      // newer version will be handled here.
      ndb_log_info("Detected a normal system restart");
      return false;
    }

    // Schema UUID doesn't match. This is an initial system restart.
    ndb_log_info("Detected an initial system restart");
    return true;
  }

  Ndb_binlog_setup(const Ndb_binlog_setup &) = delete;
  Ndb_binlog_setup operator=(const Ndb_binlog_setup &) = delete;

 public:
  Ndb_binlog_setup(THD *thd) : m_thd(thd) {}

  /**
    @brief Setup this node to take part in schema distribution by creating the
    ndbcluster util tables, perform schema synchronization and create references
    to NDB_SHARE for all tables.

    @note See special error handling required when function fails.

    @return true if setup is succesful
    @return false if setup fails. The creation of ndb_schema table and setup
    of event operation registers this node in schema distribution protocol. Thus
    this node is expected to reply to schema distribution events. Replying is
    however not possible until setup has succesfully completed and the binlog
    thread has started to handle events. If setup fails the event operation on
    ndb_schema table and all other event operations must be removed in order to
    signal unsubcribe and remove this node from schema distribution.
  */
  bool setup(Thd_ndb *thd_ndb) {
    /* Test binlog_setup on this mysqld being slower (than other mysqld) */
    if (DBUG_EVALUATE_IF("ndb_binlog_setup_slow", true, false)) {
      ndb_log_info("'ndb_binlog_setup_slow' -> sleep");
      ndb_milli_sleep(10 * 1000);
      ndb_log_info(" <- sleep");
    }

    DBUG_ASSERT(ndb_apply_status_share == nullptr);

    // Protect the schema synchronization with GSL(Global Schema Lock)
    Ndb_global_schema_lock_guard global_schema_lock_guard(m_thd);
    if (global_schema_lock_guard.lock()) {
      return false;
    }

    /* Give additional 'binlog_setup rights' to this Thd_ndb */
    Thd_ndb::Options_guard thd_ndb_options(thd_ndb);
    thd_ndb_options.set(Thd_ndb::ALLOW_BINLOG_SETUP);

    // If the ndb_schema table gets upgraded, it will reinstall
    // the table in DD and will delete the current schema UUID.
    // So, save the schema UUID of DD before trying to upgrade the table.
    dd::String_type dd_schema_uuid_old;
    if (!ndb_dd_get_schema_uuid(m_thd, &dd_schema_uuid_old)) {
      ndb_log_warning("Failed to read the schema UUID of DD");
      return false;
    }

    Ndb_schema_dist_table schema_dist_table(thd_ndb);
    if (!schema_dist_table.create_or_upgrade(
            m_thd, opt_ndb_schema_dist_upgrade_allowed))
      return false;

    if (!Ndb_schema_dist::is_ready(m_thd)) {
      ndb_log_verbose(50, "Schema distribution setup failed");
      return false;
    }

    if (DBUG_EVALUATE_IF("ndb_binlog_setup_incomplete", true, false)) {
      // Remove the dbug keyword, only fail first time and avoid infinite setup
      DBUG_SET("-d,ndb_binlog_setup_incomplete");
      // Test handling of setup failing to complete *after* created 'ndb_schema'
      ndb_log_info("Simulate 'ndb_binlog_setup_incomplete' -> return error");
      return false;
    }

    // Retrieve the new schema UUID from NDB
    std::string ndb_schema_uuid;
    if (!schema_dist_table.get_schema_uuid(&ndb_schema_uuid)) return false;

    // Check if this is a initial restart/start
    bool initial_system_restart =
        detect_initial_restart(dd_schema_uuid_old, ndb_schema_uuid);

    // If initial start/restart, update the schema UUID in DD
    if (initial_system_restart &&
        !ndb_dd_update_schema_uuid(m_thd, ndb_schema_uuid)) {
      ndb_log_warning("Failed to update schema uuid in DD.");
      return false;
    }

    // Remove tables deleted in NDB or
    // all NDB tables if this is an initial start/restart
    if (!remove_deleted_ndb_tables_from_dd(initial_system_restart)) {
      return false;
    }

    Ndb_schema_result_table schema_result_table(thd_ndb);
    if (!schema_result_table.create_or_upgrade(
            m_thd, opt_ndb_schema_dist_upgrade_allowed))
      return false;

    Ndb_apply_status_table apply_status_table(thd_ndb);
    if (!apply_status_table.create_or_upgrade(m_thd, true)) return false;

    if (!synchronize_data_dictionary()) {
      ndb_log_verbose(9, "Failed to synchronize DD with NDB");
      return false;
    }

    // Check that references for ndb_apply_status has been created
    DBUG_ASSERT(!ndb_binlog_running || ndb_apply_status_share);

    if (!Ndb_stored_grants::initialize(m_thd, thd_ndb)) {
      ndb_log_warning("stored grants: failed to initialize");
      return false;
    }

    Mutex_guard injector_mutex_g(injector_data_mutex);
    ndb_binlog_tables_inited = true;

    return true;  // Setup completed OK
  }
};

/*
  Defines for the expected order of columns in ndb_schema table, should
  match the accepted table definition.
*/
constexpr uint SCHEMA_DB_I = 0;
constexpr uint SCHEMA_NAME_I = 1;
constexpr uint SCHEMA_SLOCK_I = 2;
constexpr uint SCHEMA_QUERY_I = 3;
constexpr uint SCHEMA_NODE_ID_I = 4;
constexpr uint SCHEMA_EPOCH_I = 5;
constexpr uint SCHEMA_ID_I = 6;
constexpr uint SCHEMA_VERSION_I = 7;
constexpr uint SCHEMA_TYPE_I = 8;
constexpr uint SCHEMA_OP_ID_I = 9;

static void ndb_report_waiting(const char *key, int the_time, const char *op,
                               const char *obj) {
  ulonglong ndb_latest_epoch = 0;
  const char *proc_info = "<no info>";
  mysql_mutex_lock(&injector_event_mutex);
  if (injector_ndb) ndb_latest_epoch = injector_ndb->getLatestGCI();
  if (injector_thd) proc_info = injector_thd->proc_info;
  mysql_mutex_unlock(&injector_event_mutex);
  {
    ndb_log_info(
        "%s, waiting max %u sec for %s %s."
        "  epochs: (%u/%u,%u/%u,%u/%u)"
        "  injector proc_info: %s",
        key, the_time, op, obj, (uint)(ndb_latest_handled_binlog_epoch >> 32),
        (uint)(ndb_latest_handled_binlog_epoch),
        (uint)(ndb_latest_received_binlog_epoch >> 32),
        (uint)(ndb_latest_received_binlog_epoch),
        (uint)(ndb_latest_epoch >> 32), (uint)(ndb_latest_epoch), proc_info);
  }
}

bool Ndb_schema_dist_client::write_schema_op_to_NDB(
    Ndb *ndb, const char *query, int query_length, const char *db,
    const char *name, uint32 id, uint32 version, uint32 nodeid, uint32 type,
    uint32 schema_op_id, uint32 anyvalue) {
  DBUG_TRACE;

  // Open ndb_schema table
  Ndb_schema_dist_table schema_dist_table(m_thd_ndb);
  if (!schema_dist_table.open()) {
    return false;
  }
  const NdbDictionary::Table *ndbtab = schema_dist_table.get_table();

  // Pack db and table_name
  char db_buf[FN_REFLEN];
  char name_buf[FN_REFLEN];
  ndb_pack_varchar(ndbtab, SCHEMA_DB_I, db_buf, db, strlen(db));
  ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, name_buf, name, strlen(name));

  // Start the schema operation with all bits set in the slock column.
  // The expectation is that all participants will reply and those not
  // connected will be filtered away by the coordinator.
  std::vector<char> slock_data;
  slock_data.assign(schema_dist_table.get_slock_bytes(), 0xFF);

  // Function for writing row to ndb_schema
  std::function<const NdbError *(NdbTransaction *)> write_schema_op_func =
      [&](NdbTransaction *trans) -> const NdbError * {
    DBUG_TRACE;

    NdbOperation *op = trans->getNdbOperation(ndbtab);
    if (op == nullptr) return &trans->getNdbError();

    const Uint64 log_epoch = 0;
    if (op->writeTuple() != 0 || op->equal(SCHEMA_DB_I, db_buf) != 0 ||
        op->equal(SCHEMA_NAME_I, name_buf) != 0 ||
        op->setValue(SCHEMA_SLOCK_I, slock_data.data()) != 0 ||
        op->setValue(SCHEMA_NODE_ID_I, nodeid) != 0 ||
        op->setValue(SCHEMA_EPOCH_I, log_epoch) != 0 ||
        op->setValue(SCHEMA_ID_I, id) != 0 ||
        op->setValue(SCHEMA_VERSION_I, version) != 0 ||
        op->setValue(SCHEMA_TYPE_I, type) != 0 ||
        op->setAnyValue(anyvalue) != 0)
      return &op->getNdbError();

    NdbBlob *ndb_blob = op->getBlobHandle(SCHEMA_QUERY_I);
    if (ndb_blob == nullptr) return &op->getNdbError();

    if (ndb_blob->setValue(query, query_length) != 0)
      return &ndb_blob->getNdbError();

    if (schema_dist_table.have_schema_op_id_column()) {
      if (op->setValue(SCHEMA_OP_ID_I, schema_op_id) != 0)
        return &op->getNdbError();
    }

    if (trans->execute(NdbTransaction::Commit, NdbOperation::DefaultAbortOption,
                       1 /* force send */) != 0) {
      return &trans->getNdbError();
    }

    return nullptr;
  };

  NdbError ndb_err;
  if (!ndb_trans_retry(ndb, m_thd, ndb_err, write_schema_op_func)) {
    m_thd_ndb->push_ndb_error_warning(ndb_err);
    m_thd_ndb->push_warning("Failed to write schema operation");
    return false;
  }

  (void)ndb->getDictionary()->forceGCPWait(1);

  return true;
}

/*
  log query in ndb_schema table
*/

bool Ndb_schema_dist_client::log_schema_op_impl(
    Ndb *ndb, const char *query, int query_length, const char *db,
    const char *table_name, uint32 ndb_table_id, uint32 ndb_table_version,
    SCHEMA_OP_TYPE type, uint32 anyvalue) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("query: %s  db: %s  table_name: %s", query, db, table_name));

  // Create NDB_SCHEMA_OBJECT
  std::unique_ptr<NDB_SCHEMA_OBJECT, decltype(&NDB_SCHEMA_OBJECT::release)>
      ndb_schema_object(NDB_SCHEMA_OBJECT::get(db, table_name, ndb_table_id,
                                               ndb_table_version, true),
                        NDB_SCHEMA_OBJECT::release);

  if (DBUG_EVALUATE_IF("ndb_binlog_random_tableid", true, false)) {
    /**
     * Try to trigger a race between late incomming slock ack for
     * schema operations having its coordinator on another node,
     * which we would otherwise have discarded as no matching
     * ndb_schema_object existed, and another schema op with same 'key',
     * coordinated by this node. Thus causing a mixup betweeen these,
     * and the schema distribution getting totally out of synch.
     */
    ndb_milli_sleep(50);
  }

  // Format string to use in log printouts
  const std::string op_name = db + std::string(".") + table_name + "(" +
                              std::to_string(ndb_table_id) + "/" +
                              std::to_string(ndb_table_version) + ")";

  // Use nodeid of the primary cluster connection since that is
  // the nodeid which the coordinator and participants listen to
  const uint32 own_nodeid = g_ndb_cluster_connection->node_id();

  // Write schema operation to the table
  if (!write_schema_op_to_NDB(ndb, query, query_length, db, table_name,
                              ndb_table_id, ndb_table_version, own_nodeid, type,
                              ndb_schema_object->schema_op_id(), anyvalue)) {
    return false;
  }

  ndb_log_verbose(19, "Distribution of '%s' - started!", op_name.c_str());
  if (ndb_log_get_verbose_level() >= 19) {
    ndb_log_error_dump("Schema_op {");
    ndb_log_error_dump("type: %d", type);
    ndb_log_error_dump("query: '%s'", query);
    ndb_log_error_dump("}");
  }

  // Wait for participants to complete the schema change
  while (true) {
    const bool completed = ndb_schema_object->client_wait_completed(1);
    if (completed) {
      // Schema operation completed
      ndb_log_verbose(19, "Distribution of '%s' - completed!", op_name.c_str());
      break;
    }

    // Check if schema distribution is still ready.
    if (m_share->have_event_operation() == false) {
      // This case is unlikely, but there is small race between
      // clients first check for schema distribution ready and schema op
      // registered in the coordinator(since the message is passed
      // via NDB).
      ndb_schema_object->fail_schema_op(Ndb_schema_dist::CLIENT_ABORT,
                                        "Schema distribution is not ready");
      ndb_log_warning("Distribution of '%s' - not ready!", op_name.c_str());
      break;
    }

    if (thd_killed(m_thd) ||
        DBUG_EVALUATE_IF("ndb_schema_dist_client_killed", true, false)) {
      ndb_schema_object->fail_schema_op(Ndb_schema_dist::CLIENT_KILLED,
                                        "Client was killed");
      ndb_log_warning("Distribution of '%s' - killed!", op_name.c_str());
      break;
    }
  }

  // Inspect results in NDB_SCHEMA_OBJECT before it's released
  std::vector<NDB_SCHEMA_OBJECT::Result> participant_results;
  ndb_schema_object->client_get_schema_op_results(participant_results);
  for (auto &it : participant_results) {
    // Save result for later
    m_schema_op_results.push_back({it.nodeid, it.result, it.message});
  }

  return true;
}

/*
  ndbcluster_binlog_event_operation_teardown

  Used when a NdbEventOperation has indicated that the table has been
  dropped or connection to cluster has failed. Function need to teardown
  the NdbEventOperation and it's associated datastructures owned
  by the binlog.

  It will also signal the "injector_data_cond" so that anyone using
  ndbcluster_binlog_wait_synch_drop_table() to wait for the binlog
  to handle the drop will be notified.

  The function may be called either by Ndb_schema_event_handler which
  listens to events only on mysql.ndb_schema or by the "injector" which
  listen to events on all the other tables.
*/

static void ndbcluster_binlog_event_operation_teardown(THD *thd, Ndb *is_ndb,
                                                       NdbEventOperation *pOp) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("pOp: %p", pOp));

  // Get Ndb_event_data associated with the NdbEventOperation
  const Ndb_event_data *event_data =
      static_cast<const Ndb_event_data *>(pOp->getCustomData());
  DBUG_ASSERT(event_data);

  // Get NDB_SHARE associated with the Ndb_event_data, the share
  // is referenced by "binlog" and will not go away until released
  // further down in this function
  NDB_SHARE *share = event_data->share;

  // Invalidate any cached NdbApi table if object version is lower
  // than what was used when setting up the NdbEventOperation
  // NOTE! This functionality need to be explained further
  {
    Thd_ndb *thd_ndb = get_thd_ndb(thd);
    Ndb *ndb = thd_ndb->ndb;
    Ndb_table_guard ndbtab_g(ndb, share->db, share->table_name);
    const NDBTAB *ev_tab = pOp->getTable();
    const NDBTAB *cache_tab = ndbtab_g.get_table();
    if (cache_tab && cache_tab->getObjectId() == ev_tab->getObjectId() &&
        cache_tab->getObjectVersion() <= ev_tab->getObjectVersion())
      ndbtab_g.invalidate();
  }

  // Remove NdbEventOperation from the share
  mysql_mutex_lock(&share->mutex);
  DBUG_ASSERT(share->op == pOp);
  share->op = NULL;
  mysql_mutex_unlock(&share->mutex);

  /* Signal ha_ndbcluster::delete/rename_table that drop is done */
  DBUG_PRINT("info", ("signal that drop is done"));
  mysql_cond_broadcast(&injector_data_cond);

  // Close the table in MySQL Server
  ndb_tdc_close_cached_table(thd, share->db, share->table_name);

  // Release the "binlog" reference from NDB_SHARE
  NDB_SHARE::release_reference(share, "binlog");

  // Remove pointer to event_data from the EventOperation
  pOp->setCustomData(NULL);

  // Drop the NdbEventOperation from NdbApi
  DBUG_PRINT("info", ("Dropping event operation: %p", pOp));
  mysql_mutex_lock(&injector_event_mutex);
  is_ndb->dropEventOperation(pOp);
  mysql_mutex_unlock(&injector_event_mutex);

  // Finally delete the event_data and thus it's mem_root, shadow_table etc.
  Ndb_event_data::destroy(event_data);
}

/*
  Data used by the Ndb_schema_event_handler which lives
  as long as the NDB Binlog thread is connected to the cluster.

  NOTE! An Ndb_schema_event_handler instance only lives for one epoch

 */
class Ndb_schema_dist_data {
  uint m_own_nodeid;
  uint m_max_subscribers{0};
  // List of active schema operations in this coordinator. Having an
  // active schema operation means it need to be checked
  // for timeout or request to be killed regularly
  std::unordered_set<const NDB_SCHEMA_OBJECT *> m_active_schema_ops;

  std::chrono::steady_clock::time_point m_next_check_time;

  // Keeps track of subscribers as reported by one data node
  class Node_subscribers {
    MY_BITMAP m_bitmap;

   public:
    Node_subscribers(const Node_subscribers &) = delete;
    Node_subscribers() = delete;
    Node_subscribers(uint max_subscribers) {
      // Initialize the bitmap
      bitmap_init(&m_bitmap, nullptr, max_subscribers);

      // Assume that all bits are cleared by bitmap_init()
      DBUG_ASSERT(bitmap_is_clear_all(&m_bitmap));
    }
    ~Node_subscribers() { bitmap_free(&m_bitmap); }
    void clear_all() { bitmap_clear_all(&m_bitmap); }
    void set(uint subscriber_node_id) {
      bitmap_set_bit(&m_bitmap, subscriber_node_id);
    }
    void clear(uint subscriber_node_id) {
      bitmap_clear_bit(&m_bitmap, subscriber_node_id);
    }
    std::string to_string() const {
      return ndb_bitmap_to_hex_string(&m_bitmap);
    }

    /**
       @brief Add current subscribers to list of nodes.
       @param subscriber_list List of subscriber
    */
    void get_subscriber_list(
        std::unordered_set<uint32> &subscriber_list) const {
      for (uint i = bitmap_get_first_set(&m_bitmap); i != MY_BIT_NONE;
           i = bitmap_get_next_set(&m_bitmap, i)) {
        subscriber_list.insert(i);
      }
    }
  };
  /*
    List keeping track of the subscribers to ndb_schema. It contains one
    Node_subscribers per data node, this avoids the need to know which data
    nodes are connected.
  */
  std::unordered_map<uint, Node_subscribers *> m_subscriber_bitmaps;

  /**
    @brief Find node subscribers for given data node
    @param data_node_id Nodeid of data node
    @return Pointer to node subscribers or nullptr
   */
  Node_subscribers *find_node_subscribers(uint data_node_id) const {
    const auto it = m_subscriber_bitmaps.find(data_node_id);
    if (it == m_subscriber_bitmaps.end()) {
      // Unexpected data node id received, this may be caused by data node added
      // without restarting this MySQL Server or node id otherwise out of
      // range for current configuration. Handle the situation gracefully and
      // just print error message to the log.
      ndb_log_error("Could not find node subscribers for data node %d",
                    data_node_id);
      ndb_log_error("Restart this MySQL Server to adapt to configuration");
      return nullptr;
    }
    Node_subscribers *subscriber_bitmap = it->second;
    ndbcluster::ndbrequire(subscriber_bitmap);
    return subscriber_bitmap;
  }

  // Holds the new key for a table to be renamed
  struct NDB_SHARE_KEY *m_prepared_rename_key;

  // Holds the Ndb_event_data which is created during inplace alter table
  // prepare and used during commit
  // NOTE! this place holder is only used for the participant in same node
  const class Ndb_event_data *m_inplace_alter_event_data{nullptr};

 public:
  Ndb_schema_dist_data(const Ndb_schema_dist_data &);  // Not implemented
  Ndb_schema_dist_data() : m_prepared_rename_key(NULL) {}
  ~Ndb_schema_dist_data() {
    // There should be no schema operations active
    DBUG_ASSERT(m_active_schema_ops.size() == 0);
  }

  void init(Ndb_cluster_connection *cluster_connection) {
    Uint32 max_subscribers = cluster_connection->max_api_nodeid() + 1;
    m_own_nodeid = cluster_connection->node_id();
    NDB_SCHEMA_OBJECT::init(m_own_nodeid);

    // Add one subscriber bitmap per data node in the current configuration
    unsigned node_id;
    Ndb_cluster_connection_node_iter node_iter;
    while ((node_id = cluster_connection->get_next_node(node_iter))) {
      m_subscriber_bitmaps.emplace(node_id,
                                   new Node_subscribers(max_subscribers));
    }
    // Remember max number of subscribers
    m_max_subscribers = max_subscribers;
  }

  void release(void) {
    // Release the subscriber bitmaps
    for (const auto it : m_subscriber_bitmaps) {
      Node_subscribers *subscriber_bitmap = it.second;
      delete subscriber_bitmap;
    }
    m_subscriber_bitmaps.clear();
    m_max_subscribers = 0;

    // Release the prepared rename key, it's very unlikely
    // that the key is still around here, but just in case
    NDB_SHARE::free_key(m_prepared_rename_key);
    m_prepared_rename_key = NULL;

    // Release the event_data saved for inplace alter, it's very
    // unlikley that the event_data is still around, but just in case
    Ndb_event_data::destroy(m_inplace_alter_event_data);
    m_inplace_alter_event_data = nullptr;

    // Release any remaining active schema operations
    for (const NDB_SCHEMA_OBJECT *schema_op : m_active_schema_ops) {
      ndb_log_info(" - releasing schema operation on '%s.%s'", schema_op->db(),
                   schema_op->name());
      schema_op->fail_schema_op(Ndb_schema_dist::COORD_ABORT,
                                "Coordinator aborted");
      // Release coordinator reference
      NDB_SCHEMA_OBJECT::release(const_cast<NDB_SCHEMA_OBJECT *>(schema_op));
    }
    m_active_schema_ops.clear();
  }

  void report_data_node_failure(unsigned data_node_id) {
    ndb_log_verbose(1, "Data node %d failed", data_node_id);

    Node_subscribers *subscribers = find_node_subscribers(data_node_id);
    if (subscribers) {
      subscribers->clear_all();

      ndb_log_verbose(19, "Subscribers[%d]: %s", data_node_id,
                      subscribers->to_string().c_str());
    }
  }

  void report_subscribe(unsigned data_node_id, unsigned subscriber_node_id) {
    ndb_log_verbose(1, "Data node %d reports subscribe from node %d",
                    data_node_id, subscriber_node_id);
    ndbcluster::ndbrequire(subscriber_node_id != 0);

    Node_subscribers *subscribers = find_node_subscribers(data_node_id);
    if (subscribers) {
      subscribers->set(subscriber_node_id);

      ndb_log_verbose(19, "Subscribers[%d]: %s", data_node_id,
                      subscribers->to_string().c_str());
    }
  }

  void report_unsubscribe(unsigned data_node_id, unsigned subscriber_node_id) {
    ndb_log_verbose(1, "Data node %d reports unsubscribe from node %d",
                    data_node_id, subscriber_node_id);
    ndbcluster::ndbrequire(subscriber_node_id != 0);

    Node_subscribers *subscribers = find_node_subscribers(data_node_id);
    if (subscribers) {
      subscribers->clear(subscriber_node_id);

      ndb_log_verbose(19, "Subscribers[%d]: %s", data_node_id,
                      subscribers->to_string().c_str());
    }
  }

  /**
     @brief Get list of current subscribers
     @note A node counts as subscribed as soon as any data node report it as
     subscribed.
     @param subscriber_list The list where to return subscribers
  */
  void get_subscriber_list(std::unordered_set<uint32> &subscriber_list) const {
    for (const auto it : m_subscriber_bitmaps) {
      Node_subscribers *subscribers = it.second;
      subscribers->get_subscriber_list(subscriber_list);
    }
    // Always add own node which is always connected
    subscriber_list.insert(m_own_nodeid);
  }

  void save_prepared_rename_key(NDB_SHARE_KEY *key) {
    m_prepared_rename_key = key;
  }

  NDB_SHARE_KEY *get_prepared_rename_key() const {
    return m_prepared_rename_key;
  }

  void save_inplace_alter_event_data(const Ndb_event_data *event_data) {
    // Should not already be set when saving a new pointer
    DBUG_ASSERT(event_data == nullptr || !m_inplace_alter_event_data);
    m_inplace_alter_event_data = event_data;
  }
  const Ndb_event_data *get_inplace_alter_event_data() const {
    return m_inplace_alter_event_data;
  }

  void add_active_schema_op(NDB_SCHEMA_OBJECT *schema_op) {
    // Current assumption is that as long as all users of schema distribution
    // hold the GSL, there will ever only be one active schema operation at a
    // time. This assumption will probably change soon, but until then it can
    // be verifed with an assert.
    DBUG_ASSERT(m_active_schema_ops.size() == 0);

    // Get coordinator reference to NDB_SCHEMA_OBJECT. It will be kept alive
    // until the coordinator releases it
    NDB_SCHEMA_OBJECT::get(schema_op);

    // Insert NDB_SCHEMA_OBJECT in list of active schema ops
    ndbcluster::ndbrequire(m_active_schema_ops.insert(schema_op).second);
  }

  void remove_active_schema_op(NDB_SCHEMA_OBJECT *schema_op) {
    // Need to have active schema op for decrement
    ndbcluster::ndbrequire(m_active_schema_ops.size() > 0);

    // Remove NDB_SCHEMA_OBJECT from list of active schema ops
    ndbcluster::ndbrequire(m_active_schema_ops.erase(schema_op) == 1);

    // Release coordinator reference to NDB_SCHEMA_OBJECT
    NDB_SCHEMA_OBJECT::release(schema_op);
  }

  const std::unordered_set<const NDB_SCHEMA_OBJECT *> &active_schema_ops() {
    return m_active_schema_ops;
  }

  bool time_for_check() {
    std::chrono::steady_clock::time_point curr_time =
        std::chrono::steady_clock::now();
    if (m_next_check_time > curr_time) return false;

    // Setup time for next check in 1 second
    m_next_check_time = curr_time + std::chrono::seconds(1);
    return true;
  }

};  // class Ndb_schema_dist_data

class Ndb_schema_event_handler {
  class Ndb_schema_op {
    /*
       Unpack arbitrary length varbinary field and return pointer to zero
       terminated string allocated in current memory root.

       @param field The field to unpack
       @return pointer to string allocated in current MEM_ROOT
    */
    static char *unpack_varbinary(Field *field) {
      /*
        The Schema_dist_client will check the schema of the ndb_schema table
        and will not send any commands unless the table fulfills requirements.
        Thus this function assumes that the field is always a varbinary
        (with at least 63 bytes length since that's the legacy min limit)
      */
      ndbcluster::ndbrequire(field->type() == MYSQL_TYPE_VARCHAR);
      ndbcluster::ndbrequire(field->field_length >= 63);

      // Calculate number of length bytes, this depends on fields max length
      const uint length_bytes = HA_VARCHAR_PACKLENGTH(field->field_length);
      ndbcluster::ndbrequire(length_bytes <= 2);

      // Read length of the varbinary which is stored in the field
      const uint varbinary_length = length_bytes == 1
                                        ? static_cast<uint>(*field->ptr)
                                        : uint2korr(field->ptr);
      DBUG_PRINT("info", ("varbinary length: %u", varbinary_length));
      // Check that varbinary length is not greater than fields max length
      // (this would indicate that corrupted data has been written to table)
      ndbcluster::ndbrequire(varbinary_length <= field->field_length);

      const char *varbinary_start =
          reinterpret_cast<const char *>(field->ptr + length_bytes);
      return sql_strmake(varbinary_start, varbinary_length);
    }

    /*
       Unpack blob field and return pointer to zero terminated string allocated
       in current MEM_ROOT.

       This function assumes that the blob has already been fetched from NDB
       and is ready to be extracted from buffers allocated inside NdbApi.

       @param ndb_blob The blob column to unpack
       @return pointer to string allocated in current MEM_ROOT
    */
    static char *unpack_blob(NdbBlob *ndb_blob) {
      // Check if blob is NULL
      int blob_is_null;
      ndbcluster::ndbrequire(ndb_blob->getNull(blob_is_null) == 0);
      if (blob_is_null != 0) {
        // The blob column didn't contain anything, return empty string
        return sql_strdup("");
      }

      // Read length of blob
      Uint64 blob_len;
      ndbcluster::ndbrequire(ndb_blob->getLength(blob_len) == 0);
      if (blob_len == 0) {
        // The blob column didn't contain anything, return empty string
        return sql_strdup("");
      }

      // Allocate space for blob plus + zero terminator in current MEM_ROOT
      char *str = static_cast<char *>((*THR_MALLOC)->Alloc(blob_len + 1));
      ndbcluster::ndbrequire(str);

      // Read the blob content
      Uint32 read_len = static_cast<Uint32>(blob_len);
      ndbcluster::ndbrequire(ndb_blob->readData(str, read_len) == 0);
      ndbcluster::ndbrequire(blob_len == read_len);  // Assume all read
      str[blob_len] = 0;                             // Zero terminate

      DBUG_PRINT("unpack_blob", ("str: '%s'", str));
      return str;
    }

    void unpack_slock(const Field *field) {
      // Allocate bitmap buffer in current MEM_ROOT
      slock_buf = static_cast<my_bitmap_map *>(
          (*THR_MALLOC)->Alloc(field->field_length));
      ndbcluster::ndbrequire(slock_buf);

      // Initialize bitmap(always suceeds when buffer is already allocated)
      (void)bitmap_init(&slock, slock_buf, field->field_length * 8);

      // Copy data into bitmap buffer
      memcpy(slock_buf, field->ptr, field->field_length);
    }

    // Unpack Ndb_schema_op from event_data pointer
    void unpack_event(const Ndb_event_data *event_data) {
      TABLE *table = event_data->shadow_table;
      Field **field = table->field;

      my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);

      /* db, varbinary */
      db = unpack_varbinary(*field);
      field++;

      /* name, varbinary */
      name = unpack_varbinary(*field);
      field++;

      /* slock, binary */
      unpack_slock(*field);
      field++;

      /* query, blob */
      query = unpack_blob(event_data->ndb_value[0][SCHEMA_QUERY_I].blob);
      field++;

      /* node_id */
      node_id = (Uint32)((Field_long *)*field)->val_int();
      /* epoch */
      field++;
      epoch = ((Field_long *)*field)->val_int();
      /* id */
      field++;
      id = (Uint32)((Field_long *)*field)->val_int();
      /* version */
      field++;
      version = (Uint32)((Field_long *)*field)->val_int();
      /* type */
      field++;
      type = (Uint32)((Field_long *)*field)->val_int();
      /* schema_op_id */
      field++;
      if (*field) {
        // Optional column
        schema_op_id = (Uint32)((Field_long *)*field)->val_int();
      } else {
        schema_op_id = 0;
      }

      dbug_tmp_restore_column_map(table->read_set, old_map);
    }

   public:
    // Note! The db, name, slock_buf and query variables point to memory
    // allocated in the current MEM_ROOT. When the Ndb_schema_op is put in the
    // list to be executed after epoch, only the pointers are copied and
    // still point to same memory inside the MEM_ROOT.
    char *db;
    char *name;

   private:
    // Buffer for the slock bitmap
    my_bitmap_map *slock_buf;

   public:
    MY_BITMAP slock;
    char *query;
    size_t query_length() const {
      // Return length of "query" which is always zero terminated string
      return strlen(query);
    }
    Uint64 epoch;
    uint32 node_id;
    uint32 id;
    uint32 version;
    uint32 type;
    uint32 any_value;
    uint32 schema_op_id;

    /**
      Create a Ndb_schema_op from event_data
    */
    static const Ndb_schema_op *create(const Ndb_event_data *event_data,
                                       Uint32 any_value) {
      DBUG_TRACE;
      // Allocate memory in current MEM_ROOT
      Ndb_schema_op *schema_op =
          (Ndb_schema_op *)(*THR_MALLOC)->Alloc(sizeof(Ndb_schema_op));
      schema_op->unpack_event(event_data);
      schema_op->any_value = any_value;
      DBUG_PRINT("exit", ("'%s.%s': query: '%s' type: %d", schema_op->db,
                          schema_op->name, schema_op->query, schema_op->type));
      return schema_op;
    }
  };

  class Ndb_schema_op_result {
    uint32 m_result{0};
    std::string m_message;

   public:
    void set_result(Ndb_schema_dist::Schema_op_result_code result,
                    const std::string message) {
      // Both result and message must be set
      DBUG_ASSERT(result && message.length());
      m_result = result;
      m_message = message;
    }
    const char *message() const { return m_message.c_str(); }
    uint32 result() const { return m_result; }
  };

  class Lock_wait_timeout_guard {
   public:
    Lock_wait_timeout_guard(THD *thd, ulong lock_wait_timeout)
        : m_thd(thd),
          m_save_lock_wait_timeout(thd->variables.lock_wait_timeout) {
      m_thd->variables.lock_wait_timeout = lock_wait_timeout;
    }

    ~Lock_wait_timeout_guard() {
      m_thd->variables.lock_wait_timeout = m_save_lock_wait_timeout;
    }

   private:
    THD *const m_thd;
    ulong m_save_lock_wait_timeout;
  };

  /**
     @brief Clear conditions accumulated in THD

     @note This function should be used after calling functions that report
     detailed failure information by both writing to log as well as push as
     warnings when they fail.
  */
  void clear_THD_conditions() const {
    // Remove the THD conditions
    m_thd->get_stmt_da()->reset_diagnostics_area();
    m_thd->get_stmt_da()->reset_condition_info(m_thd);
  }

  /**
     @brief Log conditions accumulated in THD and then clear conditions.

     @note This function should be used after calling functions
     that report detailed failure information as warnings when
     they fail.
  */
  void log_and_clear_THD_conditions() const {
    // Print THD's list of warnings to error log
    Diagnostics_area::Sql_condition_iterator it(
        m_thd->get_stmt_da()->sql_conditions());

    const Sql_condition *err;
    while ((err = it++)) {
      ndb_log_warning("Got error '%d: %s'", err->mysql_errno(),
                      err->message_text());
    }
    clear_THD_conditions();
  }

  // Log error code and message returned from NDB
  void log_NDB_error(const NdbError &ndb_error) const {
    ndb_log_info("Got error '%d: %s' from NDB", ndb_error.code,
                 ndb_error.message);
  }

  static void write_schema_op_to_binlog(THD *thd, const Ndb_schema_op *schema) {
    if (!ndb_binlog_running) {
      // This mysqld is not writing a binlog
      return;
    }

    /* any_value == 0 means local cluster sourced change that
     * should be logged
     */
    if (ndbcluster_anyvalue_is_reserved(schema->any_value)) {
      /* Originating SQL node did not want this query logged */
      if (!ndbcluster_anyvalue_is_nologging(schema->any_value)) {
        ndb_log_warning(
            "unknown value for binlog signalling 0x%X, "
            "query not logged",
            schema->any_value);
      }
      return;
    }

    Uint32 queryServerId = ndbcluster_anyvalue_get_serverid(schema->any_value);
    /*
       Start with serverId as received AnyValue, in case it's a composite
       (server_id_bits < 31).
       This is for 'future', as currently schema ops do not have composite
       AnyValues.
       In future it may be useful to support *not* mapping composite
       AnyValues to/from Binlogged server-ids.
    */
    Uint32 loggedServerId = schema->any_value;

    if (queryServerId) {
      /*
         AnyValue has non-zero serverId, must be a query applied by a slave
         mysqld.
         TODO : Assert that we are running in the Binlog injector thread?
      */
      if (!g_ndb_log_slave_updates) {
        /* This MySQLD does not log slave updates */
        return;
      }
    } else {
      /* No ServerId associated with this query, mark it as ours */
      ndbcluster_anyvalue_set_serverid(loggedServerId, ::server_id);
    }

    /*
      Write the DDL query to binlog with server_id set
      to the server_id where the query originated.
    */
    const uint32 thd_server_id_save = thd->server_id;
    DBUG_ASSERT(sizeof(thd_server_id_save) == sizeof(thd->server_id));
    thd->server_id = loggedServerId;

    LEX_CSTRING thd_db_save = thd->db();
    LEX_CSTRING schema_db_lex_cstr = {schema->db, strlen(schema->db)};
    thd->reset_db(schema_db_lex_cstr);

    int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
    thd->binlog_query(THD::STMT_QUERY_TYPE, schema->query,
                      schema->query_length(),
                      false,  // is_trans
                      true,   // direct
                      schema->name[0] == 0 || thd->db().str[0] == 0, errcode);

    // Commit the binlog write
    (void)trans_commit_stmt(thd);

    /*
      Restore original server_id and db after commit
      since the server_id is being used also in the commit logic
    */
    thd->server_id = thd_server_id_save;
    thd->reset_db(thd_db_save);
  }

  /**
    @brief Inform the other nodes that schema operation has been completed by
    this node, this is done by updating the row in the ndb_schema table.

    @note The function will read the row from ndb_schema with exclusive lock,
    append it's own data to the 'slock' column and then write the row back.

    @param schema The schema operation which has just been completed

    @return different return values are returned, but not documented since they
    are currently unused

  */
  int ack_schema_op(const Ndb_schema_op *schema) const {
    DBUG_TRACE;
    Ndb *ndb = m_thd_ndb->ndb;

    // Open ndb_schema table
    Ndb_schema_dist_table schema_dist_table(m_thd_ndb);
    if (!schema_dist_table.open()) {
      // NOTE! Legacy crash unless this was cluster connection failure, there
      // are simply no other of way sending error back to coordinator
      ndbcluster::ndbrequire(ndb->getDictionary()->getNdbError().code == 4009);
      return 1;
    }
    const NdbDictionary::Table *ndbtab = schema_dist_table.get_table();

    const NdbError *ndb_error = nullptr;
    char tmp_buf[FN_REFLEN];
    NdbTransaction *trans = 0;
    int retries = 100;
    std::string before_slock;

    // Bitmap for the slock bits
    MY_BITMAP slock;
    const uint slock_bits = schema_dist_table.get_slock_bytes() * 8;
    // Make sure that own nodeid fits in slock
    ndbcluster::ndbrequire(own_nodeid() <= slock_bits);
    (void)bitmap_init(&slock, nullptr, slock_bits);

    while (1) {
      if ((trans = ndb->startTransaction()) == 0) goto err;
      {
        NdbOperation *op = 0;
        int r = 0;

        /* read row from ndb_schema with exlusive row lock */
        r |= (op = trans->getNdbOperation(ndbtab)) == 0;
        DBUG_ASSERT(r == 0);
        r |= op->readTupleExclusive();
        DBUG_ASSERT(r == 0);

        /* db */
        ndb_pack_varchar(ndbtab, SCHEMA_DB_I, tmp_buf, schema->db,
                         strlen(schema->db));
        r |= op->equal(SCHEMA_DB_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* name */
        ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, tmp_buf, schema->name,
                         strlen(schema->name));
        r |= op->equal(SCHEMA_NAME_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* slock */
        r |= op->getValue(SCHEMA_SLOCK_I, (char *)slock.bitmap) == 0;
        DBUG_ASSERT(r == 0);

        /* Execute in NDB */
        if (trans->execute(NdbTransaction::NoCommit)) goto err;
      }

      if (ndb_log_get_verbose_level() > 19) {
        // Generate the 'before slock' string
        before_slock = ndb_bitmap_to_hex_string(&slock);
      }

      bitmap_clear_bit(&slock, own_nodeid());

      if (ndb_log_get_verbose_level() > 19) {
        const std::string after_slock = ndb_bitmap_to_hex_string(&slock);
        ndb_log_info("reply to %s.%s(%u/%u) from %s to %s", schema->db,
                     schema->name, schema->id, schema->version,
                     before_slock.c_str(), after_slock.c_str());
      }

      {
        NdbOperation *op = 0;
        int r = 0;

        /* now update the tuple */
        r |= (op = trans->getNdbOperation(ndbtab)) == 0;
        DBUG_ASSERT(r == 0);
        r |= op->updateTuple();
        DBUG_ASSERT(r == 0);

        /* db */
        ndb_pack_varchar(ndbtab, SCHEMA_DB_I, tmp_buf, schema->db,
                         strlen(schema->db));
        r |= op->equal(SCHEMA_DB_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* name */
        ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, tmp_buf, schema->name,
                         strlen(schema->name));
        r |= op->equal(SCHEMA_NAME_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* slock */
        r |= op->setValue(SCHEMA_SLOCK_I, (char *)slock.bitmap);
        DBUG_ASSERT(r == 0);
        /* node_id */
        // NOTE! Sends own nodeid here instead of nodeid who started schema op
        r |= op->setValue(SCHEMA_NODE_ID_I, own_nodeid());
        DBUG_ASSERT(r == 0);
        /* type */
        r |= op->setValue(SCHEMA_TYPE_I, (uint32)SOT_CLEAR_SLOCK);
        DBUG_ASSERT(r == 0);
      }
      if (trans->execute(NdbTransaction::Commit,
                         NdbOperation::DefaultAbortOption,
                         1 /*force send*/) == 0) {
        DBUG_PRINT("info", ("node %d cleared lock on '%s.%s'", own_nodeid(),
                            schema->db, schema->name));
        (void)ndb->getDictionary()->forceGCPWait(1);
        break;
      }
    err:
      const NdbError *this_error =
          trans ? &trans->getNdbError() : &ndb->getNdbError();
      if (this_error->status == NdbError::TemporaryError &&
          !thd_killed(m_thd)) {
        if (retries--) {
          if (trans) ndb->closeTransaction(trans);
          ndb_trans_retry_sleep();
          continue;  // retry
        }
      }
      ndb_error = this_error;
      break;
    }

    if (ndb_error) {
      ndb_log_warning(
          "Could not release slock on '%s.%s', "
          "Error code: %d Message: %s",
          schema->db, schema->name, ndb_error->code, ndb_error->message);
    }
    if (trans) ndb->closeTransaction(trans);
    bitmap_free(&slock);
    return 0;
  }

  /**
    @brief Inform the other nodes that schema operation has been completed by
    all nodes, this is done by updating the row in the ndb_schema table whith
    all bits of the 'slock' column cleared.

    @note this is done to allow the coordinator to control when the schema
    operation has completed and also to be backwards compatible with
    nodes not upgraded to new protocol

    @param db First part of key, normally used for db name
    @param table_name Second part of key, normally used for table name

    @return zero on sucess

  */
  int ack_schema_op_final(const char *db, const char *table_name) const {
    DBUG_TRACE;
    Ndb *ndb = m_thd_ndb->ndb;

    // Open ndb_schema table
    Ndb_schema_dist_table schema_dist_table(m_thd_ndb);
    if (!schema_dist_table.open()) {
      // NOTE! Legacy crash unless this was cluster connection failure, there
      // are simply no other of way sending error back to coordinator
      ndbcluster::ndbrequire(ndb->getDictionary()->getNdbError().code == 4009);
      return 1;
    }
    const NdbDictionary::Table *ndbtab = schema_dist_table.get_table();

    // Pack db and table_name
    char db_buf[FN_REFLEN];
    char name_buf[FN_REFLEN];
    ndb_pack_varchar(ndbtab, SCHEMA_DB_I, db_buf, db, strlen(db));
    ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, name_buf, table_name,
                     strlen(table_name));

    // Buffer with zeroes for slock
    std::vector<char> slock_zeroes;
    slock_zeroes.assign(schema_dist_table.get_slock_bytes(), 0);
    const char *slock_buf = slock_zeroes.data();

    // Function for updating row in ndb_schema
    std::function<const NdbError *(NdbTransaction *)> ack_schema_op_final_func =
        [ndbtab, db_buf, name_buf,
         slock_buf](NdbTransaction *trans) -> const NdbError * {
      DBUG_TRACE;

      NdbOperation *op = trans->getNdbOperation(ndbtab);
      if (op == nullptr) return &trans->getNdbError();

      // Update row
      if (op->updateTuple() != 0 || op->equal(SCHEMA_NAME_I, name_buf) != 0 ||
          op->equal(SCHEMA_DB_I, db_buf) != 0 ||
          op->setValue(SCHEMA_SLOCK_I, slock_buf) != 0 ||
          op->setValue(SCHEMA_TYPE_I, (uint32)SOT_CLEAR_SLOCK) != 0)
        return &op->getNdbError();

      if (trans->execute(NdbTransaction::Commit,
                         NdbOperation::DefaultAbortOption,
                         1 /*force send*/) != 0)
        return &trans->getNdbError();

      return nullptr;
    };

    NdbError ndb_err;
    if (!ndb_trans_retry(ndb, m_thd, ndb_err, ack_schema_op_final_func)) {
      log_NDB_error(ndb_err);
      ndb_log_warning("Could not release slock on '%s.%s'", db, table_name);
      return 1;
    }
    ndb_log_verbose(19, "Cleared slock on '%s.%s'", db, table_name);

    (void)ndb->getDictionary()->forceGCPWait(1);

    return 0;
  }

  /**
    @brief Inform the other nodes that schema operation has been completed by
    this node. This is done by writing a new row to the ndb_schema_result table.

    @param schema The schema operation which has just been completed

    @return true if ack suceeds
    @return false if ack fails(writing to the table could not be done)

  */
  bool ack_schema_op_with_result(const Ndb_schema_op *schema) const {
    DBUG_TRACE;

    // Should only call this function if ndb_schema has a schema_op_id
    // column which enabled the client to send schema->schema_op_id != 0
    ndbcluster::ndbrequire(schema->schema_op_id);

    Ndb *ndb = m_thd_ndb->ndb;

    // Open ndb_schema_result table
    Ndb_schema_result_table schema_result_table(m_thd_ndb);
    if (!schema_result_table.open()) {
      // NOTE! Legacy crash unless this was cluster connection failure, there
      // are simply no other of way sending error back to coordinator
      ndbcluster::ndbrequire(ndb->getDictionary()->getNdbError().code == 4009);
      return false;
    }

    const NdbDictionary::Table *ndbtab = schema_result_table.get_table();
    const uint32 nodeid = schema->node_id;
    const uint32 schema_op_id = schema->schema_op_id;
    const uint32 participant_nodeid = own_nodeid();
    const uint32 result = m_schema_op_result.result();
    char message_buf[255];
    schema_result_table.pack_message(m_schema_op_result.message(), message_buf);

    // Function for inserting row with result in ndb_schema_result
    std::function<const NdbError *(NdbTransaction *)>
        ack_schema_op_with_result_func =
            [ndbtab, nodeid, schema_op_id, participant_nodeid, result,
             message_buf](NdbTransaction *trans) -> const NdbError * {
      DBUG_TRACE;

      NdbOperation *op = trans->getNdbOperation(ndbtab);
      if (op == nullptr) return &trans->getNdbError();

      /* Write row */
      if (op->insertTuple() != 0 ||
          op->equal(Ndb_schema_result_table::COL_NODEID, nodeid) != 0 ||
          op->equal(Ndb_schema_result_table::COL_SCHEMA_OP_ID, schema_op_id) !=
              0 ||
          op->equal(Ndb_schema_result_table::COL_PARTICIPANT_NODEID,
                    participant_nodeid) != 0 ||
          op->setValue(Ndb_schema_result_table::COL_RESULT, result) != 0 ||
          op->setValue(Ndb_schema_result_table::COL_MESSAGE, message_buf) != 0)
        return &op->getNdbError();

      if (trans->execute(NdbTransaction::Commit,
                         NdbOperation::DefaultAbortOption,
                         1 /*force send*/) != 0)
        return &trans->getNdbError();

      return nullptr;
    };

    NdbError ndb_err;
    if (!ndb_trans_retry(ndb, m_thd, ndb_err, ack_schema_op_with_result_func)) {
      log_NDB_error(ndb_err);
      ndb_log_warning(
          "Failed to send result for schema operation involving '%s.%s'",
          schema->db, schema->name);
      return false;
    }

    // Success
    ndb_log_verbose(19,
                    "Replied to schema operation '%s.%s(%u/%u)', nodeid: %d, "
                    "schema_op_id: %d",
                    schema->db, schema->name, schema->id, schema->version,
                    schema->node_id, schema->schema_op_id);

    return true;
  }

  void remove_schema_result_rows(uint32 schema_op_id) {
    Ndb *ndb = m_thd_ndb->ndb;

    // Open ndb_schema_result table
    Ndb_schema_result_table schema_result_table(m_thd_ndb);
    if (!schema_result_table.open()) {
      // NOTE! Legacy crash unless this was cluster connection failure, there
      // are simply no other of way sending error back to coordinator
      ndbcluster::ndbrequire(ndb->getDictionary()->getNdbError().code == 4009);
      return;
    }
    const NdbDictionary::Table *ndbtab = schema_result_table.get_table();
    const uint nodeid = own_nodeid();

    // Function for deleting all rows from ndb_schema_result matching
    // the given nodeid and schema operation id
    std::function<const NdbError *(NdbTransaction *)>
        remove_schema_result_rows_func =
            [nodeid, schema_op_id,
             ndbtab](NdbTransaction *trans) -> const NdbError * {
      DBUG_TRACE;
      DBUG_PRINT("enter",
                 ("nodeid: %d, schema_op_id: %d", nodeid, schema_op_id));

      NdbScanOperation *scan_op = trans->getNdbScanOperation(ndbtab);
      if (scan_op == nullptr) return &trans->getNdbError();

      if (scan_op->readTuples(NdbOperation::LM_Read,
                              NdbScanOperation::SF_KeyInfo) != 0)
        return &scan_op->getNdbError();

      // Read the columns to compare
      uint32 read_node_id, read_schema_op_id;
      if (scan_op->getValue(Ndb_schema_result_table::COL_NODEID,
                            (char *)&read_node_id) == nullptr ||
          scan_op->getValue(Ndb_schema_result_table::COL_SCHEMA_OP_ID,
                            (char *)&read_schema_op_id) == nullptr)
        return &scan_op->getNdbError();

      // Start the scan
      if (trans->execute(NdbTransaction::NoCommit) != 0)
        return &trans->getNdbError();

      // Loop through all rows
      unsigned deleted = 0;
      bool fetch = true;
      while (true) {
        const int r = scan_op->nextResult(fetch);
        if (r < 0) {
          // Failed to fetch next row
          return &scan_op->getNdbError();
        }
        fetch = false;  // Don't fetch more until nextResult returns 2

        switch (r) {
          case 0:  // Found row
            DBUG_PRINT("info", ("Found row"));
            // Delete rows if equal to nodeid and schema_op_id
            if (read_schema_op_id == schema_op_id && read_node_id == nodeid) {
              DBUG_PRINT("info", ("Deleting row"));
              if (scan_op->deleteCurrentTuple() != 0) {
                // Failed to delete row
                return &scan_op->getNdbError();
              }
              deleted++;
            }
            continue;

          case 1:
            DBUG_PRINT("info", ("No more rows"));
            // No more rows, commit the transation
            if (trans->execute(NdbTransaction::Commit) != 0) {
              // Failed to commit
              return &trans->getNdbError();
            }
            return nullptr;

          case 2:
            // Need to fetch more rows, first send the deletes
            DBUG_PRINT("info", ("Need to fetch more rows"));
            if (deleted > 0) {
              DBUG_PRINT("info", ("Sending deletes"));
              if (trans->execute(NdbTransaction::NoCommit) != 0) {
                // Failed to send
                return &trans->getNdbError();
              }
            }
            fetch = true;  // Fetch more rows
            continue;
        }
      }
      // Never reached
      ndbcluster::ndbrequire(false);
      return nullptr;
    };

    NdbError ndb_err;
    if (!ndb_trans_retry(ndb, m_thd, ndb_err, remove_schema_result_rows_func)) {
      log_NDB_error(ndb_err);
      ndb_log_error("Failed to remove rows from ndb_schema_result");
      return;
    }
    ndb_log_verbose(19,
                    "Deleted all rows from ndb_schema_result, nodeid: %d, "
                    "schema_op_id: %d",
                    nodeid, schema_op_id);
    return;
  }

  void check_wakeup_clients(Ndb_schema_dist::Schema_op_result_code result,
                            const char *message) const {
    // Build list of current subscribers
    std::unordered_set<uint32> subscribers;
    m_schema_dist_data.get_subscriber_list(subscribers);

    // Check all NDB_SCHEMA_OBJECTS for wakeup
    std::vector<uint32> schema_op_ids;
    NDB_SCHEMA_OBJECT::get_schema_op_ids(schema_op_ids);
    for (auto schema_op_id : schema_op_ids) {
      // Lookup NDB_SCHEMA_OBJECT from nodeid + schema_op_id
      std::unique_ptr<NDB_SCHEMA_OBJECT, decltype(&NDB_SCHEMA_OBJECT::release)>
          schema_object(NDB_SCHEMA_OBJECT::get(own_nodeid(), schema_op_id),
                        NDB_SCHEMA_OBJECT::release);
      if (schema_object == nullptr) {
        // The schema operation has already completed on this node
        continue;
      }

      const bool completed = schema_object->check_for_failed_subscribers(
          subscribers, result, message);
      if (completed) {
        // All participants have completed(or failed) -> send final ack
        ack_schema_op_final(schema_object->db(), schema_object->name());
      }
    }
  }

  bool check_is_ndb_schema_event(const Ndb_event_data *event_data) const {
    if (!event_data) {
      // Received event without event data pointer
      assert(false);
      return false;
    }

    NDB_SHARE *share = event_data->share;
    if (!share) {
      // Received event where the event_data is not properly initialized
      assert(false);
      return false;
    }
    assert(event_data->shadow_table);
    assert(event_data->ndb_value[0]);
    assert(event_data->ndb_value[1]);
    assert(Ndb_schema_dist_client::is_schema_dist_table(share->db,
                                                        share->table_name));
    return true;
  }

  void handle_after_epoch(const Ndb_schema_op *schema) {
    DBUG_TRACE;
    DBUG_PRINT("info", ("Pushing Ndb_schema_op on list to be "
                        "handled after epoch"));
    assert(!is_post_epoch());  // Only before epoch
    m_post_epoch_handle_list.push_back(schema, m_mem_root);
  }

  uint own_nodeid(void) const { return m_own_nodeid; }

  void ndbapi_invalidate_table(const char *db_name,
                               const char *table_name) const {
    DBUG_TRACE;
    Ndb_table_guard ndbtab_g(m_thd_ndb->ndb, db_name, table_name);
    ndbtab_g.invalidate();
  }

  NDB_SHARE *acquire_reference(const char *db, const char *name,
                               const char *reference) const {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("db: '%s', name: '%s'", db, name));

    char key[FN_REFLEN + 1];
    build_table_filename(key, sizeof(key) - 1, db, name, "", 0);
    NDB_SHARE *share = NDB_SHARE::acquire_reference_by_key(key, reference);
    return share;
  }

  bool has_shadow_table(Ndb_dd_client &dd_client, const char *schema_name,
                        const char *table_name) const {
    dd::String_type engine;
    if (dd_client.get_engine(schema_name, table_name, &engine) &&
        engine != "ndbcluster") {
      ndb_log_warning(
          "Local table '%s.%s' in engine = '%s' shadows the NDB table",
          schema_name, table_name, engine.c_str());
      return true;
    }
    return false;
  }

  bool install_table_in_dd(Ndb_dd_client &dd_client, const char *schema_name,
                           const char *table_name, dd::sdi_t sdi, int table_id,
                           int table_version, size_t num_partitions,
                           const std::string &tablespace_name,
                           bool force_overwrite,
                           bool invalidate_referenced_tables) const {
    DBUG_TRACE;

    // First acquire exclusive MDL lock on schema and table
    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
      log_and_clear_THD_conditions();
      ndb_log_error(
          "Failed to acquire exclusive metadata lock for table '%s.%s'",
          schema_name, table_name);
      return false;
    }

    // Check if there is existing table in DD which is not a NDB table, in such
    // case refuse to overwrite the "shadow table"
    if (has_shadow_table(dd_client, schema_name, table_name)) return false;

    if (!tablespace_name.empty()) {
      // Acquire IX MDL on tablespace
      if (!dd_client.mdl_lock_tablespace(tablespace_name.c_str(), true)) {
        log_and_clear_THD_conditions();
        ndb_log_error("Failed to acquire lock on tablespace '%s' for '%s.%s'",
                      tablespace_name.c_str(), schema_name, table_name);
        return false;
      }
    }

    Ndb_referenced_tables_invalidator invalidator(m_thd, dd_client);
    if (!dd_client.install_table(
            schema_name, table_name, sdi, table_id, table_version,
            num_partitions, tablespace_name, force_overwrite,
            (invalidate_referenced_tables ? &invalidator : nullptr))) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to install table '%s.%s' in DD", schema_name,
                    table_name);
      return false;
    }

    if (invalidate_referenced_tables && !invalidator.invalidate()) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to invalidate referenced tables for '%s.%s'",
                    schema_name, table_name);
      return false;
    }
    dd_client.commit();
    return true;
  }

  bool create_table_from_engine(
      const char *schema_name, const char *table_name, bool force_overwrite,
      bool invalidate_referenced_tables = false) const {
    DBUG_TRACE;
    DBUG_PRINT("enter",
               ("schema_name: %s, table_name: %s", schema_name, table_name));

    Ndb *ndb = m_thd_ndb->ndb;
    Ndb_table_guard ndbtab_g(ndb, schema_name, table_name);
    const NDBTAB *ndbtab = ndbtab_g.get_table();
    if (!ndbtab) {
      // Could not open the table from NDB, very unusual
      log_NDB_error(ndb->getDictionary()->getNdbError());
      ndb_log_error("Failed to open table '%s.%s' from NDB", schema_name,
                    table_name);
      return false;
    }

    const std::string tablespace_name =
        ndb_table_tablespace_name(ndb->getDictionary(), ndbtab);

    std::string serialized_metadata;
    if (!ndb_table_get_serialized_metadata(ndbtab, serialized_metadata)) {
      ndb_log_error("Failed to get serialized metadata for table '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    Ndb_dd_client dd_client(m_thd);

    // Deserialize the metadata from NDB, this is done like this in order to
    // allow the table to be setup for binlogging independently of whether it
    // works to install it into DD.
    Ndb_dd_table dd_table(m_thd);
    const dd::sdi_t sdi = serialized_metadata.c_str();
    if (!dd_client.deserialize_table(sdi, dd_table.get_table_def())) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to deserialize metadata for table '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    // Setup binlogging for this table. In many cases the NDB_SHARE, the
    // event and event subscriptions are already created/setup, but this
    // function is called anyway in order to create/setup any missing parts.
    if (ndbcluster_binlog_setup_table(m_thd, ndb, schema_name, table_name,
                                      dd_table.get_table_def())) {
      // Error information has been logged AND pushed -> clear warnings
      clear_THD_conditions();
      ndb_log_error("Failed to setup binlogging for table '%s.%s'", schema_name,
                    table_name);
      return false;
    }

    // Install the table definition in DD
    // NOTE! This is done after create/setup the NDB_SHARE to avoid that
    // server tries to open the table before the NDB_SHARE has been created
    if (!install_table_in_dd(dd_client, schema_name, table_name, sdi,
                             ndbtab->getObjectId(), ndbtab->getObjectVersion(),
                             ndbtab->getPartitionCount(), tablespace_name,
                             force_overwrite, invalidate_referenced_tables)) {
      ndb_log_warning("Failed to update table definition in DD");
      return false;
    }

    return true;
  }

  void handle_clear_slock(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());

    if (DBUG_EVALUATE_IF("ndb_binlog_random_tableid", true, false)) {
      // Try to create a race between SLOCK acks handled after another
      // schema operation on same object could have been started.

      // Get temporary NDB_SCHEMA_OBJECT, sleep if one does not exist
      std::unique_ptr<NDB_SCHEMA_OBJECT, decltype(&NDB_SCHEMA_OBJECT::release)>
          tmp_ndb_schema_obj(
              NDB_SCHEMA_OBJECT::get(schema->db, schema->name, schema->id,
                                     schema->version),
              NDB_SCHEMA_OBJECT::release);
      if (tmp_ndb_schema_obj == nullptr) {
        ndb_milli_sleep(10);
      }
    }

    // Get NDB_SCHEMA_OBJECT
    std::unique_ptr<NDB_SCHEMA_OBJECT, decltype(&NDB_SCHEMA_OBJECT::release)>
        ndb_schema_object(NDB_SCHEMA_OBJECT::get(schema->db, schema->name,
                                                 schema->id, schema->version),
                          NDB_SCHEMA_OBJECT::release);

    if (!ndb_schema_object) {
      // NOTE! When participants ack they send their own nodeid instead of the
      // nodeid of node who initiated the schema operation. This makes it
      // impossible to do special checks for the coordinator here. Assume that
      // since no NDB_SCHEMA_OBJECT was found, this node is not the coordinator
      // and the ack can be safely ignored.
      return;
    }

    // Handle ack sent from node using old protocol, all nodes cleared
    // in the slock column have completed(it's not enough to use only nodeid
    // since events are merged)
    if (bitmap_bits_set(&schema->slock) > 0) {
      ndb_log_verbose(19, "Coordinator, handle old protocol ack from node: %d",
                      schema->node_id);

      std::unordered_set<uint32> cleared_nodes;
      for (uint i = 0; i < schema->slock.n_bits; i++) {
        if (!bitmap_is_set(&schema->slock, i)) {
          // Node is not set in bitmap
          cleared_nodes.insert(i);
        }
      }
      ndb_schema_object->result_received_from_nodes(cleared_nodes);

      if (ndb_schema_object->check_all_participants_completed()) {
        // All participants have completed(or failed) -> send final ack
        ack_schema_op_final(ndb_schema_object->db(), ndb_schema_object->name());
        return;
      }

      return;
    }

    // Check if all coordinator completed and wake up client
    const bool coordinator_completed =
        ndb_schema_object->check_coordinator_completed();

    if (coordinator_completed) {
      remove_schema_result_rows(ndb_schema_object->schema_op_id());

      // Remove active schema operation from coordinator
      m_schema_dist_data.remove_active_schema_op(ndb_schema_object.get());
    }

    /**
     * There is a possible race condition between this binlog-thread,
     * which has not yet released its schema_object, and the
     * coordinator which possibly release its reference
     * to the same schema_object when signaled above.
     *
     * If the coordinator then starts yet another schema operation
     * on the same schema / table, it will need a schema_object with
     * the same key as the one already completed, and which this
     * thread still referrs. Thus, it will get this schema_object,
     * instead of creating a new one as normally expected.
     */
    if (DBUG_EVALUATE_IF("ndb_binlog_schema_object_race", true, false)) {
      ndb_milli_sleep(10);
    }
  }

  void handle_offline_alter_table_commit(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);
    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);

    NDB_SHARE *share =
        acquire_reference(schema->db, schema->name,
                          "offline_alter_table_commit");  // Temp ref.
    if (share) {
      mysql_mutex_lock(&share->mutex);
      if (share->op) {
        const Ndb_event_data *event_data =
            static_cast<const Ndb_event_data *>(share->op->getCustomData());
        Ndb_event_data::destroy(event_data);
        share->op->setCustomData(NULL);
        {
          Mutex_guard injector_mutex_g(injector_event_mutex);
          injector_ndb->dropEventOperation(share->op);
        }
        share->op = 0;
        NDB_SHARE::release_reference(share, "binlog");
      }
      mysql_mutex_unlock(&share->mutex);

      mysql_mutex_lock(&ndbcluster_mutex);
      NDB_SHARE::mark_share_dropped(&share);
      NDB_SHARE::release_reference_have_lock(share,
                                             "offline_alter_table_commit");
      // If this was the last share ref, it is now deleted. If there are more
      // references, the share will remain in the list of dropped until
      // remaining references are released.
      mysql_mutex_unlock(&ndbcluster_mutex);
    }

    // Install table from NDB, overwrite the existing table
    if (!create_table_from_engine(schema->db, schema->name,
                                  true /* force_overwrite */,
                                  true /* invalidate_referenced_tables */)) {
      ndb_log_error("Distribution of ALTER TABLE '%s.%s' failed", schema->db,
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of ALTER TABLE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }
  }

  void handle_online_alter_table_prepare(const Ndb_schema_op *schema) {
    assert(is_post_epoch());  // Always after epoch

    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);

    if (schema->node_id == own_nodeid()) {
      // Special case for schema dist participant in own node!
      // The schema dist client has exclusive MDL lock and thus
      // the schema dist participant(this code) on the same mysqld
      // can't open the table def from the DD, trying to acquire
      // another MDL lock will just block. Instead(since this is in
      // the same mysqld) it provides the new table def via a
      // pointer in the NDB_SHARE.
      NDB_SHARE *share =
          acquire_reference(schema->db, schema->name,
                            "online_alter_table_prepare");  // temporary ref.

      const dd::Table *new_table_def =
          static_cast<const dd::Table *>(share->inplace_alter_new_table_def);
      DBUG_ASSERT(new_table_def);

      // Create a new Ndb_event_data which will be used when creating
      // the new NdbEventOperation
      Ndb_event_data *event_data = Ndb_event_data::create_event_data(
          m_thd, share, share->db, share->table_name, share->key_string(),
          injector_thd, new_table_def);
      if (!event_data) {
        ndb_log_error("NDB Binlog: Failed to create event data for table %s.%s",
                      schema->db, schema->name);
        DBUG_ASSERT(false);
        // NOTE! Should abort the alter from here
      }

      // Release old prepared event_data, this is rare but will happen
      // when an inplace alter table fails between prepare and commit phase
      const Ndb_event_data *old_event_data =
          m_schema_dist_data.get_inplace_alter_event_data();
      if (old_event_data) {
        Ndb_event_data::destroy(old_event_data);
        m_schema_dist_data.save_inplace_alter_event_data(nullptr);
      }

      // Save the new event_data
      m_schema_dist_data.save_inplace_alter_event_data(event_data);

      NDB_SHARE::release_reference(share,
                                   "online_alter_table_prepare");  // temp ref.
    } else {
      write_schema_op_to_binlog(m_thd, schema);

      // Install table from NDB, overwrite the altered table.
      // NOTE! it will also try to setup binlogging but since the share
      // has a op assigned, that part will be skipped
      if (!create_table_from_engine(schema->db, schema->name,
                                    true /* force_overwrite */,
                                    true /* invalidate_referenced_tables */)) {
        ndb_log_error("Distribution of ALTER TABLE '%s.%s' failed", schema->db,
                      schema->name);
        m_schema_op_result.set_result(
            Ndb_schema_dist::SCHEMA_OP_FAILURE,
            "Distribution of ALTER TABLE " + std::string(1, '\'') +
                std::string(schema->name) + std::string(1, '\'') + " failed");
      }

      // Check that no event_data have been prepared yet(that is only
      // done on participant in same node)
      DBUG_ASSERT(m_schema_dist_data.get_inplace_alter_event_data() == nullptr);
    }
  }

  const Ndb_event_data *remote_participant_inplace_alter_create_event_data(
      NDB_SHARE *share, const char *schema_name, const char *table_name) const {
    DBUG_TRACE;

    // Read table definition from NDB, it might not exist in DD on this Server
    Ndb *ndb = m_thd_ndb->ndb;
    Ndb_table_guard ndbtab_g(ndb, schema_name, table_name);
    const NDBTAB *ndbtab = ndbtab_g.get_table();
    if (!ndbtab) {
      // Could not open the table from NDB, very unusual
      log_NDB_error(ndb->getDictionary()->getNdbError());
      ndb_log_error("Failed to open table '%s.%s' from NDB", schema_name,
                    table_name);
      return nullptr;
    }

    std::string serialized_metadata;
    if (!ndb_table_get_serialized_metadata(ndbtab, serialized_metadata)) {
      ndb_log_error("Failed to get serialized metadata for table '%s.%s'",
                    schema_name, table_name);
      return nullptr;
    }

    // Deserialize the metadata from NDB
    Ndb_dd_client dd_client(m_thd);
    Ndb_dd_table dd_table(m_thd);
    const dd::sdi_t sdi = serialized_metadata.c_str();
    if (!dd_client.deserialize_table(sdi, dd_table.get_table_def())) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to deserialize metadata for table '%s.%s'",
                    schema_name, table_name);
      return nullptr;
    }

    // Create new event_data
    Ndb_event_data *event_data = Ndb_event_data::create_event_data(
        m_thd, share, schema_name, table_name, share->key_string(),
        injector_thd, dd_table.get_table_def());
    if (!event_data) {
      ndb_log_error("NDB Binlog: Failed to create event data for table '%s.%s'",
                    share->db, share->table_name);
      return nullptr;
    }

    return event_data;
  }

  void handle_online_alter_table_commit(const Ndb_schema_op *schema) {
    assert(is_post_epoch());  // Always after epoch

    NDB_SHARE *share =
        acquire_reference(schema->db, schema->name,
                          "online_alter_table_commit");  // temporary ref.
    if (share) {
      ndb_log_verbose(9, "NDB Binlog: handling online alter/rename");

      mysql_mutex_lock(&share->mutex);

      const Ndb_event_data *event_data;
      if (schema->node_id == own_nodeid()) {
        // Get the event_data which has been created during prepare phase
        event_data = m_schema_dist_data.get_inplace_alter_event_data();
        if (!event_data) {
          ndb_log_error("Failed to get prepared event data '%s'",
                        share->key_string());
          DBUG_ASSERT(false);
        }
        // The event_data pointer has been taken over
        m_schema_dist_data.save_inplace_alter_event_data(nullptr);
      } else {
        // Create Ndb_event_data which will be used when creating
        // the new NdbEventOperation.
        event_data = remote_participant_inplace_alter_create_event_data(
            share, share->db, share->table_name);
        if (!event_data) {
          ndb_log_error("Failed to create event data for table '%s'",
                        share->key_string());
          DBUG_ASSERT(false);
        }
      }
      DBUG_ASSERT(event_data);

      NdbEventOperation *new_op = nullptr;
      if (share->op && event_data /* safety */) {
        Ndb_binlog_client binlog_client(m_thd, schema->db, schema->name);
        // The table have an event operation setup and during an inplace
        // alter table that need to be recreated for the new table layout.
        // NOTE! Nothing has changed here regarding whether or not the
        // table should still have event operation, i.e if it had
        // it before, it should still have it after the alter. But
        // for consistency, check that table should have event op
        DBUG_ASSERT(binlog_client.table_should_have_event_op(share));

        // Save the current event operation since create_event_op()
        // will assign the new in "share->op", also release the "binlog"
        // reference as it will be acquired again in create_event_op()
        // NOTE! This should probably be rewritten to not assign share->op and
        // acquire the reference in create_event_op()
        NdbEventOperation *const curr_op = share->op;
        share->op = nullptr;
        NDB_SHARE::release_reference(share, "binlog");

        // Get table from NDB
        Ndb_table_guard ndbtab_g(m_thd_ndb->ndb, schema->db, schema->name);
        const NDBTAB *ndbtab = ndbtab_g.get_table();

        DBUG_ASSERT(ndbtab != nullptr);

        // Create new NdbEventOperation
        if (binlog_client.create_event_op(share, ndbtab, event_data)) {
          ndb_log_error("Failed to create event operation for table '%s'",
                        share->key_string());

          // NOTE! Should fail the alter here
          DBUG_ASSERT(false);
        } else {
          // Get the newly created NdbEventOperation, will be swapped
          // into place (again) later
          new_op = share->op;
        }

        // Reinstall the current NdbEventOperation
        share->op = curr_op;
      } else {
        // New event_data was created(that's the default) but the table didn't
        // have event operations and thus the event_data is unused, free it
        Ndb_event_data::destroy(event_data);
      }

      ndb_log_verbose(9, "NDB Binlog: handling online alter/rename done");

      // There should be no event_data left in m_schema_dist_data at this point
      DBUG_ASSERT(m_schema_dist_data.get_inplace_alter_event_data() == nullptr);

      // Start using the new event operation and release the old
      if (share->op && new_op) {
        // Delete old event_data
        const Ndb_event_data *event_data =
            static_cast<const Ndb_event_data *>(share->op->getCustomData());
        share->op->setCustomData(NULL);
        Ndb_event_data::destroy(event_data);

        // Drop old event operation
        {
          Mutex_guard injector_mutex_g(injector_event_mutex);
          injector_ndb->dropEventOperation(share->op);
        }
        // Install new event operation
        share->op = new_op;
      }
      mysql_mutex_unlock(&share->mutex);

      NDB_SHARE::release_reference(share,
                                   "online_alter_table_commit");  // temp ref.
    }

    DBUG_ASSERT(m_schema_dist_data.get_inplace_alter_event_data() == nullptr);
  }

  bool remove_table_from_dd(const char *schema_name, const char *table_name) {
    DBUG_TRACE;

    Ndb_dd_client dd_client(m_thd);
    Ndb_referenced_tables_invalidator invalidator(m_thd, dd_client);

    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
      log_and_clear_THD_conditions();
      ndb_log_warning("Failed to acquire exclusive metadata lock on '%s.%s'",
                      schema_name, table_name);
      return false;
    }

    // Check if there is existing table in DD which is not a NDB table, in such
    // case refuse to remove the "shadow table"
    if (has_shadow_table(dd_client, schema_name, table_name)) return false;

    if (!dd_client.remove_table(schema_name, table_name, &invalidator)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to remove table '%s.%s' from DD", schema_name,
                    table_name);
      return false;
    }

    if (!invalidator.invalidate()) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to invalidate referenced tables for '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    dd_client.commit();
    return true;
  }

  void handle_drop_table(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(m_thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    if (!remove_table_from_dd(schema->db, schema->name)) {
      // The table couldn't be removed, continue to invalidate the table in
      // NdbApi, close cached tables etc. This case may happen when a MySQL
      // Server drops a "shadow" table and afterwards someone drops also the
      // table with same name in NDB
      ndb_log_warning(
          "Failed to remove table definition from DD, continue anyway...");
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP TABLE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }

    NDB_SHARE *share = acquire_reference(schema->db, schema->name,
                                         "drop_table");  // temporary ref.
    if (!share || !share->op) {
      ndbapi_invalidate_table(schema->db, schema->name);
      ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
    }
    if (share) {
      mysql_mutex_lock(&ndbcluster_mutex);
      NDB_SHARE::mark_share_dropped(&share);  // server ref.
      DBUG_ASSERT(share);                     // Should still be ref'ed
      NDB_SHARE::release_reference_have_lock(share,
                                             "drop_table");  // temporary ref.
      mysql_mutex_unlock(&ndbcluster_mutex);
    }

    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
  }

  /*
    The RENAME is performed in two steps.
    1) PREPARE_RENAME - sends the new table key to participants
    2) RENAME - perform the actual rename
  */

  void handle_rename_table_prepare(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) return;

    const char *new_key_for_table = schema->query;
    DBUG_PRINT("info", ("new_key_for_table: '%s'", new_key_for_table));

    // Release potentially previously prepared new_key
    {
      NDB_SHARE_KEY *old_prepared_key =
          m_schema_dist_data.get_prepared_rename_key();
      if (old_prepared_key) NDB_SHARE::free_key(old_prepared_key);
    }

    // Create a new key save it, then hope for the best(i.e
    // that it can be found later when the RENAME arrives)
    NDB_SHARE_KEY *new_prepared_key = NDB_SHARE::create_key(new_key_for_table);
    m_schema_dist_data.save_prepared_rename_key(new_prepared_key);
  }

  bool rename_table_in_dd(const char *schema_name, const char *table_name,
                          const char *new_schema_name,
                          const char *new_table_name,
                          const NdbDictionary::Table *ndbtab,
                          const std::string &tablespace_name) const {
    DBUG_TRACE;

    Ndb_dd_client dd_client(m_thd);

    // Acquire exclusive MDL lock on the table
    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to acquire exclusive metadata lock on '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    // Acquire exclusive MDL lock also on the new table name
    if (!dd_client.mdl_locks_acquire_exclusive(new_schema_name,
                                               new_table_name)) {
      log_and_clear_THD_conditions();
      ndb_log_error(
          "Failed to acquire exclusive metadata lock on new table name '%s.%s'",
          new_schema_name, new_table_name);
      return false;
    }

    if (has_shadow_table(dd_client, schema_name, table_name)) {
      // The renamed table was a "shadow table".

      if (has_shadow_table(dd_client, new_schema_name, new_table_name)) {
        // The new table name is also a "shadow table", nothing todo
        return false;
      }

      // Install the renamed table into DD
      std::string serialized_metadata;
      if (!ndb_table_get_serialized_metadata(ndbtab, serialized_metadata)) {
        ndb_log_error("Failed to get serialized metadata for table '%s.%s'",
                      new_schema_name, new_table_name);
        return false;
      }

      // Deserialize the metadata from NDB
      Ndb_dd_table dd_table(m_thd);
      const dd::sdi_t sdi = serialized_metadata.c_str();
      if (!dd_client.deserialize_table(sdi, dd_table.get_table_def())) {
        log_and_clear_THD_conditions();
        ndb_log_error("Failed to deserialized metadata for table '%s.%s'",
                      new_schema_name, new_table_name);
        return false;
      }

      if (!dd_client.install_table(
              new_schema_name, new_table_name, sdi, ndbtab->getObjectId(),
              ndbtab->getObjectVersion(), ndbtab->getPartitionCount(),
              tablespace_name, true, nullptr)) {
        log_and_clear_THD_conditions();
        ndb_log_error("Failed to install renamed table '%s.%s' in DD",
                      new_schema_name, new_table_name);
        return false;
      }

      dd_client.commit();
      return true;
    }

    Ndb_referenced_tables_invalidator invalidator(m_thd, dd_client);

    if (has_shadow_table(dd_client, new_schema_name, new_table_name)) {
      // There is a "shadow table", remove the table from DD
      ndb_log_warning(
          "Removing the renamed table '%s.%s' from DD, there is a local table",
          schema_name, table_name);
      if (!dd_client.remove_table(schema_name, table_name, &invalidator)) {
        log_and_clear_THD_conditions();
        ndb_log_error("Failed to remove the renamed table '%s.%s' from DD",
                      schema_name, table_name);
        return false;
      }
    } else {
      // There are no "shadow table", rename table in DD
      if (!dd_client.rename_table(schema_name, table_name, new_schema_name,
                                  new_table_name, ndbtab->getObjectId(),
                                  ndbtab->getObjectVersion(), &invalidator)) {
        log_and_clear_THD_conditions();
        ndb_log_error("Failed to rename table '%s.%s' to '%s.%s", schema_name,
                      table_name, new_schema_name, new_table_name);
        return false;
      }
    }

    if (!invalidator.invalidate()) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to invalidate referenced tables for '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    dd_client.commit();
    return true;
  }

  void handle_rename_table(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(m_thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    NDB_SHARE *share = acquire_reference(schema->db, schema->name,
                                         "rename_table");  // temporary ref.
    if (!share || !share->op) {
      ndbapi_invalidate_table(schema->db, schema->name);
      ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
    }
    if (share)
      NDB_SHARE::release_reference(share, "rename_table");  // temporary ref.

    share = acquire_reference(schema->db, schema->name,
                              "rename_table");  // temporary ref.
    if (!share) {
      // The RENAME need to find share so it can be renamed
      DBUG_ASSERT(share);
      return;
    }

    NDB_SHARE_KEY *prepared_key = m_schema_dist_data.get_prepared_rename_key();
    if (!prepared_key) {
      // The rename need to have new_key set
      // by a previous RENAME_PREPARE
      DBUG_ASSERT(prepared_key);
      return;
    }

    // Rename on participant is always from real to
    // real name(i.e neiher old or new name should be a temporary name)
    DBUG_ASSERT(!ndb_name_is_temp(schema->name));
    DBUG_ASSERT(!ndb_name_is_temp(NDB_SHARE::key_get_table_name(prepared_key)));

    // Open the renamed table from NDB
    const char *new_db_name = NDB_SHARE::key_get_db_name(prepared_key);
    const char *new_table_name = NDB_SHARE::key_get_table_name(prepared_key);
    Ndb_table_guard ndbtab_g(m_thd_ndb->ndb, new_db_name, new_table_name);
    const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
    if (!ndbtab) {
      // Could not open the table from NDB, very unusual
      log_NDB_error(m_thd_ndb->ndb->getDictionary()->getNdbError());
      ndb_log_error("Failed to rename, could not open table '%s.%s' from NDB",
                    new_db_name, new_table_name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of RENAME TABLE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    const std::string tablespace_name =
        ndb_table_tablespace_name(m_thd_ndb->ndb->getDictionary(), ndbtab);

    // Rename table in DD
    if (!rename_table_in_dd(schema->db, schema->name, new_db_name,
                            new_table_name, ndbtab, tablespace_name)) {
      ndb_log_warning(
          "Failed to rename table definition in DD, continue anyway...");
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of RENAME TABLE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }

    // Rename share and release the old key
    NDB_SHARE_KEY *old_key = share->key;
    NDB_SHARE::rename_share(share, prepared_key);
    m_schema_dist_data.save_prepared_rename_key(NULL);
    NDB_SHARE::free_key(old_key);

    NDB_SHARE::release_reference(share, "rename_table");  // temporary ref.

    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
  }

  void handle_drop_db(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(m_thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    Ndb_dd_client dd_client(m_thd);

    // Lock the schema in DD
    if (!dd_client.mdl_lock_schema(schema->db)) {
      // Failed to acquire lock, skip dropping
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to acquire MDL for db '%s'", schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    bool schema_exists;
    if (!dd_client.schema_exists(schema->db, &schema_exists)) {
      // Failed to check if database exists, skip dropping
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to determine if database '%s' exists", schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    if (!schema_exists) {
      DBUG_PRINT("info", ("Schema '%s' does not exist", schema->db));
      // Nothing to do
      return;
    }

    // Remove all NDB tables in the dropped database from DD,
    // this function is only called when they all have been dropped
    // from NDB by another MySQL Server
    //
    // NOTE! This is code which always run "in the server" so it would be
    // appropriate to log error messages to the server log file describing
    // any problems which occur in these functions.
    std::unordered_set<std::string> ndb_tables_in_DD;
    if (!dd_client.get_ndb_table_names_in_schema(schema->db,
                                                 &ndb_tables_in_DD)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to get list of NDB tables in database '%s'",
                    schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    Ndb_referenced_tables_invalidator invalidator(m_thd, dd_client);

    for (const auto &ndb_table_name : ndb_tables_in_DD) {
      if (!dd_client.mdl_locks_acquire_exclusive(schema->db,
                                                 ndb_table_name.c_str())) {
        log_and_clear_THD_conditions();
        ndb_log_warning("Failed to acquire exclusive MDL on '%s.%s'",
                        schema->db, ndb_table_name.c_str());
        continue;
      }

      if (!dd_client.remove_table(schema->db, ndb_table_name.c_str(),
                                  &invalidator)) {
        // Failed to remove the table from DD, not much else to do
        // than try with the next
        log_and_clear_THD_conditions();
        ndb_log_error("Failed to remove table '%s.%s' from DD", schema->db,
                      ndb_table_name.c_str());
        continue;
      }

      NDB_SHARE *share = acquire_reference(schema->db, ndb_table_name.c_str(),
                                           "drop_db");  // temporary ref.
      if (!share || !share->op) {
        ndbapi_invalidate_table(schema->db, ndb_table_name.c_str());
        ndb_tdc_close_cached_table(m_thd, schema->db, ndb_table_name.c_str());
      }
      if (share) {
        mysql_mutex_lock(&ndbcluster_mutex);
        NDB_SHARE::mark_share_dropped(&share);  // server ref.
        DBUG_ASSERT(share);                     // Should still be ref'ed
        NDB_SHARE::release_reference_have_lock(share,
                                               "drop_db");  // temporary ref.
        mysql_mutex_unlock(&ndbcluster_mutex);
      }

      ndbapi_invalidate_table(schema->db, ndb_table_name.c_str());
      ndb_tdc_close_cached_table(m_thd, schema->db, ndb_table_name.c_str());
    }

    if (!invalidator.invalidate()) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to invalidate referenced tables for database '%s'",
                    schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    dd_client.commit();

    bool found_local_tables;
    if (!dd_client.have_local_tables_in_schema(schema->db,
                                               &found_local_tables)) {
      // Failed to access the DD to check if non NDB tables existed, assume
      // the worst and skip dropping this database
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to check if database '%s' contained local tables.",
                    schema->db);
      ndb_log_error("Skipping drop of non NDB database artifacts.");
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    DBUG_PRINT("exit", ("found_local_tables: %d", found_local_tables));

    if (found_local_tables) {
      /* Tables exists as a local table, print error and leave it */
      ndb_log_warning(
          "NDB Binlog: Skipping drop database '%s' since "
          "it contained local tables "
          "binlog schema event '%s' from node %d. ",
          schema->db, schema->query, schema->node_id);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    // Run the plain DROP DATABASE query in order to remove other artifacts
    // like the physical database directory.
    // Note! This is not done in the case where a "shadow" table is found
    // in the schema, but at least all the NDB tables have in such case
    // already been removed from the DD
    Ndb_local_connection mysqld(m_thd);
    if (mysqld.drop_database(schema->db)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to execute 'DROP DATABASE' for database '%s'",
                    schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
    }
  }

  void handle_truncate_table(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    NDB_SHARE *share =
        acquire_reference(schema->db, schema->name, "truncate_table");
    // invalidation already handled by binlog thread
    if (!share || !share->op) {
      ndbapi_invalidate_table(schema->db, schema->name);
      ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
    }
    if (share) {
      // Reset the tables shared auto_increment counter
      share->reset_tuple_id_range();

      NDB_SHARE::release_reference(share, "truncate_table");  // temporary ref.
    }

    if (!create_table_from_engine(schema->db, schema->name,
                                  true /* force_overwrite */)) {
      ndb_log_error("Distribution of TRUNCATE TABLE '%s.%s' failed", schema->db,
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of TRUNCATE TABLE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }
  }

  void handle_create_table(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    if (!create_table_from_engine(schema->db, schema->name,
                                  true, /* force_overwrite */
                                  true /* invalidate_referenced_tables */)) {
      ndb_log_error("Distribution of CREATE TABLE '%s.%s' failed", schema->db,
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of CREATE TABLE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }
  }

  void handle_create_db(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(m_thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    Ndb_local_connection mysqld(m_thd);
    if (mysqld.execute_database_ddl(schema->query)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to execute 'CREATE DATABASE' for database '%s'",
                    schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of CREATE DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    // Update the Schema in DD with the id and version details
    if (!ndb_dd_update_schema_version(m_thd, schema->db, schema->id,
                                      schema->version)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to update schema version for database '%s'",
                    schema->db);
    }
  }

  void handle_alter_db(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) return;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(m_thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    Ndb_local_connection mysqld(m_thd);
    if (mysqld.execute_database_ddl(schema->query)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to execute 'ALTER DATABASE' for database '%s'",
                    schema->db);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of ALTER DATABASE " + std::string(1, '\'') +
              std::string(schema->db) + std::string(1, '\'') + " failed");
      return;
    }

    // Update the Schema in DD with the id and version details
    if (!ndb_dd_update_schema_version(m_thd, schema->db, schema->id,
                                      schema->version)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to update schema version for database '%s'",
                    schema->db);
    }
  }

  void handle_grant_op(const Ndb_schema_op *schema) {
    DBUG_TRACE;
    Ndb_local_connection sql_runner(m_thd);

    assert(!is_post_epoch());  // Always directly

    // Participant never takes GSL
    assert(
        get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    if (schema->node_id == own_nodeid()) return;

    /* SOT_GRANT was sent by a pre-8.0 mysqld. Just ignore it. */
    if (schema->type == SOT_GRANT) {
      ndb_log_verbose(9, "Got SOT_GRANT event, disregarding.");
      return;
    }

    /* For SOT_ACL_SNAPSHOT, update the snapshots for the users listed.
     */
    if (schema->type == SOT_ACL_SNAPSHOT) {
      if (!Ndb_stored_grants::update_users_from_snapshot(m_thd,
                                                         schema->query)) {
        ndb_log_error("Failed to apply ACL snapshot for users: %s",
                      schema->query);
        m_schema_op_result.set_result(Ndb_schema_dist::SCHEMA_OP_FAILURE,
                                      "Distribution of ACL change failed");
      }
      return;
    }

    DBUG_ASSERT(schema->type == SOT_ACL_STATEMENT ||
                schema->type == SOT_ACL_STATEMENT_REFRESH);

    LEX_CSTRING thd_db_save = m_thd->db();

    std::string use_db(schema->db);
    std::string query(schema->query);

    if (!query.compare(0, 4, "use ")) {
      size_t delimiter = query.find_first_of(';');
      use_db = query.substr(4, delimiter - 4);
      query = query.substr(delimiter + 1);
    }

    /* Execute ACL query */
    LEX_CSTRING set_db = {use_db.c_str(), use_db.length()};
    m_thd->reset_db(set_db);
    ndb_log_verbose(40, "Using database: %s", use_db.c_str());
    if (sql_runner.run_acl_statement(query)) {
      ndb_log_error("Failed to execute ACL query: %s", query.c_str());
      m_schema_op_result.set_result(Ndb_schema_dist::SCHEMA_OP_FAILURE,
                                    "Distribution of ACL change failed");
      m_thd->reset_db(thd_db_save);
      return;
    }

    /* Reset database */
    m_thd->reset_db(thd_db_save);

    if (schema->type == SOT_ACL_STATEMENT_REFRESH) {
      Ndb_stored_grants::maintain_cache(m_thd);
    }
  }

  bool create_tablespace_from_engine(Ndb_dd_client &dd_client,
                                     const char *tablespace_name, uint32 id,
                                     uint32 version) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("tablespace_name: %s, id: %u, version: %u",
                         tablespace_name, id, version));

    Ndb *ndb = m_thd_ndb->ndb;
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    std::vector<std::string> datafile_names;
    if (!ndb_get_datafile_names(dict, tablespace_name, &datafile_names)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get data files assigned to tablespace '%s'",
                    tablespace_name);
      return false;
    }

    if (!dd_client.mdl_lock_tablespace_exclusive(tablespace_name)) {
      log_and_clear_THD_conditions();
      ndb_log_error("MDL lock could not be acquired for tablespace '%s'",
                    tablespace_name);
      return false;
    }

    if (!dd_client.install_tablespace(tablespace_name, datafile_names, id,
                                      version, true /* force_overwrite */)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to install tablespace '%s' in DD", tablespace_name);
      return false;
    }

    return true;
  }

  void handle_create_tablespace(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) {
      return;
    }

    write_schema_op_to_binlog(m_thd, schema);

    Ndb_dd_client dd_client(m_thd);
    if (!create_tablespace_from_engine(dd_client, schema->name, schema->id,
                                       schema->version)) {
      ndb_log_error("Distribution of CREATE TABLESPACE '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of CREATE TABLESPACE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }
    dd_client.commit();
  }

  bool get_tablespace_table_refs(
      const char *name,
      std::vector<dd::Tablespace_table_ref> &table_refs) const {
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_tablespace(name, true /* intention_exclusive */)) {
      log_and_clear_THD_conditions();
      ndb_log_error("MDL lock could not be acquired on tablespace '%s'", name);
      return false;
    }

    const dd::Tablespace *existing = nullptr;
    if (!dd_client.get_tablespace(name, &existing)) {
      log_and_clear_THD_conditions();
      return false;
    }

    if (existing == nullptr) {
      // Tablespace doesn't exist, no need to update tables after the ALTER
      return true;
    }

    if (!ndb_dd_disk_data_get_table_refs(m_thd, *existing, table_refs)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to get table refs in tablespace '%s'", name);
      return false;
    }
    return true;
  }

  bool update_tablespace_id_in_tables(
      Ndb_dd_client &dd_client, const char *tablespace_name,
      const std::vector<dd::Tablespace_table_ref> &table_refs) const {
    if (!dd_client.mdl_lock_tablespace(tablespace_name,
                                       true /* intention_exclusive */)) {
      log_and_clear_THD_conditions();
      ndb_log_error("MDL lock could not be acquired on tablespace '%s'",
                    tablespace_name);
      return false;
    }

    dd::Object_id tablespace_id;
    if (!dd_client.lookup_tablespace_id(tablespace_name, &tablespace_id)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to retrieve object id of tablespace '%s'",
                    tablespace_name);
      return false;
    }

    for (auto &table_ref : table_refs) {
      // Convert table_refs to correct case when necessary
      const std::string schema_name =
          ndb_dd_fs_name_case(table_ref.m_schema_name.c_str());
      const std::string table_name =
          ndb_dd_fs_name_case(table_ref.m_name.c_str());
      if (!dd_client.mdl_locks_acquire_exclusive(schema_name.c_str(),
                                                 table_name.c_str())) {
        log_and_clear_THD_conditions();
        ndb_log_error("MDL lock could not be acquired on table '%s.%s'",
                      schema_name.c_str(), table_name.c_str());
        return false;
      }

      if (!dd_client.set_tablespace_id_in_table(
              schema_name.c_str(), table_name.c_str(), tablespace_id)) {
        log_and_clear_THD_conditions();
        ndb_log_error("Could not set tablespace id in table '%s.%s'",
                      schema_name.c_str(), table_name.c_str());
        return false;
      }
    }
    return true;
  }

  void handle_alter_tablespace(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) {
      return;
    }

    write_schema_op_to_binlog(m_thd, schema);

    // Get information about tables in the tablespace being ALTERed. This is
    // required for after the ALTER as the tablespace id of the every table
    // should be updated
    std::vector<dd::Tablespace_table_ref> table_refs;
    if (!get_tablespace_table_refs(schema->name, table_refs)) {
      ndb_log_error("Distribution of ALTER TABLESPACE '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of ALTER TABLESPACE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    Ndb_dd_client dd_client(m_thd);
    if (!create_tablespace_from_engine(dd_client, schema->name, schema->id,
                                       schema->version)) {
      ndb_log_error("Distribution of ALTER TABLESPACE '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of ALTER TABLESPACE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    if (!table_refs.empty()) {
      // Update tables in the tablespace with the new tablespace id
      if (!update_tablespace_id_in_tables(dd_client, schema->name,
                                          table_refs)) {
        ndb_log_error(
            "Failed to update tables in tablespace '%s' with the "
            "new tablespace id",
            schema->name);
        ndb_log_error("Distribution of ALTER TABLESPACE '%s' failed",
                      schema->name);
        m_schema_op_result.set_result(
            Ndb_schema_dist::SCHEMA_OP_FAILURE,
            "Distribution of ALTER TABLESPACE " + std::string(1, '\'') +
                std::string(schema->name) + std::string(1, '\'') + " failed");
        return;
      }
    }
    dd_client.commit();
  }

  void handle_drop_tablespace(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) {
      return;
    }

    write_schema_op_to_binlog(m_thd, schema);

    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_tablespace_exclusive(schema->name)) {
      log_and_clear_THD_conditions();
      ndb_log_error("MDL lock could not be acquired for tablespace '%s'",
                    schema->name);
      ndb_log_error("Distribution of DROP TABLESPACE '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP TABLESPACE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    if (!dd_client.drop_tablespace(schema->name,
                                   false /* fail_if_not_exists */)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to drop tablespace '%s' from DD", schema->name);
      ndb_log_error("Distribution of DROP TABLESPACE '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP TABLESPACE " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    dd_client.commit();
  }

  bool create_logfile_group_from_engine(const char *logfile_group_name,
                                        uint32 id, uint32 version) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("logfile_group_name: %s, id: %u, version: %u",
                         logfile_group_name, id, version));

    Ndb *ndb = m_thd_ndb->ndb;
    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    std::vector<std::string> undofile_names;
    if (!ndb_get_undofile_names(dict, logfile_group_name, &undofile_names)) {
      log_NDB_error(dict->getNdbError());
      ndb_log_error("Failed to get undo files assigned to logfile group '%s'",
                    logfile_group_name);
      return false;
    }

    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_logfile_group_exclusive(logfile_group_name)) {
      log_and_clear_THD_conditions();
      ndb_log_error("MDL lock could not be acquired for logfile group '%s'",
                    logfile_group_name);
      return false;
    }

    if (!dd_client.install_logfile_group(logfile_group_name, undofile_names, id,
                                         version, true /* force_overwrite */)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to install logfile group '%s' in DD",
                    logfile_group_name);
      return false;
    }

    dd_client.commit();
    return true;
  }

  void handle_create_logfile_group(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) {
      return;
    }

    write_schema_op_to_binlog(m_thd, schema);

    if (!create_logfile_group_from_engine(schema->name, schema->id,
                                          schema->version)) {
      ndb_log_error("Distribution of CREATE LOGFILE GROUP '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of CREATE LOGFILE GROUP " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }
  }

  void handle_alter_logfile_group(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(!is_post_epoch());  // Always directly

    if (schema->node_id == own_nodeid()) {
      return;
    }

    write_schema_op_to_binlog(m_thd, schema);

    if (!create_logfile_group_from_engine(schema->name, schema->id,
                                          schema->version)) {
      ndb_log_error("Distribution of ALTER LOGFILE GROUP '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of ALTER LOGFILE GROUP " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
    }
  }

  void handle_drop_logfile_group(const Ndb_schema_op *schema) {
    DBUG_TRACE;

    assert(is_post_epoch());  // Always after epoch

    if (schema->node_id == own_nodeid()) {
      return;
    }

    write_schema_op_to_binlog(m_thd, schema);

    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_logfile_group_exclusive(schema->name)) {
      log_and_clear_THD_conditions();
      ndb_log_error("MDL lock could not be acquired for logfile group '%s'",
                    schema->name);
      ndb_log_error("Distribution of DROP LOGFILE GROUP '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP LOGFILE GROUP " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    if (!dd_client.drop_logfile_group(schema->name,
                                      false /* fail_if_not_exists */)) {
      log_and_clear_THD_conditions();
      ndb_log_error("Failed to drop logfile group '%s' from DD", schema->name);
      ndb_log_error("Distribution of DROP LOGFILE GROUP '%s' failed",
                    schema->name);
      m_schema_op_result.set_result(
          Ndb_schema_dist::SCHEMA_OP_FAILURE,
          "Distribution of DROP LOGFILE GROUP " + std::string(1, '\'') +
              std::string(schema->name) + std::string(1, '\'') + " failed");
      return;
    }

    dd_client.commit();
  }

  int handle_schema_op(const Ndb_schema_op *schema) {
    DBUG_TRACE;
    {
      const SCHEMA_OP_TYPE schema_type = (SCHEMA_OP_TYPE)schema->type;

      ndb_log_verbose(19,
                      "got schema event on '%s.%s(%u/%u)' query: '%s' "
                      "type: %s(%d) node: %u slock: %x%08x",
                      schema->db, schema->name, schema->id, schema->version,
                      schema->query,
                      Ndb_schema_dist_client::type_name(
                          static_cast<SCHEMA_OP_TYPE>(schema->type)),
                      schema_type, schema->node_id, schema->slock.bitmap[1],
                      schema->slock.bitmap[0]);

      DBUG_EXECUTE_IF("ndb_schema_op_start_crash", DBUG_SUICIDE(););

      // Return to simulate schema operation timeout
      DBUG_EXECUTE_IF("ndb_schema_op_start_timeout", return 0;);

      if ((schema->db[0] == 0) && (schema->name[0] == 0)) {
        /**
         * This happens if there is a schema event on a table (object)
         *   that this mysqld does not know about.
         *   E.g it had a local table shadowing a ndb table...
         */
        return 0;
      }

      if (schema_type == SOT_CLEAR_SLOCK) {
        // Handle the ack after epoch to ensure that schema events are inserted
        // in the binlog after any data events
        handle_after_epoch(schema);
        return 0;
      }

      if (schema->node_id == own_nodeid()) {
        // This is the Coordinator who hear about this schema operation for
        // the first time. Save the list of current subscribers as participants
        // in the NDB_SCHEMA_OBJECT, those are the nodes who need to acknowledge
        // (or fail) before the schema operation is completed.
        std::unique_ptr<NDB_SCHEMA_OBJECT,
                        decltype(&NDB_SCHEMA_OBJECT::release)>
            ndb_schema_object(
                NDB_SCHEMA_OBJECT::get(schema->db, schema->name, schema->id,
                                       schema->version),
                NDB_SCHEMA_OBJECT::release);
        if (!ndb_schema_object) {
          // There is no NDB_SCHEMA_OBJECT waiting for this schema operation
          // Unexpected since the client who started this schema op
          // is always in same node as coordinator
          ndbcluster::ndbrequire(false);
          return 0;
        }
        std::unordered_set<uint32> subscribers;
        m_schema_dist_data.get_subscriber_list(subscribers);
        ndb_schema_object->register_participants(subscribers);
        ndb_log_verbose(
            19, "Participants: %s",
            ndb_schema_object->waiting_participants_to_string().c_str());

        // Add active schema operation to coordinator
        m_schema_dist_data.add_active_schema_op(ndb_schema_object.get());

        // Test schema dist client killed
        if (DBUG_EVALUATE_IF("ndb_schema_dist_client_killed", true, false)) {
          // Wait until the Client has set "coordinator completed"
          while (!ndb_schema_object->check_coordinator_completed())
            ndb_milli_sleep(100);
        }
      }

      // Set the custom lock_wait_timeout for schema distribution
      Lock_wait_timeout_guard lwt_guard(m_thd,
                                        opt_ndb_schema_dist_lock_wait_timeout);

      Ndb_schema_op_result schema_op_result;
      switch (schema_type) {
        case SOT_CLEAR_SLOCK:
          // Already handled above, should never end up here
          ndbcluster::ndbrequire(schema_type != SOT_CLEAR_SLOCK);
          return 0;

        case SOT_ALTER_TABLE_COMMIT:
        case SOT_RENAME_TABLE_PREPARE:
        case SOT_ONLINE_ALTER_TABLE_PREPARE:
        case SOT_ONLINE_ALTER_TABLE_COMMIT:
        case SOT_RENAME_TABLE:
        case SOT_DROP_TABLE:
        case SOT_DROP_DB:
        case SOT_DROP_TABLESPACE:
        case SOT_DROP_LOGFILE_GROUP:
          handle_after_epoch(schema);
          return 0;

        case SOT_TRUNCATE_TABLE:
          handle_truncate_table(schema);
          break;

        case SOT_CREATE_TABLE:
          handle_create_table(schema);
          break;

        case SOT_CREATE_DB:
          handle_create_db(schema);
          break;

        case SOT_ALTER_DB:
          handle_alter_db(schema);
          break;

        case SOT_CREATE_USER:
        case SOT_DROP_USER:
        case SOT_RENAME_USER:
        case SOT_GRANT:
        case SOT_REVOKE:
        case SOT_ACL_SNAPSHOT:
        case SOT_ACL_STATEMENT:
        case SOT_ACL_STATEMENT_REFRESH:
          handle_grant_op(schema);
          break;

        case SOT_TABLESPACE:
        case SOT_LOGFILE_GROUP:
          if (schema->node_id == own_nodeid()) break;
          write_schema_op_to_binlog(m_thd, schema);
          break;

        case SOT_RENAME_TABLE_NEW:
          /*
            Only very old MySQL Server connected to the cluster may
            send this schema operation, ignore it
          */
          ndb_log_error(
              "Skipping old schema operation"
              "(RENAME_TABLE_NEW) on %s.%s",
              schema->db, schema->name);
          DBUG_ASSERT(false);
          break;

        case SOT_CREATE_TABLESPACE:
          handle_create_tablespace(schema);
          break;

        case SOT_ALTER_TABLESPACE:
          handle_alter_tablespace(schema);
          break;

        case SOT_CREATE_LOGFILE_GROUP:
          handle_create_logfile_group(schema);
          break;

        case SOT_ALTER_LOGFILE_GROUP:
          handle_alter_logfile_group(schema);
          break;
      }

      if (schema->schema_op_id) {
        // Use new protocol
        if (!ack_schema_op_with_result(schema)) {
          // Fallback to old protocol as stop gap, no result will be returned
          // but at least the coordinator will be informed
          ack_schema_op(schema);
        }
      } else {
        // Use old protocol
        ack_schema_op(schema);
      }
    }

    // Errors should have been reported to log and then cleared
    DBUG_ASSERT(!m_thd->is_error());

    return 0;
  }

  void handle_schema_op_post_epoch(const Ndb_schema_op *schema) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("%s.%s: query: '%s'  type: %d", schema->db,
                         schema->name, schema->query, schema->type));

    // Set the custom lock_wait_timeout for schema distribution
    Lock_wait_timeout_guard lwt_guard(m_thd,
                                      opt_ndb_schema_dist_lock_wait_timeout);

    {
      const SCHEMA_OP_TYPE schema_type = (SCHEMA_OP_TYPE)schema->type;
      ndb_log_verbose(9, "%s - %s.%s",
                      Ndb_schema_dist_client::type_name(
                          static_cast<SCHEMA_OP_TYPE>(schema->type)),
                      schema->db, schema->name);

      switch (schema_type) {
        case SOT_DROP_DB:
          handle_drop_db(schema);
          break;

        case SOT_DROP_TABLE:
          handle_drop_table(schema);
          break;

        case SOT_RENAME_TABLE_PREPARE:
          handle_rename_table_prepare(schema);
          break;

        case SOT_RENAME_TABLE:
          handle_rename_table(schema);
          break;

        case SOT_ALTER_TABLE_COMMIT:
          handle_offline_alter_table_commit(schema);
          break;

        case SOT_ONLINE_ALTER_TABLE_PREPARE:
          handle_online_alter_table_prepare(schema);
          break;

        case SOT_ONLINE_ALTER_TABLE_COMMIT:
          handle_online_alter_table_commit(schema);
          break;

        case SOT_DROP_TABLESPACE:
          handle_drop_tablespace(schema);
          break;

        case SOT_DROP_LOGFILE_GROUP:
          handle_drop_logfile_group(schema);
          break;

        default:
          DBUG_ASSERT(false);
      }
    }

    // Errors should have been reported to log and then cleared
    DBUG_ASSERT(!m_thd->is_error());

    // There should be no MDL locks left now
    DBUG_ASSERT(!m_thd->mdl_context.has_locks());

    return;
  }

  THD *const m_thd;
  Thd_ndb *const m_thd_ndb;
  MEM_ROOT *m_mem_root;
  uint m_own_nodeid;
  Ndb_schema_dist_data &m_schema_dist_data;
  Ndb_schema_op_result m_schema_op_result;
  bool m_post_epoch;

  bool is_post_epoch(void) const { return m_post_epoch; }

  List<const Ndb_schema_op> m_post_epoch_handle_list;

 public:
  Ndb_schema_event_handler() = delete;
  Ndb_schema_event_handler(const Ndb_schema_event_handler &) = delete;

  Ndb_schema_event_handler(THD *thd, MEM_ROOT *mem_root, uint own_nodeid,
                           Ndb_schema_dist_data &schema_dist_data)
      : m_thd(thd),
        m_thd_ndb(get_thd_ndb(thd)),
        m_mem_root(mem_root),
        m_own_nodeid(own_nodeid),
        m_schema_dist_data(schema_dist_data),
        m_post_epoch(false) {}

  ~Ndb_schema_event_handler() {
    // There should be no work left todo...
    DBUG_ASSERT(m_post_epoch_handle_list.elements == 0);
  }

  void handle_schema_result_insert(uint32 nodeid, uint32 schema_op_id,
                                   uint32 participant_node_id, uint32 result,
                                   const std::string &message) {
    DBUG_TRACE;
    if (nodeid != own_nodeid()) {
      // Only the coordinator handle these events
      return;
    }

    // Unpack the message received
    Ndb_schema_result_table schema_result_table(m_thd_ndb);
    const std::string unpacked_message =
        schema_result_table.unpack_message(message);

    ndb_log_verbose(
        19,
        "Received ndb_schema_result insert, nodeid: %d, schema_op_id: %d, "
        "participant_node_id: %d, result: %d, message: '%s'",
        nodeid, schema_op_id, participant_node_id, result,
        unpacked_message.c_str());

    // Lookup NDB_SCHEMA_OBJECT from nodeid + schema_op_id
    std::unique_ptr<NDB_SCHEMA_OBJECT, decltype(&NDB_SCHEMA_OBJECT::release)>
        ndb_schema_object(NDB_SCHEMA_OBJECT::get(nodeid, schema_op_id),
                          NDB_SCHEMA_OBJECT::release);
    if (ndb_schema_object == nullptr) {
      // The schema operation has already completed on this node
      return;
    }

    ndb_schema_object->result_received_from_node(participant_node_id, result,
                                                 unpacked_message);

    if (ndb_schema_object->check_all_participants_completed()) {
      // All participants have completed(or failed) -> send final ack
      ack_schema_op_final(ndb_schema_object->db(), ndb_schema_object->name());
    }
  }

  void handle_schema_result_event(Ndb *s_ndb, NdbEventOperation *pOp,
                                  NdbDictionary::Event::TableEvent event_type,
                                  const Ndb_event_data *event_data) {
    // Test "coordinator abort active" by simulating cluster failure
    if (DBUG_EVALUATE_IF("ndb_schema_dist_coord_abort_active", true, false)) {
      ndb_log_info("Simulating cluster failure...");
      event_type = NdbDictionary::Event::TE_CLUSTER_FAILURE;
    }

    switch (event_type) {
      case NdbDictionary::Event::TE_INSERT:
        handle_schema_result_insert(
            event_data->unpack_uint32(0), event_data->unpack_uint32(1),
            event_data->unpack_uint32(2), event_data->unpack_uint32(3),
            event_data->unpack_string(4));
        break;

      case NdbDictionary::Event::TE_CLUSTER_FAILURE:
        // fall through
      case NdbDictionary::Event::TE_DROP:
        // Cluster failure or ndb_schema_result table dropped
        if (ndb_binlog_tables_inited && ndb_binlog_running)
          ndb_log_verbose(1, "NDB Binlog: util tables need to reinitialize");

        // Indicate util tables not ready
        mysql_mutex_lock(&injector_data_mutex);
        ndb_binlog_tables_inited = false;
        ndb_binlog_is_ready = false;
        mysql_mutex_unlock(&injector_data_mutex);

        ndb_tdc_close_cached_tables();

        // Tear down the event subscription on ndb_schema_result
        ndbcluster_binlog_event_operation_teardown(m_thd, s_ndb, pOp);
        break;

      default:
        // Ignore other event types
        break;
    }
    return;
  }

  void handle_event(Ndb *s_ndb, NdbEventOperation *pOp) {
    DBUG_TRACE;

    const Ndb_event_data *event_data =
        static_cast<const Ndb_event_data *>(pOp->getCustomData());
    if (Ndb_schema_dist_client::is_schema_dist_result_table(
            event_data->share->db, event_data->share->table_name)) {
      // Received event on ndb_schema_result table
      handle_schema_result_event(s_ndb, pOp, pOp->getEventType(), event_data);
      return;
    }

    if (!check_is_ndb_schema_event(event_data)) return;

    NDBEVENT::TableEvent ev_type = pOp->getEventType();

    // Test "fail all schema ops" by simulating cluster failure
    // before the schema operation has been registered
    if (DBUG_EVALUATE_IF("ndb_schema_dist_coord_fail_all", true, false)) {
      ndb_log_info("Simulating cluster failure...");
      ev_type = NdbDictionary::Event::TE_CLUSTER_FAILURE;
    }

    // Test "client detect not ready" by simulating cluster failure
    if (DBUG_EVALUATE_IF("ndb_schema_dist_client_not_ready", true, false)) {
      ndb_log_info("Simulating cluster failure...");
      ev_type = NdbDictionary::Event::TE_CLUSTER_FAILURE;
      // There should be one NDB_SCHEMA_OBJECT registered
      ndbcluster::ndbrequire(NDB_SCHEMA_OBJECT::count_active_schema_ops() == 1);
    }

    switch (ev_type) {
      case NDBEVENT::TE_INSERT:
      case NDBEVENT::TE_UPDATE: {
        /* ndb_schema table, row INSERTed or UPDATEed*/
        const Ndb_schema_op *schema_op =
            Ndb_schema_op::create(event_data, pOp->getAnyValue());
        handle_schema_op(schema_op);
        break;
      }

      case NDBEVENT::TE_DELETE:
        /* ndb_schema table, row DELETEd */
        break;

      case NDBEVENT::TE_CLUSTER_FAILURE:
        ndb_log_verbose(1, "cluster failure at epoch %u/%u.",
                        (uint)(pOp->getGCI() >> 32), (uint)(pOp->getGCI()));

        // fall through
      case NDBEVENT::TE_DROP:
        /* ndb_schema table DROPped */
        if (ndb_binlog_tables_inited && ndb_binlog_running)
          ndb_log_verbose(1, "NDB Binlog: util tables need to reinitialize");

        // Indicate util tables not ready
        mysql_mutex_lock(&injector_data_mutex);
        ndb_binlog_tables_inited = false;
        ndb_binlog_is_ready = false;
        mysql_mutex_unlock(&injector_data_mutex);

        ndb_tdc_close_cached_tables();

        ndbcluster_binlog_event_operation_teardown(m_thd, s_ndb, pOp);

        if (DBUG_EVALUATE_IF("ndb_schema_dist_client_not_ready", true, false)) {
          ndb_log_info("Wait for client to detect not ready...");
          while (NDB_SCHEMA_OBJECT::count_active_schema_ops() > 0)
            ndb_milli_sleep(100);
        }
        break;

      case NDBEVENT::TE_ALTER:
        /* ndb_schema table ALTERed */
        break;

      case NDBEVENT::TE_NODE_FAILURE: {
        /* Remove all subscribers for node */
        m_schema_dist_data.report_data_node_failure(pOp->getNdbdNodeId());
        check_wakeup_clients(Ndb_schema_dist::NODE_FAILURE, "Data node failed");
        break;
      }

      case NDBEVENT::TE_SUBSCRIBE: {
        /* Add node as subscriber */
        m_schema_dist_data.report_subscribe(pOp->getNdbdNodeId(),
                                            pOp->getReqNodeId());
        // No 'check_wakeup_clients', adding subscribers doesn't complete
        // anything
        break;
      }

      case NDBEVENT::TE_UNSUBSCRIBE: {
        /* Remove node as subscriber */
        m_schema_dist_data.report_unsubscribe(pOp->getNdbdNodeId(),
                                              pOp->getReqNodeId());
        check_wakeup_clients(Ndb_schema_dist::NODE_UNSUBSCRIBE,
                             "Node unsubscribed");
        break;
      }

      default: {
        ndb_log_error("unknown event %u, ignoring...", ev_type);
      }
    }

    return;
  }

  void check_active_schema_ops(ulonglong current_epoch) {
    // This function is called repeatedly as epochs pass but checks should only
    // be performed at regular intervals. Check if it's time for one now and
    // calculate the time for next if time is up
    if (likely(!m_schema_dist_data.time_for_check())) return;

    const uint active_ops = m_schema_dist_data.active_schema_ops().size();
    if (likely(active_ops == 0)) return;  // Nothing to do at this time

    ndb_log_info(
        "Coordinator checking active schema operations, "
        "epochs: (%u/%u,%u/%u,%u/%u), proc_info: '%s'",
        (uint)(ndb_latest_handled_binlog_epoch >> 32),
        (uint)(ndb_latest_handled_binlog_epoch),
        (uint)(ndb_latest_received_binlog_epoch >> 32),
        (uint)(ndb_latest_received_binlog_epoch), (uint)(current_epoch >> 32),
        (uint)(current_epoch), m_thd->proc_info);

    for (const NDB_SCHEMA_OBJECT *schema_object :
         m_schema_dist_data.active_schema_ops()) {
      // Print into about this schema operation
      ndb_log_info(" - schema operation active on '%s.%s'", schema_object->db(),
                   schema_object->name());
      if (ndb_log_get_verbose_level() > 30) {
        ndb_log_error_dump("%s", schema_object->to_string().c_str());
      }

      // Check if schema operation has timed out
      const bool completed = schema_object->check_timeout(
          opt_ndb_schema_dist_timeout, Ndb_schema_dist::NODE_TIMEOUT,
          "Participant timeout");
      if (completed) {
        ndb_log_warning("Schema dist coordinator detected timeout");
        // Timeout occured -> send final ack to complete the schema opration
        ack_schema_op_final(schema_object->db(), schema_object->name());
      }
    }
  }

  void post_epoch(ulonglong ndb_latest_epoch) {
    if (unlikely(m_post_epoch_handle_list.elements > 0)) {
      // Set the flag used to check that functions are called at correct time
      m_post_epoch = true;

      /*
       process any operations that should be done after
       the epoch is complete
      */
      const Ndb_schema_op *schema;
      while ((schema = m_post_epoch_handle_list.pop())) {
        if (schema->type == SOT_CLEAR_SLOCK) {
          handle_clear_slock(schema);
          continue;  // Handled an ack -> don't send new ack
        }

        handle_schema_op_post_epoch(schema);
        if (schema->schema_op_id) {
          // Use new protocol
          if (!ack_schema_op_with_result(schema)) {
            // Fallback to old protocol as stop gap, no result will be returned
            // but at least the coordinator will be informed
            ack_schema_op(schema);
          }
        } else {
          // Use old protocol
          ack_schema_op(schema);
        }
      }
    }

    check_active_schema_ops(ndb_latest_epoch);

    // There should be no work left todo...
    DBUG_ASSERT(m_post_epoch_handle_list.elements == 0);
  }
};

/*********************************************************************
  Internal helper functions for handling of the cluster replication tables
  - ndb_binlog_index
  - ndb_apply_status
*********************************************************************/

/*
  struct to hold the data to be inserted into the
  ndb_binlog_index table
*/
struct ndb_binlog_index_row {
  ulonglong epoch;
  const char *start_master_log_file;
  ulonglong start_master_log_pos;
  ulong n_inserts;
  ulong n_updates;
  ulong n_deletes;
  ulong n_schemaops;

  ulong orig_server_id;
  ulonglong orig_epoch;

  ulong gci;

  const char *next_master_log_file;
  ulonglong next_master_log_pos;

  struct ndb_binlog_index_row *next;
};

/**
  Utility class encapsulating the code which open and writes
  to the mysql.ndb_binlog_index table
*/
class Ndb_binlog_index_table_util {
  static constexpr const char *const DB_NAME = "mysql";
  static constexpr const char *const TABLE_NAME = "ndb_binlog_index";
  /*
    Open the ndb_binlog_index table for writing
  */
  static int open_binlog_index_table(THD *thd, TABLE **ndb_binlog_index) {
    const char *save_proc_info =
        thd_proc_info(thd, "Opening 'mysql.ndb_binlog_index'");

    TABLE_LIST tables(DB_NAME,     // db
                      TABLE_NAME,  // name, alias
                      TL_WRITE);   // for write

    /* Only allow real table to be opened */
    tables.required_type = dd::enum_table_type::BASE_TABLE;

    const uint flags =
        MYSQL_LOCK_IGNORE_TIMEOUT; /* Wait for lock "infinitely" */
    if (open_and_lock_tables(thd, &tables, flags)) {
      if (thd->killed)
        DBUG_PRINT("error", ("NDB Binlog: Opening ndb_binlog_index: killed"));
      else
        ndb_log_error("NDB Binlog: Opening ndb_binlog_index: %d, '%s'",
                      thd->get_stmt_da()->mysql_errno(),
                      thd->get_stmt_da()->message_text());
      thd_proc_info(thd, save_proc_info);
      return -1;
    }
    *ndb_binlog_index = tables.table;
    thd_proc_info(thd, save_proc_info);
    return 0;
  }

  /*
    Write rows to the ndb_binlog_index table
  */
  static int write_rows_impl(THD *thd, ndb_binlog_index_row *row) {
    int error = 0;
    ndb_binlog_index_row *first = row;
    TABLE *ndb_binlog_index = 0;
    // Save previous option settings
    ulonglong option_bits = thd->variables.option_bits;

    /*
      Assume this function is not called with an error set in thd
      (but clear for safety in release version)
     */
    assert(!thd->is_error());
    thd->clear_error();

    /*
      Turn off binlogging to prevent the table changes to be written to
      the binary log.
    */
    Disable_binlog_guard binlog_guard(thd);

    if (open_binlog_index_table(thd, &ndb_binlog_index)) {
      if (thd->killed)
        DBUG_PRINT(
            "error",
            ("NDB Binlog: Unable to lock table ndb_binlog_index, killed"));
      else
        ndb_log_error("NDB Binlog: Unable to lock table ndb_binlog_index");
      error = -1;
      goto add_ndb_binlog_index_err;
    }

    // Set all columns to be written
    ndb_binlog_index->use_all_columns();

    // Turn off autocommit to do all writes in one transaction
    thd->variables.option_bits |= OPTION_NOT_AUTOCOMMIT;
    do {
      ulonglong epoch = 0, orig_epoch = 0;
      uint orig_server_id = 0;

      // Intialize ndb_binlog_index->record[0]
      empty_record(ndb_binlog_index);

      ndb_binlog_index->field[NBICOL_START_POS]->store(
          first->start_master_log_pos, true);
      ndb_binlog_index->field[NBICOL_START_FILE]->store(
          first->start_master_log_file,
          (uint)strlen(first->start_master_log_file), &my_charset_bin);
      ndb_binlog_index->field[NBICOL_EPOCH]->store(epoch = first->epoch, true);
      if (ndb_binlog_index->s->fields > NBICOL_ORIG_SERVERID) {
        /* Table has ORIG_SERVERID / ORIG_EPOCH columns.
         * Write rows with different ORIG_SERVERID / ORIG_EPOCH
         * separately
         */
        ndb_binlog_index->field[NBICOL_NUM_INSERTS]->store(row->n_inserts,
                                                           true);
        ndb_binlog_index->field[NBICOL_NUM_UPDATES]->store(row->n_updates,
                                                           true);
        ndb_binlog_index->field[NBICOL_NUM_DELETES]->store(row->n_deletes,
                                                           true);
        ndb_binlog_index->field[NBICOL_NUM_SCHEMAOPS]->store(row->n_schemaops,
                                                             true);
        ndb_binlog_index->field[NBICOL_ORIG_SERVERID]->store(
            orig_server_id = row->orig_server_id, true);
        ndb_binlog_index->field[NBICOL_ORIG_EPOCH]->store(
            orig_epoch = row->orig_epoch, true);
        ndb_binlog_index->field[NBICOL_GCI]->store(first->gci, true);

        if (ndb_binlog_index->s->fields > NBICOL_NEXT_POS) {
          /* Table has next log pos fields, fill them in */
          ndb_binlog_index->field[NBICOL_NEXT_POS]->store(
              first->next_master_log_pos, true);
          ndb_binlog_index->field[NBICOL_NEXT_FILE]->store(
              first->next_master_log_file,
              (uint)strlen(first->next_master_log_file), &my_charset_bin);
        }
        row = row->next;
      } else {
        /* Old schema : Table has no separate
         * ORIG_SERVERID / ORIG_EPOCH columns.
         * Merge operation counts and write one row
         */
        while ((row = row->next)) {
          first->n_inserts += row->n_inserts;
          first->n_updates += row->n_updates;
          first->n_deletes += row->n_deletes;
          first->n_schemaops += row->n_schemaops;
        }
        ndb_binlog_index->field[NBICOL_NUM_INSERTS]->store(
            (ulonglong)first->n_inserts, true);
        ndb_binlog_index->field[NBICOL_NUM_UPDATES]->store(
            (ulonglong)first->n_updates, true);
        ndb_binlog_index->field[NBICOL_NUM_DELETES]->store(
            (ulonglong)first->n_deletes, true);
        ndb_binlog_index->field[NBICOL_NUM_SCHEMAOPS]->store(
            (ulonglong)first->n_schemaops, true);
      }

      error = ndb_binlog_index->file->ha_write_row(ndb_binlog_index->record[0]);

      /* Fault injection to test logging */
      if (DBUG_EVALUATE_IF("ndb_injector_binlog_index_write_fail_random", true,
                           false)) {
        if ((((uint32)rand()) % 10) == 9) {
          ndb_log_error("NDB Binlog: Injecting random write failure");
          error =
              ndb_binlog_index->file->ha_write_row(ndb_binlog_index->record[0]);
        }
      }

      if (error) {
        ndb_log_error(
            "NDB Binlog: Failed writing to ndb_binlog_index for "
            "epoch %u/%u orig_server_id %u orig_epoch %u/%u "
            "with error %d.",
            uint(epoch >> 32), uint(epoch), orig_server_id,
            uint(orig_epoch >> 32), uint(orig_epoch), error);

        bool seen_error_row = false;
        ndb_binlog_index_row *cursor = first;
        do {
          char tmp[128];
          if (ndb_binlog_index->s->fields > NBICOL_ORIG_SERVERID)
            snprintf(tmp, sizeof(tmp), "%u/%u,%u,%u/%u", uint(epoch >> 32),
                     uint(epoch), uint(cursor->orig_server_id),
                     uint(cursor->orig_epoch >> 32), uint(cursor->orig_epoch));

          else
            snprintf(tmp, sizeof(tmp), "%u/%u", uint(epoch >> 32), uint(epoch));

          bool error_row = (row == (cursor->next));
          ndb_log_error(
              "NDB Binlog: Writing row (%s) to ndb_binlog_index - %s", tmp,
              (error_row ? "ERROR" : (seen_error_row ? "Discarded" : "OK")));
          seen_error_row |= error_row;

        } while ((cursor = cursor->next));

        error = -1;
        goto add_ndb_binlog_index_err;
      }
    } while (row);

  add_ndb_binlog_index_err:
    /*
      Explicitly commit or rollback the writes.
      If we fail to commit we rollback.
      Note, trans_rollback_stmt() is defined to never fail.
    */
    thd->get_stmt_da()->set_overwrite_status(true);
    if (error) {
      // Error, rollback
      trans_rollback_stmt(thd);
    } else {
      assert(!thd->is_error());
      // Commit
      const bool failed = trans_commit_stmt(thd);
      if (failed || thd->transaction_rollback_request) {
        /*
          Transaction failed to commit or
          was rolled back internally by the engine
          print an error message in the log and return the
          error, which will cause replication to stop.
        */
        error = thd->get_stmt_da()->mysql_errno();
        ndb_log_error(
            "NDB Binlog: Failed committing transaction to "
            "ndb_binlog_index with error %d.",
            error);
        trans_rollback_stmt(thd);
      }
    }

    thd->get_stmt_da()->set_overwrite_status(false);

    // Restore previous option settings
    thd->variables.option_bits = option_bits;

    // Close the tables this thread has opened
    close_thread_tables(thd);

    // Release MDL locks on the opened table
    thd->mdl_context.release_transactional_locks();

    return error;
  }

  /*
    Write rows to the ndb_binlog_index table using a separate THD
    to avoid the write being killed
  */
  static void write_rows_with_new_thd(ndb_binlog_index_row *rows) {
    // Create a new THD and retry the write
    THD *new_thd = new THD;
    new_thd->set_new_thread_id();
    new_thd->thread_stack = (char *)&new_thd;
    new_thd->store_globals();
    new_thd->set_command(COM_DAEMON);
    new_thd->system_thread = SYSTEM_THREAD_NDBCLUSTER_BINLOG;
    new_thd->get_protocol_classic()->set_client_capabilities(0);
    new_thd->security_context()->skip_grants();
    new_thd->set_current_stmt_binlog_format_row();

    // Retry the write
    const int retry_result = write_rows_impl(new_thd, rows);
    if (retry_result) {
      ndb_log_error(
          "NDB Binlog: Failed writing to ndb_binlog_index table "
          "while retrying after kill during shutdown");
      DBUG_ASSERT(false);  // Crash in debug compile
    }

    new_thd->restore_globals();
    delete new_thd;
  }

 public:
  /*
    Write rows to the ndb_binlog_index table
  */
  static inline int write_rows(THD *thd, ndb_binlog_index_row *rows) {
    return write_rows_impl(thd, rows);
  }

  /*
    Retry write rows to the ndb_binlog_index table after the THD
    has been killed (which should only happen during mysqld shutdown).

    NOTE! The reason that the session(aka. THD) is being killed is that
    it's in the global list of session and mysqld thus ask it to stop
    during shutdown by setting the "killed" flag. It's not possible to
    prevent the THD from being killed and instead a brand new THD is
    used which is not in the global list of sessions. Furthermore it's
    a feature to have the THD in the list of global session since it
    should show up in SHOW PROCESSLIST.
  */
  static void write_rows_retry_after_kill(THD *orig_thd,
                                          ndb_binlog_index_row *rows) {
    // Should only be called when original THD has been killed
    DBUG_ASSERT(orig_thd->is_killed());

    write_rows_with_new_thd(rows);

    // Relink this thread with original THD
    orig_thd->store_globals();
  }

  /*
    @brief Remove all rows from mysql.ndb_binlog_index table that contain
    references to the given binlog filename.

    @note this function modifies THD state. Caller must ensure that
    the passed in THD is not affected by these changes. Presumably
    the state fixes should be moved down into Ndb_local_connection.

    @param thd The thread handle
    @param filename Name of the binlog file whose references should be removed

    @return true if failure to delete from the table occurs
  */

  static bool remove_rows_for_file(THD *thd, const char *filename) {
    Ndb_local_connection mysqld(thd);

    // Set isolation level to be independent from server settings
    thd->variables.transaction_isolation = ISO_REPEATABLE_READ;

    // Turn autocommit on, this will make delete_rows() commit
    thd->variables.option_bits &= ~OPTION_NOT_AUTOCOMMIT;

    // Ensure that file paths are escaped in a way that does not
    // interfere with path separator on Windows
    thd->variables.sql_mode |= MODE_NO_BACKSLASH_ESCAPES;

    // ignore "table does not exist" as it is a "consistent" behavior
    const bool ignore_no_such_table = true;
    std::string where;
    where.append("File='").append(filename).append("'");
    if (mysqld.delete_rows(DB_NAME, TABLE_NAME, ignore_no_such_table, where)) {
      // Failed
      return true;
    }
    return false;
  }
};
constexpr const char *const Ndb_binlog_index_table_util::DB_NAME;
constexpr const char *const Ndb_binlog_index_table_util::TABLE_NAME;

// Wrapper function allowing Ndb_binlog_index_table_util::remove_rows_for_file()
// to be forward declared
static bool ndbcluster_binlog_index_remove_file(THD *thd,
                                                const char *filename) {
  return Ndb_binlog_index_table_util::remove_rows_for_file(thd, filename);
}

/*********************************************************************
  Functions for start, stop, wait for ndbcluster binlog thread
*********************************************************************/

int ndbcluster_binlog_start() {
  DBUG_TRACE;

  if (::server_id == 0) {
    ndb_log_warning(
        "server id set to zero - changes logged to "
        "binlog with server id zero will be logged with "
        "another server id by slave mysqlds");
  }

  /*
     Check that ServerId is not using the reserved bit or bits reserved
     for application use
  */
  if ((::server_id & 0x1 << 31) ||                             // Reserved bit
      !ndbcluster_anyvalue_is_serverid_in_range(::server_id))  // server_id_bits
  {
    ndb_log_error(
        "server id provided is too large to be represented in "
        "opt_server_id_bits or is reserved");
    return -1;
  }

  /*
     Check that v2 events are enabled if log-transaction-id is set
  */
  if (opt_ndb_log_transaction_id && log_bin_use_v1_row_events) {
    ndb_log_error(
        "--ndb-log-transaction-id requires v2 Binlog row events "
        "but server is using v1.");
    return -1;
  }

  ndb_binlog_thread.init();

  /**
   * Note that injector_event_mutex is init'ed as a 'SLOW' mutex.
   * This is required as a FAST mutex could starve a waiter thread
   * forever if the thread holding the lock holds it for long.
   * See my_thread_global_init() which explicit warns about this.
   */
  mysql_mutex_init(PSI_INSTRUMENT_ME, &injector_event_mutex,
                   MY_MUTEX_INIT_SLOW);
  mysql_cond_init(PSI_INSTRUMENT_ME, &injector_data_cond);
  mysql_mutex_init(PSI_INSTRUMENT_ME, &injector_data_mutex, MY_MUTEX_INIT_FAST);

  // The binlog thread globals has been initied and should be freed
  ndbcluster_binlog_inited = 1;

  /* Start ndb binlog thread */
  if (ndb_binlog_thread.start()) {
    DBUG_PRINT("error", ("Could not start ndb binlog thread"));
    return -1;
  }

  return 0;
}

void ndbcluster_binlog_set_server_started() {
  ndb_binlog_thread.set_server_started();
}

void NDB_SHARE::set_binlog_flags(Ndb_binlog_type ndb_binlog_type) {
  DBUG_TRACE;
  switch (ndb_binlog_type) {
    case NBT_NO_LOGGING:
      DBUG_PRINT("info", ("NBT_NO_LOGGING"));
      flags |= NDB_SHARE::FLAG_NO_BINLOG;
      return;
    case NBT_DEFAULT:
      DBUG_PRINT("info", ("NBT_DEFAULT"));
      if (opt_ndb_log_updated_only) {
        flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      } else {
        flags |= NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      }
      if (opt_ndb_log_update_as_write) {
        flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      } else {
        flags |= NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      }
      if (opt_ndb_log_update_minimal) {
        flags |= NDB_SHARE::FLAG_BINLOG_MODE_MINIMAL_UPDATE;
      }
      break;
    case NBT_UPDATED_ONLY:
      DBUG_PRINT("info", ("NBT_UPDATED_ONLY"));
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      break;
    case NBT_USE_UPDATE:
      DBUG_PRINT("info", ("NBT_USE_UPDATE"));
      // fall through
    case NBT_UPDATED_ONLY_USE_UPDATE:
      DBUG_PRINT("info", ("NBT_UPDATED_ONLY_USE_UPDATE"));
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      break;
    case NBT_FULL:
      DBUG_PRINT("info", ("NBT_FULL"));
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      break;
    case NBT_FULL_USE_UPDATE:
      DBUG_PRINT("info", ("NBT_FULL_USE_UPDATE"));
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      break;
    case NBT_UPDATED_ONLY_MINIMAL:
      DBUG_PRINT("info", ("NBT_UPDATED_ONLY_MINIMAL"));
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_MINIMAL_UPDATE;
      break;
    case NBT_UPDATED_FULL_MINIMAL:
      DBUG_PRINT("info", ("NBT_UPDATED_FULL_MINIMAL"));
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_FULL;
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_MINIMAL_UPDATE;
      break;
    default:
      return;
  }
  flags &= ~NDB_SHARE::FLAG_NO_BINLOG;
}

/*
  Ndb_binlog_client::read_replication_info

  This function retrieves the data for the given table
  from the ndb_replication table.

  If the table is not found, or the table does not exist,
  then defaults are returned.
*/
bool Ndb_binlog_client::read_replication_info(
    Ndb *ndb, const char *db, const char *table_name, uint server_id,
    uint32 *binlog_flags, const st_conflict_fn_def **conflict_fn,
    st_conflict_fn_arg *args, uint *num_args) {
  DBUG_TRACE;

  /* Override for ndb_apply_status when logging */
  if (opt_ndb_log_apply_status) {
    if (Ndb_apply_status_table::is_apply_status_table(db, table_name)) {
      // Ensure to get all columns from ndb_apply_status updates and that events
      // are always logged as WRITES.
      ndb_log_info(
          "ndb-log-apply-status forcing 'mysql.ndb_apply_status' to FULL "
          "USE_WRITE");
      *binlog_flags = NBT_FULL;
      *conflict_fn = NULL;
      *num_args = 0;
      return false;
    }
  }

  Ndb_rep_tab_reader rep_tab_reader;

  int const rc = rep_tab_reader.lookup(ndb, db, table_name, server_id);

  if (rc == 0) {
    // lookup() may return a warning although it succeeds
    const char *msg = rep_tab_reader.get_warning_message();
    if (msg != NULL) {
      push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                          ER_NDB_REPLICATION_SCHEMA_ERROR,
                          ER_THD(m_thd, ER_NDB_REPLICATION_SCHEMA_ERROR), msg);
      ndb_log_warning("NDB Binlog: %s", msg);
    }
  } else {
    /* When rep_tab_reader.lookup() returns with non-zero error code,
    it must give a warning message describing why it failed*/
    const char *msg = rep_tab_reader.get_warning_message();
    DBUG_ASSERT(msg);
    my_error(ER_NDB_REPLICATION_SCHEMA_ERROR, MYF(0), msg);
    ndb_log_warning("NDB Binlog: %s", msg);
    return true;
  }

  *binlog_flags = rep_tab_reader.get_binlog_flags();
  const char *conflict_fn_spec = rep_tab_reader.get_conflict_fn_spec();

  if (conflict_fn_spec != NULL) {
    char msgbuf[FN_REFLEN];
    if (parse_conflict_fn_spec(conflict_fn_spec, conflict_fn, args, num_args,
                               msgbuf, sizeof(msgbuf)) != 0) {
      my_error(ER_CONFLICT_FN_PARSE_ERROR, MYF(0), msgbuf);

      /*
        Log as well, useful for contexts where the thd's stack of
        warnings are ignored
      */
      ndb_log_warning(
          "NDB Slave: Table %s.%s : Parse error on conflict fn : %s", db,
          table_name, msgbuf);

      return true;
    }
  } else {
    /* No conflict function specified */
    conflict_fn = NULL;
    num_args = 0;
  }

  return false;
}

int Ndb_binlog_client::apply_replication_info(
    Ndb *ndb, NDB_SHARE *share, const NdbDictionary::Table *ndbtab,
    const st_conflict_fn_def *conflict_fn, const st_conflict_fn_arg *args,
    uint num_args, uint32 binlog_flags) {
  DBUG_TRACE;
  char tmp_buf[FN_REFLEN];

  DBUG_PRINT("info", ("Setting binlog flags to %u", binlog_flags));
  share->set_binlog_flags((enum Ndb_binlog_type)binlog_flags);

  if (conflict_fn != NULL) {
    if (setup_conflict_fn(ndb, &share->m_cfn_share, share->db,
                          share->table_name, share->get_binlog_use_update(),
                          ndbtab, tmp_buf, sizeof(tmp_buf), conflict_fn, args,
                          num_args) == 0) {
      ndb_log_verbose(1, "NDB Slave: %s", tmp_buf);
    } else {
      /*
        Dump setup failure message to error log
        for cases where thd warning stack is
        ignored
      */
      ndb_log_warning("NDB Slave: Table %s.%s : %s", share->db,
                      share->table_name, tmp_buf);

      push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                          ER_CONFLICT_FN_PARSE_ERROR,
                          ER_THD(m_thd, ER_CONFLICT_FN_PARSE_ERROR), tmp_buf);

      return -1;
    }
  } else {
    /* No conflict function specified */
    slave_reset_conflict_fn(share->m_cfn_share);
  }

  return 0;
}

int Ndb_binlog_client::read_and_apply_replication_info(
    Ndb *ndb, NDB_SHARE *share, const NdbDictionary::Table *ndbtab,
    uint server_id) {
  DBUG_TRACE;
  uint32 binlog_flags;
  const st_conflict_fn_def *conflict_fn = NULL;
  st_conflict_fn_arg args[MAX_CONFLICT_ARGS];
  uint num_args = MAX_CONFLICT_ARGS;

  if (read_replication_info(ndb, share->db, share->table_name, server_id,
                            &binlog_flags, &conflict_fn, args, &num_args) ||
      apply_replication_info(ndb, share, ndbtab, conflict_fn, args, num_args,
                             binlog_flags)) {
    return -1;
  }

  return 0;
}

/*
  Common function for setting up everything for logging a table at
  create/discover.
*/
static int ndbcluster_setup_binlog_for_share(THD *thd, Ndb *ndb,
                                             NDB_SHARE *share,
                                             const dd::Table *table_def) {
  DBUG_TRACE;

  // This function should not be used to setup binlogging
  // of tables with temporary names.
  DBUG_ASSERT(!ndb_name_is_temp(share->table_name));

  Mutex_guard share_g(share->mutex);
  if (share->op != 0) {
    DBUG_PRINT("info", ("binlogging already setup"));
    return 0;
  }

  Ndb_binlog_client binlog_client(thd, share->db, share->table_name);

  Ndb_table_guard ndbtab_g(ndb, share->db, share->table_name);
  const NDBTAB *ndbtab = ndbtab_g.get_table();
  if (ndbtab == 0) {
    const NdbError ndb_error = ndb->getDictionary()->getNdbError();
    ndb_log_verbose(1,
                    "NDB Binlog: Failed to open table '%s' from NDB, "
                    "error: '%d - %s'",
                    share->key_string(), ndb_error.code, ndb_error.message);
    return -1;  // error
  }

  if (binlog_client.read_and_apply_replication_info(ndb, share, ndbtab,
                                                    ::server_id)) {
    ndb_log_error(
        "NDB Binlog: Failed to read and apply replication "
        "info for table '%s'",
        share->key_string());
    return -1;
  }

  if (binlog_client.table_should_have_event(share, ndbtab)) {
    // Check if the event already exists in NDB, otherwise create it
    if (!binlog_client.event_exists_for_table(ndb, share)) {
      // The event din't exist, create the event in NDB
      if (binlog_client.create_event(ndb, ndbtab, share)) {
        // Failed to create event
        return -1;
      }
    }

    if (binlog_client.table_should_have_event_op(share)) {
      // Create the NDB event operation on the event
      Ndb_event_data *event_data;
      if (!binlog_client.create_event_data(share, table_def, &event_data) ||
          binlog_client.create_event_op(share, ndbtab, event_data)) {
        // Failed to create event data or event operation
        return -1;
      }
    }
  }

  return 0;
}

int ndbcluster_binlog_setup_table(THD *thd, Ndb *ndb, const char *db,
                                  const char *table_name,
                                  const dd::Table *table_def) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s', table_name: '%s'", db, table_name));
  DBUG_ASSERT(table_def);

  DBUG_ASSERT(!ndb_name_is_blob_prefix(table_name));

  // Create key for ndbcluster_open_tables
  char key[FN_REFLEN + 1];
  {
    char *end = key + build_table_filename(key, sizeof(key) - 1, db, "", "", 0);
    end += tablename_to_filename(table_name, end,
                                 (uint)(sizeof(key) - (end - key)));
  }

  mysql_mutex_lock(&ndbcluster_mutex);

  // Check if NDB_SHARE for this table already exist
  NDB_SHARE *share =
      NDB_SHARE::acquire_reference_by_key_have_lock(key, "create_binlog_setup");
  if (share == nullptr) {
    // NDB_SHARE didn't exist, the normal case, try to create it
    share = NDB_SHARE::create_and_acquire_reference(key, "create_binlog_setup");
    if (share == nullptr) {
      // Could not create the NDB_SHARE. Unlikely, catch in debug
      DBUG_ASSERT(false);
      return -1;
    }
  }
  mysql_mutex_unlock(&ndbcluster_mutex);

  // Before 'schema_dist_is_ready', Thd_ndb::ALLOW_BINLOG_SETUP is required
  int ret = 0;
  if (Ndb_schema_dist::is_ready(thd) ||
      get_thd_ndb(thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP)) {
    ret = ndbcluster_setup_binlog_for_share(thd, ndb, share, table_def);
  }

  NDB_SHARE::release_reference(share, "create_binlog_setup");  // temporary ref.

  return ret;
}

int Ndb_binlog_client::create_event(Ndb *ndb,
                                    const NdbDictionary::Table *ndbtab,
                                    const NDB_SHARE *share) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table: '%s', version: %d", ndbtab->getName(),
                       ndbtab->getObjectVersion()));
  DBUG_PRINT("enter", ("share->key: '%s'", share->key_string()));
  DBUG_ASSERT(share);

  // Never create event on table with temporary name
  DBUG_ASSERT(!ndb_name_is_temp(ndbtab->getName()));

  // Never create event on the blob table(s)
  DBUG_ASSERT(!ndb_name_is_blob_prefix(ndbtab->getName()));

  std::string event_name =
      event_name_for_table(m_dbname, m_tabname, share->get_binlog_full());

  ndb->setDatabaseName(share->db);
  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  NDBEVENT my_event(event_name.c_str());
  my_event.setTable(*ndbtab);
  my_event.addTableEvent(NDBEVENT::TE_ALL);
  if (ndb_table_has_hidden_pk(ndbtab)) {
    /* Hidden primary key, subscribe for all attributes */
    my_event.setReport(
        (NDBEVENT::EventReport)(NDBEVENT::ER_ALL | NDBEVENT::ER_DDL));
    DBUG_PRINT("info", ("subscription all"));
  } else {
    if (Ndb_schema_dist_client::is_schema_dist_table(share->db,
                                                     share->table_name)) {
      /**
       * ER_SUBSCRIBE is only needed on schema distribution table
       */
      my_event.setReport((NDBEVENT::EventReport)(
          NDBEVENT::ER_ALL | NDBEVENT::ER_SUBSCRIBE | NDBEVENT::ER_DDL));
      DBUG_PRINT("info", ("subscription all and subscribe"));
    } else if (Ndb_schema_dist_client::is_schema_dist_result_table(
                   share->db, share->table_name)) {
      my_event.setReport(
          (NDBEVENT::EventReport)(NDBEVENT::ER_ALL | NDBEVENT::ER_DDL));
      DBUG_PRINT("info", ("subscription all"));
    } else {
      if (share->get_binlog_full()) {
        my_event.setReport(
            (NDBEVENT::EventReport)(NDBEVENT::ER_ALL | NDBEVENT::ER_DDL));
        DBUG_PRINT("info", ("subscription all"));
      } else {
        my_event.setReport(
            (NDBEVENT::EventReport)(NDBEVENT::ER_UPDATED | NDBEVENT::ER_DDL));
        DBUG_PRINT("info", ("subscription only updated"));
      }
    }
  }
  if (ndb_table_has_blobs(ndbtab)) my_event.mergeEvents(true);

  /* add all columns to the event */
  const int n_cols = ndbtab->getNoOfColumns();
  for (int a = 0; a < n_cols; a++) my_event.addEventColumn(a);

  if (dict->createEvent(my_event))  // Add event to database
  {
    if (dict->getNdbError().classification != NdbError::SchemaObjectExists) {
      // Failed to create event, log warning
      log_warning(ER_GET_ERRMSG,
                  "Unable to create event in database. "
                  "Event: %s  Error Code: %d  Message: %s",
                  event_name.c_str(), dict->getNdbError().code,
                  dict->getNdbError().message);
      return -1;
    }

    /*
      try retrieving the event, if table version/id matches, we will get
      a valid event.  Otherwise we have an old event from before
    */
    const NDBEVENT *ev;
    if ((ev = dict->getEvent(event_name.c_str()))) {
      delete ev;
      return 0;
    }

    // Old event from before; an error, but try to correct it
    if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT &&
        dict->dropEvent(my_event.getName(), 1)) {
      // Failed to drop the old event, log warning
      log_warning(ER_GET_ERRMSG,
                  "Unable to create event in database. "
                  "Attempt to correct with drop failed. "
                  "Event: %s Error Code: %d Message: %s",
                  event_name.c_str(), dict->getNdbError().code,
                  dict->getNdbError().message);
      return -1;
    }

    // Try to add the event again
    if (dict->createEvent(my_event)) {
      // Still failed to create the event, log warning
      log_warning(ER_GET_ERRMSG,
                  "Unable to create event in database. "
                  "Attempt to correct with drop ok, but create failed. "
                  "Event: %s Error Code: %d Message: %s",
                  event_name.c_str(), dict->getNdbError().code,
                  dict->getNdbError().message);
      return -1;
    }
  }

  ndb_log_verbose(1, "Created event '%s' for table '%s.%s' in NDB",
                  event_name.c_str(), m_dbname, m_tabname);

  return 0;
}

inline int is_ndb_compatible_type(Field *field) {
  return !(field->flags & BLOB_FLAG) && field->type() != MYSQL_TYPE_BIT &&
         field->pack_length() != 0;
}

/*
  - create NdbEventOperation for receiving log events
  - setup ndb recattrs for reception of log event data
  - "start" the event operation

  used at create/discover of tables
*/
int Ndb_binlog_client::create_event_op(NDB_SHARE *share,
                                       const NdbDictionary::Table *ndbtab,
                                       const Ndb_event_data *event_data) {
  /*
    we are in either create table or rename table so table should be
    locked, hence we can work with the share without locks
  */

  DBUG_TRACE;
  DBUG_PRINT("enter", ("table: '%s', share->key: '%s'", ndbtab->getName(),
                       share->key_string()));
  DBUG_ASSERT(share);
  DBUG_ASSERT(event_data);

  // Never create event op on table with temporary name
  DBUG_ASSERT(!ndb_name_is_temp(ndbtab->getName()));

  // Never create event op on the blob table(s)
  DBUG_ASSERT(!ndb_name_is_blob_prefix(ndbtab->getName()));

  // Schema dist tables need special processing
  const bool is_schema_dist_setup =
      Ndb_schema_dist_client::is_schema_dist_table(share->db,
                                                   share->table_name) ||
      Ndb_schema_dist_client::is_schema_dist_result_table(share->db,
                                                          share->table_name);

  // Check if this is the event operation on mysql.ndb_apply_status
  // as it need special processing
  const bool do_ndb_apply_status_share =
      Ndb_apply_status_table::is_apply_status_table(share->db,
                                                    share->table_name);

  std::string event_name =
      event_name_for_table(m_dbname, m_tabname, share->get_binlog_full());

  // There should be no NdbEventOperation assigned yet
  DBUG_ASSERT(!share->op);

  TABLE *table = event_data->shadow_table;

  int retries = 100;
  int retry_sleep = 0;
  while (1) {
    if (retry_sleep > 0) {
      ndb_retry_sleep(retry_sleep);
    }
    Mutex_guard injector_mutex_g(injector_event_mutex);
    Ndb *ndb = injector_ndb;
    if (is_schema_dist_setup) ndb = schema_ndb;

    if (ndb == NULL) return -1;

    NdbEventOperation *op;
    if (is_schema_dist_setup)
      op = ndb->createEventOperation(event_name.c_str());
    else {
      // set injector_ndb database/schema from table internal name
      int ret = ndb->setDatabaseAndSchemaName(ndbtab);
      ndbcluster::ndbrequire(ret == 0);
      op = ndb->createEventOperation(event_name.c_str());
      // reset to catch errors
      ndb->setDatabaseName("");
    }
    if (!op) {
      const NdbError &ndb_err = ndb->getNdbError();
      if (ndb_err.code == 4710) {
        // Error code 4710 is returned when table or event is not found. The
        // generic error message for 4710 says "Event not found" but should
        // be reported as "table not found"
        log_warning(ER_GET_ERRMSG,
                    "Failed to create event operation on '%s', "
                    "table '%s' not found",
                    event_name.c_str(), table->s->table_name.str);
        return -1;
      }
      log_warning(ER_GET_ERRMSG,
                  "Failed to create event operation on '%s', error: %d - %s",
                  event_name.c_str(), ndb_err.code, ndb_err.message);
      return -1;
    }

    if (ndb_table_has_blobs(ndbtab))
      op->mergeEvents(true);  // currently not inherited from event

    const uint n_columns = ndbtab->getNoOfColumns();
    const uint n_stored_fields = Ndb_table_map::num_stored_fields(table);
    const uint val_length = sizeof(NdbValue) * n_columns;

    /*
       Allocate memory globally so it can be reused after online alter table
    */
    if (my_multi_malloc(PSI_INSTRUMENT_ME, MYF(MY_WME),
                        &event_data->ndb_value[0], val_length,
                        &event_data->ndb_value[1], val_length, NULL) == 0) {
      log_warning(ER_GET_ERRMSG,
                  "Failed to allocate records for event operation");
      return -1;
    }

    Ndb_table_map map(table);
    for (uint j = 0; j < n_columns; j++) {
      const char *col_name = ndbtab->getColumn(j)->getName();
      NdbValue attr0, attr1;
      if (j < n_stored_fields) {
        Field *f = table->field[map.get_field_for_column(j)];
        if (is_ndb_compatible_type(f)) {
          DBUG_PRINT("info", ("%s compatible", col_name));
          attr0.rec = op->getValue(col_name, (char *)f->ptr);
          attr1.rec = op->getPreValue(
              col_name, (f->ptr - table->record[0]) + (char *)table->record[1]);
        } else if (!(f->flags & BLOB_FLAG)) {
          DBUG_PRINT("info", ("%s non compatible", col_name));
          attr0.rec = op->getValue(col_name);
          attr1.rec = op->getPreValue(col_name);
        } else {
          DBUG_PRINT("info", ("%s blob", col_name));
          DBUG_ASSERT(ndb_table_has_blobs(ndbtab));
          attr0.blob = op->getBlobHandle(col_name);
          attr1.blob = op->getPreBlobHandle(col_name);
          if (attr0.blob == NULL || attr1.blob == NULL) {
            log_warning(ER_GET_ERRMSG,
                        "Failed to cretate NdbEventOperation on '%s', "
                        "blob field %u handles failed, error: %d - %s",
                        event_name.c_str(), j, op->getNdbError().code,
                        op->getNdbError().message);
            ndb->dropEventOperation(op);
            return -1;
          }
        }
      } else {
        DBUG_PRINT("info", ("%s hidden key", col_name));
        attr0.rec = op->getValue(col_name);
        attr1.rec = op->getPreValue(col_name);
      }
      event_data->ndb_value[0][j].ptr = attr0.ptr;
      event_data->ndb_value[1][j].ptr = attr1.ptr;
      DBUG_PRINT("info",
                 ("&event_data->ndb_value[0][%d]: 0x%lx  "
                  "event_data->ndb_value[0][%d]: 0x%lx",
                  j, (long)&event_data->ndb_value[0][j], j, (long)attr0.ptr));
      DBUG_PRINT("info",
                 ("&event_data->ndb_value[1][%d]: 0x%lx  "
                  "event_data->ndb_value[1][%d]: 0x%lx",
                  j, (long)&event_data->ndb_value[0][j], j, (long)attr1.ptr));
    }
    op->setCustomData(
        const_cast<Ndb_event_data *>(event_data));  // set before execute
    share->op = op;                                 // assign op in NDB_SHARE

    /* Check if user explicitly requires monitoring of empty updates */
    if (opt_ndb_log_empty_update) op->setAllowEmptyUpdate(true);

    if (op->execute()) {
      // Failed to create the NdbEventOperation
      const NdbError &ndb_err = op->getNdbError();
      share->op = NULL;
      retries--;
      if (ndb_err.status != NdbError::TemporaryError && ndb_err.code != 1407) {
        // Don't retry after these errors
        retries = 0;
      }
      if (retries == 0) {
        log_warning(ER_GET_ERRMSG,
                    "Failed to activate NdbEventOperation for '%s', "
                    "error: %d - %s",
                    event_name.c_str(), ndb_err.code, ndb_err.message);
      }
      op->setCustomData(NULL);
      ndb->dropEventOperation(op);
      if (retries && !m_thd->killed) {
        // fairly high retry sleep, temporary error on schema operation can
        // take some time to resolve
        retry_sleep = 100;  // milliseconds
        continue;
      }
      // Delete the event data, caller should create new before calling
      // this function again
      Ndb_event_data::destroy(event_data);
      return -1;
    }
    break;
  }

  /* ndb_share reference binlog */
  NDB_SHARE::acquire_reference_on_existing(share, "binlog");

  if (do_ndb_apply_status_share) {
    ndb_apply_status_share = NDB_SHARE::acquire_reference_on_existing(
        share, "ndb_apply_status_share");

    DBUG_ASSERT(get_thd_ndb(m_thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP));
  }

  ndb_log_verbose(1, "NDB Binlog: logging %s (%s,%s)", share->key_string(),
                  share->get_binlog_full() ? "FULL" : "UPDATED",
                  share->get_binlog_use_update() ? "USE_UPDATE" : "USE_WRITE");
  return 0;
}

void Ndb_binlog_client::drop_events_for_table(THD *thd, Ndb *ndb,
                                              const char *db,
                                              const char *table_name) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: %s, tabname: %s", db, table_name));

  if (DBUG_EVALUATE_IF("ndb_skip_drop_event", true, false)) {
    ndb_log_verbose(1, "NDB Binlog: skipping drop event on '%s.%s'", db,
                    table_name);
    return;
  }

  for (uint i = 0; i < 2; i++) {
    std::string event_name = event_name_for_table(db, table_name, i);

    NdbDictionary::Dictionary *dict = ndb->getDictionary();
    if (dict->dropEvent(event_name.c_str()) == 0) {
      // Event dropped successfully
      continue;
    }

    if (dict->getNdbError().code == 4710 || dict->getNdbError().code == 1419) {
      // Failed to drop event but return code says it was
      // because the event didn't exist, ignore
      continue;
    }

    /* Failed to drop event, push warning and write to log */
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                        ER_THD(thd, ER_GET_ERRMSG), dict->getNdbError().code,
                        dict->getNdbError().message, "NDB");

    ndb_log_error(
        "NDB Binlog: Unable to drop event for '%s.%s' from NDB, "
        "event_name: '%s' error: '%d - %s'",
        db, table_name, event_name.c_str(), dict->getNdbError().code,
        dict->getNdbError().message);
  }
}

/*
  Wait for the binlog thread to drop it's NdbEventOperations
  during a drop table

  Syncronized drop between client and injector thread is
  neccessary in order to maintain ordering in the binlog,
  such that the drop occurs _after_ any inserts/updates/deletes.

  Also the injector thread need to be given time to detect the
  drop and release it's resources allocated in the NDB_SHARE.
*/

int ndbcluster_binlog_wait_synch_drop_table(THD *thd, NDB_SHARE *share) {
  DBUG_TRACE;
  DBUG_ASSERT(share);

  const char *save_proc_info = thd->proc_info;
  thd->proc_info = "Syncing ndb table schema operation and binlog";

  int max_timeout = DEFAULT_SYNC_TIMEOUT;

  mysql_mutex_lock(&share->mutex);
  while (share->op) {
    struct timespec abstime;
    set_timespec(&abstime, 1);

    // Unlock the share and wait for injector to signal that
    // something has happened. (NOTE! convoluted in order to
    // only use injector_data_cond with injector_data_mutex)
    mysql_mutex_unlock(&share->mutex);
    mysql_mutex_lock(&injector_data_mutex);
    const int ret = mysql_cond_timedwait(&injector_data_cond,
                                         &injector_data_mutex, &abstime);
    mysql_mutex_unlock(&injector_data_mutex);
    mysql_mutex_lock(&share->mutex);

    if (thd->killed || share->op == 0) break;
    if (ret) {
      max_timeout--;
      if (max_timeout == 0) {
        ndb_log_error("%s, delete table timed out. Ignoring...",
                      share->key_string());
        DBUG_ASSERT(false);
        break;
      }
      if (ndb_log_get_verbose_level())
        ndb_report_waiting("delete table", max_timeout, "delete table",
                           share->key_string());
    }
  }
  mysql_mutex_unlock(&share->mutex);

  thd->proc_info = save_proc_info;

  return 0;
}

void ndbcluster_binlog_validate_sync_blacklist(THD *thd) {
  ndb_binlog_thread.validate_sync_blacklist(thd);
}

bool ndbcluster_binlog_check_table_async(const std::string &db_name,
                                         const std::string &table_name) {
  if (db_name.empty()) {
    ndb_log_error("Database name of object to be synchronized not set");
    return false;
  }

  if (table_name.empty()) {
    ndb_log_error("Table name of object to be synchronized not set");
    return false;
  }

  return ndb_binlog_thread.add_table_to_check(db_name, table_name);
}

bool ndbcluster_binlog_check_logfile_group_async(const std::string &lfg_name) {
  if (lfg_name.empty()) {
    ndb_log_error("Name of logfile group to be synchronized not set");
    return false;
  }

  return ndb_binlog_thread.add_logfile_group_to_check(lfg_name);
}

bool ndbcluster_binlog_check_tablespace_async(
    const std::string &tablespace_name) {
  if (tablespace_name.empty()) {
    ndb_log_error("Name of tablespace to be synchronized not set");
    return false;
  }

  return ndb_binlog_thread.add_tablespace_to_check(tablespace_name);
}

bool ndbcluster_binlog_check_schema_async(const std::string &schema_name) {
  if (schema_name.empty()) {
    ndb_log_error("Name of schema to be synchronized not set");
    return false;
  }
  return ndb_binlog_thread.add_schema_to_check(schema_name);
}

/********************************************************************
  Internal helper functions for differentd events from the stoarage nodes
  used by the ndb injector thread
********************************************************************/

/*
  Unpack a record read from NDB

  SYNOPSIS
    ndb_unpack_record()
    buf                 Buffer to store read row

  NOTE
    The data for each row is read directly into the
    destination buffer. This function is primarily
    called in order to check if any fields should be
    set to null.
*/

static void ndb_unpack_record(TABLE *table, NdbValue *value, MY_BITMAP *defined,
                              uchar *buf) {
  Field **p_field = table->field, *field = *p_field;
  ptrdiff_t row_offset = (ptrdiff_t)(buf - table->record[0]);
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_TRACE;

  /*
    Set the filler bits of the null byte, since they are
    not touched in the code below.

    The filler bits are the MSBs in the last null byte
  */
  if (table->s->null_bytes > 0)
    buf[table->s->null_bytes - 1] |= 256U - (1U << table->s->last_null_bit_pos);
  /*
    Set null flag(s)
  */
  for (; field; p_field++, field = *p_field) {
    if (field->is_virtual_gcol()) {
      if (field->flags & BLOB_FLAG) {
        /**
         * Valgrind shows Server binlog code uses length
         * of virtual blob fields for allocation decisions
         * even when the blob is not read
         */
        Field_blob *field_blob = (Field_blob *)field;
        DBUG_PRINT("info", ("[%u] is virtual blob, setting length 0",
                            field->field_index));
        Uint32 zerolen = 0;
        field_blob->set_ptr((uchar *)&zerolen, NULL);
      }

      continue;
    }

    field->set_notnull(row_offset);
    if ((*value).ptr) {
      if (!(field->flags & BLOB_FLAG)) {
        int is_null = (*value).rec->isNULL();
        if (is_null) {
          if (is_null > 0) {
            DBUG_PRINT("info", ("[%u] NULL", field->field_index));
            field->set_null(row_offset);
          } else {
            DBUG_PRINT("info", ("[%u] UNDEFINED", field->field_index));
            bitmap_clear_bit(defined, field->field_index);
          }
        } else if (field->type() == MYSQL_TYPE_BIT) {
          Field_bit *field_bit = static_cast<Field_bit *>(field);

          /*
            Move internal field pointer to point to 'buf'.  Calling
            the correct member function directly since we know the
            type of the object.
           */
          field_bit->Field_bit::move_field_offset(row_offset);
          if (field->pack_length() < 5) {
            DBUG_PRINT("info",
                       ("bit field H'%.8X", (*value).rec->u_32_value()));
            field_bit->Field_bit::store((longlong)(*value).rec->u_32_value(),
                                        true);
          } else {
            DBUG_PRINT("info",
                       ("bit field H'%.8X%.8X", *(Uint32 *)(*value).rec->aRef(),
                        *((Uint32 *)(*value).rec->aRef() + 1)));
#ifdef WORDS_BIGENDIAN
            /* lsw is stored first */
            Uint32 *buf = (Uint32 *)(*value).rec->aRef();
            field_bit->Field_bit::store(
                (((longlong)*buf) & 0x00000000FFFFFFFFLL) |
                    ((((longlong) * (buf + 1)) << 32) & 0xFFFFFFFF00000000LL),
                true);
#else
            field_bit->Field_bit::store((longlong)(*value).rec->u_64_value(),
                                        true);
#endif
          }
          /*
            Move back internal field pointer to point to original
            value (usually record[0]).
           */
          field_bit->Field_bit::move_field_offset(-row_offset);
          DBUG_PRINT("info",
                     ("[%u] SET", (*value).rec->getColumn()->getColumnNo()));
          DBUG_DUMP("info", (const uchar *)field->ptr, field->pack_length());
        } else {
          DBUG_ASSERT(
              !strcmp((*value).rec->getColumn()->getName(), field->field_name));
          DBUG_PRINT("info",
                     ("[%u] SET", (*value).rec->getColumn()->getColumnNo()));
          DBUG_DUMP("info", (const uchar *)field->ptr, field->pack_length());
        }
      } else {
        NdbBlob *ndb_blob = (*value).blob;
        const uint field_no = field->field_index;
        int isNull;
        ndb_blob->getDefined(isNull);
        if (isNull == 1) {
          DBUG_PRINT("info", ("[%u] NULL", field_no));
          field->set_null(row_offset);
        } else if (isNull == -1) {
          DBUG_PRINT("info", ("[%u] UNDEFINED", field_no));
          bitmap_clear_bit(defined, field_no);
        } else {
#ifndef DBUG_OFF
          // pointer vas set in get_ndb_blobs_value
          Field_blob *field_blob = (Field_blob *)field;
          const uchar *ptr = field_blob->get_blob_data(row_offset);
          uint32 len = field_blob->get_length(row_offset);
          DBUG_PRINT("info", ("[%u] SET ptr: 0x%lx  len: %u", field_no,
                              (long)ptr, len));
#endif
        }
      }       // else
    }         // if ((*value).ptr)
    value++;  // this field was not virtual
  }           // for()
  dbug_tmp_restore_column_map(table->write_set, old_map);
}

/*
  Handle error states on events from the storage nodes
*/
static int handle_error(NdbEventOperation *pOp) {
  Ndb_event_data *event_data = (Ndb_event_data *)pOp->getCustomData();
  NDB_SHARE *share = event_data->share;
  DBUG_TRACE;

  ndb_log_error("NDB Binlog: unhandled error %d for table %s", pOp->hasError(),
                share->key_string());
  pOp->clearError();
  return 0;
}

/*
  Handle _non_ data events from the storage nodes
*/

static void handle_non_data_event(THD *thd, NdbEventOperation *pOp,
                                  ndb_binlog_index_row &row) {
  const Ndb_event_data *event_data =
      static_cast<const Ndb_event_data *>(pOp->getCustomData());
  NDB_SHARE *share = event_data->share;
  const NDBEVENT::TableEvent type = pOp->getEventType();

  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("pOp: %p, event_data: %p, share: %p", pOp, event_data, share));
  DBUG_PRINT("enter", ("type: %d", type));

  if (type == NDBEVENT::TE_DROP || type == NDBEVENT::TE_ALTER) {
    // Count schema events
    row.n_schemaops++;
  }

  switch (type) {
    case NDBEVENT::TE_CLUSTER_FAILURE:
      ndb_log_verbose(1, "NDB Binlog: cluster failure for %s at epoch %u/%u.",
                      share->key_string(), (uint)(pOp->getGCI() >> 32),
                      (uint)(pOp->getGCI()));
      // fallthrough
    case NDBEVENT::TE_DROP:
      if (ndb_apply_status_share == share) {
        if (ndb_binlog_tables_inited && ndb_binlog_running)
          ndb_log_verbose(1, "NDB Binlog: util tables need to reinitialize");

        /* release the ndb_apply_status_share */
        NDB_SHARE::release_reference(ndb_apply_status_share,
                                     "ndb_apply_status_share");
        ndb_apply_status_share = NULL;

        Mutex_guard injector_g(injector_data_mutex);
        ndb_binlog_tables_inited = false;
      }

      ndbcluster_binlog_event_operation_teardown(thd, injector_ndb, pOp);
      break;

    case NDBEVENT::TE_ALTER:
      DBUG_PRINT("info", ("TE_ALTER"));
      break;

    case NDBEVENT::TE_NODE_FAILURE:
    case NDBEVENT::TE_SUBSCRIBE:
    case NDBEVENT::TE_UNSUBSCRIBE:
      /* ignore */
      break;

    default:
      ndb_log_error(
          "NDB Binlog: unknown non data event %d for %s. "
          "Ignoring...",
          (unsigned)type, share->key_string());
      break;
  }
}

/*
  Handle data events from the storage nodes
*/
inline ndb_binlog_index_row *ndb_find_binlog_index_row(
    ndb_binlog_index_row **rows, uint orig_server_id, int flag) {
  ndb_binlog_index_row *row = *rows;
  if (opt_ndb_log_orig) {
    ndb_binlog_index_row *first = row, *found_id = 0;
    for (;;) {
      if (row->orig_server_id == orig_server_id) {
        /* */
        if (!flag || !row->orig_epoch) return row;
        if (!found_id) found_id = row;
      }
      if (row->orig_server_id == 0) break;
      row = row->next;
      if (row == NULL) {
        // Allocate memory in current MEM_ROOT
        row = (ndb_binlog_index_row *)(*THR_MALLOC)
                  ->Alloc(sizeof(ndb_binlog_index_row));
        memset(row, 0, sizeof(ndb_binlog_index_row));
        row->next = first;
        *rows = row;
        if (found_id) {
          /*
            If we found index_row with same server id already
            that row will contain the current stats.
            Copy stats over to new and reset old.
          */
          row->n_inserts = found_id->n_inserts;
          row->n_updates = found_id->n_updates;
          row->n_deletes = found_id->n_deletes;
          found_id->n_inserts = 0;
          found_id->n_updates = 0;
          found_id->n_deletes = 0;
        }
        /* keep track of schema ops only on "first" index_row */
        row->n_schemaops = first->n_schemaops;
        first->n_schemaops = 0;
        break;
      }
    }
    row->orig_server_id = orig_server_id;
  }
  return row;
}

static int handle_data_event(NdbEventOperation *pOp,
                             ndb_binlog_index_row **rows,
                             injector::transaction &trans,
                             unsigned &trans_row_count,
                             unsigned &trans_slave_row_count) {
  Ndb_event_data *event_data = (Ndb_event_data *)pOp->getCustomData();
  TABLE *table = event_data->shadow_table;
  NDB_SHARE *share = event_data->share;
  bool reflected_op = false;
  bool refresh_op = false;
  bool read_op = false;

  if (pOp != share->op) {
    return 0;
  }

  uint32 anyValue = pOp->getAnyValue();
  if (ndbcluster_anyvalue_is_reserved(anyValue)) {
    if (ndbcluster_anyvalue_is_nologging(anyValue)) return 0;

    if (ndbcluster_anyvalue_is_reflect_op(anyValue)) {
      DBUG_PRINT("info", ("Anyvalue -> Reflect (%u)", anyValue));
      reflected_op = true;
      anyValue = 0;
    } else if (ndbcluster_anyvalue_is_refresh_op(anyValue)) {
      DBUG_PRINT("info", ("Anyvalue -> Refresh"));
      refresh_op = true;
      anyValue = 0;
    } else if (ndbcluster_anyvalue_is_read_op(anyValue)) {
      DBUG_PRINT("info", ("Anyvalue -> Read"));
      read_op = true;
      anyValue = 0;
    } else {
      ndb_log_warning(
          "unknown value for binlog signalling 0x%X, "
          "event not logged",
          anyValue);
      return 0;
    }
  }

  uint32 originating_server_id = ndbcluster_anyvalue_get_serverid(anyValue);
  bool log_this_slave_update = g_ndb_log_slave_updates;
  bool count_this_event = true;

  if (share == ndb_apply_status_share) {
    /*
       Note that option values are read without synchronisation w.r.t.
       thread setting option variable or epoch boundaries.
    */
    if (opt_ndb_log_apply_status || opt_ndb_log_orig) {
      Uint32 ndb_apply_status_logging_server_id = originating_server_id;
      Uint32 ndb_apply_status_server_id = 0;
      Uint64 ndb_apply_status_epoch = 0;
      bool event_has_data = false;

      switch (pOp->getEventType()) {
        case NDBEVENT::TE_INSERT:
        case NDBEVENT::TE_UPDATE:
          event_has_data = true;
          break;

        case NDBEVENT::TE_DELETE:
          break;
        default:
          /* We should REALLY never get here */
          abort();
      }

      if (likely(event_has_data)) {
        /* unpack data to fetch orig_server_id and orig_epoch */
        MY_BITMAP b;
        uint32 bitbuf[128 / (sizeof(uint32) * 8)];
        ndb_bitmap_init(b, bitbuf, table->s->fields);
        bitmap_copy(&b, &event_data->stored_columns);
        ndb_unpack_record(table, event_data->ndb_value[0], &b,
                          table->record[0]);
        ndb_apply_status_server_id =
            (uint)((Field_long *)table->field[0])->val_int();
        ndb_apply_status_epoch = ((Field_longlong *)table->field[1])->val_int();

        if (opt_ndb_log_apply_status) {
          /*
             Determine if event came from our immediate Master server
             Ignore locally manually sourced and reserved events
          */
          if ((ndb_apply_status_logging_server_id != 0) &&
              (!ndbcluster_anyvalue_is_reserved(
                  ndb_apply_status_logging_server_id))) {
            bool isFromImmediateMaster = (ndb_apply_status_server_id ==
                                          ndb_apply_status_logging_server_id);

            if (isFromImmediateMaster) {
              /*
                 We log this event with our server-id so that it
                 propagates back to the originating Master (our
                 immediate Master)
              */
              assert(ndb_apply_status_logging_server_id != ::server_id);

              originating_server_id =
                  0; /* Will be set to our ::serverid below */
            }
          }
        }

        if (opt_ndb_log_orig) {
          /* store */
          ndb_binlog_index_row *row =
              ndb_find_binlog_index_row(rows, ndb_apply_status_server_id, 1);
          row->orig_epoch = ndb_apply_status_epoch;
        }
      }
    }  // opt_ndb_log_apply_status || opt_ndb_log_orig)

    if (opt_ndb_log_apply_status) {
      /* We are logging ndb_apply_status changes
       * Don't count this event as making an epoch non-empty
       * Log this event in the Binlog
       */
      count_this_event = false;
      log_this_slave_update = true;
    } else {
      /* Not logging ndb_apply_status updates, discard this event now */
      return 0;
    }
  }

  if (originating_server_id == 0)
    originating_server_id = ::server_id;
  else {
    assert(!reflected_op && !refresh_op);
    /* Track that we received a replicated row event */
    if (likely(count_this_event)) trans_slave_row_count++;

    if (!log_this_slave_update) {
      /*
        This event comes from a slave applier since it has an originating
        server id set. Since option to log slave updates is not set, skip it.
      */
      return 0;
    }
  }

  /*
     Start with logged_server_id as AnyValue in case it's a composite
     (server_id_bits < 31).  This way any user-values are passed-through
     to the Binlog in the high bits of the event's Server Id.
     In future it may be useful to support *not* mapping composite
     AnyValues to/from Binlogged server-ids.
  */
  uint32 logged_server_id = anyValue;
  ndbcluster_anyvalue_set_serverid(logged_server_id, originating_server_id);

  /*
     Get NdbApi transaction id for this event to put into Binlog
  */
  Ndb_binlog_extra_row_info extra_row_info;
  const unsigned char *extra_row_info_ptr = NULL;
  Uint16 erif_flags = 0;
  if (opt_ndb_log_transaction_id) {
    erif_flags |= Ndb_binlog_extra_row_info::NDB_ERIF_TRANSID;
    extra_row_info.setTransactionId(pOp->getTransId());
  }

  /* Set conflict flags member if necessary */
  Uint16 event_conflict_flags = 0;
  assert(!(reflected_op && refresh_op));
  if (reflected_op) {
    event_conflict_flags |= NDB_ERIF_CFT_REFLECT_OP;
  } else if (refresh_op) {
    event_conflict_flags |= NDB_ERIF_CFT_REFRESH_OP;
  } else if (read_op) {
    event_conflict_flags |= NDB_ERIF_CFT_READ_OP;
  }

  if (DBUG_EVALUATE_IF("ndb_injector_set_event_conflict_flags", true, false)) {
    event_conflict_flags = 0xfafa;
  }
  if (event_conflict_flags != 0) {
    erif_flags |= Ndb_binlog_extra_row_info::NDB_ERIF_CFT_FLAGS;
    extra_row_info.setConflictFlags(event_conflict_flags);
  }

  if (erif_flags != 0) {
    extra_row_info.setFlags(erif_flags);
    if (likely(!log_bin_use_v1_row_events)) {
      extra_row_info_ptr = extra_row_info.generateBuffer();
    } else {
      /**
       * Can't put the metadata in a v1 event
       * Produce 1 warning at most
       */
      if (!g_injector_v1_warning_emitted) {
        ndb_log_error(
            "Binlog Injector discarding row event "
            "meta data as server is using v1 row events. "
            "(%u %x)",
            opt_ndb_log_transaction_id, event_conflict_flags);

        g_injector_v1_warning_emitted = true;
      }
    }
  }

  DBUG_ASSERT(trans.good());
  DBUG_ASSERT(table != 0);

#ifndef DBUG_OFF
  Ndb_table_map::print_table("table", table);
#endif

  MY_BITMAP b;
  my_bitmap_map
      bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE + 8 * sizeof(my_bitmap_map) - 1) /
             (8 * sizeof(my_bitmap_map))];
  ndb_bitmap_init(b, bitbuf, table->s->fields);
  bitmap_copy(&b, &event_data->stored_columns);
  if (bitmap_is_clear_all(&b)) {
    DBUG_PRINT("info", ("Skip logging of event without stored columns"));
    return 0;
  }

  /*
   row data is already in table->record[0]
   As we told the NdbEventOperation to do this
   (saves moving data about many times)
  */

  /*
    for now malloc/free blobs buffer each time
    TODO if possible share single permanent buffer with handlers
   */
  uchar *blobs_buffer[2] = {0, 0};
  uint blobs_buffer_size[2] = {0, 0};

  ndb_binlog_index_row *row =
      ndb_find_binlog_index_row(rows, originating_server_id, 0);

  switch (pOp->getEventType()) {
    case NDBEVENT::TE_INSERT:
      if (likely(count_this_event)) {
        row->n_inserts++;
        trans_row_count++;
      }
      DBUG_PRINT("info", ("INSERT INTO %s.%s", table->s->db.str,
                          table->s->table_name.str));
      {
        int ret;
        (void)ret;  // Bug27150740 HANDLE_DATA_EVENT NEED ERROR HANDLING
        if (event_data->have_blobs) {
          ptrdiff_t ptrdiff = 0;
          ret = get_ndb_blobs_value(table, event_data->ndb_value[0],
                                    blobs_buffer[0], blobs_buffer_size[0],
                                    ptrdiff);
          assert(ret == 0);
        }
        ndb_unpack_record(table, event_data->ndb_value[0], &b,
                          table->record[0]);
        ret = trans.write_row(logged_server_id,
                              injector::transaction::table(table, true), &b,
                              table->record[0], extra_row_info_ptr);
        assert(ret == 0);
      }
      break;
    case NDBEVENT::TE_DELETE:
      if (likely(count_this_event)) {
        row->n_deletes++;
        trans_row_count++;
      }
      DBUG_PRINT("info", ("DELETE FROM %s.%s", table->s->db.str,
                          table->s->table_name.str));
      {
        /*
          table->record[0] contains only the primary key in this case
          since we do not have an after image
        */
        int n;
        if (!share->get_binlog_full() && table->s->primary_key != MAX_KEY)
          n = 0; /*
                   use the primary key only as it save time and space and
                   it is the only thing needed to log the delete
                 */
        else
          n = 1; /*
                   we use the before values since we don't have a primary key
                   since the mysql server does not handle the hidden primary
                   key
                 */

        int ret;
        (void)ret;  // Bug27150740 HANDLE_DATA_EVENT NEED ERROR HANDLING
        if (event_data->have_blobs) {
          ptrdiff_t ptrdiff = table->record[n] - table->record[0];
          ret = get_ndb_blobs_value(table, event_data->ndb_value[n],
                                    blobs_buffer[n], blobs_buffer_size[n],
                                    ptrdiff);
          assert(ret == 0);
        }
        ndb_unpack_record(table, event_data->ndb_value[n], &b,
                          table->record[n]);
        DBUG_EXECUTE("info",
                     Ndb_table_map::print_record(table, table->record[n]););
        ret = trans.delete_row(logged_server_id,
                               injector::transaction::table(table, true), &b,
                               table->record[n], extra_row_info_ptr);
        assert(ret == 0);
      }
      break;
    case NDBEVENT::TE_UPDATE:
      if (likely(count_this_event)) {
        row->n_updates++;
        trans_row_count++;
      }
      DBUG_PRINT("info",
                 ("UPDATE %s.%s", table->s->db.str, table->s->table_name.str));
      {
        int ret;
        (void)ret;  // Bug27150740 HANDLE_DATA_EVENT NEED ERROR HANDLING
        if (event_data->have_blobs) {
          ptrdiff_t ptrdiff = 0;
          ret = get_ndb_blobs_value(table, event_data->ndb_value[0],
                                    blobs_buffer[0], blobs_buffer_size[0],
                                    ptrdiff);
          assert(ret == 0);
        }
        ndb_unpack_record(table, event_data->ndb_value[0], &b,
                          table->record[0]);
        DBUG_EXECUTE("info",
                     Ndb_table_map::print_record(table, table->record[0]););
        if (table->s->primary_key != MAX_KEY &&
            !share->get_binlog_use_update()) {
          /*
            since table has a primary key, we can do a write
            using only after values
          */
          ret = trans.write_row(logged_server_id,
                                injector::transaction::table(table, true), &b,
                                table->record[0],  // after values
                                extra_row_info_ptr);
          assert(ret == 0);
        } else {
          /*
            mysql server cannot handle the ndb hidden key and
            therefore needs the before image as well
          */
          if (event_data->have_blobs) {
            ptrdiff_t ptrdiff = table->record[1] - table->record[0];
            ret = get_ndb_blobs_value(table, event_data->ndb_value[1],
                                      blobs_buffer[1], blobs_buffer_size[1],
                                      ptrdiff);
            assert(ret == 0);
          }
          ndb_unpack_record(table, event_data->ndb_value[1], &b,
                            table->record[1]);
          DBUG_EXECUTE("info",
                       Ndb_table_map::print_record(table, table->record[1]););

          MY_BITMAP col_bitmap_before_update;
          my_bitmap_map bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                                8 * sizeof(my_bitmap_map) - 1) /
                               (8 * sizeof(my_bitmap_map))];
          ndb_bitmap_init(col_bitmap_before_update, bitbuf, table->s->fields);
          if (share->get_binlog_update_minimal()) {
            event_data->generate_minimal_bitmap(&col_bitmap_before_update, &b);
          } else {
            bitmap_copy(&col_bitmap_before_update, &b);
          }

          ret = trans.update_row(logged_server_id,
                                 injector::transaction::table(table, true),
                                 &col_bitmap_before_update, &b,
                                 table->record[1],  // before values
                                 table->record[0],  // after values
                                 extra_row_info_ptr);
          assert(ret == 0);
        }
      }
      break;
    default:
      /* We should REALLY never get here. */
      DBUG_PRINT("info", ("default - uh oh, a brain exploded."));
      break;
  }

  if (event_data->have_blobs) {
    my_free(blobs_buffer[0]);
    my_free(blobs_buffer[1]);
  }

  return 0;
}

/****************************************************************
  Injector thread main loop
****************************************************************/

void Ndb_binlog_thread::remove_event_operations(Ndb *ndb) const {
  DBUG_TRACE;
  NdbEventOperation *op;
  while ((op = ndb->getEventOperation())) {
    DBUG_ASSERT(
        !ndb_name_is_blob_prefix(op->getEvent()->getTable()->getName()));
    DBUG_PRINT("info",
               ("removing event operation on %s", op->getEvent()->getName()));

    Ndb_event_data *event_data = (Ndb_event_data *)op->getCustomData();
    DBUG_ASSERT(event_data);

    NDB_SHARE *share = event_data->share;
    DBUG_ASSERT(share != NULL);
    DBUG_ASSERT(share->op == op);
    Ndb_event_data::destroy(event_data);
    op->setCustomData(NULL);

    mysql_mutex_lock(&share->mutex);
    share->op = 0;
    mysql_mutex_unlock(&share->mutex);

    NDB_SHARE::release_reference(share, "binlog");

    ndb->dropEventOperation(op);
  }
}

void Ndb_binlog_thread::remove_all_event_operations(Ndb *s_ndb,
                                                    Ndb *i_ndb) const {
  DBUG_TRACE;

  if (ndb_apply_status_share) {
    NDB_SHARE::release_reference(ndb_apply_status_share,
                                 "ndb_apply_status_share");
    ndb_apply_status_share = NULL;
  }

  if (s_ndb) remove_event_operations(s_ndb);

  if (i_ndb) remove_event_operations(i_ndb);

  if (ndb_log_get_verbose_level() > 15) {
    NDB_SHARE::print_remaining_open_tables();
  }
}

static long long g_event_data_count = 0;
static long long g_event_nondata_count = 0;
static long long g_event_bytes_count = 0;

static void update_injector_stats(Ndb *schemaNdb, Ndb *dataNdb) {
  // Update globals to sum of totals from each listening Ndb object
  g_event_data_count = schemaNdb->getClientStat(Ndb::DataEventsRecvdCount) +
                       dataNdb->getClientStat(Ndb::DataEventsRecvdCount);
  g_event_nondata_count =
      schemaNdb->getClientStat(Ndb::NonDataEventsRecvdCount) +
      dataNdb->getClientStat(Ndb::NonDataEventsRecvdCount);
  g_event_bytes_count = schemaNdb->getClientStat(Ndb::EventBytesRecvdCount) +
                        dataNdb->getClientStat(Ndb::EventBytesRecvdCount);
}

static SHOW_VAR ndb_status_vars_injector[] = {
    {"api_event_data_count_injector",
     reinterpret_cast<char *>(&g_event_data_count), SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"api_event_nondata_count_injector",
     reinterpret_cast<char *>(&g_event_nondata_count), SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"api_event_bytes_count_injector",
     reinterpret_cast<char *>(&g_event_bytes_count), SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_status_injector(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = reinterpret_cast<char *>(&ndb_status_vars_injector);
  return 0;
}

/**
   injectApplyStatusWriteRow

   Inject a WRITE_ROW event on the ndb_apply_status table into
   the Binlog.
   This contains our server_id and the supplied epoch number.
   When applied on the Slave it gives a transactional position
   marker
*/
static bool injectApplyStatusWriteRow(injector::transaction &trans,
                                      ulonglong gci) {
  DBUG_TRACE;
  if (ndb_apply_status_share == NULL) {
    ndb_log_error("Could not get apply status share");
    DBUG_ASSERT(ndb_apply_status_share != NULL);
    return false;
  }

  longlong gci_to_store = (longlong)gci;

#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("ndb_binlog_injector_cycle_gcis", true, false)) {
    ulonglong gciHi = ((gci_to_store >> 32) & 0xffffffff);
    ulonglong gciLo = (gci_to_store & 0xffffffff);
    gciHi = (gciHi % 3);
    ndb_log_warning("Binlog injector cycling gcis (%llu -> %llu)", gci_to_store,
                    (gciHi << 32) + gciLo);
    gci_to_store = (gciHi << 32) + gciLo;
  }
  if (DBUG_EVALUATE_IF("ndb_binlog_injector_repeat_gcis", true, false)) {
    ulonglong gciHi = ((gci_to_store >> 32) & 0xffffffff);
    ulonglong gciLo = (gci_to_store & 0xffffffff);
    gciHi = 0xffffff00;
    gciLo = 0;
    ndb_log_warning("Binlog injector repeating gcis (%llu -> %llu)",
                    gci_to_store, (gciHi << 32) + gciLo);
    gci_to_store = (gciHi << 32) + gciLo;
  }
#endif

  /* Build row buffer for generated ndb_apply_status
     WRITE_ROW event
     First get the relevant table structure.
  */
  DBUG_ASSERT(ndb_apply_status_share->op);
  Ndb_event_data *event_data =
      (Ndb_event_data *)ndb_apply_status_share->op->getCustomData();
  DBUG_ASSERT(event_data);
  DBUG_ASSERT(event_data->shadow_table);
  TABLE *apply_status_table = event_data->shadow_table;

  /*
    Intialize apply_status_table->record[0]

    When iterating past the end of the last epoch, the first event of
    the new epoch may be on ndb_apply_status.  Its event data saved
    in record[0] would be overwritten here by a subsequent event on a
    normal table.  So save and restore its record[0].
  */
  static const ulong sav_max = 512;  // current is 284
  const ulong sav_len = apply_status_table->s->reclength;
  DBUG_ASSERT(sav_len <= sav_max);
  uchar sav_buf[sav_max];
  memcpy(sav_buf, apply_status_table->record[0], sav_len);
  empty_record(apply_status_table);

  apply_status_table->field[0]->store((longlong)::server_id, true);
  apply_status_table->field[1]->store((longlong)gci_to_store, true);
  apply_status_table->field[2]->store("", 0, &my_charset_bin);
  apply_status_table->field[3]->store((longlong)0, true);
  apply_status_table->field[4]->store((longlong)0, true);
#ifndef DBUG_OFF
  const LEX_CSTRING &name = apply_status_table->s->table_name;
  DBUG_PRINT("info", ("use_table: %.*s", (int)name.length, name.str));
#endif
  injector::transaction::table tbl(apply_status_table, true);
  int ret = trans.use_table(::server_id, tbl);
  ndbcluster::ndbrequire(ret == 0);

  ret = trans.write_row(
      ::server_id, injector::transaction::table(apply_status_table, true),
      &apply_status_table->s->all_set, apply_status_table->record[0]);

  assert(ret == 0);

  memcpy(apply_status_table->record[0], sav_buf, sav_len);
  return true;
}

extern ulong opt_ndb_report_thresh_binlog_epoch_slip;
extern ulong opt_ndb_report_thresh_binlog_mem_usage;
extern ulong opt_ndb_eventbuffer_max_alloc;
extern uint opt_ndb_eventbuffer_free_percent;

Ndb_binlog_thread::Ndb_binlog_thread() : Ndb_component("Binlog") {}

Ndb_binlog_thread::~Ndb_binlog_thread() {}

void Ndb_binlog_thread::do_wakeup() {
  log_info("Wakeup");

  /*
    The binlog thread is normally waiting for another
    event from the cluster with short timeout and should
    soon(within 1 second) detect that stop has been requested.

    There are really no purpose(yet) to signal some condition
    trying to wake the thread up should it be waiting somewhere
    else since those waits are also short.
  */
}

bool Ndb_binlog_thread::check_reconnect_incident(
    THD *thd, injector *inj, Reconnect_type incident_id) const {
  log_verbose(1, "Check for incidents");

  if (incident_id == MYSQLD_STARTUP) {
    LOG_INFO log_info;
    mysql_bin_log.get_current_log(&log_info);
    log_verbose(60, " - current binlog file: %s", log_info.log_file_name);

    uint log_number = 0;
    if ((sscanf(strend(log_info.log_file_name) - 6, "%u", &log_number) == 1) &&
        log_number == 1) {
      /*
        This is the fist binlog file, skip writing incident since
        there is really no log to have a gap in
      */
      log_verbose(60, " - skipping incident for first log, log_number: %u",
                  log_number);
      return false;  // No incident written
    }
    log_verbose(60, " - current binlog file number: %u", log_number);
  }

  // Write an incident event to the binlog since it's not possible to know what
  // has happened in the cluster while not being connected.
  LEX_CSTRING msg;
  switch (incident_id) {
    case MYSQLD_STARTUP:
      msg = {STRING_WITH_LEN("mysqld startup")};
      break;
    case CLUSTER_DISCONNECT:
      msg = {STRING_WITH_LEN("cluster disconnect")};
      break;
  }
  log_verbose(20, "Writing incident for %s", msg.str);
  (void)inj->record_incident(
      thd, binary_log::Incident_event::INCIDENT_LOST_EVENTS, msg);

  return true;  // Incident written
}

bool Ndb_binlog_thread::handle_purge(const char *filename) {
  if (is_server_started()) {
    // The binlog thread currently only handles purge requests
    // that occurs before "server started"
    return false;
  }

  // The "server started" state is not yet reached, defer the purge request of
  // this binlog file to later and handle it just before entering main loop
  log_verbose(1, "Remember purge binlog file: '%s'", filename);
  std::lock_guard<std::mutex> lock_pending_purges(m_purge_mutex);
  m_pending_purges.push_back(filename);
  return true;
}

void Ndb_binlog_thread::recall_pending_purges(THD *thd) {
  std::lock_guard<std::mutex> lock_pending_purges(m_purge_mutex);

  // Iterate list of pending purges and delete corresponding
  // rows from ndb_binlog_index table
  for (const std::string filename : m_pending_purges) {
    log_verbose(1, "Purging binlog file: '%s'", filename.c_str());

    if (Ndb_binlog_index_table_util::remove_rows_for_file(thd,
                                                          filename.c_str())) {
      log_warning("Failed to purge binlog file: '%s'", filename.c_str());
    }
  }
  // All pending purges performed, clear the list
  m_pending_purges.clear();
}

/*
  Events are handled one epoch at a time.
  Handle the lowest available epoch first.
*/
static Uint64 find_epoch_to_handle(const NdbEventOperation *s_pOp,
                                   const NdbEventOperation *i_pOp) {
  if (i_pOp != NULL) {
    if (s_pOp != NULL) {
      return std::min(i_pOp->getEpoch(), s_pOp->getEpoch());
    }
    return i_pOp->getEpoch();
  }
  if (s_pOp != NULL) {
    if (ndb_binlog_running) {
      return std::min(ndb_latest_received_binlog_epoch, s_pOp->getEpoch());
    }
    return s_pOp->getEpoch();
  }
  // 'latest_received' is '0' if not binlogging
  return ndb_latest_received_binlog_epoch;
}

void Ndb_binlog_thread::do_run() {
  THD *thd; /* needs to be first for thread_stack */
  Ndb *i_ndb = NULL;
  Ndb *s_ndb = NULL;
  Thd_ndb *thd_ndb = NULL;
  injector *inj = injector::instance();
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();

  enum {
    BCCC_starting,
    BCCC_running,
    BCCC_restart,
  } binlog_thread_state;

  /* Controls that only one incident is written per reconnect */
  bool do_reconnect_incident = true;
  /* Controls message of the reconnnect incident */
  Reconnect_type reconnect_incident_id = MYSQLD_STARTUP;

  DBUG_TRACE;

  log_info("Starting...");

  thd = new THD; /* note that constructor of THD uses DBUG_ */
  THD_CHECK_SENTRY(thd);

  /* We need to set thd->thread_id before thd->store_globals, or it will
     set an invalid value for thd->variables.pseudo_thread_id.
  */
  thd->set_new_thread_id();

  thd->thread_stack = (char *)&thd; /* remember where our stack is */
  thd->store_globals();

  thd->set_command(COM_DAEMON);
  thd->system_thread = SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->get_protocol_classic()->set_client_capabilities(0);
  thd->security_context()->skip_grants();
  // Create thd->net vithout vio
  thd->get_protocol_classic()->init_net((Vio *)0);

  // Ndb binlog thread always use row format
  thd->set_current_stmt_binlog_format_row();

  thd->real_id = my_thread_self();
  thd_manager->add_thd(thd);
  thd->lex->start_transaction_opt = 0;

  log_info("Started");

  Ndb_binlog_setup binlog_setup(thd);
  Ndb_schema_dist_data schema_dist_data;

restart_cluster_failure:
  /**
   * Maintain a current schema & injector eventOp to be handled.
   * s_pOp and s_ndb handle events from the 'ndb_schema' dist table,
   * while i_pOp and i_ndb is for binlogging 'everything else'.
   */
  NdbEventOperation *s_pOp = NULL;
  NdbEventOperation *i_pOp = NULL;
  binlog_thread_state = BCCC_starting;

  log_verbose(1, "Setting up");

  if (!(thd_ndb = Thd_ndb::seize(thd))) {
    log_error("Creating Thd_ndb object failed");
    goto err;
  }
  thd_ndb->set_option(Thd_ndb::NO_LOG_SCHEMA_OP);

  if (!(s_ndb = new (std::nothrow) Ndb(g_ndb_cluster_connection)) ||
      s_ndb->setNdbObjectName("schema change monitoring") || s_ndb->init()) {
    log_error("Creating schema Ndb object failed");
    goto err;
  }
  log_verbose(49, "Created schema Ndb object, reference: 0x%x, name: '%s'",
              s_ndb->getReference(), s_ndb->getNdbObjectName());

  // empty database
  if (!(i_ndb = new (std::nothrow) Ndb(g_ndb_cluster_connection)) ||
      i_ndb->setNdbObjectName("data change monitoring") || i_ndb->init()) {
    log_error("Creating injector Ndb object failed");
    goto err;
  }
  log_verbose(49, "Created injector Ndb object, reference: 0x%x, name: '%s'",
              i_ndb->getReference(), i_ndb->getNdbObjectName());

  /* Set free percent event buffer needed to resume buffering */
  if (i_ndb->set_eventbuffer_free_percent(opt_ndb_eventbuffer_free_percent)) {
    log_error("Setting eventbuffer free percent failed");
    goto err;
  }

  log_verbose(10, "Exposing global references");
  /*
    Expose global reference to our Ndb object.

    Used by both sql client thread and binlog thread to interact
    with the storage
  */
  mysql_mutex_lock(&injector_event_mutex);
  injector_thd = thd;
  injector_ndb = i_ndb;
  schema_ndb = s_ndb;
  DBUG_PRINT("info", ("set schema_ndb to s_ndb"));
  mysql_mutex_unlock(&injector_event_mutex);

  if (opt_bin_log && opt_ndb_log_bin) {
    // Binary log has been enabled for the server and changes
    // to NDB tables should be logged
    ndb_binlog_running = true;
  }
  log_verbose(1, "Setup completed");

  /*
    Wait for the MySQL Server to start (so that the binlog is started
    and thus can receive the first GAP event)
  */
  if (!wait_for_server_started()) {
    goto err;
  }

  // Defer call of THD::init_query_mem_roots until after
  // wait_for_server_started() to ensure that the parts of
  // MySQL Server it uses has been created
  thd->init_query_mem_roots();
  lex_start(thd);

  if (do_reconnect_incident && ndb_binlog_running) {
    if (check_reconnect_incident(thd, inj, reconnect_incident_id)) {
      // Incident written, don't report incident again unless Ndb_binlog_thread
      // is restarted
      do_reconnect_incident = false;
    }
  }
  reconnect_incident_id = CLUSTER_DISCONNECT;

  // Handle pending purge requests from before "server started" state
  recall_pending_purges(thd);

  {
    log_verbose(1, "Wait for cluster to start");
    thd->proc_info = "Waiting for ndbcluster to start";
    thd_set_thd_ndb(thd, thd_ndb);

    while (!ndbcluster_is_connected(1) || !binlog_setup.setup(thd_ndb)) {
      // Failed to complete binlog_setup, remove all existing event
      // operations from potential partial setup
      remove_all_event_operations(s_ndb, i_ndb);

      // Fail any schema operations that has been registered but
      // never reached the coordinator
      NDB_SCHEMA_OBJECT::fail_all_schema_ops(Ndb_schema_dist::COORD_ABORT,
                                             "Aborted after setup");

      if (!thd_ndb->valid_ndb()) {
        /*
          Cluster has gone away before setup was completed.
          Restart binlog
          thread to get rid of any garbage on the ndb objects
        */
        binlog_thread_state = BCCC_restart;
        goto err;
      }
      if (is_stop_requested()) {
        goto err;
      }
      if (thd->killed == THD::KILL_CONNECTION) {
        /*
          Since the ndb binlog thread adds itself to the "global thread list"
          it need to look at the "killed" flag and stop the thread to avoid
          that the server hangs during shutdown while waiting for the "global
          thread list" to be emtpy.
        */
        log_info(
            "Server shutdown detected while "
            "waiting for ndbcluster to start...");
        goto err;
      }
      ndb_milli_sleep(1000);
    }  // while (!ndb_binlog_setup())

    DBUG_ASSERT(ndbcluster_hton->slot != ~(uint)0);

    /*
      Prevent schema dist participant from (implicitly)
      taking GSL lock as part of taking MDL lock
    */
    thd_ndb->set_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT);
  }

  /* Apply privilege statements stored in snapshot */
  if (!Ndb_stored_grants::apply_stored_grants(thd)) {
    ndb_log_error("stored grants: failed to apply stored grants.");
  }

  schema_dist_data.init(g_ndb_cluster_connection);

  {
    log_verbose(1, "Wait for first event");
    // wait for the first event
    thd->proc_info = "Waiting for first event from ndbcluster";
    Uint64 schema_gci;
    do {
      DBUG_PRINT("info", ("Waiting for the first event"));

      if (is_stop_requested()) goto err;

      my_thread_yield();
      mysql_mutex_lock(&injector_event_mutex);
      (void)s_ndb->pollEvents(100, &schema_gci);
      mysql_mutex_unlock(&injector_event_mutex);
    } while (schema_gci == 0 || ndb_latest_received_binlog_epoch == schema_gci);

    if (ndb_binlog_running) {
      Uint64 gci = i_ndb->getLatestGCI();
      while (gci < schema_gci || gci == ndb_latest_received_binlog_epoch) {
        if (is_stop_requested()) goto err;

        my_thread_yield();
        mysql_mutex_lock(&injector_event_mutex);
        (void)i_ndb->pollEvents(10, &gci);
        mysql_mutex_unlock(&injector_event_mutex);
      }
      if (gci > schema_gci) {
        schema_gci = gci;
      }
    }
    // now check that we have epochs consistent with what we had before the
    // restart
    DBUG_PRINT("info", ("schema_gci: %u/%u", (uint)(schema_gci >> 32),
                        (uint)(schema_gci)));
    {
      i_ndb->flushIncompleteEvents(schema_gci);
      s_ndb->flushIncompleteEvents(schema_gci);
      if (schema_gci < ndb_latest_handled_binlog_epoch) {
        log_error(
            "cluster has been restarted --initial or with older filesystem. "
            "ndb_latest_handled_binlog_epoch: %u/%u, while current epoch: "
            "%u/%u. "
            "RESET MASTER should be issued. Resetting "
            "ndb_latest_handled_binlog_epoch.",
            (uint)(ndb_latest_handled_binlog_epoch >> 32),
            (uint)(ndb_latest_handled_binlog_epoch), (uint)(schema_gci >> 32),
            (uint)(schema_gci));
        ndb_set_latest_trans_gci(0);
        ndb_latest_handled_binlog_epoch = 0;
        ndb_latest_applied_binlog_epoch = 0;
        ndb_latest_received_binlog_epoch = 0;
        ndb_index_stat_restart();
      } else if (ndb_latest_applied_binlog_epoch > 0) {
        log_warning(
            "cluster has reconnected. "
            "Changes to the database that occurred while "
            "disconnected will not be in the binlog");
      }
      log_verbose(1, "starting log at epoch %u/%u", (uint)(schema_gci >> 32),
                  (uint)(schema_gci));
    }
    log_verbose(1, "Got first event");
  }
  /*
    binlog thread is ready to receive events
    - client threads may now start updating data, i.e. tables are
    no longer read only
  */
  mysql_mutex_lock(&injector_data_mutex);
  ndb_binlog_is_ready = true;
  mysql_mutex_unlock(&injector_data_mutex);

  log_verbose(1, "ndb tables writable");
  ndb_tdc_close_cached_tables();

  /*
     Signal any waiting thread that ndb table setup is
     now complete
  */
  ndb_notify_tables_writable();

  {
    static LEX_CSTRING db_lex_cstr = EMPTY_CSTR;
    thd->reset_db(db_lex_cstr);
  }

  log_verbose(1, "Startup and setup completed");

  /*
    Main NDB Injector loop
  */
  do_reconnect_incident = true;  // Report incident if disconnected
  binlog_thread_state = BCCC_running;

  /**
   * Injector loop runs until itself bring it out of 'BCCC_running' state,
   * or we get a stop-request from outside. In the later case we ensure that
   * all ongoing transaction epochs are completed first.
   */
  while (binlog_thread_state == BCCC_running &&
         (!is_stop_requested() ||
          ndb_latest_handled_binlog_epoch < ndb_get_latest_trans_gci())) {
#ifndef DBUG_OFF
    /**
     * As the Binlog thread is not a client thread, the 'set debug' commands
     * does not affect it. Update our thread-local debug settings from 'global'
     */
    {
      char buf[256];
      DBUG_EXPLAIN_INITIAL(buf, sizeof(buf));
      DBUG_SET(buf);
    }
#endif

    /*
      now we don't want any events before next gci is complete
    */
    thd->proc_info = "Waiting for event from ndbcluster";
    thd->set_time();

    /**
     * The binlog-thread holds the injector_mutex when waiting for
     * pollEvents() - which is >99% of the elapsed time. As the
     * native mutex guarantees no 'fairness', there is no guarantee
     * that another thread waiting for the mutex will immeditately
     * get the lock when unlocked by this thread. Thus this thread
     * may lock it again rather soon and starve the waiting thread.
     * To avoid this, my_thread_yield() is used to give any waiting
     * threads a chance to run and grab the injector_mutex when
     * it is available. The same pattern is used multiple places
     * in the BI-thread where there are wait-loops holding this mutex.
     */
    my_thread_yield();

    /* Can't hold mutex too long, so wait for events in 10ms steps */
    int tot_poll_wait = 10;

    // If there are remaining unhandled injector eventOp we continue
    // handling of these, else poll for more.
    if (i_pOp == NULL) {
      // Capture any dynamic changes to max_alloc
      i_ndb->set_eventbuf_max_alloc(opt_ndb_eventbuffer_max_alloc);

      mysql_mutex_lock(&injector_event_mutex);
      Uint64 latest_epoch = 0;
      const int poll_wait = (ndb_binlog_running) ? tot_poll_wait : 0;
      const int res = i_ndb->pollEvents(poll_wait, &latest_epoch);
      (void)res;  // Unused except DBUG_PRINT
      mysql_mutex_unlock(&injector_event_mutex);
      i_pOp = i_ndb->nextEvent();
      if (ndb_binlog_running) {
        ndb_latest_received_binlog_epoch = latest_epoch;
        tot_poll_wait = 0;
      }
      DBUG_PRINT("info", ("pollEvents res: %d", res));
    }

    // Epoch to handle from i_ndb. Use latest 'empty epoch' if no events.
    const Uint64 i_epoch =
        (i_pOp != NULL) ? i_pOp->getEpoch() : ndb_latest_received_binlog_epoch;

    // If there are remaining unhandled schema eventOp we continue
    // handling of these, else poll for more.
    if (s_pOp == NULL) {
      if (DBUG_EVALUATE_IF("ndb_binlog_injector_yield_before_schema_pollEvent",
                           true, false)) {
        /**
         * Simulate that the binlog thread yields the CPU inbetween
         * these two pollEvents, which can result in reading a
         * 'schema_gci > gci'. (Likely due to mutex locking)
         */
        ndb_milli_sleep(50);
      }

      Uint64 schema_epoch = 0;
      mysql_mutex_lock(&injector_event_mutex);
      int schema_res = s_ndb->pollEvents(tot_poll_wait, &schema_epoch);
      mysql_mutex_unlock(&injector_event_mutex);
      s_pOp = s_ndb->nextEvent();

      /*
        Make sure we have seen any schema epochs upto the injector epoch,
        or we have an earlier schema event to handle.
      */
      while (s_pOp == NULL && i_epoch > schema_epoch && schema_res >= 0) {
        static char buf[64];
        thd->proc_info = "Waiting for schema epoch";
        snprintf(buf, sizeof(buf), "%s %u/%u(%u/%u)", thd->proc_info,
                 (uint)(schema_epoch >> 32), (uint)(schema_epoch),
                 (uint)(ndb_latest_received_binlog_epoch >> 32),
                 (uint)(ndb_latest_received_binlog_epoch));
        thd->proc_info = buf;

        my_thread_yield();
        mysql_mutex_lock(&injector_event_mutex);
        schema_res = s_ndb->pollEvents(10, &schema_epoch);
        mysql_mutex_unlock(&injector_event_mutex);
        s_pOp = s_ndb->nextEvent();
      }
    }

    /*
      We have now a (possibly empty) set of available events which the
      binlog injects should apply. These could span either a single,
      or possibly multiple epochs. In order to get the ordering between
      schema events and 'ordinary' events injected in a correct order
      relative to each other, we apply them one epoch at a time, with
      the schema events always applied first.
    */

    // Calculate the epoch to handle events from in this iteration.
    const Uint64 current_epoch = find_epoch_to_handle(s_pOp, i_pOp);
    DBUG_ASSERT(current_epoch != 0 || !ndb_binlog_running);

    // Did someone else request injector thread to stop?
    DBUG_ASSERT(binlog_thread_state == BCCC_running);
    if (is_stop_requested() &&
        (ndb_latest_handled_binlog_epoch >= ndb_get_latest_trans_gci() ||
         !ndb_binlog_running))
      break; /* Stopping thread */

    if (thd->killed == THD::KILL_CONNECTION) {
      /*
        Since the ndb binlog thread adds itself to the "global thread list"
        it need to look at the "killed" flag and stop the thread to avoid
        that the server hangs during shutdown while waiting for the "global
        thread list" to be emtpy.
        In pre 5.6 versions the thread was also added to "global thread
        list" but the "global thread *count*" variable was not incremented
        and thus the same problem didn't exist.
        The only reason for adding the ndb binlog thread to "global thread
        list" is to be able to see the thread state using SHOW PROCESSLIST
        and I_S.PROCESSLIST
      */
      log_info("Server shutdown detected...");
      break;
    }

    MEM_ROOT **root_ptr = THR_MALLOC;
    MEM_ROOT *old_root = *root_ptr;
    MEM_ROOT mem_root;
    init_sql_alloc(PSI_INSTRUMENT_ME, &mem_root, 4096, 0);

    // The Ndb_schema_event_handler does not necessarily need
    // to use the same memroot(or vice versa)
    Ndb_schema_event_handler schema_event_handler(
        thd, &mem_root, g_ndb_cluster_connection->node_id(), schema_dist_data);

    *root_ptr = &mem_root;

    if (unlikely(s_pOp != NULL && s_pOp->getEpoch() == current_epoch)) {
      thd->proc_info = "Processing events from schema table";
      g_ndb_log_slave_updates = opt_log_slave_updates;
      s_ndb->setReportThreshEventGCISlip(
          opt_ndb_report_thresh_binlog_epoch_slip);
      s_ndb->setReportThreshEventFreeMem(
          opt_ndb_report_thresh_binlog_mem_usage);

      // Handle all schema event, limit within 'current_epoch'
      while (s_pOp != NULL && s_pOp->getEpoch() == current_epoch) {
        if (!s_pOp->hasError()) {
          schema_event_handler.handle_event(s_ndb, s_pOp);

          if (DBUG_EVALUATE_IF("ndb_binlog_slow_failure_handling", true,
                               false)) {
            if (!ndb_binlog_is_ready) {
              log_info("Just lost schema connection, hanging around");
              ndb_milli_sleep(10 * 1000);  // seconds * 1000
              /* There could be a race where client side reconnect before we
               * are able to detect 's_ndb->getEventOperation() == NULL'.
               * Thus, we never restart the binlog thread as supposed to.
               * -> 'ndb_binlog_is_ready' remains false and we get stuck in
               * RO-mode
               */
              log_info("...and on our way");
            }
          }

          DBUG_PRINT("info",
                     ("s_ndb first: %s", s_ndb->getEventOperation()
                                             ? s_ndb->getEventOperation()
                                                   ->getEvent()
                                                   ->getTable()
                                                   ->getName()
                                             : "<empty>"));
          DBUG_PRINT("info",
                     ("i_ndb first: %s", i_ndb->getEventOperation()
                                             ? i_ndb->getEventOperation()
                                                   ->getEvent()
                                                   ->getTable()
                                                   ->getName()
                                             : "<empty>"));
        } else {
          log_error("error %d (%s) on handling binlog schema event",
                    s_pOp->getNdbError().code, s_pOp->getNdbError().message);
        }
        s_pOp = s_ndb->nextEvent();
      }
      update_injector_stats(s_ndb, i_ndb);
    }

    Uint64 inconsistent_epoch = 0;
    if (!ndb_binlog_running) {
      /*
        Just consume any events, not used if no binlogging
        e.g. node failure events
      */
      while (i_pOp != NULL && i_pOp->getEpoch() == current_epoch) {
        if ((unsigned)i_pOp->getEventType() >=
            (unsigned)NDBEVENT::TE_FIRST_NON_DATA_EVENT) {
          ndb_binlog_index_row row;
          handle_non_data_event(thd, i_pOp, row);
        }
        i_pOp = i_ndb->nextEvent();
      }
      update_injector_stats(s_ndb, i_ndb);
    }

    // i_pOp == NULL means an inconsistent epoch or the queue is empty
    else if (i_pOp == NULL && !i_ndb->isConsistent(inconsistent_epoch)) {
      char errmsg[72];
      snprintf(errmsg, sizeof(errmsg),
               "Detected missing data in GCI %llu, "
               "inserting GAP event",
               inconsistent_epoch);
      DBUG_PRINT("info", ("Detected missing data in GCI %llu, "
                          "inserting GAP event",
                          inconsistent_epoch));
      LEX_CSTRING const msg = {errmsg, strlen(errmsg)};
      inj->record_incident(
          thd, binary_log::Incident_event::INCIDENT_LOST_EVENTS, msg);
    }

    /* Handle all events withing 'current_epoch', or possible
     * log an empty epoch if log_empty_epoch is specified.
     */
    else if ((i_pOp != NULL && i_pOp->getEpoch() == current_epoch) ||
             (ndb_log_empty_epochs() &&
              current_epoch > ndb_latest_handled_binlog_epoch)) {
      thd->proc_info = "Processing events";
      ndb_binlog_index_row _row;
      ndb_binlog_index_row *rows = &_row;
      injector::transaction trans;
      unsigned trans_row_count = 0;
      unsigned trans_slave_row_count = 0;

      if (i_pOp == NULL || i_pOp->getEpoch() != current_epoch) {
        /*
          Must be an empty epoch since the condition
          (ndb_log_empty_epochs() &&
           current_epoch > ndb_latest_handled_binlog_epoch)
          must be true we write empty epoch into
          ndb_binlog_index
        */
        DBUG_ASSERT(ndb_log_empty_epochs());
        DBUG_ASSERT(current_epoch > ndb_latest_handled_binlog_epoch);
        DBUG_PRINT("info", ("Writing empty epoch for gci %llu", current_epoch));
        DBUG_PRINT("info", ("Initializing transaction"));
        inj->new_trans(thd, &trans);
        rows = &_row;
        memset(&_row, 0, sizeof(_row));
        thd->variables.character_set_client = &my_charset_latin1;
        goto commit_to_binlog;
      } else {
        assert(i_pOp != NULL && i_pOp->getEpoch() == current_epoch);
        rows = &_row;

        DBUG_PRINT("info",
                   ("Handling epoch: %u/%u", (uint)(current_epoch >> 32),
                    (uint)(current_epoch)));
        // sometimes get TE_ALTER with invalid table
        DBUG_ASSERT(
            i_pOp->getEventType() == NdbDictionary::Event::TE_ALTER ||
            !ndb_name_is_blob_prefix(i_pOp->getEvent()->getTable()->getName()));
        DBUG_ASSERT(current_epoch <= ndb_latest_received_binlog_epoch);

        /* Update our thread-local debug settings based on the global */
#ifndef DBUG_OFF
        /* Get value of global...*/
        {
          char buf[256];
          DBUG_EXPLAIN_INITIAL(buf, sizeof(buf));
          //  fprintf(stderr, "Ndb Binlog Injector, setting debug to %s\n",
          //          buf);
          DBUG_SET(buf);
        }
#endif

        /* initialize some variables for this epoch */

        i_ndb->set_eventbuf_max_alloc(opt_ndb_eventbuffer_max_alloc);
        g_ndb_log_slave_updates = opt_log_slave_updates;
        i_ndb->setReportThreshEventGCISlip(
            opt_ndb_report_thresh_binlog_epoch_slip);
        i_ndb->setReportThreshEventFreeMem(
            opt_ndb_report_thresh_binlog_mem_usage);

        memset(&_row, 0, sizeof(_row));
        thd->variables.character_set_client = &my_charset_latin1;
        DBUG_PRINT("info", ("Initializing transaction"));
        inj->new_trans(thd, &trans);
        trans_row_count = 0;
        trans_slave_row_count = 0;
        // pass table map before epoch
        {
          Uint32 iter = 0;
          const NdbEventOperation *gci_op;
          Uint32 event_types;
          Uint32 cumulative_any_value;

          while ((gci_op = i_ndb->getNextEventOpInEpoch3(
                      &iter, &event_types, &cumulative_any_value)) != NULL) {
            Ndb_event_data *event_data =
                (Ndb_event_data *)gci_op->getCustomData();
            NDB_SHARE *share = (event_data) ? event_data->share : NULL;
            DBUG_PRINT("info",
                       ("per gci_op: 0x%lx  share: 0x%lx  event_types: 0x%x",
                        (long)gci_op, (long)share, event_types));
            // workaround for interface returning TE_STOP events
            // which are normally filtered out below in the nextEvent loop
            if ((event_types & ~NdbDictionary::Event::TE_STOP) == 0) {
              DBUG_PRINT("info", ("Skipped TE_STOP on table %s",
                                  gci_op->getEvent()->getTable()->getName()));
              continue;
            }
            // this should not happen
            if (share == NULL || event_data->shadow_table == NULL) {
              DBUG_PRINT("info", ("no share or table %s!",
                                  gci_op->getEvent()->getTable()->getName()));
              continue;
            }
            if (share == ndb_apply_status_share) {
              // skip this table, it is handled specially
              continue;
            }
            TABLE *table = event_data->shadow_table;
#ifndef DBUG_OFF
            const LEX_CSTRING &name = table->s->table_name;
#endif
            if ((event_types & (NdbDictionary::Event::TE_INSERT |
                                NdbDictionary::Event::TE_UPDATE |
                                NdbDictionary::Event::TE_DELETE)) == 0) {
              DBUG_PRINT("info", ("skipping non data event table: %.*s",
                                  (int)name.length, name.str));
              continue;
            }
            if (!trans.good()) {
              DBUG_PRINT("info",
                         ("Found new data event, initializing transaction"));
              inj->new_trans(thd, &trans);
            }
            {
              bool use_table = true;
              if (ndbcluster_anyvalue_is_reserved(cumulative_any_value)) {
                /*
                   All events for this table in this epoch are marked as
                   nologging, therefore we do not include the table in the epoch
                   transaction.
                */
                if (ndbcluster_anyvalue_is_nologging(cumulative_any_value)) {
                  DBUG_PRINT("info", ("Skip binlogging table table: %.*s",
                                      (int)name.length, name.str));
                  use_table = false;
                }
              }
              if (use_table) {
                DBUG_PRINT("info",
                           ("use_table: %.*s, cols %u", (int)name.length,
                            name.str, table->s->fields));
                injector::transaction::table tbl(table, true);
                int ret = trans.use_table(::server_id, tbl);
                ndbcluster::ndbrequire(ret == 0);
              }
            }
          }
        }
        if (trans.good()) {
          /* Inject ndb_apply_status WRITE_ROW event */
          if (!injectApplyStatusWriteRow(trans, current_epoch)) {
            log_error("Failed to inject apply status write row");
          }
        }

        do {
          if (i_pOp->hasError() && handle_error(i_pOp) < 0) goto err;

#ifndef DBUG_OFF
          {
            Ndb_event_data *event_data =
                (Ndb_event_data *)i_pOp->getCustomData();
            NDB_SHARE *share = (event_data) ? event_data->share : NULL;
            DBUG_PRINT("info",
                       ("EVENT TYPE: %d  Epoch: %u/%u last applied: %u/%u  "
                        "share: 0x%lx (%s.%s)",
                        i_pOp->getEventType(), (uint)(current_epoch >> 32),
                        (uint)(current_epoch),
                        (uint)(ndb_latest_applied_binlog_epoch >> 32),
                        (uint)(ndb_latest_applied_binlog_epoch), (long)share,
                        share ? share->db : "'NULL'",
                        share ? share->table_name : "'NULL'"));
            DBUG_ASSERT(share != 0);
          }
          // assert that there is consistancy between gci op list
          // and event list
          {
            Uint32 iter = 0;
            const NdbEventOperation *gci_op;
            Uint32 event_types;
            while ((gci_op = i_ndb->getGCIEventOperations(
                        &iter, &event_types)) != NULL) {
              if (gci_op == i_pOp) break;
            }
            DBUG_ASSERT(gci_op == i_pOp);
            DBUG_ASSERT((event_types & i_pOp->getEventType()) != 0);
          }
#endif

          if ((unsigned)i_pOp->getEventType() <
              (unsigned)NDBEVENT::TE_FIRST_NON_DATA_EVENT)
            handle_data_event(i_pOp, &rows, trans, trans_row_count,
                              trans_slave_row_count);
          else {
            handle_non_data_event(thd, i_pOp, *rows);
            DBUG_PRINT("info",
                       ("s_ndb first: %s", s_ndb->getEventOperation()
                                               ? s_ndb->getEventOperation()
                                                     ->getEvent()
                                                     ->getTable()
                                                     ->getName()
                                               : "<empty>"));
            DBUG_PRINT("info",
                       ("i_ndb first: %s", i_ndb->getEventOperation()
                                               ? i_ndb->getEventOperation()
                                                     ->getEvent()
                                                     ->getTable()
                                                     ->getName()
                                               : "<empty>"));
          }

          // Capture any dynamic changes to max_alloc
          i_ndb->set_eventbuf_max_alloc(opt_ndb_eventbuffer_max_alloc);

          i_pOp = i_ndb->nextEvent();
        } while (i_pOp && i_pOp->getEpoch() == current_epoch);

        update_injector_stats(s_ndb, i_ndb);

        /*
          NOTE: i_pOp is now referring to an event in the next epoch
          or is == NULL
        */

        while (trans.good()) {
        commit_to_binlog:
          if (!ndb_log_empty_epochs()) {
            /*
              If
                - We did not add any 'real' rows to the Binlog AND
                - We did not apply any slave row updates, only
                  ndb_apply_status updates
              THEN
                Don't write the Binlog transaction which just
                contains ndb_apply_status updates.
                (For cicular rep with log_apply_status, ndb_apply_status
                updates will propagate while some related, real update
                is propagating)
            */
            if ((trans_row_count == 0) &&
                (!(opt_ndb_log_apply_status && trans_slave_row_count))) {
              /* nothing to commit, rollback instead */
              if (int r = trans.rollback()) {
                log_error("Error during ROLLBACK of GCI %u/%u. Error: %d",
                          uint(current_epoch >> 32), uint(current_epoch), r);
                /* TODO: Further handling? */
              }
              break;
            }
          }
          thd->proc_info = "Committing events to binlog";
          if (int r = trans.commit()) {
            log_error("Error during COMMIT of GCI. Error: %d", r);
            /* TODO: Further handling? */
          }
          injector::transaction::binlog_pos start = trans.start_pos();
          injector::transaction::binlog_pos next = trans.next_pos();
          rows->gci = (Uint32)(current_epoch >> 32);  // Expose gci hi/lo
          rows->epoch = current_epoch;
          rows->start_master_log_file = start.file_name();
          rows->start_master_log_pos = start.file_pos();
          if ((next.file_pos() == 0) && ndb_log_empty_epochs()) {
            /* Empty transaction 'committed' due to log_empty_epochs
             * therefore no next position
             */
            rows->next_master_log_file = start.file_name();
            rows->next_master_log_pos = start.file_pos();
          } else {
            rows->next_master_log_file = next.file_name();
            rows->next_master_log_pos = next.file_pos();
          }

          DBUG_PRINT("info", ("COMMIT epoch: %lu", (ulong)current_epoch));
          if (opt_ndb_log_binlog_index) {
            if (Ndb_binlog_index_table_util::write_rows(thd, rows)) {
              /*
                 Writing to ndb_binlog_index failed, check if it's because THD
                 have been killed and retry in such case
              */
              if (thd->killed) {
                DBUG_PRINT("error", ("Failed to write to ndb_binlog_index at "
                                     "shutdown, retrying"));
                Ndb_binlog_index_table_util::write_rows_retry_after_kill(thd,
                                                                         rows);
              }
            }
          }
          ndb_latest_applied_binlog_epoch = current_epoch;
          break;
        }  // while (trans.good())

        /*
          NOTE: There are possible more i_pOp available.
          However, these are from another epoch and should be handled
          in next iteration of the binlog injector loop.
        */
      }
    }  // end: 'handled a 'current_epoch' of i_pOp's

    // Notify the schema event handler about post_epoch so it may finish
    // any outstanding business
    schema_event_handler.post_epoch(current_epoch);

    free_root(&mem_root, MYF(0));
    *root_ptr = old_root;

    if (current_epoch > ndb_latest_handled_binlog_epoch) {
      Mutex_guard injector_mutex_g(injector_data_mutex);
      ndb_latest_handled_binlog_epoch = current_epoch;
      // Signal ndbcluster_binlog_wait'ers
      mysql_cond_broadcast(&injector_data_cond);
    }

    DBUG_ASSERT(binlog_thread_state == BCCC_running);

    // When a cluster failure occurs, each event operation will receive a
    // TE_CLUSTER_FAILURE event causing it to be torn down and removed.
    // When all event operations has been removed from their respective Ndb
    // object, the thread should restart and try to connect to NDB again.
    if (i_ndb->getEventOperation() == NULL &&
        s_ndb->getEventOperation() == NULL) {
      log_error("All event operations gone, restarting thread");
      binlog_thread_state = BCCC_restart;
      break;
    }

    if (!ndb_binlog_tables_inited /* relaxed read without lock */) {
      // One(or more) of the ndbcluster util tables have been dropped, restart
      // the thread in order to create or setup the util table(s) again
      log_error("The util tables has been lost, restarting thread");
      binlog_thread_state = BCCC_restart;
      break;
    }

    // Synchronize 1 object from the queue of objects detected for automatic
    // synchronization
    synchronize_detected_object(thd);
  }

  // Check if loop has been terminated without properly handling all events
  if (ndb_binlog_running &&
      ndb_latest_handled_binlog_epoch < ndb_get_latest_trans_gci()) {
    log_error(
        "latest transaction in epoch %u/%u not in binlog "
        "as latest handled epoch is %u/%u",
        (uint)(ndb_get_latest_trans_gci() >> 32),
        (uint)(ndb_get_latest_trans_gci()),
        (uint)(ndb_latest_handled_binlog_epoch >> 32),
        (uint)(ndb_latest_handled_binlog_epoch));
  }

err:
  if (binlog_thread_state != BCCC_restart) {
    log_info("Shutting down");
    thd->proc_info = "Shutting down";
  } else {
    log_info("Restarting");
    thd->proc_info = "Restarting";
  }

  mysql_mutex_lock(&injector_event_mutex);
  /* don't mess with the injector_ndb anymore from other threads */
  injector_thd = NULL;
  injector_ndb = NULL;
  schema_ndb = NULL;
  mysql_mutex_unlock(&injector_event_mutex);

  mysql_mutex_lock(&injector_data_mutex);
  ndb_binlog_tables_inited = false;
  mysql_mutex_unlock(&injector_data_mutex);

  Ndb_stored_grants::shutdown(thd_ndb);

  thd->reset_db(NULL_CSTR);  // as not to try to free memory
  remove_all_event_operations(s_ndb, i_ndb);

  schema_dist_data.release();

  // Fail any schema operations that has been registered but
  // never reached the coordinator
  NDB_SCHEMA_OBJECT::fail_all_schema_ops(Ndb_schema_dist::COORD_ABORT,
                                         "Aborted during shutdown");

  delete s_ndb;
  s_ndb = NULL;

  delete i_ndb;
  i_ndb = NULL;

  if (thd_ndb) {
    Thd_ndb::release(thd_ndb);
    thd_set_thd_ndb(thd, NULL);
    thd_ndb = NULL;
  }

  /**
   * release all extra references from tables
   */
  log_verbose(9, "Release extra share references");
  NDB_SHARE::release_extra_share_references();

  log_info("Stopping...");

  ndb_tdc_close_cached_tables();
  if (ndb_log_get_verbose_level() > 15) {
    NDB_SHARE::print_remaining_open_tables();
  }

  if (binlog_thread_state == BCCC_restart) {
    goto restart_cluster_failure;
  }

  // Release the thd->net created without vio
  thd->get_protocol_classic()->end_net();
  thd->release_resources();
  thd_manager->remove_thd(thd);
  delete thd;

  ndb_binlog_running = false;
  mysql_cond_broadcast(&injector_data_cond);

  log_info("Stopped");

  DBUG_PRINT("exit", ("ndb_binlog_thread"));
}

/*
  Return string containing current status of ndb binlog as
  comma separated name value pairs.

  Used by ndbcluster_show_status() to fill the "binlog" row
  in result of SHOW ENGINE NDB STATUS

  @param     buf     The buffer where to print status string
  @param     bufzies Size of the buffer

  @return    Length of the string printed to "buf" or 0 if no string
             is printed
*/

size_t ndbcluster_show_status_binlog(char *buf, size_t buf_size) {
  DBUG_TRACE;

  mysql_mutex_lock(&injector_event_mutex);
  if (injector_ndb) {
    const ulonglong latest_epoch = injector_ndb->getLatestGCI();
    mysql_mutex_unlock(&injector_event_mutex);

    // Get highest trans gci seen by the cluster connections
    const ulonglong latest_trans_epoch = ndb_get_latest_trans_gci();

    const size_t buf_len = snprintf(
        buf, buf_size,
        "latest_epoch=%llu, "
        "latest_trans_epoch=%llu, "
        "latest_received_binlog_epoch=%llu, "
        "latest_handled_binlog_epoch=%llu, "
        "latest_applied_binlog_epoch=%llu",
        latest_epoch, latest_trans_epoch, ndb_latest_received_binlog_epoch,
        ndb_latest_handled_binlog_epoch, ndb_latest_applied_binlog_epoch);
    return buf_len;
  } else
    mysql_mutex_unlock(&injector_event_mutex);
  return 0;
}

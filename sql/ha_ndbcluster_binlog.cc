/*
  Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ha_ndbcluster_binlog.h"

#include <mysql/psi/mysql_thread.h>

#include "my_dbug.h"
#include "my_thread.h"
#include "mysql/plugin.h"
#include "sql/binlog.h"
#include "sql/dd/types/abstract_table.h" // dd::enum_table_type
#include "sql/derror.h"     // ER_THD
#include "sql/ha_ndbcluster.h"
#include "sql/ha_ndbcluster_connection.h"
#include "sql/mysqld.h"     // opt_bin_log
#include "sql/mysqld_thd_manager.h" // Global_THD_manager
#include "sql/ndb_binlog_client.h"
#include "sql/ndb_bitmap.h"
#include "sql/ndb_dd.h"
#include "sql/ndb_dd_client.h"
#include "sql/ndb_dd_table.h"
#include "sql/ndb_global_schema_lock.h"
#include "sql/ndb_global_schema_lock_guard.h"
#include "sql/ndb_local_connection.h"
#include "sql/ndb_log.h"
#include "sql/ndb_name_util.h"
#include "sql/ndb_ndbapi_util.h"
#include "sql/ndb_sleep.h"
#include "sql/ndb_table_guard.h"
#include "sql/ndb_tdc.h"
#include "sql/ndb_thd.h"
#include "sql/rpl_injector.h"
#include "sql/rpl_slave.h"
#include "sql/sql_lex.h"
#include "sql/sql_table.h"  // build_table_filename,
#include "sql/thd_raii.h"
#include "sql/transaction.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/ndb_cluster_connection.hpp"

typedef NdbDictionary::Event NDBEVENT;
typedef NdbDictionary::Object NDBOBJ;
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Dictionary NDBDICT;

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

bool ndb_log_empty_epochs(void);

void ndb_index_stat_restart();

#include "sql/ha_ndbcluster_tables.h"
#include "sql/ndb_anyvalue.h"
#include "sql/ndb_binlog_extra_row_info.h"
#include "sql/ndb_binlog_thread.h"
#include "sql/ndb_dist_priv_util.h"
#include "sql/ndb_event_data.h"
#include "sql/ndb_repl_tab.h"
#include "sql/ndb_schema_dist.h"
#include "sql/ndb_schema_object.h"

extern Ndb_cluster_connection* g_ndb_cluster_connection;

/*
  Timeout for syncing schema events between
  mysql servers, and between mysql server and the binlog
*/
static const int DEFAULT_SYNC_TIMEOUT= 120;

/* Column numbers in the ndb_binlog_index table */
enum Ndb_binlog_index_cols
{
  NBICOL_START_POS                 = 0
  ,NBICOL_START_FILE               = 1
  ,NBICOL_EPOCH                    = 2
  ,NBICOL_NUM_INSERTS              = 3
  ,NBICOL_NUM_UPDATES              = 4
  ,NBICOL_NUM_DELETES              = 5
  ,NBICOL_NUM_SCHEMAOPS            = 6
  /* Following colums in schema 'v2' */
  ,NBICOL_ORIG_SERVERID            = 7
  ,NBICOL_ORIG_EPOCH               = 8
  ,NBICOL_GCI                      = 9
  /* Following columns in schema 'v3' */
  ,NBICOL_NEXT_POS                 = 10
  ,NBICOL_NEXT_FILE                = 11
};

class Mutex_guard
{
public:
  Mutex_guard(mysql_mutex_t &mutex) : m_mutex(mutex)
  {
    mysql_mutex_lock(&m_mutex);
  }
  ~Mutex_guard()
  {
    mysql_mutex_unlock(&m_mutex);
  }
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
static mysql_cond_t  injector_data_cond;

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

/*
  Flag showing if the ndb binlog should be created, if so == true
  false if not
*/
bool ndb_binlog_running= false;
static bool ndb_binlog_tables_inited= false;  //injector_data_mutex, relaxed
static bool ndb_binlog_is_ready= false;       //injector_data_mutex, relaxed
 
bool
ndb_binlog_is_read_only(void)
{
  /*
    Could be called from any client thread. Need a mutex to 
    protect ndb_binlog_tables_inited and ndb_binlog_is_ready.
  */
  Mutex_guard injector_g(injector_data_mutex);
  if (!ndb_binlog_tables_inited)
  {
    /* the ndb_* system tables not setup yet */
    return true;
  }

  if (ndb_binlog_running && !ndb_binlog_is_ready)
  {
    /*
      The binlog thread is supposed to write to binlog
      but not ready (still initializing or has lost connection)
    */
    return true;
  }
  return false;
}

static THD *injector_thd= NULL;

/*
  Global reference to ndb injector thd object.

  Used mainly by the binlog index thread, but exposed to the client sql
  thread for one reason; to setup the events operations for a table
  to enable ndb injector thread receiving events.

  Must therefore always be used with a surrounding
  mysql_mutex_lock(&injector_event_mutex), when create/dropEventOperation
*/
static Ndb *injector_ndb= NULL;  //Need injector_event_mutex
static Ndb *schema_ndb= NULL;    //Need injector_event_mutex

static int ndbcluster_binlog_inited= 0;

/* NDB Injector thread (used for binlog creation) */
static ulonglong ndb_latest_applied_binlog_epoch= 0;
static ulonglong ndb_latest_handled_binlog_epoch= 0;
static ulonglong ndb_latest_received_binlog_epoch= 0;

NDB_SHARE *ndb_apply_status_share= NULL;
static NDB_SHARE *ndb_schema_share= NULL; //Need injector_data_mutex

extern bool opt_log_slave_updates;
static bool g_ndb_log_slave_updates;

static bool g_injector_v1_warning_emitted = false;

static void remove_all_event_operations(Ndb *s_ndb, Ndb *i_ndb);

bool ndb_schema_dist_is_ready(void)
{
  Mutex_guard schema_share_g(injector_data_mutex);
  if (ndb_schema_share)
    return true;

  DBUG_PRINT("info", ("ndb schema dist not ready"));
  return false;
}


static void run_query(THD *thd, char *buf, char *end,
                      const int *no_print_error)
{
  /*
    NOTE! Don't use this function for new implementation, backward
    compat. only
  */

  Ndb_local_connection mysqld(thd);

  /*
    Run the query, suppress some errors from being printed
    to log and ignore any error returned
  */
  (void)mysqld.raw_run_query(buf, (end - buf),
                             no_print_error);
}


bool
Ndb_binlog_client::create_event_data(NDB_SHARE *share,
                                     const dd::Table *table_def,
                                     Ndb_event_data **event_data) const
{
  DBUG_ENTER("Ndb_binlog_client::create_event_data");
  DBUG_ASSERT(table_def);
  DBUG_ASSERT(event_data);

  Ndb_event_data* new_event_data =
      Ndb_event_data::create_event_data(m_thd, share,
                                        share->db, share->table_name,
                                        share->key_string(), injector_thd,
                                        table_def);
  if (!new_event_data)
    DBUG_RETURN(false);

  // Return the newly created event_data to caller
  *event_data = new_event_data;

  DBUG_RETURN(true);
}


static int
get_ndb_blobs_value(TABLE* table, NdbValue* value_array,
                    uchar*& buffer, uint& buffer_size,
                    my_ptrdiff_t ptrdiff)
{
  DBUG_ENTER("get_ndb_blobs_value");

  // Field has no field number so cannot use TABLE blob_field
  // Loop twice, first only counting total buffer size
  for (int loop= 0; loop <= 1; loop++)
  {
    uint32 offset= 0;
    for (uint i= 0; i < table->s->fields; i++)
    {
      Field *field= table->field[i];
      NdbValue value= value_array[i];
      if (! (field->flags & BLOB_FLAG && field->stored_in_db))
        continue;
      if (value.blob == NULL)
      {
        DBUG_PRINT("info",("[%u] skipped", i));
        continue;
      }
      Field_blob *field_blob= (Field_blob *)field;
      NdbBlob *ndb_blob= value.blob;
      int isNull;
      if (ndb_blob->getNull(isNull) != 0)
        DBUG_RETURN(-1);
      if (isNull == 0) {
        Uint64 len64= 0;
        if (ndb_blob->getLength(len64) != 0)
          DBUG_RETURN(-1);
        // Align to Uint64
        uint32 size= Uint32(len64);
        if (size % 8 != 0)
          size+= 8 - size % 8;
        if (loop == 1)
        {
          uchar *buf= buffer + offset;
          uint32 len= buffer_size - offset;  // Size of buf
          if (ndb_blob->readData(buf, len) != 0)
            DBUG_RETURN(-1);
          DBUG_PRINT("info", ("[%u] offset: %u  buf: 0x%lx  len=%u  [ptrdiff=%d]",
                              i, offset, (long) buf, len, (int)ptrdiff));
          DBUG_ASSERT(len == len64);
          // Ugly hack assumes only ptr needs to be changed
          field_blob->set_ptr_offset(ptrdiff, len, buf);
        }
        offset+= size;
      }
      else if (loop == 1) // undefined or null
      {
        // have to set length even in this case
        uchar *buf= buffer + offset; // or maybe NULL
        uint32 len= 0;
        field_blob->set_ptr_offset(ptrdiff, len, buf);
        DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
      }
    }
    if (loop == 0 && offset > buffer_size)
    {
      my_free(buffer);
      buffer_size= 0;
      DBUG_PRINT("info", ("allocate blobs buffer size %u", offset));
      buffer= (uchar*) my_malloc(PSI_INSTRUMENT_ME, offset, MYF(MY_WME));
      if (buffer == NULL)
      {
        ndb_log_error("get_ndb_blobs_value, my_malloc(%u) failed", offset);
        DBUG_RETURN(-1);
      }
      buffer_size= offset;
    }
  }
  DBUG_RETURN(0);
}


/*
  @brief Wait until the last committed epoch from the session enters the
         binlog. Wait a maximum of 30 seconds. This wait is necessary in
         SHOW BINLOG EVENTS so that the user see its own changes. Also
         in RESET MASTER before clearing ndbcluster's binlog index.
  @param thd Thread handle to wait for its changes to enter the binlog.
*/
static void ndbcluster_binlog_wait(THD *thd)
{
  DBUG_ENTER("ndbcluster_binlog_wait");

  if (!ndb_binlog_running)
  {
    DBUG_PRINT("exit", ("Not writing binlog -> nothing to wait for"));
    DBUG_VOID_RETURN;
  }

  // Assumption is that only these commands will wait
  DBUG_ASSERT(thd_sql_command(thd) == SQLCOM_SHOW_BINLOG_EVENTS ||
              thd_sql_command(thd) == SQLCOM_FLUSH ||
              thd_sql_command(thd) == SQLCOM_RESET);

  if (thd->system_thread == SYSTEM_THREAD_NDBCLUSTER_BINLOG)
  {
    // Binlog Injector thread should not wait for itself
    DBUG_PRINT("exit", ("binlog injector should not wait for itself"));
    DBUG_VOID_RETURN;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (!thd_ndb)
  {
    // Thread has not used NDB before, no need for waiting
    DBUG_PRINT("exit", ("Thread has not used NDB, nothing to wait for"));
    DBUG_VOID_RETURN;
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
  int count = 30; // seconds
  mysql_mutex_lock(&injector_data_mutex);
  const Uint64 start_handled_epoch = ndb_latest_handled_binlog_epoch;
  while (!thd->killed && count && ndb_binlog_running &&
         (ndb_latest_handled_binlog_epoch == 0 ||
          ndb_latest_handled_binlog_epoch < session_last_committed_epoch))
  {
    count--;
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&injector_data_cond, &injector_data_mutex, &abstime);
  }
  mysql_mutex_unlock(&injector_data_mutex);

  if (count == 0)
  {
    ndb_log_warning("Thread id %u timed out (30s) waiting for epoch %u/%u "
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
  DBUG_VOID_RETURN;
}

/*
  Setup THD object
  'Inspired' from ha_ndbcluster.cc : ndb_util_thread_func
*/
THD *
ndb_create_thd(char * stackptr)
{
  DBUG_ENTER("ndb_create_thd");
  THD * thd= new THD; /* note that constructor of THD uses DBUG_ */
  if (thd == 0)
  {
    DBUG_RETURN(0);
  }
  THD_CHECK_SENTRY(thd);

  thd->thread_stack= stackptr; /* remember where our stack is */
  if (thd->store_globals())
  {
    delete thd;
    DBUG_RETURN(0);
  }

  thd->init_query_mem_roots();
  thd->set_command(COM_DAEMON);
  thd->system_thread= SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->get_protocol_classic()->set_client_capabilities(0);
  thd->lex->start_transaction_opt= 0;
  thd->security_context()->skip_grants();

  CHARSET_INFO *charset_connection= get_charset_by_csname("utf8",
                                                          MY_CS_PRIMARY,
                                                          MYF(MY_WME));
  thd->variables.character_set_client= charset_connection;
  thd->variables.character_set_results= charset_connection;
  thd->variables.collation_connection= charset_connection;
  thd->update_charset();
  DBUG_RETURN(thd);
}

/*
  Called from MYSQL_BIN_LOG::purge_logs in log.cc when the binlog "file"
  is removed
*/

static int
ndbcluster_binlog_index_purge_file(THD *passed_thd, const char *file)
{
  int stack_base = 0;
  int error = 0;
  DBUG_ENTER("ndbcluster_binlog_index_purge_file");
  DBUG_PRINT("enter", ("file: %s", file));

  if (!ndb_binlog_running || (passed_thd && passed_thd->slave_thread))
    DBUG_RETURN(0);

  /**
   * This function cannot safely reuse the passed thd object
   * due to the variety of places from which it is called.
   *   new/delete one...yuck!
   */
  THD* my_thd;
  if ((my_thd = ndb_create_thd((char*)&stack_base) /* stack ptr */) == 0)
  {
    /**
     * TODO return proper error code here,
     * BUT! return code is not (currently) checked in
     *      log.cc : purge_index_entry() so we settle for warning printout
     */
    ndb_log_warning("Unable to purge " NDB_REP_DB "." NDB_REP_TABLE
                    " File=%s (failed to setup thd)", file);
    DBUG_RETURN(0);
  }


  /*
    delete rows from mysql.ndb_binlog_index table for the given
    filename, if table does not exist ignore the error as it
    is a "consistent" behavior
  */
  Ndb_local_connection mysqld(my_thd);
  const bool ignore_no_such_table = true;

  // Set needed isolation level to be independent from server settings
  my_thd->variables.transaction_isolation= ISO_REPEATABLE_READ;
  // Turn autocommit on
  // This is needed to ensure calls to mysqld.delete_rows commits.
  my_thd->variables.option_bits&= ~OPTION_NOT_AUTOCOMMIT;
  // Ensure that file paths on Windows are not modified by parser
  my_thd->variables.sql_mode|= MODE_NO_BACKSLASH_ESCAPES;
  if(mysqld.delete_rows(STRING_WITH_LEN("mysql"),
                        STRING_WITH_LEN("ndb_binlog_index"),
                        ignore_no_such_table,
                        "File='", file, "'", NULL))
  {
    // Failed to delete rows from table
    error = 1;
  }

  delete my_thd;
  
  if (passed_thd)
  {
    /* Relink passed THD with this thread */
    passed_thd->store_globals();
  }

  DBUG_RETURN(error);
}


// Determine if privilege tables are distributed, ie. stored in NDB
bool
Ndb_dist_priv_util::priv_tables_are_in_ndb(THD* thd)
{
  bool distributed= false;
  Ndb_dist_priv_util dist_priv;
  DBUG_ENTER("ndbcluster_distributed_privileges");

  Ndb *ndb= check_ndb_in_thd(thd);
  if (!ndb)
    DBUG_RETURN(false); // MAGNUS, error message?

  if (ndb->setDatabaseName(dist_priv.database()) != 0)
    DBUG_RETURN(false);

  const char* table_name;
  while((table_name= dist_priv.iter_next_table()))
  {
    DBUG_PRINT("info", ("table_name: %s", table_name));
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), table_name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    if (ndbtab)
    {
      distributed= true;
    }
    else if (distributed)
    {
      ndb_log_error("Inconsistency detected in distributed "
                    "privilege tables. Table '%s.%s' is not distributed",
                    dist_priv.database(), table_name);
      DBUG_RETURN(false);
    }
  }
  DBUG_RETURN(distributed);
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

static void
ndbcluster_binlog_log_query(handlerton*, THD *thd,
                            enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char*)
{
  DBUG_ENTER("ndbcluster_binlog_log_query");
  DBUG_PRINT("enter", ("binlog_command: %d, db: '%s', query: '%s'",
                       binlog_command, db, query));

  switch (binlog_command) {
    case LOGCOM_CREATE_DB: {
      DBUG_PRINT("info", ("New database '%s' created", db));

      Ndb_schema_dist_client schema_dist_client(thd);

      if (!schema_dist_client.prepare(db, ""))
      {
        // Could not prepare the schema distribution client
        // NOTE! As there is no way return error, this may have to be
        // revisited, the prepare should be done
        // much earlier where it can return an error for the query
        DBUG_VOID_RETURN;
      }

      const bool result = schema_dist_client.create_db(query, query_length, db);
      if (!result) {
        // NOTE! There is currently no way to report an error from this
        // function, just log an error and proceed
        ndb_log_error("Failed to distribute 'CREATE DATABASE %s'", db);
      }
    } break;

    case LOGCOM_ALTER_DB: {
      DBUG_PRINT("info", ("The database '%s' was altered", db));

      Ndb_schema_dist_client schema_dist_client(thd);

      if (!schema_dist_client.prepare(db, ""))
      {
        // Could not prepare the schema distribution client
        // NOTE! As there is no way return error, this may have to be
        // revisited, the prepare should be done
        // much earlier where it can return an error for the query
        DBUG_VOID_RETURN;
      }

      const bool result = schema_dist_client.alter_db(query, query_length, db);
      if (!result) {
        // NOTE! There is currently no way to report an error from this
        // function, just log an error and proceed
        ndb_log_error("Failed to distribute 'ALTER DATABASE %s'", db);
      }
    } break;

    case LOGCOM_ACL_NOTIFY: {
      DBUG_PRINT("info", ("Privilege tables have been modified"));

      if (!Ndb_dist_priv_util::priv_tables_are_in_ndb(thd)) {
        DBUG_VOID_RETURN;
      }

      Ndb_schema_dist_client schema_dist_client(thd);

      if (!schema_dist_client.prepare(db, ""))
      {
        // Could not prepare the schema distribution client
        // NOTE! As there is no way return error, this may have to be
        // revisited, the prepare should be done
        // much earlier where it can return an error for the query
        DBUG_VOID_RETURN;
      }

      /*
        NOTE! Grant statements with db set to NULL is very rare but
        may be provoked by for example dropping the currently selected
        database. Since Ndb_schema_dist_client::log_schema_op() does not allow
        db to be NULL(can't create a key for the ndb_schema_object nor
        write NULL to ndb_schema), the situation is salvaged by setting db
        to the constant string "mysql" which should work in most cases.

        Interestingly enough this "hack" has the effect that grant statements
        are written to the remote binlog in same format as if db would have
        been NULL.
      */
      if (!db) {
        db = "mysql";
      }

      const bool result =
          schema_dist_client.acl_notify(query, query_length, db);
      if (!result) {
        // NOTE! There is currently no way to report an error from this
        // function, just log an error and proceed
        ndb_log_error("Failed to distribute '%s'", query);
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
  DBUG_VOID_RETURN;
}

// Instantiate Ndb_binlog_thread component
static Ndb_binlog_thread ndb_binlog_thread;


/*
  End use of the NDB Cluster binlog
   - wait for binlog thread to shutdown
*/

int ndbcluster_binlog_end()
{
  DBUG_ENTER("ndbcluster_binlog_end");

  if (ndbcluster_binlog_inited)
  {
    ndbcluster_binlog_inited= 0;

    ndb_binlog_thread.stop();
    ndb_binlog_thread.deinit();

    mysql_mutex_destroy(&injector_event_mutex);
    mysql_mutex_destroy(&injector_data_mutex);
    mysql_cond_destroy(&injector_data_cond);
  }

  DBUG_RETURN(0);
}

/*****************************************************************
  functions called from slave sql client threads
****************************************************************/
static void ndbcluster_reset_slave(THD *thd)
{
  if (!ndb_binlog_running)
    return;

  DBUG_ENTER("ndbcluster_reset_slave");

  /*
    delete all rows from mysql.ndb_apply_status table
    - if table does not exist ignore the error as it
      is a consistent behavior
  */
  if (opt_ndb_clear_apply_status)
  {
    Ndb_local_connection mysqld(thd);
    const bool ignore_no_such_table = true;
    if(mysqld.delete_rows(STRING_WITH_LEN("mysql"),
                          STRING_WITH_LEN("ndb_apply_status"),
                          ignore_no_such_table,
                          NULL))
    {
      // Failed to delete rows from table
    }
  }

  g_ndb_slave_state.atResetSlave();

  // pending fix for bug#59844 will make this function return int
  DBUG_VOID_RETURN;
}


static int ndbcluster_binlog_func(handlerton*, THD *thd,
                                  enum_binlog_func fn, 
                                  void *arg)
{
  DBUG_ENTER("ndbcluster_binlog_func");
  int res= 0;
  switch(fn)
  {
  case BFN_RESET_LOGS:
    break;
  case BFN_RESET_SLAVE:
    ndbcluster_reset_slave(thd);
    break;
  case BFN_BINLOG_WAIT:
    ndbcluster_binlog_wait(thd);
    break;
  case BFN_BINLOG_END:
    res= ndbcluster_binlog_end();
    break;
  case BFN_BINLOG_PURGE_FILE:
    res= ndbcluster_binlog_index_purge_file(thd, (const char *)arg);
    break;
  }
  DBUG_RETURN(res);
}

void ndbcluster_binlog_init(handlerton* h)
{
  h->binlog_func=      ndbcluster_binlog_func;
  h->binlog_log_query= ndbcluster_binlog_log_query;
}


/*
   ndb_notify_tables_writable
   
   Called to notify any waiting threads that Ndb tables are
   now writable
*/ 
static void ndb_notify_tables_writable()
{
  mysql_mutex_lock(&ndbcluster_mutex);
  ndb_setup_complete= 1;
  mysql_cond_broadcast(&ndbcluster_cond);
  mysql_mutex_unlock(&ndbcluster_mutex);
}


static bool
migrate_table_with_old_extra_metadata(THD *thd, Ndb *ndb,
                                      const char *schema_name,
                                      const char *table_name,
                                      void* unpacked_data,
                                      Uint32 unpacked_len,
                                      bool force_overwrite)
{
#ifndef BUG27543602
  // Skip installation of the ndb_index_stat* tables as a
  // temporary workaround for Bug 27543602. They can still
  // be accessed from ndb tools
  if (strcmp(NDB_REP_DB, schema_name) == 0 &&
      (strcmp("ndb_index_stat_head", table_name) == 0 ||
       strcmp("ndb_index_stat_sample", table_name) == 0))
  {
    return true;
  }
#endif

  // Migrate tables that have old metadata to data dictionary
  // using on the fly translation
  ndb_log_info("Table '%s.%s' has obsolete extra metadata. "
               "The table is installed into the data dictionary "
               "by translating the old metadata", schema_name,
               table_name);

  const uchar* frm_data = static_cast<const uchar*>(unpacked_data);

  // Install table in DD
  Ndb_dd_client dd_client(thd);

  // First acquire exclusive MDL lock on schema and table
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name))
  {
    ndb_log_error("Failed to acquire MDL lock on table '%s.%s'",
                  schema_name, table_name);
    return false;
  }

  const bool migrate_result=
             dd_client.migrate_table(schema_name, table_name, frm_data,
                                     unpacked_len, force_overwrite);

  if (!migrate_result)
  {
    // Failed to create DD entry for table
    ndb_log_error("Failed to create entry in DD for table '%s.%s'",
                  schema_name, table_name);
    return false;
  }

  // Check if table need to be setup for binlogging or
  // schema distribution
  const dd::Table* table_def;

  // Acquire MDL lock on table
  if (!dd_client.mdl_lock_table(schema_name, table_name))
  {
    ndb_log_error("Failed to acquire MDL lock for table '%s.%s'",
                  schema_name, table_name);
    return false;
  }

  if (!dd_client.get_table(schema_name, table_name, &table_def))
  {
    ndb_log_error("Failed to open table '%s.%s' from DD",
                  schema_name, table_name);
    return false;
  }

  if (ndbcluster_binlog_setup_table(thd, ndb,
                                    schema_name, table_name,
                                    table_def) != 0)
  {
    ndb_log_error("Failed to setup binlog for table '%s.%s'",
                  schema_name, table_name);
    return false;
  }

  return true;
}


static int
ndb_create_table_from_engine(THD *thd,
                             const char *schema_name,
                             const char *table_name,
                             bool force_overwrite = false)
{
  DBUG_ENTER("ndb_create_table_from_engine");
  DBUG_PRINT("enter", ("schema_name: %s, table_name: %s",
                       schema_name, table_name));

  Thd_ndb* thd_ndb = get_thd_ndb(thd);
  Ndb* ndb = thd_ndb->ndb;
  NDBDICT* dict = ndb->getDictionary();

  if (ndb->setDatabaseName(schema_name))
  {
    DBUG_PRINT("error", ("Failed to set database name of Ndb object"));
    DBUG_RETURN(false);
  }

  Ndb_table_guard ndbtab_g(dict, table_name);
  const NDBTAB *tab= ndbtab_g.get_table();
  if (!tab)
  {
    // Could not open the table from NDB
    const NdbError err= dict->getNdbError();
    if (err.code == 709 || err.code == 723)
    {
      // Got the normal 'No such table existed'
      DBUG_PRINT("info", ("No such table, error: %u", err.code));
      DBUG_RETURN(709);
    }

    // Got an unexpected error
    DBUG_PRINT("error", ("Got unexpected error when trying to open table "
                         "from NDB, error %u", err.code));
    DBUG_ASSERT(false); // Catch in debug
    DBUG_RETURN(1);
  }

  DBUG_PRINT("info", ("Found table '%s'", table_name));

  dd::sdi_t sdi;
  {
    Uint32 version;
    void* unpacked_data;
    Uint32 unpacked_len;
    const int get_result =
        tab->getExtraMetadata(version,
                              &unpacked_data, &unpacked_len);
    if (get_result != 0)
    {
      DBUG_PRINT("error", ("Could not get extra metadata, error: %d",
                           get_result));
      DBUG_RETURN(10);
    }

    if (version == 1)
    {
      const bool migrate_result=
          migrate_table_with_old_extra_metadata(thd, ndb,
                                                schema_name,
                                                table_name,
                                                unpacked_data,
                                                unpacked_len,
                                                force_overwrite);

      if (!migrate_result)
      {
        free(unpacked_data);
        DBUG_PRINT("error", ("Failed to create entry in DD for table '%s.%s' "
                             , schema_name, table_name));
        DBUG_RETURN(11);
      }

      free(unpacked_data);
      DBUG_RETURN(0);
    }

    sdi.assign(static_cast<const char*>(unpacked_data), unpacked_len);

    free(unpacked_data);
  }


  // Found table, now install it in DD
  Ndb_dd_client dd_client(thd);

  // First acquire exclusive MDL lock on schema and table
  if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name))
  {
    DBUG_RETURN(12);
  }

  if (!dd_client.install_table(schema_name, table_name,
                               sdi,
                               tab->getObjectId(), tab->getObjectVersion(),
                               force_overwrite))
  {
    DBUG_RETURN(13);
  }

  const dd::Table* table_def;
  if (!dd_client.get_table(schema_name, table_name, &table_def))
  {
    DBUG_RETURN(14);
  }

  // Check if binlogging should be setup for this table
  if (ndbcluster_binlog_setup_table(thd, ndb,
                                    schema_name, table_name,
                                    table_def))
  {
    DBUG_RETURN(37);
  }

  dd_client.commit();

  DBUG_RETURN(0);
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

  THD* const m_thd;
  Thd_ndb* const m_thd_ndb;

  /*
    NDB has no representation of the database schema objects, but
    the mysql.ndb_schema table contains the latest schema operations
    done via a mysqld, and thus reflects databases created/dropped/altered.
    This function tries to restore the correct state w.r.t created databases
    using the information in that table.
  */
  static
  int find_all_databases(THD *thd, Thd_ndb* thd_ndb)
  {
    Ndb *ndb= thd_ndb->ndb;
    NDBDICT *dict= ndb->getDictionary();
    NdbTransaction *trans= NULL;
    NdbError ndb_error;
    int retries= 100;
    int retry_sleep= 30; /* 30 milliseconds, transaction */
    DBUG_ENTER("Ndb_binlog_setup::find_all_databases");

    /*
      Function should only be called while ndbcluster_global_schema_lock
      is held, to ensure that ndb_schema table is not being updated while
      scanning.
    */
    if (!thd_ndb->has_required_global_schema_lock("Ndb_binlog_setup::find_all_databases"))
      DBUG_RETURN(1);

    ndb->setDatabaseName(NDB_REP_DB);

    Thd_ndb::Options_guard thd_ndb_options(thd_ndb);
    thd_ndb_options.set(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT);
    while (1)
    {
      char db_buffer[FN_REFLEN];
      char *db= db_buffer+1;
      char name[FN_REFLEN];
      char query[64000];
      Ndb_table_guard ndbtab_g(dict, NDB_SCHEMA_TABLE);
      const NDBTAB *ndbtab= ndbtab_g.get_table();
      NdbScanOperation *op;
      NdbBlob *query_blob_handle;
      int r= 0;
      if (ndbtab == NULL)
      {
        ndb_error= dict->getNdbError();
        goto error;
      }
      trans= ndb->startTransaction();
      if (trans == NULL)
      {
        ndb_error= ndb->getNdbError();
        goto error;
      }
      op= trans->getNdbScanOperation(ndbtab);
      if (op == NULL)
      {
        ndb_error= trans->getNdbError();
        goto error;
      }

      op->readTuples(NdbScanOperation::LM_Read,
                     NdbScanOperation::SF_TupScan, 1);

      r|= op->getValue("db", db_buffer) == NULL;
      r|= op->getValue("name", name) == NULL;
      r|= (query_blob_handle= op->getBlobHandle("query")) == NULL;
      r|= query_blob_handle->getValue(query, sizeof(query));

      if (r)
      {
        ndb_error= op->getNdbError();
        goto error;
      }

      if (trans->execute(NdbTransaction::NoCommit))
      {
        ndb_error= trans->getNdbError();
        goto error;
      }

      while ((r= op->nextResult()) == 0)
      {
        unsigned db_len= db_buffer[0];
        unsigned name_len= name[0];
        /*
          name_len == 0 means no table name, hence the row
          is for a database
        */
        if (db_len > 0 && name_len == 0)
        {
          /* database found */
          db[db_len]= 0;

          /* find query */
          Uint64 query_length= 0;
          if (query_blob_handle->getLength(query_length))
          {
            ndb_error= query_blob_handle->getNdbError();
            goto error;
          }
          query[query_length]= 0;
          build_table_filename(name, sizeof(name), db, "", "", 0);
          int database_exists= !my_access(name, F_OK);
          if (native_strncasecmp("CREATE", query, 6) == 0)
          {
            /* Database should exist */
            if (!database_exists)
            {
              /* create missing database */
              ndb_log_info("Discovered missing database '%s'", db);
              const int no_print_error[1]= {0};
              run_query(thd, query, query + query_length,
                        no_print_error);
            }
          }
          else if (native_strncasecmp("ALTER", query, 5) == 0)
          {
            /* Database should exist */
            if (!database_exists)
            {
              /* create missing database */
              ndb_log_info("Discovered missing database '%s'", db);
              const int no_print_error[1]= {0};
              name_len= (unsigned)snprintf(name, sizeof(name), "CREATE DATABASE %s", db);
              run_query(thd, name, name + name_len,
                        no_print_error);
              run_query(thd, query, query + query_length,
                        no_print_error);
            }
          }
          else if (native_strncasecmp("DROP", query, 4) == 0)
          {
            /* Database should not exist */
            if (database_exists)
            {
              /* drop missing database */
              ndb_log_info("Discovered remaining database '%s'", db);
            }
          }
        }
      }
      if (r == -1)
      {
        ndb_error= op->getNdbError();
        goto error;
      }
      ndb->closeTransaction(trans);
      trans= NULL;
      DBUG_RETURN(0); // success

    error:
      if (trans)
      {
        ndb->closeTransaction(trans);
        trans= NULL;
      }
      if (ndb_error.status == NdbError::TemporaryError && !thd->killed)
      {
        if (retries--)
        {
          ndb_log_warning("ndbcluster_find_all_databases retry: %u - %s",
                          ndb_error.code,
                          ndb_error.message);
          ndb_retry_sleep(retry_sleep);
          continue; // retry
        }
      }
      if (!thd->killed)
      {
        ndb_log_error("ndbcluster_find_all_databases fail: %u - %s",
                      ndb_error.code,
                      ndb_error.message);
      }

      DBUG_RETURN(1); // not temp error or too many retries
    }
  }


  bool
  get_ndb_table_names_in_schema(const char* schema_name,
                                std::unordered_set<std::string>* names)
  {
    Ndb* ndb = get_thd_ndb(m_thd)->ndb;
    NDBDICT* dict= ndb->getDictionary();

    NdbDictionary::Dictionary::List list;
    if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
      return false;

    for (uint i= 0 ; i < list.count ; i++)
    {
      NDBDICT::List::Element& elmt= list.elements[i];

      if (strcmp(schema_name, elmt.database) != 0)
      {
        DBUG_PRINT("info", ("Skipping %s.%s table, not in schema %s",
                            elmt.database, elmt.name, schema_name));
        continue;
      }

      if (ndb_name_is_temp(elmt.name) ||
          ndb_name_is_blob_prefix(elmt.name))
      {
        DBUG_PRINT("info", ("Skipping %s.%s in NDB", elmt.database, elmt.name));
        continue;
      }

      DBUG_PRINT("info", ("Found %s.%s in NDB", elmt.database, elmt.name));
      if (elmt.state != NDBOBJ::StateOnline &&
          elmt.state != NDBOBJ::StateBackup &&
          elmt.state != NDBOBJ::StateBuilding)
      {
        ndb_log_info("Skipping setup of table '%s.%s', in state %d",
                     elmt.database, elmt.name, elmt.state);
        continue;
      }

      names->insert(elmt.name);
    }

    return true;
  }


  bool
  remove_table_from_dd(const char* schema_name,
                       const char* table_name)
  {
    Ndb_dd_client dd_client(m_thd);

    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name))
    {
      return false;
    }

    if (!dd_client.remove_table(schema_name, table_name))
    {
      return false;
    }

    dd_client.commit();

    return true; // OK
  }

  bool
  remove_deleted_ndb_tables_from_dd()
  {
    Ndb_dd_client dd_client(m_thd);

    // Fetch list of schemas in DD
    std::vector<std::string> schema_names;
    if (!dd_client.fetch_schema_names(&schema_names))
    {
      ndb_log_verbose(19,
                      "Failed to remove deleted NDB tables, could not "
                      "fetch schema names");
      return false;
    }

    // Iterate over each schema and remove deleted NDB tables
    // from the DD one by one
    for (const auto name : schema_names)
    {
      const char* schema_name= name.c_str();
      // Lock the schema in DD
      if (!dd_client.mdl_lock_schema(schema_name))
      {
        ndb_log_info("Failed to MDL lock schema");
        return false;
      }

      // Fetch list of NDB tables in DD, also acquire MDL lock on
      // table names
      std::unordered_set<std::string> ndb_tables_in_DD;
      if (!dd_client.get_ndb_table_names_in_schema(schema_name,
                                                   &ndb_tables_in_DD))
      {
        ndb_log_info("Failed to get list of NDB tables in DD");
        return false;
      }

      // Fetch list of NDB tables in NDB
      std::unordered_set<std::string> ndb_tables_in_NDB;
      if (!get_ndb_table_names_in_schema(schema_name, &ndb_tables_in_NDB))
      {
        ndb_log_info("Failed to get list of NDB tables in NDB");
        return false;
      }

      // Iterate over all NDB tables found in DD. If they
      // don't exist in NDB anymore, then remove the table
      // from DD

      for (const auto ndb_table_name : ndb_tables_in_DD)
      {
        if (ndb_tables_in_NDB.count(ndb_table_name) == 0)
        {
          ndb_log_info("Removing table '%s.%s'",
                       schema_name, ndb_table_name.c_str());
          remove_table_from_dd(schema_name, ndb_table_name.c_str());
        }
      }
    }

    return true;

  }


  bool
  install_table_from_NDB(THD *thd,
                         const char *schema_name,
                         const char *table_name,
                         const NdbDictionary::Table* ndbtab,
                         bool force_overwrite = false)
  {
    DBUG_ENTER("install_table_from_NDB");
    DBUG_PRINT("enter", ("schema_name: %s, table_name: %s",
                         schema_name, table_name));

    Thd_ndb* thd_ndb = get_thd_ndb(thd);
    Ndb* ndb = thd_ndb->ndb;

    dd::sdi_t sdi;
    {
      Uint32 version;
      void* unpacked_data;
      Uint32 unpacked_len;
      const int get_result =
          ndbtab->getExtraMetadata(version,
                                   &unpacked_data, &unpacked_len);
      if (get_result != 0)
      {
        DBUG_PRINT("error", ("Could not get extra metadata, error: %d",
                             get_result));
        DBUG_RETURN(false);
      }

      if (version != 1 && version != 2)
      {
        // Skip install of table which has unsupported extra metadata
        // versions
        ndb_log_info("Skipping setup of table '%s.%s', it has "
                     "unsupported extra metadata version %d.",
                     schema_name, table_name, version);
        return true; // Skipped
      }

      if (version == 1)
      {
        const bool migrate_result=
                   migrate_table_with_old_extra_metadata(thd, ndb,
                                                         schema_name,
                                                         table_name,
                                                         unpacked_data,
                                                         unpacked_len,
                                                         force_overwrite);

        if (!migrate_result)
        {
          free(unpacked_data);
          DBUG_RETURN(false);
        }

        free(unpacked_data);
        DBUG_RETURN(true);
      }

      sdi.assign(static_cast<const char*>(unpacked_data), unpacked_len);

      free(unpacked_data);
    }

    // Found table, now install it in DD
    Ndb_dd_client dd_client(thd);

    // First acquire exclusive MDL lock on schema and table
    if (!dd_client.mdl_locks_acquire_exclusive(schema_name, table_name))
    {
      ndb_log_error("Couldn't acquire exclusive metadata locks on '%s.%s'",
                    schema_name, table_name);
      DBUG_RETURN(false);
    }

    if (!dd_client.install_table(schema_name, table_name,
                                 sdi,
                                 ndbtab->getObjectId(),
                                 ndbtab->getObjectVersion(),
                                 force_overwrite))
    {
      // Failed to install table
      ndb_log_warning("Failed to install table '%s.%s'",
                      schema_name, table_name);
      DBUG_RETURN(false);
    }

    const dd::Table* table_def;
    if (!dd_client.get_table(schema_name, table_name, &table_def))
    {
      ndb_log_error("Couldn't open table '%s.%s' from DD after install",
                    schema_name, table_name);
      DBUG_RETURN(false);
    }

    // Check if binlogging should be setup for this table
    if (ndbcluster_binlog_setup_table(thd, ndb,
                                      schema_name, table_name,
                                      table_def))
    {
      DBUG_RETURN(false);
    }

    dd_client.commit();

    DBUG_RETURN(true); // OK
  }


  bool
  synchronize_table(const char* schema_name,
                    const char* table_name)
  {


    Ndb* ndb = get_thd_ndb(m_thd)->ndb;
    NDBDICT* dict= ndb->getDictionary();

    ndb_log_verbose(1,
                    "    Synchronizing table '%s.%s'",
                    schema_name, table_name);

    ndb->setDatabaseName(schema_name);
    Ndb_table_guard ndbtab_g(dict, table_name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    if (!ndbtab)
    {
      // Failed to open the table from NDB
      ndb_log_error("Failed to setup table '%s.%s', error: %d, %s",
                    schema_name, table_name,
                    dict->getNdbError().code,
                    dict->getNdbError().message);

       // Failed, table was listed but could not be opened, retry
      return false;
    }

    if (ndbtab->getFrmLength() == 0)
    {
      ndb_log_verbose(1,
                      "Skipping setup of table '%s.%s', no extra "
                      "metadata", schema_name, table_name);
      return true; // Ok, table skipped
    }

    {
      Uint32 version;
      void* unpacked_data;
      Uint32 unpacked_length;
      const int get_result =
          ndbtab->getExtraMetadata(version,
                                   &unpacked_data, &unpacked_length);

      if (get_result != 0)
      {
        // Header corrupt or failed to unpack
        ndb_log_error("Failed to setup table '%s.%s', could not "
                      "unpack extra metadata, error: %d",
                      schema_name, table_name, get_result);
        return false;
      }

      free(unpacked_data);
    }

    Ndb_dd_client dd_client(m_thd);

    // Acquire MDL lock on table
    if (!dd_client.mdl_lock_table(schema_name, table_name))
    {
      ndb_log_error("Failed to acquire MDL lock for table '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    const dd::Table* existing;
    if (!dd_client.get_table(schema_name, table_name, &existing))
    {
      ndb_log_error("Failed to open table '%s.%s' from DD",
                    schema_name, table_name);
      return false;
    }

    if (existing == nullptr)
    {
      ndb_log_info("Table '%s.%s' does not exist in DD, installing...",
                   schema_name, table_name);

      if (!install_table_from_NDB(m_thd, schema_name, table_name,
                                  ndbtab, false /* need overwrite */))
      {
        // Failed to install into DD or setup binlogging
        ndb_log_error("Failed to install table '%s.%s'",
                      schema_name, table_name);
        return false;
      }
      return true; // OK
    }

    // Skip if table exists in DD, but is in other engine
    const dd::String_type engine = ndb_dd_table_get_engine(existing);
    if (engine != "ndbcluster")
    {
      ndb_log_info("Skipping table '%s.%s' with same name which is in "
                   "engine '%s'",
                   schema_name, table_name,
                   engine.c_str());
      return true; // Skipped
    }

    int table_id, table_version;
    if (!ndb_dd_table_get_object_id_and_version(existing,
                                                table_id, table_version))
    {
      //
      ndb_log_error("Failed to extract id and version from table definition "
                    "for table '%s.%s'", schema_name, table_name);
      DBUG_ASSERT(false);
      return false;
    }

    // Check that latest version of table definition for this NDB table
    // is installed in DD
    if (ndbtab->getObjectId() != table_id ||
        ndbtab->getObjectVersion() != table_version)
    {
      ndb_log_info("Table '%s.%s' have different version in DD, reinstalling...",
                     schema_name, table_name);
      if (!install_table_from_NDB(m_thd, schema_name, table_name,
                                  ndbtab, true /* need overwrite */))
      {
        // Failed to create table from NDB
        ndb_log_error("Failed to install table '%s.%s' from NDB",
                      schema_name, table_name);
        return false;
      }
    }

    // Check if table need to be setup for binlogging or
    // schema distribution
    const dd::Table* table_def;
    if (!dd_client.get_table(schema_name, table_name, &table_def))
    {
      ndb_log_error("Failed to open table '%s.%s' from DD",
                    schema_name, table_name);
      return false;
    }

    if (ndbcluster_binlog_setup_table(m_thd, ndb,
                                      schema_name, table_name,
                                      table_def) != 0)
    {
      ndb_log_error("Failed to setup binlog for table '%s.%s'",
                    schema_name, table_name);
      return false;
    }

    return true;
  }


  bool
  synchronize_schema(const char* schema_name)
  {
    Ndb_dd_client dd_client(m_thd);

    ndb_log_info("  Synchronizing schema '%s'", schema_name);

    // Lock the schema in DD
    if (!dd_client.mdl_lock_schema(schema_name))
    {
      ndb_log_info("Failed to MDL lock schema");
      return false;
    }

    // Fetch list of NDB tables in NDB
    std::unordered_set<std::string> ndb_tables_in_NDB;
    if (!get_ndb_table_names_in_schema(schema_name, &ndb_tables_in_NDB))
    {
      ndb_log_info("Failed to get list of NDB tables in NDB");
      return false;
    }

    // Iterate over each table in NDB and synchronize them to DD
    for (const auto ndb_table_name : ndb_tables_in_NDB)
    {
      if (!synchronize_table(schema_name, ndb_table_name.c_str()))
      {
        ndb_log_info("Failed to synchronize table '%s.%s'",
                      schema_name, ndb_table_name.c_str());
        continue;
      }
    }

    return true;
  }


  bool
  synchronize_dd(void)
  {
    // Current assumption is that databases already has been
    // synched by 'find_all_databases"

    Ndb_dd_client dd_client(m_thd);

    ndb_log_info("Starting metadata synchronization...");

    // Fetch list of schemas in DD
    std::vector<std::string> schema_names;
    if (!dd_client.fetch_schema_names(&schema_names))
    {
      ndb_log_verbose(19,
                      "Failed to synchronize metadata, could not "
                      "fetch schema names");
      return false;
    }

    // Iterate over each schema and synchronize it one by one,
    // the assumption is that even large deployments have
    // a manageable number of tables in each schema
    for (const auto name : schema_names)
    {
      if (!synchronize_schema(name.c_str()))
      {
        ndb_log_info("Failed to synchronize metadata, schema: '%s'",
                     name.c_str());
        return false;
      }
    }

    // NOTE! While upgrading MySQL Server from version
    // without DD the synchronize code should loop through and
    // remove files that ndbcluster used to put in the data directory
    // (like .ndb and .frm files). Such files would otherwise prevent
    // for example DROP DATABASE to drop the actual data directory.
    // This point where it's known that the DD is in synch with
    // NDB's dictionary would be a good place to do that removal of
    // old files from the data directory.

    ndb_log_info("Completed metadata synchronization");

    return true;
  }

  static bool
  create_cluster_sys_table(THD *thd, const char* db, size_t db_length,
                           const char* table, size_t table_length,
                           const char* create_definitions,
                           const char* create_options)
  {
    /* Need a connection to create table, else retry later. */
    if (g_ndb_cluster_connection->get_no_ready() <= 0)
      return true;

    /*
      Check if table exists in MySQL DD and in such case remove
      it from DD since there is none in NDB.
    */

    Ndb_dd_client dd_client(thd);

    if (!dd_client.mdl_locks_acquire_exclusive(db, table))
    {
      ndb_log_info("Failed to MDL lock '%s.%s'", db, table);
      return true; // failed
    }

    const dd::Table* existing;
    if (dd_client.get_table(db, table, &existing))
    {
      ndb_log_verbose(1, "Removing '%s.%s' from DD", db, table);

      if (!dd_client.remove_table(db, table))
      {
        ndb_log_info("Failed to remove '%s.%s' from DD", db, table);
      }

      dd_client.commit();

      /*
        The table existed in and was deleted from DD. It's possible
        that someone has tried to use it and thus it might have been
        inserted in the table definition cache. Close the table
        in the table definition cace(tdc).
      */
      ndb_log_verbose(1, "Removing table '%s.%s'' from table definition cache",
                      db, table);

      ndb_tdc_close_cached_table(thd, db, table);
    }

    Ndb_local_connection mysqld(thd);

    const bool create_if_not_exists = true;
    const bool res = mysqld.create_sys_table(db, db_length,
                                             table, table_length,
                                             create_if_not_exists,
                                             create_definitions,
                                             create_options);
    return res;
  }


  static bool
  ndb_apply_table__create(THD *thd)
  {
    DBUG_ENTER("ndb_apply_table__create");

    /* NOTE! Updating this table schema must be reflected in ndb_restore */
    const bool res =
      create_cluster_sys_table(thd,
                               STRING_WITH_LEN("mysql"),
                               STRING_WITH_LEN("ndb_apply_status"),
                               // table_definition
                               "server_id INT UNSIGNED NOT NULL,"
                               "epoch BIGINT UNSIGNED NOT NULL, "
                               "log_name VARCHAR(255) BINARY NOT NULL, "
                               "start_pos BIGINT UNSIGNED NOT NULL, "
                               "end_pos BIGINT UNSIGNED NOT NULL, "
                               "PRIMARY KEY USING HASH (server_id)",
                               // table_options
                               "ENGINE=NDB CHARACTER SET latin1");
    DBUG_RETURN(res);
  }


  static bool
  ndb_schema_table__create(THD *thd)
  {
    DBUG_ENTER("ndb_schema_table__create");

    /* NOTE! Updating this table schema must be reflected in ndb_restore */
    const bool res =
      create_cluster_sys_table(thd,
                               STRING_WITH_LEN("mysql"),
                               STRING_WITH_LEN("ndb_schema"),
                               // table_definition
                               "db VARBINARY("
                               NDB_MAX_DDL_NAME_BYTESIZE_STR
                               ") NOT NULL,"
                               "name VARBINARY("
                               NDB_MAX_DDL_NAME_BYTESIZE_STR
                               ") NOT NULL,"
                               "slock BINARY(32) NOT NULL,"
                               "query BLOB NOT NULL,"
                               "node_id INT UNSIGNED NOT NULL,"
                               "epoch BIGINT UNSIGNED NOT NULL,"
                               "id INT UNSIGNED NOT NULL,"
                               "version INT UNSIGNED NOT NULL,"
                               "type INT UNSIGNED NOT NULL,"
                               "PRIMARY KEY USING HASH (db,name)",
                               // table_options
                               "ENGINE=NDB CHARACTER SET latin1");
    DBUG_RETURN(res);
  }

  Ndb_binlog_setup(const Ndb_binlog_setup&); // Not copyable
  Ndb_binlog_setup operator=(const Ndb_binlog_setup&); // Not assignable

public:

  Ndb_binlog_setup(THD* thd) :
    m_thd(thd),
    m_thd_ndb(get_thd_ndb(thd))
  {
    // Ndb* object in Thd_ndb should've been assigned
    assert(m_thd_ndb->ndb);
  }

  bool
  setup(void)
  {
    /* Test binlog_setup on this mysqld being slower (than other mysqld) */
    if (DBUG_EVALUATE_IF("ndb_binlog_setup_slow", true, false))
    {
      ndb_log_info("'ndb_binlog_setup_slow' -> sleep");
      ndb_milli_sleep(10*1000); // seconds * 1000
      ndb_log_info(" <- sleep");
    }

    while (true) //To allow 'break' out to error handling
    {
      DBUG_ASSERT(ndb_schema_share == NULL);
      DBUG_ASSERT(ndb_apply_status_share == NULL);

      /**
        The Global Schema Lock (GSL) protects the discovery of the tables,
        and creation of the schema change distribution event (ndb_schema_share)
        to be atomic. This make sure that the schema does not change without
        being distributed to other mysqld's.
      */
      Ndb_global_schema_lock_guard global_schema_lock_guard(m_thd);
      if (global_schema_lock_guard.lock())
      {
        break;
      }

      // Remove deleted NDB tables
      if (!remove_deleted_ndb_tables_from_dd())
      {
        break;
      }

      /* Give additional 'binlog_setup rights' to this Thd_ndb */
      Thd_ndb::Options_guard thd_ndb_options(m_thd_ndb);
      thd_ndb_options.set(Thd_ndb::ALLOW_BINLOG_SETUP);

      if (ndb_create_table_from_engine(m_thd,
                                       NDB_REP_DB, NDB_SCHEMA_TABLE,
                                       true))
      {
        if (ndb_schema_table__create(m_thd))
          break;
      }
      if (ndb_schema_share == NULL)  //Needed for 'ndb_schema_dist_is_ready()'
      {
        ndb_log_verbose(50, "Schema distribution setup failed");
        break;
      }

      /**
        NOTE: At this point the creation of 'ndb_schema_share' has set
        ndb_schema_dist_is_ready(), which also announced our subscription
        (and handling) of schema change events.
        We are excpected to act on any such changes (SLOCK) by all other mysqld.
        However, this is not possible until setup has succesfully
        completed, and our binlog-thread started to handle events.
        Thus, if we fail to complete the setup below, the schema changes *must*
        be unsubscribed as part of error handling. Any other mysqld's waiting
        for us to reply, will then get an unsibscribe-event instead, which breaks
        the wait.
       */
       assert(ndb_schema_dist_is_ready());

       DBUG_EXECUTE_IF("ndb_binlog_setup_incomplete",
       {
         // Test handling of binlog_setup failing to complete *after*
         // created 'ndb_schema'
         ndb_log_info("'ndb_binlog_setup_incomplete' -> return");
         // NOTE! This break has no effect as it only breaks the while
         // in the DBUG_EXECUTE_IF macro
         break;
       });

       if (ndb_create_table_from_engine(m_thd,
                                        NDB_REP_DB, NDB_APPLY_TABLE,
                                        true))
       {
         if (ndb_apply_table__create(m_thd))
           break;
       }
       /*
          NOTE! Failure to create APPLY_TABLE eventOp is retried
          by find_all_files(), and eventually failed.
       */

       if (find_all_databases(m_thd, m_thd_ndb))
         break;

       if (!synchronize_dd())
       {
         ndb_log_verbose(9, "Failed to synchronize DD with NDB");
         break;
       }

       /* Shares w/ eventOp subscr. for NDB_SCHEMA_TABLE and
          NDB_APPLY_TABLE created? */
       DBUG_ASSERT(ndb_schema_share);
       DBUG_ASSERT(!ndb_binlog_running || ndb_apply_status_share);

       Mutex_guard injector_mutex_g(injector_data_mutex);
       ndb_binlog_tables_inited= true;
       return true;     // Setup completed -> OK
    }

    /**
      Error handling:
      Failed to complete ndb_binlog_setup.
      Remove all existing event operations from a possible partial setup
    */
    if (ndb_schema_dist_is_ready()) // Can't leave failed setup with 'dist_is_ready'
    {
      ndb_log_info("Clean up leftovers");
      remove_all_event_operations(schema_ndb, injector_ndb);
    }

    /* There should not be a partial setup left behind */
    DBUG_ASSERT(!ndb_schema_dist_is_ready());
    return false;
  }

}; // class Ndb_binlog_setup


static bool
ndb_binlog_setup(THD *thd)
{
  Ndb_binlog_setup binlog_setup(thd);
  return binlog_setup.setup();
}


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
constexpr uint SCHEMA_SLOCK_SIZE = 32;


static void ndb_report_waiting(const char *key,
                               int the_time,
                               const char *op,
                               const char *obj,
                               const MY_BITMAP * map)
{
  ulonglong ndb_latest_epoch= 0;
  const char *proc_info= "<no info>";
  mysql_mutex_lock(&injector_event_mutex);
  if (injector_ndb)
    ndb_latest_epoch= injector_ndb->getLatestGCI();
  if (injector_thd)
    proc_info= injector_thd->proc_info;
  mysql_mutex_unlock(&injector_event_mutex);
  if (map == 0)
  {
    ndb_log_info("%s, waiting max %u sec for %s %s."
                 "  epochs: (%u/%u,%u/%u,%u/%u)"
                 "  injector proc_info: %s",
                 key, the_time, op, obj,
                 (uint)(ndb_latest_handled_binlog_epoch >> 32),
                 (uint)(ndb_latest_handled_binlog_epoch),
                 (uint)(ndb_latest_received_binlog_epoch >> 32),
                 (uint)(ndb_latest_received_binlog_epoch),
                 (uint)(ndb_latest_epoch >> 32),
                 (uint)(ndb_latest_epoch),
                 proc_info);
  }
  else
  {
    ndb_log_info("%s, waiting max %u sec for %s %s."
                 "  epochs: (%u/%u,%u/%u,%u/%u)"
                 "  injector proc_info: %s map: %x%08x",
                 key, the_time, op, obj,
                 (uint)(ndb_latest_handled_binlog_epoch >> 32),
                 (uint)(ndb_latest_handled_binlog_epoch),
                 (uint)(ndb_latest_received_binlog_epoch >> 32),
                 (uint)(ndb_latest_received_binlog_epoch),
                 (uint)(ndb_latest_epoch >> 32),
                 (uint)(ndb_latest_epoch),
                 proc_info, map->bitmap[1], map->bitmap[0]);
  }
}


/*
  log query in ndb_schema table
*/

int Ndb_schema_dist_client::log_schema_op_impl(
    Ndb* ndb,
    const char *query, int query_length, const char *db, const char *table_name,
    uint32 ndb_table_id, uint32 ndb_table_version, SCHEMA_OP_TYPE type,
    bool log_query_on_participant)
{
  DBUG_ENTER("Ndb_schema_dist_client::log_schema_op_impl");

  DBUG_PRINT("enter", ("query: %s  db: %s  table_name: %s",
                       query, db, table_name));
  if (!ndb_schema_share)
  {
    DBUG_RETURN(0);
  }

  /* Check that the database name will fit within limits */
  if(strlen(db) > NDB_MAX_DDL_NAME_BYTESIZE)
  {
    // Catch unexpected commands with too long db length
    DBUG_ASSERT(type == SOT_CREATE_DB ||
                type == SOT_ALTER_DB ||
                type == SOT_DROP_DB);
    push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                        ER_TOO_LONG_IDENT,
                        "Ndb has an internal limit of %u bytes on the size of schema identifiers",
                        NDB_MAX_DDL_NAME_BYTESIZE);
    DBUG_RETURN(ER_TOO_LONG_IDENT);
  }


  NDB_SCHEMA_OBJECT *ndb_schema_object;
  {
    ndb_schema_object= ndb_get_schema_object(db, table_name, true);

    /**
     * We will either get a newly created schema_object, or a 
     * 'all-clear' schema_object completed but still referred
     * by my binlog-injector-thread. In both cases there should
     * be no outstanding SLOCK's.
     * See also the 'ndb_binlog_schema_object_race' error injection.
     */ 
    DBUG_ASSERT(bitmap_is_clear_all(&ndb_schema_object->slock_bitmap));

    /**
     * Expect answer from all other nodes by default(those
     * who are not subscribed will be filtered away by
     * the Coordinator which keep track of such stuff)
     */
    bitmap_set_all(&ndb_schema_object->slock_bitmap);

    ndb_schema_object->table_id= ndb_table_id;
    ndb_schema_object->table_version= ndb_table_version;

    if (DBUG_EVALUATE_IF("ndb_binlog_random_tableid", true, false))
    {
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
  }

  {
    /* begin protect ndb_schema_share */
    Mutex_guard ndb_schema_share_g(injector_data_mutex);
    if (ndb_schema_share == NULL)
    {
      ndb_free_schema_object(&ndb_schema_object);
      DBUG_RETURN(0);
    }
  }

  char save_db[FN_REFLEN];
  strcpy(save_db, ndb->getDatabaseName());

  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(NDB_REP_DB);
  Ndb_table_guard ndbtab_g(dict, NDB_SCHEMA_TABLE);
  const NDBTAB *ndbtab= ndbtab_g.get_table();
  NdbTransaction *trans = nullptr;
  int retries= 100;

  const NdbError *ndb_error = nullptr;
  if (ndbtab == nullptr)
  {
    ndb_error= &dict->getNdbError();
    goto end;
  }

  while (1)
  {
    char tmp_buf[FN_REFLEN];
    const Uint64 log_epoch = 0;
    const uint32 log_type= (uint32)type;
    const char *log_db= db;
    const char *log_tab= table_name;
    const char *log_subscribers= (char*)ndb_schema_object->slock;
    // Use nodeid of the primary cluster connection since that is
    // the nodeid which the coordinator and participants listen to
    const uint32 log_node_id = g_ndb_cluster_connection->node_id();

    if ((trans= ndb->startTransaction()) == 0)
      goto err;

    {
      NdbOperation *op= nullptr;
      int r= 0;
      r|= (op= trans->getNdbOperation(ndbtab)) == nullptr;
      DBUG_ASSERT(r == 0);
      r|= op->writeTuple();
      DBUG_ASSERT(r == 0);
      
      /* db */
      ndb_pack_varchar(ndbtab, SCHEMA_DB_I, tmp_buf, log_db,
                       strlen(log_db));
      r|= op->equal(SCHEMA_DB_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* name */
      ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, tmp_buf, log_tab,
                       strlen(log_tab));
      r|= op->equal(SCHEMA_NAME_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* slock */
      DBUG_ASSERT(ndbtab->getColumn(SCHEMA_SLOCK_I)->getLength() ==
                  (int)no_bytes_in_map(&ndb_schema_object->slock_bitmap));
      r|= op->setValue(SCHEMA_SLOCK_I, log_subscribers);
      DBUG_ASSERT(r == 0);
      /* query */
      {
        NdbBlob *ndb_blob= op->getBlobHandle(SCHEMA_QUERY_I);
        DBUG_ASSERT(ndb_blob != nullptr);
        uint blob_len= query_length;
        const char* blob_ptr= query;
        r|= ndb_blob->setValue(blob_ptr, blob_len);
        DBUG_ASSERT(r == 0);
      }
      /* node_id */
      r|= op->setValue(SCHEMA_NODE_ID_I, log_node_id);
      DBUG_ASSERT(r == 0);
      /* epoch */
      r|= op->setValue(SCHEMA_EPOCH_I, log_epoch);
      DBUG_ASSERT(r == 0);
      /* id */
      r|= op->setValue(SCHEMA_ID_I, ndb_table_id);
      DBUG_ASSERT(r == 0);
      /* version */
      r|= op->setValue(SCHEMA_VERSION_I, ndb_table_version);
      DBUG_ASSERT(r == 0);
      /* type */
      r|= op->setValue(SCHEMA_TYPE_I, log_type);
      DBUG_ASSERT(r == 0);
      /* any value */
      Uint32 anyValue = 0;
      if (! m_thd->slave_thread)
      {
        /* Schema change originating from this MySQLD, check SQL_LOG_BIN
         * variable and pass 'setting' to all logging MySQLDs via AnyValue  
         */
        if (thd_test_options(m_thd, OPTION_BIN_LOG)) /* e.g. SQL_LOG_BIN == on */
        {
          DBUG_PRINT("info", ("Schema event for binlogging"));
          ndbcluster_anyvalue_set_normal(anyValue);
        }
        else
        {
          DBUG_PRINT("info", ("Schema event not for binlogging")); 
          ndbcluster_anyvalue_set_nologging(anyValue);
        }

        if(!log_query_on_participant)
        {
          DBUG_PRINT("info", ("Forcing query not to be binlogged on participant"));
          ndbcluster_anyvalue_set_nologging(anyValue);
        }
      }
      else
      {
        /* 
           Slave propagating replicated schema event in ndb_schema
           In case replicated serverId is composite 
           (server-id-bits < 31) we copy it into the 
           AnyValue as-is
           This is for 'future', as currently Schema operations
           do not have composite AnyValues.
           In future it may be useful to support *not* mapping composite
           AnyValues to/from Binlogged server-ids.
        */
        DBUG_PRINT("info", ("Replicated schema event with original server id %d",
                            m_thd->server_id));
        anyValue = thd_unmasked_server_id(m_thd);
      }

#ifndef DBUG_OFF
      /*
        MySQLD will set the user-portion of AnyValue (if any) to all 1s
        This tests code filtering ServerIds on the value of server-id-bits.
      */
      const char* p = getenv("NDB_TEST_ANYVALUE_USERDATA");
      if (p != 0  && *p != 0 && *p != '0' && *p != 'n' && *p != 'N')
      {
        dbug_ndbcluster_anyvalue_set_userbits(anyValue);
      }
#endif  
      r|= op->setAnyValue(anyValue);
      DBUG_ASSERT(r == 0);
    }
    if (trans->execute(NdbTransaction::Commit, NdbOperation::DefaultAbortOption,
                       1 /* force send */) == 0)
    {
      DBUG_PRINT("info", ("logged: %s", query));
      dict->forceGCPWait(1);
      break;
    }
err:
    const NdbError *this_error= trans ?
      &trans->getNdbError() : &ndb->getNdbError();
    if (this_error->status == NdbError::TemporaryError && !m_thd->killed)
    {
      if (retries--)
      {
        if (trans)
          ndb->closeTransaction(trans);
        ndb_retry_sleep(30); /* milliseconds, transaction */
        continue; // retry
      }
    }
    ndb_error= this_error;
    break;
  }
end:
  if (ndb_error)
    push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG, ER_THD(m_thd, ER_GET_ERRMSG),
                        ndb_error->code,
                        ndb_error->message,
                        "Could not log query '%s' on other mysqld's");
          
  if (trans)
    ndb->closeTransaction(trans);
  ndb->setDatabaseName(save_db);

  ndb_log_verbose(19,
                  "distributed %s.%s(%u/%u) type: %s(%u) "
                  "query: \'%s\' to %x%08x",
                  db, table_name, ndb_table_id, ndb_table_version,
                  type_name(type), type, query,
                  ndb_schema_object->slock_bitmap.bitmap[1],
                  ndb_schema_object->slock_bitmap.bitmap[0]);

  /*
    Wait for other mysqld's to acknowledge the table operation
  */
  if (unlikely(ndb_error))
  {
    ndb_log_error("%s, distributing %s err: %u",
                  type_str(type), ndb_schema_object->key,
                  ndb_error->code);
  }
  else if (!bitmap_is_clear_all(&ndb_schema_object->slock_bitmap))
  {
    int max_timeout= DEFAULT_SYNC_TIMEOUT;
    mysql_mutex_lock(&ndb_schema_object->mutex);
    while (true)
    {
      struct timespec abstime;
      set_timespec(&abstime, 1);

      // Wait for operation on ndb_schema_object to complete.
      // Condition for completion is that 'slock_bitmap' is cleared,
      // which is signaled by ::handle_clear_slock() on
      // 'ndb_schema_object->cond'
      const int ret= mysql_cond_timedwait(&ndb_schema_object->cond,
                                          &ndb_schema_object->mutex,
                                          &abstime);

      if (m_thd->killed)
        break;

      { //Scope of ndb_schema_share protection
        Mutex_guard ndb_schema_share_g(injector_data_mutex);
        if (ndb_schema_share == NULL)
          break;
      }

      if (bitmap_is_clear_all(&ndb_schema_object->slock_bitmap))
        break; //Done, normal completion

      if (ret)
      {
        max_timeout--;
        if (max_timeout == 0)
        {
          ndb_log_error("%s, distributing %s timed out. Ignoring...",
                        type_str(type), ndb_schema_object->key);
          DBUG_ASSERT(false);
          break;
        }
        if (ndb_log_get_verbose_level())
          ndb_report_waiting(type_str(type), max_timeout,
                             "distributing", ndb_schema_object->key,
                             &ndb_schema_object->slock_bitmap);
      }
    }
    mysql_mutex_unlock(&ndb_schema_object->mutex);
  }
  else
  {
    ndb_log_verbose(19, "%s, not waiting for distributing %s",
                    type_str(type), ndb_schema_object->key);
  }

  ndb_free_schema_object(&ndb_schema_object);

  ndb_log_verbose(19,
                  "distribution of %s.%s(%u/%u) type: %s(%u) query: \'%s\'"
                  " - complete!",
                  db, table_name, ndb_table_id, ndb_table_version,
                  type_name(type), type, query);

  DBUG_RETURN(0);
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

static
void
ndbcluster_binlog_event_operation_teardown(THD *thd,
                                           Ndb *is_ndb,
                                           NdbEventOperation *pOp)
{
  DBUG_ENTER("ndbcluster_binlog_event_operation_teardown");
  DBUG_PRINT("enter", ("pOp: %p", pOp));

  // Should only called for TE_DROP and TE_CLUSTER_FAILURE event
  DBUG_ASSERT(pOp->getEventType() == NDBEVENT::TE_DROP ||
              pOp->getEventType() == NDBEVENT::TE_CLUSTER_FAILURE);

  // Get Ndb_event_data associated with the NdbEventOperation
  const Ndb_event_data* event_data=
    static_cast<const Ndb_event_data*>(pOp->getCustomData());
  DBUG_ASSERT(event_data);

  // Get NDB_SHARE associated with the Ndb_event_data, the share
  // is referenced by "binlog" and will not go away until released
  // further down in this function
  NDB_SHARE *share= event_data->share;

  // Invalidate any cached NdbApi table if object version is lower
  // than what was used when setting up the NdbEventOperation
  // NOTE! This functionality need to be explained further
  {
    Thd_ndb *thd_ndb= get_thd_ndb(thd);
    Ndb *ndb= thd_ndb->ndb;
    NDBDICT *dict= ndb->getDictionary();
    ndb->setDatabaseName(share->db);
    Ndb_table_guard ndbtab_g(dict, share->table_name);
    const NDBTAB *ev_tab= pOp->getTable();
    const NDBTAB *cache_tab= ndbtab_g.get_table();
    if (cache_tab &&
        cache_tab->getObjectId() == ev_tab->getObjectId() &&
        cache_tab->getObjectVersion() <= ev_tab->getObjectVersion())
      ndbtab_g.invalidate();
  }

  // Release the share of the NdbEventoperation
  mysql_mutex_lock(&share->mutex);
  DBUG_ASSERT(share->op == pOp);
  share->op= NULL;
  mysql_mutex_unlock(&share->mutex);

  /* Signal ha_ndbcluster::delete/rename_table that drop is done */
  DBUG_PRINT("info", ("signal that drop is done"));
  mysql_cond_broadcast(&injector_data_cond);

  // Close the table in MySQL Server
  ndb_tdc_close_cached_table(thd, share->db, share->table_name);

  // Release the "binlog" reference from NDB_SHARE
  mysql_mutex_lock(&ndbcluster_mutex);
  NDB_SHARE::release_reference_have_lock(share, "binlog");
  mysql_mutex_unlock(&ndbcluster_mutex);

  // Remove pointer to event_data from the EventOperation
  pOp->setCustomData(NULL);

  // Drop the NdbEventOperation from NdbApi
  DBUG_PRINT("info", ("Dropping event operation: %p", pOp));
  mysql_mutex_lock(&injector_event_mutex);
  is_ndb->dropEventOperation(pOp);
  mysql_mutex_unlock(&injector_event_mutex);

  // Finally delete the event_data and thus it's mem_root, shadow_table etc.
  Ndb_event_data::destroy(event_data);

  DBUG_VOID_RETURN;
}


/*
  Data used by the Ndb_schema_event_handler which lives
  as long as the NDB Binlog thread is connected to the cluster.

  NOTE! An Ndb_schema_event_handler instance only lives for one epoch

 */
class Ndb_schema_dist_data {
  static const uint max_ndb_nodes= 256; /* multiple of 32 */
  uchar m_data_node_id_list[max_ndb_nodes];
  /*
    The subscribers to ndb_schema are tracked separately for each
    data node. This avoids the need to know which data nodes are
    connected.
    An api counts as subscribed as soon as one of the data nodes
    report it as subscibed.
  */
  MY_BITMAP *subscriber_bitmap;
  unsigned m_num_bitmaps;

  // Holds the new key for a table to be renamed
  struct NDB_SHARE_KEY* m_prepared_rename_key;

  // Holds the Ndb_event_data which is created during inplace alter table
  // prepare and used during commit
  // NOTE! this place holder is only used for the participant in same node
  const class Ndb_event_data* m_inplace_alter_event_data{nullptr};
public:
  Ndb_schema_dist_data(const Ndb_schema_dist_data&); // Not implemented
  Ndb_schema_dist_data() :
    subscriber_bitmap(NULL),
    m_num_bitmaps(0),
    m_prepared_rename_key(NULL)
  {}

  void init(Ndb_cluster_connection* cluster_connection)
  {
    const uint own_nodeid = cluster_connection->node_id();

    // Initialize "g_node_id_map" which maps from nodeid to index in
    // subscriber bitmaps array. The mapping array is only used when
    // the NDB binlog thread handles events on the mysql.ndb_schema table
    uint node_id, i= 0;
    Ndb_cluster_connection_node_iter node_iter;
    memset((void *)m_data_node_id_list, 0xFFFF, sizeof(m_data_node_id_list));
    while ((node_id= cluster_connection->get_next_node(node_iter)))
      m_data_node_id_list[node_id]= i++;

    {
      // Create array of bitmaps for keeping track of subscribed nodes
      unsigned no_nodes= cluster_connection->no_db_nodes();
      subscriber_bitmap= (MY_BITMAP*)my_malloc(PSI_INSTRUMENT_ME,
                                               no_nodes * sizeof(MY_BITMAP),
                                               MYF(MY_WME));
      for (unsigned i= 0; i < no_nodes; i++)
      {
        bitmap_init(&subscriber_bitmap[i],
                    (Uint32*)my_malloc(PSI_INSTRUMENT_ME,
                                       max_ndb_nodes/8, MYF(MY_WME)),
                    max_ndb_nodes, false);
        DBUG_ASSERT(bitmap_is_clear_all(&subscriber_bitmap[i]));
        bitmap_set_bit(&subscriber_bitmap[i], own_nodeid); //'self' is always active
      }
      // Remember the number of bitmaps allocated
      m_num_bitmaps = no_nodes;
    }
  }

  void release(void)
  {
    if (!m_num_bitmaps)
    {
      // Allow release without init(), happens when binlog thread
      // is terminated before connection to cluster has been made
      // NOTE! Should be possible to use static memory for the arrays
      return;
    }

    for (unsigned i= 0; i < m_num_bitmaps; i++)
    {
      // Free memory allocated for the bitmap
      // allocated by my_malloc() and passed as "buf" to bitmap_init()
      bitmap_free(&subscriber_bitmap[i]);
    }
    // Free memory allocated for the bitmap array
    my_free(subscriber_bitmap);
    m_num_bitmaps = 0;

    // Release the prepared rename key, it's very unlikely
    // that the key is still around here, but just in case
    NDB_SHARE::free_key(m_prepared_rename_key);
    m_prepared_rename_key = NULL;

    // Release the event_data saved for inplace alter, it's very
    // unlikley that the event_data is still around, but just in case
    Ndb_event_data::destroy(m_inplace_alter_event_data);
    m_inplace_alter_event_data = nullptr;
  }

  void report_data_node_failure(unsigned data_node_id)
  {
    uint8 idx= map2subscriber_bitmap_index(data_node_id);
    bitmap_clear_all(&subscriber_bitmap[idx]);
    DBUG_PRINT("info",("Data node %u failure", data_node_id));

    ndb_log_verbose(1,
                    "Data node: %d failed, subscriber bitmask %x%08x",
                    data_node_id,
                    subscriber_bitmap[idx].bitmap[1],
                    subscriber_bitmap[idx].bitmap[0]);

    check_wakeup_clients();
  }

  void report_subscribe(unsigned data_node_id, unsigned subscriber_node_id)
  {
    uint8 idx= map2subscriber_bitmap_index(data_node_id);
    DBUG_ASSERT(subscriber_node_id != 0);
    bitmap_set_bit(&subscriber_bitmap[idx], subscriber_node_id);
    DBUG_PRINT("info",("Data node %u reported node %u subscribed ",
                       data_node_id, subscriber_node_id));
    ndb_log_verbose(1,
                    "Data node: %d reports subscribe from node %d, "
                    "subscriber bitmask %x%08x",
                    data_node_id, subscriber_node_id,
                    subscriber_bitmap[idx].bitmap[1],
                    subscriber_bitmap[idx].bitmap[0]);

    //No 'wakeup_clients' now, as *adding* subscribers didn't complete anything
  }

  void report_unsubscribe(unsigned data_node_id, unsigned subscriber_node_id)
  {
    uint8 idx= map2subscriber_bitmap_index(data_node_id);
    DBUG_ASSERT(subscriber_node_id != 0);
    bitmap_clear_bit(&subscriber_bitmap[idx], subscriber_node_id);
    DBUG_PRINT("info",("Data node %u reported node %u unsubscribed ",
                       data_node_id, subscriber_node_id));
    ndb_log_verbose(1,
                    "Data node: %d reports unsubscribe from node %d, "
                    "subscriber bitmask %x%08x",
                    data_node_id, subscriber_node_id,
                    subscriber_bitmap[idx].bitmap[1],
                    subscriber_bitmap[idx].bitmap[0]);

    check_wakeup_clients();
  }

  void get_subscriber_bitmask(MY_BITMAP* servers) const
  {
    for (unsigned i= 0; i < m_num_bitmaps; i++)
    {
      bitmap_union(servers, &subscriber_bitmap[i]);
    }
  }

  void save_prepared_rename_key(NDB_SHARE_KEY* key)
  {
    m_prepared_rename_key = key;
  }

  NDB_SHARE_KEY* get_prepared_rename_key() const
  {
    return m_prepared_rename_key;
  }

  void save_inplace_alter_event_data(const Ndb_event_data* event_data)
  {
    // Should not already be set when saving a new pointer
    DBUG_ASSERT(event_data == nullptr ||
                !m_inplace_alter_event_data);
    m_inplace_alter_event_data = event_data;
  }
  const Ndb_event_data* get_inplace_alter_event_data() const
  {
    return m_inplace_alter_event_data;
  }

private:

  // Map from nodeid to position in subscriber bitmaps array
  uint8 map2subscriber_bitmap_index(uint data_node_id) const
  {
    DBUG_ASSERT(data_node_id <
                (sizeof(m_data_node_id_list)/sizeof(m_data_node_id_list[0])));
    const uint8 bitmap_index = m_data_node_id_list[data_node_id];
    DBUG_ASSERT(bitmap_index != 0xFF);
    DBUG_ASSERT(bitmap_index < m_num_bitmaps);
    return bitmap_index;
  }

  void check_wakeup_clients() const
  {
    // Build bitmask of current participants
    uint32 participants_buf[256/32];
    MY_BITMAP participants;
    bitmap_init(&participants, participants_buf, 256, false);
    get_subscriber_bitmask(&participants);

    // Check all Client's for wakeup
    NDB_SCHEMA_OBJECT::check_waiters(participants);
  }

}; //class Ndb_schema_dist_data


#include "sql/ndb_local_schema.h"

class Ndb_schema_event_handler {

  class Ndb_schema_op
  {
    // Unpack Ndb_schema_op from event_data pointer
    void unpack_event(const Ndb_event_data *event_data)
    {
      TABLE *table= event_data->shadow_table;
      Field **field;
      /* unpack blob values */
      uchar* blobs_buffer= 0;
      uint blobs_buffer_size= 0;
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
      {
        ptrdiff_t ptrdiff= 0;
        int ret= get_ndb_blobs_value(table, event_data->ndb_value[0],
                                     blobs_buffer, blobs_buffer_size,
                                     ptrdiff);
        if (ret != 0)
        {
          my_free(blobs_buffer);
          DBUG_PRINT("info", ("blob read error"));
          DBUG_ASSERT(false);
        }
      }
      /* db varchar 1 length uchar */
      field= table->field;
      db_length= *(uint8*)(*field)->ptr;
      DBUG_ASSERT(db_length <= (*field)->field_length);
      DBUG_ASSERT((*field)->field_length + 1 == sizeof(db));
      memcpy(db, (*field)->ptr + 1, db_length);
      db[db_length]= 0;
      /* name varchar 1 length uchar */
      field++;
      name_length= *(uint8*)(*field)->ptr;
      DBUG_ASSERT(name_length <= (*field)->field_length);
      DBUG_ASSERT((*field)->field_length + 1 == sizeof(name));
      memcpy(name, (*field)->ptr + 1, name_length);
      name[name_length]= 0;
      /* slock fixed length */
      field++;
      slock_length= (*field)->field_length;
      DBUG_ASSERT((*field)->field_length == sizeof(slock_buf));
      memcpy(slock_buf, (*field)->ptr, slock_length);
      /* query blob */
      field++;
      {
        Field_blob *field_blob= (Field_blob*)(*field);
        uint blob_len= field_blob->get_length((*field)->ptr);
        uchar *blob_ptr= 0;
        field_blob->get_ptr(&blob_ptr);
        DBUG_ASSERT(blob_len == 0 || blob_ptr != 0);
        query_length= blob_len;
        query= sql_strmake((char*) blob_ptr, blob_len);
      }
      /* node_id */
      field++;
      node_id= (Uint32)((Field_long *)*field)->val_int();
      /* epoch */
      field++;
      epoch= ((Field_long *)*field)->val_int();
      /* id */
      field++;
      id= (Uint32)((Field_long *)*field)->val_int();
      /* version */
      field++;
      version= (Uint32)((Field_long *)*field)->val_int();
      /* type */
      field++;
      type= (Uint32)((Field_long *)*field)->val_int();
      /* free blobs buffer */
      my_free(blobs_buffer);
      dbug_tmp_restore_column_map(table->read_set, old_map);
    }

  public:
    uchar db_length;
    char db[64];
    uchar name_length;
    char name[64];
    uchar slock_length;
    uint32 slock_buf[SCHEMA_SLOCK_SIZE/4];
    MY_BITMAP slock;
    unsigned short query_length;
    char *query;
    Uint64 epoch;
    uint32 node_id;
    uint32 id;
    uint32 version;
    uint32 type;
    uint32 any_value;

    /**
      Create a Ndb_schema_op from event_data
    */
    static const Ndb_schema_op*
    create(const Ndb_event_data* event_data,
           Uint32 any_value)
    {
      DBUG_ENTER("Ndb_schema_op::create");
      Ndb_schema_op* schema_op=
        (Ndb_schema_op*)sql_alloc(sizeof(Ndb_schema_op));
      bitmap_init(&schema_op->slock,
                  schema_op->slock_buf, 8*SCHEMA_SLOCK_SIZE, false);
      schema_op->unpack_event(event_data);
      schema_op->any_value= any_value;
      DBUG_PRINT("exit", ("%s.%s: query: '%s'  type: %d",
                          schema_op->db, schema_op->name,
                          schema_op->query,
                          schema_op->type));
      DBUG_RETURN(schema_op);
    }
  }; //class Ndb_schema_op


  // NOTE! This function has misleading name
  static void
  print_could_not_discover_error(THD *thd,
                                 const Ndb_schema_op *schema)
  {
    ndb_log_error("NDB Binlog: Could not discover table '%s.%s' from "
                  "binlog schema event '%s' from node %d.",
                  schema->db, schema->name, schema->query,
                  schema->node_id);

    // Print thd's list of warnings to error log
    {
      Diagnostics_area::Sql_condition_iterator
          it(thd->get_stmt_da()->sql_conditions());

      const Sql_condition *err;
      while ((err= it++))
      {
        ndb_log_warning("NDB Binlog: (%d) %s",
                        err->mysql_errno(), err->message_text());
      }
    }
  }


  static void
  write_schema_op_to_binlog(THD *thd, const Ndb_schema_op *schema)
  {

    if (!ndb_binlog_running)
    {
      // This mysqld is not writing a binlog
      return;
    }

    /* any_value == 0 means local cluster sourced change that
     * should be logged
     */
    if (ndbcluster_anyvalue_is_reserved(schema->any_value))
    {
      /* Originating SQL node did not want this query logged */
      if (!ndbcluster_anyvalue_is_nologging(schema->any_value))
      {
        ndb_log_warning("unknown value for binlog signalling 0x%X, "
                        "query not logged", schema->any_value);
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

    if (queryServerId)
    {
      /*
         AnyValue has non-zero serverId, must be a query applied by a slave
         mysqld.
         TODO : Assert that we are running in the Binlog injector thread?
      */
      if (! g_ndb_log_slave_updates)
      {
        /* This MySQLD does not log slave updates */
        return;
      }
    }
    else
    {
      /* No ServerId associated with this query, mark it as ours */
      ndbcluster_anyvalue_set_serverid(loggedServerId, ::server_id);
    }

    /*
      Write the DDL query to binlog with server_id set
      to the server_id where the query originated.
    */
    const uint32 thd_server_id_save= thd->server_id;
    DBUG_ASSERT(sizeof(thd_server_id_save) == sizeof(thd->server_id));
    thd->server_id = loggedServerId;

    LEX_CSTRING thd_db_save= thd->db();
    LEX_CSTRING schema_db_lex_cstr= {schema->db, strlen(schema->db)};
    thd->reset_db(schema_db_lex_cstr);

    int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
    thd->binlog_query(THD::STMT_QUERY_TYPE,
                      schema->query, schema->query_length,
                      false, // is_trans
                      true, // direct
                      schema->name[0] == 0 || thd->db().str[0] == 0,
                      errcode);

    // Commit the binlog write
    (void)trans_commit_stmt(thd);

    /*
      Restore original server_id and db after commit
      since the server_id is being used also in the commit logic
    */
    thd->server_id= thd_server_id_save;
    thd->reset_db(thd_db_save);
  }


  /*
    Acknowledge handling of schema operation
    - Inform the other nodes that schema op has
      been completed by this node (by updating the
      row for this op in ndb_schema table)
  */
  int
  ack_schema_op(const Ndb_schema_op *schema) const
  {
    const char* const db = schema->db;
    const char* const table_name = schema->name;
    const uint32 table_id = schema->id;
    const uint32 table_version = schema->version;

    DBUG_ENTER("ack_schema_op");

    const NdbError *ndb_error= 0;
    Ndb *ndb= check_ndb_in_thd(m_thd);
    char save_db[FN_HEADLEN];
    strcpy(save_db, ndb->getDatabaseName());

    char tmp_buf[FN_REFLEN];
    NDBDICT *dict= ndb->getDictionary();
    ndb->setDatabaseName(NDB_REP_DB);
    Ndb_table_guard ndbtab_g(dict, NDB_SCHEMA_TABLE);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    NdbTransaction *trans= 0;
    int retries= 100;
    int retry_sleep= 30; /* 30 milliseconds, transaction */

    MY_BITMAP slock;
    uint32 bitbuf[SCHEMA_SLOCK_SIZE/4];
    bitmap_init(&slock, bitbuf, sizeof(bitbuf)*8, false);

    if (ndbtab == 0)
    {
      if (dict->getNdbError().code != 4009)
        abort();
      DBUG_RETURN(0);
    }

    while (1)
    {
      if ((trans= ndb->startTransaction()) == 0)
        goto err;
      {
        NdbOperation *op= 0;
        int r= 0;

        /* read the bitmap exlusive */
        r|= (op= trans->getNdbOperation(ndbtab)) == 0;
        DBUG_ASSERT(r == 0);
        r|= op->readTupleExclusive();
        DBUG_ASSERT(r == 0);

        /* db */
        ndb_pack_varchar(ndbtab, SCHEMA_DB_I, tmp_buf, db,
                         strlen(db));
        r|= op->equal(SCHEMA_DB_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* name */
        ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, tmp_buf,
                         table_name, strlen(table_name));
        r|= op->equal(SCHEMA_NAME_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* slock */
        r|= op->getValue(SCHEMA_SLOCK_I, (char*)slock.bitmap) == 0;
        DBUG_ASSERT(r == 0);
      }
      if (trans->execute(NdbTransaction::NoCommit))
        goto err;

      char before_slock[32];
      if (ndb_log_get_verbose_level() > 19)
      {
        /* Format 'before slock' into temp string */
        snprintf(before_slock, sizeof(before_slock), "%x%08x",
                    slock.bitmap[1], slock.bitmap[0]);
      }

      /**
       * The coordinator (only) knows the relative order of subscribe
       * events vs. other event ops. The subscribers known at the point
       * in time when it acks its own distrubution req, are the
       * participants in the schema distribution. Modify the initially
       * 'all_set' slock bitmap with the participating servers.
       */
      if (schema->node_id == own_nodeid())
      {
        // Build bitmask of subscribers known to Coordinator
        MY_BITMAP servers;
        uint32 bitbuf[SCHEMA_SLOCK_SIZE/4];
        bitmap_init(&servers, bitbuf, sizeof(bitbuf)*8, false);
        m_schema_dist_data.get_subscriber_bitmask(&servers);
        bitmap_intersect(&slock, &servers);
      }
      bitmap_clear_bit(&slock, own_nodeid());

      ndb_log_verbose(19, "reply to %s.%s(%u/%u) from %s to %x%08x",
                           db, table_name,
                           table_id, table_version,
                           before_slock,
                           slock.bitmap[1],
                           slock.bitmap[0]);

      {
        NdbOperation *op= 0;
        int r= 0;

        /* now update the tuple */
        r|= (op= trans->getNdbOperation(ndbtab)) == 0;
        DBUG_ASSERT(r == 0);
        r|= op->updateTuple();
        DBUG_ASSERT(r == 0);

        /* db */
        ndb_pack_varchar(ndbtab, SCHEMA_DB_I, tmp_buf, db,
                         strlen(db));
        r|= op->equal(SCHEMA_DB_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* name */
        ndb_pack_varchar(ndbtab, SCHEMA_NAME_I, tmp_buf,
                         table_name, strlen(table_name));
        r|= op->equal(SCHEMA_NAME_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* slock */
        r|= op->setValue(SCHEMA_SLOCK_I, (char*)slock.bitmap);
        DBUG_ASSERT(r == 0);
        /* node_id */
        r|= op->setValue(SCHEMA_NODE_ID_I, own_nodeid());
        DBUG_ASSERT(r == 0);
        /* type */
        r|= op->setValue(SCHEMA_TYPE_I, (uint32)SOT_CLEAR_SLOCK);
        DBUG_ASSERT(r == 0);
      }
      if (trans->execute(NdbTransaction::Commit,
                         NdbOperation::DefaultAbortOption, 1 /*force send*/) == 0)
      {
        DBUG_PRINT("info", ("node %d cleared lock on '%s.%s'",
                            own_nodeid(), db, table_name));
        dict->forceGCPWait(1);
        break;
      }
    err:
      const NdbError *this_error= trans ?
        &trans->getNdbError() : &ndb->getNdbError();
      if (this_error->status == NdbError::TemporaryError &&
          !thd_killed(m_thd))
      {
        if (retries--)
        {
          if (trans)
            ndb->closeTransaction(trans);
          ndb_retry_sleep(retry_sleep);
          continue; // retry
        }
      }
      ndb_error= this_error;
      break;
    }

    if (ndb_error)
    {
      ndb_log_warning("Could not release slock on '%s.%s', "
                      "Error code: %d Message: %s",
                      db, table_name,
                      ndb_error->code, ndb_error->message);
    }
    if (trans)
      ndb->closeTransaction(trans);
    ndb->setDatabaseName(save_db);
    DBUG_RETURN(0);
  }


  bool check_is_ndb_schema_event(const Ndb_event_data* event_data) const
  {
    if (!event_data)
    {
      // Received event without event data pointer
      assert(false);
      return false;
    }

    NDB_SHARE *share= event_data->share;
    if (!share)
    {
      // Received event where the event_data is not properly initialized
      assert(false);
      return false;
    }
    assert(event_data->shadow_table);
    assert(event_data->ndb_value[0]);
    assert(event_data->ndb_value[1]);

    Mutex_guard ndb_schema_share_g(injector_data_mutex);
    if (share != ndb_schema_share)
    {
      // Received event from s_ndb not pointing at the ndb_schema_share
      assert(false);
      return false;
    }
    assert(Ndb_schema_dist_client::is_schema_dist_table(share->db,
                                                        share->table_name));
    return true;
  }


  void
  handle_after_epoch(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_after_epoch");
    DBUG_PRINT("info", ("Pushing Ndb_schema_op on list to be "
                        "handled after epoch"));
    assert(!is_post_epoch()); // Only before epoch
    m_post_epoch_handle_list.push_back(schema, m_mem_root);
    DBUG_VOID_RETURN;
  }


  void
  ack_after_epoch(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("ack_after_epoch");
    assert(!is_post_epoch()); // Only before epoch
    m_post_epoch_ack_list.push_back(schema, m_mem_root);
    DBUG_VOID_RETURN;
  }


  uint own_nodeid(void) const
  {
    return m_own_nodeid;
  }


  void
  ndbapi_invalidate_table(const char* db_name, const char* table_name) const
  {
    DBUG_ENTER("ndbapi_invalidate_table");
    Thd_ndb *thd_ndb= get_thd_ndb(m_thd);
    Ndb *ndb= thd_ndb->ndb;

    ndb->setDatabaseName(db_name);
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), table_name);
    ndbtab_g.invalidate();
    DBUG_VOID_RETURN;
  }


  NDB_SHARE* acquire_reference(const char* db, const char* name,
                               const char* reference) const
  {
    DBUG_ENTER("acquire_reference");
    DBUG_PRINT("enter", ("db: '%s', name: '%s'", db, name));

    char key[FN_REFLEN + 1];
    build_table_filename(key, sizeof(key) - 1,
                         db, name, "", 0);
    NDB_SHARE *share=
        NDB_SHARE::acquire_reference_by_key(key,
                                            reference);
    DBUG_RETURN(share);
  }


  void handle_clear_slock(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_clear_slock");

    assert(is_post_epoch());

    // Try to create a race between SLOCK acks handled after another
    // schema operation could have been started.
    if (DBUG_EVALUATE_IF("ndb_binlog_random_tableid", true, false))
    {
      NDB_SCHEMA_OBJECT *p =
          ndb_get_schema_object(schema->db, schema->name, false);
      if (p == NULL)
      {
        ndb_milli_sleep(10);
      }
      else
      {
        ndb_free_schema_object(&p);
      }
    }

    /* Ack to any SQL thread waiting for schema op to complete */
    NDB_SCHEMA_OBJECT *ndb_schema_object =
        ndb_get_schema_object(schema->db, schema->name, false);
    if (!ndb_schema_object)
    {
      /* Noone waiting for this schema op in this mysqld */
      ndb_log_verbose(19, "Discarding event...no obj: '%s.%s' (%u/%u)",
                      schema->db, schema->name, schema->id, schema->version);
      DBUG_VOID_RETURN;
    }

    if (ndb_schema_object->table_id != schema->id ||
        ndb_schema_object->table_version != schema->version)
    {
      /* Someone waiting, but for another id/version... */
      ndb_log_verbose(19, "Discarding event...key: '%s.%s' "
                          "non matching id/version [%u/%u] != [%u/%u]",
                          schema->db, schema->name,
                          ndb_schema_object->table_id,
                          ndb_schema_object->table_version,
                          schema->id,
                          schema->version);
      ndb_free_schema_object(&ndb_schema_object);
      DBUG_VOID_RETURN;
    }

    mysql_mutex_lock(&ndb_schema_object->mutex);
    DBUG_DUMP("ndb_schema_object->slock_bitmap.bitmap",
              (uchar*)ndb_schema_object->slock_bitmap.bitmap,
              no_bytes_in_map(&ndb_schema_object->slock_bitmap));

    char before_slock[32];
    if (ndb_log_get_verbose_level() > 19)
    {
      /* Format 'before slock' into temp string */
      snprintf(before_slock, sizeof(before_slock), "%x%08x",
                  ndb_schema_object->slock[1],
                  ndb_schema_object->slock[0]);
    }

    /**
     * Remove any ack'ed schema-slocks. slock_bitmap is initially 'all-set'.
     * 'schema->slock' replied from any participant will have cleared its
     * own slock-bit. The Coordinator reply will in addition clear all bits
     * for servers not participating in the schema distribution.
     */
    bitmap_intersect(&ndb_schema_object->slock_bitmap, &schema->slock);

    /* Print updated slock together with before image of it */
    ndb_log_verbose(19, "CLEAR_SLOCK key: '%s.%s' (%u/%u) %x%08x, from "
                        "%s to %x%08x",
                        schema->db, schema->name, schema->id, schema->version,
                        schema->slock_buf[1], schema->slock_buf[0],
                        before_slock,
                        ndb_schema_object->slock[1],
                        ndb_schema_object->slock[0]);

    DBUG_DUMP("ndb_schema_object->slock_bitmap.bitmap",
              (uchar*)ndb_schema_object->slock_bitmap.bitmap,
              no_bytes_in_map(&ndb_schema_object->slock_bitmap));

    /* Wake up the waiter */
    mysql_mutex_unlock(&ndb_schema_object->mutex);
    mysql_cond_signal(&ndb_schema_object->cond);

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
    if (DBUG_EVALUATE_IF("ndb_binlog_schema_object_race", true, false))
    {
      ndb_milli_sleep(10);
    }
    ndb_free_schema_object(&ndb_schema_object);
    DBUG_VOID_RETURN;
  }


  void
  handle_offline_alter_table_commit(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_offline_alter_table_commit");

    assert(is_post_epoch()); // Always after epoch

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);
    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);

    NDB_SHARE *share=
        acquire_reference(schema->db, schema->name,
                          "offline_alter_table_commit");  // Temp ref.
    if (share)
    {
      mysql_mutex_lock(&share->mutex);
      if (share->op)
      {
        const Ndb_event_data *event_data=
          static_cast<const Ndb_event_data*>(share->op->getCustomData());
        Ndb_event_data::destroy(event_data);
        share->op->setCustomData(NULL);
        {
          Mutex_guard injector_mutex_g(injector_event_mutex);
          injector_ndb->dropEventOperation(share->op);
        }
        share->op= 0;
        NDB_SHARE::release_reference(share, "binlog");
      }
      mysql_mutex_unlock(&share->mutex);

      mysql_mutex_lock(&ndbcluster_mutex);
      NDB_SHARE::mark_share_dropped(&share);
      NDB_SHARE::release_reference_have_lock(share,
                                             "offline_alter_table_commit");
      /**
       * If this was the last share ref, it is now deleted.
       * If there are more references, the share will remain in the
       * list of dropped until remaining references are released.
       */
      mysql_mutex_unlock(&ndbcluster_mutex);
    } // if (share)

    bool exists_in_DD;
    Ndb_local_schema::Table tab(m_thd, schema->db, schema->name);
    if (tab.is_local_table(&exists_in_DD) &&
       !Ndb_dist_priv_util::is_distributed_priv_table(schema->db,
                                                      schema->name))
    {
      ndb_log_error("NDB Binlog: Skipping locally defined table '%s.%s' "
                    "from binlog schema event '%s' from node %d.",
                    schema->db, schema->name, schema->query,
                    schema->node_id);
      DBUG_VOID_RETURN;
    }

    // Install table from NDB, overwrite the existing table
    if (ndb_create_table_from_engine(m_thd,
                                     schema->db, schema->name,
                                     true /* force_overwrite */))
    {
      // NOTE! The below function has a rather misleading name of
      // actual functionality which failed
      print_could_not_discover_error(m_thd, schema);
    }
    DBUG_VOID_RETURN;
  }


  void
  handle_online_alter_table_prepare(const Ndb_schema_op* schema)
  {
    assert(is_post_epoch()); // Always after epoch

    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);

    if (schema->node_id == own_nodeid())
    {
      // Special case for schema dist participant in own node!
      // The schema dist client has exclusive MDL lock and thus
      // the schema dist participant(this code) on the same mysqld
      // can't open the table def from the DD, trying to acquire
      // another MDL lock will just block. Instead(since this is in
      // the same mysqld) it provides the new table def via a
      // pointer in the NDB_SHARE.
      NDB_SHARE *share=
          acquire_reference(schema->db, schema->name,
                            "online_alter_table_prepare"); // temporary ref.

      const dd::Table* new_table_def =
          static_cast<const dd::Table*>(share->inplace_alter_new_table_def);
      DBUG_ASSERT(new_table_def);


      // Create a new Ndb_event_data which will be used when creating
      // the new NdbEventOperation
      Ndb_event_data* event_data =
          Ndb_event_data::create_event_data(m_thd, share,
                                            share->db, share->table_name,
                                            share->key_string(), injector_thd,
                                            new_table_def);
      if (!event_data)
      {
        ndb_log_error("NDB Binlog: Failed to create event data for table %s.%s",
                      schema->db, schema->name);
        DBUG_ASSERT(false);
        // NOTE! Should abort the alter from here
      }

      // Release old prepared event_data, this is rare but will happen
      // when an inplace alter table fails between prepare and commit phase
      const Ndb_event_data* old_event_data =
          m_schema_dist_data.get_inplace_alter_event_data();
      if (old_event_data)
      {
        Ndb_event_data::destroy(old_event_data);
        m_schema_dist_data.save_inplace_alter_event_data(nullptr);
      }

      // Save the new event_data
      m_schema_dist_data.save_inplace_alter_event_data(event_data);

      NDB_SHARE::release_reference(share,
                                   "online_alter_table_prepare"); // temp ref.
    }
    else
    {
      write_schema_op_to_binlog(m_thd, schema);

      bool exists_in_DD;
      Ndb_local_schema::Table tab(m_thd, schema->db, schema->name);
      if (!tab.is_local_table(&exists_in_DD))
      {
        // Install table from NDB, overwrite the altered table.
        // NOTE! it will also try to setup binlogging but since the share
        // has a op assigned, that part will be skipped
        if (ndb_create_table_from_engine(m_thd,
                                         schema->db, schema->name,
                                         true /* force_overwrite */))
        {
          // NOTE! The below function has a rather misleading name of
          // actual functionality which failed
          print_could_not_discover_error(m_thd, schema);
        }
      }

      // Check that no event_data have been prepared yet(that is only
      // done on participant in same node)
      DBUG_ASSERT(m_schema_dist_data.get_inplace_alter_event_data() == nullptr);
    }
  }


  const Ndb_event_data*
  remote_participant_inplace_alter_create_event_data(NDB_SHARE *share) const
  {
    DBUG_ENTER("remote_participant_inplace_alter_create_event_data");

    // Read the table definition from DD
    Ndb_dd_client dd_client(m_thd);
    if (!dd_client.mdl_lock_table(share->db, share->table_name))
    {
      ndb_log_error("NDB Binlog: Failed to acquire MDL lock for table '%s.%s'",
                    share->db, share->table_name);
      DBUG_RETURN(nullptr);
    }

    const dd::Table* table_def;
    if (!dd_client.get_table(share->db, share->table_name, &table_def))
    {
      ndb_log_error("NDB Binlog: Failed to read table '%s.%s' from DD",
                    share->db, share->table_name);
      DBUG_RETURN(nullptr);
    }

    // Create new event_data
    Ndb_event_data* event_data =
        Ndb_event_data::create_event_data(m_thd, share,
                                          share->db, share->table_name,
                                          share->key_string(), injector_thd,
                                          table_def);
    if (!event_data)
    {
      ndb_log_error("NDB Binlog: Failed to create event data for table '%s.%s'",
                    share->db, share->table_name);
      DBUG_RETURN(nullptr);
    }

    DBUG_RETURN(event_data);
  }


  void
  handle_online_alter_table_commit(const Ndb_schema_op* schema)
  {
    assert(is_post_epoch()); // Always after epoch

    NDB_SHARE *share=
        acquire_reference(schema->db, schema->name,
                          "online_alter_table_commit"); // temporary ref.
    if (share)
    {
      ndb_log_verbose(9, "NDB Binlog: handling online alter/rename");

      mysql_mutex_lock(&share->mutex);

      const Ndb_event_data* event_data;
      if (schema->node_id == own_nodeid())
      {
        // Get the event_data which has been created during prepare phase
        event_data =
            m_schema_dist_data.get_inplace_alter_event_data();
        if (!event_data)
        {
          ndb_log_error("Failed to get prepared event data '%s'",
                        share->key_string());
          DBUG_ASSERT(false);
        }
        // The event_data pointer has been taken over
        m_schema_dist_data.save_inplace_alter_event_data(nullptr);
      }
      else
      {
        // Create Ndb_event_data which will be used when creating
        // the new NdbEventOperation.
        event_data =
            remote_participant_inplace_alter_create_event_data(share);
        if (!event_data)
        {
          ndb_log_error("Failed to create event data for table '%s'",
                        share->key_string());
          DBUG_ASSERT(false);
        }
      }
      DBUG_ASSERT(event_data);

      NdbEventOperation* new_op = nullptr;
      if (share->op && event_data /* safety */)
      {
        Ndb_binlog_client binlog_client(m_thd, schema->db, schema->name);
        // The table have an event operation setup and during an inplace
        // alter table that need to be recrated for the new table layout.
        // NOTE! Nothing has changed here regarding wheter or not the
        // table should still have event operation, i.e if it had
        // it before, it should still have it after the alter. But
        // for consistency, check that table should have event op
        DBUG_ASSERT(binlog_client.table_should_have_event_op(share));

        // Save the current event operation since create_event_op()
        // will assign the new in "share->op", also release the "binlog"
        // reference as it will be acquired again in create_event_op()
        // NOTE! This should probably be rewritten to not assign share->op and
        // acquire the reference in create_event_op()
        NdbEventOperation * const curr_op= share->op;
        share->op= nullptr;
        NDB_SHARE::release_reference(share, "binlog");

        // Get table from NDB
        Thd_ndb *thd_ndb= get_thd_ndb(m_thd);
        Ndb *ndb= thd_ndb->ndb;
        ndb->setDatabaseName(schema->db);
        Ndb_table_guard ndbtab_g(ndb->getDictionary(), schema->name);
        const NDBTAB *ndbtab= ndbtab_g.get_table();

        // Create new NdbEventOperation
        if (binlog_client.create_event_op(share, ndbtab, event_data))
        {
          ndb_log_error("Failed to create event operation for table '%s'",
                        share->key_string());

          // NOTE! Should fail the alter here
          DBUG_ASSERT(false);
        }
        else
        {
          // Get the newly created NdbEventOperation, will be swapped
          // into place (again) later
          new_op= share->op;
        }

        // Reinstall the current NdbEventOperation
        share->op= curr_op;
      }
      else
      {
        // New event_data was created(that's the default) but the table didn't
        // have event operations and thus the event_data is unused, free it
        Ndb_event_data::destroy(event_data);
      }

      ndb_log_verbose(9, "NDB Binlog: handling online alter/rename done");

      // There should be no event_data left in m_schema_dist_data at this point
      DBUG_ASSERT(m_schema_dist_data.get_inplace_alter_event_data() == nullptr);

      // Start using the new event operation and release the old
      if (share->op && new_op)
      {
        // Delete old event_data
        const Ndb_event_data *event_data=
          static_cast<const Ndb_event_data*>(share->op->getCustomData());
        share->op->setCustomData(NULL);
        Ndb_event_data::destroy(event_data);

        // Drop old event operation
        {
          Mutex_guard injector_mutex_g(injector_event_mutex);
          injector_ndb->dropEventOperation(share->op);
        }
        // Install new event operation
        share->op= new_op;
      }
      mysql_mutex_unlock(&share->mutex);

      NDB_SHARE::release_reference(share,
                                   "online_alter_table_commit"); // temp ref.
    }

    DBUG_ASSERT(m_schema_dist_data.get_inplace_alter_event_data() == nullptr);
  }


  void
  handle_drop_table(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_drop_table");

    assert(is_post_epoch()); // Always after epoch

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    bool exists_in_DD;
    Ndb_local_schema::Table tab(m_thd, schema->db, schema->name);
    if (tab.is_local_table(&exists_in_DD))
    {
      /* Table is not a NDB table in this mysqld -> leave it */
      ndb_log_warning("NDB Binlog: Skipping drop of locally "
                      "defined table '%s.%s' from binlog schema "
                      "event '%s' from node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);

      // There should be no NDB_SHARE for this table
      assert(!acquire_reference(schema->db, schema->name, "drop_table"));

      DBUG_VOID_RETURN;
    }

    if (exists_in_DD)
    {
      // The table exists in DD on this Server, remove it
      tab.remove_table();
    }
    else
    {
      // The table didn't exist in DD, no need to remove but still
      // continue to invalidate the table in NdbApi, close cached tables
      // etc. This case may happen when a MySQL Server drops a "shadow"
      // table and afterwards someone drops also the table with same name
      // in NDB
      // NOTE! Probably could check after a drop of "shadow" table if a
      // table with same name exists in NDB
     ndb_log_info("NDB Binlog: Ignoring drop of table '%s.%s' since it "
                  "doesn't exist in DD", schema->db, schema->name);
    }

    NDB_SHARE *share= acquire_reference(schema->db, schema->name,
                                        "drop_table"); // temporary ref.
    if (!share || !share->op)
    {
      ndbapi_invalidate_table(schema->db, schema->name);
      ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
    }
    if (share)
    {
      mysql_mutex_lock(&ndbcluster_mutex);
      NDB_SHARE::mark_share_dropped(&share); // server ref.
      DBUG_ASSERT(share);                    // Should still be ref'ed
      NDB_SHARE::release_reference_have_lock(share, "drop_table"); // temporary ref.
      mysql_mutex_unlock(&ndbcluster_mutex);
    }

    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);

    DBUG_VOID_RETURN;
  }


  /*
    The RENAME is performed in two steps.
    1) PREPARE_RENAME - sends the new table key to participants
    2) RENAME - perform the actual rename
  */

  void
  handle_rename_table_prepare(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_rename_table_prepare");

    assert(is_post_epoch()); // Always after epoch

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    const char* new_key_for_table= schema->query;
    DBUG_PRINT("info", ("new_key_for_table: '%s'", new_key_for_table));

    // Release potentially previously prepared new_key
    {
      NDB_SHARE_KEY* old_prepared_key =
          m_schema_dist_data.get_prepared_rename_key();
      if (old_prepared_key)
        NDB_SHARE::free_key(old_prepared_key);
    }

    // Create a new key save it, then hope for the best(i.e
    // that it can be found later when the RENAME arrives)
    NDB_SHARE_KEY* new_prepared_key =
        NDB_SHARE::create_key(new_key_for_table);
    m_schema_dist_data.save_prepared_rename_key(new_prepared_key);

    DBUG_VOID_RETURN;
  }


  bool
  get_table_version_from_NDB(const char* db_name, const char* table_name,
                             int* table_id, int* table_version)
  {
    DBUG_ENTER("get_table_version_from_NDB");
    DBUG_PRINT("enter", ("db_name: %s, table_name: %s",
                         db_name, table_name));

    Thd_ndb* thd_ndb = get_thd_ndb(m_thd);
    Ndb* ndb = thd_ndb->ndb;
    ndb->setDatabaseName(db_name);
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), table_name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    if (!ndbtab)
    {
      // Could not open table
      DBUG_RETURN(false);
    }

    *table_id = ndbtab->getObjectId();
    *table_version = ndbtab->getObjectVersion();

    DBUG_PRINT("info", ("table_id: %d, table_version: %d",
                        *table_id, *table_version));
    DBUG_RETURN(true);
  }


  void
  handle_rename_table(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_rename_table");

    assert(is_post_epoch()); // Always after epoch

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    bool exists_in_DD;
    Ndb_local_schema::Table from(m_thd, schema->db, schema->name);
    if (from.is_local_table(&exists_in_DD))
    {
      /* Tables exists as a local table, print warning and leave it */
      ndb_log_warning("NDB Binlog: Skipping rename of locally "
                      "defined table '%s.%s' from binlog schema "
                      "event '%s' from node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    NDB_SHARE *share= acquire_reference(schema->db, schema->name,
                                        "rename_table");  // temporary ref.
    if (!share || !share->op)
    {
      ndbapi_invalidate_table(schema->db, schema->name);
      ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
    }
    if (share)
      NDB_SHARE::release_reference(share, "rename_table"); // temporary ref.

    share= acquire_reference(schema->db, schema->name,
                             "rename_table"); // temporary ref.
    if (!share)
    {
      // The RENAME need to find share so it can be renamed
      DBUG_ASSERT(share);
      DBUG_VOID_RETURN;
    }

    NDB_SHARE_KEY* prepared_key =
        m_schema_dist_data.get_prepared_rename_key();
    if (!prepared_key)
    {
      // The rename need to have new_key set
      // by a previous RENAME_PREPARE
      DBUG_ASSERT(prepared_key);
      DBUG_VOID_RETURN;
    }

    // Rename on participant is always from real to
    // real name(i.e neiher old or new name should be a temporary name)
    DBUG_ASSERT(!ndb_name_is_temp(schema->name));
    DBUG_ASSERT(!ndb_name_is_temp(NDB_SHARE::key_get_table_name(prepared_key)));

    // Get the renamed tables id and new version from NDB
    // NOTE! It would be better if these parameters was passed in the
    // schema dist protocol. Both the id and version are used as the "key"
    // when communicating but that's the original table id and version
    // and not the new
    int ndb_table_id, ndb_table_version;
    if (!get_table_version_from_NDB(NDB_SHARE::key_get_db_name(prepared_key),
                                    NDB_SHARE::key_get_table_name(prepared_key),
                                    &ndb_table_id, &ndb_table_version))
    {
      // It was not possible to open the table from NDB
      DBUG_ASSERT(false);
      DBUG_VOID_RETURN;
    }

    // Rename the local table
    from.rename_table(NDB_SHARE::key_get_db_name(prepared_key),
                      NDB_SHARE::key_get_table_name(prepared_key),
                      ndb_table_id, ndb_table_version);

    // Rename share and release the old key
    NDB_SHARE_KEY* old_key = share->key;
    NDB_SHARE::rename_share(share, prepared_key);
    m_schema_dist_data.save_prepared_rename_key(NULL);
    NDB_SHARE::free_key(old_key);

    NDB_SHARE::release_reference(share, "rename_table"); // temporary ref.

    ndbapi_invalidate_table(schema->db, schema->name);
    ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);

    DBUG_VOID_RETURN;
  }


  void
  handle_drop_db(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_drop_db");

    assert(is_post_epoch()); // Always after epoch

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    Ndb_dd_client dd_client(m_thd);

    // Lock the schema in DD
    if (!dd_client.mdl_lock_schema(schema->db))
    {
      DBUG_PRINT("info", ("Failed to acquire MDL for db '%s'", schema->db));
      // Failed to lock the DD, skip dropping the database
      DBUG_VOID_RETURN;
    }

    bool schema_exists;
    if (!dd_client.schema_exists(schema->db, &schema_exists))
    {
        DBUG_PRINT("info", ("Failed to determine if schema '%s' exists",
                            schema->db));
        // Failed to check if schema existed, skip dropping the database
        DBUG_VOID_RETURN;
    }

    if (!schema_exists)
    {
      DBUG_PRINT("info", ("Schema '%s' does not exist",
                          schema->db));
      // Nothing to do
      DBUG_VOID_RETURN;
    }

    // Remove all NDB tables in the dropped database from DD,
    // this function is only called when they all have been dropped
    // from NDB by another MySQL Server
    //
    // NOTE! This is code which always run "in the server" so it would be
    // appropriate to log error messages to the server log file describing
    // any problems which occur in these functions.
    std::unordered_set<std::string> ndb_tables_in_DD;
    if (!dd_client.get_ndb_table_names_in_schema(schema->db, &ndb_tables_in_DD))
    {
      DBUG_PRINT("info", ("Failed to get list of NDB table in schema '%s'",
                          schema->db));
      DBUG_VOID_RETURN;
    }

    for (const auto ndb_table_name : ndb_tables_in_DD)
    {
      if (!dd_client.mdl_locks_acquire_exclusive(schema->db,
                                                 ndb_table_name.c_str()))
      {
        DBUG_PRINT("error", ("Failed to acquire exclusive MDL on '%s.%s'",
                             schema->db, ndb_table_name.c_str()));
        DBUG_ASSERT(false);
        continue;
      }

      if (!dd_client.remove_table(schema->db, ndb_table_name.c_str()))
      {
        // Failed to remove the table from DD, not much else to do
        // than try with the next
        DBUG_PRINT("error", ("Failed to remove table '%s.%s' from DD",
                             schema->db, ndb_table_name.c_str()));
        DBUG_ASSERT(false);
        continue;
      }

      NDB_SHARE *share=
          acquire_reference(schema->db, ndb_table_name.c_str(),
                            "drop_db"); // temporary ref.
      if (!share || !share->op)
      {
        ndbapi_invalidate_table(schema->db, ndb_table_name.c_str());
        ndb_tdc_close_cached_table(m_thd, schema->db, ndb_table_name.c_str());
      }
      if (share)
      {
        mysql_mutex_lock(&ndbcluster_mutex);
        NDB_SHARE::mark_share_dropped(&share); // server ref.
        DBUG_ASSERT(share);                    // Should still be ref'ed
        NDB_SHARE::release_reference_have_lock(share, "drop_db"); // temporary ref.
        mysql_mutex_unlock(&ndbcluster_mutex);
      }

      ndbapi_invalidate_table(schema->db, ndb_table_name.c_str());
      ndb_tdc_close_cached_table(m_thd, schema->db, ndb_table_name.c_str());
    }

    dd_client.commit();

    bool found_local_tables;
    if (!dd_client.have_local_tables_in_schema(schema->db, &found_local_tables))
    {
      DBUG_PRINT("info", ("Failed to check if db contained local tables"));
      // Failed to access the DD to check if non NDB tables existed, assume
      // the worst and skip dropping this database
      DBUG_VOID_RETURN;
    }

    DBUG_PRINT("exit",("found_local_tables: %d", found_local_tables));

    if (found_local_tables)
    {
      /* Tables exists as a local table, print error and leave it */
      ndb_log_warning("NDB Binlog: Skipping drop database '%s' since "
                      "it contained local tables "
                      "binlog schema event '%s' from node %d. ",
                      schema->db, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    // Run the plain DROP DATABASE query in order to remove other artifacts
    // like the physical database directory.
    // Note! This is not done in the case where a "shadow" table is found
    // in the schema, but at least all the NDB tables have in such case
    // already been removed from the DD
    const int no_print_error[1]= {0};
    run_query(m_thd, schema->query,
              schema->query + schema->query_length,
              no_print_error);

    DBUG_VOID_RETURN;
  }


  void
  handle_truncate_table(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_truncate_table");

    assert(!is_post_epoch()); // Always directly

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    NDB_SHARE *share= acquire_reference(schema->db, schema->name,
                                        "truncate_table");
    // invalidation already handled by binlog thread
    if (!share || !share->op)
    {
      ndbapi_invalidate_table(schema->db, schema->name);
      ndb_tdc_close_cached_table(m_thd, schema->db, schema->name);
    }
    if (share)
    {
      // Reset the tables shared auto_increment counter
      share->reset_tuple_id_range();

      NDB_SHARE::release_reference(share, "truncate_table"); // temporary ref.
    }

    bool exists_in_DD;
    Ndb_local_schema::Table tab(m_thd, schema->db, schema->name);
    if (tab.is_local_table(&exists_in_DD))
    {
      ndb_log_warning("NDB Binlog: Skipping locally defined table "
                      "'%s.%s' from binlog schema event '%s' from node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    if (ndb_create_table_from_engine(m_thd, schema->db, schema->name,
                                     true /* force_overwrite */))
    {
      // NOTE! The below function has a rather misleading name of
      // actual functionality which failed
      print_could_not_discover_error(m_thd, schema);
    }

    DBUG_VOID_RETURN;
  }


  void
  handle_create_table(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_create_table");

    assert(!is_post_epoch()); // Always directly

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    bool exists_in_DD;
    Ndb_local_schema::Table tab(m_thd, schema->db, schema->name);
    if (tab.is_local_table(&exists_in_DD))
    {
      ndb_log_warning("NDB Binlog: Skipping locally defined table '%s.%s' from "
                      "binlog schema event '%s' from node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    if (ndb_create_table_from_engine(m_thd, schema->db, schema->name))
    {
      // NOTE! The below function has a rather misleading name of
      // actual functionality which failed
      print_could_not_discover_error(m_thd, schema);
    }

    DBUG_VOID_RETURN;
  }


  void
  handle_create_db(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_create_db");

    assert(!is_post_epoch()); // Always directly

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    const int no_print_error[1]= {0};
    run_query(m_thd, schema->query,
              schema->query + schema->query_length,
              no_print_error);

    DBUG_VOID_RETURN;
  }


  void
  handle_alter_db(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_alter_db");

    assert(!is_post_epoch()); // Always directly

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    // Participant never takes GSL
    assert(get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    const int no_print_error[1]= {0};
    run_query(m_thd, schema->query,
              schema->query + schema->query_length,
              no_print_error);

    DBUG_VOID_RETURN;
  }


  void
  handle_grant_op(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_grant_op");

    assert(!is_post_epoch()); // Always directly

    if (schema->node_id == own_nodeid())
      DBUG_VOID_RETURN;

    write_schema_op_to_binlog(m_thd, schema);

    ndb_log_verbose(9, "Got dist_priv event: %s, flushing privileges",
                    Ndb_schema_dist_client::type_name(
                        static_cast<SCHEMA_OP_TYPE>(schema->type)));

    // Participant never takes GSL
    assert(get_thd_ndb(m_thd)->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT));

    const int no_print_error[1]= {0};
    char *cmd= (char *) "flush privileges";
    run_query(m_thd, cmd,
              cmd + strlen(cmd),
              no_print_error);

    DBUG_VOID_RETURN;
  }


  int
  handle_schema_op(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_schema_op");
    {
      const SCHEMA_OP_TYPE schema_type= (SCHEMA_OP_TYPE)schema->type;

      ndb_log_verbose(19,
                      "got schema event on %s.%s(%u/%u) query: '%s' "
                      "type: %s(%d) node: %u slock: %x%08x",
                      schema->db, schema->name,
                      schema->id, schema->version,
                      schema->query,
                      Ndb_schema_dist_client::type_name(
                          static_cast<SCHEMA_OP_TYPE>(schema->type)),
                      schema_type,
                      schema->node_id,
                      schema->slock.bitmap[1],
                      schema->slock.bitmap[0]);

      if ((schema->db[0] == 0) && (schema->name[0] == 0))
      {
        /**
         * This happens if there is a schema event on a table (object)
         *   that this mysqld does not know about.
         *   E.g it had a local table shadowing a ndb table...
         */
        DBUG_RETURN(0);
      }

      switch (schema_type)
      {
      case SOT_CLEAR_SLOCK:
        /*
          handle slock after epoch is completed to ensure that
          schema events get inserted in the binlog after any data
          events
        */
        handle_after_epoch(schema);
        DBUG_RETURN(0);

      case SOT_ALTER_TABLE_COMMIT:
      case SOT_RENAME_TABLE_PREPARE:
      case SOT_ONLINE_ALTER_TABLE_PREPARE:
      case SOT_ONLINE_ALTER_TABLE_COMMIT:
      case SOT_RENAME_TABLE:
      case SOT_DROP_TABLE:
      case SOT_DROP_DB:
        handle_after_epoch(schema);
        ack_after_epoch(schema);
        DBUG_RETURN(0);

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
        handle_grant_op(schema);
        break;

      case SOT_TABLESPACE:
      case SOT_LOGFILE_GROUP:
        if (schema->node_id == own_nodeid())
          break;
        write_schema_op_to_binlog(m_thd, schema);
        break;

      case SOT_RENAME_TABLE_NEW:
        /*
          Only very old MySQL Server connected to the cluster may
          send this schema operation, ignore it
        */
        ndb_log_error("Skipping old schema operation"
                      "(RENAME_TABLE_NEW) on %s.%s",
                      schema->db, schema->name);
        DBUG_ASSERT(false);
        break;

      }

      /* signal that schema operation has been handled */
      DBUG_DUMP("slock", (uchar*) schema->slock_buf, schema->slock_length);
      if (bitmap_is_set(&schema->slock, own_nodeid()))
      {
        ack_schema_op(schema);
      }
    }
    DBUG_RETURN(0);
  }


  void
  handle_schema_op_post_epoch(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_schema_op_post_epoch");
    DBUG_PRINT("enter", ("%s.%s: query: '%s'  type: %d",
                         schema->db, schema->name,
                         schema->query, schema->type));

    {
      const SCHEMA_OP_TYPE schema_type= (SCHEMA_OP_TYPE)schema->type;
      ndb_log_verbose(9, "%s - %s.%s",
                      Ndb_schema_dist_client::type_name(
                          static_cast<SCHEMA_OP_TYPE>(schema->type)),
                      schema->db, schema->name);

      switch (schema_type)
      {
      case SOT_CLEAR_SLOCK:
        handle_clear_slock(schema);
        break;

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

      default:
        DBUG_ASSERT(false);
      }
    }

    DBUG_VOID_RETURN;
  }

  THD* m_thd;
  MEM_ROOT* m_mem_root;
  uint m_own_nodeid;
  Ndb_schema_dist_data& m_schema_dist_data;
  bool m_post_epoch;

  bool is_post_epoch(void) const { return m_post_epoch; }

  List<const Ndb_schema_op> m_post_epoch_handle_list;
  List<const Ndb_schema_op> m_post_epoch_ack_list;

public:
  Ndb_schema_event_handler(); // Not implemented
  Ndb_schema_event_handler(const Ndb_schema_event_handler&); // Not implemented

  Ndb_schema_event_handler(THD* thd, MEM_ROOT* mem_root, uint own_nodeid,
                           Ndb_schema_dist_data& schema_dist_data):
    m_thd(thd), m_mem_root(mem_root), m_own_nodeid(own_nodeid),
    m_schema_dist_data(schema_dist_data),
    m_post_epoch(false)
  {
  }


  ~Ndb_schema_event_handler()
  {
    // There should be no work left todo...
    DBUG_ASSERT(m_post_epoch_handle_list.elements == 0);
    DBUG_ASSERT(m_post_epoch_ack_list.elements == 0);
  }


  void handle_event(Ndb* s_ndb, NdbEventOperation *pOp)
  {
    DBUG_ENTER("handle_event");

    const Ndb_event_data *event_data=
      static_cast<const Ndb_event_data*>(pOp->getCustomData());

    if (!check_is_ndb_schema_event(event_data))
      DBUG_VOID_RETURN;

    const NDBEVENT::TableEvent ev_type= pOp->getEventType();
    switch (ev_type)
    {
    case NDBEVENT::TE_INSERT:
    case NDBEVENT::TE_UPDATE:
    {
      /* ndb_schema table, row INSERTed or UPDATEed*/
      const Ndb_schema_op* schema_op=
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
        ndb_log_verbose(1,
                        "NDB Binlog: ndb tables initially readonly "
                        "on reconnect.");

      /* release the ndb_schema_share */
      mysql_mutex_lock(&injector_data_mutex);
      NDB_SHARE::release_reference(ndb_schema_share, "ndb_schema_share");
      ndb_schema_share= NULL;

      ndb_binlog_tables_inited= false;
      ndb_binlog_is_ready= false;
      mysql_mutex_unlock(&injector_data_mutex);

      ndb_tdc_close_cached_tables();

      ndbcluster_binlog_event_operation_teardown(m_thd, s_ndb, pOp);
      break;

    case NDBEVENT::TE_ALTER:
      /* ndb_schema table ALTERed */
      break;

    case NDBEVENT::TE_NODE_FAILURE:
    {
      /* Remove all subscribers for node */
      m_schema_dist_data.report_data_node_failure(pOp->getNdbdNodeId());
      break;
    }

    case NDBEVENT::TE_SUBSCRIBE:
    {
      /* Add node as subscriber */
      m_schema_dist_data.report_subscribe(pOp->getNdbdNodeId(), pOp->getReqNodeId());
      break;
    }

    case NDBEVENT::TE_UNSUBSCRIBE:
    {
      /* Remove node as subscriber */
      m_schema_dist_data.report_unsubscribe(pOp->getNdbdNodeId(), pOp->getReqNodeId());
      break;
    }

    default:
    {
      ndb_log_error("unknown event %u, ignoring...", ev_type);
    }
    }

    DBUG_VOID_RETURN;
  }


  void post_epoch()
  {
    if (unlikely(m_post_epoch_handle_list.elements > 0))
    {
      // Set the flag used to check that functions are called at correct time
      m_post_epoch= true;

      /*
       process any operations that should be done after
       the epoch is complete
      */
      const Ndb_schema_op* schema;
      while ((schema= m_post_epoch_handle_list.pop()))
      {
        handle_schema_op_post_epoch(schema);
      }

      /*
       process any operations that should be unlocked/acked after
       the epoch is complete
      */
      while ((schema= m_post_epoch_ack_list.pop()))
      {
        ack_schema_op(schema);
      }
    }
    // There should be no work left todo...
    DBUG_ASSERT(m_post_epoch_handle_list.elements == 0);
    DBUG_ASSERT(m_post_epoch_ack_list.elements == 0);
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
class Ndb_binlog_index_table_util
{

  /*
    Open the ndb_binlog_index table for writing
  */
  static int
  open_binlog_index_table(THD *thd,
                          TABLE **ndb_binlog_index)
  {
    const char *save_proc_info=
      thd_proc_info(thd, "Opening " NDB_REP_DB "." NDB_REP_TABLE);

    TABLE_LIST tables(STRING_WITH_LEN(NDB_REP_DB),    // db
                      STRING_WITH_LEN(NDB_REP_TABLE), // name
                      NDB_REP_TABLE,                  // alias
                      TL_WRITE);                      // for write

    /* Only allow real table to be opened */
    tables.required_type= dd::enum_table_type::BASE_TABLE;

    const uint flags =
      MYSQL_LOCK_IGNORE_TIMEOUT; /* Wait for lock "infinitely" */
    if (open_and_lock_tables(thd, &tables, flags))
    {
      if (thd->killed)
        DBUG_PRINT("error", ("NDB Binlog: Opening ndb_binlog_index: killed"));
      else
        ndb_log_error("NDB Binlog: Opening ndb_binlog_index: %d, '%s'",
                      thd->get_stmt_da()->mysql_errno(),
                      thd->get_stmt_da()->message_text());
      thd_proc_info(thd, save_proc_info);
      return -1;
    }
    *ndb_binlog_index= tables.table;
    thd_proc_info(thd, save_proc_info);
    return 0;
  }


  /*
    Write rows to the ndb_binlog_index table
  */
  static int
  write_rows_impl(THD *thd,
                  ndb_binlog_index_row *row)
  {
    int error= 0;
    ndb_binlog_index_row *first= row;
    TABLE *ndb_binlog_index= 0;
    // Save previous option settings
    ulonglong option_bits= thd->variables.option_bits;

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

    if (open_binlog_index_table(thd, &ndb_binlog_index))
    {
      if (thd->killed)
        DBUG_PRINT("error", ("NDB Binlog: Unable to lock table ndb_binlog_index, killed"));
      else
        ndb_log_error("NDB Binlog: Unable to lock table ndb_binlog_index");
      error= -1;
      goto add_ndb_binlog_index_err;
    }

    // Set all columns to be written
    ndb_binlog_index->use_all_columns();

    // Turn off autocommit to do all writes in one transaction
    thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;
    do
    {
      ulonglong epoch= 0, orig_epoch= 0;
      uint orig_server_id= 0;

      // Intialize ndb_binlog_index->record[0]
      empty_record(ndb_binlog_index);

      ndb_binlog_index->field[NBICOL_START_POS]
        ->store(first->start_master_log_pos, true);
      ndb_binlog_index->field[NBICOL_START_FILE]
        ->store(first->start_master_log_file,
                (uint)strlen(first->start_master_log_file),
                &my_charset_bin);
      ndb_binlog_index->field[NBICOL_EPOCH]
        ->store(epoch= first->epoch, true);
      if (ndb_binlog_index->s->fields > NBICOL_ORIG_SERVERID)
      {
        /* Table has ORIG_SERVERID / ORIG_EPOCH columns.
         * Write rows with different ORIG_SERVERID / ORIG_EPOCH
         * separately
         */
        ndb_binlog_index->field[NBICOL_NUM_INSERTS]
          ->store(row->n_inserts, true);
        ndb_binlog_index->field[NBICOL_NUM_UPDATES]
          ->store(row->n_updates, true);
        ndb_binlog_index->field[NBICOL_NUM_DELETES]
          ->store(row->n_deletes, true);
        ndb_binlog_index->field[NBICOL_NUM_SCHEMAOPS]
          ->store(row->n_schemaops, true);
        ndb_binlog_index->field[NBICOL_ORIG_SERVERID]
          ->store(orig_server_id= row->orig_server_id, true);
        ndb_binlog_index->field[NBICOL_ORIG_EPOCH]
          ->store(orig_epoch= row->orig_epoch, true);
        ndb_binlog_index->field[NBICOL_GCI]
          ->store(first->gci, true);

        if (ndb_binlog_index->s->fields > NBICOL_NEXT_POS)
        {
          /* Table has next log pos fields, fill them in */
          ndb_binlog_index->field[NBICOL_NEXT_POS]
            ->store(first->next_master_log_pos, true);
          ndb_binlog_index->field[NBICOL_NEXT_FILE]
            ->store(first->next_master_log_file,
                    (uint)strlen(first->next_master_log_file),
                    &my_charset_bin);
        }
        row= row->next;
      }
      else
      {
        /* Old schema : Table has no separate
         * ORIG_SERVERID / ORIG_EPOCH columns.
         * Merge operation counts and write one row
         */
        while ((row= row->next))
        {
          first->n_inserts+= row->n_inserts;
          first->n_updates+= row->n_updates;
          first->n_deletes+= row->n_deletes;
          first->n_schemaops+= row->n_schemaops;
        }
        ndb_binlog_index->field[NBICOL_NUM_INSERTS]
          ->store((ulonglong)first->n_inserts, true);
        ndb_binlog_index->field[NBICOL_NUM_UPDATES]
          ->store((ulonglong)first->n_updates, true);
        ndb_binlog_index->field[NBICOL_NUM_DELETES]
          ->store((ulonglong)first->n_deletes, true);
        ndb_binlog_index->field[NBICOL_NUM_SCHEMAOPS]
          ->store((ulonglong)first->n_schemaops, true);
      }

      error= ndb_binlog_index->file->ha_write_row(ndb_binlog_index->record[0]);

      /* Fault injection to test logging */
      if (DBUG_EVALUATE_IF("ndb_injector_binlog_index_write_fail_random", true,
                           false))
      {
        if ((((uint32)rand()) % 10) == 9)
        {
          ndb_log_error("NDB Binlog: Injecting random write failure");
          error= ndb_binlog_index->file->ha_write_row(ndb_binlog_index->record[0]);
        }
      }

      if (error)
      {
        ndb_log_error("NDB Binlog: Failed writing to ndb_binlog_index for "
                      "epoch %u/%u orig_server_id %u orig_epoch %u/%u "
                      "with error %d.",
                      uint(epoch >> 32), uint(epoch),
                      orig_server_id,
                      uint(orig_epoch >> 32), uint(orig_epoch),
                      error);
      
        bool seen_error_row = false;
        ndb_binlog_index_row* cursor= first;
        do
        {
          char tmp[128];
          if (ndb_binlog_index->s->fields > NBICOL_ORIG_SERVERID)
            snprintf(tmp, sizeof(tmp), "%u/%u,%u,%u/%u",
                        uint(epoch >> 32), uint(epoch),
                        uint(cursor->orig_server_id),
                        uint(cursor->orig_epoch >> 32),
                        uint(cursor->orig_epoch));

          else
            snprintf(tmp, sizeof(tmp), "%u/%u", uint(epoch >> 32), uint(epoch));

          bool error_row = (row == (cursor->next));
          ndb_log_error("NDB Binlog: Writing row (%s) to ndb_binlog_index - %s",
                        tmp,
                        (error_row?"ERROR":
                                   (seen_error_row?"Discarded":"OK")));
          seen_error_row |= error_row;

        } while ((cursor = cursor->next));

        error= -1;
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
    if (error)
    {
      // Error, rollback
      trans_rollback_stmt(thd);
    }
    else
    {
      assert(!thd->is_error());
      // Commit
      const bool failed= trans_commit_stmt(thd);
      if (failed ||
          thd->transaction_rollback_request)
      {
        /*
          Transaction failed to commit or
          was rolled back internally by the engine
          print an error message in the log and return the
          error, which will cause replication to stop.
        */
        error= thd->get_stmt_da()->mysql_errno();
        ndb_log_error("NDB Binlog: Failed committing transaction to "
                      "ndb_binlog_index with error %d.",
                      error);
        trans_rollback_stmt(thd);
      }
    }

    thd->get_stmt_da()->set_overwrite_status(false);

    // Restore previous option settings
    thd->variables.option_bits= option_bits;

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
  static
  void write_rows_with_new_thd(ndb_binlog_index_row *rows)
  {
    // Create a new THD and retry the write
    THD* new_thd = new THD;
    new_thd->set_new_thread_id();
    new_thd->thread_stack = (char*)&new_thd;
    new_thd->store_globals();
    new_thd->set_command(COM_DAEMON);
    new_thd->system_thread = SYSTEM_THREAD_NDBCLUSTER_BINLOG;
    new_thd->get_protocol_classic()->set_client_capabilities(0);
    new_thd->security_context()->skip_grants();
    new_thd->set_current_stmt_binlog_format_row();

    // Retry the write
    const int retry_result = write_rows_impl(new_thd, rows);
    if (retry_result)
    {
      ndb_log_error("NDB Binlog: Failed writing to ndb_binlog_index table "
                      "while retrying after kill during shutdown");
      DBUG_ASSERT(false); // Crash in debug compile
    }

    new_thd->restore_globals();
    delete new_thd;
  }

public:

  /*
    Write rows to the ndb_binlog_index table
  */
  static inline
  int write_rows(THD *thd,
                 ndb_binlog_index_row *rows)
  {
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
  static
  void write_rows_retry_after_kill(THD* orig_thd, ndb_binlog_index_row *rows)
  {
    // Should only be called when original THD has been killed
    DBUG_ASSERT(orig_thd->is_killed());

    write_rows_with_new_thd(rows);

    // Relink this thread with original THD
    orig_thd->store_globals();
  }
};


/*********************************************************************
  Functions for start, stop, wait for ndbcluster binlog thread
*********************************************************************/

int ndbcluster_binlog_start()
{
  DBUG_ENTER("ndbcluster_binlog_start");

  if (::server_id == 0)
  {
    ndb_log_warning("server id set to zero - changes logged to "
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
    ndb_log_error("server id provided is too large to be represented in "
                  "opt_server_id_bits or is reserved");
    DBUG_RETURN(-1);
  }

  /*
     Check that v2 events are enabled if log-transaction-id is set
  */
  if (opt_ndb_log_transaction_id &&
      log_bin_use_v1_row_events)
  {
    ndb_log_error("--ndb-log-transaction-id requires v2 Binlog row events "
                  "but server is using v1.");
    DBUG_RETURN(-1);
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
  mysql_mutex_init(PSI_INSTRUMENT_ME, &injector_data_mutex,
                   MY_MUTEX_INIT_FAST);

  // The binlog thread globals has been initied and should be freed
  ndbcluster_binlog_inited= 1;

  /* Start ndb binlog thread */
  if (ndb_binlog_thread.start())
  {
    DBUG_PRINT("error", ("Could not start ndb binlog thread"));
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}


void ndbcluster_binlog_set_server_started()
{
  ndb_binlog_thread.set_server_started();
}


void
NDB_SHARE::set_binlog_flags(Ndb_binlog_type ndb_binlog_type)
{
  DBUG_ENTER("set_binlog_flags");
  switch (ndb_binlog_type)
  {
  case NBT_NO_LOGGING:
    DBUG_PRINT("info", ("NBT_NO_LOGGING"));
    flags |= NDB_SHARE::FLAG_NO_BINLOG;
    DBUG_VOID_RETURN;
  case NBT_DEFAULT:
    DBUG_PRINT("info", ("NBT_DEFAULT"));
    if (opt_ndb_log_updated_only)
    {
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_FULL;
    }
    else
    {
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_FULL;
    }
    if (opt_ndb_log_update_as_write)
    {
      flags &= ~NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
    }
    else
    {
      flags |= NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
    }
    if (opt_ndb_log_update_minimal)
    {
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
    DBUG_VOID_RETURN;
  }
  flags &= ~NDB_SHARE::FLAG_NO_BINLOG;
  DBUG_VOID_RETURN;
}


/*
  Ndb_binlog_client::read_replication_info

  This function retrieves the data for the given table
  from the ndb_replication table.

  If the table is not found, or the table does not exist,
  then defaults are returned.
*/
bool
Ndb_binlog_client::read_replication_info(Ndb *ndb,
                                         const char* db,
                                         const char* table_name,
                                         uint server_id,
                                         uint32* binlog_flags,
                                         const st_conflict_fn_def** conflict_fn,
                                         st_conflict_fn_arg* args,
                                         uint* num_args)
{
  DBUG_ENTER("Ndb_binlog_client::read_replication_info");

  /* Override for ndb_apply_status when logging */
  if (opt_ndb_log_apply_status)
  {
    if (strcmp(db, NDB_REP_DB) == 0 &&
        strcmp(table_name, NDB_APPLY_TABLE) == 0)
    {
      /*
        Ensure that we get all columns from ndb_apply_status updates
        by forcing FULL event type
        Also, ensure that ndb_apply_status events are always logged as
        WRITES.
      */
      DBUG_PRINT("info", ("ndb_apply_status defaulting to FULL, USE_WRITE"));
      ndb_log_info("ndb-log-apply-status forcing %s.%s to FULL USE_WRITE",
                   NDB_REP_DB, NDB_APPLY_TABLE);
      *binlog_flags = NBT_FULL;
      *conflict_fn = NULL;
      *num_args = 0;
      DBUG_RETURN(false);
    }
  }

  Ndb_rep_tab_reader rep_tab_reader;

  int const rc = rep_tab_reader.lookup(ndb,
                                 db,
                                 table_name,
                                 server_id);


  if (rc == 0)
  {
    // lookup() may return a warning although it succeeds
    const char* msg = rep_tab_reader.get_warning_message();
    if (msg != NULL)
    {
      push_warning_printf(m_thd, Sql_condition::SL_WARNING,
          ER_NDB_REPLICATION_SCHEMA_ERROR,
          ER_THD(m_thd, ER_NDB_REPLICATION_SCHEMA_ERROR),
          msg);
      ndb_log_warning("NDB Binlog: %s", msg);
    }
  }
  else
  {
    /* When rep_tab_reader.lookup() returns with non-zero error code,
    it must give a warning message describing why it failed*/
    const char* msg = rep_tab_reader.get_warning_message();
    DBUG_ASSERT(msg);
    my_error(ER_NDB_REPLICATION_SCHEMA_ERROR, MYF(0), msg);
    ndb_log_warning("NDB Binlog: %s", msg);
    DBUG_RETURN(true);
  }

  *binlog_flags= rep_tab_reader.get_binlog_flags();
  const char* conflict_fn_spec= rep_tab_reader.get_conflict_fn_spec();

  if (conflict_fn_spec != NULL)
  {
    char msgbuf[ FN_REFLEN ];
    if (parse_conflict_fn_spec(conflict_fn_spec,
                               conflict_fn,
                               args,
                               num_args,
                               msgbuf,
                               sizeof(msgbuf)) != 0)
    {
      my_error(ER_CONFLICT_FN_PARSE_ERROR, MYF(0), msgbuf);

      /*
        Log as well, useful for contexts where the thd's stack of
        warnings are ignored
      */
      ndb_log_warning("NDB Slave: Table %s.%s : Parse error on conflict fn : %s",
                      db, table_name,
                      msgbuf);

      DBUG_RETURN(true);
    }
  }
  else
  {
    /* No conflict function specified */
    conflict_fn= NULL;
    num_args= 0;
  }

  DBUG_RETURN(false);
}


int
Ndb_binlog_client::apply_replication_info(Ndb* ndb, NDB_SHARE *share,
                                          const NdbDictionary::Table* ndbtab,
                                          const st_conflict_fn_def* conflict_fn,
                                          const st_conflict_fn_arg* args,
                                          uint num_args,
                                          uint32 binlog_flags)
{
  DBUG_ENTER("Ndb_binlog_client::apply_replication_info");
  char tmp_buf[FN_REFLEN];

  DBUG_PRINT("info", ("Setting binlog flags to %u", binlog_flags));
  share->set_binlog_flags((enum Ndb_binlog_type)binlog_flags);

  if (conflict_fn != NULL)
  {
    if (setup_conflict_fn(ndb,
                          &share->m_cfn_share,
                          share->db,
                          share->table_name,
                          share->get_binlog_use_update(),
                          ndbtab,
                          tmp_buf, sizeof(tmp_buf),
                          conflict_fn,
                          args,
                          num_args) == 0)
    {
      ndb_log_verbose(1, "NDB Slave: %s", tmp_buf);
    }
    else
    {
      /*
        Dump setup failure message to error log
        for cases where thd warning stack is
        ignored
      */
      ndb_log_warning("NDB Slave: Table %s.%s : %s",
                      share->db, share->table_name, tmp_buf);

      push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                          ER_CONFLICT_FN_PARSE_ERROR,
                          ER_THD(m_thd, ER_CONFLICT_FN_PARSE_ERROR),
                          tmp_buf);

      DBUG_RETURN(-1);
    }
  }
  else
  {
    /* No conflict function specified */
    slave_reset_conflict_fn(share->m_cfn_share);
  }

  DBUG_RETURN(0);
}


int
Ndb_binlog_client::read_and_apply_replication_info(Ndb *ndb, NDB_SHARE *share,
                            const NdbDictionary::Table* ndbtab, uint server_id)
{
  DBUG_ENTER("Ndb_binlog_client::read_and_apply_replication_info");
  uint32 binlog_flags;
  const st_conflict_fn_def* conflict_fn= NULL;
  st_conflict_fn_arg args[MAX_CONFLICT_ARGS];
  uint num_args = MAX_CONFLICT_ARGS;

  if (read_replication_info(ndb,
                            share->db,
                            share->table_name,
                            server_id,
                            &binlog_flags,
                            &conflict_fn,
                            args,
                            &num_args) ||
      apply_replication_info(ndb, share, ndbtab,
                             conflict_fn,
                             args,
                             num_args,
                             binlog_flags))
  {
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}


/*
  Common function for setting up everything for logging a table at
  create/discover.
*/
static
int ndbcluster_setup_binlog_for_share(THD *thd, Ndb *ndb,
                                      NDB_SHARE *share,
                                      const dd::Table* table_def)
{
  DBUG_ENTER("ndbcluster_setup_binlog_for_share");

  // This function should not be used to setup binlogging
  // of tables with temporary names.
  DBUG_ASSERT(!ndb_name_is_temp(share->table_name));

  Mutex_guard share_g(share->mutex);
  if (share->op != 0)
  {
    DBUG_PRINT("info", ("binlogging already setup"));
    DBUG_RETURN(0);
  }

  Ndb_binlog_client binlog_client(thd, share->db, share->table_name);

  ndb->setDatabaseName(share->db);
  NDBDICT *dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, share->table_name);
  const NDBTAB *ndbtab= ndbtab_g.get_table();
  if (ndbtab == 0)
  {
    ndb_log_verbose(1,
                    "NDB Binlog: Failed to open table '%s' from NDB, "
                    "error %s, %d",
                    share->key_string(),
                    dict->getNdbError().message,
                    dict->getNdbError().code);
    DBUG_RETURN(-1); // error
  }

  if (binlog_client.read_and_apply_replication_info(ndb, share, ndbtab,
                                                    ::server_id))
  {
    ndb_log_error("NDB Binlog: Failed to read and apply replication "
                  "info for table '%s'", share->key_string());
    DBUG_RETURN(-1);
  }

  if (binlog_client.table_should_have_event(share, ndbtab))
  {
    // Check if the event already exists in NDB, otherwise create it
    if (!binlog_client.event_exists_for_table(ndb, share))
    {
      // The event din't exist, create the event in NDB
      if (binlog_client.create_event(ndb, ndbtab,
                                     share))
      {
        // Failed to create event
        DBUG_RETURN(-1);
      }
    }

    if (binlog_client.table_should_have_event_op(share))
    {
      // Create the NDB event operation on the event
      Ndb_event_data* event_data;
      if (!binlog_client.create_event_data(share, table_def, &event_data) ||
          binlog_client.create_event_op(share, ndbtab, event_data))
      {
        // Failed to create event data or event operation
        DBUG_RETURN(-1);
      }
    }
  }

  DBUG_RETURN(0);
}


int ndbcluster_binlog_setup_table(THD *thd, Ndb *ndb,
                                  const char *db,
                                  const char *table_name,
                                  const dd::Table* table_def)
{
  DBUG_ENTER("ndbcluster_binlog_setup_table");
  DBUG_PRINT("enter",("db: '%s', table_name: '%s'", db, table_name));
  DBUG_ASSERT(table_def);

  DBUG_ASSERT(!ndb_name_is_blob_prefix(table_name));

  // Create key for ndbcluster_open_tables
  char key[FN_REFLEN + 1];
  {
    char *end= key +
               build_table_filename(key, sizeof(key) - 1, db, "", "", 0);
    end+= tablename_to_filename(table_name, end,
                                (uint)(sizeof(key)-(end-key)));

    ndb_log_verbose(1, "Using key '%s'", key);
  }

  mysql_mutex_lock(&ndbcluster_mutex);

  // Check if NDB_SHARE for this table already exist
  NDB_SHARE* share =
      NDB_SHARE::acquire_reference_by_key_have_lock(key,
                                                    "create_binlog_setup");
  if (share == nullptr)
  {
    // NDB_SHARE didn't exist, the normal case, try to create it
    share = NDB_SHARE::create_and_acquire_reference(key,
                                                    "create_binlog_setup");
    if (share == nullptr)
    {
      // Could not create the NDB_SHARE. Unlikely, catch in debug
      DBUG_ASSERT(false);
      DBUG_RETURN(-1);
    }
  }
  mysql_mutex_unlock(&ndbcluster_mutex);

  // Before 'schema_dist_is_ready', Thd_ndb::ALLOW_BINLOG_SETUP is required
  int ret= 0;
  if (ndb_schema_dist_is_ready() ||
      get_thd_ndb(thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP))
  {
    ret= ndbcluster_setup_binlog_for_share(thd, ndb, share, table_def);
  }

  NDB_SHARE::release_reference(share, "create_binlog_setup"); // temporary ref.

  DBUG_RETURN(ret);
}


int
Ndb_binlog_client::create_event(Ndb *ndb, const NdbDictionary::Table*ndbtab,
                                const NDB_SHARE* share)
{
  DBUG_ENTER("Ndb_binlog_client::create_event");
  DBUG_PRINT("enter", ("table: '%s', version: %d",
                      ndbtab->getName(), ndbtab->getObjectVersion()));
  DBUG_PRINT("enter", ("share->key: '%s'", share->key_string()));
  DBUG_ASSERT(share);

  // Never create event on table with temporary name
  DBUG_ASSERT(!ndb_name_is_temp(ndbtab->getName()));

  // Never create event on the blob table(s)
  DBUG_ASSERT(!ndb_name_is_blob_prefix(ndbtab->getName()));

  std::string event_name =
      event_name_for_table(m_dbname, m_tabname, share->get_binlog_full());

  ndb->setDatabaseName(share->db);
  NDBDICT *dict= ndb->getDictionary();
  NDBEVENT my_event(event_name.c_str());
  my_event.setTable(*ndbtab);
  my_event.addTableEvent(NDBEVENT::TE_ALL);
  if (ndb_table_has_hidden_pk(ndbtab))
  {
    /* Hidden primary key, subscribe for all attributes */
    my_event.setReport((NDBEVENT::EventReport)
                       (NDBEVENT::ER_ALL | NDBEVENT::ER_DDL));
    DBUG_PRINT("info", ("subscription all"));
  }
  else
  {
    if (Ndb_schema_dist_client::is_schema_dist_table(share->db,
                                                     share->table_name)) {
      /**
       * ER_SUBSCRIBE is only needed on schema distribution table
       */
      my_event.setReport((NDBEVENT::EventReport)
                         (NDBEVENT::ER_ALL |
                          NDBEVENT::ER_SUBSCRIBE |
                          NDBEVENT::ER_DDL));
      DBUG_PRINT("info", ("subscription all and subscribe"));
    }
    else
    {
      if (share->get_binlog_full())
      {
        my_event.setReport((NDBEVENT::EventReport)
                           (NDBEVENT::ER_ALL | NDBEVENT::ER_DDL));
        DBUG_PRINT("info", ("subscription all"));
      }
      else
      {
        my_event.setReport((NDBEVENT::EventReport)
                           (NDBEVENT::ER_UPDATED | NDBEVENT::ER_DDL));
        DBUG_PRINT("info", ("subscription only updated"));
      }
    }
  }
  if (ndb_table_has_blobs(ndbtab))
    my_event.mergeEvents(true);

  /* add all columns to the event */
  const int n_cols = ndbtab->getNoOfColumns();
  for(int a= 0; a < n_cols; a++)
    my_event.addEventColumn(a);

  if (dict->createEvent(my_event)) // Add event to database
  {
    if (dict->getNdbError().classification != NdbError::SchemaObjectExists)
    {
      // Failed to create event, log warning
      log_warning(ER_GET_ERRMSG,
                  "Unable to create event in database. "
                  "Event: %s  Error Code: %d  Message: %s",
                  event_name.c_str(),
                  dict->getNdbError().code, dict->getNdbError().message);
      DBUG_RETURN(-1);
    }

    /*
      try retrieving the event, if table version/id matches, we will get
      a valid event.  Otherwise we have an old event from before
    */
    const NDBEVENT *ev;
    if ((ev= dict->getEvent(event_name.c_str())))
    {
      delete ev;
      DBUG_RETURN(0);
    }

    // Old event from before; an error, but try to correct it
    if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT &&
        dict->dropEvent(my_event.getName(), 1))
    {
      // Failed to drop the old event, log warning
      log_warning(ER_GET_ERRMSG,
                  "Unable to create event in database. "
                  "Attempt to correct with drop failed. "
                  "Event: %s Error Code: %d Message: %s",
                  event_name.c_str(),
                  dict->getNdbError().code, dict->getNdbError().message);
      DBUG_RETURN(-1);
    }

    // Try to add the event again
    if (dict->createEvent(my_event))
    {
      // Still failed to create the event, log warning
      log_warning(ER_GET_ERRMSG,
                  "Unable to create event in database. "
                  "Attempt to correct with drop ok, but create failed. "
                  "Event: %s Error Code: %d Message: %s",
                  event_name.c_str(),
                  dict->getNdbError().code,dict->getNdbError().message);
      DBUG_RETURN(-1);
    }
  }

  ndb_log_verbose(1, "Created event '%s' for table '%s.%s' in NDB",
                  event_name.c_str(), m_dbname, m_tabname);

  DBUG_RETURN(0);
}


inline int is_ndb_compatible_type(Field *field)
{
  return
    !(field->flags & BLOB_FLAG) &&
    field->type() != MYSQL_TYPE_BIT &&
    field->pack_length() != 0;
}


/*
  - create NdbEventOperation for receiving log events
  - setup ndb recattrs for reception of log event data
  - "start" the event operation

  used at create/discover of tables
*/
int
Ndb_binlog_client::create_event_op(NDB_SHARE* share,
                                   const NdbDictionary::Table* ndbtab,
                                   const Ndb_event_data* event_data)
{
  /*
    we are in either create table or rename table so table should be
    locked, hence we can work with the share without locks
  */

  DBUG_ENTER("Ndb_binlog_client::create_event_op");
  DBUG_PRINT("enter", ("table: '%s', share->key: '%s'",
                       ndbtab->getName(), share->key_string()));
  DBUG_ASSERT(share);
  DBUG_ASSERT(event_data);

  // Never create event op on table with temporary name
  DBUG_ASSERT(!ndb_name_is_temp(ndbtab->getName()));

  // Never create event op on the blob table(s)
  DBUG_ASSERT(!ndb_name_is_blob_prefix(ndbtab->getName()));

  // Check if this is the event operation on mysql.ndb_schema
  // as it need special processing
  const bool do_ndb_schema_share = Ndb_schema_dist_client::is_schema_dist_table(
      share->db, share->table_name);

  // Check if this is the event operation on mysql.ndb_apply_status
  // as it need special processing
  const bool do_ndb_apply_status_share =
      (strcmp(share->db, NDB_REP_DB) == 0 &&
       strcmp(share->table_name, NDB_APPLY_TABLE) == 0);

  std::string event_name =
      event_name_for_table(m_dbname, m_tabname, share->get_binlog_full());

  // There should be no NdbEventOperation assigned yet
  DBUG_ASSERT(!share->op);

  TABLE *table= event_data->shadow_table;

  int retries= 100;
  int retry_sleep= 0;
  while (1)
  {
    if (retry_sleep > 0)
    {
      ndb_retry_sleep(retry_sleep);
    }
    Mutex_guard injector_mutex_g(injector_event_mutex);
    Ndb *ndb= injector_ndb;
    if (do_ndb_schema_share)
      ndb= schema_ndb;

    if (ndb == NULL)
      DBUG_RETURN(-1);

    NdbEventOperation* op;
    if (do_ndb_schema_share)
      op= ndb->createEventOperation(event_name.c_str());
    else
    {
      // set injector_ndb database/schema from table internal name
      int ret= ndb->setDatabaseAndSchemaName(ndbtab);
      assert(ret == 0); NDB_IGNORE_VALUE(ret);
      op= ndb->createEventOperation(event_name.c_str());
      // reset to catch errors
      ndb->setDatabaseName("");
    }
    if (!op)
    {
      log_warning(ER_GET_ERRMSG,
                  "Failed to create NdbEventOperation on '%s', error: %d - %s",
                  event_name.c_str(),
                  ndb->getNdbError().code, ndb->getNdbError().message);
      DBUG_RETURN(-1);
    }

    if (ndb_table_has_blobs(ndbtab))
      op->mergeEvents(true); // currently not inherited from event

    const uint n_columns= ndbtab->getNoOfColumns();
    const uint n_stored_fields= Ndb_table_map::num_stored_fields(table);
    const uint val_length= sizeof(NdbValue) * n_columns;

    /*
       Allocate memory globally so it can be reused after online alter table
    */
    if (my_multi_malloc(PSI_INSTRUMENT_ME,
                        MYF(MY_WME),
                        &event_data->ndb_value[0],
                        val_length,
                        &event_data->ndb_value[1],
                        val_length,
                        NULL) == 0)
    {
      DBUG_PRINT("info", ("Failed to allocate records for event operation"));
      DBUG_RETURN(-1);
    }

    Ndb_table_map map(table);
    for (uint j= 0; j < n_columns; j++)
    {
      const char *col_name= ndbtab->getColumn(j)->getName();
      NdbValue attr0, attr1;
      if (j < n_stored_fields)
      {
        Field *f= table->field[map.get_field_for_column(j)];
        if (is_ndb_compatible_type(f))
        {
          DBUG_PRINT("info", ("%s compatible", col_name));
          attr0.rec= op->getValue(col_name, (char*) f->ptr);
          attr1.rec= op->getPreValue(col_name,
                                     (f->ptr - table->record[0]) +
                                     (char*) table->record[1]);
        }
        else if (! (f->flags & BLOB_FLAG))
        {
          DBUG_PRINT("info", ("%s non compatible", col_name));
          attr0.rec= op->getValue(col_name);
          attr1.rec= op->getPreValue(col_name);
        }
        else
        {
          DBUG_PRINT("info", ("%s blob", col_name));
          DBUG_ASSERT(ndb_table_has_blobs(ndbtab));
          attr0.blob= op->getBlobHandle(col_name);
          attr1.blob= op->getPreBlobHandle(col_name);
          if (attr0.blob == NULL || attr1.blob == NULL)
          {
            log_warning(ER_GET_ERRMSG,
                        "Failed to cretate NdbEventOperation on '%s', "
                        "blob field %u handles failed, error: %d - %s",
                        event_name.c_str(), j,
                        op->getNdbError().code, op->getNdbError().message);
            ndb->dropEventOperation(op);
            DBUG_RETURN(-1);
          }
        }
      }
      else
      {
        DBUG_PRINT("info", ("%s hidden key", col_name));
        attr0.rec= op->getValue(col_name);
        attr1.rec= op->getPreValue(col_name);
      }
      event_data->ndb_value[0][j].ptr= attr0.ptr;
      event_data->ndb_value[1][j].ptr= attr1.ptr;
      DBUG_PRINT("info", ("&event_data->ndb_value[0][%d]: 0x%lx  "
                          "event_data->ndb_value[0][%d]: 0x%lx",
                          j, (long) &event_data->ndb_value[0][j],
                          j, (long) attr0.ptr));
      DBUG_PRINT("info", ("&event_data->ndb_value[1][%d]: 0x%lx  "
                          "event_data->ndb_value[1][%d]: 0x%lx",
                          j, (long) &event_data->ndb_value[0][j],
                          j, (long) attr1.ptr));
    }
    op->setCustomData((void *) event_data); // set before execute
    share->op= op; // assign op in NDB_SHARE

    /* Check if user explicitly requires monitoring of empty updates */
    if (opt_ndb_log_empty_update)
      op->setAllowEmptyUpdate(true);

    if (op->execute())
    {
      // Failed to create the NdbEventOperation
      share->op= NULL;
      retries--;
      if (op->getNdbError().status != NdbError::TemporaryError &&
          op->getNdbError().code != 1407)
        retries= 0;
      if (retries == 0)
      {
        ndb_log_warning("Failed to activate NdbEventOperation for '%s', "
                        "error: %d - %s",
                        event_name.c_str(),
                        op->getNdbError().code, op->getNdbError().message);
      }
      op->setCustomData(NULL);
      ndb->dropEventOperation(op);
      if (retries && !m_thd->killed)
      {
        // fairly high retry sleep, temporary error on schema operation can
        // take some time to resolve
        retry_sleep = 100; // milliseconds
        continue;
      }
      // Delete the event data, caller should create new before calling
      // this function again
      Ndb_event_data::destroy(event_data);
      DBUG_RETURN(-1);
    }
    break;
  }

  /* ndb_share reference binlog */
  NDB_SHARE::acquire_reference_on_existing(share, "binlog");

  if (do_ndb_apply_status_share)
  {
    ndb_apply_status_share =
        NDB_SHARE::acquire_reference_on_existing(share,
                                                 "ndb_apply_status_share");

    DBUG_ASSERT(get_thd_ndb(m_thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP));
  }
  else if (do_ndb_schema_share)
  {
    // ndb_schema_share also protected by injector_data_mutex
    Mutex_guard ndb_schema_share_g(injector_data_mutex);

    ndb_schema_share =
        NDB_SHARE::acquire_reference_on_existing(share,
                                                 "ndb_schema_share");

    DBUG_ASSERT(get_thd_ndb(m_thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP));
  }

  ndb_log_verbose(1, "NDB Binlog: logging %s (%s,%s)",
                  share->key_string(),
                  share->get_binlog_full() ? "FULL" : "UPDATED",
                  share->get_binlog_use_update() ? "USE_UPDATE" : "USE_WRITE");
  DBUG_RETURN(0);
}




void
Ndb_binlog_client::drop_events_for_table(THD *thd, Ndb *ndb,
                                         const char *db,
                                         const char *table_name)
{
  DBUG_ENTER("Ndb_binlog_client::drop_events_for_table");
  DBUG_PRINT("enter", ("db: %s, tabname: %s", db, table_name));

  for (uint i= 0; i < 2; i++)
  {
    std::string event_name =
        event_name_for_table(db, table_name, i,
                             false /* don't allow hardcoded event name */);
    
    NDBDICT *dict= ndb->getDictionary();
    if (dict->dropEvent(event_name.c_str()) == 0)
    {
      // Event dropped successfully
      continue;
    }

    if (dict->getNdbError().code == 4710 ||
        dict->getNdbError().code == 1419)
    {
      // Failed to drop event but return code says it was
      // because the event didn't exist, ignore
      continue;
    }

    /* Failed to drop event, push warning and write to log */
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG, ER_THD(thd, ER_GET_ERRMSG),
                        dict->getNdbError().code,
                        dict->getNdbError().message, "NDB");

    ndb_log_error("NDB Binlog: Unable to drop event for '%s.%s' from NDB, "
                  "event_name: '%s' error: '%d - %s'",
                  db, table_name, event_name.c_str(),
                  dict->getNdbError().code,
                  dict->getNdbError().message);
  }
  DBUG_VOID_RETURN;
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

int
ndbcluster_binlog_wait_synch_drop_table(THD *thd, NDB_SHARE *share)
{
  DBUG_ENTER("ndbcluster_binlog_synch_drop_table");
  DBUG_ASSERT(share);

  const char *save_proc_info= thd->proc_info;
  thd->proc_info= "Syncing ndb table schema operation and binlog";

  int max_timeout= DEFAULT_SYNC_TIMEOUT;

  mysql_mutex_lock(&share->mutex);
  while (share->op)
  {
    struct timespec abstime;
    set_timespec(&abstime, 1);

    // Unlock the share and wait for injector to signal that
    // something has happened. (NOTE! convoluted in order to
    // only use injector_data_cond with injector_data_mutex)
    mysql_mutex_unlock(&share->mutex);
    mysql_mutex_lock(&injector_data_mutex);
    const int ret= mysql_cond_timedwait(&injector_data_cond,
                                        &injector_data_mutex,
                                        &abstime);
    mysql_mutex_unlock(&injector_data_mutex);
    mysql_mutex_lock(&share->mutex);

    if (thd->killed ||
        share->op == 0)
      break;
    if (ret)
    {
      max_timeout--;
      if (max_timeout == 0)
      {
        ndb_log_error("%s, delete table timed out. Ignoring...",
                      share->key_string());
        DBUG_ASSERT(false);
        break;
      }
      if (ndb_log_get_verbose_level())
        ndb_report_waiting("delete table", max_timeout,
                           "delete table", share->key_string(), 0);
    }
  }
  mysql_mutex_unlock(&share->mutex);

  thd->proc_info= save_proc_info;

  DBUG_RETURN(0);
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

static void ndb_unpack_record(TABLE *table, NdbValue *value,
                              MY_BITMAP *defined, uchar *buf)
{
  Field **p_field= table->field, *field= *p_field;
  my_ptrdiff_t row_offset= (my_ptrdiff_t) (buf - table->record[0]);
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_ENTER("ndb_unpack_record");

  /*
    Set the filler bits of the null byte, since they are
    not touched in the code below.
    
    The filler bits are the MSBs in the last null byte
  */ 
  if (table->s->null_bytes > 0)
       buf[table->s->null_bytes - 1]|= 256U - (1U <<
					       table->s->last_null_bit_pos);
  /*
    Set null flag(s)
  */
  for ( ; field; p_field++, field= *p_field)
  {
    if(field->is_virtual_gcol())
      continue;

    field->set_notnull(row_offset);
    if ((*value).ptr)
    {
      if (!(field->flags & BLOB_FLAG))
      {
        int is_null= (*value).rec->isNULL();
        if (is_null)
        {
          if (is_null > 0)
          {
            DBUG_PRINT("info",("[%u] NULL", field->field_index));
            field->set_null(row_offset);
          }
          else
          {
            DBUG_PRINT("info",("[%u] UNDEFINED", field->field_index));
            bitmap_clear_bit(defined, field->field_index);
          }
        }
        else if (field->type() == MYSQL_TYPE_BIT)
        {
          Field_bit *field_bit= static_cast<Field_bit*>(field);

          /*
            Move internal field pointer to point to 'buf'.  Calling
            the correct member function directly since we know the
            type of the object.
           */
          field_bit->Field_bit::move_field_offset(row_offset);
          if (field->pack_length() < 5)
          {
            DBUG_PRINT("info", ("bit field H'%.8X", 
                                (*value).rec->u_32_value()));
            field_bit->Field_bit::store((longlong) (*value).rec->u_32_value(),
                                        true);
          }
          else
          {
            DBUG_PRINT("info", ("bit field H'%.8X%.8X",
                                *(Uint32 *)(*value).rec->aRef(),
                                *((Uint32 *)(*value).rec->aRef()+1)));
#ifdef WORDS_BIGENDIAN
            /* lsw is stored first */
            Uint32 *buf= (Uint32 *)(*value).rec->aRef();
            field_bit->Field_bit::store((((longlong)*buf)
                                         & 0x00000000FFFFFFFFLL)
                                        |
                                        ((((longlong)*(buf+1)) << 32)
                                         & 0xFFFFFFFF00000000LL),
                                        true);
#else
            field_bit->Field_bit::store((longlong)
                                        (*value).rec->u_64_value(), true);
#endif
          }
          /*
            Move back internal field pointer to point to original
            value (usually record[0]).
           */
          field_bit->Field_bit::move_field_offset(-row_offset);
          DBUG_PRINT("info",("[%u] SET",
                             (*value).rec->getColumn()->getColumnNo()));
          DBUG_DUMP("info", (const uchar*) field->ptr, field->pack_length());
        }
        else
        {
          DBUG_ASSERT(!strcmp((*value).rec->getColumn()->getName(), field->field_name));
          DBUG_PRINT("info",("[%u] SET",
                             (*value).rec->getColumn()->getColumnNo()));
          DBUG_DUMP("info", (const uchar*) field->ptr, field->pack_length());
        }
      }
      else
      {
        NdbBlob *ndb_blob= (*value).blob;
        const uint field_no= field->field_index;
        int isNull;
        ndb_blob->getDefined(isNull);
        if (isNull == 1)
        {
          DBUG_PRINT("info",("[%u] NULL", field_no));
          field->set_null(row_offset);
        }
        else if (isNull == -1)
        {
          DBUG_PRINT("info",("[%u] UNDEFINED", field_no));
          bitmap_clear_bit(defined, field_no);
        }
        else
        {
#ifndef DBUG_OFF
          // pointer vas set in get_ndb_blobs_value
          Field_blob *field_blob= (Field_blob*)field;
          uchar* ptr;
          field_blob->get_ptr(&ptr, row_offset);
          uint32 len= field_blob->get_length(row_offset);
          DBUG_PRINT("info",("[%u] SET ptr: 0x%lx  len: %u",
                             field_no, (long) ptr, len));
#endif
        }
      } // else
    } // if ((*value).ptr)
    value++;  // this field was not virtual
  } // for()
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_VOID_RETURN;
}

/*
  Handle error states on events from the storage nodes
*/
static int
handle_error(NdbEventOperation *pOp)
{
  Ndb_event_data *event_data= (Ndb_event_data *) pOp->getCustomData();
  NDB_SHARE *share= event_data->share;
  DBUG_ENTER("handle_error");

  ndb_log_error("NDB Binlog: unhandled error %d for table %s",
                pOp->hasError(), share->key_string());
  pOp->clearError();
  DBUG_RETURN(0);
}


/*
  Handle _non_ data events from the storage nodes
*/

static
void
handle_non_data_event(THD *thd,
                      NdbEventOperation *pOp,
                      ndb_binlog_index_row &row)
{
  const Ndb_event_data* event_data=
    static_cast<const Ndb_event_data*>(pOp->getCustomData());
  NDB_SHARE *share= event_data->share;
  const NDBEVENT::TableEvent type= pOp->getEventType();

  DBUG_ENTER("handle_non_data_event");
  DBUG_PRINT("enter", ("pOp: %p, event_data: %p, share: %p",
                       pOp, event_data, share));
  DBUG_PRINT("enter", ("type: %d", type));

  if (type == NDBEVENT::TE_DROP ||
      type == NDBEVENT::TE_ALTER)
  {
    // Count schema events
    row.n_schemaops++;
  }

  switch (type)
  {
  case NDBEVENT::TE_CLUSTER_FAILURE:
    ndb_log_verbose(1,
                    "NDB Binlog: cluster failure for %s at epoch %u/%u.",
                    share->key_string(),
                    (uint)(pOp->getGCI() >> 32),
                    (uint)(pOp->getGCI()));
    // fallthrough
  case NDBEVENT::TE_DROP:
    if (ndb_apply_status_share == share)
    {
      if (ndb_binlog_tables_inited && ndb_binlog_running)
        ndb_log_verbose(1,
                        "NDB Binlog: ndb tables initially "
                        "readonly on reconnect.");

      /* release the ndb_apply_status_share */
      NDB_SHARE::release_reference(ndb_apply_status_share,
                                   "ndb_apply_status_share");
      ndb_apply_status_share= NULL;

      Mutex_guard injector_g(injector_data_mutex);
      ndb_binlog_tables_inited= false;
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
    ndb_log_error("NDB Binlog: unknown non data event %d for %s. "
                  "Ignoring...", (unsigned) type, share->key_string());
    break;
  }

  DBUG_VOID_RETURN;
}

/*
  Handle data events from the storage nodes
*/
inline ndb_binlog_index_row *
ndb_find_binlog_index_row(ndb_binlog_index_row **rows,
                          uint orig_server_id, int flag)
{
  ndb_binlog_index_row *row= *rows;
  if (opt_ndb_log_orig)
  {
    ndb_binlog_index_row *first= row, *found_id= 0;
    for (;;)
    {
      if (row->orig_server_id == orig_server_id)
      {
        /* */
        if (!flag || !row->orig_epoch)
          return row;
        if (!found_id)
          found_id= row;
      }
      if (row->orig_server_id == 0)
        break;
      row= row->next;
      if (row == NULL)
      {
        row= (ndb_binlog_index_row*)sql_alloc(sizeof(ndb_binlog_index_row));
        memset(row, 0, sizeof(ndb_binlog_index_row));
        row->next= first;
        *rows= row;
        if (found_id)
        {
          /*
            If we found index_row with same server id already
            that row will contain the current stats.
            Copy stats over to new and reset old.
          */
          row->n_inserts= found_id->n_inserts;
          row->n_updates= found_id->n_updates;
          row->n_deletes= found_id->n_deletes;
          found_id->n_inserts= 0;
          found_id->n_updates= 0;
          found_id->n_deletes= 0;
        }
        /* keep track of schema ops only on "first" index_row */
        row->n_schemaops= first->n_schemaops;
        first->n_schemaops= 0;
        break;
      }
    }
    row->orig_server_id= orig_server_id;
  }
  return row;
}


static int
handle_data_event(NdbEventOperation *pOp,
                  ndb_binlog_index_row **rows,
                  injector::transaction &trans,
                  unsigned &trans_row_count,
                  unsigned &trans_slave_row_count)
{
  Ndb_event_data *event_data= (Ndb_event_data *) pOp->getCustomData();
  TABLE *table= event_data->shadow_table;
  NDB_SHARE *share= event_data->share;
  bool reflected_op = false;
  bool refresh_op = false;
  bool read_op = false;

  if (pOp != share->op)
  {
    return 0;
  }

  uint32 anyValue= pOp->getAnyValue();
  if (ndbcluster_anyvalue_is_reserved(anyValue))
  {
    if (ndbcluster_anyvalue_is_nologging(anyValue))
      return 0;
    
    if (ndbcluster_anyvalue_is_reflect_op(anyValue))
    {
      DBUG_PRINT("info", ("Anyvalue -> Reflect (%u)", anyValue));
      reflected_op = true;
      anyValue = 0;
    }
    else if (ndbcluster_anyvalue_is_refresh_op(anyValue))
    {
      DBUG_PRINT("info", ("Anyvalue -> Refresh"));
      refresh_op = true;
      anyValue = 0;
    }
    else if (ndbcluster_anyvalue_is_read_op(anyValue))
    {
      DBUG_PRINT("info", ("Anyvalue -> Read"));
      read_op = true;
      anyValue = 0;
    }
    else
    {
      ndb_log_warning("unknown value for binlog signalling 0x%X, "
                      "event not logged",
                      anyValue);
      return 0;
    }
  }

  uint32 originating_server_id= ndbcluster_anyvalue_get_serverid(anyValue);
  bool log_this_slave_update = g_ndb_log_slave_updates;
  bool count_this_event = true;

  if (share == ndb_apply_status_share)
  {
    /* 
       Note that option values are read without synchronisation w.r.t. 
       thread setting option variable or epoch boundaries.
    */
    if (opt_ndb_log_apply_status ||
        opt_ndb_log_orig)
    {
      Uint32 ndb_apply_status_logging_server_id= originating_server_id;
      Uint32 ndb_apply_status_server_id= 0;
      Uint64 ndb_apply_status_epoch= 0;
      bool event_has_data = false;

      switch(pOp->getEventType())
      {
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
      
      if (likely( event_has_data ))
      {
        /* unpack data to fetch orig_server_id and orig_epoch */
        MY_BITMAP b;
        uint32 bitbuf[128 / (sizeof(uint32) * 8)];
        ndb_bitmap_init(b, bitbuf, table->s->fields);
        bitmap_copy(&b, &event_data->stored_columns);
        ndb_unpack_record(table, event_data->ndb_value[0], &b, table->record[0]);
        ndb_apply_status_server_id= (uint)((Field_long *)table->field[0])->val_int();
        ndb_apply_status_epoch= ((Field_longlong *)table->field[1])->val_int();
        
        if (opt_ndb_log_apply_status)
        {
          /* 
             Determine if event came from our immediate Master server
             Ignore locally manually sourced and reserved events 
          */
          if ((ndb_apply_status_logging_server_id != 0) &&
              (! ndbcluster_anyvalue_is_reserved(ndb_apply_status_logging_server_id)))
          {
            bool isFromImmediateMaster = (ndb_apply_status_server_id ==
                                          ndb_apply_status_logging_server_id);
            
            if (isFromImmediateMaster)
            {
              /* 
                 We log this event with our server-id so that it 
                 propagates back to the originating Master (our
                 immediate Master)
              */
              assert(ndb_apply_status_logging_server_id != ::server_id);
              
              originating_server_id= 0; /* Will be set to our ::serverid below */
            }
          }
        }

        if (opt_ndb_log_orig)
        {
          /* store */
          ndb_binlog_index_row *row= ndb_find_binlog_index_row
            (rows, ndb_apply_status_server_id, 1);
          row->orig_epoch= ndb_apply_status_epoch;
        }
      }
    } // opt_ndb_log_apply_status || opt_ndb_log_orig)

    if (opt_ndb_log_apply_status)
    {
      /* We are logging ndb_apply_status changes
       * Don't count this event as making an epoch non-empty
       * Log this event in the Binlog
       */
      count_this_event = false;
      log_this_slave_update = true;
    }
    else
    {
      /* Not logging ndb_apply_status updates, discard this event now */
      return 0;
    }
  }
  
  if (originating_server_id == 0)
    originating_server_id= ::server_id;
  else 
  {
    assert(!reflected_op && !refresh_op);
    /* Track that we received a replicated row event */
    if (likely( count_this_event ))
      trans_slave_row_count++;
    
    if (!log_this_slave_update)
    {
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
  uint32 logged_server_id= anyValue;
  ndbcluster_anyvalue_set_serverid(logged_server_id, originating_server_id);

  /*
     Get NdbApi transaction id for this event to put into Binlog
  */
  Ndb_binlog_extra_row_info extra_row_info;
  const uchar* extra_row_info_ptr = NULL;
  Uint16 erif_flags = 0;
  if (opt_ndb_log_transaction_id)
  {
    erif_flags |= Ndb_binlog_extra_row_info::NDB_ERIF_TRANSID;
    extra_row_info.setTransactionId(pOp->getTransId());
  }

  /* Set conflict flags member if necessary */
  Uint16 event_conflict_flags = 0;
  assert(! (reflected_op && refresh_op));
  if (reflected_op)
  {
    event_conflict_flags |= NDB_ERIF_CFT_REFLECT_OP;
  }
  else if (refresh_op)
  {
    event_conflict_flags |= NDB_ERIF_CFT_REFRESH_OP;
  }
  else if (read_op)
  {
    event_conflict_flags |= NDB_ERIF_CFT_READ_OP;
  }
    
  if (DBUG_EVALUATE_IF("ndb_injector_set_event_conflict_flags", true, false))
  {
    event_conflict_flags = 0xfafa;
  }
  if (event_conflict_flags != 0)
  {
    erif_flags |= Ndb_binlog_extra_row_info::NDB_ERIF_CFT_FLAGS;
    extra_row_info.setConflictFlags(event_conflict_flags);
  }

  if (erif_flags != 0)
  {
    extra_row_info.setFlags(erif_flags);
    if (likely(!log_bin_use_v1_row_events))
    {
      extra_row_info_ptr = extra_row_info.generateBuffer();
    }
    else
    {
      /**
       * Can't put the metadata in a v1 event
       * Produce 1 warning at most
       */
      if (!g_injector_v1_warning_emitted)
      {
        ndb_log_error("Binlog Injector discarding row event "
                      "meta data as server is using v1 row events. "
                      "(%u %x)",
                      opt_ndb_log_transaction_id,
                      event_conflict_flags);

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
  my_bitmap_map bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                            8*sizeof(my_bitmap_map) - 1) /
                           (8*sizeof(my_bitmap_map))];
  ndb_bitmap_init(b, bitbuf, table->s->fields);
  bitmap_copy(&b, &event_data->stored_columns);
  if (bitmap_is_clear_all(&b))
  {
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
  uchar* blobs_buffer[2] = { 0, 0 };
  uint blobs_buffer_size[2] = { 0, 0 };

  ndb_binlog_index_row *row=
    ndb_find_binlog_index_row(rows, originating_server_id, 0);

  switch(pOp->getEventType())
  {
  case NDBEVENT::TE_INSERT:
    if (likely( count_this_event ))
    {
      row->n_inserts++;
      trans_row_count++;
    }
    DBUG_PRINT("info", ("INSERT INTO %s.%s",
                        table->s->db.str, table->s->table_name.str));
    {
      int ret;
      (void) ret; // Bug27150740 HANDLE_DATA_EVENT NEED ERROR HANDLING
      if (event_data->have_blobs)
      {
        my_ptrdiff_t ptrdiff= 0;
        ret = get_ndb_blobs_value(table, event_data->ndb_value[0],
                                  blobs_buffer[0],
                                  blobs_buffer_size[0],
                                  ptrdiff);
        assert(ret == 0);
      }
      ndb_unpack_record(table, event_data->ndb_value[0], &b, table->record[0]);
      ret = trans.write_row(logged_server_id,
                            injector::transaction::table(table, true),
                            &b, table->record[0],
                            extra_row_info_ptr);
      assert(ret == 0);
    }
    break;
  case NDBEVENT::TE_DELETE:
    if (likely( count_this_event ))
    {
      row->n_deletes++;
      trans_row_count++;
    }
    DBUG_PRINT("info",("DELETE FROM %s.%s",
                       table->s->db.str, table->s->table_name.str));
    {
      /*
        table->record[0] contains only the primary key in this case
        since we do not have an after image
      */
      int n;
      if (!share->get_binlog_full() && table->s->primary_key != MAX_KEY)
        n= 0; /*
                use the primary key only as it save time and space and
                it is the only thing needed to log the delete
              */
      else
        n= 1; /*
                we use the before values since we don't have a primary key
                since the mysql server does not handle the hidden primary
                key
              */

      int ret;
      (void) ret; // Bug27150740 HANDLE_DATA_EVENT NEED ERROR HANDLING
      if (event_data->have_blobs)
      {
        my_ptrdiff_t ptrdiff= table->record[n] - table->record[0];
        ret = get_ndb_blobs_value(table, event_data->ndb_value[n],
                                  blobs_buffer[n],
                                  blobs_buffer_size[n],
                                  ptrdiff);
        assert(ret == 0);
      }
      ndb_unpack_record(table, event_data->ndb_value[n], &b, table->record[n]);
      DBUG_EXECUTE("info", Ndb_table_map::print_record(table,
                                                       table->record[n]););
      ret = trans.delete_row(logged_server_id,
                             injector::transaction::table(table, true),
                             &b, table->record[n],
                             extra_row_info_ptr);
      assert(ret == 0);
    }
    break;
  case NDBEVENT::TE_UPDATE:
    if (likely( count_this_event ))
    {
      row->n_updates++;
      trans_row_count++;
    }
    DBUG_PRINT("info", ("UPDATE %s.%s",
                        table->s->db.str, table->s->table_name.str));
    {
      int ret;
      (void) ret; // Bug27150740 HANDLE_DATA_EVENT NEED ERROR HANDLING
      if (event_data->have_blobs)
      {
        my_ptrdiff_t ptrdiff= 0;
        ret = get_ndb_blobs_value(table, event_data->ndb_value[0],
                                  blobs_buffer[0],
                                  blobs_buffer_size[0],
                                  ptrdiff);
        assert(ret == 0);
      }
      ndb_unpack_record(table, event_data->ndb_value[0],
                        &b, table->record[0]);
      DBUG_EXECUTE("info", Ndb_table_map::print_record(table,
                                                       table->record[0]););
      if (table->s->primary_key != MAX_KEY &&
          !share->get_binlog_use_update())
      {
        /*
          since table has a primary key, we can do a write
          using only after values
        */
        ret = trans.write_row(logged_server_id,
                              injector::transaction::table(table, true),
                              &b, table->record[0],// after values
                              extra_row_info_ptr);
        assert(ret == 0);
      }
      else
      {
        /*
          mysql server cannot handle the ndb hidden key and
          therefore needs the before image as well
        */
        if (event_data->have_blobs)
        {
          my_ptrdiff_t ptrdiff= table->record[1] - table->record[0];
          ret = get_ndb_blobs_value(table, event_data->ndb_value[1],
                                    blobs_buffer[1],
                                    blobs_buffer_size[1],
                                    ptrdiff);
          assert(ret == 0);
        }
        ndb_unpack_record(table, event_data->ndb_value[1], &b, table->record[1]);
        DBUG_EXECUTE("info", Ndb_table_map::print_record(table,
                                                         table->record[1]););

        MY_BITMAP col_bitmap_before_update;
        my_bitmap_map bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                                  8*sizeof(my_bitmap_map) - 1) /
                                 (8*sizeof(my_bitmap_map))];
        ndb_bitmap_init(col_bitmap_before_update, bitbuf, table->s->fields);
        if (share->get_binlog_update_minimal())
        {
          event_data->generate_minimal_bitmap(&col_bitmap_before_update, &b);
        }
        else
        {
          bitmap_copy(&col_bitmap_before_update, &b);
        }

        ret = trans.update_row(logged_server_id,
                               injector::transaction::table(table, true),
                               &col_bitmap_before_update, &b,
                               table->record[1], // before values
                               table->record[0], // after values
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

  if (event_data->have_blobs)
  {
    my_free(blobs_buffer[0]);
    my_free(blobs_buffer[1]);
  }

  return 0;
}


/****************************************************************
  Injector thread main loop
****************************************************************/

static void
remove_event_operations(Ndb* ndb)
{
  DBUG_ENTER("remove_event_operations");
  NdbEventOperation *op;
  while ((op= ndb->getEventOperation()))
  {
    DBUG_ASSERT(!ndb_name_is_blob_prefix(op->getEvent()->getTable()->getName()));
    DBUG_PRINT("info", ("removing event operation on %s",
                        op->getEvent()->getName()));

    Ndb_event_data *event_data= (Ndb_event_data *) op->getCustomData();
    DBUG_ASSERT(event_data);

    NDB_SHARE *share= event_data->share;
    DBUG_ASSERT(share != NULL);
    DBUG_ASSERT(share->op == op);
    Ndb_event_data::destroy(event_data);
    op->setCustomData(NULL);

    mysql_mutex_lock(&share->mutex);
    share->op= 0;
    mysql_mutex_unlock(&share->mutex);

    NDB_SHARE::release_reference(share, "binlog");

    ndb->dropEventOperation(op);
  }
  DBUG_VOID_RETURN;
}

static void remove_all_event_operations(Ndb *s_ndb, Ndb *i_ndb)
{
  DBUG_ENTER("remove_all_event_operations");

  /* protect ndb_schema_share */
  mysql_mutex_lock(&injector_data_mutex);
  if (ndb_schema_share)
  {
    NDB_SHARE::release_reference(ndb_schema_share, "ndb_schema_share");
    ndb_schema_share= NULL;
  }
  mysql_mutex_unlock(&injector_data_mutex);
  /* end protect ndb_schema_share */

  /**
   * '!ndb_schema_dist_is_ready()' allows us relax the concurrency control
   * below as 'not ready' guarantees that no event subscribtion will be created.
   */
  if (ndb_apply_status_share)
  {
    NDB_SHARE::release_reference(ndb_apply_status_share,
                                 "ndb_apply_status_share");
    ndb_apply_status_share= NULL;
  }

  if (s_ndb)
    remove_event_operations(s_ndb);

  if (i_ndb)
    remove_event_operations(i_ndb);

  if (ndb_log_get_verbose_level() > 15)
  {
    NDB_SHARE::print_remaining_open_tables();
  }
  DBUG_VOID_RETURN;
}

extern long long g_event_data_count;
extern long long g_event_nondata_count;
extern long long g_event_bytes_count;

void updateInjectorStats(Ndb* schemaNdb, Ndb* dataNdb)
{
  /* Update globals to sum of totals from each listening
   * Ndb object
   */
  g_event_data_count = 
    schemaNdb->getClientStat(Ndb::DataEventsRecvdCount) + 
    dataNdb->getClientStat(Ndb::DataEventsRecvdCount);
  g_event_nondata_count = 
    schemaNdb->getClientStat(Ndb::NonDataEventsRecvdCount) + 
    dataNdb->getClientStat(Ndb::NonDataEventsRecvdCount);
  g_event_bytes_count = 
    schemaNdb->getClientStat(Ndb::EventBytesRecvdCount) + 
    dataNdb->getClientStat(Ndb::EventBytesRecvdCount);
}

/**
   injectApplyStatusWriteRow

   Inject a WRITE_ROW event on the ndb_apply_status table into
   the Binlog.
   This contains our server_id and the supplied epoch number.
   When applied on the Slave it gives a transactional position
   marker
*/
static
bool
injectApplyStatusWriteRow(injector::transaction& trans,
                          ulonglong gci)
{
  DBUG_ENTER("injectApplyStatusWriteRow");
  if (ndb_apply_status_share == NULL)
  {
    ndb_log_error("Could not get apply status share");
    DBUG_ASSERT(ndb_apply_status_share != NULL);
    DBUG_RETURN(false);
  }

  longlong gci_to_store = (longlong) gci;

#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("ndb_binlog_injector_cycle_gcis", true, false))
  {
    ulonglong gciHi = ((gci_to_store >> 32)
                       & 0xffffffff);
    ulonglong gciLo = (gci_to_store & 0xffffffff);
    gciHi = (gciHi % 3);
    ndb_log_warning("Binlog injector cycling gcis (%llu -> %llu)",
                    gci_to_store, (gciHi << 32) + gciLo);
    gci_to_store = (gciHi << 32) + gciLo;
  }
  if (DBUG_EVALUATE_IF("ndb_binlog_injector_repeat_gcis", true, false))
  {
    ulonglong gciHi = ((gci_to_store >> 32)
                       & 0xffffffff);
    ulonglong gciLo = (gci_to_store & 0xffffffff);
    gciHi=0xffffff00;
    gciLo=0;
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
  Ndb_event_data* event_data=
    (Ndb_event_data *) ndb_apply_status_share->op->getCustomData();
  DBUG_ASSERT(event_data);
  DBUG_ASSERT(event_data->shadow_table);
  TABLE* apply_status_table= event_data->shadow_table;

  /*
    Intialize apply_status_table->record[0]

    When iterating past the end of the last epoch, the first event of
    the new epoch may be on ndb_apply_status.  Its event data saved
    in record[0] would be overwritten here by a subsequent event on a
    normal table.  So save and restore its record[0].
  */
  static const ulong sav_max= 512; // current is 284
  const ulong sav_len= apply_status_table->s->reclength;
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
  const LEX_STRING &name= apply_status_table->s->table_name;
  DBUG_PRINT("info", ("use_table: %.*s",
                      (int) name.length, name.str));
#endif
  injector::transaction::table tbl(apply_status_table, true);
  int ret = trans.use_table(::server_id, tbl);
  assert(ret == 0); NDB_IGNORE_VALUE(ret);

  ret= trans.write_row(::server_id,
                       injector::transaction::table(apply_status_table,
                                                    true),
                       &apply_status_table->s->all_set,
                       apply_status_table->record[0]);

  assert(ret == 0);

  memcpy(apply_status_table->record[0], sav_buf, sav_len);
  DBUG_RETURN(true);
}


extern ulong opt_ndb_report_thresh_binlog_epoch_slip;
extern ulong opt_ndb_report_thresh_binlog_mem_usage;
extern ulong opt_ndb_eventbuffer_max_alloc;
extern uint opt_ndb_eventbuffer_free_percent;

Ndb_binlog_thread::Ndb_binlog_thread()
  : Ndb_component("Binlog")
{
}


Ndb_binlog_thread::~Ndb_binlog_thread()
{
}


void Ndb_binlog_thread::do_wakeup()
{
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


bool
Ndb_binlog_thread::check_reconnect_incident(THD* thd, injector *inj,
                                            Reconnect_type incident_id) const
{
  log_verbose(1, "Check for incidents");

  if (incident_id == MYSQLD_STARTUP)
  {
    LOG_INFO log_info;
    mysql_bin_log.get_current_log(&log_info);
    log_verbose(60, " - current binlog file: %s",
                log_info.log_file_name);

    uint log_number = 0;
    if ((sscanf(strend(log_info.log_file_name) - 6, "%u",
                &log_number) == 1) &&
        log_number == 1)
    {
      /*
        This is the fist binlog file, skip writing incident since
        there is really no log to have a gap in
      */
      log_verbose(60, " - skipping incident for first log, log_number: %u",
                  log_number);
      return false; // No incident written
    }
    log_verbose(60, " - current binlog file number: %u", log_number);
  }

  /*
    Insert an incident event since it's not possible to know what has
    happened in the cluster while not being connected.
  */
  LEX_STRING const msg[2] =
  {
    { C_STRING_WITH_LEN("mysqld startup") },
    { C_STRING_WITH_LEN("cluster disconnect") }
  };
  DBUG_ASSERT(incident_id < NDB_ARRAY_SIZE(msg));

  log_verbose(20, "Writing incident for %s", msg[incident_id].str);

  (void)inj->record_incident(thd,
                       binary_log::Incident_event::INCIDENT_LOST_EVENTS,
                       msg[incident_id]);

  return true; // Incident written
}


/*
  Events are handled one epoch at a time.
  Handle the lowest available epoch first.
*/
static
Uint64
find_epoch_to_handle(const NdbEventOperation *s_pOp, 
                     const NdbEventOperation *i_pOp)
{
  if (i_pOp != NULL)
  {
    if (s_pOp != NULL)
    {
      return std::min(i_pOp->getEpoch(),s_pOp->getEpoch());
    }
    return i_pOp->getEpoch();
  }
  if (s_pOp != NULL)
  {
    if (ndb_binlog_running)
    {
      return std::min(ndb_latest_received_binlog_epoch,s_pOp->getEpoch());
    }
    return s_pOp->getEpoch();
  }
  // 'latest_received' is '0' if not binlogging
  return ndb_latest_received_binlog_epoch;
}


void
Ndb_binlog_thread::do_run()
{
  THD *thd; /* needs to be first for thread_stack */
  Ndb *i_ndb= NULL;
  Ndb *s_ndb= NULL;
  Thd_ndb *thd_ndb=NULL;
  injector *inj= injector::instance();
  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();

  enum { BCCC_starting, BCCC_running, BCCC_restart,  } binlog_thread_state;

  /* Controls that only one incident is written per reconnect */
  bool do_reconnect_incident = true;
  /* Controls message of the reconnnect incident */
  Reconnect_type reconnect_incident_id = MYSQLD_STARTUP;

  DBUG_ENTER("ndb_binlog_thread");

  log_info("Starting...");

  thd= new THD; /* note that constructor of THD uses DBUG_ */
  THD_CHECK_SENTRY(thd);

  /* We need to set thd->thread_id before thd->store_globals, or it will
     set an invalid value for thd->variables.pseudo_thread_id.
  */
  thd->set_new_thread_id();

  thd->thread_stack= (char*) &thd; /* remember where our stack is */
  if (thd->store_globals())
  {
    delete thd;
    DBUG_VOID_RETURN;
  }

  thd->set_command(COM_DAEMON);
  thd->system_thread= SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->get_protocol_classic()->set_client_capabilities(0);
  thd->security_context()->skip_grants();
  // Create thd->net vithout vio
  thd->get_protocol_classic()->init_net((Vio *) 0);

  // Ndb binlog thread always use row format
  thd->set_current_stmt_binlog_format_row();

  thd->real_id= my_thread_self();
  thd_manager->add_thd(thd);
  thd->lex->start_transaction_opt= 0;

  log_info("Started");

  Ndb_schema_dist_data schema_dist_data;

restart_cluster_failure:
  /**
   * Maintain a current schema & injector eventOp to be handled.
   * s_pOp and s_ndb handle events from the 'ndb_schema' dist table,
   * while i_pOp and i_ndb is for binlogging 'everything else'.
   */
  NdbEventOperation *s_pOp= NULL;
  NdbEventOperation *i_pOp= NULL;
  binlog_thread_state= BCCC_starting;

  log_verbose(1, "Setting up");

  if (!(thd_ndb= Thd_ndb::seize(thd)))
  {
    log_error("Creating Thd_ndb object failed");
    goto err;
  }
  thd_ndb->set_option(Thd_ndb::NO_LOG_SCHEMA_OP); 

  if (!(s_ndb= new (std::nothrow) Ndb(g_ndb_cluster_connection,
                                      NDB_REP_DB)) ||
      s_ndb->setNdbObjectName("Ndb Binlog schema change monitoring") ||
      s_ndb->init())
  {
    log_error("Creating schema Ndb object failed");
    goto err;
  }
  log_info("Created schema Ndb object, reference: 0x%x, name: '%s'",
           s_ndb->getReference(), s_ndb->getNdbObjectName());

  // empty database
  if (!(i_ndb= new (std::nothrow) Ndb(g_ndb_cluster_connection, "")) ||
      i_ndb->setNdbObjectName("Ndb Binlog data change monitoring") ||
      i_ndb->init())
  {
    log_error("Creating injector Ndb object failed");
    goto err;
  }
  log_info("Created injector Ndb object, reference: 0x%x, name: '%s'",
                      i_ndb->getReference(), i_ndb->getNdbObjectName());

  /* Set free percent event buffer needed to resume buffering */
  if (i_ndb->set_eventbuffer_free_percent(opt_ndb_eventbuffer_free_percent))
  {
    log_error("Setting ventbuffer free percent failed");
    goto err;
  }

  log_verbose(10, "Exposing global references");
  /*
    Expose global reference to our ndb object.

    Used by both sql client thread and binlog thread to interact
    with the storage
  */
  mysql_mutex_lock(&injector_event_mutex);
  injector_thd= thd;
  injector_ndb= i_ndb;
  schema_ndb= s_ndb;
  DBUG_PRINT("info", ("set schema_ndb to s_ndb"));
  mysql_mutex_unlock(&injector_event_mutex);

  if (opt_bin_log && opt_ndb_log_bin)
  {
    ndb_binlog_running= true;
  }
  log_verbose(1, "Setup completed");

  /*
    Wait for the MySQL Server to start (so that the binlog is started
    and thus can receive the first GAP event)
  */
  if (!wait_for_server_started())
  {
    goto err;
  }

  // Defer call of THD::init_query_mem_roots until after
  // wait_for_server_started() to ensure that the parts of
  // MySQL Server it uses has been created
  thd->init_query_mem_roots();
  lex_start(thd);

  if (do_reconnect_incident && ndb_binlog_running)
  {
    if (check_reconnect_incident(thd, inj, reconnect_incident_id))
    {
      // Incident written, don't report incident again unless Ndb_binlog_thread
      // is restarted
      do_reconnect_incident = false;
    }
  }
  reconnect_incident_id= CLUSTER_DISCONNECT;
  {
    log_verbose(1, "Wait for cluster to start");
    thd->proc_info= "Waiting for ndbcluster to start";
    thd_set_thd_ndb(thd, thd_ndb);

    while (!ndbcluster_is_connected(1) || !ndb_binlog_setup(thd))
    {
      if (!thd_ndb->valid_ndb())
      {
        /*
          Cluster has gone away before setup was completed.
          Restart binlog
          thread to get rid of any garbage on the ndb objects
        */
        binlog_thread_state= BCCC_restart;
        goto err;
      }
      if (is_stop_requested())
      {
        goto err;
      }
      if (thd->killed == THD::KILL_CONNECTION)
      {
        /*
          Since the ndb binlog thread adds itself to the "global thread list"
          it need to look at the "killed" flag and stop the thread to avoid
          that the server hangs during shutdown while waiting for the "global
          thread list" to be emtpy.
        */
        log_info("Server shutdown detected while "
                  "waiting for ndbcluster to start...");
        goto err;
      }
      ndb_milli_sleep(1000);
    } //while (!ndb_binlog_setup())

    DBUG_ASSERT(ndbcluster_hton->slot != ~(uint)0);

    /*
      Prevent schema dist participant from (implicitly)
      taking GSL lock as part of taking MDL lock
    */
    thd_ndb->set_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT);
  }

  schema_dist_data.init(g_ndb_cluster_connection);

  {
    log_verbose(1, "Wait for first event");
    // wait for the first event
    thd->proc_info= "Waiting for first event from ndbcluster";
    Uint64 schema_gci;
    do
    {
      DBUG_PRINT("info", ("Waiting for the first event"));

      if (is_stop_requested())
        goto err;

      my_thread_yield();
      mysql_mutex_lock(&injector_event_mutex);
      (void)s_ndb->pollEvents(100, &schema_gci);
      mysql_mutex_unlock(&injector_event_mutex);
    } while (schema_gci == 0 || ndb_latest_received_binlog_epoch == schema_gci);

    if (ndb_binlog_running)
    {
      Uint64 gci= i_ndb->getLatestGCI();
      while (gci < schema_gci || gci == ndb_latest_received_binlog_epoch)
      {
        if (is_stop_requested())
          goto err;

        my_thread_yield();
        mysql_mutex_lock(&injector_event_mutex);
        (void)i_ndb->pollEvents(10, &gci);
        mysql_mutex_unlock(&injector_event_mutex);
      }
      if (gci > schema_gci)
      {
        schema_gci= gci;
      }
    }
    // now check that we have epochs consistent with what we had before the restart
    DBUG_PRINT("info", ("schema_gci: %u/%u", (uint)(schema_gci >> 32),
                        (uint)(schema_gci)));
    {
      i_ndb->flushIncompleteEvents(schema_gci);
      s_ndb->flushIncompleteEvents(schema_gci);
      if (schema_gci < ndb_latest_handled_binlog_epoch)
      {
        log_error("cluster has been restarted --initial or with older filesystem. "
                  "ndb_latest_handled_binlog_epoch: %u/%u, while current epoch: %u/%u. "
                  "RESET MASTER should be issued. Resetting ndb_latest_handled_binlog_epoch.",
                  (uint)(ndb_latest_handled_binlog_epoch >> 32),
                  (uint)(ndb_latest_handled_binlog_epoch),
                  (uint)(schema_gci >> 32),
                  (uint)(schema_gci));
        ndb_set_latest_trans_gci(0);
        ndb_latest_handled_binlog_epoch= 0;
        ndb_latest_applied_binlog_epoch= 0;
        ndb_latest_received_binlog_epoch= 0;
        ndb_index_stat_restart();
      }
      else if (ndb_latest_applied_binlog_epoch > 0)
      {
        log_warning("cluster has reconnected. "
                    "Changes to the database that occured while "
                    "disconnected will not be in the binlog");
      }
      log_verbose(1,
                  "starting log at epoch %u/%u",
                  (uint)(schema_gci >> 32),
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
  ndb_binlog_is_ready= true;
  mysql_mutex_unlock(&injector_data_mutex);

  log_verbose(1, "ndb tables writable");
  ndb_tdc_close_cached_tables();

  /* 
     Signal any waiting thread that ndb table setup is
     now complete
  */
  ndb_notify_tables_writable();

  {
    static LEX_CSTRING db_lex_cstr= EMPTY_CSTR;
    thd->reset_db(db_lex_cstr);
  }

  log_verbose(1, "Startup and setup completed");

  /*
    Main NDB Injector loop
  */
  do_reconnect_incident = true; // Report incident if disconnected
  binlog_thread_state= BCCC_running;

  /**
   * Injector loop runs until itself bring it out of 'BCCC_running' state,
   * or we get a stop-request from outside. In the later case we ensure that
   * all ongoing transaction epochs are completed first.
   */
  while (binlog_thread_state == BCCC_running &&
          (!is_stop_requested() ||
           ndb_latest_handled_binlog_epoch < ndb_get_latest_trans_gci()))
  {
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
    thd->proc_info= "Waiting for event from ndbcluster";
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
    int tot_poll_wait= 10;

    // If there are remaining unhandled injector eventOp we continue
    // handling of these, else poll for more.
    if (i_pOp == NULL)
    {
      // Capture any dynamic changes to max_alloc
      i_ndb->set_eventbuf_max_alloc(opt_ndb_eventbuffer_max_alloc);

      mysql_mutex_lock(&injector_event_mutex);
      Uint64 latest_epoch= 0;
      const int poll_wait= (ndb_binlog_running) ? tot_poll_wait : 0;
      const int res= i_ndb->pollEvents(poll_wait, &latest_epoch);
      (void)res; // Unused except DBUG_PRINT
      mysql_mutex_unlock(&injector_event_mutex);
      i_pOp = i_ndb->nextEvent();
      if (ndb_binlog_running)
      {
        ndb_latest_received_binlog_epoch= latest_epoch;
        tot_poll_wait= 0;
      }
      DBUG_PRINT("info", ("pollEvents res: %d", res));
    }

    // Epoch to handle from i_ndb. Use latest 'empty epoch' if no events.
    const Uint64 i_epoch = (i_pOp != NULL)
                             ? i_pOp->getEpoch()
                             : ndb_latest_received_binlog_epoch;

    // If there are remaining unhandled schema eventOp we continue
    // handling of these, else poll for more.
    if (s_pOp == NULL)
    {
      if (DBUG_EVALUATE_IF("ndb_binlog_injector_yield_before_schema_pollEvent",
                           true, false))
      {
        /**
         * Simulate that the binlog thread yields the CPU inbetween 
         * these two pollEvents, which can result in reading a
         * 'schema_gci > gci'. (Likely due to mutex locking)
         */
        ndb_milli_sleep(50);
      }
  
      Uint64 schema_epoch= 0;
      mysql_mutex_lock(&injector_event_mutex);
      int schema_res= s_ndb->pollEvents(tot_poll_wait, &schema_epoch);
      mysql_mutex_unlock(&injector_event_mutex);
      s_pOp = s_ndb->nextEvent();

      /*
        Make sure we have seen any schema epochs upto the injector epoch,
        or we have an earlier schema event to handle.
      */
      while (s_pOp == NULL && i_epoch > schema_epoch && schema_res >= 0)
      {
        static char buf[64];
        thd->proc_info= "Waiting for schema epoch";
        snprintf(buf, sizeof(buf), "%s %u/%u(%u/%u)", thd->proc_info,
                    (uint)(schema_epoch >> 32),
                    (uint)(schema_epoch),
                    (uint)(ndb_latest_received_binlog_epoch >> 32),
                    (uint)(ndb_latest_received_binlog_epoch));
        thd->proc_info= buf;

        my_thread_yield();
        mysql_mutex_lock(&injector_event_mutex);
        schema_res= s_ndb->pollEvents(10, &schema_epoch);
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
    const Uint64 current_epoch = find_epoch_to_handle(s_pOp,i_pOp);
    DBUG_ASSERT(current_epoch != 0 || !ndb_binlog_running);

    // Did someone else request injector thread to stop?
    DBUG_ASSERT(binlog_thread_state == BCCC_running);
    if (is_stop_requested() &&
        (ndb_latest_handled_binlog_epoch >= ndb_get_latest_trans_gci() ||
         !ndb_binlog_running))
      break; /* Stopping thread */

    if (thd->killed == THD::KILL_CONNECTION)
    {
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

    MEM_ROOT **root_ptr= THR_MALLOC;
    MEM_ROOT *old_root= *root_ptr;
    MEM_ROOT mem_root;
    init_sql_alloc(PSI_INSTRUMENT_ME, &mem_root, 4096, 0);

    // The Ndb_schema_event_handler does not necessarily need
    // to use the same memroot(or vice versa)
    Ndb_schema_event_handler
      schema_event_handler(thd, &mem_root,
                           g_ndb_cluster_connection->node_id(),
                           schema_dist_data);

    *root_ptr= &mem_root;

    if (unlikely(s_pOp != NULL && s_pOp->getEpoch() == current_epoch))
    {
      thd->proc_info= "Processing events from schema table";
      g_ndb_log_slave_updates= opt_log_slave_updates;
      s_ndb->
        setReportThreshEventGCISlip(opt_ndb_report_thresh_binlog_epoch_slip);
      s_ndb->
        setReportThreshEventFreeMem(opt_ndb_report_thresh_binlog_mem_usage);

      // Handle all schema event, limit within 'current_epoch'
      while (s_pOp != NULL && s_pOp->getEpoch() == current_epoch)
      {
        if (!s_pOp->hasError())
        {
          schema_event_handler.handle_event(s_ndb, s_pOp);

          if (DBUG_EVALUATE_IF("ndb_binlog_slow_failure_handling", true, false))
          {
            if (!ndb_binlog_is_ready)
            {
	      log_info("Just lost schema connection, hanging around");
              ndb_milli_sleep(10*1000); // seconds * 1000
              /* There could be a race where client side reconnect before we
               * are able to detect 's_ndb->getEventOperation() == NULL'.
               * Thus, we never restart the binlog thread as supposed to.
               * -> 'ndb_binlog_is_ready' remains false and we get stuck in RO-mode
               */
	      log_info("...and on our way");
            }
          }

          DBUG_PRINT("info", ("s_ndb first: %s", s_ndb->getEventOperation() ?
                              s_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                              "<empty>"));
          DBUG_PRINT("info", ("i_ndb first: %s", i_ndb->getEventOperation() ?
                              i_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                              "<empty>"));
        }
        else
        {
          log_error("error %d (%s) on handling binlog schema event",
                    s_pOp->getNdbError().code,
                    s_pOp->getNdbError().message);
        }
        s_pOp = s_ndb->nextEvent();
      }
      updateInjectorStats(s_ndb, i_ndb);
    }

    Uint64 inconsistent_epoch= 0;
    if (!ndb_binlog_running)
    {
      /*
        Just consume any events, not used if no binlogging
        e.g. node failure events
      */
      while (i_pOp != NULL && i_pOp->getEpoch() == current_epoch)
      {
        if ((unsigned) i_pOp->getEventType() >=
            (unsigned) NDBEVENT::TE_FIRST_NON_DATA_EVENT)
        {
          ndb_binlog_index_row row;
          handle_non_data_event(thd, i_pOp, row);
        }
        i_pOp= i_ndb->nextEvent();
      }
      updateInjectorStats(s_ndb, i_ndb);
    }

    // i_pOp == NULL means an inconsistent epoch or the queue is empty
    else if (i_pOp == NULL && !i_ndb->isConsistent(inconsistent_epoch))
    {
      char errmsg[64];
      uint end= sprintf(&errmsg[0],
                        "Detected missing data in GCI %llu, "
                        "inserting GAP event", inconsistent_epoch);
      errmsg[end]= '\0';
      DBUG_PRINT("info",
                 ("Detected missing data in GCI %llu, "
                  "inserting GAP event", inconsistent_epoch));
      LEX_STRING const msg= { C_STRING_WITH_LEN(errmsg) };
      inj->record_incident(thd,
                           binary_log::Incident_event::INCIDENT_LOST_EVENTS,
                           msg);
    }

    /* Handle all events withing 'current_epoch', or possible
     * log an empty epoch if log_empty_epoch is specified.
     */
    else if ((i_pOp != NULL && i_pOp->getEpoch() == current_epoch) ||
             (ndb_log_empty_epochs() &&
              current_epoch > ndb_latest_handled_binlog_epoch))
    {
      thd->proc_info= "Processing events";
      ndb_binlog_index_row _row;
      ndb_binlog_index_row *rows= &_row;
      injector::transaction trans;
      unsigned trans_row_count= 0;
      unsigned trans_slave_row_count= 0;

      if (i_pOp == NULL || i_pOp->getEpoch() != current_epoch)
      {
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
        rows= &_row;
        memset(&_row, 0, sizeof(_row));
        thd->variables.character_set_client= &my_charset_latin1;
        goto commit_to_binlog;
      }
      else
      {
        assert(i_pOp != NULL && i_pOp->getEpoch() == current_epoch);
        rows= &_row;

        DBUG_PRINT("info", ("Handling epoch: %u/%u",
                            (uint)(current_epoch >> 32),
                            (uint)(current_epoch)));
        // sometimes get TE_ALTER with invalid table
        DBUG_ASSERT(i_pOp->getEventType() == NdbDictionary::Event::TE_ALTER ||
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
        g_ndb_log_slave_updates= opt_log_slave_updates;
        i_ndb->
          setReportThreshEventGCISlip(opt_ndb_report_thresh_binlog_epoch_slip);
        i_ndb->setReportThreshEventFreeMem(opt_ndb_report_thresh_binlog_mem_usage);

        memset(&_row, 0, sizeof(_row));
        thd->variables.character_set_client= &my_charset_latin1;
        DBUG_PRINT("info", ("Initializing transaction"));
        inj->new_trans(thd, &trans);
        trans_row_count= 0;
        trans_slave_row_count= 0;
        // pass table map before epoch
        {
          Uint32 iter= 0;
          const NdbEventOperation *gci_op;
          Uint32 event_types;
          Uint32 cumulative_any_value;

          while ((gci_op= i_ndb->getNextEventOpInEpoch3(&iter, &event_types, &cumulative_any_value))
                 != NULL)
          {
            Ndb_event_data *event_data=
              (Ndb_event_data *) gci_op->getCustomData();
            NDB_SHARE *share= (event_data)?event_data->share:NULL;
            DBUG_PRINT("info", ("per gci_op: 0x%lx  share: 0x%lx  event_types: 0x%x",
                                (long) gci_op, (long) share, event_types));
            // workaround for interface returning TE_STOP events
            // which are normally filtered out below in the nextEvent loop
            if ((event_types & ~NdbDictionary::Event::TE_STOP) == 0)
            {
              DBUG_PRINT("info", ("Skipped TE_STOP on table %s",
                                  gci_op->getEvent()->getTable()->getName()));
              continue;
            }
            // this should not happen
            if (share == NULL || event_data->shadow_table == NULL)
            {
              DBUG_PRINT("info", ("no share or table %s!",
                                  gci_op->getEvent()->getTable()->getName()));
              continue;
            }
            if (share == ndb_apply_status_share)
            {
              // skip this table, it is handled specially
              continue;
            }
            TABLE *table= event_data->shadow_table;
#ifndef DBUG_OFF
            const LEX_STRING &name= table->s->table_name;
#endif
            if ((event_types & (NdbDictionary::Event::TE_INSERT |
                                NdbDictionary::Event::TE_UPDATE |
                                NdbDictionary::Event::TE_DELETE)) == 0)
            {
              DBUG_PRINT("info", ("skipping non data event table: %.*s",
                                  (int) name.length, name.str));
              continue;
            }
            if (!trans.good())
            {
              DBUG_PRINT("info",
                         ("Found new data event, initializing transaction"));
              inj->new_trans(thd, &trans);
            }
            {
              bool use_table= true;
              if (ndbcluster_anyvalue_is_reserved(cumulative_any_value))
              {
                /*
                   All events for this table in this epoch are marked as nologging,
                   therefore we do not include the table in the epoch transaction.
                */
                if (ndbcluster_anyvalue_is_nologging(cumulative_any_value))
                {
                  DBUG_PRINT("info", ("Skip binlogging table table: %.*s",
                                      (int) name.length, name.str));
                  use_table= false;
                }
              }
              if (use_table)
              {
                DBUG_PRINT("info", ("use_table: %.*s, cols %u",
                                    (int) name.length, name.str,
                                    table->s->fields));
                injector::transaction::table tbl(table, true);
                int ret = trans.use_table(::server_id, tbl);
                assert(ret == 0); NDB_IGNORE_VALUE(ret);
              }
            }
          }
        }
        if (trans.good())
        {
          /* Inject ndb_apply_status WRITE_ROW event */
          if (!injectApplyStatusWriteRow(trans, current_epoch))
          {
            log_error("Failed to inject apply status write row");
          }
        }

        do
        {
          if (i_pOp->hasError() &&
              handle_error(i_pOp) < 0)
            goto err;

#ifndef DBUG_OFF
          {
            Ndb_event_data *event_data=
              (Ndb_event_data *) i_pOp->getCustomData();
            NDB_SHARE *share= (event_data)?event_data->share:NULL;
            DBUG_PRINT("info",
                       ("EVENT TYPE: %d  Epoch: %u/%u last applied: %u/%u  "
                        "share: 0x%lx (%s.%s)", i_pOp->getEventType(),
                        (uint)(current_epoch >> 32),
                        (uint)(current_epoch),
                        (uint)(ndb_latest_applied_binlog_epoch >> 32),
                        (uint)(ndb_latest_applied_binlog_epoch),
                        (long) share,
                        share ? share->db :  "'NULL'",
                        share ? share->table_name : "'NULL'"));
            DBUG_ASSERT(share != 0);
          }
          // assert that there is consistancy between gci op list
          // and event list
          {
            Uint32 iter= 0;
            const NdbEventOperation *gci_op;
            Uint32 event_types;
            while ((gci_op= i_ndb->getGCIEventOperations(&iter, &event_types))
                   != NULL)
            {
              if (gci_op == i_pOp)
                break;
            }
            DBUG_ASSERT(gci_op == i_pOp);
            DBUG_ASSERT((event_types & i_pOp->getEventType()) != 0);
          }
#endif

          if ((unsigned) i_pOp->getEventType() <
              (unsigned) NDBEVENT::TE_FIRST_NON_DATA_EVENT)
            handle_data_event(i_pOp, &rows, trans,
                              trans_row_count, trans_slave_row_count);
          else
          {
            handle_non_data_event(thd, i_pOp, *rows);
            DBUG_PRINT("info", ("s_ndb first: %s", s_ndb->getEventOperation() ?
                                s_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                                "<empty>"));
            DBUG_PRINT("info", ("i_ndb first: %s", i_ndb->getEventOperation() ?
                                i_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                                "<empty>"));
          }

          // Capture any dynamic changes to max_alloc
          i_ndb->set_eventbuf_max_alloc(opt_ndb_eventbuffer_max_alloc);

          i_pOp = i_ndb->nextEvent();
        } while (i_pOp && i_pOp->getEpoch() == current_epoch);

        updateInjectorStats(s_ndb, i_ndb);
        
        /*
          NOTE: i_pOp is now referring to an event in the next epoch
          or is == NULL
        */

        while (trans.good())
        {
      commit_to_binlog:
          if (!ndb_log_empty_epochs())
          {
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
                (! (opt_ndb_log_apply_status &&
                    trans_slave_row_count) ))
            {
              /* nothing to commit, rollback instead */
              if (int r= trans.rollback())
              {
                log_error("Error during ROLLBACK of GCI %u/%u. Error: %d",
                          uint(current_epoch >> 32), uint(current_epoch), r);
                /* TODO: Further handling? */
              }
              break;
            }
          }
          thd->proc_info= "Committing events to binlog";
          if (int r= trans.commit())
          {
            log_error("Error during COMMIT of GCI. Error: %d", r);
            /* TODO: Further handling? */
          }
          injector::transaction::binlog_pos start= trans.start_pos();
          injector::transaction::binlog_pos next = trans.next_pos();
          rows->gci= (Uint32)(current_epoch >> 32); // Expose gci hi/lo
          rows->epoch= current_epoch;
          rows->start_master_log_file= start.file_name();
          rows->start_master_log_pos= start.file_pos();
          if ((next.file_pos() == 0) &&
              ndb_log_empty_epochs())
          {
            /* Empty transaction 'committed' due to log_empty_epochs
             * therefore no next position
             */
            rows->next_master_log_file= start.file_name();
            rows->next_master_log_pos= start.file_pos();
          }
          else
          {
            rows->next_master_log_file= next.file_name();
            rows->next_master_log_pos= next.file_pos();
          }

          DBUG_PRINT("info", ("COMMIT epoch: %lu", (ulong) current_epoch));
          if (opt_ndb_log_binlog_index)
          {
            if (Ndb_binlog_index_table_util::write_rows(thd, rows))
            {
              /* 
                 Writing to ndb_binlog_index failed, check if it's because THD have
                 been killed and retry in such case
              */
              if (thd->killed)
              {
                DBUG_PRINT("error", ("Failed to write to ndb_binlog_index at shutdown, retrying"));
                Ndb_binlog_index_table_util::write_rows_retry_after_kill(thd, rows);
              }
            }
          }
          ndb_latest_applied_binlog_epoch= current_epoch;
          break;
        } //while (trans.good())

        /*
          NOTE: There are possible more i_pOp available.
          However, these are from another epoch and should be handled
          in next iteration of the binlog injector loop.
        */
      }
    } //end: 'handled a 'current_epoch' of i_pOp's

    // Notify the schema event handler about post_epoch so it may finish
    // any outstanding business
    schema_event_handler.post_epoch();

    free_root(&mem_root, MYF(0));
    *root_ptr= old_root;

    if (current_epoch > ndb_latest_handled_binlog_epoch)
    {
      Mutex_guard injector_mutex_g(injector_data_mutex);
      ndb_latest_handled_binlog_epoch= current_epoch;
      // Signal ndbcluster_binlog_wait'ers
      mysql_cond_broadcast(&injector_data_cond);
    }

    // If all eventOp subscriptions has been teared down,
    // the binlog thread should now restart.
    DBUG_ASSERT(binlog_thread_state == BCCC_running);
    if (i_ndb->getEventOperation() == NULL &&
        s_ndb->getEventOperation() == NULL)
    {
      DBUG_PRINT("info", ("binlog_thread_state= BCCC_restart"));
      if (ndb_latest_handled_binlog_epoch < ndb_get_latest_trans_gci() && ndb_binlog_running)
      {
        log_error("latest transaction in epoch %u/%u not in binlog "
                  "as latest handled epoch is %u/%u",
                  (uint)(ndb_get_latest_trans_gci() >> 32),
                  (uint)(ndb_get_latest_trans_gci()),
                  (uint)(ndb_latest_handled_binlog_epoch >> 32),
                  (uint)(ndb_latest_handled_binlog_epoch));
      }
      binlog_thread_state= BCCC_restart;
      break;
    }
  }
 err:
  if (binlog_thread_state != BCCC_restart)
  {
    log_info("Shutting down");
    thd->proc_info= "Shutting down";
  }
  else
  { 
    log_info("Restarting");
    thd->proc_info= "Restarting";
  }

  mysql_mutex_lock(&injector_event_mutex);
  /* don't mess with the injector_ndb anymore from other threads */
  injector_thd= NULL;
  injector_ndb= NULL;
  schema_ndb= NULL;
  mysql_mutex_unlock(&injector_event_mutex);

  mysql_mutex_lock(&injector_data_mutex);
  ndb_binlog_tables_inited= false;
  mysql_mutex_unlock(&injector_data_mutex);

  thd->reset_db(NULL_CSTR); // as not to try to free memory
  remove_all_event_operations(s_ndb, i_ndb);

  delete s_ndb;
  s_ndb= NULL;

  delete i_ndb;
  i_ndb= NULL;

  if (thd_ndb)
  {
    Thd_ndb::release(thd_ndb);
    thd_set_thd_ndb(thd, NULL);
    thd_ndb= NULL;
  }

  /**
   * release all extra references from tables
   */
  log_verbose(9, "Release extra share references");
  NDB_SHARE::release_extra_share_references();

  log_info("Stopping...");

  ndb_tdc_close_cached_tables();
  if (ndb_log_get_verbose_level() > 15)
  {
    NDB_SHARE::print_remaining_open_tables();
  }

  schema_dist_data.release();

  if (binlog_thread_state == BCCC_restart)
  {
    goto restart_cluster_failure;
  }

  // Release the thd->net created without vio
  thd->get_protocol_classic()->end_net();
  thd->release_resources();
  thd_manager->remove_thd(thd);
  delete thd;

  ndb_binlog_running= false;
  mysql_cond_broadcast(&injector_data_cond);

  log_info("Stopped");

  DBUG_PRINT("exit", ("ndb_binlog_thread"));
  DBUG_VOID_RETURN;
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

size_t
ndbcluster_show_status_binlog(char *buf, size_t buf_size)
{
  DBUG_ENTER("ndbcluster_show_status_binlog");
  
  mysql_mutex_lock(&injector_event_mutex);
  if (injector_ndb)
  {
    const ulonglong latest_epoch= injector_ndb->getLatestGCI();
    mysql_mutex_unlock(&injector_event_mutex);

    // Get highest trans gci seen by the cluster connections
    const ulonglong latest_trans_epoch = ndb_get_latest_trans_gci();

    const size_t buf_len =
      snprintf(buf, buf_size,
                  "latest_epoch=%llu, "
                  "latest_trans_epoch=%llu, "
                  "latest_received_binlog_epoch=%llu, "
                  "latest_handled_binlog_epoch=%llu, "
                  "latest_applied_binlog_epoch=%llu",
                  latest_epoch,
                  latest_trans_epoch,
                  ndb_latest_received_binlog_epoch,
                  ndb_latest_handled_binlog_epoch,
                  ndb_latest_applied_binlog_epoch);
      DBUG_RETURN(buf_len);
  }
  else
    mysql_mutex_unlock(&injector_event_mutex);
  DBUG_RETURN(0);
}

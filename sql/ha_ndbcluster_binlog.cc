/*
  Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ha_ndbcluster_glue.h"
#include "ha_ndbcluster.h"
#include "ha_ndbcluster_connection.h"
#include "ndb_local_connection.h"
#include "ndb_thd.h"
#include "ndb_table_guard.h"
#include "ndb_global_schema_lock.h"
#include "ndb_global_schema_lock_guard.h"
#include "ndb_tdc.h"
#include "ndb_name_util.h"
#include "ndb_bitmap.h"
#include <NdbSleep.h>

#include "rpl_injector.h"
#include "rpl_filter.h"
#include "rpl_slave.h"
#include "binlog.h"
#include "ha_ndbcluster_binlog.h"
#include <ndbapi/NdbDictionary.hpp>
#include <ndbapi/ndb_cluster_connection.hpp>
#include "mysqld_thd_manager.h"  // Global_THD_manager

#include <mysql/psi/mysql_thread.h>

extern my_bool opt_ndb_log_orig;
extern my_bool opt_ndb_log_bin;
extern my_bool opt_ndb_log_update_as_write;
extern my_bool opt_ndb_log_updated_only;
extern my_bool opt_ndb_log_update_minimal;
extern my_bool opt_ndb_log_binlog_index;
extern my_bool opt_ndb_log_apply_status;
extern ulong opt_ndb_extra_logging;
extern st_ndb_slave_state g_ndb_slave_state;
extern my_bool opt_ndb_log_transaction_id;
extern my_bool log_bin_use_v1_row_events;
extern my_bool opt_ndb_log_empty_update;
extern my_bool opt_ndb_clear_apply_status;

bool ndb_log_empty_epochs(void);

void ndb_index_stat_restart();

/*
  defines for cluster replication table names
*/
#include "ha_ndbcluster_tables.h"

#include "ndb_dist_priv_util.h"
#include "ndb_anyvalue.h"
#include "ndb_binlog_extra_row_info.h"
#include "ndb_event_data.h"
#include "ndb_schema_object.h"
#include "ndb_schema_dist.h"
#include "ndb_repl_tab.h"
#include "ndb_binlog_thread.h"
#include "ndb_find_files_list.h"

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
  Flag showing if the ndb binlog should be created, if so == TRUE
  FALSE if not
*/
my_bool ndb_binlog_running= FALSE;
static my_bool ndb_binlog_tables_inited= FALSE;  //injector_data_mutex, relaxed
static my_bool ndb_binlog_is_ready= FALSE;       //injector_data_mutex, relaxed
 
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

extern my_bool opt_log_slave_updates;
static my_bool g_ndb_log_slave_updates;

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

#ifndef DBUG_OFF
static void print_records(TABLE *table, const uchar *record)
{
  for (uint j= 0; j < table->s->fields; j++)
  {
    char buf[40];
    int pos= 0;
    Field *field= table->field[j];
    const uchar* field_ptr= field->ptr - table->record[0] + record;
    int pack_len= field->pack_length();
    int n= pack_len < 10 ? pack_len : 10;

    for (int i= 0; i < n && pos < 20; i++)
    {
      pos+= sprintf(&buf[pos]," %x", (int) (uchar) field_ptr[i]);
    }
    buf[pos]= 0;
    DBUG_PRINT("info",("[%u]field_ptr[0->%d]: %s", j, n, buf));
  }
}
#else
#define print_records(a,b)
#endif


#ifndef DBUG_OFF
static void dbug_print_table(const char *info, TABLE *table)
{
  if (table == 0)
  {
    DBUG_PRINT("info",("%s: (null)", info));
    return;
  }
  DBUG_PRINT("info",
             ("%s: %s.%s s->fields: %d  "
              "reclength: %lu  rec_buff_length: %u  record[0]: 0x%lx  "
              "record[1]: 0x%lx",
              info,
              table->s->db.str,
              table->s->table_name.str,
              table->s->fields,
              table->s->reclength,
              table->s->rec_buff_length,
              (long) table->record[0],
              (long) table->record[1]));

  for (unsigned int i= 0; i < table->s->fields; i++) 
  {
    Field *f= table->field[i];
    DBUG_PRINT("info",
               ("[%d] \"%s\"(0x%lx:%s%s%s%s%s%s) type: %d  pack_length: %d  "
                "ptr: 0x%lx[+%d]  null_bit: %u  null_ptr: 0x%lx[+%d]",
                i,
                f->field_name,
                (long) f->flags,
                (f->flags & PRI_KEY_FLAG)  ? "pri"       : "attr",
                (f->flags & NOT_NULL_FLAG) ? ""          : ",nullable",
                (f->flags & UNSIGNED_FLAG) ? ",unsigned" : ",signed",
                (f->flags & ZEROFILL_FLAG) ? ",zerofill" : "",
                (f->flags & BLOB_FLAG)     ? ",blob"     : "",
                (f->flags & BINARY_FLAG)   ? ",binary"   : "",
                f->real_type(),
                f->pack_length(),
                (long) f->ptr, (int) (f->ptr - table->record[0]),
                f->null_bit,
                (long) f->null_offset(0),
                (int) f->null_offset()));
    if (f->type() == MYSQL_TYPE_BIT)
    {
      Field_bit *g= (Field_bit*) f;
      DBUG_PRINT("MYSQL_TYPE_BIT",("field_length: %d  bit_ptr: 0x%lx[+%d] "
                                   "bit_ofs: %d  bit_len: %u",
                                   g->field_length, (long) g->bit_ptr,
                                   (int) ((uchar*) g->bit_ptr -
                                          table->record[0]),
                                   g->bit_ofs, g->bit_len));
    }
  }
}
#else
#define dbug_print_table(a,b)
#endif


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

static void
ndb_binlog_close_shadow_table(NDB_SHARE *share)
{
  DBUG_ENTER("ndb_binlog_close_shadow_table");
  Ndb_event_data *event_data= share->event_data;
  if (event_data)
  {
    delete event_data;
    share->event_data= 0;
  }
  DBUG_VOID_RETURN;
}


/*
  Open a shadow table for the table given in share.
  - The shadow table is (mainly) used when an event is
    received from the data nodes which need to be written
    to the binlog injector.
*/

static int
ndb_binlog_open_shadow_table(THD *thd, NDB_SHARE *share)
{
  int error;
  DBUG_ASSERT(share->event_data == 0);
  Ndb_event_data *event_data= share->event_data= new Ndb_event_data(share);
  DBUG_ENTER("ndb_binlog_open_shadow_table");

  MEM_ROOT **root_ptr= my_thread_get_THR_MALLOC();
  MEM_ROOT *old_root= *root_ptr;
  init_sql_alloc(PSI_INSTRUMENT_ME, &event_data->mem_root, 1024, 0);
  *root_ptr= &event_data->mem_root;

  TABLE_SHARE *shadow_table_share=
    (TABLE_SHARE*)alloc_root(&event_data->mem_root, sizeof(TABLE_SHARE));
  TABLE *shadow_table=
    (TABLE*)alloc_root(&event_data->mem_root, sizeof(TABLE));

  init_tmp_table_share(thd, shadow_table_share,
                       share->db, 0,
                       share->table_name,
                       share->key_string());
  if ((error= open_table_def(thd, shadow_table_share, 0)) ||
      (error= open_table_from_share(thd, shadow_table_share, "", 0,
                                    (uint) (OPEN_FRM_FILE_ONLY | DELAYED_OPEN | READ_ALL),
                                    0, shadow_table,
                                    false
                                    )))
  {
    DBUG_PRINT("error", ("failed to open shadow table, error: %d my_errno: %d",
                         error, my_errno()));
    free_table_share(shadow_table_share);
    delete event_data;
    share->event_data= 0;
    *root_ptr= old_root;
    DBUG_RETURN(error);
  }
  event_data->shadow_table= shadow_table;

  mysql_mutex_lock(&LOCK_open);
  assign_new_table_id(shadow_table_share);
  mysql_mutex_unlock(&LOCK_open);

  shadow_table->in_use= injector_thd;
  

  // Allocate strings for db and table_name for shadow_table
  // in event_data's MEM_ROOT(where the shadow_table itself is allocated)
  lex_string_copy(&event_data->mem_root,
                  &shadow_table->s->db,
                  share->db);
  lex_string_copy(&event_data->mem_root,
                  &shadow_table->s->table_name,
                  share->table_name);

  /* We can't use 'use_all_columns()' as the file object is not setup yet */
  shadow_table->column_bitmaps_set_no_signal(&shadow_table->s->all_set,
                                             &shadow_table->s->all_set);

  share->set_binlog_flags_for_table(shadow_table);
  event_data->init_pk_bitmap();
#ifndef DBUG_OFF
  dbug_print_table("table", shadow_table);
#endif
  *root_ptr= old_root;
  DBUG_RETURN(0);
}


/*
  Initialize the binlog part of the NDB_SHARE
*/
int ndbcluster_binlog_init_share(THD *thd, NDB_SHARE *share, TABLE *_table)
{
  DBUG_ENTER("ndbcluster_binlog_init_share");

  if (!share->need_events(ndb_binlog_running))
  {
    share->set_binlog_flags_for_table(_table);
    DBUG_RETURN(0);
  }

  DBUG_RETURN(ndb_binlog_open_shadow_table(thd, share));
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
        sql_print_error("get_ndb_blobs_value: my_malloc(%u) failed", offset);
        DBUG_RETURN(-1);
      }
      buffer_size= offset;
    }
  }
  DBUG_RETURN(0);
}


/*****************************************************************
  functions called from master sql client threads
****************************************************************/

/*
  called in mysql_show_binlog_events and reset_logs to make sure we wait for
  all events originating from the 'thd' to arrive in the binlog.

  'thd' is expected to be non-NULL.

  Wait for the epoch in which the last transaction of the 'thd' is a part of.

  Wait a maximum of 30 seconds.
*/
static void ndbcluster_binlog_wait(THD *thd)
{
  if (ndb_binlog_running)
  {
    DBUG_ENTER("ndbcluster_binlog_wait");
    DBUG_ASSERT(thd);
    DBUG_ASSERT(thd_sql_command(thd) == SQLCOM_SHOW_BINLOG_EVENTS ||
                thd_sql_command(thd) == SQLCOM_FLUSH ||
                thd_sql_command(thd) == SQLCOM_RESET);
    /*
      Binlog Injector should not wait for itself
    */
    if (thd->system_thread == SYSTEM_THREAD_NDBCLUSTER_BINLOG)
      DBUG_VOID_RETURN;

    Thd_ndb *thd_ndb = get_thd_ndb(thd);
    if (!thd_ndb)
    {
      /*
       thd has not interfaced with ndb before
       so there is no need for waiting
      */
       DBUG_VOID_RETURN;
    }

    const char *save_info = thd->proc_info;
    thd->proc_info = "Waiting for ndbcluster binlog update to "
	"reach current position";

   /*
     Highest epoch that a transaction against Ndb has received
     as part of commit processing *in this thread*. This is a
     per-session 'most recent change' indicator.
    */
    const Uint64 session_last_committed_epoch =
      thd_ndb->m_last_commit_epoch_session;

    /*
     * Wait until the last committed epoch from the session enters Binlog.
     * Break any possible deadlock after 30s.
     */
    int count = 30;

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
      sql_print_warning("NDB: Thread id %u timed out (30s) waiting for epoch %u/%u "
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
    
    thd->proc_info= save_info;
    DBUG_VOID_RETURN;
  }
}

/*
 Called from MYSQL_BIN_LOG::reset_logs in log.cc when binlog is emptied
*/
static int ndbcluster_reset_logs(THD *thd)
{
  if (!ndb_binlog_running)
    return 0;

  /* only reset master should reset logs */
  if (!((thd->lex->sql_command == SQLCOM_RESET) &&
        (thd->lex->type & REFRESH_MASTER)))
    return 0;

  DBUG_ENTER("ndbcluster_reset_logs");

  /*
    Wait for all events originating from this mysql server has
    reached the binlog before continuing to reset
  */
  ndbcluster_binlog_wait(thd);

  /*
    Truncate mysql.ndb_binlog_index table, if table does not
    exist ignore the error as it is a "consistent" behavior
  */
  Ndb_local_connection mysqld(thd);
  const bool ignore_no_such_table = true;
  if(mysqld.truncate_table(STRING_WITH_LEN("mysql"),
                           STRING_WITH_LEN("ndb_binlog_index"),
                           ignore_no_such_table))
  {
    // Failed to truncate table
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
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

  thd->init_for_queries();
  thd_set_command(thd, COM_DAEMON);
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
     * Will sql_print_warning fail with no thd?
     */
    sql_print_warning("NDB: Unable to purge "
                      NDB_REP_DB "." NDB_REP_TABLE
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
  my_thd->variables.tx_isolation= ISO_REPEATABLE_READ;
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
      sql_print_error("NDB: Inconsistency detected in distributed "
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
ndbcluster_binlog_log_query(handlerton *hton, THD *thd,
                            enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char *table_name)
{
  DBUG_ENTER("ndbcluster_binlog_log_query");
  DBUG_PRINT("enter", ("db: %s  table_name: %s  query: %s",
                       db, table_name, query));

  DBUG_EXECUTE_IF("ndb_binlog_random_tableid",
  {
    /**
     * Simulate behaviour immediately after mysql_main() init:
     *   We do *not* set the random seed, which according to 'man rand'
     *   is equivalent of setting srand(1). In turn this will result
     *   in the same sequence of random numbers being produced on all mysqlds.
     */ 
    srand(1);
  });

  enum SCHEMA_OP_TYPE type;
  /**
   * Don't have any table_id/_version to uniquely identify the 
   *  schema operation. Set the special values 0/0 which allows
   *  ndbcluster_log_schema_op() to produce its own unique ids.
   */
  const uint32 table_id= 0, table_version= 0;
  switch (binlog_command)
  {
  case LOGCOM_CREATE_DB:
    DBUG_PRINT("info", ("New database '%s' created", db));
    type= SOT_CREATE_DB;
    break;

  case LOGCOM_ALTER_DB:
    DBUG_PRINT("info", ("The database '%s' was altered", db));
    type= SOT_ALTER_DB;
    break;

  case LOGCOM_ACL_NOTIFY:
    DBUG_PRINT("info", ("Privilege tables have been modified"));
    type= SOT_GRANT;
    if (!Ndb_dist_priv_util::priv_tables_are_in_ndb(thd))
    {
      DBUG_VOID_RETURN;
    }
    /*
      NOTE! Grant statements with db set to NULL is very rare but
      may be provoked by for example dropping the currently selected
      database. Since ndbcluster_log_schema_op does not allow
      db to be NULL(can't create a key for the ndb_schem_object nor
      writeNULL to ndb_schema), the situation is salvaged by setting db
      to the constant string "mysql" which should work in most cases.

      Interestingly enough this "hack" has the effect that grant statements
      are written to the remote binlog in same format as if db would have
      been NULL.
    */
    if (!db)
      db = "mysql";
    break;    

  default:
    DBUG_PRINT("info", ("Ignoring binlog_log_query notification"));
    DBUG_VOID_RETURN;
    break;

  }
  ndbcluster_log_schema_op(thd, query, query_length,
                           db, table_name, table_id, table_version, type,
                           NULL, NULL);
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

/*
  Initialize the binlog part of the ndb handlerton
*/

static int ndbcluster_binlog_func(handlerton *hton, THD *thd, 
                                  enum_binlog_func fn, 
                                  void *arg)
{
  DBUG_ENTER("ndbcluster_binlog_func");
  int res= 0;
  switch(fn)
  {
  case BFN_RESET_LOGS:
    res= ndbcluster_reset_logs(thd);
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


static bool
create_cluster_sys_table(THD *thd, const char* db, size_t db_length,
                         const char* table, size_t table_length,
                         const char* create_definitions,
                         const char* create_options)
{
  /* Need a connection to create table, else retry later. */
  if (g_ndb_cluster_connection->get_no_ready() <= 0)
    return true; 

  if (opt_ndb_extra_logging)
    sql_print_information("NDB: Creating %s.%s", db, table);

  Ndb_local_connection mysqld(thd);

  /*
    Check if table exists in MySQL "dictionary"(i.e on disk)
    if so, remove it since there is none in Ndb
  */
  {
    char path[FN_REFLEN + 1];
    build_table_filename(path, sizeof(path) - 1,
                         db, table, reg_ext, 0);
    if (my_delete(path, MYF(0)) == 0)
    {
      /*
        The .frm file existed and was deleted from disk.
        It's possible that someone has tried to use it and thus
        it might have been inserted in the table definition cache.
        It must be flushed to avoid that it exist only in the
        table definition cache.
      */
      if (opt_ndb_extra_logging)
        sql_print_information("NDB: Flushing %s.%s", db, table);

      /* Flush mysql.ndb_apply_status table, ignore all errors */
      (void)mysqld.flush_table(db, db_length,
                               table, table_length);
    }
  }

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


/**
  Utility class encapsulating the code which setup the 'ndb binlog thread'
  to be "connected" to the cluster.
  This involves:
   - synchronizing the local mysqld data dictionary with that in NDB
   - subscribing to changes that happen in NDB, thus allowing:
    -- local mysqld data dictionary to be kept in synch
    -- binlog of changes in NDB to be written

*/

class Ndb_binlog_setup {

  THD* const m_thd;
  Thd_ndb* const m_thd_ndb;

/*
  Clean-up any stray files for non-existing NDB tables
  - "stray" means that there is a .frm + .ndb file on disk
    but there exists no such table in NDB. The two files
    can then be deleted from disk to get in synch with
    what's in NDB.
*/
static
void clean_away_stray_files(THD *thd, Thd_ndb* thd_ndb)
{
  DBUG_ENTER("Ndb_binlog_setup::clean_away_stray_files");

  // Populate list of databases
  Ndb_find_files_list db_names(thd);
  if (!db_names.find_databases(mysql_data_home))
  {
    thd->clear_error();
    DBUG_PRINT("info", ("Failed to find databases"));
    DBUG_VOID_RETURN;
  }

  LEX_STRING *db_name;
  while ((db_name= db_names.next()))
  {
    DBUG_PRINT("info", ("Found database %s", db_name->str));
    if (strcmp(NDB_REP_DB, db_name->str)) /* Skip system database */
    {

      sql_print_information("NDB: Cleaning stray tables from database '%s'",
                            db_name->str);

      char path[FN_REFLEN + 1];
      build_table_filename(path, sizeof(path) - 1, db_name->str, "", "", 0);
      
      /* Require that no binlog setup is attempted yet, that will come later
       * right now we just want to get rid of stray frms et al
       */
      Thd_ndb::Options_guard thd_ndb_options(thd_ndb);
      thd_ndb_options.set(Thd_ndb::SKIP_BINLOG_SETUP_IN_FIND_FILES);

      Ndb_find_files_list tab_names(thd);
      if (!tab_names.find_tables(db_name->str, path))
      {
        thd->clear_error();
        DBUG_PRINT("info", ("Failed to find tables"));
      }
    }
  }
  DBUG_VOID_RETURN;
}

/*
  Ndb has no representation of the database schema objects.
  The mysql.ndb_schema table contains the latest schema operations
  done via a mysqld, and thus reflects databases created/dropped/altered
  while a mysqld was disconnected.  This function tries to recover
  the correct state w.r.t created databases using the information in
  that table.
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
            sql_print_information("NDB: Discovered missing database '%s'", db);
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
            sql_print_information("NDB: Discovered missing database '%s'", db);
            const int no_print_error[1]= {0};
            name_len= (unsigned)my_snprintf(name, sizeof(name), "CREATE DATABASE %s", db);
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
            sql_print_information("NDB: Discovered remaining database '%s'", db);
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
        sql_print_warning("NDB: ndbcluster_find_all_databases retry: %u - %s",
                          ndb_error.code,
                          ndb_error.message);
        do_retry_sleep(retry_sleep);
        continue; // retry
      }
    }
    if (!thd->killed)
    {
      sql_print_error("NDB: ndbcluster_find_all_databases fail: %u - %s",
                      ndb_error.code,
                      ndb_error.message);
    }

    DBUG_RETURN(1); // not temp error or too many retries
  }
}


/*
  find all tables in ndb and discover those needed
*/
static
int find_all_files(THD *thd, Ndb* ndb)
{
  char key[FN_REFLEN + 1];
  int unhandled= 0, retries= 5, skipped= 0;
  DBUG_ENTER("Ndb_binlog_setup::find_all_files");

  NDBDICT* dict= ndb->getDictionary();

  do
  {
    NdbDictionary::Dictionary::List list;
    if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
      DBUG_RETURN(1);
    unhandled= 0;
    skipped= 0;
    retries--;
    for (uint i= 0 ; i < list.count ; i++)
    {
      NDBDICT::List::Element& elmt= list.elements[i];
      if (IS_TMP_PREFIX(elmt.name) || IS_NDB_BLOB_PREFIX(elmt.name))
      {
        DBUG_PRINT("info", ("Skipping %s.%s in NDB", elmt.database, elmt.name));
        continue;
      }
      DBUG_PRINT("info", ("Found %s.%s in NDB", elmt.database, elmt.name));
      if (elmt.state != NDBOBJ::StateOnline &&
          elmt.state != NDBOBJ::StateBackup &&
          elmt.state != NDBOBJ::StateBuilding)
      {
        sql_print_information("NDB: skipping setup table %s.%s, in state %d",
                              elmt.database, elmt.name, elmt.state);
        skipped++;
        continue;
      }

      ndb->setDatabaseName(elmt.database);
      Ndb_table_guard ndbtab_g(dict, elmt.name);
      const NDBTAB *ndbtab= ndbtab_g.get_table();
      if (!ndbtab)
      {
        if (retries == 0)
          sql_print_error("NDB: failed to setup table %s.%s, error: %d, %s",
                          elmt.database, elmt.name,
                          dict->getNdbError().code,
                          dict->getNdbError().message);
        unhandled++;
        continue;
      }

      if (ndbtab->getFrmLength() == 0)
        continue;

      /* check if database exists */
      char *end= key +
        build_table_filename(key, sizeof(key) - 1, elmt.database, "", "", 0);
      if (my_access(key, F_OK))
      {
        /* no such database defined, skip table */
        continue;
      }
      /* finalize construction of path */
      end+= tablename_to_filename(elmt.name, end,
                                  (uint)(sizeof(key)-(end-key)));
      uchar *data= 0, *pack_data= 0;
      size_t length, pack_length;
      int discover= 0;
      if (readfrm(key, &data, &length) ||
          packfrm(data, length, &pack_data, &pack_length))
      {
        discover= 1;
        sql_print_information("NDB: missing frm for %s.%s, discovering...",
                              elmt.database, elmt.name);
      }
      else if (cmp_frm(ndbtab, pack_data, pack_length))
      {
        /* ndb_share reference temporary */
        NDB_SHARE *share= get_share(key, 0, FALSE);
        if (share)
        {
          DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                                   share->key_string(), share->use_count));
        }
        if (!share || get_ndb_share_state(share) != NSS_ALTERED)
        {
          discover= 1;
          sql_print_information("NDB: mismatch in frm for %s.%s,"
                                " discovering...",
                                elmt.database, elmt.name);
        }
        if (share)
        {
          /* ndb_share reference temporary free */
          DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                   share->key_string(), share->use_count));
          free_share(&share);  // temporary ref.
        }
      }
      my_free(data);
      my_free(pack_data);

      if (discover)
      {
        /* ToDo 4.1 database needs to be created if missing */
        if (ndb_create_table_from_engine(thd, elmt.database, elmt.name))
        {
          /* ToDo 4.1 handle error */
        }
      }
      else
      {
        /* set up replication for this table */
        if (ndbcluster_create_binlog_setup(thd, ndb, key,
                                           elmt.database, elmt.name, NULL))
        {
          unhandled++;
          continue;
        }
      }
    }
  }
  while (unhandled && retries);

  DBUG_RETURN(-(skipped + unhandled));
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
  DBUG_EXECUTE_IF("ndb_binlog_setup_slow",
  {
    sql_print_information("ndb_binlog_setup: 'ndb_binlog_setup_slow' -> sleep");
    NdbSleep_SecSleep(10);
    sql_print_information("ndb_binlog_setup <- sleep");
  });

  while (true) //To allow 'break' out to error handling
  {
    DBUG_ASSERT(ndb_schema_share == NULL);
    DBUG_ASSERT(ndb_apply_status_share == NULL);

    /**
     * The Global Schema Lock (GSL) protects the discovery of the tables,
     * and creation of the schema change distribution event (ndb_schema_share)
     * to be atomic. This make sure that the schema does not change without 
     * being distributed to other mysqld's.
     */
    Ndb_global_schema_lock_guard global_schema_lock_guard(m_thd);
    if (global_schema_lock_guard.lock(false))
    {
      break;
    }

    /* Give additional 'binlog_setup rights' to this Thd_ndb */
    Thd_ndb::Options_guard thd_ndb_options(m_thd_ndb);
    thd_ndb_options.set(Thd_ndb::ALLOW_BINLOG_SETUP);

    if (ndb_create_table_from_engine(m_thd, NDB_REP_DB, NDB_SCHEMA_TABLE))
    {
      if (ndb_schema_table__create(m_thd))
        break;
    }
    if (ndb_schema_share == NULL)  //Needed for 'ndb_schema_dist_is_ready()'
      break;  

    /**
     * NOTE: At this point the creation of 'ndb_schema_share' has set
     * ndb_schema_dist_is_ready(), which also announced our subscription
     * (and handling) of schema change events.
     * We are excpected to act on any such changes (SLOCK) by all other mysqld.
     * However, this is not possible yet until setup has succesfully
     * completed, and our binlog-thread started to handle events.
     * Thus, if we fail to complete the setup below, the schema changes *must*
     * be unsubscribed as part of error handling. Any other mysqld's waiting
     * for us to reply, will then get an unsibscribe-event instead, which breaks
     * the wait.
     */
    assert(ndb_schema_dist_is_ready());

    /* Test handling of binlog_setup failing to complete *after* created 'ndb_schema' */
    DBUG_EXECUTE_IF("ndb_binlog_setup_incomplete",
    {
      sql_print_information("ndb_binlog_setup: 'ndb_binlog_setup_incomplete' -> return");
      break;
    });

    if (ndb_create_table_from_engine(m_thd, NDB_REP_DB, NDB_APPLY_TABLE))
    {
      if (ndb_apply_table__create(m_thd))
        break;
    }
    /* Note: Failure of creating APPLY_TABLE eventOp is retried
       by find_all_files(), and eventually failed.
    */

    clean_away_stray_files(m_thd, m_thd_ndb);

    if (find_all_databases(m_thd, m_thd_ndb))
      break;

    if (find_all_files(m_thd, m_thd_ndb->ndb))
      break;

    /* Shares w/ eventOp subscr. for NDB_SCHEMA_TABLE and NDB_APPLY_TABLE created? */
    DBUG_ASSERT(ndb_schema_share);
    DBUG_ASSERT(!ndb_binlog_running || ndb_apply_status_share);

    Mutex_guard injector_mutex_g(injector_data_mutex);
    ndb_binlog_tables_inited= TRUE;
    return true;     // Setup completed -> OK
  } //end global schema lock

  /**
   * Error handling:
   * Failed to complete ndb_binlog_setup.
   * Remove all existing event operations from a possible partial setup
   */
  if (ndb_schema_dist_is_ready()) //Can't leave failed setup with 'dist_is_ready'
  {
    sql_print_information("ndb_binlog_setup: Clean up leftovers");
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
  Defines and struct for schema table.
  Should reflect table definition above.
*/
#define SCHEMA_DB_I 0u
#define SCHEMA_NAME_I 1u
#define SCHEMA_SLOCK_I 2u
#define SCHEMA_QUERY_I 3u
#define SCHEMA_NODE_ID_I 4u
#define SCHEMA_EPOCH_I 5u
#define SCHEMA_ID_I 6u
#define SCHEMA_VERSION_I 7u
#define SCHEMA_TYPE_I 8u
#define SCHEMA_SIZE 9u
#define SCHEMA_SLOCK_SIZE 32u


/*
  log query in schema table
*/
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
    sql_print_information("NDB %s:"
                          " waiting max %u sec for %s %s."
                          "  epochs: (%u/%u,%u/%u,%u/%u)"
                          "  injector proc_info: %s"
                          ,key, the_time, op, obj
                          ,(uint)(ndb_latest_handled_binlog_epoch >> 32)
                          ,(uint)(ndb_latest_handled_binlog_epoch)
                          ,(uint)(ndb_latest_received_binlog_epoch >> 32)
                          ,(uint)(ndb_latest_received_binlog_epoch)
                          ,(uint)(ndb_latest_epoch >> 32)
                          ,(uint)(ndb_latest_epoch)
                          ,proc_info
                          );
  }
  else
  {
    sql_print_information("NDB %s:"
                          " waiting max %u sec for %s %s."
                          "  epochs: (%u/%u,%u/%u,%u/%u)"
                          "  injector proc_info: %s map: %x%08x"
                          ,key, the_time, op, obj
                          ,(uint)(ndb_latest_handled_binlog_epoch >> 32)
                          ,(uint)(ndb_latest_handled_binlog_epoch)
                          ,(uint)(ndb_latest_received_binlog_epoch >> 32)
                          ,(uint)(ndb_latest_received_binlog_epoch)
                          ,(uint)(ndb_latest_epoch >> 32)
                          ,(uint)(ndb_latest_epoch)
                          ,proc_info
                          ,map->bitmap[1]
                          ,map->bitmap[0]
                          );
  }
}


extern void update_slave_api_stats(Ndb*);

int ndbcluster_log_schema_op(THD *thd,
                             const char *query, int query_length,
                             const char *db, const char *table_name,
                             uint32 ndb_table_id,
                             uint32 ndb_table_version,
                             enum SCHEMA_OP_TYPE type,
                             const char *new_db, const char *new_table_name,
                             bool log_query_on_participant)
{
  DBUG_ENTER("ndbcluster_log_schema_op");
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb)
  {
    if (!(thd_ndb= Thd_ndb::seize(thd)))
    {
      sql_print_error("Could not allocate Thd_ndb object");
      DBUG_RETURN(1);
    }
    thd_set_thd_ndb(thd, thd_ndb);
  }

  DBUG_PRINT("enter", ("query: %s  db: %s  table_name: %s",
                       query, db, table_name));
  if (!ndb_schema_share ||
      thd_ndb->check_option(Thd_ndb::NO_LOG_SCHEMA_OP))
  {
    if (thd->slave_thread)
      update_slave_api_stats(thd_ndb->ndb);

    DBUG_RETURN(0);
  }

  /* Check that the database name will fit within limits */
  if(strlen(db) > NDB_MAX_DDL_NAME_BYTESIZE)
  {
    // Catch unexpected commands with too long db length
    DBUG_ASSERT(type == SOT_CREATE_DB ||
                type == SOT_ALTER_DB ||
                type == SOT_DROP_DB);
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TOO_LONG_IDENT,
                        "Ndb has an internal limit of %u bytes on the size of schema identifiers",
                        NDB_MAX_DDL_NAME_BYTESIZE);
    DBUG_RETURN(ER_TOO_LONG_IDENT);
  }

  char tmp_buf2[FN_REFLEN];
  char quoted_table1[2 + 2 * FN_REFLEN + 1];
  char quoted_db1[2 + 2 * FN_REFLEN + 1];
  char quoted_db2[2 + 2 * FN_REFLEN + 1];
  char quoted_table2[2 + 2 * FN_REFLEN + 1];
  size_t id_length= 0;
  const char *type_str;
  uint32 log_type= (uint32)type;
  switch (type)
  {
  case SOT_DROP_TABLE:
    /* drop database command, do not log at drop table */
    if (thd->lex->sql_command ==  SQLCOM_DROP_DB)
      DBUG_RETURN(0);
    /*
      Rewrite the drop table query as it may contain several tables
      but drop_table() is called once for each table in the query
      ie. DROP TABLE t1, t2;
          -> DROP TABLE t1 + DROP TABLE t2
    */

    query= tmp_buf2;
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_table1,
                                            table_name, 0);
    quoted_table1[id_length]= '\0';
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_db1,
                                            db, 0);
    quoted_db1[id_length]= '\0';
    query_length= (uint) (strxmov(tmp_buf2, "drop table ", quoted_db1, ".",
                                  quoted_table1, NullS) - tmp_buf2);
    type_str= "drop table";
    break;
  case SOT_RENAME_TABLE_PREPARE:
    type_str= "rename table prepare";
    break;
  case SOT_RENAME_TABLE:
    /*
      Rewrite the rename table query as it may contain several tables
      but rename_table() is called once for each table in the query
      ie. RENAME TABLE t1 to tx, t2 to ty;
          -> RENAME TABLE t1 to tx + RENAME TABLE t2 to ty
    */
    query= tmp_buf2;
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_db1,
                                            db, 0);
    quoted_db1[id_length]= '\0';
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_table1,
                                            table_name, 0);
    quoted_table1[id_length]= '\0';
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_db2,
                                            new_db, 0);
    quoted_db2[id_length]= '\0';
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_table2,
                                            new_table_name, 0);
    quoted_table2[id_length]= '\0';
    query_length= (uint) (strxmov(tmp_buf2, "rename table ",
                                  quoted_db1, ".", quoted_table1, " to ",
                                  quoted_db2, ".", quoted_table2, NullS) - tmp_buf2);
    type_str= "rename table";
    break;
  case SOT_CREATE_TABLE:
    type_str= "create table";
    break;
  case SOT_ALTER_TABLE_COMMIT:
    type_str= "alter table";
    break;
  case SOT_ONLINE_ALTER_TABLE_PREPARE:
    type_str= "online alter table prepare";
    break;
  case SOT_ONLINE_ALTER_TABLE_COMMIT:
    type_str= "online alter table commit";
    break;
  case SOT_DROP_DB:
    type_str= "drop db";
    break;
  case SOT_CREATE_DB:
    type_str= "create db";
    break;
  case SOT_ALTER_DB:
    type_str= "alter db";
    break;
  case SOT_TABLESPACE:
    type_str= "tablespace";
    break;
  case SOT_LOGFILE_GROUP:
    type_str= "logfile group";
    break;
  case SOT_TRUNCATE_TABLE:
    type_str= "truncate table";
    break;
  case SOT_CREATE_USER:
    type_str= "create user";
    break;
  case SOT_DROP_USER:
    type_str= "drop user";
    break;
  case SOT_RENAME_USER:
    type_str= "rename user";
    break;
  case SOT_GRANT:
    type_str= "grant/revoke";
    break;
  case SOT_REVOKE:
    type_str= "revoke all";
    break;
  default:
    abort(); /* should not happen, programming error */
  }

  // Use nodeid of the primary cluster connection since that is
  // the nodeid which the coordinator and participants listen to
  const uint32 node_id= g_ndb_cluster_connection->node_id();

  /**
   * If table_id/_version is not specified, we have to produce
   * our own unique identifier for the schema operation.
   * Use a sequence counter and own node_id for uniqueness.
   */
  if (ndb_table_id == 0 && ndb_table_version == 0)
  {
    static uint32 seq_id = 0;
    mysql_mutex_lock(&ndbcluster_mutex);
    ndb_table_id = ++seq_id;
    ndb_table_version = node_id;
    mysql_mutex_unlock(&ndbcluster_mutex);
  }

  NDB_SCHEMA_OBJECT *ndb_schema_object;
  {
    char key[FN_REFLEN + 1];
    build_table_filename(key, sizeof(key) - 1, db, table_name, "", 0);
    ndb_schema_object= ndb_get_schema_object(key, true);

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

    DBUG_EXECUTE_IF("ndb_binlog_random_tableid",
    {
      /**
       * Try to trigger a race between late incomming slock ack for
       * schema operations having its coordinator on another node,
       * which we would otherwise have discarded as no matching
       * ndb_schema_object existed, and another schema op with same 'key',
       * coordinated by this node. Thus causing a mixup betweeen these,
       * and the schema distribution getting totally out of synch.
       */
      NdbSleep_MilliSleep(50);
    });
  }

  const NdbError *ndb_error= 0;
  Uint64 epoch= 0;
  {
    /* begin protect ndb_schema_share */
    Mutex_guard ndb_schema_share_g(injector_data_mutex);
    if (ndb_schema_share == NULL)
    {
      ndb_free_schema_object(&ndb_schema_object);
      DBUG_RETURN(0);
    }
  }

  Ndb *ndb= thd_ndb->ndb;
  char save_db[FN_REFLEN];
  strcpy(save_db, ndb->getDatabaseName());

  char tmp_buf[FN_REFLEN];
  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(NDB_REP_DB);
  Ndb_table_guard ndbtab_g(dict, NDB_SCHEMA_TABLE);
  const NDBTAB *ndbtab= ndbtab_g.get_table();
  NdbTransaction *trans= 0;
  int retries= 100;
  int retry_sleep= 30; /* 30 milliseconds, transaction */
  const NDBCOL *col[SCHEMA_SIZE];
  unsigned sz[SCHEMA_SIZE];

  if (ndbtab == 0)
  {
    if (strcmp(NDB_REP_DB, db) != 0 ||
        strcmp(NDB_SCHEMA_TABLE, table_name))
    {
      ndb_error= &dict->getNdbError();
    }
    goto end;
  }

  {
    uint i;
    for (i= 0; i < SCHEMA_SIZE; i++)
    {
      col[i]= ndbtab->getColumn(i);
      if (i != SCHEMA_QUERY_I)
      {
        sz[i]= col[i]->getLength();
        DBUG_ASSERT(sz[i] <= sizeof(tmp_buf));
      }
    }
  }

  while (1)
  {
    const char *log_db= db;
    const char *log_tab= table_name;
    const char *log_subscribers= (char*)ndb_schema_object->slock;
    if ((trans= ndb->startTransaction()) == 0)
      goto err;
    while (1)
    {
      NdbOperation *op= 0;
      int r= 0;
      r|= (op= trans->getNdbOperation(ndbtab)) == 0;
      DBUG_ASSERT(r == 0);
      r|= op->writeTuple();
      DBUG_ASSERT(r == 0);
      
      /* db */
      ndb_pack_varchar(col[SCHEMA_DB_I], tmp_buf, log_db, (int)strlen(log_db));
      r|= op->equal(SCHEMA_DB_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* name */
      ndb_pack_varchar(col[SCHEMA_NAME_I], tmp_buf, log_tab,
                       (int)strlen(log_tab));
      r|= op->equal(SCHEMA_NAME_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* slock */
      DBUG_ASSERT(sz[SCHEMA_SLOCK_I] ==
                  no_bytes_in_map(&ndb_schema_object->slock_bitmap));
      r|= op->setValue(SCHEMA_SLOCK_I, log_subscribers);
      DBUG_ASSERT(r == 0);
      /* query */
      {
        NdbBlob *ndb_blob= op->getBlobHandle(SCHEMA_QUERY_I);
        DBUG_ASSERT(ndb_blob != 0);
        uint blob_len= query_length;
        const char* blob_ptr= query;
        r|= ndb_blob->setValue(blob_ptr, blob_len);
        DBUG_ASSERT(r == 0);
      }
      /* node_id */
      r|= op->setValue(SCHEMA_NODE_ID_I, node_id);
      DBUG_ASSERT(r == 0);
      /* epoch */
      r|= op->setValue(SCHEMA_EPOCH_I, epoch);
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
      if (! thd->slave_thread)
      {
        /* Schema change originating from this MySQLD, check SQL_LOG_BIN
         * variable and pass 'setting' to all logging MySQLDs via AnyValue  
         */
        if (thd_options(thd) & OPTION_BIN_LOG) /* e.g. SQL_LOG_BIN == on */
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
                            thd->server_id));
        anyValue = thd_unmasked_server_id(thd);
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
      break;
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
    if (this_error->status == NdbError::TemporaryError && !thd->killed)
    {
      if (retries--)
      {
        if (trans)
          ndb->closeTransaction(trans);
        do_retry_sleep(retry_sleep);
        continue; // retry
      }
    }
    ndb_error= this_error;
    break;
  }
end:
  if (ndb_error)
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        ndb_error->code,
                        ndb_error->message,
                        "Could not log query '%s' on other mysqld's");
          
  if (trans)
    ndb->closeTransaction(trans);
  ndb->setDatabaseName(save_db);

  if (opt_ndb_extra_logging > 19)
  {
    sql_print_information("NDB: distributed %s.%s(%u/%u) type: %s(%u) query: \'%s\' to %x%08x",
                          db,
                          table_name,
                          ndb_table_id,
                          ndb_table_version,
                          get_schema_type_name(log_type),
                          log_type,
                          query,
                          ndb_schema_object->slock_bitmap.bitmap[1],
                          ndb_schema_object->slock_bitmap.bitmap[0]);
  }

  /*
    Wait for other mysqld's to acknowledge the table operation
  */
  if (unlikely(ndb_error))
  {
    sql_print_error("NDB %s: distributing %s err: %u",
                    type_str, ndb_schema_object->key,
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

      if (thd->killed)
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
          sql_print_error("NDB %s: distributing %s timed out. Ignoring...",
                          type_str, ndb_schema_object->key);
          DBUG_ASSERT(false);
          break;
        }
        if (opt_ndb_extra_logging)
          ndb_report_waiting(type_str, max_timeout,
                             "distributing", ndb_schema_object->key,
                             &ndb_schema_object->slock_bitmap);
      }
    }
    mysql_mutex_unlock(&ndb_schema_object->mutex);
  }
  else if (opt_ndb_extra_logging > 19)
  {
    sql_print_information("NDB %s: not waiting for distributing %s",
                          type_str, ndb_schema_object->key);
  }

  ndb_free_schema_object(&ndb_schema_object);

  if (opt_ndb_extra_logging > 19)
  {
    sql_print_information("NDB: distribution of %s.%s(%u/%u) type: %s(%u) query: \'%s\'"
                          " - complete!",
                          db,
                          table_name,
                          ndb_table_id,
                          ndb_table_version,
                          get_schema_type_name(log_type),
                          log_type,
                          query);
  }

  if (thd->slave_thread)
    update_slave_api_stats(ndb);

  DBUG_RETURN(0);
}


/*
  ndb_handle_schema_change

  Used when an event has been receieved telling that the table has been
  dropped or connection to cluster has failed. Function checks if the
  table needs to be removed from any of the many places where it's
  referenced or cached, finally the EventOperation is dropped and
  the event_data structure is released.

  The function may be called either by Ndb_schema_event_handler which
  listens to events only on mysql.ndb_schema or by the "injector" which
  listen to events on all the other tables.

*/

static
int
ndb_handle_schema_change(THD *thd, Ndb *is_ndb, NdbEventOperation *pOp,
                         const Ndb_event_data *event_data)
{
  DBUG_ENTER("ndb_handle_schema_change");
  DBUG_PRINT("enter", ("pOp: %p", pOp));

  // Only called for TE_DROP and TE_CLUSTER_FAILURE event
  DBUG_ASSERT(pOp->getEventType() == NDBEVENT::TE_DROP ||
              pOp->getEventType() == NDBEVENT::TE_CLUSTER_FAILURE);

  DBUG_ASSERT(event_data);
  DBUG_ASSERT(pOp->getCustomData() == event_data);


  NDB_SHARE *share= event_data->share;
  dbug_print_share("changed share: ", share);

  TABLE *shadow_table= event_data->shadow_table;
  const char *tabname= shadow_table->s->table_name.str;
  const char *dbname= shadow_table->s->db.str;
  {
    Thd_ndb *thd_ndb= get_thd_ndb(thd);
    Ndb *ndb= thd_ndb->ndb;
    NDBDICT *dict= ndb->getDictionary();
    ndb->setDatabaseName(dbname);
    Ndb_table_guard ndbtab_g(dict, tabname);
    const NDBTAB *ev_tab= pOp->getTable();
    const NDBTAB *cache_tab= ndbtab_g.get_table();
    if (cache_tab &&
        cache_tab->getObjectId() == ev_tab->getObjectId() &&
        cache_tab->getObjectVersion() <= ev_tab->getObjectVersion())
      ndbtab_g.invalidate();
  }

  mysql_mutex_lock(&share->mutex);
  DBUG_ASSERT(share->state == NSS_DROPPED || 
              share->op == pOp || share->new_op == pOp);
  share->new_op= NULL;
  share->op= NULL;
  mysql_mutex_unlock(&share->mutex);

  /* Signal ha_ndbcluster::delete/rename_table that drop is done */
  DBUG_PRINT("info", ("signal that drop is done"));
  mysql_cond_broadcast(&injector_data_cond);

  ndb_tdc_close_cached_table(thd, dbname, tabname);

  mysql_mutex_lock(&ndbcluster_mutex);
  const bool is_remote_change= !ndb_has_node_id(pOp->getReqNodeId());
  if (is_remote_change && share->state != NSS_DROPPED)
  {
    /* Mark share as DROPPED will free the ref from list of open_tables */
    DBUG_PRINT("info", ("remote change"));
    ndbcluster_mark_share_dropped(&share);
    DBUG_ASSERT(share != NULL);
  }

  /* ndb_share reference binlog free */
  DBUG_PRINT("NDB_SHARE", ("%s binlog free  use_count: %u",
                           share->key_string(), share->use_count));
  free_share(&share, TRUE);
  mysql_mutex_unlock(&ndbcluster_mutex);

  // Remove pointer to event_data from the EventOperation
  pOp->setCustomData(NULL);

  DBUG_PRINT("info", ("Dropping event operation: %p", pOp));
  mysql_mutex_lock(&injector_event_mutex);
  is_ndb->dropEventOperation(pOp);
  mysql_mutex_unlock(&injector_event_mutex);

  // Finally delete the event_data and thus it's mem_root, shadow_table etc.
  DBUG_PRINT("info", ("Deleting event_data"));
  delete event_data;

  DBUG_RETURN(0);
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
                    max_ndb_nodes, FALSE);
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
  }

  void report_data_node_failure(unsigned data_node_id)
  {
    uint8 idx= map2subscriber_bitmap_index(data_node_id);
    bitmap_clear_all(&subscriber_bitmap[idx]);
    DBUG_PRINT("info",("Data node %u failure", data_node_id));
    if (opt_ndb_extra_logging)
    {
      sql_print_information("NDB Schema dist: Data node: %d failed,"
                            " subscriber bitmask %x%08x",
                            data_node_id,
                            subscriber_bitmap[idx].bitmap[1],
                            subscriber_bitmap[idx].bitmap[0]);
    }
    check_wakeup_clients();
  }

  void report_subscribe(unsigned data_node_id, unsigned subscriber_node_id)
  {
    uint8 idx= map2subscriber_bitmap_index(data_node_id);
    DBUG_ASSERT(subscriber_node_id != 0);
    bitmap_set_bit(&subscriber_bitmap[idx], subscriber_node_id);
    DBUG_PRINT("info",("Data node %u reported node %u subscribed ",
                       data_node_id, subscriber_node_id));
    if (opt_ndb_extra_logging)
    {
      sql_print_information("NDB Schema dist: Data node: %d reports "
                            "subscribe from node %d, subscriber bitmask %x%08x",
                            data_node_id,
                            subscriber_node_id,
                            subscriber_bitmap[idx].bitmap[1],
                            subscriber_bitmap[idx].bitmap[0]);
    }
    //No 'wakeup_clients' now, as *adding* subscribers didn't complete anything
  }

  void report_unsubscribe(unsigned data_node_id, unsigned subscriber_node_id)
  {
    uint8 idx= map2subscriber_bitmap_index(data_node_id);
    DBUG_ASSERT(subscriber_node_id != 0);
    bitmap_clear_bit(&subscriber_bitmap[idx], subscriber_node_id);
    DBUG_PRINT("info",("Data node %u reported node %u unsubscribed ",
                       data_node_id, subscriber_node_id));
    if (opt_ndb_extra_logging)
    {
      sql_print_information("NDB Schema dist: Data node: %d reports "
                            "unsubscribe from node %d, subscriber bitmask %x%08x",
                            data_node_id,
                            subscriber_node_id,
                            subscriber_bitmap[idx].bitmap[1],
                            subscriber_bitmap[idx].bitmap[0]);
    }
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
    bitmap_init(&participants, participants_buf, 256, FALSE);
    get_subscriber_bitmask(&participants);

    // Check all Client's for wakeup
    NDB_SCHEMA_OBJECT::check_waiters(participants);
  }

}; //class Ndb_schema_dist_data


#include "ndb_local_schema.h"

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
          DBUG_ASSERT(FALSE);
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
                  schema_op->slock_buf, 8*SCHEMA_SLOCK_SIZE, FALSE);
      schema_op->unpack_event(event_data);
      schema_op->any_value= any_value;
      DBUG_PRINT("exit", ("%s.%s: query: '%s'  type: %d",
                          schema_op->db, schema_op->name,
                          schema_op->query,
                          schema_op->type));
      DBUG_RETURN(schema_op);
    }
  }; //class Ndb_schema_op

  static void
  print_could_not_discover_error(THD *thd,
                                 const Ndb_schema_op *schema)
  {
    sql_print_error("NDB Binlog: Could not discover table '%s.%s' from "
                    "binlog schema event '%s' from node %d. "
                    "my_errno: %d",
                    schema->db, schema->name, schema->query,
                    schema->node_id, my_errno());
    thd_print_warning_list(thd, "NDB Binlog");
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
        sql_print_warning("NDB: unknown value for binlog signalling 0x%X, "
                          "query not logged",
                          schema->any_value);
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
    const NDBCOL *col[SCHEMA_SIZE];

    MY_BITMAP slock;
    uint32 bitbuf[SCHEMA_SLOCK_SIZE/4];
    bitmap_init(&slock, bitbuf, sizeof(bitbuf)*8, false);

    if (ndbtab == 0)
    {
      if (dict->getNdbError().code != 4009)
        abort();
      DBUG_RETURN(0);
    }

    {
      uint i;
      for (i= 0; i < SCHEMA_SIZE; i++)
      {
        col[i]= ndbtab->getColumn(i);
        if (i != SCHEMA_QUERY_I)
        {
          DBUG_ASSERT(col[i]->getLength() <= (int)sizeof(tmp_buf));
        }
      }
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
        ndb_pack_varchar(col[SCHEMA_DB_I], tmp_buf, db, (int)strlen(db));
        r|= op->equal(SCHEMA_DB_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* name */
        ndb_pack_varchar(col[SCHEMA_NAME_I], tmp_buf, table_name,
                         (int)strlen(table_name));
        r|= op->equal(SCHEMA_NAME_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* slock */
        r|= op->getValue(SCHEMA_SLOCK_I, (char*)slock.bitmap) == 0;
        DBUG_ASSERT(r == 0);
      }
      if (trans->execute(NdbTransaction::NoCommit))
        goto err;

      char before_slock[32];
      if (unlikely(opt_ndb_extra_logging > 19))
      {
        /* Format 'before slock' into temp string */
        my_snprintf(before_slock, sizeof(before_slock), "%x%08x",
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

      if (unlikely(opt_ndb_extra_logging > 19))
      {
        sql_print_information("NDB: reply to %s.%s(%u/%u) from %s to %x%08x",
                              db, table_name,
                              table_id, table_version,
                              before_slock,
                              slock.bitmap[1],
                              slock.bitmap[0]);
      }

      {
        NdbOperation *op= 0;
        int r= 0;

        /* now update the tuple */
        r|= (op= trans->getNdbOperation(ndbtab)) == 0;
        DBUG_ASSERT(r == 0);
        r|= op->updateTuple();
        DBUG_ASSERT(r == 0);

        /* db */
        ndb_pack_varchar(col[SCHEMA_DB_I], tmp_buf, db, (int)strlen(db));
        r|= op->equal(SCHEMA_DB_I, tmp_buf);
        DBUG_ASSERT(r == 0);
        /* name */
        ndb_pack_varchar(col[SCHEMA_NAME_I], tmp_buf, table_name,
                         (int)strlen(table_name));
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
          do_retry_sleep(retry_sleep);
          continue; // retry
        }
      }
      ndb_error= this_error;
      break;
    }

    if (ndb_error)
    {
      sql_print_warning("NDB: Could not release slock on '%s.%s', "
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
    assert(!strncmp(share->db, STRING_WITH_LEN(NDB_REP_DB)));
    assert(!strncmp(share->table_name, STRING_WITH_LEN(NDB_SCHEMA_TABLE)));
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


  void
  mysqld_close_cached_table(const char* db_name, const char* table_name) const
  {
    DBUG_ENTER("mysqld_close_cached_table");
     // Just mark table as "need reopen"
    const bool wait_for_refresh = false;
    // Not waiting -> no timeout needed
    const ulong timeout = 0;

    TABLE_LIST table_list;
    memset(&table_list, 0, sizeof(table_list));
    table_list.db= (char*)db_name;
    table_list.alias= table_list.table_name= (char*)table_name;

    close_cached_tables(m_thd, &table_list,
                        wait_for_refresh, timeout);
    DBUG_VOID_RETURN;
  }

  void
  mysqld_close_cached_tables_referenced_by(const char* db,
                                           const char* table_name) const
  {
    (void)db;
    (void)table_name;
    /* The DDL might have added/dropped/altered a foreign key constraint,
     * the referenced parent table handlers' metadata would be outdated.
     * But finding out the exact parents is not possible at this point.
     * So flush out all tables */
    ndb_tdc_close_cached_tables();
  }


  void
  mysqld_write_frm_from_ndb(const char* db_name,
                            const char* table_name) const
  {
    DBUG_ENTER("mysqld_write_frm_from_ndb");
    Thd_ndb *thd_ndb= get_thd_ndb(m_thd);
    Ndb *ndb= thd_ndb->ndb;
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), table_name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    if (!ndbtab)
    {
      /*
        Bug#14773491 reports crash in 'cmp_frm' due to
        ndbtab* being NULL -> bail out here
      */
      sql_print_error("NDB schema: Could not find table '%s.%s' in NDB",
                      db_name, table_name);
      DBUG_ASSERT(false);
      DBUG_VOID_RETURN;
    }

    char key[FN_REFLEN];
    build_table_filename(key, sizeof(key)-1,
                         db_name, table_name, NullS, 0);

    uchar *data= 0, *pack_data= 0;
    size_t length, pack_length;

    if (readfrm(key, &data, &length) == 0 &&
        packfrm(data, length, &pack_data, &pack_length) == 0 &&
        cmp_frm(ndbtab, pack_data, pack_length))
    {
      DBUG_PRINT("info", ("Detected frm change of table %s.%s",
                          db_name, table_name));

      DBUG_DUMP("frm", (uchar*) ndbtab->getFrmData(),
                        ndbtab->getFrmLength());
      my_free(data);
      data= NULL;

      int error;
      if ((error= unpackfrm(&data, &length,
                            (const uchar*) ndbtab->getFrmData())) ||
          (error= writefrm(key, data, length)))
      {
        sql_print_error("NDB: Failed write frm for %s.%s, error %d",
                        db_name, table_name, error);
      }
    }
    my_free(data);
    my_free(pack_data);
    DBUG_VOID_RETURN;
  }


  NDB_SHARE* get_share(const Ndb_schema_op* schema) const
  {
    DBUG_ENTER("get_share(Ndb_schema_op*)");
    char key[FN_REFLEN + 1];
    build_table_filename(key, sizeof(key) - 1,
                         schema->db, schema->name, "", 0);
    NDB_SHARE *share= ndbcluster_get_share(key, 0, FALSE, FALSE);
    if (share)
    {
      DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                               share->key_string(), share->use_count));
    }
    DBUG_RETURN(share);
  }


  bool
  check_if_local_tables_in_db(const char *dbname) const
  {
    DBUG_ENTER("check_if_local_tables_in_db");
    DBUG_PRINT("info", ("Looking for files in directory %s", dbname));
    Ndb_find_files_list files(m_thd);
    char path[FN_REFLEN + 1];
    build_table_filename(path, sizeof(path) - 1, dbname, "", "", 0);
    if (!files.find_tables(dbname, path))
    {
      m_thd->clear_error();
      DBUG_PRINT("info", ("Failed to find files"));
      DBUG_RETURN(true);
    }
    DBUG_PRINT("info",("found: %d files", files.found_files()));

    LEX_STRING *tabname;
    while ((tabname= files.next()))
    {
      DBUG_PRINT("info", ("Found table %s", tabname->str));
      if (ndbcluster_check_if_local_table(dbname, tabname->str))
        DBUG_RETURN(true);
    }

    DBUG_RETURN(false);
  }


  bool is_local_table(const char* db_name, const char* table_name) const
  {
    return ndbcluster_check_if_local_table(db_name, table_name);
  }


  void handle_clear_slock(const Ndb_schema_op* schema)
  {
    DBUG_ENTER("handle_clear_slock");

    assert(is_post_epoch());

    char key[FN_REFLEN + 1];
    build_table_filename(key, sizeof(key) - 1, schema->db, schema->name, "", 0);

    // Try to create a race between SLOCK acks handled after another
    // schema operation could have been started.
    DBUG_EXECUTE_IF("ndb_binlog_random_tableid",
    {
      NDB_SCHEMA_OBJECT *p= ndb_get_schema_object(key, false);
      if (p == NULL)
      {
        NdbSleep_MilliSleep(10);
      }
      else
      {
        ndb_free_schema_object(&p);
      }
    });

    /* Ack to any SQL thread waiting for schema op to complete */
    NDB_SCHEMA_OBJECT *ndb_schema_object= ndb_get_schema_object(key, false);
    if (!ndb_schema_object)
    {
      /* Noone waiting for this schema op in this mysqld */
      if (opt_ndb_extra_logging > 19)
        sql_print_information("NDB: Discarding event...no obj: %s (%u/%u)",
                              key, schema->id, schema->version);
      DBUG_VOID_RETURN;
    }

    if (ndb_schema_object->table_id != schema->id ||
        ndb_schema_object->table_version != schema->version)
    {
      /* Someone waiting, but for another id/version... */
      if (opt_ndb_extra_logging > 19)
        sql_print_information("NDB: Discarding event...key: %s "
                              "non matching id/version [%u/%u] != [%u/%u]",
                              key,
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
    if (unlikely(opt_ndb_extra_logging > 19))
    {
      /* Format 'before slock' into temp string */
      my_snprintf(before_slock, sizeof(before_slock), "%x%08x",
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

    if (unlikely(opt_ndb_extra_logging > 19))
    {
      /* Print updated slock together with before image of it */
      sql_print_information("NDB: CLEAR_SLOCK key: %s(%u/%u) %x%08x, from %s to %x%08x",
                            key, schema->id, schema->version, 
                            schema->slock_buf[1], schema->slock_buf[0],
                            before_slock,
                            ndb_schema_object->slock[1],
                            ndb_schema_object->slock[0]);
    }

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
    DBUG_EXECUTE_IF("ndb_binlog_schema_object_race",
    {
      NdbSleep_MilliSleep(10);
    });
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
    mysqld_close_cached_table(schema->db, schema->name);
    mysqld_close_cached_tables_referenced_by(schema->db, schema->name);

    /**
     * Note about get_share() / free_share() referrences:
     *
     *  1) All shares have a ref count related to their 'discovery' by dictionary.
     *     Referred from 'ndbcluster_open_tables' until they are 'unrefed'
     *     with ndbcluster_mark_share_dropped().
     *  2) All shares are referred by the binlog thread if its DDL operations 
     *     should be replicated with schema events ('share->op != NULL')
     *     Unref with free_share() when binlog repl for share is removed.
     *  3) All shares are ref counted when they are temporarily referred
     *     inside a function. (as below). Unref with free_share() as last
     *     share related operation when all above has been completed.
     *  4) Each ha_ndbcluster instance may have a share reference (m_share)
     *     until it ::close() the handle, which will unref it with free_share().
     */
    NDB_SHARE *share= get_share(schema);  // 3) Temporary pin 'share'
    if (share)
    {
      mysql_mutex_lock(&share->mutex);
      if (share->op)
      {
        Ndb_event_data *event_data=
          (Ndb_event_data *) share->op->getCustomData();
        if (event_data)
          delete event_data;
        share->op->setCustomData(NULL);
        {
          Mutex_guard injector_mutex_g(injector_event_mutex);
          injector_ndb->dropEventOperation(share->op);
        }
        share->op= 0;
        free_share(&share);   // Free binlog ref, 2)
        DBUG_ASSERT(share);   // Still ref'ed by 1) & 3)
      }
      mysql_mutex_unlock(&share->mutex);

      mysql_mutex_lock(&ndbcluster_mutex);
      ndbcluster_mark_share_dropped(&share); // Unref. from dictionary, 1)
      DBUG_ASSERT(share);                    // Still ref'ed by temp, 3)
      free_share(&share,TRUE);               // Free temporary ref, 3)
      /**
       * If this was the last share ref, it is now deleted.
       * If there are more (trailing) references, the share will remain as an
       * instance in the dropped_tables-hash until remaining references are dropped.
       */
      mysql_mutex_unlock(&ndbcluster_mutex);
    } // if (share)

    if (is_local_table(schema->db, schema->name) &&
       !Ndb_dist_priv_util::is_distributed_priv_table(schema->db,
                                                      schema->name))
    {
      sql_print_error("NDB Binlog: Skipping locally defined table '%s.%s' "
                      "from binlog schema event '%s' from node %d.",
                      schema->db, schema->name, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    // Instantiate a new 'share' for the altered table.
    if (ndb_create_table_from_engine(m_thd, schema->db, schema->name))
    {
      print_could_not_discover_error(m_thd, schema);
    }
    DBUG_VOID_RETURN;
  }


  void
  handle_online_alter_table_prepare(const Ndb_schema_op* schema)
  {
    assert(is_post_epoch()); // Always after epoch

    ndbapi_invalidate_table(schema->db, schema->name);
    mysqld_close_cached_table(schema->db, schema->name);
    mysqld_close_cached_tables_referenced_by(schema->db, schema->name);

    if (schema->node_id != own_nodeid())
    {
      write_schema_op_to_binlog(m_thd, schema);
      if (!is_local_table(schema->db, schema->name))
      {
        mysqld_write_frm_from_ndb(schema->db, schema->name);
      }
    }
  }


  void
  handle_online_alter_table_commit(const Ndb_schema_op* schema)
  {
    assert(is_post_epoch()); // Always after epoch

    NDB_SHARE *share= get_share(schema);
    if (share)
    {
      if (opt_ndb_extra_logging > 9)
        sql_print_information("NDB Binlog: handling online alter/rename");

      mysql_mutex_lock(&share->mutex);
      ndb_binlog_close_shadow_table(share);

      if (ndb_binlog_open_shadow_table(m_thd, share))
      {
        sql_print_error("NDB Binlog: Failed to re-open shadow table %s.%s",
                        schema->db, schema->name);
        mysql_mutex_unlock(&share->mutex);
      }
      else
      {
        /*
          Start subscribing to data changes to the new table definition
        */
        String event_name(INJECTOR_EVENT_LEN);
        ndb_rep_event_name(&event_name, schema->db, schema->name,
                           get_binlog_full(share));
        NdbEventOperation *tmp_op= share->op;
        share->new_op= 0;
        share->op= 0;

        Thd_ndb *thd_ndb= get_thd_ndb(m_thd);
        Ndb *ndb= thd_ndb->ndb;
        Ndb_table_guard ndbtab_g(ndb->getDictionary(), schema->name);
        const NDBTAB *ndbtab= ndbtab_g.get_table();
        if (ndbcluster_create_event_ops(m_thd, share, ndbtab,
                                        event_name.c_ptr()))
        {
          sql_print_error("NDB Binlog:"
                          "FAILED CREATE (DISCOVER) EVENT OPERATIONS Event: %s",
                          event_name.c_ptr());
        }
        else
        {
          share->new_op= share->op;
        }
        share->op= tmp_op;
        mysql_mutex_unlock(&share->mutex);

        if (opt_ndb_extra_logging > 9)
          sql_print_information("NDB Binlog: handling online "
                                "alter/rename done");
      }
      mysql_mutex_lock(&share->mutex);
      if (share->op && share->new_op)
      {
        Ndb_event_data *event_data=
          (Ndb_event_data *) share->op->getCustomData();
        if (event_data)
          delete event_data;
        share->op->setCustomData(NULL);
        {
          Mutex_guard injector_mutex_g(injector_event_mutex);
          injector_ndb->dropEventOperation(share->op);
        }
        share->op= share->new_op;
        share->new_op= 0;
        free_share(&share);
        DBUG_ASSERT(share);   // Should still be ref'ed
      }
      mysql_mutex_unlock(&share->mutex);

      free_share(&share);     // temporary ref.
    }
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

    Ndb_local_schema::Table tab(m_thd, schema->db, schema->name);
    if (tab.is_local_table())
    {
      /* Table is not a NDB table in this mysqld -> leave it */
      sql_print_error("NDB Binlog: Skipping drop of locally "
                      "defined table '%s.%s' from binlog schema "
                      "event '%s' from node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);

      // There should be no NDB_SHARE for this table
      assert(!get_share(schema));

      DBUG_VOID_RETURN;
    }

    tab.remove_table();

    NDB_SHARE *share= get_share(schema); // temporary ref.
    if (!share || !share->op)
    {
      ndbapi_invalidate_table(schema->db, schema->name);
      mysqld_close_cached_table(schema->db, schema->name);
    }
    if (share)
    {
      mysql_mutex_lock(&ndbcluster_mutex);
      ndbcluster_mark_share_dropped(&share); // server ref.
      DBUG_ASSERT(share);                    // Should still be ref'ed
      free_share(&share, TRUE);              // temporary ref.
      mysql_mutex_unlock(&ndbcluster_mutex);
    }

    ndbapi_invalidate_table(schema->db, schema->name);
    mysqld_close_cached_table(schema->db, schema->name);
    mysqld_close_cached_tables_referenced_by(schema->db, schema->name);

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

    Ndb_local_schema::Table from(m_thd, schema->db, schema->name);
    if (from.is_local_table())
    {
      /* Tables exists as a local table, print error and leave it */
      sql_print_error("NDB Binlog: Skipping renaming locally "
                      "defined table '%s.%s' from binlog schema "
                      "event '%s' from node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    NDB_SHARE *share= get_share(schema); // temporary ref.
    if (!share || !share->op)
    {
      ndbapi_invalidate_table(schema->db, schema->name);
      mysqld_close_cached_table(schema->db, schema->name);
    }
    if (share)
      free_share(&share);      // temporary ref.

    share= get_share(schema);  // temporary ref.
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
    DBUG_ASSERT(!IS_TMP_PREFIX(schema->name));
    DBUG_ASSERT(!IS_TMP_PREFIX(NDB_SHARE::key_get_table_name(prepared_key)));

    // Rename the local table
    from.rename_table(NDB_SHARE::key_get_db_name(prepared_key),
                      NDB_SHARE::key_get_table_name(prepared_key));

    // Rename share and release the old key
    NDB_SHARE_KEY* old_key = share->key;
    ndbcluster_rename_share(m_thd, share, prepared_key);
    m_schema_dist_data.save_prepared_rename_key(NULL);
    NDB_SHARE::free_key(old_key);

    free_share(&share);  // temporary ref.

    ndbapi_invalidate_table(schema->db, schema->name);
    mysqld_close_cached_table(schema->db, schema->name);

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

    if (check_if_local_tables_in_db(schema->db))
    {
      /* Tables exists as a local table, print error and leave it */
      sql_print_error("NDB Binlog: Skipping drop database '%s' since "
                      "it contained local tables "
                      "binlog schema event '%s' from node %d. ",
                      schema->db, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    const int no_print_error[1]= {0};
    run_query(m_thd, schema->query,
              schema->query + schema->query_length,
              no_print_error);

    mysqld_close_cached_tables_referenced_by(schema->db, schema->name);

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

    NDB_SHARE *share= get_share(schema);
    // invalidation already handled by binlog thread
    if (!share || !share->op)
    {
      ndbapi_invalidate_table(schema->db, schema->name);
      mysqld_close_cached_table(schema->db, schema->name);
    }
    if (share)
    {
      /**
       * We need to reset any pre-fetch auto_increment range
       * since an open share may be kept during the truncate
       * operation for mysql servers that don't monitor table
       * DDL-events (if binlogging is disabled).
       */
      if (get_binlog_nologging(share))
        reset_tuple_id_range(share);
      free_share(&share); // temporary ref.
    }

    if (is_local_table(schema->db, schema->name))
    {
      sql_print_error("NDB Binlog: Skipping locally defined table "
                      "'%s.%s' from binlog schema event '%s' from "
                      "node %d. ",
                      schema->db, schema->name, schema->query,
                      schema->node_id);
      DBUG_VOID_RETURN;
    }

    if (ndb_create_table_from_engine(m_thd, schema->db, schema->name))
    {
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

    if (is_local_table(schema->db, schema->name))
    {
      sql_print_error("NDB Binlog: Skipping locally defined table '%s.%s' from "
                          "binlog schema event '%s' from node %d. ",
                          schema->db, schema->name, schema->query,
                          schema->node_id);
      DBUG_VOID_RETURN;
    }

    if (ndb_create_table_from_engine(m_thd, schema->db, schema->name))
    {
      print_could_not_discover_error(m_thd, schema);
    }

    mysqld_close_cached_tables_referenced_by(schema->db, schema->name);

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

    if (opt_ndb_extra_logging > 9)
      sql_print_information("Got dist_priv event: %s, "
                            "flushing privileges",
                            get_schema_type_name(schema->type));

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

      if (opt_ndb_extra_logging > 19)
      {
        sql_print_information("NDB: got schema event on %s.%s(%u/%u) query: '%s' type: %s(%d) node: %u slock: %x%08x",
                              schema->db, schema->name,
                              schema->id, schema->version,
                              schema->query,
                              get_schema_type_name(schema_type),
                              schema_type,
                              schema->node_id,
                              schema->slock.bitmap[1],
                              schema->slock.bitmap[0]);
      }

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
        sql_print_error("NDB schema: Skipping old schema operation"
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
      if (opt_ndb_extra_logging > 9)
        sql_print_information("%s - %s.%s",
                              get_schema_type_name(schema_type),
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
        DBUG_ASSERT(FALSE);
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
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Schema dist: cluster failure "
                              "at epoch %u/%u.",
                              (uint)(pOp->getGCI() >> 32),
                              (uint)(pOp->getGCI()));
      // fall through
    case NDBEVENT::TE_DROP:
      /* ndb_schema table DROPped */
      if (opt_ndb_extra_logging &&
          ndb_binlog_tables_inited && ndb_binlog_running)
        sql_print_information("NDB Binlog: ndb tables initially "
                              "read only on reconnect.");

      /* release the ndb_schema_share */
      mysql_mutex_lock(&injector_data_mutex);
      free_share(&ndb_schema_share);
      ndb_schema_share= NULL;

      ndb_binlog_tables_inited= FALSE;
      ndb_binlog_is_ready= FALSE;
      mysql_mutex_unlock(&injector_data_mutex);

      ndb_tdc_close_cached_tables();

      ndb_handle_schema_change(m_thd, s_ndb, pOp, event_data);
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
      sql_print_error("NDB Schema dist: unknown event %u, ignoring...",
                      ev_type);
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

    TABLE_LIST tables;
    tables.init_one_table(STRING_WITH_LEN(NDB_REP_DB),    // db
                          STRING_WITH_LEN(NDB_REP_TABLE), // name
                          NDB_REP_TABLE,                  // alias
                          TL_WRITE);                      // for write

    /* Only allow real table to be opened */
    tables.required_type= FRMTYPE_TABLE;

    const uint flags =
      MYSQL_LOCK_IGNORE_TIMEOUT; /* Wait for lock "infinitely" */
    if (open_and_lock_tables(thd, &tables, flags))
    {
      if (thd->killed)
        DBUG_PRINT("error", ("NDB Binlog: Opening ndb_binlog_index: killed"));
      else
        sql_print_error("NDB Binlog: Opening ndb_binlog_index: %d, '%s'",
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
    tmp_disable_binlog(thd);

    if (open_binlog_index_table(thd, &ndb_binlog_index))
    {
      if (thd->killed)
        DBUG_PRINT("error", ("NDB Binlog: Unable to lock table ndb_binlog_index, killed"));
      else
        sql_print_error("NDB Binlog: Unable to lock table ndb_binlog_index");
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
      DBUG_EXECUTE_IF("ndb_injector_binlog_index_write_fail_random",
                      {
                        if ((((uint32) rand()) % 10) == 9)
                        {
                          sql_print_error("NDB Binlog: Injecting random write failure");
                          error= ndb_binlog_index->file->ha_write_row(ndb_binlog_index->record[0]);
                        }
                      });
    
      if (error)
      {
        sql_print_error("NDB Binlog: Failed writing to ndb_binlog_index for epoch %u/%u "
                        " orig_server_id %u orig_epoch %u/%u "
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
            my_snprintf(tmp, sizeof(tmp), "%u/%u,%u,%u/%u",
                        uint(epoch >> 32), uint(epoch),
                        uint(cursor->orig_server_id),
                        uint(cursor->orig_epoch >> 32), 
                        uint(cursor->orig_epoch));
        
          else
            my_snprintf(tmp, sizeof(tmp), "%u/%u", uint(epoch >> 32), uint(epoch));
        
          bool error_row = (row == (cursor->next));
          sql_print_error("NDB Binlog: Writing row (%s) to ndb_binlog_index - %s",
                          tmp,
                          (error_row?"ERROR":
                           (seen_error_row?"Discarded":
                            "OK")));
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
        sql_print_error("NDB Binlog: Failed committing transaction to ndb_binlog_index "
                        "with error %d.",
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

    reenable_binlog(thd);
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
    thd_set_command(new_thd, COM_DAEMON);
    new_thd->system_thread = SYSTEM_THREAD_NDBCLUSTER_BINLOG;
    new_thd->get_protocol_classic()->set_client_capabilities(0);
    new_thd->security_context()->skip_grants();
    new_thd->set_current_stmt_binlog_format_row();

    // Retry the write
    const int retry_result = write_rows_impl(new_thd, rows);
    if (retry_result)
    {
      sql_print_error("NDB Binlog: Failed writing to ndb_binlog_index table "
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
    sql_print_warning("NDB: server id set to zero - changes logged to "
                      "bin log with server id zero will be logged with "
                      "another server id by slave mysqlds");
  }

  /* 
     Check that ServerId is not using the reserved bit or bits reserved
     for application use
  */
  if ((::server_id & 0x1 << 31) ||                             // Reserved bit
      !ndbcluster_anyvalue_is_serverid_in_range(::server_id))  // server_id_bits
  {
    sql_print_error("NDB: server id provided is too large to be represented in "
                    "opt_server_id_bits or is reserved");
    DBUG_RETURN(-1);
  }

  /*
     Check that v2 events are enabled if log-transaction-id is set
  */
  if (opt_ndb_log_transaction_id &&
      log_bin_use_v1_row_events)
  {
    sql_print_error("NDB: --ndb-log-transaction-id requires v2 Binlog row events "
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


/**************************************************************
  Internal helper functions for creating/dropping ndb events
  used by the client sql threads
**************************************************************/
void
ndb_rep_event_name(String *event_name,const char *db, const char *tbl,
                   bool full, bool allow_hardcoded_name)
{
  if (allow_hardcoded_name &&
      strcmp(db,  NDB_REP_DB) == 0 &&
      strcmp(tbl, NDB_SCHEMA_TABLE) == 0)
  {
    // Always use REPL$ as prefix for the event on mysql.ndb_schema
    // (unless when dropping events and allow_hardcoded_name is set to false)
    full = false;
  }
 
  if (full)
    event_name->set_ascii("REPLF$", 6);
  else
    event_name->set_ascii("REPL$", 5);
  event_name->append(db);
#ifdef NDB_WIN32
  /*
   * Some bright spark decided that we should sometimes have backslashes.
   * This causes us pain as the event is db/table and not db\table so trying
   * to drop db\table when we meant db/table ends in the event lying around
   * after drop table, leading to all sorts of pain.
  */
  String backslash_sep(1);
  backslash_sep.set_ascii("\\",1);

  int bsloc;
  if((bsloc= event_name->strstr(backslash_sep,0))!=-1)
	  event_name->replace(bsloc, 1, "/", 1);
#endif
  if (tbl)
  {
    event_name->append('/');
    event_name->append(tbl);
  }
  DBUG_PRINT("info", ("ndb_rep_event_name: %s", event_name->c_ptr()));
}

#ifdef HAVE_NDB_BINLOG
static void 
set_binlog_flags(NDB_SHARE *share,
                 Ndb_binlog_type ndb_binlog_type)
{
  DBUG_ENTER("set_binlog_flags");
  switch (ndb_binlog_type)
  {
  case NBT_NO_LOGGING:
    DBUG_PRINT("info", ("NBT_NO_LOGGING"));
    set_binlog_nologging(share);
    DBUG_VOID_RETURN;
  case NBT_DEFAULT:
    DBUG_PRINT("info", ("NBT_DEFAULT"));
    if (opt_ndb_log_updated_only)
    {
      set_binlog_updated_only(share);
    }
    else
    {
      set_binlog_full(share);
    }
    if (opt_ndb_log_update_as_write)
    {
      set_binlog_use_write(share);
    }
    else
    {
      set_binlog_use_update(share);
    }
    if (opt_ndb_log_update_minimal)
    {
      set_binlog_update_minimal(share);
    }
    break;
  case NBT_UPDATED_ONLY:
    DBUG_PRINT("info", ("NBT_UPDATED_ONLY"));
    set_binlog_updated_only(share);
    set_binlog_use_write(share);
    break;
  case NBT_USE_UPDATE:
    DBUG_PRINT("info", ("NBT_USE_UPDATE"));
  case NBT_UPDATED_ONLY_USE_UPDATE:
    DBUG_PRINT("info", ("NBT_UPDATED_ONLY_USE_UPDATE"));
    set_binlog_updated_only(share);
    set_binlog_use_update(share);
    break;
  case NBT_FULL:
    DBUG_PRINT("info", ("NBT_FULL"));
    set_binlog_full(share);
    set_binlog_use_write(share);
    break;
  case NBT_FULL_USE_UPDATE:
    DBUG_PRINT("info", ("NBT_FULL_USE_UPDATE"));
    set_binlog_full(share);
    set_binlog_use_update(share);
    break;
  case NBT_UPDATED_ONLY_MINIMAL:
    DBUG_PRINT("info", ("NBT_UPDATED_ONLY_MINIMAL"));
    set_binlog_updated_only(share);
    set_binlog_use_update(share);
    set_binlog_update_minimal(share);
    break;
  case NBT_UPDATED_FULL_MINIMAL:
    DBUG_PRINT("info", ("NBT_UPDATED_FULL_MINIMAL"));
    set_binlog_full(share);
    set_binlog_use_update(share);
    set_binlog_update_minimal(share);
    break;
  default:
    DBUG_VOID_RETURN;
  }
  set_binlog_logging(share);
  DBUG_VOID_RETURN;
}


/*
  ndbcluster_get_binlog_replication_info

  This function retrieves the data for the given table
  from the ndb_replication table.

  If the table is not found, or the table does not exist,
  then defaults are returned.
*/
int
ndbcluster_get_binlog_replication_info(THD *thd, Ndb *ndb,
                                       const char* db,
                                       const char* table_name,
                                       uint server_id,
                                       Uint32* binlog_flags,
                                       const st_conflict_fn_def** conflict_fn,
                                       st_conflict_fn_arg* args,
                                       Uint32* num_args)
{
  DBUG_ENTER("ndbcluster_get_binlog_replication_info");

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
      sql_print_information("NDB: ndb-log-apply-status forcing "
                            "%s.%s to FULL USE_WRITE",
                            NDB_REP_DB, NDB_APPLY_TABLE);
      *binlog_flags = NBT_FULL;
      *conflict_fn = NULL;
      *num_args = 0;
      DBUG_RETURN(0);
    }
  }

  Ndb_rep_tab_reader rep_tab_reader;

  int rc = rep_tab_reader.lookup(ndb,
                                 db,
                                 table_name,
                                 server_id);

  const char* msg = rep_tab_reader.get_warning_message();
  if (msg != NULL)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_NDB_REPLICATION_SCHEMA_ERROR,
                        ER(ER_NDB_REPLICATION_SCHEMA_ERROR),
                        msg);
    sql_print_warning("NDB Binlog: %s",
                      msg);
  }

  if (rc != 0)
    DBUG_RETURN(ER_NDB_REPLICATION_SCHEMA_ERROR);

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
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_CONFLICT_FN_PARSE_ERROR,
                          ER(ER_CONFLICT_FN_PARSE_ERROR),
                          msgbuf);

      /*
        Log as well, useful for contexts where the thd's stack of
        warnings are ignored
      */
      if (opt_ndb_extra_logging)
      {
        sql_print_warning("NDB Slave: Table %s.%s : Parse error on conflict fn : %s",
                          db, table_name,
                          msgbuf);
      }

      DBUG_RETURN(ER_CONFLICT_FN_PARSE_ERROR);
    }
  }
  else
  {
    /* No conflict function specified */
    conflict_fn= NULL;
    num_args= 0;
  }

  DBUG_RETURN(0);
}

int
ndbcluster_apply_binlog_replication_info(THD *thd,
                                         NDB_SHARE *share,
                                         const NDBTAB* ndbtab,
                                         const st_conflict_fn_def* conflict_fn,
                                         const st_conflict_fn_arg* args,
                                         Uint32 num_args,
                                         Uint32 binlog_flags)
{
  DBUG_ENTER("ndbcluster_apply_binlog_replication_info");
  char tmp_buf[FN_REFLEN];

  DBUG_PRINT("info", ("Setting binlog flags to %u", binlog_flags));
  set_binlog_flags(share, (enum Ndb_binlog_type)binlog_flags);

  if (conflict_fn != NULL)
  {
    if (setup_conflict_fn(get_thd_ndb(thd)->ndb, 
                          &share->m_cfn_share,
                          share->db,
                          share->table_name,
                          ((share->flags & NSF_BLOB_FLAG) != 0),
                          get_binlog_use_update(share),
                          ndbtab,
                          tmp_buf, sizeof(tmp_buf),
                          conflict_fn,
                          args,
                          num_args) == 0)
    {
      if (opt_ndb_extra_logging)
      {
        sql_print_information("%s", tmp_buf);
      }
    }
    else
    {
      /*
        Dump setup failure message to error log
        for cases where thd warning stack is
        ignored
      */
      sql_print_warning("NDB Slave: Table %s.%s : %s",
                        share->db,
                        share->table_name,
                        tmp_buf);

      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_CONFLICT_FN_PARSE_ERROR,
                          ER(ER_CONFLICT_FN_PARSE_ERROR),
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
ndbcluster_read_binlog_replication(THD *thd, Ndb *ndb,
                                   NDB_SHARE *share,
                                   const NDBTAB *ndbtab,
                                   uint server_id)
{
  DBUG_ENTER("ndbcluster_read_binlog_replication");
  Uint32 binlog_flags;
  const st_conflict_fn_def* conflict_fn= NULL;
  st_conflict_fn_arg args[MAX_CONFLICT_ARGS];
  Uint32 num_args = MAX_CONFLICT_ARGS;

  if ((ndbcluster_get_binlog_replication_info(thd, ndb,
                                              share->db,
                                              share->table_name,
                                              server_id,
                                              &binlog_flags,
                                              &conflict_fn,
                                              args,
                                              &num_args) != 0) ||
      (ndbcluster_apply_binlog_replication_info(thd,
                                                share,
                                                ndbtab,
                                                conflict_fn,
                                                args,
                                                num_args,
                                                binlog_flags) != 0))
  {
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}
#endif /* HAVE_NDB_BINLOG */

bool
ndbcluster_check_if_local_table(const char *dbname, const char *tabname)
{
  char key[FN_REFLEN + 1];
  char ndb_file[FN_REFLEN + 1];

  DBUG_ENTER("ndbcluster_check_if_local_table");
  build_table_filename(key, sizeof(key)-1, dbname, tabname, reg_ext, 0);
  build_table_filename(ndb_file, sizeof(ndb_file)-1,
                       dbname, tabname, ha_ndb_ext, 0);
  /* Check that any defined table is an ndb table */
  DBUG_PRINT("info", ("Looking for file %s and %s", key, ndb_file));
  if ((! my_access(key, F_OK)) && my_access(ndb_file, F_OK))
  {
    DBUG_PRINT("info", ("table file %s not on disk, local table", ndb_file));   
  
  
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/*
  Common function for setting up everything for logging a table at
  create/discover.
*/
static
int ndbcluster_create_binlog_setup(THD *thd, Ndb *ndb,
                                   NDB_SHARE *share)
{
  DBUG_ENTER("ndbcluster_create_binlog_setup");

  Mutex_guard share_g(share->mutex);
  if (get_binlog_nologging(share) || share->op != 0 || share->new_op != 0)
  {
    DBUG_RETURN(0);     // replication already setup, or should not
  }

  if (!share->need_events(ndb_binlog_running))
  {
    set_binlog_nologging(share);
    DBUG_RETURN(0);
  }

  while (share && !IS_TMP_PREFIX(share->table_name))
  {
    /*
      ToDo make sanity check of share so that the table is actually the same
      I.e. we need to do open file from frm in this case
      Currently awaiting this to be fixed in the 4.1 tree in the general
      case
    */

    /* Create the event in NDB */
    ndb->setDatabaseName(share->db);

    NDBDICT *dict= ndb->getDictionary();
    Ndb_table_guard ndbtab_g(dict, share->table_name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    if (ndbtab == 0)
    {
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: Failed to get table %s from ndb: "
                              "%s, %d",
                              share->key_string(),
                              dict->getNdbError().message,
                              dict->getNdbError().code);
      break; // error
    }
#ifdef HAVE_NDB_BINLOG
    /*
     */
    ndbcluster_read_binlog_replication(thd, ndb, share, ndbtab,
                                       ::server_id);
#endif
    /*
      check if logging turned off for this table
    */
    if ((share->flags & NSF_HIDDEN_PK) &&
        (share->flags & NSF_BLOB_FLAG) &&
        !(share->flags & NSF_NO_BINLOG))
    {
      DBUG_PRINT("NDB_SHARE", ("NSF_HIDDEN_PK && NSF_BLOB_FLAG -> NSF_NO_BINLOG"));
      share->flags |= NSF_NO_BINLOG;
    }
    if (get_binlog_nologging(share))
    {
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: NOT logging %s",
                              share->key_string());
      DBUG_RETURN(0);
    }

    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, share->db, share->table_name, get_binlog_full(share));
    /*
      event should have been created by someone else,
      but let's make sure, and create if it doesn't exist
    */
    const NDBEVENT *ev= dict->getEvent(event_name.c_ptr());
    if (!ev)
    {
      if (ndbcluster_create_event(thd, ndb, ndbtab, event_name.c_ptr(), share))
      {
        sql_print_error("NDB Binlog: "
                        "FAILED CREATE (DISCOVER) TABLE Event: %s",
                        event_name.c_ptr());
        break; // error
      }
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: "
                              "CREATE (DISCOVER) TABLE Event: %s",
                              event_name.c_ptr());
    }
    else
    {
      delete ev;
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: DISCOVER TABLE Event: %s",
                              event_name.c_ptr());
    }

    /*
      create the event operations for receiving logging events
    */
    if (ndbcluster_create_event_ops(thd, share,
                                    ndbtab, event_name.c_ptr()))
    {
      sql_print_error("NDB Binlog:"
                      "FAILED CREATE (DISCOVER) EVENT OPERATIONS Event: %s",
                      event_name.c_ptr());
      /* a warning has been issued to the client */
      break;
    }
    DBUG_RETURN(0);
  }
  DBUG_RETURN(-1);
}

int ndbcluster_create_binlog_setup(THD *thd, Ndb *ndb, const char *key,
                                   const char *db,
                                   const char *table_name,
                                   TABLE * table)
{
  DBUG_ENTER("ndbcluster_create_binlog_setup");
  DBUG_PRINT("enter",("key: %s %s.%s",
                      key, db, table_name));
  DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(table_name));

  // Get a temporary ref AND a ref from open_tables iff created.
  NDB_SHARE* share= get_share(key, table, true, false);
  if (share == NULL)
  {
    /**
     * Failed to create share
     */
    DBUG_RETURN(-1);
  }

  // Before 'schema_dist_is_ready', Thd_ndb::ALLOW_BINLOG_SETUP is required
  int ret= 0;
  if (ndb_schema_dist_is_ready() ||
      get_thd_ndb(thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP))
  {
    ret= ndbcluster_create_binlog_setup(thd, ndb, share);
  }
  free_share(&share); // temporary ref.
  DBUG_RETURN(ret);
}

int
ndbcluster_create_event(THD *thd, Ndb *ndb, const NDBTAB *ndbtab,
                        const char *event_name, NDB_SHARE *share,
                        int push_warning)
{
  DBUG_ENTER("ndbcluster_create_event");
  DBUG_PRINT("enter", ("table: '%s', version: %d",
                      ndbtab->getName(), ndbtab->getObjectVersion()));
  DBUG_PRINT("enter", ("event: '%s', share->key: '%s'",
                       event_name, share->key_string()));

  // Never create event on table with temporary name
  DBUG_ASSERT(! IS_TMP_PREFIX(ndbtab->getName()));
  // Never create event on the blob table(s)
  DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(ndbtab->getName()));
  DBUG_ASSERT(share);

  if (get_binlog_nologging(share))
  {
    if (opt_ndb_extra_logging && ndb_binlog_running)
      sql_print_information("NDB Binlog: NOT logging %s",
                            share->key_string());
    DBUG_PRINT("info", ("share->flags & NSF_NO_BINLOG, flags: %x %d",
                        share->flags, share->flags & NSF_NO_BINLOG));
    DBUG_RETURN(0);
  }

  ndb->setDatabaseName(share->db);
  NDBDICT *dict= ndb->getDictionary();
  NDBEVENT my_event(event_name);
  my_event.setTable(*ndbtab);
  my_event.addTableEvent(NDBEVENT::TE_ALL);
  if (share->flags & NSF_HIDDEN_PK)
  {
    if (share->flags & NSF_BLOB_FLAG)
    {
      sql_print_error("NDB Binlog: logging of table %s "
                      "with BLOB attribute and no PK is not supported",
                      share->key_string());
      if (push_warning)
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            ER(ER_ILLEGAL_HA_CREATE_OPTION),
                            ndbcluster_hton_name,
                            "Binlog of table with BLOB attribute and no PK");

      set_binlog_nologging(share);
      DBUG_RETURN(-1);
    }
    /* No primary key, subscribe for all attributes */
    my_event.setReport((NDBEVENT::EventReport)
                       (NDBEVENT::ER_ALL | NDBEVENT::ER_DDL));
    DBUG_PRINT("info", ("subscription all"));
  }
  else
  {
    if (strcmp(share->db, NDB_REP_DB) == 0 &&
        strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
    {
      /**
       * ER_SUBSCRIBE is only needed on NDB_SCHEMA_TABLE
       */
      my_event.setReport((NDBEVENT::EventReport)
                         (NDBEVENT::ER_ALL |
                          NDBEVENT::ER_SUBSCRIBE |
                          NDBEVENT::ER_DDL));
      DBUG_PRINT("info", ("subscription all and subscribe"));
    }
    else
    {
      if (get_binlog_full(share))
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
  if (share->flags & NSF_BLOB_FLAG)
    my_event.mergeEvents(TRUE);

  /* add all columns to the event */
  int n_cols= ndbtab->getNoOfColumns();
  for(int a= 0; a < n_cols; a++)
    my_event.addEventColumn(a);

  if (dict->createEvent(my_event)) // Add event to database
  {
    if (dict->getNdbError().classification != NdbError::SchemaObjectExists)
    {
      /*
        failed, print a warning
      */
      if (push_warning > 1)
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                            dict->getNdbError().code,
                            dict->getNdbError().message, "NDB");
      sql_print_error("NDB Binlog: Unable to create event in database. "
                      "Event: %s  Error Code: %d  Message: %s", event_name,
                      dict->getNdbError().code, dict->getNdbError().message);
      DBUG_RETURN(-1);
    }

    /*
      try retrieving the event, if table version/id matches, we will get
      a valid event.  Otherwise we have a trailing event from before
    */
    const NDBEVENT *ev;
    if ((ev= dict->getEvent(event_name)))
    {
      delete ev;
      DBUG_RETURN(0);
    }

    /*
      trailing event from before; an error, but try to correct it
    */
    if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT &&
        dict->dropEvent(my_event.getName(), 1))
    {
      if (push_warning > 1)
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                            dict->getNdbError().code,
                            dict->getNdbError().message, "NDB");
      sql_print_error("NDB Binlog: Unable to create event in database. "
                      " Attempt to correct with drop failed. "
                      "Event: %s Error Code: %d Message: %s",
                      event_name,
                      dict->getNdbError().code,
                      dict->getNdbError().message);
      DBUG_RETURN(-1);
    }

    /*
      try to add the event again
    */
    if (dict->createEvent(my_event))
    {
      if (push_warning > 1)
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                            dict->getNdbError().code,
                            dict->getNdbError().message, "NDB");
      sql_print_error("NDB Binlog: Unable to create event in database. "
                      " Attempt to correct with drop ok, but create failed. "
                      "Event: %s Error Code: %d Message: %s",
                      event_name,
                      dict->getNdbError().code,
                      dict->getNdbError().message);
      DBUG_RETURN(-1);
    }
  }

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
  - create eventOperations for receiving log events
  - setup ndb recattrs for reception of log event data
  - "start" the event operation

  used at create/discover of tables
*/
int
ndbcluster_create_event_ops(THD *thd, NDB_SHARE *share,
                            const NDBTAB *ndbtab, const char *event_name)
{
  /*
    we are in either create table or rename table so table should be
    locked, hence we can work with the share without locks
  */

  DBUG_ENTER("ndbcluster_create_event_ops");
  DBUG_PRINT("enter", ("table: '%s' event: '%s', share->key: '%s'",
                       ndbtab->getName(), event_name, share->key_string()));

  // Never create event on table with temporary name
  DBUG_ASSERT(! IS_TMP_PREFIX(ndbtab->getName()));
  // Never create event on the blob table(s)
  DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(ndbtab->getName()));
  DBUG_ASSERT(share);

  if (get_binlog_nologging(share))
  {
    DBUG_PRINT("info", ("share->flags & NSF_NO_BINLOG, flags: %x",
                        share->flags));
    DBUG_RETURN(0);
  }

  // Don't allow event ops to be created on distributed priv tables
  // they are distributed via ndb_schema
  assert(!Ndb_dist_priv_util::is_distributed_priv_table(share->db,
                                                        share->table_name));

  int do_ndb_schema_share= 0, do_ndb_apply_status_share= 0;
  if (!ndb_schema_share && strcmp(share->db, NDB_REP_DB) == 0 &&
      strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
    do_ndb_schema_share= 1;
  else if (!ndb_apply_status_share && strcmp(share->db, NDB_REP_DB) == 0 &&
           strcmp(share->table_name, NDB_APPLY_TABLE) == 0)
    do_ndb_apply_status_share= 1;
  else
#ifdef HAVE_NDB_BINLOG
    if (!binlog_filter->db_ok(share->db) ||
        !ndb_binlog_running ||
        is_exceptions_table(share->table_name))
#endif
  {
    set_binlog_nologging(share);
    DBUG_RETURN(0);
  }

  // Check that the share agrees
  DBUG_ASSERT(share->need_events(ndb_binlog_running));

  Ndb_event_data *event_data= share->event_data;
  if (share->op)
  {
    event_data= (Ndb_event_data *) share->op->getCustomData();
    assert(event_data->share == share);
    assert(share->event_data == 0);

    DBUG_ASSERT(share->use_count > 1);
    sql_print_error("NDB Binlog: discover reusing old ev op");
    /* ndb_share reference ToDo free */
    DBUG_PRINT("NDB_SHARE", ("%s ToDo free  use_count: %u",
                             share->key_string(), share->use_count));
    free_share(&share); // old event op already has reference
    assert(false);   //OJA, possibly ndbcluster_mark_share_dropped()?
    DBUG_RETURN(0);
  }

  DBUG_ASSERT(event_data != 0);
  TABLE *table= event_data->shadow_table;

  int retries= 100;
  int retry_sleep= 0;
  while (1)
  {
    if (retry_sleep > 0)
    {
      do_retry_sleep(retry_sleep);
    }
    Mutex_guard injector_mutex_g(injector_event_mutex);
    Ndb *ndb= injector_ndb;
    if (do_ndb_schema_share)
      ndb= schema_ndb;

    if (ndb == NULL)
      DBUG_RETURN(-1);

    NdbEventOperation* op;
    if (do_ndb_schema_share)
      op= ndb->createEventOperation(event_name);
    else
    {
      // set injector_ndb database/schema from table internal name
      int ret= ndb->setDatabaseAndSchemaName(ndbtab);
      assert(ret == 0); NDB_IGNORE_VALUE(ret);
      op= ndb->createEventOperation(event_name);
      // reset to catch errors
      ndb->setDatabaseName("");
    }
    if (!op)
    {
      sql_print_error("NDB Binlog: Creating NdbEventOperation failed for"
                      " %s",event_name);
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                          ndb->getNdbError().code,
                          ndb->getNdbError().message,
                          "NDB");
      DBUG_RETURN(-1);
    }

    if (share->flags & NSF_BLOB_FLAG)
      op->mergeEvents(TRUE); // currently not inherited from event

    const uint n_columns= ndbtab->getNoOfColumns();
    const uint n_stored_fields= table->s->stored_fields;
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
          DBUG_ASSERT(share->flags & NSF_BLOB_FLAG);
          attr0.blob= op->getBlobHandle(col_name);
          attr1.blob= op->getPreBlobHandle(col_name);
          if (attr0.blob == NULL || attr1.blob == NULL)
          {
            sql_print_error("NDB Binlog: Creating NdbEventOperation"
                            " blob field %u handles failed (code=%d) for %s",
                            j, op->getNdbError().code, event_name);
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                                op->getNdbError().code,
                                op->getNdbError().message,
                                "NDB");
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
    share->event_data= 0;                   // take over event data
    share->op= op; // assign op in NDB_SHARE

    /* Check if user explicitly requires monitoring of empty updates */
    if (opt_ndb_log_empty_update)
      op->setAllowEmptyUpdate(true);

    if (op->execute())
    {
      share->op= NULL;
      retries--;
      if (op->getNdbError().status != NdbError::TemporaryError &&
          op->getNdbError().code != 1407)
        retries= 0;
      if (retries == 0)
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_GET_ERRMSG, ER(ER_GET_ERRMSG), 
                            op->getNdbError().code, op->getNdbError().message,
                            "NDB");
        sql_print_error("NDB Binlog: ndbevent->execute failed for %s; %d %s",
                        event_name,
                        op->getNdbError().code, op->getNdbError().message);
      }
      share->event_data= event_data;
      op->setCustomData(NULL);
      ndb->dropEventOperation(op);
      if (retries && !thd->killed)
      {
        /*
          100 milliseconds, temporary error on schema operation can
          take some time to be resolved
        */
        retry_sleep = 100;
        continue;
      }
      DBUG_RETURN(-1);
    }
    break;
  }

  /* ndb_share reference binlog */
  get_share(share);
  DBUG_PRINT("NDB_SHARE", ("%s binlog  use_count: %u",
                           share->key_string(), share->use_count));
  if (do_ndb_apply_status_share)
  {
    /* ndb_share reference binlog extra */
    ndb_apply_status_share= get_share(share);
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra  use_count: %u",
                             share->key_string(), share->use_count));
    DBUG_ASSERT(get_thd_ndb(thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP));
  }
  else if (do_ndb_schema_share)
  {
    /* ndb_share reference binlog extra */
    Mutex_guard ndb_schema_share_g(injector_data_mutex);
    ndb_schema_share= get_share(share);
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra  use_count: %u",
                             share->key_string(), share->use_count));
    DBUG_ASSERT(get_thd_ndb(thd)->check_option(Thd_ndb::ALLOW_BINLOG_SETUP));
  }

  DBUG_PRINT("info",("%s share->op: 0x%lx  share->use_count: %u",
                     share->key_string(), (long) share->op,
                     share->use_count));

  if (opt_ndb_extra_logging)
    sql_print_information("NDB Binlog: logging %s (%s,%s)",
                          share->key_string(),
                          get_binlog_full(share) ? "FULL" : "UPDATED",
                          get_binlog_use_update(share) ? "USE_UPDATE" : "USE_WRITE");
  DBUG_RETURN(0);
}

int
ndbcluster_drop_event(THD *thd, Ndb *ndb, NDB_SHARE *share,
                      const char *dbname,
                      const char *tabname)
{
  DBUG_ENTER("ndbcluster_drop_event");
  /*
    There might be 2 types of events setup for the table, we cannot know
    which ones are supposed to be there as they may have been created
    differently for different mysqld's.  So we drop both
  */
  for (uint i= 0; i < 2; i++)
  {
    NDBDICT *dict= ndb->getDictionary();
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, dbname, tabname, i,
                       false /* don't allow hardcoded event name */);
    
    if (!dict->dropEvent(event_name.c_ptr()))
      continue;

    if (dict->getNdbError().code != 4710 &&
        dict->getNdbError().code != 1419)
    {
      /* drop event failed for some reason, issue a warning */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                          dict->getNdbError().code,
                          dict->getNdbError().message, "NDB");
      /* error is not that the event did not exist */
      sql_print_error("NDB Binlog: Unable to drop event in database. "
                      "Event: %s Error Code: %d Message: %s",
                      event_name.c_ptr(),
                      dict->getNdbError().code,
                      dict->getNdbError().message);
      /* ToDo; handle error? */
      if (share && share->op &&
          share->op->getState() == NdbEventOperation::EO_EXECUTING &&
          dict->getNdbError().mysql_code != HA_ERR_NO_CONNECTION)
      {
        DBUG_ASSERT(FALSE);
        DBUG_RETURN(-1);
      }
    }
  }
  DBUG_RETURN(0);
}

/*
  when entering the calling thread should have a share lock id share != 0
  then the injector thread will have  one as well, i.e. share->use_count == 0
  (unless it has already dropped... then share->op == 0)
*/

int
ndbcluster_handle_drop_table(THD *thd, Ndb *ndb, NDB_SHARE *share,
                             const char *type_str,
                             const char * dbname, const char * tabname)
{
  DBUG_ENTER("ndbcluster_handle_drop_table");

  if (dbname && tabname)
  {
    if (ndbcluster_drop_event(thd, ndb, share, dbname, tabname))
      DBUG_RETURN(-1);
  }

  if (share == 0 || share->op == 0)
  {
    DBUG_RETURN(0);
  }

/*
  Syncronized drop between client thread and injector thread is
  neccessary in order to maintain ordering in the binlog,
  such that the drop occurs _after_ any inserts/updates/deletes.

  The penalty for this is that the drop table becomes slow.

  This wait is however not strictly neccessary to produce a binlog
  that is usable.  However the slave does not currently handle
  these out of order.
*/
  const char *save_proc_info= thd->proc_info;
  thd->proc_info= "Syncing ndb table schema operation and binlog";

  mysql_mutex_lock(&share->mutex);
  int max_timeout= DEFAULT_SYNC_TIMEOUT;
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
        sql_print_error("NDB %s: %s timed out. Ignoring...",
                        type_str, share->key_string());
        DBUG_ASSERT(false);
        break;
      }
      if (opt_ndb_extra_logging)
        ndb_report_waiting(type_str, max_timeout,
                           type_str, share->key_string(), 0);
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
                                        TRUE);
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
                                        TRUE);
#else
            field_bit->Field_bit::store((longlong)
                                        (*value).rec->u_64_value(), TRUE);
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

  sql_print_error("NDB Binlog: unhandled error %d for table %s",
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
    if (opt_ndb_extra_logging)
      sql_print_information("NDB Binlog: cluster failure for %s at epoch %u/%u.",
                            share->key_string(),
                            (uint)(pOp->getGCI() >> 32),
                            (uint)(pOp->getGCI()));
    // fallthrough
  case NDBEVENT::TE_DROP:
    if (ndb_apply_status_share == share)
    {
      if (opt_ndb_extra_logging &&
          ndb_binlog_tables_inited && ndb_binlog_running)
        sql_print_information("NDB Binlog: ndb tables initially "
                              "read only on reconnect.");

      /* release the ndb_apply_status_share */
      free_share(&ndb_apply_status_share);
      ndb_apply_status_share= NULL;

      Mutex_guard injector_g(injector_data_mutex);
      ndb_binlog_tables_inited= FALSE;
    }

    ndb_handle_schema_change(thd, injector_ndb, pOp, event_data);
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
    sql_print_error("NDB Binlog: unknown non data event %d for %s. "
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
handle_data_event(THD* thd, Ndb *ndb, NdbEventOperation *pOp,
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
      sql_print_warning("NDB: unknown value for binlog signalling 0x%X, "
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
        uint n_fields= table->s->fields;
        MY_BITMAP b;
        uint32 bitbuf[128 / (sizeof(uint32) * 8)];
        bitmap_init(&b, bitbuf, n_fields, FALSE);
        bitmap_copy(&b, & (share->stored_columns));
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
    
  DBUG_EXECUTE_IF("ndb_injector_set_event_conflict_flags",
                  {
                    event_conflict_flags = 0xfafa;
                  });
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
        sql_print_error("NDB: Binlog Injector discarding row event "
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

  dbug_print_table("table", table);

  uint n_fields= table->s->fields;
  DBUG_PRINT("info", ("Assuming %u columns for table %s",
                      n_fields, table->s->table_name.str));
  MY_BITMAP b;
  my_bitmap_map bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                            8*sizeof(my_bitmap_map) - 1) /
                           (8*sizeof(my_bitmap_map))];
  ndb_bitmap_init(b, bitbuf, n_fields);
  bitmap_copy(&b, & (share->stored_columns));

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
      if (share->flags & NSF_BLOB_FLAG)
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
                            &b, n_fields, table->record[0],
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
      if (!get_binlog_full(share) && table->s->primary_key != MAX_KEY)
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
      if (share->flags & NSF_BLOB_FLAG)
      {
        my_ptrdiff_t ptrdiff= table->record[n] - table->record[0];
        ret = get_ndb_blobs_value(table, event_data->ndb_value[n],
                                  blobs_buffer[n],
                                  blobs_buffer_size[n],
                                  ptrdiff);
        assert(ret == 0);
      }
      ndb_unpack_record(table, event_data->ndb_value[n], &b, table->record[n]);
      DBUG_EXECUTE("info", print_records(table, table->record[n]););
      ret = trans.delete_row(logged_server_id,
                             injector::transaction::table(table, true),
                             &b, n_fields, table->record[n],
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
      if (share->flags & NSF_BLOB_FLAG)
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
      DBUG_EXECUTE("info", print_records(table, table->record[0]););
      if (table->s->primary_key != MAX_KEY &&
          !get_binlog_use_update(share)) 
      {
        /*
          since table has a primary key, we can do a write
          using only after values
        */
        ret = trans.write_row(logged_server_id,
                              injector::transaction::table(table, true),
                              &b, n_fields, table->record[0],// after values
                              extra_row_info_ptr);
        assert(ret == 0);
      }
      else
      {
        /*
          mysql server cannot handle the ndb hidden key and
          therefore needs the before image as well
        */
        if (share->flags & NSF_BLOB_FLAG)
        {
          my_ptrdiff_t ptrdiff= table->record[1] - table->record[0];
          ret = get_ndb_blobs_value(table, event_data->ndb_value[1],
                                    blobs_buffer[1],
                                    blobs_buffer_size[1],
                                    ptrdiff);
          assert(ret == 0);
        }
        ndb_unpack_record(table, event_data->ndb_value[1], &b, table->record[1]);
        DBUG_EXECUTE("info", print_records(table, table->record[1]););

        MY_BITMAP col_bitmap_before_update;
        my_bitmap_map bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                                  8*sizeof(my_bitmap_map) - 1) /
                                 (8*sizeof(my_bitmap_map))];
        ndb_bitmap_init(col_bitmap_before_update, bitbuf, n_fields);
        if (get_binlog_update_minimal(share))
        {
          event_data->generate_minimal_bitmap(&col_bitmap_before_update, &b);
        }
        else
        {
          bitmap_copy(&col_bitmap_before_update, &b);
        }

        ret = trans.update_row(logged_server_id,
                               injector::transaction::table(table, true),
                               &col_bitmap_before_update, &b, n_fields,
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

  if (share->flags & NSF_BLOB_FLAG)
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
    DBUG_ASSERT(!IS_NDB_BLOB_PREFIX(op->getEvent()->getTable()->getName()));
    DBUG_PRINT("info", ("removing event operation on %s",
                        op->getEvent()->getName()));

    Ndb_event_data *event_data= (Ndb_event_data *) op->getCustomData();
    DBUG_ASSERT(event_data);

    NDB_SHARE *share= event_data->share;
    DBUG_ASSERT(share != NULL);
    DBUG_ASSERT(share->op == op || share->new_op == op);
    DBUG_ASSERT(share->event_data == NULL);
    share->event_data= event_data; // Give back event_data ownership
    op->setCustomData(NULL);

    mysql_mutex_lock(&share->mutex);
    share->op= 0;
    share->new_op= 0;
    mysql_mutex_unlock(&share->mutex);

    DBUG_PRINT("NDB_SHARE", ("%s binlog free  use_count: %u",
                             share->key_string(), share->use_count));
    free_share(&share);

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
    /* ndb_share reference binlog extra free */
    if (ndb_apply_status_share)
    {
      DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                               ndb_apply_status_share->key_string(),
                               ndb_apply_status_share->use_count));
    }
    free_share(&ndb_schema_share);
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
    /* ndb_share reference binlog extra free */
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                             ndb_apply_status_share->key_string(),
                             ndb_apply_status_share->use_count));
    free_share(&ndb_apply_status_share);
    ndb_apply_status_share= NULL;
  }

  if (s_ndb)
    remove_event_operations(s_ndb);

  if (i_ndb)
    remove_event_operations(i_ndb);

  if (opt_ndb_extra_logging > 15)
  {
    mysql_mutex_lock(&ndbcluster_mutex);
    if (ndbcluster_open_tables.records)
    {
      sql_print_information("remove_all_event_operations: Remaining open tables: ");
      for (uint i= 0; i < ndbcluster_open_tables.records; i++)
      {
        NDB_SHARE* share = (NDB_SHARE*)my_hash_element(&ndbcluster_open_tables,i);
        sql_print_information("  %s.%s, use_count: %u",
                              share->db,
                              share->table_name,
                              share->use_count);
      }
    }
    mysql_mutex_unlock(&ndbcluster_mutex);
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
    sql_print_error("NDB: Could not get apply status share");
    DBUG_ASSERT(ndb_apply_status_share != NULL);
    DBUG_RETURN(false);
  }

  longlong gci_to_store = (longlong) gci;

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("ndb_binlog_injector_cycle_gcis",
                  {
                    ulonglong gciHi = ((gci_to_store >> 32) 
                                       & 0xffffffff);
                    ulonglong gciLo = (gci_to_store & 0xffffffff);
                    gciHi = (gciHi % 3);
                    sql_print_warning("NDB Binlog injector cycling gcis (%llu -> %llu)",
                                      gci_to_store, (gciHi << 32) + gciLo);
                    gci_to_store = (gciHi << 32) + gciLo;
                  });
  DBUG_EXECUTE_IF("ndb_binlog_injector_repeat_gcis",
                  {
                    ulonglong gciHi = ((gci_to_store >> 32) 
                                       & 0xffffffff);
                    ulonglong gciLo = (gci_to_store & 0xffffffff);
                    gciHi=0xffffff00;
                    gciLo=0;
                    sql_print_warning("NDB Binlog injector repeating gcis (%llu -> %llu)",
                                      gci_to_store, (gciHi << 32) + gciLo);
                    gci_to_store = (gciHi << 32) + gciLo;
                  });
#endif

  /* Build row buffer for generated ndb_apply_status
     WRITE_ROW event
     First get the relevant table structure.
  */
  DBUG_ASSERT(!ndb_apply_status_share->event_data);
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
                       apply_status_table->s->fields,
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
  uint incident_id= 0;
  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();

  enum { BCCC_starting, BCCC_running, BCCC_restart,  } binlog_thread_state;

  /**
   * If we get error after having reported incident
   *   but before binlog started...we do "Restarting Cluster Binlog"
   *   in that case, don't report incident again
   */
  bool do_incident = true;

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

  thd_set_command(thd, COM_DAEMON);
  thd->system_thread= SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->get_protocol_classic()->set_client_capabilities(0);
  thd->security_context()->skip_grants();
  // Create thd->net vithout vio
  thd->get_protocol_classic()->init_net((st_vio *) 0);

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

  if (!(s_ndb= new Ndb(g_ndb_cluster_connection, NDB_REP_DB)) ||
      s_ndb->setNdbObjectName("Ndb Binlog schema change monitoring") ||
      s_ndb->init())
  {
    log_error("Creating schema Ndb object failed");
    goto err;
  }
  log_info("Created schema Ndb object, reference: 0x%x, name: '%s'",
           s_ndb->getReference(), s_ndb->getNdbObjectName());

  // empty database
  if (!(i_ndb= new Ndb(g_ndb_cluster_connection, "")) ||
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
    ndb_binlog_running= TRUE;
  }
  log_verbose(1, "Setup completed");
  /* Thread start up completed  */

  log_verbose(1, "Wait for server start completed");
  /*
    wait for mysql server to start (so that the binlog is started
    and thus can receive the first GAP event)
  */
  mysql_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
  {
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&COND_server_started, &LOCK_server_started,
                         &abstime);
    if (is_stop_requested())
    {
      mysql_mutex_unlock(&LOCK_server_started);
      goto err;
    }
  }
  mysql_mutex_unlock(&LOCK_server_started);

  // Defer call of THD::init_for_query until after mysqld_server_started
  // to ensure that the parts of MySQL Server it uses has been created
  thd->init_for_queries();
  lex_start(thd);

  log_verbose(1, "Check for incidents");

  while (do_incident && ndb_binlog_running)
  {
    /*
      check if it is the first log, if so we do not insert a GAP event
      as there is really no log to have a GAP in
    */
    if (incident_id == 0)
    {
      LOG_INFO log_info;
      mysql_bin_log.get_current_log(&log_info);
      int len=  (uint)strlen(log_info.log_file_name);
      uint no= 0;
      if ((sscanf(log_info.log_file_name + len - 6, "%u", &no) == 1) &&
          no == 1)
      {
        /* this is the fist log, so skip GAP event */
        break;
      }
    }

    /*
      Always insert a GAP event as we cannot know what has happened
      in the cluster while not being connected.
    */
    LEX_STRING const msg[2]=
      {
        { C_STRING_WITH_LEN("mysqld startup")    },
        { C_STRING_WITH_LEN("cluster disconnect")}
      };
    int ret = inj->record_incident(thd,
                                   binary_log::Incident_event::INCIDENT_LOST_EVENTS,
                                   msg[incident_id]);
    assert(ret == 0); NDB_IGNORE_VALUE(ret);
    do_incident = false; // Don't report incident again, unless we get started
    break;
  }
  incident_id= 1;
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
        sql_print_information("NDB Binlog: Server shutdown detected while "
                              "waiting for ndbcluster to start...");
        goto err;
      }
      NdbSleep_MilliSleep(1000);
    } //while (!ndb_binlog_setup())

    DBUG_ASSERT(ndbcluster_hton->slot != ~(uint)0);

    /*
      Prevent schema dist participant from (implicitly)
      taking GSL lock as part of taking MDL lock
    */
    thd_ndb->set_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT);

    thd->query_id= 0; // to keep valgrind quiet
  }

  schema_dist_data.init(g_ndb_cluster_connection);

  {
    log_verbose(1, "Wait for first event");
    // wait for the first event
    thd->proc_info= "Waiting for first event from ndbcluster";
    int schema_res, res;
    Uint64 schema_gci;
    do
    {
      DBUG_PRINT("info", ("Waiting for the first event"));

      if (is_stop_requested())
        goto err;

      my_thread_yield();
      mysql_mutex_lock(&injector_event_mutex);
      schema_res= s_ndb->pollEvents(100, &schema_gci);
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
        res= i_ndb->pollEvents(10, &gci);
        mysql_mutex_unlock(&injector_event_mutex);
      }
      if (gci > schema_gci)
      {
        schema_gci= gci;
      }
    }
    // now check that we have epochs consistent with what we had before the restart
    DBUG_PRINT("info", ("schema_res: %d  schema_gci: %u/%u", schema_res,
                        (uint)(schema_gci >> 32),
                        (uint)(schema_gci)));
    {
      i_ndb->flushIncompleteEvents(schema_gci);
      s_ndb->flushIncompleteEvents(schema_gci);
      if (schema_gci < ndb_latest_handled_binlog_epoch)
      {
        sql_print_error("NDB Binlog: cluster has been restarted --initial or with older filesystem. "
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
        sql_print_warning("NDB Binlog: cluster has reconnected. "
                          "Changes to the database that occured while "
                          "disconnected will not be in the binlog");
      }
      if (opt_ndb_extra_logging)
      {
        sql_print_information("NDB Binlog: starting log at epoch %u/%u",
                              (uint)(schema_gci >> 32),
                              (uint)(schema_gci));
      }
    }
    log_verbose(1, "Got first event");
  }
  /*
    binlog thread is ready to receive events
    - client threads may now start updating data, i.e. tables are
    no longer read only
  */
  mysql_mutex_lock(&injector_data_mutex);
  ndb_binlog_is_ready= TRUE;
  mysql_mutex_unlock(&injector_data_mutex);

  if (opt_ndb_extra_logging)
    sql_print_information("NDB Binlog: ndb tables writable");
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
  do_incident = true; // If we get disconnected again...do incident report
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
      DBUG_EXECUTE_IF("ndb_binlog_injector_yield_before_schema_pollEvent",
      {
        /**
         * Simulate that the binlog thread yields the CPU inbetween 
         * these two pollEvents, which can result in reading a
         * 'schema_gci > gci'. (Likely due to mutex locking)
         */
        NdbSleep_MilliSleep(50);
      });
  
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
        my_snprintf(buf, sizeof(buf), "%s %u/%u(%u/%u)", thd->proc_info,
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
      sql_print_information("NDB Binlog: Server shutdown detected...");
      break;
    }

    MEM_ROOT **root_ptr= my_thread_get_THR_MALLOC();
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

          DBUG_EXECUTE_IF("ndb_binlog_slow_failure_handling",
          {
            if (!ndb_binlog_is_ready)
            {
	      sql_print_information("NDB Binlog: Just lost schema connection, hanging around");
              NdbSleep_SecSleep(10);
              /* There could be a race where client side reconnect before we 
               * are able to detect 's_ndb->getEventOperation() == NULL'.
               * Thus, we never restart the binlog thread as supposed to.
               * -> 'ndb_binlog_is_ready' remains false and we get stuck in RO-mode
               */
	      sql_print_information("NDB Binlog: ...and on our way");
            }
          });

          DBUG_PRINT("info", ("s_ndb first: %s", s_ndb->getEventOperation() ?
                              s_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                              "<empty>"));
          DBUG_PRINT("info", ("i_ndb first: %s", i_ndb->getEventOperation() ?
                              i_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                              "<empty>"));
        }
        else
          sql_print_error("NDB: error %lu (%s) on handling "
                          "binlog schema event",
                          (ulong) s_pOp->getNdbError().code,
                          s_pOp->getNdbError().message);
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
                    ! IS_NDB_BLOB_PREFIX(i_pOp->getEvent()->getTable()->getName()));
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
            sql_print_error("NDB Binlog: Failed to inject apply status write row");
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
            handle_data_event(thd, i_ndb, i_pOp, &rows, trans,
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
                sql_print_error("NDB Binlog: "
                                "Error during ROLLBACK of GCI %u/%u. Error: %d",
                                uint(current_epoch >> 32), uint(current_epoch), r);
                /* TODO: Further handling? */
              }
              break;
            }
          }
          thd->proc_info= "Committing events to binlog";
          if (int r= trans.commit())
          {
            sql_print_error("NDB Binlog: "
                            "Error during COMMIT of GCI. Error: %d",
                            r);
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
        sql_print_error("NDB Binlog: latest transaction in epoch %u/%u not in binlog "
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
  ndb_binlog_tables_inited= FALSE;
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
  {
    if (opt_ndb_extra_logging > 9)
      sql_print_information("NDB Binlog: Release extra share references");

    mysql_mutex_lock(&ndbcluster_mutex);
    while (ndbcluster_open_tables.records)
    {
      NDB_SHARE * share = (NDB_SHARE*)my_hash_element(&ndbcluster_open_tables,0);
      /*
        The share kept by the server has not been freed, free it
        Will also take it out of _open_tables list
      */
      DBUG_ASSERT(share->use_count > 0);
      DBUG_ASSERT(share->state != NSS_DROPPED);
      ndbcluster_mark_share_dropped(&share);
    }
    mysql_mutex_unlock(&ndbcluster_mutex);
  }
  log_info("Stopping...");

  ndb_tdc_close_cached_tables();
  if (opt_ndb_extra_logging > 15)
  {
    sql_print_information("NDB Binlog: remaining open tables: ");
    mysql_mutex_lock(&ndbcluster_mutex);
    for (uint i= 0; i < ndbcluster_open_tables.records; i++)
    {
      NDB_SHARE* share = (NDB_SHARE*)my_hash_element(&ndbcluster_open_tables,i);
      sql_print_information("  %s.%s state: %u use_count: %u",
                            share->db,
                            share->table_name,
                            (uint)share->state,
                            share->use_count);
    }
    mysql_mutex_unlock(&ndbcluster_mutex);
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

  ndb_binlog_running= FALSE;
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
      my_snprintf(buf, buf_size,
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

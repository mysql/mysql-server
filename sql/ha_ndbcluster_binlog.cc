/*
  Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "unireg.h"         // REQUIRED: for other includes
#include "sql_show.h"
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include "ha_ndbcluster.h"

#ifdef HAVE_NDB_BINLOG
#include "rpl_injector.h"
#include "rpl_filter.h"
#include "slave.h"
#include "log_event.h"
#include "ha_ndbcluster_binlog.h"
#include "NdbDictionary.hpp"
#include "ndb_cluster_connection.hpp"
#include <util/NdbAutoPtr.hpp>

#include "sql_base.h"                           // close_thread_tables
#include "sql_table.h"                         // build_table_filename
#include "table.h"                             // open_table_from_share
#include "discover.h"                          // readfrm, writefrm
#include "lock.h"                              // MYSQL_LOCK_IGNORE_FLUSH,
                                               // mysql_unlock_tables
#include "sql_parse.h"                         // mysql_parse
#include "transaction.h"

#ifdef ndb_dynamite
#undef assert
#define assert(x) do { if(x) break; ::printf("%s %d: assert failed: %s\n", __FILE__, __LINE__, #x); ::fflush(stdout); ::signal(SIGABRT,SIG_DFL); ::abort(); ::kill(::getpid(),6); ::kill(::getpid(),9); } while (0)
#endif

extern my_bool opt_ndb_log_binlog_index;
extern ulong opt_ndb_extra_logging;
/*
  defines for cluster replication table names
*/
#include "ha_ndbcluster_tables.h"
#define NDB_APPLY_TABLE_FILE "./" NDB_REP_DB "/" NDB_APPLY_TABLE
#define NDB_SCHEMA_TABLE_FILE "./" NDB_REP_DB "/" NDB_SCHEMA_TABLE

/*
  Timeout for syncing schema events between
  mysql servers, and between mysql server and the binlog
*/
static const int DEFAULT_SYNC_TIMEOUT= 120;


/*
  Flag showing if the ndb injector thread is running, if so == 1
  -1 if it was started but later stopped for some reason
   0 if never started
*/
static int ndb_binlog_thread_running= 0;

/*
  Flag showing if the ndb binlog should be created, if so == TRUE
  FALSE if not
*/
my_bool ndb_binlog_running= FALSE;
my_bool ndb_binlog_tables_inited= FALSE;

/*
  Global reference to the ndb injector thread THD oject

  Has one sole purpose, for setting the in_use table member variable
  in get_share(...)
*/
THD *injector_thd= 0;

/*
  Global reference to ndb injector thd object.

  Used mainly by the binlog index thread, but exposed to the client sql
  thread for one reason; to setup the events operations for a table
  to enable ndb injector thread receiving events.

  Must therefore always be used with a surrounding
  mysql_mutex_lock(&injector_mutex), when doing create/dropEventOperation
*/
static Ndb *injector_ndb= 0;
static Ndb *schema_ndb= 0;

static int ndbcluster_binlog_inited= 0;
/*
  Flag "ndbcluster_binlog_terminating" set when shutting down mysqld.
  Server main loop should call handlerton function:

  ndbcluster_hton->binlog_func ==
  ndbcluster_binlog_func(...,BFN_BINLOG_END,...) ==
  ndbcluster_binlog_end

  at shutdown, which sets the flag. And then server needs to wait for it
  to complete.  Otherwise binlog will not be complete.

  ndbcluster_hton->panic == ndbcluster_end() will not return until
  ndb binlog is completed
*/
static int ndbcluster_binlog_terminating= 0;

/*
  Mutex and condition used for interacting between client sql thread
  and injector thread
*/
pthread_t ndb_binlog_thread;
mysql_mutex_t injector_mutex;
mysql_cond_t  injector_cond;

/* NDB Injector thread (used for binlog creation) */
static ulonglong ndb_latest_applied_binlog_epoch= 0;
static ulonglong ndb_latest_handled_binlog_epoch= 0;
static ulonglong ndb_latest_received_binlog_epoch= 0;

NDB_SHARE *ndb_apply_status_share= 0;
NDB_SHARE *ndb_schema_share= 0;
mysql_mutex_t ndb_schema_share_mutex;

extern my_bool opt_log_slave_updates;
static my_bool g_ndb_log_slave_updates;

/* Schema object distribution handling */
HASH ndb_schema_objects;
typedef struct st_ndb_schema_object {
  mysql_mutex_t mutex;
  char *key;
  uint key_length;
  uint use_count;
  MY_BITMAP slock_bitmap;
  uint32 slock[256/32]; // 256 bits for lock status of table
} NDB_SCHEMA_OBJECT;
static NDB_SCHEMA_OBJECT *ndb_get_schema_object(const char *key,
                                                my_bool create_if_not_exists,
                                                my_bool have_lock);
static void ndb_free_schema_object(NDB_SCHEMA_OBJECT **ndb_schema_object,
                                   bool have_lock);

static Uint64 *p_latest_trans_gci= 0;

/*
  Global variables for holding the ndb_binlog_index table reference
*/
static TABLE *ndb_binlog_index= 0;
static TABLE_LIST binlog_tables;

/*
  Helper functions
*/

#ifndef DBUG_OFF
/* purecov: begin deadcode */
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
/* purecov: end */
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
                (long) f->null_ptr,
                (int) ((uchar*) f->null_ptr - table->record[0])));
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


/*
  Run a query through mysql_parse

  Used to:
  - purging the ndb_binlog_index
  - creating the ndb_apply_status table
*/
static void run_query(THD *thd, char *buf, char *end,
                      const int *no_print_error, my_bool disable_binlog)
{
  ulong save_thd_query_length= thd->query_length();
  char *save_thd_query= thd->query();
  ulong save_thread_id= thd->variables.pseudo_thread_id;
  struct system_status_var save_thd_status_var= thd->status_var;
  THD_TRANS save_thd_transaction_all= thd->transaction.all;
  THD_TRANS save_thd_transaction_stmt= thd->transaction.stmt;
  ulonglong save_thd_options= thd->variables.option_bits;
  DBUG_ASSERT(sizeof(save_thd_options) == sizeof(thd->variables.option_bits));
  NET save_thd_net= thd->net;

  bzero((char*) &thd->net, sizeof(NET));
  thd->set_query(buf, (uint) (end - buf));
  thd->variables.pseudo_thread_id= thread_id;
  thd->transaction.stmt.modified_non_trans_table= FALSE;
  if (disable_binlog)
    thd->variables.option_bits&= ~OPTION_BIN_LOG;
    
  DBUG_PRINT("query", ("%s", thd->query()));

  DBUG_ASSERT(!thd->in_sub_stmt);
  DBUG_ASSERT(!thd->locked_tables_mode);

  {
    Parser_state parser_state;
    if (!parser_state.init(thd, thd->query(), thd->query_length()))
      mysql_parse(thd, thd->query(), thd->query_length(), &parser_state);
  }

  if (no_print_error && thd->is_slave_error)
  {
    int i;
    Thd_ndb *thd_ndb= get_thd_ndb(thd);
    for (i= 0; no_print_error[i]; i++)
      if ((thd_ndb->m_error_code == no_print_error[i]) ||
          (thd->stmt_da->sql_errno() == (unsigned) no_print_error[i]))
        break;
    if (!no_print_error[i])
      sql_print_error("NDB: %s: error %s %d(ndb: %d) %d %d",
                      buf,
                      thd->stmt_da->message(),
                      thd->stmt_da->sql_errno(),
                      thd_ndb->m_error_code,
                      (int) thd->is_error(), thd->is_slave_error);
  }
  /*
    XXX: this code is broken. mysql_parse()/mysql_reset_thd_for_next_command()
    can not be called from within a statement, and
    run_query() can be called from anywhere, including from within
    a sub-statement.
    This particular reset is a temporary hack to avoid an assert
    for double assignment of the diagnostics area when run_query()
    is called from ndbcluster_reset_logs(), which is called from
    mysql_flush().
  */
  thd->stmt_da->reset_diagnostics_area();

  thd->variables.option_bits= save_thd_options;
  thd->set_query(save_thd_query, save_thd_query_length);
  thd->variables.pseudo_thread_id= save_thread_id;
  thd->status_var= save_thd_status_var;
  thd->transaction.all= save_thd_transaction_all;
  thd->transaction.stmt= save_thd_transaction_stmt;
  thd->net= save_thd_net;
  thd->set_current_stmt_binlog_format_row();

  if (thd == injector_thd)
  {
    /*
      running the query will close all tables, including the ndb_binlog_index
      used in injector_thd
    */
    ndb_binlog_index= 0;
  }
}

static void
ndbcluster_binlog_close_table(THD *thd, NDB_SHARE *share)
{
  DBUG_ENTER("ndbcluster_binlog_close_table");
  if (share->table_share)
  {
    closefrm(share->table, 1);
    share->table_share= 0;
    share->table= 0;
  }
  DBUG_ASSERT(share->table == 0);
  DBUG_VOID_RETURN;
}


/*
  Creates a TABLE object for the ndb cluster table

  NOTES
    This does not open the underlying table
*/

static int
ndbcluster_binlog_open_table(THD *thd, NDB_SHARE *share,
                             TABLE_SHARE *table_share, TABLE *table,
                             int reopen)
{
  int error;
  DBUG_ENTER("ndbcluster_binlog_open_table");
  
  init_tmp_table_share(thd, table_share, share->db, 0, share->table_name, 
                       share->key);
  if ((error= open_table_def(thd, table_share, 0)))
  {
    DBUG_PRINT("error", ("open_table_def failed: %d my_errno: %d", error, my_errno));
    free_table_share(table_share);
    DBUG_RETURN(error);
  }
  if ((error= open_table_from_share(thd, table_share, "", 0 /* fon't allocate buffers */, 
                                    (uint) READ_ALL, 0, table, FALSE)))
  {
    DBUG_PRINT("error", ("open_table_from_share failed %d my_errno: %d", error, my_errno));
    free_table_share(table_share);
    DBUG_RETURN(error);
  }
  mysql_mutex_lock(&LOCK_open);
  assign_new_table_id(table_share);
  mysql_mutex_unlock(&LOCK_open);

  if (!reopen)
  {
    // allocate memory on ndb share so it can be reused after online alter table
    (void)multi_alloc_root(&share->mem_root,
                           &(share->record[0]), table->s->rec_buff_length,
                           &(share->record[1]), table->s->rec_buff_length,
                           NULL);
  }
  {
    my_ptrdiff_t row_offset= share->record[0] - table->record[0];
    Field **p_field;
    for (p_field= table->field; *p_field; p_field++)
      (*p_field)->move_field_offset(row_offset);
    table->record[0]= share->record[0];
    table->record[1]= share->record[1];
  }

  table->in_use= injector_thd;
  
  table->s->db.str= share->db;
  table->s->db.length= strlen(share->db);
  table->s->table_name.str= share->table_name;
  table->s->table_name.length= strlen(share->table_name);
  
  DBUG_ASSERT(share->table_share == 0);
  share->table_share= table_share;
  DBUG_ASSERT(share->table == 0);
  share->table= table;
  /* We can't use 'use_all_columns()' as the file object is not setup yet */
  table->column_bitmaps_set_no_signal(&table->s->all_set, &table->s->all_set);
#ifndef DBUG_OFF
  dbug_print_table("table", table);
#endif
  DBUG_RETURN(0);
}


/*
  Initialize the binlog part of the NDB_SHARE
*/
int ndbcluster_binlog_init_share(NDB_SHARE *share, TABLE *_table)
{
  THD *thd= current_thd;
  MEM_ROOT *mem_root= &share->mem_root;
  int do_event_op= ndb_binlog_running;
  int error= 0;
  DBUG_ENTER("ndbcluster_binlog_init_share");

  share->connect_count= g_ndb_cluster_connection->get_connect_count();

  share->op= 0;
  share->table= 0;

  if (!ndb_schema_share &&
      strcmp(share->db, NDB_REP_DB) == 0 &&
      strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
    do_event_op= 1;
  else if (!ndb_apply_status_share &&
           strcmp(share->db, NDB_REP_DB) == 0 &&
           strcmp(share->table_name, NDB_APPLY_TABLE) == 0)
    do_event_op= 1;

  {
    int i, no_nodes= g_ndb_cluster_connection->no_db_nodes();
    share->subscriber_bitmap= (MY_BITMAP*)
      alloc_root(mem_root, no_nodes * sizeof(MY_BITMAP));
    for (i= 0; i < no_nodes; i++)
    {
      bitmap_init(&share->subscriber_bitmap[i],
                  (Uint32*)alloc_root(mem_root, max_ndb_nodes/8),
                  max_ndb_nodes, FALSE);
      bitmap_clear_all(&share->subscriber_bitmap[i]);
    }
  }

  if (!do_event_op)
  {
    if (_table)
    {
      if (_table->s->primary_key == MAX_KEY)
        share->flags|= NSF_HIDDEN_PK;
      if (_table->s->blob_fields != 0)
        share->flags|= NSF_BLOB_FLAG;
    }
    else
    {
      share->flags|= NSF_NO_BINLOG;
    }
    DBUG_RETURN(error);
  }
  while (1) 
  {
    int error;
    TABLE_SHARE *table_share= (TABLE_SHARE *) alloc_root(mem_root, sizeof(*table_share));
    TABLE *table= (TABLE*) alloc_root(mem_root, sizeof(*table));
    if ((error= ndbcluster_binlog_open_table(thd, share, table_share, table, 0)))
      break;
    /*
      ! do not touch the contents of the table
      it may be in use by the injector thread
    */
    MEM_ROOT *mem_root= &share->mem_root;
    share->ndb_value[0]= (NdbValue*)
      alloc_root(mem_root, sizeof(NdbValue) *
                 (table->s->fields + 2 /*extra for hidden key and part key*/));
    share->ndb_value[1]= (NdbValue*)
      alloc_root(mem_root, sizeof(NdbValue) *
                 (table->s->fields + 2 /*extra for hidden key and part key*/));

    if (table->s->primary_key == MAX_KEY)
      share->flags|= NSF_HIDDEN_PK;
    if (table->s->blob_fields != 0)
      share->flags|= NSF_BLOB_FLAG;
    break;
  }
  DBUG_RETURN(error);
}

/*****************************************************************
  functions called from master sql client threads
****************************************************************/

/*
  called in mysql_show_binlog_events and reset_logs to make sure we wait for
  all events originating from this mysql server to arrive in the binlog

  Wait for the last epoch in which the last transaction is a part of.

  Wait a maximum of 30 seconds.
*/
static void ndbcluster_binlog_wait(THD *thd)
{
  if (ndb_binlog_running)
  {
    DBUG_ENTER("ndbcluster_binlog_wait");
    const char *save_info= thd ? thd->proc_info : 0;
    ulonglong wait_epoch= *p_latest_trans_gci;
    int count= 30;
    if (thd)
      thd->proc_info= "Waiting for ndbcluster binlog update to "
	"reach current position";
    while (count && ndb_binlog_running &&
           ndb_latest_handled_binlog_epoch < wait_epoch)
    {
      count--;
      sleep(1);
    }
    if (thd)
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

  DBUG_ENTER("ndbcluster_reset_logs");

  /*
    Wait for all events orifinating from this mysql server has
    reached the binlog before continuing to reset
  */
  ndbcluster_binlog_wait(thd);

  char buf[1024];
  char *end= strmov(buf, "DELETE FROM " NDB_REP_DB "." NDB_REP_TABLE);

  run_query(thd, buf, end, NULL, TRUE);

  DBUG_RETURN(0);
}

/*
  Called from MYSQL_BIN_LOG::purge_logs in log.cc when the binlog "file"
  is removed
*/

static int
ndbcluster_binlog_index_purge_file(THD *thd, const char *file)
{
  if (!ndb_binlog_running || thd->slave_thread)
    return 0;

  DBUG_ENTER("ndbcluster_binlog_index_purge_file");
  DBUG_PRINT("enter", ("file: %s", file));

  char buf[1024];
  char *end= strmov(strmov(strmov(buf,
                                  "DELETE FROM "
                                  NDB_REP_DB "." NDB_REP_TABLE
                                  " WHERE File='"), file), "'");

  run_query(thd, buf, end, NULL, TRUE);

  DBUG_RETURN(0);
}

static void
ndbcluster_binlog_log_query(handlerton *hton, THD *thd, enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char *table_name)
{
  DBUG_ENTER("ndbcluster_binlog_log_query");
  DBUG_PRINT("enter", ("db: %s  table_name: %s  query: %s",
                       db, table_name, query));
  enum SCHEMA_OP_TYPE type;
  int log= 0;
  switch (binlog_command)
  {
  case LOGCOM_CREATE_TABLE:
    type= SOT_CREATE_TABLE;
    DBUG_ASSERT(FALSE);
    break;
  case LOGCOM_ALTER_TABLE:
    type= SOT_ALTER_TABLE;
    log= 1;
    break;
  case LOGCOM_RENAME_TABLE:
    type= SOT_RENAME_TABLE;
    DBUG_ASSERT(FALSE);
    break;
  case LOGCOM_DROP_TABLE:
    type= SOT_DROP_TABLE;
    DBUG_ASSERT(FALSE);
    break;
  case LOGCOM_CREATE_DB:
    type= SOT_CREATE_DB;
    log= 1;
    break;
  case LOGCOM_ALTER_DB:
    type= SOT_ALTER_DB;
    log= 1;
    break;
  case LOGCOM_DROP_DB:
    type= SOT_DROP_DB;
    DBUG_ASSERT(FALSE);
    break;
  }
  if (log)
  {
    ndbcluster_log_schema_op(thd, 0, query, query_length,
                             db, table_name, 0, 0, type,
                             0, 0);
  }
  DBUG_VOID_RETURN;
}


/*
  End use of the NDB Cluster binlog
   - wait for binlog thread to shutdown
*/

static int ndbcluster_binlog_end(THD *thd)
{
  DBUG_ENTER("ndbcluster_binlog_end");

  if (!ndbcluster_binlog_inited)
    DBUG_RETURN(0);
  ndbcluster_binlog_inited= 0;

#ifdef HAVE_NDB_BINLOG
  if (ndb_util_thread_running > 0)
  {
    /*
      Wait for util thread to die (as this uses the injector mutex)
      There is a very small change that ndb_util_thread dies and the
      following mutex is freed before it's accessed. This shouldn't
      however be a likely case as the ndbcluster_binlog_end is supposed to
      be called before ndb_cluster_end().
    */
    mysql_mutex_lock(&LOCK_ndb_util_thread);
    /* Ensure mutex are not freed if ndb_cluster_end is running at same time */
    ndb_util_thread_running++;
    ndbcluster_terminating= 1;
    mysql_cond_signal(&COND_ndb_util_thread);
    while (ndb_util_thread_running > 1)
      mysql_cond_wait(&COND_ndb_util_ready, &LOCK_ndb_util_thread);
    ndb_util_thread_running--;
    mysql_mutex_unlock(&LOCK_ndb_util_thread);
  }

  /* wait for injector thread to finish */
  ndbcluster_binlog_terminating= 1;
  mysql_mutex_lock(&injector_mutex);
  mysql_cond_signal(&injector_cond);
  while (ndb_binlog_thread_running > 0)
    mysql_cond_wait(&injector_cond, &injector_mutex);
  mysql_mutex_unlock(&injector_mutex);

  mysql_mutex_destroy(&injector_mutex);
  mysql_cond_destroy(&injector_cond);
  mysql_mutex_destroy(&ndb_schema_share_mutex);
#endif

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
  char buf[1024];
  char *end= strmov(buf, "DELETE FROM " NDB_REP_DB "." NDB_APPLY_TABLE);
  run_query(thd, buf, end, NULL, TRUE);
  DBUG_VOID_RETURN;
}

/*
  Initialize the binlog part of the ndb handlerton
*/

/**
  Upon the sql command flush logs, we need to ensure that all outstanding
  ndb data to be logged has made it to the binary log to get a deterministic
  behavior on the rotation of the log.
 */
static bool ndbcluster_flush_logs(handlerton *hton)
{
  ndbcluster_binlog_wait(current_thd);
  return FALSE;
}

static int ndbcluster_binlog_func(handlerton *hton, THD *thd, 
                                  enum_binlog_func fn, 
                                  void *arg)
{
  switch(fn)
  {
  case BFN_RESET_LOGS:
    ndbcluster_reset_logs(thd);
    break;
  case BFN_RESET_SLAVE:
    ndbcluster_reset_slave(thd);
    break;
  case BFN_BINLOG_WAIT:
    ndbcluster_binlog_wait(thd);
    break;
  case BFN_BINLOG_END:
    ndbcluster_binlog_end(thd);
    break;
  case BFN_BINLOG_PURGE_FILE:
    ndbcluster_binlog_index_purge_file(thd, (const char *)arg);
    break;
  }
  return 0;
}

void ndbcluster_binlog_init_handlerton()
{
  handlerton *h= ndbcluster_hton;
  h->flush_logs=       ndbcluster_flush_logs;
  h->binlog_func=      ndbcluster_binlog_func;
  h->binlog_log_query= ndbcluster_binlog_log_query;
}





/*
  check the availability af the ndb_apply_status share
  - return share, but do not increase refcount
  - return 0 if there is no share
*/
static NDB_SHARE *ndbcluster_check_ndb_apply_status_share()
{
  mysql_mutex_lock(&ndbcluster_mutex);

  void *share= my_hash_search(&ndbcluster_open_tables,
                              (uchar*) NDB_APPLY_TABLE_FILE,
                              sizeof(NDB_APPLY_TABLE_FILE) - 1);
  DBUG_PRINT("info",("ndbcluster_check_ndb_apply_status_share %s 0x%lx",
                     NDB_APPLY_TABLE_FILE, (long) share));
  mysql_mutex_unlock(&ndbcluster_mutex);
  return (NDB_SHARE*) share;
}

/*
  check the availability af the schema share
  - return share, but do not increase refcount
  - return 0 if there is no share
*/
static NDB_SHARE *ndbcluster_check_ndb_schema_share()
{
  mysql_mutex_lock(&ndbcluster_mutex);

  void *share= my_hash_search(&ndbcluster_open_tables,
                              (uchar*) NDB_SCHEMA_TABLE_FILE,
                              sizeof(NDB_SCHEMA_TABLE_FILE) - 1);
  DBUG_PRINT("info",("ndbcluster_check_ndb_schema_share %s 0x%lx",
                     NDB_SCHEMA_TABLE_FILE, (long) share));
  mysql_mutex_unlock(&ndbcluster_mutex);
  return (NDB_SHARE*) share;
}

/*
  Create the ndb_apply_status table
*/
static int ndbcluster_create_ndb_apply_status_table(THD *thd)
{
  DBUG_ENTER("ndbcluster_create_ndb_apply_status_table");

  /*
    Check if we already have the apply status table.
    If so it should have been discovered at startup
    and thus have a share
  */

  if (ndbcluster_check_ndb_apply_status_share())
    DBUG_RETURN(0);

  if (g_ndb_cluster_connection->get_no_ready() <= 0)
    DBUG_RETURN(0);

  char buf[1024 + 1], *end;

  if (opt_ndb_extra_logging)
    sql_print_information("NDB: Creating " NDB_REP_DB "." NDB_APPLY_TABLE);

  /*
    Check if apply status table exists in MySQL "dictionary"
    if so, remove it since there is none in Ndb
  */
  {
    build_table_filename(buf, sizeof(buf) - 1,
                         NDB_REP_DB, NDB_APPLY_TABLE, reg_ext, 0);
    mysql_file_delete(key_file_frm, buf, MYF(0));
  }

  /*
    Note, updating this table schema must be reflected in ndb_restore
  */
  end= strmov(buf, "CREATE TABLE IF NOT EXISTS "
                   NDB_REP_DB "." NDB_APPLY_TABLE
                   " ( server_id INT UNSIGNED NOT NULL,"
                   " epoch BIGINT UNSIGNED NOT NULL, "
                   " log_name VARCHAR(255) BINARY NOT NULL, "
                   " start_pos BIGINT UNSIGNED NOT NULL, "
                   " end_pos BIGINT UNSIGNED NOT NULL, "
                   " PRIMARY KEY USING HASH (server_id) ) ENGINE=NDB CHARACTER SET latin1");

  const int no_print_error[6]= {ER_TABLE_EXISTS_ERROR,
                                701,
                                702,
                                721, // Table already exist
                                4009,
                                0}; // do not print error 701 etc
  run_query(thd, buf, end, no_print_error, TRUE);

  DBUG_RETURN(0);
}


/*
  Create the schema table
*/
static int ndbcluster_create_schema_table(THD *thd)
{
  DBUG_ENTER("ndbcluster_create_schema_table");

  /*
    Check if we already have the schema table.
    If so it should have been discovered at startup
    and thus have a share
  */

  if (ndbcluster_check_ndb_schema_share())
    DBUG_RETURN(0);

  if (g_ndb_cluster_connection->get_no_ready() <= 0)
    DBUG_RETURN(0);

  char buf[1024 + 1], *end;

  if (opt_ndb_extra_logging)
    sql_print_information("NDB: Creating " NDB_REP_DB "." NDB_SCHEMA_TABLE);

  /*
    Check if schema table exists in MySQL "dictionary"
    if so, remove it since there is none in Ndb
  */
  {
    build_table_filename(buf, sizeof(buf) - 1,
                         NDB_REP_DB, NDB_SCHEMA_TABLE, reg_ext, 0);
    mysql_file_delete(key_file_frm, buf, MYF(0));
  }

  /*
    Update the defines below to reflect the table schema
  */
  end= strmov(buf, "CREATE TABLE IF NOT EXISTS "
                   NDB_REP_DB "." NDB_SCHEMA_TABLE
                   " ( db VARBINARY(63) NOT NULL,"
                   " name VARBINARY(63) NOT NULL,"
                   " slock BINARY(32) NOT NULL,"
                   " query BLOB NOT NULL,"
                   " node_id INT UNSIGNED NOT NULL,"
                   " epoch BIGINT UNSIGNED NOT NULL,"
                   " id INT UNSIGNED NOT NULL,"
                   " version INT UNSIGNED NOT NULL,"
                   " type INT UNSIGNED NOT NULL,"
                   " PRIMARY KEY USING HASH (db,name) ) ENGINE=NDB CHARACTER SET latin1");

  const int no_print_error[6]= {ER_TABLE_EXISTS_ERROR,
                                701,
                                702,
                                721, // Table already exist
                                4009,
                                0}; // do not print error 701 etc
  run_query(thd, buf, end, no_print_error, TRUE);

  DBUG_RETURN(0);
}

int ndbcluster_setup_binlog_table_shares(THD *thd)
{
  if (!ndb_schema_share &&
      ndbcluster_check_ndb_schema_share() == 0)
  {
    ndb_create_table_from_engine(thd, NDB_REP_DB, NDB_SCHEMA_TABLE);
    if (!ndb_schema_share)
    {
      ndbcluster_create_schema_table(thd);
      // always make sure we create the 'schema' first
      if (!ndb_schema_share)
        return 1;
    }
  }
  if (!ndb_apply_status_share &&
      ndbcluster_check_ndb_apply_status_share() == 0)
  {
    ndb_create_table_from_engine(thd, NDB_REP_DB, NDB_APPLY_TABLE);
    if (!ndb_apply_status_share)
    {
      ndbcluster_create_ndb_apply_status_table(thd);
      if (!ndb_apply_status_share)
        return 1;
    }
  }
  if (!ndbcluster_find_all_files(thd))
  {
    ndb_binlog_tables_inited= TRUE;
    if (opt_ndb_extra_logging)
      sql_print_information("NDB Binlog: ndb tables writable");
    close_cached_tables(NULL, NULL, FALSE, LONG_TIMEOUT);
    /* Signal injector thread that all is setup */
    mysql_cond_signal(&injector_cond);
  }
  return 0;
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

struct Cluster_schema
{
  uchar db_length;
  char db[64];
  uchar name_length;
  char name[64];
  uchar slock_length;
  uint32 slock[SCHEMA_SLOCK_SIZE/4];
  unsigned short query_length;
  char *query;
  Uint64 epoch;
  uint32 node_id;
  uint32 id;
  uint32 version;
  uint32 type;
  uint32 any_value;
};

static void print_could_not_discover_error(THD *thd,
                                           const Cluster_schema *schema)
{
  sql_print_error("NDB Binlog: Could not discover table '%s.%s' from "
                  "binlog schema event '%s' from node %d. "
                  "my_errno: %d",
                   schema->db, schema->name, schema->query,
                   schema->node_id, my_errno);
  List_iterator_fast<MYSQL_ERROR> it(thd->warning_info->warn_list());
  MYSQL_ERROR *err;
  while ((err= it++))
    sql_print_warning("NDB Binlog: (%d)%s", err->get_sql_errno(),
                      err->get_message_text());
}

/*
  Transfer schema table data into corresponding struct
*/
static void ndbcluster_get_schema(NDB_SHARE *share,
                                  Cluster_schema *s)
{
  TABLE *table= share->table;
  Field **field;
  /* unpack blob values */
  uchar* blobs_buffer= 0;
  uint blobs_buffer_size= 0;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
  {
    ptrdiff_t ptrdiff= 0;
    int ret= get_ndb_blobs_value(table, share->ndb_value[0],
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
  s->db_length= *(uint8*)(*field)->ptr;
  DBUG_ASSERT(s->db_length <= (*field)->field_length);
  DBUG_ASSERT((*field)->field_length + 1 == sizeof(s->db));
  memcpy(s->db, (*field)->ptr + 1, s->db_length);
  s->db[s->db_length]= 0;
  /* name varchar 1 length uchar */
  field++;
  s->name_length= *(uint8*)(*field)->ptr;
  DBUG_ASSERT(s->name_length <= (*field)->field_length);
  DBUG_ASSERT((*field)->field_length + 1 == sizeof(s->name));
  memcpy(s->name, (*field)->ptr + 1, s->name_length);
  s->name[s->name_length]= 0;
  /* slock fixed length */
  field++;
  s->slock_length= (*field)->field_length;
  DBUG_ASSERT((*field)->field_length == sizeof(s->slock));
  memcpy(s->slock, (*field)->ptr, s->slock_length);
  /* query blob */
  field++;
  {
    Field_blob *field_blob= (Field_blob*)(*field);
    uint blob_len= field_blob->get_length((*field)->ptr);
    uchar *blob_ptr= 0;
    field_blob->get_ptr(&blob_ptr);
    DBUG_ASSERT(blob_len == 0 || blob_ptr != 0);
    s->query_length= blob_len;
    s->query= sql_strmake((char*) blob_ptr, blob_len);
  }
  /* node_id */
  field++;
  s->node_id= ((Field_long *)*field)->val_int();
  /* epoch */
  field++;
  s->epoch= ((Field_long *)*field)->val_int();
  /* id */
  field++;
  s->id= ((Field_long *)*field)->val_int();
  /* version */
  field++;
  s->version= ((Field_long *)*field)->val_int();
  /* type */
  field++;
  s->type= ((Field_long *)*field)->val_int();
  /* free blobs buffer */
  my_free(blobs_buffer);
  dbug_tmp_restore_column_map(table->read_set, old_map);
}

/*
  helper function to pack a ndb varchar
*/
char *ndb_pack_varchar(const NDBCOL *col, char *buf,
                       const char *str, int sz)
{
  switch (col->getArrayType())
  {
    case NDBCOL::ArrayTypeFixed:
      memcpy(buf, str, sz);
      break;
    case NDBCOL::ArrayTypeShortVar:
      *(uchar*)buf= (uchar)sz;
      memcpy(buf + 1, str, sz);
      break;
    case NDBCOL::ArrayTypeMediumVar:
      int2store(buf, sz);
      memcpy(buf + 2, str, sz);
      break;
  }
  return buf;
}

/*
  acknowledge handling of schema operation
*/
static int
ndbcluster_update_slock(THD *thd,
                        const char *db,
                        const char *table_name)
{
  DBUG_ENTER("ndbcluster_update_slock");
  if (!ndb_schema_share)
  {
    DBUG_RETURN(0);
  }

  const NdbError *ndb_error= 0;
  uint32 node_id= g_ndb_cluster_connection->node_id();
  Ndb *ndb= check_ndb_in_thd(thd);
  char save_db[FN_HEADLEN];
  strcpy(save_db, ndb->getDatabaseName());

  char tmp_buf[FN_REFLEN];
  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(NDB_REP_DB);
  Ndb_table_guard ndbtab_g(dict, NDB_SCHEMA_TABLE);
  const NDBTAB *ndbtab= ndbtab_g.get_table();
  NdbTransaction *trans= 0;
  int retries= 100;
  int retry_sleep= 10; /* 10 milliseconds, transaction */
  const NDBCOL *col[SCHEMA_SIZE];
  unsigned sz[SCHEMA_SIZE];

  MY_BITMAP slock;
  uint32 bitbuf[SCHEMA_SLOCK_SIZE/4];
  bitmap_init(&slock, bitbuf, sizeof(bitbuf)*8, false);

  if (ndbtab == 0)
  {
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
        sz[i]= col[i]->getLength();
        DBUG_ASSERT(sz[i] <= sizeof(tmp_buf));
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
      ndb_pack_varchar(col[SCHEMA_DB_I], tmp_buf, db, strlen(db));
      r|= op->equal(SCHEMA_DB_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* name */
      ndb_pack_varchar(col[SCHEMA_NAME_I], tmp_buf, table_name,
                       strlen(table_name));
      r|= op->equal(SCHEMA_NAME_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* slock */
      r|= op->getValue(SCHEMA_SLOCK_I, (char*)slock.bitmap) == 0;
      DBUG_ASSERT(r == 0);
    }
    if (trans->execute(NdbTransaction::NoCommit))
      goto err;
    bitmap_clear_bit(&slock, node_id);
    {
      NdbOperation *op= 0;
      int r= 0;

      /* now update the tuple */
      r|= (op= trans->getNdbOperation(ndbtab)) == 0;
      DBUG_ASSERT(r == 0);
      r|= op->updateTuple();
      DBUG_ASSERT(r == 0);

      /* db */
      ndb_pack_varchar(col[SCHEMA_DB_I], tmp_buf, db, strlen(db));
      r|= op->equal(SCHEMA_DB_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* name */
      ndb_pack_varchar(col[SCHEMA_NAME_I], tmp_buf, table_name,
                       strlen(table_name));
      r|= op->equal(SCHEMA_NAME_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* slock */
      r|= op->setValue(SCHEMA_SLOCK_I, (char*)slock.bitmap);
      DBUG_ASSERT(r == 0);
      /* node_id */
      r|= op->setValue(SCHEMA_NODE_ID_I, node_id);
      DBUG_ASSERT(r == 0);
      /* type */
      r|= op->setValue(SCHEMA_TYPE_I, (uint32)SOT_CLEAR_SLOCK);
      DBUG_ASSERT(r == 0);
    }
    if (trans->execute(NdbTransaction::Commit) == 0)
    {
      dict->forceGCPWait();
      DBUG_PRINT("info", ("node %d cleared lock on '%s.%s'",
                          node_id, db, table_name));
      break;
    }
  err:
    const NdbError *this_error= trans ?
      &trans->getNdbError() : &ndb->getNdbError();
    if (this_error->status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        if (trans)
          ndb->closeTransaction(trans);
        my_sleep(retry_sleep);
        continue; // retry
      }
    }
    ndb_error= this_error;
    break;
  }

  if (ndb_error)
  {
    char buf[1024];
    my_snprintf(buf, sizeof(buf), "Could not release lock on '%s.%s'",
                db, table_name);
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        ndb_error->code, ndb_error->message, buf);
  }
  if (trans)
    ndb->closeTransaction(trans);
  ndb->setDatabaseName(save_db);
  DBUG_RETURN(0);
}

/*
  log query in schema table
*/
static void ndb_report_waiting(const char *key,
                               int the_time,
                               const char *op,
                               const char *obj)
{
  ulonglong ndb_latest_epoch= 0;
  const char *proc_info= "<no info>";
  mysql_mutex_lock(&injector_mutex);
  if (injector_ndb)
    ndb_latest_epoch= injector_ndb->getLatestGCI();
  if (injector_thd)
    proc_info= injector_thd->proc_info;
  mysql_mutex_unlock(&injector_mutex);
  sql_print_information("NDB %s:"
                        " waiting max %u sec for %s %s."
                        "  epochs: (%u,%u,%u)"
                        "  injector proc_info: %s"
                        ,key, the_time, op, obj
                        ,(uint)ndb_latest_handled_binlog_epoch
                        ,(uint)ndb_latest_received_binlog_epoch
                        ,(uint)ndb_latest_epoch
                        ,proc_info
                        );
}

int ndbcluster_log_schema_op(THD *thd, NDB_SHARE *share,
                             const char *query, int query_length,
                             const char *db, const char *table_name,
                             uint32 ndb_table_id,
                             uint32 ndb_table_version,
                             enum SCHEMA_OP_TYPE type,
                             const char *new_db, const char *new_table_name)
{
  DBUG_ENTER("ndbcluster_log_schema_op");
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb)
  {
    if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
    {
      sql_print_error("Could not allocate Thd_ndb object");
      DBUG_RETURN(1);
    }
    set_thd_ndb(thd, thd_ndb);
  }

  DBUG_PRINT("enter",
             ("query: %s  db: %s  table_name: %s  thd_ndb->options: %d",
              query, db, table_name, thd_ndb->options));
  if (!ndb_schema_share || thd_ndb->options & TNO_NO_LOG_SCHEMA_OP)
  {
    DBUG_RETURN(0);
  }

  char tmp_buf2[FN_REFLEN];
  char quoted_table1[2 + 2 * FN_REFLEN + 1];
  char quoted_db1[2 + 2 * FN_REFLEN + 1];
  char quoted_db2[2 + 2 * FN_REFLEN + 1];
  char quoted_table2[2 + 2 * FN_REFLEN + 1];
  int id_length= 0;
  const char *type_str;
  switch (type)
  {
  case SOT_DROP_TABLE:
    /* drop database command, do not log at drop table */
    if (thd->lex->sql_command ==  SQLCOM_DROP_DB)
      DBUG_RETURN(0);
    /* redo the drop table query as is may contain several tables */
    query= tmp_buf2;
    id_length= my_strmov_quoted_identifier (thd, (char *) quoted_table1,
                                            table_name, 0);
    quoted_table1[id_length]= '\0';
    query_length= (uint) (strxmov(tmp_buf2, "drop table ",
                                  quoted_table1, NullS) - tmp_buf2);
    type_str= "drop table";
    break;
  case SOT_RENAME_TABLE:
    /* redo the rename table query as is may contain several tables */
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
  case SOT_ALTER_TABLE:
    type_str= "alter table";
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
  default:
    abort(); /* should not happen, programming error */
  }

  NDB_SCHEMA_OBJECT *ndb_schema_object;
  {
    char key[FN_REFLEN + 1];
    build_table_filename(key, sizeof(key) - 1, db, table_name, "", 0);
    ndb_schema_object= ndb_get_schema_object(key, TRUE, FALSE);
  }

  const NdbError *ndb_error= 0;
  uint32 node_id= g_ndb_cluster_connection->node_id();
  Uint64 epoch= 0;
  MY_BITMAP schema_subscribers;
  uint32 bitbuf[sizeof(ndb_schema_object->slock)/4];
  char bitbuf_e[sizeof(bitbuf)];
  bzero(bitbuf_e, sizeof(bitbuf_e));
  {
    int i, updated= 0;
    int no_storage_nodes= g_ndb_cluster_connection->no_db_nodes();
    bitmap_init(&schema_subscribers, bitbuf, sizeof(bitbuf)*8, FALSE);
    bitmap_set_all(&schema_subscribers);

    /* begin protect ndb_schema_share */
    mysql_mutex_lock(&ndb_schema_share_mutex);
    if (ndb_schema_share == 0)
    {
      mysql_mutex_unlock(&ndb_schema_share_mutex);
      if (ndb_schema_object)
        ndb_free_schema_object(&ndb_schema_object, FALSE);
      DBUG_RETURN(0);    
    }
    mysql_mutex_lock(&ndb_schema_share->mutex);
    for (i= 0; i < no_storage_nodes; i++)
    {
      MY_BITMAP *table_subscribers= &ndb_schema_share->subscriber_bitmap[i];
      if (!bitmap_is_clear_all(table_subscribers))
      {
        bitmap_intersect(&schema_subscribers,
                         table_subscribers);
        updated= 1;
      }
    }
    mysql_mutex_unlock(&ndb_schema_share->mutex);
    mysql_mutex_unlock(&ndb_schema_share_mutex);
    /* end protect ndb_schema_share */

    if (updated)
    {
      bitmap_clear_bit(&schema_subscribers, node_id);
      /*
        if setting own acknowledge bit it is important that
        no other mysqld's are registred, as subsequent code
        will cause the original event to be hidden (by blob
        merge event code)
      */
      if (bitmap_is_clear_all(&schema_subscribers))
          bitmap_set_bit(&schema_subscribers, node_id);
    }
    else
      bitmap_clear_all(&schema_subscribers);

    if (ndb_schema_object)
    {
      mysql_mutex_lock(&ndb_schema_object->mutex);
      memcpy(ndb_schema_object->slock, schema_subscribers.bitmap,
             sizeof(ndb_schema_object->slock));
      mysql_mutex_unlock(&ndb_schema_object->mutex);
    }

    DBUG_DUMP("schema_subscribers", (uchar*)schema_subscribers.bitmap,
              no_bytes_in_map(&schema_subscribers));
    DBUG_PRINT("info", ("bitmap_is_clear_all(&schema_subscribers): %d",
                        bitmap_is_clear_all(&schema_subscribers)));
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
  int retry_sleep= 10; /* 10 milliseconds, transaction */
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
    const char *log_subscribers= (char*)schema_subscribers.bitmap;
    uint32 log_type= (uint32)type;
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
      ndb_pack_varchar(col[SCHEMA_DB_I], tmp_buf, log_db, strlen(log_db));
      r|= op->equal(SCHEMA_DB_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* name */
      ndb_pack_varchar(col[SCHEMA_NAME_I], tmp_buf, log_tab,
                       strlen(log_tab));
      r|= op->equal(SCHEMA_NAME_I, tmp_buf);
      DBUG_ASSERT(r == 0);
      /* slock */
      DBUG_ASSERT(sz[SCHEMA_SLOCK_I] == sizeof(bitbuf));
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
      if (!(thd->variables.option_bits & OPTION_BIN_LOG))
        r|= op->setAnyValue(NDB_ANYVALUE_FOR_NOLOGGING);
      else
        r|= op->setAnyValue(thd->server_id);
      DBUG_ASSERT(r == 0);
      if (log_db != new_db && new_db && new_table_name)
      {
        log_db= new_db;
        log_tab= new_table_name;
        log_subscribers= bitbuf_e; // no ack expected on this
        log_type= (uint32)SOT_RENAME_TABLE_NEW;
        continue;
      }
      break;
    }
    if (trans->execute(NdbTransaction::Commit) == 0)
    {
      DBUG_PRINT("info", ("logged: %s", query));
      break;
    }
err:
    const NdbError *this_error= trans ?
      &trans->getNdbError() : &ndb->getNdbError();
    if (this_error->status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        if (trans)
          ndb->closeTransaction(trans);
        my_sleep(retry_sleep);
        continue; // retry
      }
    }
    ndb_error= this_error;
    break;
  }
end:
  if (ndb_error)
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        ndb_error->code,
                        ndb_error->message,
                        "Could not log query '%s' on other mysqld's");
          
  if (trans)
    ndb->closeTransaction(trans);
  ndb->setDatabaseName(save_db);

  /*
    Wait for other mysqld's to acknowledge the table operation
  */
  if (ndb_error == 0 &&
      !bitmap_is_clear_all(&schema_subscribers))
  {
    /*
      if own nodeid is set we are a single mysqld registred
      as an optimization we update the slock directly
    */
    if (bitmap_is_set(&schema_subscribers, node_id))
      ndbcluster_update_slock(thd, db, table_name);
    else
      dict->forceGCPWait();

    int max_timeout= DEFAULT_SYNC_TIMEOUT;
    mysql_mutex_lock(&ndb_schema_object->mutex);
    while (1)
    {
      struct timespec abstime;
      int i;
      int no_storage_nodes= g_ndb_cluster_connection->no_db_nodes();
      set_timespec(abstime, 1);
      int ret= mysql_cond_timedwait(&injector_cond,
                                    &ndb_schema_object->mutex,
                                    &abstime);
      if (thd->killed)
        break;

      /* begin protect ndb_schema_share */
      mysql_mutex_lock(&ndb_schema_share_mutex);
      if (ndb_schema_share == 0)
      {
        mysql_mutex_unlock(&ndb_schema_share_mutex);
        break;
      }
      mysql_mutex_lock(&ndb_schema_share->mutex);
      for (i= 0; i < no_storage_nodes; i++)
      {
        /* remove any unsubscribed from schema_subscribers */
        MY_BITMAP *tmp= &ndb_schema_share->subscriber_bitmap[i];
        if (!bitmap_is_clear_all(tmp))
          bitmap_intersect(&schema_subscribers, tmp);
      }
      mysql_mutex_unlock(&ndb_schema_share->mutex);
      mysql_mutex_unlock(&ndb_schema_share_mutex);
      /* end protect ndb_schema_share */

      /* remove any unsubscribed from ndb_schema_object->slock */
      bitmap_intersect(&ndb_schema_object->slock_bitmap, &schema_subscribers);

      DBUG_DUMP("ndb_schema_object->slock_bitmap.bitmap",
                (uchar*)ndb_schema_object->slock_bitmap.bitmap,
                no_bytes_in_map(&ndb_schema_object->slock_bitmap));

      if (bitmap_is_clear_all(&ndb_schema_object->slock_bitmap))
        break;

      if (ret)
      {
        max_timeout--;
        if (max_timeout == 0)
        {
          sql_print_error("NDB %s: distributing %s timed out. Ignoring...",
                          type_str, ndb_schema_object->key);
          break;
        }
        if (opt_ndb_extra_logging)
          ndb_report_waiting(type_str, max_timeout,
                             "distributing", ndb_schema_object->key);
      }
    }
    mysql_mutex_unlock(&ndb_schema_object->mutex);
  }

  if (ndb_schema_object)
    ndb_free_schema_object(&ndb_schema_object, FALSE);

  DBUG_RETURN(0);
}

/*
  Handle _non_ data events from the storage nodes
*/
int
ndb_handle_schema_change(THD *thd, Ndb *ndb, NdbEventOperation *pOp,
                         NDB_SHARE *share)
{
  DBUG_ENTER("ndb_handle_schema_change");
  TABLE* table= share->table;
  TABLE_SHARE *table_share= share->table_share;
  const char *dbname= table_share->db.str;
  const char *tabname= table_share->table_name.str;
  bool do_close_cached_tables= FALSE;
  bool is_online_alter_table= FALSE;
  bool is_rename_table= FALSE;
  bool is_remote_change=
    (uint) pOp->getReqNodeId() != g_ndb_cluster_connection->node_id();

  if (pOp->getEventType() == NDBEVENT::TE_ALTER)
  {
    if (pOp->tableFrmChanged())
    {
      DBUG_PRINT("info", ("NDBEVENT::TE_ALTER: table frm changed"));
      is_online_alter_table= TRUE;
    }
    else
    {
      DBUG_PRINT("info", ("NDBEVENT::TE_ALTER: name changed"));
      DBUG_ASSERT(pOp->tableNameChanged());
      is_rename_table= TRUE;
    }
  }

  {
    ndb->setDatabaseName(dbname);
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), tabname);
    const NDBTAB *ev_tab= pOp->getTable();
    const NDBTAB *cache_tab= ndbtab_g.get_table();
    if (cache_tab &&
        cache_tab->getObjectId() == ev_tab->getObjectId() &&
        cache_tab->getObjectVersion() <= ev_tab->getObjectVersion())
      ndbtab_g.invalidate();
  }

  /*
    Refresh local frm file and dictionary cache if
    remote on-line alter table
  */
  if (is_remote_change && is_online_alter_table)
  {
    const char *tabname= table_share->table_name.str;
    char key[FN_REFLEN + 1];
    uchar *data= 0, *pack_data= 0;
    size_t length, pack_length;
    int error;
    NDBDICT *dict= ndb->getDictionary();
    const NDBTAB *altered_table= pOp->getTable();
    
    DBUG_PRINT("info", ("Detected frm change of table %s.%s",
                        dbname, tabname));
    build_table_filename(key, FN_LEN - 1, dbname, tabname, NullS, 0);
    /*
      If the there is no local table shadowing the altered table and 
      it has an frm that is different than the one on disk then 
      overwrite it with the new table definition
    */
    if (!ndbcluster_check_if_local_table(dbname, tabname) &&
	readfrm(key, &data, &length) == 0 &&
        packfrm(data, length, &pack_data, &pack_length) == 0 &&
        cmp_frm(altered_table, pack_data, pack_length))
    {
      DBUG_DUMP("frm", (uchar*) altered_table->getFrmData(), 
                altered_table->getFrmLength());
      Ndb_table_guard ndbtab_g(dict, tabname);
      const NDBTAB *old= ndbtab_g.get_table();
      if (!old &&
          old->getObjectVersion() != altered_table->getObjectVersion())
        dict->putTable(altered_table);
      
      my_free(data);
      data= NULL;
      if ((error= unpackfrm(&data, &length,
                            (const uchar*) altered_table->getFrmData())) ||
          (error= writefrm(key, data, length)))
      {
        sql_print_information("NDB: Failed write frm for %s.%s, error %d",
                              dbname, tabname, error);
      }
      
      // copy names as memory will be freed
      NdbAutoPtr<char> a1((char *)(dbname= strdup(dbname)));
      NdbAutoPtr<char> a2((char *)(tabname= strdup(tabname)));
      ndbcluster_binlog_close_table(thd, share);

      TABLE_LIST table_list;
      bzero((char*) &table_list,sizeof(table_list));
      table_list.db= (char *)dbname;
      table_list.alias= table_list.table_name= (char *)tabname;
      close_cached_tables(thd, &table_list, FALSE, LONG_TIMEOUT);

      if ((error= ndbcluster_binlog_open_table(thd, share,
                                               table_share, table, 1)))
        sql_print_information("NDB: Failed to re-open table %s.%s",
                              dbname, tabname);

      table= share->table;
      table_share= share->table_share;
      dbname= table_share->db.str;
      tabname= table_share->table_name.str;
    }
    my_free(data);
    my_free(pack_data);
  }

  // If only frm was changed continue replicating
  if (is_online_alter_table)
  {
    /* Signal ha_ndbcluster::alter_table that drop is done */
    mysql_cond_signal(&injector_cond);
    DBUG_RETURN(0);
  }

  mysql_mutex_lock(&share->mutex);
  if (is_rename_table && !is_remote_change)
  {
    DBUG_PRINT("info", ("Detected name change of table %s.%s",
                        share->db, share->table_name));
    /* ToDo: remove printout */
    if (opt_ndb_extra_logging)
      sql_print_information("NDB Binlog: rename table %s%s/%s -> %s.",
                            share_prefix, share->table->s->db.str,
                            share->table->s->table_name.str,
                            share->key);
    {
      ndb->setDatabaseName(share->table->s->db.str);
      Ndb_table_guard ndbtab_g(ndb->getDictionary(),
                               share->table->s->table_name.str);
      const NDBTAB *ev_tab= pOp->getTable();
      const NDBTAB *cache_tab= ndbtab_g.get_table();
      if (cache_tab &&
          cache_tab->getObjectId() == ev_tab->getObjectId() &&
          cache_tab->getObjectVersion() <= ev_tab->getObjectVersion())
        ndbtab_g.invalidate();
    }
    /* do the rename of the table in the share */
    share->table->s->db.str= share->db;
    share->table->s->db.length= strlen(share->db);
    share->table->s->table_name.str= share->table_name;
    share->table->s->table_name.length= strlen(share->table_name);
  }
  DBUG_ASSERT(share->op == pOp || share->op_old == pOp);
  if (share->op_old == pOp)
    share->op_old= 0;
  else
    share->op= 0;
  // either just us or drop table handling as well
      
  /* Signal ha_ndbcluster::delete/rename_table that drop is done */
  mysql_mutex_unlock(&share->mutex);
  mysql_cond_signal(&injector_cond);

  mysql_mutex_lock(&ndbcluster_mutex);
  /* ndb_share reference binlog free */
  DBUG_PRINT("NDB_SHARE", ("%s binlog free  use_count: %u",
                           share->key, share->use_count));
  free_share(&share, TRUE);
  if (is_remote_change && share && share->state != NSS_DROPPED)
  {
    DBUG_PRINT("info", ("remote change"));
    share->state= NSS_DROPPED;
    if (share->use_count != 1)
    {
      /* open handler holding reference */
      /* wait with freeing create ndb_share to below */
      do_close_cached_tables= TRUE;
    }
    else
    {
      /* ndb_share reference create free */
      DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share, TRUE);
      share= 0;
    }
  }
  else
    share= 0;
  mysql_mutex_unlock(&ndbcluster_mutex);

  pOp->setCustomData(0);

  mysql_mutex_lock(&injector_mutex);
  ndb->dropEventOperation(pOp);
  pOp= 0;
  mysql_mutex_unlock(&injector_mutex);

  if (do_close_cached_tables)
  {
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    table_list.db= (char *)dbname;
    table_list.alias= table_list.table_name= (char *)tabname;
    close_cached_tables(thd, &table_list, FALSE, LONG_TIMEOUT);
    /* ndb_share reference create free */
    DBUG_PRINT("NDB_SHARE", ("%s create free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share);
  }
  DBUG_RETURN(0);
}

static void ndb_binlog_query(THD *thd, Cluster_schema *schema)
{
  if (schema->any_value & NDB_ANYVALUE_RESERVED)
  {
    if (schema->any_value != NDB_ANYVALUE_FOR_NOLOGGING)
      sql_print_warning("NDB: unknown value for binlog signalling 0x%X, "
                        "query not logged",
                        schema->any_value);
    return;
  }
  uint32 thd_server_id_save= thd->server_id;
  DBUG_ASSERT(sizeof(thd_server_id_save) == sizeof(thd->server_id));
  char *thd_db_save= thd->db;
  if (schema->any_value == 0)
    thd->server_id= ::server_id;
  else
    thd->server_id= schema->any_value;
  thd->db= schema->db;
  int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
  thd->binlog_query(THD::STMT_QUERY_TYPE, schema->query,
                    schema->query_length, FALSE, TRUE,
                    schema->name[0] == 0 || thd->db[0] == 0,
                    errcode);
  thd->server_id= thd_server_id_save;
  thd->db= thd_db_save;
}

static int
ndb_binlog_thread_handle_schema_event(THD *thd, Ndb *ndb,
                                      NdbEventOperation *pOp,
                                      List<Cluster_schema> 
                                      *post_epoch_log_list,
                                      List<Cluster_schema> 
                                      *post_epoch_unlock_list,
                                      MEM_ROOT *mem_root)
{
  DBUG_ENTER("ndb_binlog_thread_handle_schema_event");
  NDB_SHARE *tmp_share= (NDB_SHARE *)pOp->getCustomData();
  if (tmp_share && ndb_schema_share == tmp_share)
  {
    NDBEVENT::TableEvent ev_type= pOp->getEventType();
    DBUG_PRINT("enter", ("%s.%s  ev_type: %d",
                         tmp_share->db, tmp_share->table_name, ev_type));
    if (ev_type == NDBEVENT::TE_UPDATE ||
        ev_type == NDBEVENT::TE_INSERT)
    {
      Cluster_schema *schema= (Cluster_schema *)
        sql_alloc(sizeof(Cluster_schema));
      MY_BITMAP slock;
      bitmap_init(&slock, schema->slock, 8*SCHEMA_SLOCK_SIZE, FALSE);
      uint node_id= g_ndb_cluster_connection->node_id();
      {
        ndbcluster_get_schema(tmp_share, schema);
        schema->any_value= pOp->getAnyValue();
      }
      enum SCHEMA_OP_TYPE schema_type= (enum SCHEMA_OP_TYPE)schema->type;
      DBUG_PRINT("info",
                 ("%s.%s: log query_length: %d  query: '%s'  type: %d",
                  schema->db, schema->name,
                  schema->query_length, schema->query,
                  schema_type));
      if (schema_type == SOT_CLEAR_SLOCK)
      {
        /*
          handle slock after epoch is completed to ensure that
          schema events get inserted in the binlog after any data
          events
        */
        post_epoch_log_list->push_back(schema, mem_root);
        DBUG_RETURN(0);
      }
      if (schema->node_id != node_id)
      {
        int log_query= 0, post_epoch_unlock= 0;
        switch (schema_type)
        {
        case SOT_DROP_TABLE:
          // fall through
        case SOT_RENAME_TABLE:
          // fall through
        case SOT_RENAME_TABLE_NEW:
          // fall through
        case SOT_ALTER_TABLE:
          post_epoch_log_list->push_back(schema, mem_root);
          /* acknowledge this query _after_ epoch completion */
          post_epoch_unlock= 1;
          break;
	case SOT_TRUNCATE_TABLE:
        {
          char key[FN_REFLEN + 1];
          build_table_filename(key, sizeof(key) - 1,
                               schema->db, schema->name, "", 0);
          /* ndb_share reference temporary, free below */
          NDB_SHARE *share= get_share(key, 0, FALSE, FALSE);
          if (share)
          {
            DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                                     share->key, share->use_count));
          }
          // invalidation already handled by binlog thread
          if (!share || !share->op)
          {
            {
              injector_ndb->setDatabaseName(schema->db);
              Ndb_table_guard ndbtab_g(injector_ndb->getDictionary(),
                                       schema->name);
              ndbtab_g.invalidate();
            }
            TABLE_LIST table_list;
            bzero((char*) &table_list,sizeof(table_list));
            table_list.db= schema->db;
            table_list.alias= table_list.table_name= schema->name;
            close_cached_tables(thd, &table_list, FALSE, LONG_TIMEOUT);
          }
          /* ndb_share reference temporary free */
          if (share)
          {
            DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                     share->key, share->use_count));
            free_share(&share);
          }
        }
        // fall through
        case SOT_CREATE_TABLE:
          if (ndbcluster_check_if_local_table(schema->db, schema->name))
          {
            DBUG_PRINT("info", ("NDB Binlog: Skipping locally defined table '%s.%s'",
                                schema->db, schema->name));
            sql_print_error("NDB Binlog: Skipping locally defined table '%s.%s' from "
                            "binlog schema event '%s' from node %d. ",
                            schema->db, schema->name, schema->query,
                            schema->node_id);
          }
          else if (ndb_create_table_from_engine(thd, schema->db, schema->name))
          {
            print_could_not_discover_error(thd, schema);
          }
          log_query= 1;
          break;
        case SOT_DROP_DB:
          /* Drop the database locally if it only contains ndb tables */
          if (! ndbcluster_check_if_local_tables_in_db(thd, schema->db))
          {
            const int no_print_error[1]= {0};
            run_query(thd, schema->query,
                      schema->query + schema->query_length,
                      no_print_error,    /* print error */
                      TRUE);   /* don't binlog the query */
            /* binlog dropping database after any table operations */
            post_epoch_log_list->push_back(schema, mem_root);
            /* acknowledge this query _after_ epoch completion */
            post_epoch_unlock= 1;
          }
          else
          {
            /* Database contained local tables, leave it */
            sql_print_error("NDB Binlog: Skipping drop database '%s' since it contained local tables "
                            "binlog schema event '%s' from node %d. ",
                            schema->db, schema->query,
                            schema->node_id);
            log_query= 1;
          }
          break;
        case SOT_CREATE_DB:
          /* fall through */
        case SOT_ALTER_DB:
        {
          const int no_print_error[1]= {0};
          run_query(thd, schema->query,
                    schema->query + schema->query_length,
                    no_print_error,    /* print error */
                    TRUE);   /* don't binlog the query */
          log_query= 1;
          break;
        }
        case SOT_TABLESPACE:
        case SOT_LOGFILE_GROUP:
          log_query= 1;
          break;
        case SOT_CLEAR_SLOCK:
          abort();
        }
        if (log_query && ndb_binlog_running)
          ndb_binlog_query(thd, schema);
        /* signal that schema operation has been handled */
        DBUG_DUMP("slock", (uchar*) schema->slock, schema->slock_length);
        if (bitmap_is_set(&slock, node_id))
        {
          if (post_epoch_unlock)
            post_epoch_unlock_list->push_back(schema, mem_root);
          else
            ndbcluster_update_slock(thd, schema->db, schema->name);
        }
      }
      DBUG_RETURN(0);
    }
    /*
      the normal case of UPDATE/INSERT has already been handled
    */
    switch (ev_type)
    {
    case NDBEVENT::TE_DELETE:
      // skip
      break;
    case NDBEVENT::TE_CLUSTER_FAILURE:
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: cluster failure for %s at epoch %u.",
                              ndb_schema_share->key, (unsigned) pOp->getGCI());
      // fall through
    case NDBEVENT::TE_DROP:
      if (opt_ndb_extra_logging &&
          ndb_binlog_tables_inited && ndb_binlog_running)
        sql_print_information("NDB Binlog: ndb tables initially "
                              "read only on reconnect.");

      /* begin protect ndb_schema_share */
      mysql_mutex_lock(&ndb_schema_share_mutex);
      /* ndb_share reference binlog extra free */
      DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                               ndb_schema_share->key,
                               ndb_schema_share->use_count));
      free_share(&ndb_schema_share);
      ndb_schema_share= 0;
      ndb_binlog_tables_inited= 0;
      mysql_mutex_unlock(&ndb_schema_share_mutex);
      /* end protect ndb_schema_share */

      close_cached_tables(NULL, NULL, FALSE, LONG_TIMEOUT);
      // fall through
    case NDBEVENT::TE_ALTER:
      ndb_handle_schema_change(thd, ndb, pOp, tmp_share);
      break;
    case NDBEVENT::TE_NODE_FAILURE:
    {
      uint8 node_id= g_node_id_map[pOp->getNdbdNodeId()];
      DBUG_ASSERT(node_id != 0xFF);
      mysql_mutex_lock(&tmp_share->mutex);
      bitmap_clear_all(&tmp_share->subscriber_bitmap[node_id]);
      DBUG_PRINT("info",("NODE_FAILURE UNSUBSCRIBE[%d]", node_id));
      if (opt_ndb_extra_logging)
      {
        sql_print_information("NDB Binlog: Node: %d, down,"
                              " Subscriber bitmask %x%x",
                              pOp->getNdbdNodeId(),
                              tmp_share->subscriber_bitmap[node_id].bitmap[1],
                              tmp_share->subscriber_bitmap[node_id].bitmap[0]);
      }
      mysql_mutex_unlock(&tmp_share->mutex);
      mysql_cond_signal(&injector_cond);
      break;
    }
    case NDBEVENT::TE_SUBSCRIBE:
    {
      uint8 node_id= g_node_id_map[pOp->getNdbdNodeId()];
      uint8 req_id= pOp->getReqNodeId();
      DBUG_ASSERT(req_id != 0 && node_id != 0xFF);
      mysql_mutex_lock(&tmp_share->mutex);
      bitmap_set_bit(&tmp_share->subscriber_bitmap[node_id], req_id);
      DBUG_PRINT("info",("SUBSCRIBE[%d] %d", node_id, req_id));
      if (opt_ndb_extra_logging)
      {
        sql_print_information("NDB Binlog: Node: %d, subscribe from node %d,"
                              " Subscriber bitmask %x%x",
                              pOp->getNdbdNodeId(),
                              req_id,
                              tmp_share->subscriber_bitmap[node_id].bitmap[1],
                              tmp_share->subscriber_bitmap[node_id].bitmap[0]);
      }
      mysql_mutex_unlock(&tmp_share->mutex);
      mysql_cond_signal(&injector_cond);
      break;
    }
    case NDBEVENT::TE_UNSUBSCRIBE:
    {
      uint8 node_id= g_node_id_map[pOp->getNdbdNodeId()];
      uint8 req_id= pOp->getReqNodeId();
      DBUG_ASSERT(req_id != 0 && node_id != 0xFF);
      mysql_mutex_lock(&tmp_share->mutex);
      bitmap_clear_bit(&tmp_share->subscriber_bitmap[node_id], req_id);
      DBUG_PRINT("info",("UNSUBSCRIBE[%d] %d", node_id, req_id));
      if (opt_ndb_extra_logging)
      {
        sql_print_information("NDB Binlog: Node: %d, unsubscribe from node %d,"
                              " Subscriber bitmask %x%x",
                              pOp->getNdbdNodeId(),
                              req_id,
                              tmp_share->subscriber_bitmap[node_id].bitmap[1],
                              tmp_share->subscriber_bitmap[node_id].bitmap[0]);
      }
      mysql_mutex_unlock(&tmp_share->mutex);
      mysql_cond_signal(&injector_cond);
      break;
    }
    default:
      sql_print_error("NDB Binlog: unknown non data event %d for %s. "
                      "Ignoring...", (unsigned) ev_type, tmp_share->key);
    }
  }
  DBUG_RETURN(0);
}

/*
  process any operations that should be done after
  the epoch is complete
*/
static void
ndb_binlog_thread_handle_schema_event_post_epoch(THD *thd,
                                                 List<Cluster_schema>
                                                 *post_epoch_log_list,
                                                 List<Cluster_schema>
                                                 *post_epoch_unlock_list)
{
  if (post_epoch_log_list->elements == 0)
    return;
  DBUG_ENTER("ndb_binlog_thread_handle_schema_event_post_epoch");
  Cluster_schema *schema;
  while ((schema= post_epoch_log_list->pop()))
  {
    DBUG_PRINT("info",
               ("%s.%s: log query_length: %d  query: '%s'  type: %d",
                schema->db, schema->name,
                schema->query_length, schema->query,
                schema->type));
    int log_query= 0;
    {
      enum SCHEMA_OP_TYPE schema_type= (enum SCHEMA_OP_TYPE)schema->type;
      char key[FN_REFLEN + 1];
      build_table_filename(key, sizeof(key) - 1, schema->db, schema->name, "", 0);
      if (schema_type == SOT_CLEAR_SLOCK)
      {
        mysql_mutex_lock(&ndbcluster_mutex);
        NDB_SCHEMA_OBJECT *ndb_schema_object=
          (NDB_SCHEMA_OBJECT*) my_hash_search(&ndb_schema_objects,
                                              (uchar*) key, strlen(key));
        if (ndb_schema_object)
        {
          mysql_mutex_lock(&ndb_schema_object->mutex);
          memcpy(ndb_schema_object->slock, schema->slock,
                 sizeof(ndb_schema_object->slock));
          DBUG_DUMP("ndb_schema_object->slock_bitmap.bitmap",
                    (uchar*)ndb_schema_object->slock_bitmap.bitmap,
                    no_bytes_in_map(&ndb_schema_object->slock_bitmap));
          mysql_mutex_unlock(&ndb_schema_object->mutex);
          mysql_cond_signal(&injector_cond);
        }
        mysql_mutex_unlock(&ndbcluster_mutex);
        continue;
      }
      /* ndb_share reference temporary, free below */
      NDB_SHARE *share= get_share(key, 0, FALSE, FALSE);
      if (share)
      {
        DBUG_PRINT("NDB_SHARE", ("%s temporary  use_count: %u",
                                 share->key, share->use_count));
      }
      switch (schema_type)
      {
      case SOT_DROP_DB:
        log_query= 1;
        break;
      case SOT_DROP_TABLE:
        log_query= 1;
        // invalidation already handled by binlog thread
        if (share && share->op)
        {
          break;
        }
        // fall through
      case SOT_RENAME_TABLE:
        // fall through
      case SOT_ALTER_TABLE:
        // invalidation already handled by binlog thread
        if (!share || !share->op)
        {
          {
            injector_ndb->setDatabaseName(schema->db);
            Ndb_table_guard ndbtab_g(injector_ndb->getDictionary(),
                                     schema->name);
            ndbtab_g.invalidate();
          }
          TABLE_LIST table_list;
          bzero((char*) &table_list,sizeof(table_list));
          table_list.db= schema->db;
          table_list.alias= table_list.table_name= schema->name;
          close_cached_tables(thd, &table_list, FALSE, LONG_TIMEOUT);
        }
        if (schema_type != SOT_ALTER_TABLE)
          break;
        // fall through
      case SOT_RENAME_TABLE_NEW:
        log_query= 1;
        if (ndb_binlog_running && (!share || !share->op))
        {
          /*
            we need to free any share here as command below
            may need to call handle_trailing_share
          */
          if (share)
          {
            /* ndb_share reference temporary free */
            DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                     share->key, share->use_count));
            free_share(&share);
            share= 0;
          }
          if (ndbcluster_check_if_local_table(schema->db, schema->name))
          {
            DBUG_PRINT("info", ("NDB Binlog: Skipping locally defined table '%s.%s'",
                                schema->db, schema->name));
            sql_print_error("NDB Binlog: Skipping locally defined table '%s.%s' from "
                            "binlog schema event '%s' from node %d. ",
                            schema->db, schema->name, schema->query,
                            schema->node_id);
          }
          else if (ndb_create_table_from_engine(thd, schema->db, schema->name))
          {
            print_could_not_discover_error(thd, schema);
          }
        }
        break;
      default:
        DBUG_ASSERT(FALSE);
      }
      if (share)
      {
        /* ndb_share reference temporary free */
        DBUG_PRINT("NDB_SHARE", ("%s temporary free  use_count: %u",
                                 share->key, share->use_count));
        free_share(&share);
        share= 0;
      }
    }
    if (ndb_binlog_running && log_query)
      ndb_binlog_query(thd, schema);
  }
  while ((schema= post_epoch_unlock_list->pop()))
  {
    ndbcluster_update_slock(thd, schema->db, schema->name);
  }
  DBUG_VOID_RETURN;
}

/*
  Timer class for doing performance measurements
*/

/*********************************************************************
  Internal helper functions for handeling of the cluster replication tables
  - ndb_binlog_index
  - ndb_apply_status
*********************************************************************/

/*
  struct to hold the data to be inserted into the
  ndb_binlog_index table
*/
struct ndb_binlog_index_row {
  ulonglong gci;
  const char *master_log_file;
  ulonglong master_log_pos;
  ulonglong n_inserts;
  ulonglong n_updates;
  ulonglong n_deletes;
  ulonglong n_schemaops;
};

/*
  Open the ndb_binlog_index table
*/
static int open_ndb_binlog_index(THD *thd, TABLE **ndb_binlog_index)
{
  static char repdb[]= NDB_REP_DB;
  static char reptable[]= NDB_REP_TABLE;
  const char *save_proc_info= thd->proc_info;
  TABLE_LIST *tables= &binlog_tables;

  tables->init_one_table(repdb, strlen(repdb), reptable, strlen(reptable),
                         reptable, TL_WRITE);
  thd->proc_info= "Opening " NDB_REP_DB "." NDB_REP_TABLE;

  tables->required_type= FRMTYPE_TABLE;
  thd->clear_error();
  if (open_and_lock_tables(thd, tables, FALSE, 0))
  {
    if (thd->killed)
      sql_print_error("NDB Binlog: Opening ndb_binlog_index: killed");
    else
      sql_print_error("NDB Binlog: Opening ndb_binlog_index: %d, '%s'",
                      thd->stmt_da->sql_errno(),
                      thd->stmt_da->message());
    thd->proc_info= save_proc_info;
    return -1;
  }
  *ndb_binlog_index= tables->table;
  thd->proc_info= save_proc_info;
  (*ndb_binlog_index)->use_all_columns();
  return 0;
}


/*
  Insert one row in the ndb_binlog_index
*/

int ndb_add_ndb_binlog_index(THD *thd, void *_row)
{
  ndb_binlog_index_row &row= *(ndb_binlog_index_row *) _row;
  int error= 0;
  /*
    Turn of binlogging to prevent the table changes to be written to
    the binary log.
  */
  ulong saved_options= thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_BIN_LOG;

  if (!ndb_binlog_index && open_ndb_binlog_index(thd, &ndb_binlog_index))
  {
    sql_print_error("NDB Binlog: Unable to lock table ndb_binlog_index");
    error= -1;
    goto add_ndb_binlog_index_err;
  }

  /*
    Intialize ndb_binlog_index->record[0]
  */
  empty_record(ndb_binlog_index);

  ndb_binlog_index->field[0]->store(row.master_log_pos);
  ndb_binlog_index->field[1]->store(row.master_log_file,
                                strlen(row.master_log_file),
                                &my_charset_bin);
  ndb_binlog_index->field[2]->store(row.gci);
  ndb_binlog_index->field[3]->store(row.n_inserts);
  ndb_binlog_index->field[4]->store(row.n_updates);
  ndb_binlog_index->field[5]->store(row.n_deletes);
  ndb_binlog_index->field[6]->store(row.n_schemaops);

  if ((error= ndb_binlog_index->file->ha_write_row(ndb_binlog_index->record[0])))
  {
    sql_print_error("NDB Binlog: Writing row to ndb_binlog_index: %d", error);
    error= -1;
    goto add_ndb_binlog_index_err;
  }

add_ndb_binlog_index_err:
  thd->stmt_da->can_overwrite_status= TRUE;
  thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
  thd->stmt_da->can_overwrite_status= FALSE;
  close_thread_tables(thd);
  /*
    There should be no need for rolling back transaction due to deadlock
    (since ndb_binlog_index is non transactional).
  */
  DBUG_ASSERT(! thd->transaction_rollback_request);

  thd->mdl_context.release_transactional_locks();
  ndb_binlog_index= 0;
  thd->variables.option_bits= saved_options;
  return error;
}

/*********************************************************************
  Functions for start, stop, wait for ndbcluster binlog thread
*********************************************************************/

enum Binlog_thread_state
{
  BCCC_running= 0,
  BCCC_exit= 1,
  BCCC_restart= 2
};

static enum Binlog_thread_state do_ndbcluster_binlog_close_connection= BCCC_restart;

int ndbcluster_binlog_start()
{
  DBUG_ENTER("ndbcluster_binlog_start");

  if (::server_id == 0)
  {
    sql_print_warning("NDB: server id set to zero will cause any other mysqld "
                      "with bin log to log with wrong server id");
  }
  else if (::server_id & 0x1 << 31)
  {
    sql_print_error("NDB: server id's with high bit set is reserved for internal "
                    "purposes");
    DBUG_RETURN(-1);
  }

  mysql_mutex_init(key_injector_mutex, &injector_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_injector_cond, &injector_cond, NULL);
  mysql_mutex_init(key_ndb_schema_share_mutex,
                   &ndb_schema_share_mutex, MY_MUTEX_INIT_FAST);

  /* Create injector thread */
  if (mysql_thread_create(key_thread_ndb_binlog,
                          &ndb_binlog_thread, &connection_attrib,
                          ndb_binlog_thread_func, 0))
  {
    DBUG_PRINT("error", ("Could not create ndb injector thread"));
    mysql_cond_destroy(&injector_cond);
    mysql_mutex_destroy(&injector_mutex);
    DBUG_RETURN(-1);
  }

  ndbcluster_binlog_inited= 1;

  /* Wait for the injector thread to start */
  mysql_mutex_lock(&injector_mutex);
  while (!ndb_binlog_thread_running)
    mysql_cond_wait(&injector_cond, &injector_mutex);
  mysql_mutex_unlock(&injector_mutex);

  if (ndb_binlog_thread_running < 0)
    DBUG_RETURN(-1);

  DBUG_RETURN(0);
}


/**************************************************************
  Internal helper functions for creating/dropping ndb events
  used by the client sql threads
**************************************************************/
void
ndb_rep_event_name(String *event_name,const char *db, const char *tbl)
{
  event_name->set_ascii("REPL$", 5);
  event_name->append(db);
  if (tbl)
  {
    event_name->append('/');
    event_name->append(tbl);
  }
}

bool
ndbcluster_check_if_local_table(const char *dbname, const char *tabname)
{
  char key[FN_REFLEN + 1];
  char ndb_file[FN_REFLEN + 1];

  DBUG_ENTER("ndbcluster_check_if_local_table");
  build_table_filename(key, FN_LEN-1, dbname, tabname, reg_ext, 0);
  build_table_filename(ndb_file, FN_LEN-1, dbname, tabname, ha_ndb_ext, 0);
  /* Check that any defined table is an ndb table */
  DBUG_PRINT("info", ("Looking for file %s and %s", key, ndb_file));
  if ((! my_access(key, F_OK)) && my_access(ndb_file, F_OK))
  {
    DBUG_PRINT("info", ("table file %s not on disk, local table", ndb_file));   
  
  
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

bool
ndbcluster_check_if_local_tables_in_db(THD *thd, const char *dbname)
{
  DBUG_ENTER("ndbcluster_check_if_local_tables_in_db");
  DBUG_PRINT("info", ("Looking for files in directory %s", dbname));
  LEX_STRING *tabname;
  List<LEX_STRING> files;
  char path[FN_REFLEN + 1];

  build_table_filename(path, sizeof(path) - 1, dbname, "", "", 0);
  if (find_files(thd, &files, dbname, path, NullS, 0) != FIND_FILES_OK)
  {
    DBUG_PRINT("info", ("Failed to find files"));
    DBUG_RETURN(true);
  }
  DBUG_PRINT("info",("found: %d files", files.elements));
  while ((tabname= files.pop()))
  {
    DBUG_PRINT("info", ("Found table %s", tabname->str));
    if (ndbcluster_check_if_local_table(dbname, tabname->str))
      DBUG_RETURN(true);
  }
  
  DBUG_RETURN(false);
}

/*
  Common function for setting up everything for logging a table at
  create/discover.
*/
int ndbcluster_create_binlog_setup(Ndb *ndb, const char *key,
                                   uint key_len,
                                   const char *db,
                                   const char *table_name,
                                   my_bool share_may_exist)
{
  int do_event_op= ndb_binlog_running;
  DBUG_ENTER("ndbcluster_create_binlog_setup");
  DBUG_PRINT("enter",("key: %s  key_len: %d  %s.%s  share_may_exist: %d",
                      key, key_len, db, table_name, share_may_exist));
  DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(table_name));
  DBUG_ASSERT(strlen(key) == key_len);

  mysql_mutex_lock(&ndbcluster_mutex);

  /* Handle any trailing share */
  NDB_SHARE *share= (NDB_SHARE*) my_hash_search(&ndbcluster_open_tables,
                                                (uchar*) key, key_len);

  if (share && share_may_exist)
  {
    if (share->flags & NSF_NO_BINLOG ||
        share->op != 0 ||
        share->op_old != 0)
    {
      mysql_mutex_unlock(&ndbcluster_mutex);
      DBUG_RETURN(0); // replication already setup, or should not
    }
  }

  if (share)
  {
    if (share->op || share->op_old)
    {
      my_errno= HA_ERR_TABLE_EXIST;
      mysql_mutex_unlock(&ndbcluster_mutex);
      DBUG_RETURN(1);
    }
    if (!share_may_exist || share->connect_count != 
        g_ndb_cluster_connection->get_connect_count())
    {
      handle_trailing_share(share);
      share= NULL;
    }
  }

  /* Create share which is needed to hold replication information */
  if (share)
  {
    /* ndb_share reference create */
    ++share->use_count;
    DBUG_PRINT("NDB_SHARE", ("%s create  use_count: %u",
                             share->key, share->use_count));
  }
  /* ndb_share reference create */
  else if (!(share= get_share(key, 0, TRUE, TRUE)))
  {
    sql_print_error("NDB Binlog: "
                    "allocating table share for %s failed", key);
  }
  else
  {
    DBUG_PRINT("NDB_SHARE", ("%s create  use_count: %u",
                             share->key, share->use_count));
  }

  if (!ndb_schema_share &&
      strcmp(share->db, NDB_REP_DB) == 0 &&
      strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
    do_event_op= 1;
  else if (!ndb_apply_status_share &&
           strcmp(share->db, NDB_REP_DB) == 0 &&
           strcmp(share->table_name, NDB_APPLY_TABLE) == 0)
    do_event_op= 1;

  if (!do_event_op)
  {
    share->flags|= NSF_NO_BINLOG;
    mysql_mutex_unlock(&ndbcluster_mutex);
    DBUG_RETURN(0);
  }
  mysql_mutex_unlock(&ndbcluster_mutex);

  while (share && !IS_TMP_PREFIX(table_name))
  {
    /*
      ToDo make sanity check of share so that the table is actually the same
      I.e. we need to do open file from frm in this case
      Currently awaiting this to be fixed in the 4.1 tree in the general
      case
    */

    /* Create the event in NDB */
    ndb->setDatabaseName(db);

    NDBDICT *dict= ndb->getDictionary();
    Ndb_table_guard ndbtab_g(dict, table_name);
    const NDBTAB *ndbtab= ndbtab_g.get_table();
    if (ndbtab == 0)
    {
      if (opt_ndb_extra_logging)
        sql_print_information("NDB Binlog: Failed to get table %s from ndb: "
                              "%s, %d", key, dict->getNdbError().message,
                              dict->getNdbError().code);
      break; // error
    }
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, db, table_name);
    /*
      event should have been created by someone else,
      but let's make sure, and create if it doesn't exist
    */
    const NDBEVENT *ev= dict->getEvent(event_name.c_ptr());
    if (!ev)
    {
      if (ndbcluster_create_event(ndb, ndbtab, event_name.c_ptr(), share))
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
    if (ndbcluster_create_event_ops(share, ndbtab, event_name.c_ptr()))
    {
      sql_print_error("NDB Binlog:"
                      "FAILED CREATE (DISCOVER) EVENT OPERATIONS Event: %s",
                      event_name.c_ptr());
      /* a warning has been issued to the client */
      DBUG_RETURN(0);
    }
    DBUG_RETURN(0);
  }
  DBUG_RETURN(-1);
}

int
ndbcluster_create_event(Ndb *ndb, const NDBTAB *ndbtab,
                        const char *event_name, NDB_SHARE *share,
                        int push_warning)
{
  THD *thd= current_thd;
  DBUG_ENTER("ndbcluster_create_event");
  DBUG_PRINT("info", ("table=%s version=%d event=%s share=%s",
                      ndbtab->getName(), ndbtab->getObjectVersion(),
                      event_name, share ? share->key : "(nil)"));
  DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(ndbtab->getName()));
  if (!share)
  {
    DBUG_PRINT("info", ("share == NULL"));
    DBUG_RETURN(0);
  }
  if (share->flags & NSF_NO_BINLOG)
  {
    DBUG_PRINT("info", ("share->flags & NSF_NO_BINLOG, flags: %x %d",
                        share->flags, share->flags & NSF_NO_BINLOG));
    DBUG_RETURN(0);
  }

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
                      share->key);
      if (push_warning)
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            ER(ER_ILLEGAL_HA_CREATE_OPTION),
                            ndbcluster_hton_name,
                            "Binlog of table with BLOB attribute and no PK");

      share->flags|= NSF_NO_BINLOG;
      DBUG_RETURN(-1);
    }
    /* No primary key, subscribe for all attributes */
    my_event.setReport(NDBEVENT::ER_ALL);
    DBUG_PRINT("info", ("subscription all"));
  }
  else
  {
    if (ndb_schema_share || strcmp(share->db, NDB_REP_DB) ||
        strcmp(share->table_name, NDB_SCHEMA_TABLE))
    {
      my_event.setReport(NDBEVENT::ER_UPDATED);
      DBUG_PRINT("info", ("subscription only updated"));
    }
    else
    {
      my_event.setReport((NDBEVENT::EventReport)
                         (NDBEVENT::ER_ALL | NDBEVENT::ER_SUBSCRIBE));
      DBUG_PRINT("info", ("subscription all and subscribe"));
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
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
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
        dict->dropEvent(my_event.getName()))
    {
      if (push_warning > 1)
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
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
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
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
#ifdef NDB_BINLOG_EXTRA_WARNINGS
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        0, "NDB Binlog: Removed trailing event",
                        "NDB");
#endif
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
ndbcluster_create_event_ops(NDB_SHARE *share, const NDBTAB *ndbtab,
                            const char *event_name)
{
  THD *thd= current_thd;
  /*
    we are in either create table or rename table so table should be
    locked, hence we can work with the share without locks
  */

  DBUG_ENTER("ndbcluster_create_event_ops");
  DBUG_PRINT("enter", ("table: %s event: %s", ndbtab->getName(), event_name));
  DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(ndbtab->getName()));

  DBUG_ASSERT(share != 0);

  if (share->flags & NSF_NO_BINLOG)
  {
    DBUG_PRINT("info", ("share->flags & NSF_NO_BINLOG, flags: %x",
                        share->flags));
    DBUG_RETURN(0);
  }

  int do_ndb_schema_share= 0, do_ndb_apply_status_share= 0;
  if (!ndb_schema_share && strcmp(share->db, NDB_REP_DB) == 0 &&
      strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
    do_ndb_schema_share= 1;
  else if (!ndb_apply_status_share && strcmp(share->db, NDB_REP_DB) == 0 &&
           strcmp(share->table_name, NDB_APPLY_TABLE) == 0)
    do_ndb_apply_status_share= 1;
  else if (!binlog_filter->db_ok(share->db) || !ndb_binlog_running)
  {
    share->flags|= NSF_NO_BINLOG;
    DBUG_RETURN(0);
  }

  if (share->op)
  {
    assert(share->op->getCustomData() == (void *) share);

    DBUG_ASSERT(share->use_count > 1);
    sql_print_error("NDB Binlog: discover reusing old ev op");
    /* ndb_share reference ToDo free */
    DBUG_PRINT("NDB_SHARE", ("%s ToDo free  use_count: %u",
                             share->key, share->use_count));
    free_share(&share); // old event op already has reference
    DBUG_RETURN(0);
  }

  TABLE *table= share->table;

  int retries= 100;
  /*
    100 milliseconds, temporary error on schema operation can
    take some time to be resolved
  */
  int retry_sleep= 100;
  while (1)
  {
    mysql_mutex_lock(&injector_mutex);
    Ndb *ndb= injector_ndb;
    if (do_ndb_schema_share)
      ndb= schema_ndb;

    if (ndb == 0)
    {
      mysql_mutex_unlock(&injector_mutex);
      DBUG_RETURN(-1);
    }

    NdbEventOperation* op;
    if (do_ndb_schema_share)
      op= ndb->createEventOperation(event_name);
    else
    {
      // set injector_ndb database/schema from table internal name
      int ret= ndb->setDatabaseAndSchemaName(ndbtab);
      assert(ret == 0);
      op= ndb->createEventOperation(event_name);
      // reset to catch errors
      ndb->setDatabaseName("");
    }
    if (!op)
    {
      sql_print_error("NDB Binlog: Creating NdbEventOperation failed for"
                      " %s",event_name);
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                          ndb->getNdbError().code,
                          ndb->getNdbError().message,
                          "NDB");
      mysql_mutex_unlock(&injector_mutex);
      DBUG_RETURN(-1);
    }

    if (share->flags & NSF_BLOB_FLAG)
      op->mergeEvents(TRUE); // currently not inherited from event

    DBUG_PRINT("info", ("share->ndb_value[0]: 0x%lx  share->ndb_value[1]: 0x%lx",
                        (long) share->ndb_value[0],
                        (long) share->ndb_value[1]));
    int n_columns= ndbtab->getNoOfColumns();
    int n_fields= table ? table->s->fields : 0; // XXX ???
    for (int j= 0; j < n_columns; j++)
    {
      const char *col_name= ndbtab->getColumn(j)->getName();
      NdbValue attr0, attr1;
      if (j < n_fields)
      {
        Field *f= share->table->field[j];
        if (is_ndb_compatible_type(f))
        {
          DBUG_PRINT("info", ("%s compatible", col_name));
          attr0.rec= op->getValue(col_name, (char*) f->ptr);
          attr1.rec= op->getPreValue(col_name,
                                     (f->ptr - share->table->record[0]) +
                                     (char*) share->table->record[1]);
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
            push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                                op->getNdbError().code,
                                op->getNdbError().message,
                                "NDB");
            ndb->dropEventOperation(op);
            mysql_mutex_unlock(&injector_mutex);
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
      share->ndb_value[0][j].ptr= attr0.ptr;
      share->ndb_value[1][j].ptr= attr1.ptr;
      DBUG_PRINT("info", ("&share->ndb_value[0][%d]: 0x%lx  "
                          "share->ndb_value[0][%d]: 0x%lx",
                          j, (long) &share->ndb_value[0][j],
                          j, (long) attr0.ptr));
      DBUG_PRINT("info", ("&share->ndb_value[1][%d]: 0x%lx  "
                          "share->ndb_value[1][%d]: 0x%lx",
                          j, (long) &share->ndb_value[0][j],
                          j, (long) attr1.ptr));
    }
    op->setCustomData((void *) share); // set before execute
    share->op= op; // assign op in NDB_SHARE
    if (op->execute())
    {
      share->op= NULL;
      retries--;
      if (op->getNdbError().status != NdbError::TemporaryError &&
          op->getNdbError().code != 1407)
        retries= 0;
      if (retries == 0)
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_GET_ERRMSG, ER(ER_GET_ERRMSG), 
                            op->getNdbError().code, op->getNdbError().message,
                            "NDB");
        sql_print_error("NDB Binlog: ndbevent->execute failed for %s; %d %s",
                        event_name,
                        op->getNdbError().code, op->getNdbError().message);
      }
      ndb->dropEventOperation(op);
      mysql_mutex_unlock(&injector_mutex);
      if (retries)
      {
        my_sleep(retry_sleep);
        continue;
      }
      DBUG_RETURN(-1);
    }
    mysql_mutex_unlock(&injector_mutex);
    break;
  }

  /* ndb_share reference binlog */
  get_share(share);
  DBUG_PRINT("NDB_SHARE", ("%s binlog  use_count: %u",
                           share->key, share->use_count));
  if (do_ndb_apply_status_share)
  {
    /* ndb_share reference binlog extra */
    ndb_apply_status_share= get_share(share);
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra  use_count: %u",
                             share->key, share->use_count));
    mysql_cond_signal(&injector_cond);
  }
  else if (do_ndb_schema_share)
  {
    /* ndb_share reference binlog extra */
    ndb_schema_share= get_share(share);
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra  use_count: %u",
                             share->key, share->use_count));
    mysql_cond_signal(&injector_cond);
  }

  DBUG_PRINT("info",("%s share->op: 0x%lx  share->use_count: %u",
                     share->key, (long) share->op, share->use_count));

  if (opt_ndb_extra_logging)
    sql_print_information("NDB Binlog: logging %s", share->key);
  DBUG_RETURN(0);
}

/*
  when entering the calling thread should have a share lock id share != 0
  then the injector thread will have  one as well, i.e. share->use_count == 0
  (unless it has already dropped... then share->op == 0)
*/
int
ndbcluster_handle_drop_table(Ndb *ndb, const char *event_name,
                             NDB_SHARE *share, const char *type_str)
{
  DBUG_ENTER("ndbcluster_handle_drop_table");
  THD *thd= current_thd;

  NDBDICT *dict= ndb->getDictionary();
  if (event_name && dict->dropEvent(event_name))
  {
    if (dict->getNdbError().code != 4710)
    {
      /* drop event failed for some reason, issue a warning */
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                          dict->getNdbError().code,
                          dict->getNdbError().message, "NDB");
      /* error is not that the event did not exist */
      sql_print_error("NDB Binlog: Unable to drop event in database. "
                      "Event: %s Error Code: %d Message: %s",
                      event_name,
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
  these out of order, thus we are keeping the SYNC_DROP_ defined
  for now.
*/
  const char *save_proc_info= thd->proc_info;
#define SYNC_DROP_
#ifdef SYNC_DROP_
  thd->proc_info= "Syncing ndb table schema operation and binlog";
  mysql_mutex_lock(&share->mutex);
  int max_timeout= DEFAULT_SYNC_TIMEOUT;
  while (share->op)
  {
    struct timespec abstime;
    set_timespec(abstime, 1);
    int ret= mysql_cond_timedwait(&injector_cond,
                                  &share->mutex,
                                  &abstime);
    if (thd->killed ||
        share->op == 0)
      break;
    if (ret)
    {
      max_timeout--;
      if (max_timeout == 0)
      {
        sql_print_error("NDB %s: %s timed out. Ignoring...",
                        type_str, share->key);
        break;
      }
      if (opt_ndb_extra_logging)
        ndb_report_waiting(type_str, max_timeout,
                           type_str, share->key);
    }
  }
  mysql_mutex_unlock(&share->mutex);
#else
  mysql_mutex_lock(&share->mutex);
  share->op_old= share->op;
  share->op= 0;
  mysql_mutex_unlock(&share->mutex);
#endif
  thd->proc_info= save_proc_info;

  DBUG_RETURN(0);
}


/********************************************************************
  Internal helper functions for differentd events from the stoarage nodes
  used by the ndb injector thread
********************************************************************/

/*
  Handle error states on events from the storage nodes
*/
static int ndb_binlog_thread_handle_error(Ndb *ndb, NdbEventOperation *pOp,
                                          ndb_binlog_index_row &row)
{
  NDB_SHARE *share= (NDB_SHARE *)pOp->getCustomData();
  DBUG_ENTER("ndb_binlog_thread_handle_error");

  int overrun= pOp->isOverrun();
  if (overrun)
  {
    /*
      ToDo: this error should rather clear the ndb_binlog_index...
      and continue
    */
    sql_print_error("NDB Binlog: Overrun in event buffer, "
                    "this means we have dropped events. Cannot "
                    "continue binlog for %s", share->key);
    pOp->clearError();
    DBUG_RETURN(-1);
  }

  if (!pOp->isConsistent())
  {
    /*
      ToDo: this error should rather clear the ndb_binlog_index...
      and continue
    */
    sql_print_error("NDB Binlog: Not Consistent. Cannot "
                    "continue binlog for %s. Error code: %d"
                    " Message: %s", share->key,
                    pOp->getNdbError().code,
                    pOp->getNdbError().message);
    pOp->clearError();
    DBUG_RETURN(-1);
  }
  sql_print_error("NDB Binlog: unhandled error %d for table %s",
                  pOp->hasError(), share->key);
  pOp->clearError();
  DBUG_RETURN(0);
}

static int
ndb_binlog_thread_handle_non_data_event(THD *thd, Ndb *ndb,
                                        NdbEventOperation *pOp,
                                        ndb_binlog_index_row &row)
{
  NDB_SHARE *share= (NDB_SHARE *)pOp->getCustomData();
  NDBEVENT::TableEvent type= pOp->getEventType();

  switch (type)
  {
  case NDBEVENT::TE_CLUSTER_FAILURE:
    if (opt_ndb_extra_logging)
      sql_print_information("NDB Binlog: cluster failure for %s at epoch %u.",
                            share->key, (unsigned) pOp->getGCI());
    if (ndb_apply_status_share == share)
    {
      if (opt_ndb_extra_logging &&
          ndb_binlog_tables_inited && ndb_binlog_running)
        sql_print_information("NDB Binlog: ndb tables initially "
                              "read only on reconnect.");
      /* ndb_share reference binlog extra free */
      DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                               share->key, share->use_count));
      free_share(&ndb_apply_status_share);
      ndb_apply_status_share= 0;
      ndb_binlog_tables_inited= 0;
    }
    DBUG_PRINT("error", ("CLUSTER FAILURE EVENT: "
                        "%s  received share: 0x%lx  op: 0x%lx  share op: 0x%lx  "
                        "op_old: 0x%lx",
                         share->key, (long) share, (long) pOp,
                         (long) share->op, (long) share->op_old));
    break;
  case NDBEVENT::TE_DROP:
    if (ndb_apply_status_share == share)
    {
      if (opt_ndb_extra_logging &&
          ndb_binlog_tables_inited && ndb_binlog_running)
        sql_print_information("NDB Binlog: ndb tables initially "
                              "read only on reconnect.");
      /* ndb_share reference binlog extra free */
      DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                               share->key, share->use_count));
      free_share(&ndb_apply_status_share);
      ndb_apply_status_share= 0;
      ndb_binlog_tables_inited= 0;
    }
    /* ToDo: remove printout */
    if (opt_ndb_extra_logging)
      sql_print_information("NDB Binlog: drop table %s.", share->key);
    // fall through
  case NDBEVENT::TE_ALTER:
    row.n_schemaops++;
    DBUG_PRINT("info", ("TABLE %s  EVENT: %s  received share: 0x%lx  op: 0x%lx  "
                        "share op: 0x%lx  op_old: 0x%lx",
                        type == NDBEVENT::TE_DROP ? "DROP" : "ALTER",
                        share->key, (long) share, (long) pOp,
                        (long) share->op, (long) share->op_old));
    break;
  case NDBEVENT::TE_NODE_FAILURE:
    /* fall through */
  case NDBEVENT::TE_SUBSCRIBE:
    /* fall through */
  case NDBEVENT::TE_UNSUBSCRIBE:
    /* ignore */
    return 0;
  default:
    sql_print_error("NDB Binlog: unknown non data event %d for %s. "
                    "Ignoring...", (unsigned) type, share->key);
    return 0;
  }

  ndb_handle_schema_change(thd, ndb, pOp, share);
  return 0;
}

/*
  Handle data events from the storage nodes
*/
static int
ndb_binlog_thread_handle_data_event(Ndb *ndb, NdbEventOperation *pOp,
                                    ndb_binlog_index_row &row,
                                    injector::transaction &trans)
{
  NDB_SHARE *share= (NDB_SHARE*) pOp->getCustomData();
  if (share == ndb_apply_status_share)
    return 0;

  uint32 originating_server_id= pOp->getAnyValue();
  if (originating_server_id == 0)
    originating_server_id= ::server_id;
  else if (originating_server_id & NDB_ANYVALUE_RESERVED)
  {
    if (originating_server_id != NDB_ANYVALUE_FOR_NOLOGGING)
      sql_print_warning("NDB: unknown value for binlog signalling 0x%X, "
                        "event not logged",
                        originating_server_id);
    return 0;
  }
  else if (!g_ndb_log_slave_updates)
  {
    /*
      This event comes from a slave applier since it has an originating
      server id set. Since option to log slave updates is not set, skip it.
    */
    return 0;
  }

  TABLE *table= share->table;
  DBUG_ASSERT(trans.good());
  DBUG_ASSERT(table != 0);

  dbug_print_table("table", table);

  TABLE_SHARE *table_s= table->s;
  uint n_fields= table_s->fields;
  MY_BITMAP b;
  /* Potential buffer for the bitmap */
  uint32 bitbuf[128 / (sizeof(uint32) * 8)];
  bitmap_init(&b, n_fields <= sizeof(bitbuf) * 8 ? bitbuf : NULL, 
              n_fields, FALSE);
  bitmap_set_all(&b);

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

  switch(pOp->getEventType())
  {
  case NDBEVENT::TE_INSERT:
    row.n_inserts++;
    DBUG_PRINT("info", ("INSERT INTO %s.%s",
                        table_s->db.str, table_s->table_name.str));
    {
      if (share->flags & NSF_BLOB_FLAG)
      {
        my_ptrdiff_t ptrdiff= 0;
        int ret __attribute__((unused))= get_ndb_blobs_value(table, share->ndb_value[0],
                                               blobs_buffer[0],
                                               blobs_buffer_size[0],
                                               ptrdiff);
        DBUG_ASSERT(ret == 0);
      }
      ndb_unpack_record(table, share->ndb_value[0], &b, table->record[0]);
      int ret __attribute__((unused))= trans.write_row(originating_server_id,
                                        injector::transaction::table(table,
                                                                     TRUE),
                                        &b, n_fields, table->record[0]);
      DBUG_ASSERT(ret == 0);
    }
    break;
  case NDBEVENT::TE_DELETE:
    row.n_deletes++;
    DBUG_PRINT("info",("DELETE FROM %s.%s",
                       table_s->db.str, table_s->table_name.str));
    {
      /*
        table->record[0] contains only the primary key in this case
        since we do not have an after image
      */
      int n;
      if (table->s->primary_key != MAX_KEY)
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

      if (share->flags & NSF_BLOB_FLAG)
      {
        my_ptrdiff_t ptrdiff= table->record[n] - table->record[0];
        int ret __attribute__((unused))= get_ndb_blobs_value(table, share->ndb_value[n],
                                               blobs_buffer[n],
                                               blobs_buffer_size[n],
                                               ptrdiff);
        DBUG_ASSERT(ret == 0);
      }
      ndb_unpack_record(table, share->ndb_value[n], &b, table->record[n]);
      DBUG_EXECUTE("info", print_records(table, table->record[n]););
      int ret __attribute__((unused))= trans.delete_row(originating_server_id,
                                          injector::transaction::table(table,
                                                                       TRUE),
                                          &b, n_fields, table->record[n]);
      DBUG_ASSERT(ret == 0);
    }
    break;
  case NDBEVENT::TE_UPDATE:
    row.n_updates++;
    DBUG_PRINT("info", ("UPDATE %s.%s",
                        table_s->db.str, table_s->table_name.str));
    {
      if (share->flags & NSF_BLOB_FLAG)
      {
        my_ptrdiff_t ptrdiff= 0;
        int ret __attribute__((unused))= get_ndb_blobs_value(table, share->ndb_value[0],
                                               blobs_buffer[0],
                                               blobs_buffer_size[0],
                                               ptrdiff);
        DBUG_ASSERT(ret == 0);
      }
      ndb_unpack_record(table, share->ndb_value[0],
                        &b, table->record[0]);
      DBUG_EXECUTE("info", print_records(table, table->record[0]););
      if (table->s->primary_key != MAX_KEY) 
      {
        /*
          since table has a primary key, we can do a write
          using only after values
        */
        trans.write_row(originating_server_id,
                        injector::transaction::table(table, TRUE),
                        &b, n_fields, table->record[0]);// after values
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
          int ret __attribute__((unused))= get_ndb_blobs_value(table, share->ndb_value[1],
                                                 blobs_buffer[1],
                                                 blobs_buffer_size[1],
                                                 ptrdiff);
          DBUG_ASSERT(ret == 0);
        }
        ndb_unpack_record(table, share->ndb_value[1], &b, table->record[1]);
        DBUG_EXECUTE("info", print_records(table, table->record[1]););
        int ret __attribute__((unused))= trans.update_row(originating_server_id,
                                            injector::transaction::table(table,
                                                                         TRUE),
                                            &b, n_fields,
                                            table->record[1], // before values
                                            table->record[0]);// after values
        DBUG_ASSERT(ret == 0);
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

//#define RUN_NDB_BINLOG_TIMER
#ifdef RUN_NDB_BINLOG_TIMER
class Timer
{
public:
  Timer() { start(); }
  void start() { gettimeofday(&m_start, 0); }
  void stop() { gettimeofday(&m_stop, 0); }
  ulong elapsed_ms()
  {
    return (ulong)
      (((longlong) m_stop.tv_sec - (longlong) m_start.tv_sec) * 1000 +
       ((longlong) m_stop.tv_usec -
        (longlong) m_start.tv_usec + 999) / 1000);
  }
private:
  struct timeval m_start,m_stop;
};
#endif

/****************************************************************
  Injector thread main loop
****************************************************************/

static uchar *
ndb_schema_objects_get_key(NDB_SCHEMA_OBJECT *schema_object,
                           size_t *length,
                           my_bool not_used __attribute__((unused)))
{
  *length= schema_object->key_length;
  return (uchar*) schema_object->key;
}

static NDB_SCHEMA_OBJECT *ndb_get_schema_object(const char *key,
                                                my_bool create_if_not_exists,
                                                my_bool have_lock)
{
  NDB_SCHEMA_OBJECT *ndb_schema_object;
  uint length= (uint) strlen(key);
  DBUG_ENTER("ndb_get_schema_object");
  DBUG_PRINT("enter", ("key: '%s'", key));

  if (!have_lock)
    mysql_mutex_lock(&ndbcluster_mutex);
  while (!(ndb_schema_object=
           (NDB_SCHEMA_OBJECT*) my_hash_search(&ndb_schema_objects,
                                               (uchar*) key,
                                               length)))
  {
    if (!create_if_not_exists)
    {
      DBUG_PRINT("info", ("does not exist"));
      break;
    }
    if (!(ndb_schema_object=
          (NDB_SCHEMA_OBJECT*) my_malloc(sizeof(*ndb_schema_object) + length + 1,
                                         MYF(MY_WME | MY_ZEROFILL))))
    {
      DBUG_PRINT("info", ("malloc error"));
      break;
    }
    ndb_schema_object->key= (char *)(ndb_schema_object+1);
    memcpy(ndb_schema_object->key, key, length + 1);
    ndb_schema_object->key_length= length;
    if (my_hash_insert(&ndb_schema_objects, (uchar*) ndb_schema_object))
    {
      my_free(ndb_schema_object);
      break;
    }
    mysql_mutex_init(key_ndb_schema_object_mutex, &ndb_schema_object->mutex, MY_MUTEX_INIT_FAST);
    bitmap_init(&ndb_schema_object->slock_bitmap, ndb_schema_object->slock,
                sizeof(ndb_schema_object->slock)*8, FALSE);
    bitmap_clear_all(&ndb_schema_object->slock_bitmap);
    break;
  }
  if (ndb_schema_object)
  {
    ndb_schema_object->use_count++;
    DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->use_count));
  }
  if (!have_lock)
    mysql_mutex_unlock(&ndbcluster_mutex);
  DBUG_RETURN(ndb_schema_object);
}


static void ndb_free_schema_object(NDB_SCHEMA_OBJECT **ndb_schema_object,
                                   bool have_lock)
{
  DBUG_ENTER("ndb_free_schema_object");
  DBUG_PRINT("enter", ("key: '%s'", (*ndb_schema_object)->key));
  if (!have_lock)
    mysql_mutex_lock(&ndbcluster_mutex);
  if (!--(*ndb_schema_object)->use_count)
  {
    DBUG_PRINT("info", ("use_count: %d", (*ndb_schema_object)->use_count));
    my_hash_delete(&ndb_schema_objects, (uchar*) *ndb_schema_object);
    mysql_mutex_destroy(&(*ndb_schema_object)->mutex);
    my_free(*ndb_schema_object);
    *ndb_schema_object= 0;
  }
  else
  {
    DBUG_PRINT("info", ("use_count: %d", (*ndb_schema_object)->use_count));
  }
  if (!have_lock)
    mysql_mutex_unlock(&ndbcluster_mutex);
  DBUG_VOID_RETURN;
}

extern ulong opt_ndb_report_thresh_binlog_epoch_slip;
extern ulong opt_ndb_report_thresh_binlog_mem_usage;

pthread_handler_t ndb_binlog_thread_func(void *arg)
{
  THD *thd; /* needs to be first for thread_stack */
  Ndb *i_ndb= 0;
  Ndb *s_ndb= 0;
  Thd_ndb *thd_ndb=0;
  int ndb_update_ndb_binlog_index= 1;
  injector *inj= injector::instance();
  uint incident_id= 0;

#ifdef RUN_NDB_BINLOG_TIMER
  Timer main_timer;
#endif

  mysql_mutex_lock(&injector_mutex);
  /*
    Set up the Thread
  */
  my_thread_init();
  DBUG_ENTER("ndb_binlog_thread");

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  THD_CHECK_SENTRY(thd);
  thd->set_current_stmt_binlog_format_row();

  /* We need to set thd->thread_id before thd->store_globals, or it will
     set an invalid value for thd->variables.pseudo_thread_id.
  */
  mysql_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thread_id++;
  mysql_mutex_unlock(&LOCK_thread_count);

  mysql_thread_set_psi_id(thd->thread_id);

  thd->thread_stack= (char*) &thd; /* remember where our stack is */
  if (thd->store_globals())
  {
    thd->cleanup();
    delete thd;
    ndb_binlog_thread_running= -1;
    mysql_mutex_unlock(&injector_mutex);
    mysql_cond_signal(&injector_cond);

    DBUG_LEAVE;                               // Must match DBUG_ENTER()
    my_thread_end();
    pthread_exit(0);
    return NULL;                              // Avoid compiler warnings
  }

  thd->init_for_queries();
  thd->command= COM_DAEMON;
  thd->system_thread= SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->main_security_ctx.host_or_ip= "";
  thd->client_capabilities= 0;
  my_net_init(&thd->net, 0);
  thd->main_security_ctx.master_access= ~0;
  thd->main_security_ctx.priv_user[0]= 0;
  /* Do not use user-supplied timeout value for system threads. */
  thd->variables.lock_wait_timeout= LONG_TIMEOUT;

  /*
    Set up ndb binlog
  */
  sql_print_information("Starting MySQL Cluster Binlog Thread");

  pthread_detach_this_thread();
  thd->real_id= pthread_self();
  mysql_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  thd->lex->start_transaction_opt= 0;

  if (!(s_ndb= new Ndb(g_ndb_cluster_connection, "")) ||
      s_ndb->init())
  {
    sql_print_error("NDB Binlog: Getting Schema Ndb object failed");
    ndb_binlog_thread_running= -1;
    mysql_mutex_unlock(&injector_mutex);
    mysql_cond_signal(&injector_cond);
    goto err;
  }

  // empty database
  if (!(i_ndb= new Ndb(g_ndb_cluster_connection, "")) ||
      i_ndb->init())
  {
    sql_print_error("NDB Binlog: Getting Ndb object failed");
    ndb_binlog_thread_running= -1;
    mysql_mutex_unlock(&injector_mutex);
    mysql_cond_signal(&injector_cond);
    goto err;
  }

  /* init hash for schema object distribution */
  (void) my_hash_init(&ndb_schema_objects, system_charset_info, 32, 0, 0,
                   (my_hash_get_key)ndb_schema_objects_get_key, 0, 0);

  /*
    Expose global reference to our ndb object.

    Used by both sql client thread and binlog thread to interact
    with the storage
    mysql_mutex_lock(&injector_mutex);
  */
  injector_thd= thd;
  injector_ndb= i_ndb;
  p_latest_trans_gci= 
    injector_ndb->get_ndb_cluster_connection().get_latest_trans_gci();
  schema_ndb= s_ndb;

  if (opt_bin_log)
  {
    ndb_binlog_running= TRUE;
  }

  /* Thread start up completed  */
  ndb_binlog_thread_running= 1;
  mysql_mutex_unlock(&injector_mutex);
  mysql_cond_signal(&injector_cond);

  /*
    wait for mysql server to start (so that the binlog is started
    and thus can receive the first GAP event)
  */
  mysql_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
  {
    struct timespec abstime;
    set_timespec(abstime, 1);
    mysql_cond_timedwait(&COND_server_started, &LOCK_server_started,
                         &abstime);
    if (ndbcluster_terminating)
    {
      mysql_mutex_unlock(&LOCK_server_started);
      goto err;
    }
  }
  mysql_mutex_unlock(&LOCK_server_started);
restart:
  /*
    Main NDB Injector loop
  */
  while (ndb_binlog_running)
  {
    /*
      check if it is the first log, if so we do not insert a GAP event
      as there is really no log to have a GAP in
    */
    if (incident_id == 0)
    {
      LOG_INFO log_info;
      mysql_bin_log.get_current_log(&log_info);
      int len=  strlen(log_info.log_file_name);
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
    int error __attribute__((unused))=
      inj->record_incident(thd, INCIDENT_LOST_EVENTS, msg[incident_id]);
    DBUG_ASSERT(!error);
    break;
  }
  incident_id= 1;
  {
    thd->proc_info= "Waiting for ndbcluster to start";

    mysql_mutex_lock(&injector_mutex);
    while (!ndb_schema_share ||
           (ndb_binlog_running && !ndb_apply_status_share))
    {
      /* ndb not connected yet */
      struct timespec abstime;
      set_timespec(abstime, 1);
      mysql_cond_timedwait(&injector_cond, &injector_mutex, &abstime);
      if (ndbcluster_binlog_terminating)
      {
        mysql_mutex_unlock(&injector_mutex);
        goto err;
      }
    }
    mysql_mutex_unlock(&injector_mutex);

    if (thd_ndb == NULL)
    {
      DBUG_ASSERT(ndbcluster_hton->slot != ~(uint)0);
      if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
      {
        sql_print_error("Could not allocate Thd_ndb object");
        goto err;
      }
      set_thd_ndb(thd, thd_ndb);
      thd_ndb->options|= TNO_NO_LOG_SCHEMA_OP;
      thd->query_id= 0; // to keep valgrind quiet
    }
  }

  {
    // wait for the first event
    thd->proc_info= "Waiting for first event from ndbcluster";
    int schema_res, res;
    Uint64 schema_gci;
    do
    {
      DBUG_PRINT("info", ("Waiting for the first event"));

      if (ndbcluster_binlog_terminating)
        goto err;

      schema_res= s_ndb->pollEvents(100, &schema_gci);
    } while (schema_gci == 0 || ndb_latest_received_binlog_epoch == schema_gci);
    if (ndb_binlog_running)
    {
      Uint64 gci= i_ndb->getLatestGCI();
      while (gci < schema_gci || gci == ndb_latest_received_binlog_epoch)
      {
        if (ndbcluster_binlog_terminating)
          goto err;
        res= i_ndb->pollEvents(10, &gci);
      }
      if (gci > schema_gci)
      {
        schema_gci= gci;
      }
    }
    // now check that we have epochs consistant with what we had before the restart
    DBUG_PRINT("info", ("schema_res: %d  schema_gci: %lu", schema_res,
                        (long) schema_gci));
    {
      i_ndb->flushIncompleteEvents(schema_gci);
      s_ndb->flushIncompleteEvents(schema_gci);
      if (schema_gci < ndb_latest_handled_binlog_epoch)
      {
        sql_print_error("NDB Binlog: cluster has been restarted --initial or with older filesystem. "
                        "ndb_latest_handled_binlog_epoch: %u, while current epoch: %u. "
                        "RESET MASTER should be issued. Resetting ndb_latest_handled_binlog_epoch.",
                        (unsigned) ndb_latest_handled_binlog_epoch, (unsigned) schema_gci);
        *p_latest_trans_gci= 0;
        ndb_latest_handled_binlog_epoch= 0;
        ndb_latest_applied_binlog_epoch= 0;
        ndb_latest_received_binlog_epoch= 0;
      }
      else if (ndb_latest_applied_binlog_epoch > 0)
      {
        sql_print_warning("NDB Binlog: cluster has reconnected. "
                          "Changes to the database that occured while "
                          "disconnected will not be in the binlog");
      }
      if (opt_ndb_extra_logging)
      {
        sql_print_information("NDB Binlog: starting log at epoch %u",
                              (unsigned)schema_gci);
      }
    }
  }
  {
    static char db[]= "";
    thd->db= db;
  }
  do_ndbcluster_binlog_close_connection= BCCC_running;
  for ( ; !((ndbcluster_binlog_terminating ||
             do_ndbcluster_binlog_close_connection) &&
            ndb_latest_handled_binlog_epoch >= *p_latest_trans_gci) &&
          do_ndbcluster_binlog_close_connection != BCCC_restart; )
  {
#ifndef DBUG_OFF
    if (do_ndbcluster_binlog_close_connection)
    {
      DBUG_PRINT("info", ("do_ndbcluster_binlog_close_connection: %d, "
                          "ndb_latest_handled_binlog_epoch: %lu, "
                          "*p_latest_trans_gci: %lu",
                          do_ndbcluster_binlog_close_connection,
                          (ulong) ndb_latest_handled_binlog_epoch,
                          (ulong) *p_latest_trans_gci));
    }
#endif
#ifdef RUN_NDB_BINLOG_TIMER
    main_timer.stop();
    sql_print_information("main_timer %ld ms",  main_timer.elapsed_ms());
    main_timer.start();
#endif

    /*
      now we don't want any events before next gci is complete
    */
    thd->proc_info= "Waiting for event from ndbcluster";
    thd->set_time();
    
    /* wait for event or 1000 ms */
    Uint64 gci= 0, schema_gci;
    int res= 0, tot_poll_wait= 1000;
    if (ndb_binlog_running)
    {
      res= i_ndb->pollEvents(tot_poll_wait, &gci);
      tot_poll_wait= 0;
    }
    else
    {
      /*
        Just consume any events, not used if no binlogging
        e.g. node failure events
      */
      Uint64 tmp_gci;
      if (i_ndb->pollEvents(0, &tmp_gci))
        while (i_ndb->nextEvent())
          ;
    }
    int schema_res= s_ndb->pollEvents(tot_poll_wait, &schema_gci);
    ndb_latest_received_binlog_epoch= gci;

    while (gci > schema_gci && schema_res >= 0)
    {
      static char buf[64];
      thd->proc_info= "Waiting for schema epoch";
      my_snprintf(buf, sizeof(buf), "%s %u(%u)", thd->proc_info, (unsigned) schema_gci, (unsigned) gci);
      thd->proc_info= buf;
      schema_res= s_ndb->pollEvents(10, &schema_gci);
    }

    if ((ndbcluster_binlog_terminating ||
         do_ndbcluster_binlog_close_connection) &&
        (ndb_latest_handled_binlog_epoch >= *p_latest_trans_gci ||
         !ndb_binlog_running))
      break; /* Shutting down server */

    if (ndb_binlog_index && ndb_binlog_index->s->has_old_version())
    {
      if (ndb_binlog_index->s->has_old_version())
      {
        trans_commit_stmt(thd);
        close_thread_tables(thd);
        thd->mdl_context.release_transactional_locks();
        ndb_binlog_index= 0;
      }
    }

    MEM_ROOT **root_ptr=
      my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
    MEM_ROOT *old_root= *root_ptr;
    MEM_ROOT mem_root;
    init_sql_alloc(&mem_root, 4096, 0);
    List<Cluster_schema> post_epoch_log_list;
    List<Cluster_schema> post_epoch_unlock_list;
    *root_ptr= &mem_root;

    if (unlikely(schema_res > 0))
    {
      thd->proc_info= "Processing events from schema table";
      s_ndb->
        setReportThreshEventGCISlip(opt_ndb_report_thresh_binlog_epoch_slip);
      s_ndb->
        setReportThreshEventFreeMem(opt_ndb_report_thresh_binlog_mem_usage);
      NdbEventOperation *pOp= s_ndb->nextEvent();
      while (pOp != NULL)
      {
        if (!pOp->hasError())
        {
          ndb_binlog_thread_handle_schema_event(thd, s_ndb, pOp,
                                                &post_epoch_log_list,
                                                &post_epoch_unlock_list,
                                                &mem_root);
          DBUG_PRINT("info", ("s_ndb first: %s", s_ndb->getEventOperation() ?
                              s_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                              "<empty>"));
          DBUG_PRINT("info", ("i_ndb first: %s", i_ndb->getEventOperation() ?
                              i_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                              "<empty>"));
          if (i_ndb->getEventOperation() == NULL &&
              s_ndb->getEventOperation() == NULL &&
              do_ndbcluster_binlog_close_connection == BCCC_running)
          {
            DBUG_PRINT("info", ("do_ndbcluster_binlog_close_connection= BCCC_restart"));
            do_ndbcluster_binlog_close_connection= BCCC_restart;
            if (ndb_latest_received_binlog_epoch < *p_latest_trans_gci && ndb_binlog_running)
            {
              sql_print_error("NDB Binlog: latest transaction in epoch %lu not in binlog "
                              "as latest received epoch is %lu",
                              (ulong) *p_latest_trans_gci,
                              (ulong) ndb_latest_received_binlog_epoch);
            }
          }
        }
        else
          sql_print_error("NDB: error %lu (%s) on handling "
                          "binlog schema event",
                          (ulong) pOp->getNdbError().code,
                          pOp->getNdbError().message);
        pOp= s_ndb->nextEvent();
      }
    }

    if (res > 0)
    {
      DBUG_PRINT("info", ("pollEvents res: %d", res));
      thd->proc_info= "Processing events";
      NdbEventOperation *pOp= i_ndb->nextEvent();
      ndb_binlog_index_row row;
      while (pOp != NULL)
      {
#ifdef RUN_NDB_BINLOG_TIMER
        Timer gci_timer, write_timer;
        int event_count= 0;
        gci_timer.start();
#endif
        gci= pOp->getGCI();
        DBUG_PRINT("info", ("Handling gci: %d", (unsigned)gci));
        // sometimes get TE_ALTER with invalid table
        DBUG_ASSERT(pOp->getEventType() == NdbDictionary::Event::TE_ALTER ||
                    ! IS_NDB_BLOB_PREFIX(pOp->getEvent()->getTable()->getName()));
        DBUG_ASSERT(gci <= ndb_latest_received_binlog_epoch);

        /* initialize some variables for this epoch */
        g_ndb_log_slave_updates= opt_log_slave_updates;
        i_ndb->
          setReportThreshEventGCISlip(opt_ndb_report_thresh_binlog_epoch_slip);
        i_ndb->setReportThreshEventFreeMem(opt_ndb_report_thresh_binlog_mem_usage);

        bzero((char*) &row, sizeof(row));
        thd->variables.character_set_client= &my_charset_latin1;
        injector::transaction trans;
        // pass table map before epoch
        {
          Uint32 iter= 0;
          const NdbEventOperation *gci_op;
          Uint32 event_types;
          while ((gci_op= i_ndb->getGCIEventOperations(&iter, &event_types))
                 != NULL)
          {
            NDB_SHARE *share= (NDB_SHARE*)gci_op->getCustomData();
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
            if (share == NULL || share->table == NULL)
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
            TABLE *table= share->table;
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
            DBUG_PRINT("info", ("use_table: %.*s",
                                (int) name.length, name.str));
            injector::transaction::table tbl(table, TRUE);
            int ret __attribute__((unused))= trans.use_table(::server_id, tbl);
            DBUG_ASSERT(ret == 0);
          }
        }
        if (trans.good())
        {
          if (ndb_apply_status_share)
          {
            TABLE *table= ndb_apply_status_share->table;

#ifndef DBUG_OFF
            const LEX_STRING& name= table->s->table_name;
            DBUG_PRINT("info", ("use_table: %.*s",
                                (int) name.length, name.str));
#endif
            injector::transaction::table tbl(table, TRUE);
            int ret __attribute__((unused))= trans.use_table(::server_id, tbl);
            DBUG_ASSERT(ret == 0);

	    /* 
	       Intialize table->record[0] 
	    */
	    empty_record(table);

            table->field[0]->store((longlong)::server_id);
            table->field[1]->store((longlong)gci);
            table->field[2]->store("", 0, &my_charset_bin);
            table->field[3]->store((longlong)0);
            table->field[4]->store((longlong)0);
            trans.write_row(::server_id,
                            injector::transaction::table(table, TRUE),
                            &table->s->all_set, table->s->fields,
                            table->record[0]);
          }
          else
          {
            sql_print_error("NDB: Could not get apply status share");
          }
        }
#ifdef RUN_NDB_BINLOG_TIMER
        write_timer.start();
#endif
        do
        {
#ifdef RUN_NDB_BINLOG_TIMER
          event_count++;
#endif
          if (pOp->hasError() &&
              ndb_binlog_thread_handle_error(i_ndb, pOp, row) < 0)
            goto err;

#ifndef DBUG_OFF
          {
            NDB_SHARE *share= (NDB_SHARE*) pOp->getCustomData();
            DBUG_PRINT("info",
                       ("EVENT TYPE: %d  GCI: %ld  last applied: %ld  "
                        "share: 0x%lx (%s.%s)", pOp->getEventType(),
                        (long) gci,
                        (long) ndb_latest_applied_binlog_epoch,
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
              if (gci_op == pOp)
                break;
            }
            DBUG_ASSERT(gci_op == pOp);
            DBUG_ASSERT((event_types & pOp->getEventType()) != 0);
          }
#endif
          if ((unsigned) pOp->getEventType() <
              (unsigned) NDBEVENT::TE_FIRST_NON_DATA_EVENT)
            ndb_binlog_thread_handle_data_event(i_ndb, pOp, row, trans);
          else
          {
            // set injector_ndb database/schema from table internal name
            int ret __attribute__((unused))=
              i_ndb->setDatabaseAndSchemaName(pOp->getEvent()->getTable());
            DBUG_ASSERT(ret == 0);
            ndb_binlog_thread_handle_non_data_event(thd, i_ndb, pOp, row);
            // reset to catch errors
            i_ndb->setDatabaseName("");
            DBUG_PRINT("info", ("s_ndb first: %s", s_ndb->getEventOperation() ?
                                s_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                                "<empty>"));
            DBUG_PRINT("info", ("i_ndb first: %s", i_ndb->getEventOperation() ?
                                i_ndb->getEventOperation()->getEvent()->getTable()->getName() :
                                "<empty>"));
            if (i_ndb->getEventOperation() == NULL &&
                s_ndb->getEventOperation() == NULL &&
                do_ndbcluster_binlog_close_connection == BCCC_running)
            {
              DBUG_PRINT("info", ("do_ndbcluster_binlog_close_connection= BCCC_restart"));
              do_ndbcluster_binlog_close_connection= BCCC_restart;
              if (ndb_latest_received_binlog_epoch < *p_latest_trans_gci && ndb_binlog_running)
              {
                sql_print_error("NDB Binlog: latest transaction in epoch %lu not in binlog "
                                "as latest received epoch is %lu",
                                (ulong) *p_latest_trans_gci,
                                (ulong) ndb_latest_received_binlog_epoch);
              }
            }
          }

          pOp= i_ndb->nextEvent();
        } while (pOp && pOp->getGCI() == gci);

        /*
          note! pOp is not referring to an event in the next epoch
          or is == 0
        */
#ifdef RUN_NDB_BINLOG_TIMER
        write_timer.stop();
#endif

        if (trans.good())
        {
          //DBUG_ASSERT(row.n_inserts || row.n_updates || row.n_deletes);
          thd->proc_info= "Committing events to binlog";
          injector::transaction::binlog_pos start= trans.start_pos();
          if (int r= trans.commit())
          {
            sql_print_error("NDB Binlog: "
                            "Error during COMMIT of GCI. Error: %d",
                            r);
            /* TODO: Further handling? */
          }
          row.gci= gci;
          row.master_log_file= start.file_name();
          row.master_log_pos= start.file_pos();

          DBUG_PRINT("info", ("COMMIT gci: %lu", (ulong) gci));
          if (ndb_update_ndb_binlog_index)
            ndb_add_ndb_binlog_index(thd, &row);
          ndb_latest_applied_binlog_epoch= gci;
        }
        ndb_latest_handled_binlog_epoch= gci;
#ifdef RUN_NDB_BINLOG_TIMER
        gci_timer.stop();
        sql_print_information("gci %ld event_count %d write time "
                              "%ld(%d e/s), total time %ld(%d e/s)",
                              (ulong)gci, event_count,
                              write_timer.elapsed_ms(),
                              (1000*event_count) / write_timer.elapsed_ms(),
                              gci_timer.elapsed_ms(),
                              (1000*event_count) / gci_timer.elapsed_ms());
#endif
      }
    }

    ndb_binlog_thread_handle_schema_event_post_epoch(thd,
                                                     &post_epoch_log_list,
                                                     &post_epoch_unlock_list);
    free_root(&mem_root, MYF(0));
    *root_ptr= old_root;
    ndb_latest_handled_binlog_epoch= ndb_latest_received_binlog_epoch;
  }
  if (do_ndbcluster_binlog_close_connection == BCCC_restart)
  {
    ndb_binlog_tables_inited= FALSE;
    trans_commit_stmt(thd);
    close_thread_tables(thd);
    thd->mdl_context.release_transactional_locks();
    ndb_binlog_index= 0;
    goto restart;
  }
err:
  sql_print_information("Stopping Cluster Binlog");
  DBUG_PRINT("info",("Shutting down cluster binlog thread"));
  thd->proc_info= "Shutting down";
  thd->stmt_da->can_overwrite_status= TRUE;
  thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
  thd->stmt_da->can_overwrite_status= FALSE;
  close_thread_tables(thd);
  thd->mdl_context.release_transactional_locks();
  mysql_mutex_lock(&injector_mutex);
  /* don't mess with the injector_ndb anymore from other threads */
  injector_thd= 0;
  injector_ndb= 0;
  p_latest_trans_gci= 0;
  schema_ndb= 0;
  mysql_mutex_unlock(&injector_mutex);
  thd->db= 0; // as not to try to free memory

  if (ndb_apply_status_share)
  {
    /* ndb_share reference binlog extra free */
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                             ndb_apply_status_share->key,
                             ndb_apply_status_share->use_count));
    free_share(&ndb_apply_status_share);
    ndb_apply_status_share= 0;
  }
  if (ndb_schema_share)
  {
    /* begin protect ndb_schema_share */
    mysql_mutex_lock(&ndb_schema_share_mutex);
    /* ndb_share reference binlog extra free */
    DBUG_PRINT("NDB_SHARE", ("%s binlog extra free  use_count: %u",
                             ndb_schema_share->key,
                             ndb_schema_share->use_count));
    free_share(&ndb_schema_share);
    ndb_schema_share= 0;
    ndb_binlog_tables_inited= 0;
    mysql_mutex_unlock(&ndb_schema_share_mutex);
    /* end protect ndb_schema_share */
  }

  /* remove all event operations */
  if (s_ndb)
  {
    NdbEventOperation *op;
    DBUG_PRINT("info",("removing all event operations"));
    while ((op= s_ndb->getEventOperation()))
    {
      DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(op->getEvent()->getTable()->getName()));
      DBUG_PRINT("info",("removing event operation on %s",
                         op->getEvent()->getName()));
      NDB_SHARE *share= (NDB_SHARE*) op->getCustomData();
      DBUG_ASSERT(share != 0);
      DBUG_ASSERT(share->op == op ||
                  share->op_old == op);
      share->op= share->op_old= 0;
      /* ndb_share reference binlog free */
      DBUG_PRINT("NDB_SHARE", ("%s binlog free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
      s_ndb->dropEventOperation(op);
    }
    delete s_ndb;
    s_ndb= 0;
  }
  if (i_ndb)
  {
    NdbEventOperation *op;
    DBUG_PRINT("info",("removing all event operations"));
    while ((op= i_ndb->getEventOperation()))
    {
      DBUG_ASSERT(! IS_NDB_BLOB_PREFIX(op->getEvent()->getTable()->getName()));
      DBUG_PRINT("info",("removing event operation on %s",
                         op->getEvent()->getName()));
      NDB_SHARE *share= (NDB_SHARE*) op->getCustomData();
      DBUG_ASSERT(share != 0);
      DBUG_ASSERT(share->op == op ||
                  share->op_old == op);
      share->op= share->op_old= 0;
      /* ndb_share reference binlog free */
      DBUG_PRINT("NDB_SHARE", ("%s binlog free  use_count: %u",
                               share->key, share->use_count));
      free_share(&share);
      i_ndb->dropEventOperation(op);
    }
    delete i_ndb;
    i_ndb= 0;
  }

  my_hash_free(&ndb_schema_objects);

  net_end(&thd->net);
  thd->cleanup();
  delete thd;

  ndb_binlog_thread_running= -1;
  ndb_binlog_running= FALSE;
  mysql_cond_signal(&injector_cond);

  DBUG_PRINT("exit", ("ndb_binlog_thread"));

  DBUG_LEAVE;                               // Must match DBUG_ENTER()
  my_thread_end();
  pthread_exit(0);
  return NULL;                              // Avoid compiler warnings
}

bool
ndbcluster_show_status_binlog(THD* thd, stat_print_fn *stat_print,
                              enum ha_stat_type stat_type)
{
  char buf[IO_SIZE];
  uint buflen;
  ulonglong ndb_latest_epoch= 0;
  DBUG_ENTER("ndbcluster_show_status_binlog");
  
  mysql_mutex_lock(&injector_mutex);
  if (injector_ndb)
  {
    char buff1[22],buff2[22],buff3[22],buff4[22],buff5[22];
    ndb_latest_epoch= injector_ndb->getLatestGCI();
    mysql_mutex_unlock(&injector_mutex);

    buflen=
      snprintf(buf, sizeof(buf),
               "latest_epoch=%s, "
               "latest_trans_epoch=%s, "
               "latest_received_binlog_epoch=%s, "
               "latest_handled_binlog_epoch=%s, "
               "latest_applied_binlog_epoch=%s",
               llstr(ndb_latest_epoch, buff1),
               llstr(*p_latest_trans_gci, buff2),
               llstr(ndb_latest_received_binlog_epoch, buff3),
               llstr(ndb_latest_handled_binlog_epoch, buff4),
               llstr(ndb_latest_applied_binlog_epoch, buff5));
    if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                   "binlog", strlen("binlog"),
                   buf, buflen))
      DBUG_RETURN(TRUE);
  }
  else
    mysql_mutex_unlock(&injector_mutex);
  DBUG_RETURN(FALSE);
}

#endif /* HAVE_NDB_BINLOG */
#endif

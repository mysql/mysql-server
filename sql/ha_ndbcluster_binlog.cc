/* Copyright (C) 2000-2003 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "mysql_priv.h"
#include "ha_ndbcluster.h"

#ifdef HAVE_NDB_BINLOG
#include "rpl_injector.h"
#include "slave.h"
#include "ha_ndbcluster_binlog.h"

/*
  defines for cluster replication table names
*/
#include "ha_ndbcluster_tables.h"
#define NDB_APPLY_TABLE_FILE "./" NDB_REP_DB "/" NDB_APPLY_TABLE
#define NDB_SCHEMA_TABLE_FILE "./" NDB_REP_DB "/" NDB_SCHEMA_TABLE

/*
  Flag showing if the ndb injector thread is running, if so == 1
  -1 if it was started but later stopped for some reason
   0 if never started
*/
int ndb_binlog_thread_running= 0;

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
  pthread_mutex_lock(&injector_mutex), when doing create/dropEventOperation
*/
static Ndb *injector_ndb= 0;
static Ndb *schema_ndb= 0;

/*
  Mutex and condition used for interacting between client sql thread
  and injector thread
*/
pthread_t ndb_binlog_thread;
pthread_mutex_t injector_mutex;
pthread_cond_t  injector_cond;

/* NDB Injector thread (used for binlog creation) */
static ulonglong ndb_latest_applied_binlog_epoch= 0;
static ulonglong ndb_latest_handled_binlog_epoch= 0;
static ulonglong ndb_latest_received_binlog_epoch= 0;

NDB_SHARE *apply_status_share= 0;
NDB_SHARE *schema_share= 0;

/* instantiated in storage/ndb/src/ndbapi/Ndbif.cpp */
extern Uint64 g_latest_trans_gci;

/*
  Global variables for holding the binlog_index table reference
*/
static TABLE *binlog_index= 0;
static TABLE_LIST binlog_tables;

/*
  Helper functions
*/

#ifndef DBUG_OFF
static void print_records(TABLE *table, const char *record)
{
  if (_db_on_)
  {
    for (uint j= 0; j < table->s->fields; j++)
    {
      char buf[40];
      int pos= 0;
      Field *field= table->field[j];
      const byte* field_ptr= field->ptr - table->record[0] + record;
      int pack_len= field->pack_length();
      int n= pack_len < 10 ? pack_len : 10;
      
      for (int i= 0; i < n && pos < 20; i++)
      {
	pos+= sprintf(&buf[pos]," %x", (int) (unsigned char) field_ptr[i]);
      }
      buf[pos]= 0;
      DBUG_PRINT("info",("[%u]field_ptr[0->%d]: %s", j, n, buf));
    }
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
              "reclength: %d  rec_buff_length: %d  record[0]: %lx  "
              "record[1]: %lx",
              info,
              table->s->db.str,
              table->s->table_name.str,
              table->s->fields,
              table->s->reclength,
              table->s->rec_buff_length,
              table->record[0],
              table->record[1]));

  for (unsigned int i= 0; i < table->s->fields; i++) 
  {
    Field *f= table->field[i];
    DBUG_PRINT("info",
               ("[%d] \"%s\"(0x%lx:%s%s%s%s%s%s) type: %d  pack_length: %d  "
                "ptr: 0x%lx[+%d]  null_bit: %u  null_ptr: 0x%lx[+%d]",
                i,
                f->field_name,
                f->flags,
                (f->flags & PRI_KEY_FLAG)  ? "pri"       : "attr",
                (f->flags & NOT_NULL_FLAG) ? ""          : ",nullable",
                (f->flags & UNSIGNED_FLAG) ? ",unsigned" : ",signed",
                (f->flags & ZEROFILL_FLAG) ? ",zerofill" : "",
                (f->flags & BLOB_FLAG)     ? ",blob"     : "",
                (f->flags & BINARY_FLAG)   ? ",binary"   : "",
                f->real_type(),
                f->pack_length(),
                f->ptr, f->ptr - table->record[0],
                f->null_bit,
                f->null_ptr, (byte*) f->null_ptr - table->record[0]));
    if (f->type() == MYSQL_TYPE_BIT)
    {
      Field_bit *g= (Field_bit*) f;
      DBUG_PRINT("MYSQL_TYPE_BIT",("field_length: %d  bit_ptr: 0x%lx[+%d] "
                                   "bit_ofs: %u  bit_len: %u",
                                   g->field_length, g->bit_ptr,
                                   (byte*) g->bit_ptr-table->record[0],
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
  - purging the cluster_replication.binlog_index
  - creating the cluster_replication.apply_status table
*/
static void run_query(THD *thd, char *buf, char *end,
                      my_bool print_error, my_bool disable_binlog)
{
  ulong save_query_length= thd->query_length;
  char *save_query= thd->query;
  ulong save_thread_id= thd->variables.pseudo_thread_id;
  ulonglong save_thd_options= thd->options;
  DBUG_ASSERT(sizeof(save_thd_options) == sizeof(thd->options));
  NET save_net= thd->net;

  bzero((char*) &thd->net, sizeof(NET));
  thd->query_length= end - buf;
  thd->query= buf;
  thd->variables.pseudo_thread_id= thread_id;
  if (disable_binlog)
    thd->options&= ~OPTION_BIN_LOG;
    
  DBUG_PRINT("query", ("%s", thd->query));
  mysql_parse(thd, thd->query, thd->query_length);

  if (print_error && thd->query_error)
  {
    sql_print_error("NDB: %s: error %s %d %d %d",
                    buf, thd->net.last_error, thd->net.last_errno,
                    thd->net.report_error, thd->query_error);
  }

  thd->options= save_thd_options;
  thd->query_length= save_query_length;
  thd->query= save_query;
  thd->variables.pseudo_thread_id= save_thread_id;
  thd->net= save_net;

  if (thd == injector_thd)
  {
    /*
      running the query will close all tables, including the binlog_index
      used in injector_thd
    */
    binlog_index= 0;
  }
}

/*
  Initialize the binlog part of the NDB_SHARE
*/
void ndbcluster_binlog_init_share(NDB_SHARE *share, TABLE *_table)
{
  THD *thd= current_thd;
  MEM_ROOT *mem_root= &share->mem_root;

  share->op= 0;
  share->table= 0;
  if (ndb_binlog_thread_running <= 0)
  {
    DBUG_ASSERT(_table != 0);
    if (_table->s->primary_key == MAX_KEY)
      share->flags|= NSF_HIDDEN_PK;
    return;
  }
  while (1) 
  {
    TABLE_SHARE *table_share= 
      (TABLE_SHARE *) my_malloc(sizeof(*table_share), MYF(MY_WME));
    TABLE *table= (TABLE*) my_malloc(sizeof(*table), MYF(MY_WME));
    int error;

    init_tmp_table_share(table_share, share->db, 0, share->table_name, 
                         share->key);
    if ((error= open_table_def(thd, table_share, 0)))
    {
      sql_print_error("Unable to get table share for %s, error=%d",
                      share->key, error);
      DBUG_PRINT("error", ("open_table_def failed %d", error));
      my_free((gptr) table_share, MYF(0));
      table_share= 0;
      my_free((gptr) table, MYF(0));
      table= 0;
      break;
    }
    if ((error= open_table_from_share(thd, table_share, "", 0, 
                                      (uint) READ_ALL, 0, table)))
    {
      sql_print_error("Unable to open table for %s, error=%d(%d)",
                      share->key, error, my_errno);
      DBUG_PRINT("error", ("open_table_from_share failed %d", error));
      my_free((gptr) table_share, MYF(0));
      table_share= 0;
      my_free((gptr) table, MYF(0));
      table= 0;
      break;
    }
    assign_new_table_id(table);
    if (!table->record[1] || table->record[1] == table->record[0])
    {
      table->record[1]= alloc_root(&table->mem_root,
                                   table->s->rec_buff_length);
    }
    table->in_use= injector_thd;
        
    table->s->db.str= share->db;
    table->s->db.length= strlen(share->db);
    table->s->table_name.str= share->table_name;
    table->s->table_name.length= strlen(share->table_name);
 
    share->table_share= table_share;
    share->table= table;
#ifndef DBUG_OFF
    dbug_print_table("table", table);
#endif
    /*
      ! do not touch the contents of the table
      it may be in use by the injector thread
    */
    share->ndb_value[0]= (NdbValue*)
      alloc_root(mem_root, sizeof(NdbValue) * table->s->fields
                 + 1 /*extra for hidden key*/);
    share->ndb_value[1]= (NdbValue*)
      alloc_root(mem_root, sizeof(NdbValue) * table->s->fields
                 +1 /*extra for hidden key*/);
    {
      int i, no_nodes= g_ndb_cluster_connection->no_db_nodes();
      share->subscriber_bitmap= (MY_BITMAP*)
        alloc_root(mem_root, no_nodes * sizeof(MY_BITMAP));
      for (i= 0; i < no_nodes; i++)
      {
        bitmap_init(&share->subscriber_bitmap[i],
                    (Uint32*)alloc_root(mem_root, max_ndb_nodes/8),
                    max_ndb_nodes, false);
        bitmap_clear_all(&share->subscriber_bitmap[i]);
      }
      bitmap_init(&share->slock_bitmap, share->slock,
                  sizeof(share->slock)*8, false);
      bitmap_clear_all(&share->slock_bitmap);
    }
    if (table->s->primary_key == MAX_KEY)
      share->flags|= NSF_HIDDEN_PK;
    break;
  }
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
  if (ndb_binlog_thread_running > 0)
  {
    DBUG_ENTER("ndbcluster_binlog_wait");
    const char *save_info= thd ? thd->proc_info : 0;
    ulonglong wait_epoch= g_latest_trans_gci;
    int count= 30;
    if (thd)
      thd->proc_info= "Waiting for ndbcluster binlog update to "
	"reach current position";
    while (count && ndb_binlog_thread_running > 0 &&
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
 Called from MYSQL_LOG::reset_logs in log.cc when binlog is emptied
*/
static int ndbcluster_reset_logs(THD *thd)
{
  if (ndb_binlog_thread_running <= 0)
    return 0;

  DBUG_ENTER("ndbcluster_reset_logs");

  /*
    Wait for all events orifinating from this mysql server has
    reached the binlog before continuing to reset
  */
  ndbcluster_binlog_wait(thd);

  char buf[1024];
  char *end= strmov(buf, "DELETE FROM " NDB_REP_DB "." NDB_REP_TABLE);

  run_query(thd, buf, end, FALSE, TRUE);

  DBUG_RETURN(0);
}

/*
  Called from MYSQL_LOG::purge_logs in log.cc when the binlog "file"
  is removed
*/

static int
ndbcluster_binlog_index_purge_file(THD *thd, const char *file)
{
  if (ndb_binlog_thread_running <= 0)
    return 0;

  DBUG_ENTER("ndbcluster_binlog_index_purge_file");
  DBUG_PRINT("enter", ("file: %s", file));

  char buf[1024];
  char *end= strmov(strmov(strmov(buf,
                                  "DELETE FROM "
                                  NDB_REP_DB "." NDB_REP_TABLE
                                  " WHERE File='"), file), "'");

  run_query(thd, buf, end, FALSE, TRUE);

  DBUG_RETURN(0);
}

static void
ndbcluster_binlog_log_query(THD *thd, enum_binlog_command binlog_command,
                            const char *query, uint query_length,
                            const char *db, const char *table_name)
{
  DBUG_ENTER("ndbcluster_binlog_log_query");
  DBUG_PRINT("enter", ("db: %s  table_name: %s  query: %s",
                       db, table_name, query));
  DBUG_VOID_RETURN;
}

/*
  End use of the NDB Cluster table handler
  - free all global variables allocated by 
    ndbcluster_init()
*/

static int ndbcluster_binlog_end(THD *thd)
{
  DBUG_ENTER("ndb_binlog_end");

  if (!ndbcluster_util_inited)
    DBUG_RETURN(0);

  // Kill ndb utility thread
  (void) pthread_mutex_lock(&LOCK_ndb_util_thread);
  DBUG_PRINT("exit",("killing ndb util thread: %lx", ndb_util_thread));
  (void) pthread_cond_signal(&COND_ndb_util_thread);
  (void) pthread_mutex_unlock(&LOCK_ndb_util_thread);

#ifdef HAVE_NDB_BINLOG
  /* wait for injector thread to finish */
  if (ndb_binlog_thread_running > 0)
  {
    pthread_mutex_lock(&injector_mutex);
    while (ndb_binlog_thread_running > 0)
    {
      struct timespec abstime;
      set_timespec(abstime, 1);
      pthread_cond_timedwait(&injector_cond, &injector_mutex, &abstime);
    }
    pthread_mutex_unlock(&injector_mutex);
  }

  /* remove all shares */
  {
    pthread_mutex_lock(&ndbcluster_mutex);
    for (uint i= 0; i < ndbcluster_open_tables.records; i++)
    {
      NDB_SHARE *share=
        (NDB_SHARE*) hash_element(&ndbcluster_open_tables, i);
      if (share->table)
        DBUG_PRINT("share",
                   ("table->s->db.table_name: %s.%s",
                    share->table->s->db.str, share->table->s->table_name.str));
      if (share->state != NSS_DROPPED && !--share->use_count)
        real_free_share(&share);
      else
      {
        DBUG_PRINT("share",
                   ("[%d] 0x%lx  key: %s  key_length: %d",
                    i, share, share->key, share->key_length));
        DBUG_PRINT("share",
                   ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
                    share->db, share->table_name,
                    share->use_count, share->commit_count));
      }
    }
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
#endif
  ndbcluster_util_inited= 0;
  DBUG_RETURN(0);
}

/*****************************************************************
  functions called from slave sql client threads
****************************************************************/
static void ndbcluster_reset_slave(THD *thd)
{
  if (ndb_binlog_thread_running <= 0)
    return;

  DBUG_ENTER("ndbcluster_reset_slave");
  char buf[1024];
  char *end= strmov(buf, "DELETE FROM " NDB_REP_DB "." NDB_APPLY_TABLE);
  run_query(thd, buf, end, FALSE, TRUE);
  DBUG_VOID_RETURN;
}

/*
  Initialize the binlog part of the ndb handlerton
*/
static int ndbcluster_binlog_func(THD *thd, enum_binlog_func fn, void *arg)
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
  handlerton &h= ndbcluster_hton;
  h.binlog_func=      ndbcluster_binlog_func;
  h.binlog_log_query= ndbcluster_binlog_log_query;
}





/*
  check the availability af the cluster_replication.apply_status share
  - return share, but do not increase refcount
  - return 0 if there is no share
*/
static NDB_SHARE *ndbcluster_check_apply_status_share()
{
  pthread_mutex_lock(&ndbcluster_mutex);

  void *share= hash_search(&ndbcluster_open_tables, 
                           NDB_APPLY_TABLE_FILE,
                           sizeof(NDB_APPLY_TABLE_FILE) - 1);
  DBUG_PRINT("info",("ndbcluster_check_apply_status_share %s %p",
                     NDB_APPLY_TABLE_FILE, share));
  pthread_mutex_unlock(&ndbcluster_mutex);
  return (NDB_SHARE*) share;
}

/*
  check the availability af the cluster_replication.schema share
  - return share, but do not increase refcount
  - return 0 if there is no share
*/
static NDB_SHARE *ndbcluster_check_schema_share()
{
  pthread_mutex_lock(&ndbcluster_mutex);

  void *share= hash_search(&ndbcluster_open_tables, 
                           NDB_SCHEMA_TABLE_FILE,
                           sizeof(NDB_SCHEMA_TABLE_FILE) - 1);
  DBUG_PRINT("info",("ndbcluster_check_schema_share %s %p",
                     NDB_SCHEMA_TABLE_FILE, share));
  pthread_mutex_unlock(&ndbcluster_mutex);
  return (NDB_SHARE*) share;
}

/*
  Create the cluster_replication.apply_status table
*/
static int ndbcluster_create_apply_status_table(THD *thd)
{
  DBUG_ENTER("ndbcluster_create_apply_status_table");

  /*
    Check if we already have the apply status table.
    If so it should have been discovered at startup
    and thus have a share
  */

  if (ndbcluster_check_apply_status_share())
    DBUG_RETURN(0);

  if (g_ndb_cluster_connection->get_no_ready() <= 0)
    DBUG_RETURN(0);

  char buf[1024], *end;

  if (ndb_extra_logging)
    sql_print_information("NDB: Creating " NDB_REP_DB "." NDB_APPLY_TABLE);

  /*
    Check if apply status table exists in MySQL "dictionary"
    if so, remove it since there is none in Ndb
  */
  {
    strxnmov(buf, sizeof(buf),
             mysql_data_home,
             "/" NDB_REP_DB "/" NDB_APPLY_TABLE,
             reg_ext, NullS);
    unpack_filename(buf,buf);
    my_delete(buf, MYF(0));
  }

  /*
    Note, updating this table schema must be reflected in ndb_restore
  */
  end= strmov(buf, "CREATE TABLE IF NOT EXISTS "
                   NDB_REP_DB "." NDB_APPLY_TABLE
                   " ( server_id INT UNSIGNED NOT NULL,"
                   " epoch BIGINT UNSIGNED NOT NULL, "
                   " PRIMARY KEY USING HASH (server_id) ) ENGINE=NDB");

  run_query(thd, buf, end, TRUE, TRUE);

  DBUG_RETURN(0);
}


/*
  Create the cluster_replication.schema table
*/
static int ndbcluster_create_schema_table(THD *thd)
{
  DBUG_ENTER("ndbcluster_create_schema_table");

  /*
    Check if we already have the schema table.
    If so it should have been discovered at startup
    and thus have a share
  */

  if (ndbcluster_check_schema_share())
    DBUG_RETURN(0);

  if (g_ndb_cluster_connection->get_no_ready() <= 0)
    DBUG_RETURN(0);

  char buf[1024], *end;

  if (ndb_extra_logging)
    sql_print_information("NDB: Creating " NDB_REP_DB "." NDB_SCHEMA_TABLE);

  /*
    Check if schema table exists in MySQL "dictionary"
    if so, remove it since there is none in Ndb
  */
  {
    strxnmov(buf, sizeof(buf),
             mysql_data_home,
             "/" NDB_REP_DB "/" NDB_SCHEMA_TABLE,
             reg_ext, NullS);
    unpack_filename(buf,buf);
    my_delete(buf, MYF(0));
  }

  /*
    Update the defines below to reflect the table schema
  */
  end= strmov(buf, "CREATE TABLE IF NOT EXISTS "
                   NDB_REP_DB "." NDB_SCHEMA_TABLE
                   " ( db VARCHAR(63) NOT NULL,"
                   " name VARCHAR(63) NOT NULL,"
                   " slock BINARY(32) NOT NULL,"
                   " query VARCHAR(4094) NOT NULL,"
                   " node_id INT UNSIGNED NOT NULL,"
                   " epoch BIGINT UNSIGNED NOT NULL,"
                   " id INT UNSIGNED NOT NULL,"
                   " version INT UNSIGNED NOT NULL,"
                   " type INT UNSIGNED NOT NULL,"
                   " PRIMARY KEY USING HASH (db,name) ) ENGINE=NDB");

  run_query(thd, buf, end, TRUE, TRUE);

  DBUG_RETURN(0);
}

void ndbcluster_setup_binlog_table_shares(THD *thd)
{
  int done_find_all_files= 0;
  if (!apply_status_share &&
      ndbcluster_check_apply_status_share() == 0)
  {
    if (!done_find_all_files)
    {
      ndbcluster_find_all_files(thd);
      done_find_all_files= 1;
    }
    ndbcluster_create_apply_status_table(thd);
  }
  if (!schema_share &&
      ndbcluster_check_schema_share() == 0)
  {
    if (!done_find_all_files)
    {
      ndbcluster_find_all_files(thd);
      done_find_all_files= 1;
    }
    ndbcluster_create_schema_table(thd);
  }
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
#define SCHEMA_QUERY_SIZE 4096u

struct Cluster_replication_schema
{
  unsigned char db_length;
  char db[64];
  unsigned char name_length;
  char name[64];
  unsigned char slock_length;
  uint32 slock[SCHEMA_SLOCK_SIZE/4];
  unsigned short query_length;
  char query[SCHEMA_QUERY_SIZE];
  Uint64 epoch;
  uint32 node_id;
  uint32 id;
  uint32 version;
  uint32 type;
};

/*
  Transfer schema table data into corresponding struct
*/
static void ndbcluster_get_schema(TABLE *table,
                                  Cluster_replication_schema *s)
{
  Field **field;
  /* db varchar 1 length byte */
  field= table->field;
  s->db_length= *(uint8*)(*field)->ptr;
  DBUG_ASSERT(s->db_length <= (*field)->field_length);
  DBUG_ASSERT((*field)->field_length + 1 == sizeof(s->db));
  memcpy(s->db, (*field)->ptr + 1, s->db_length);
  s->db[s->db_length]= 0;
  /* name varchar 1 length byte */
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
  /* query varchar 2 length bytes */
  field++;
  s->query_length= uint2korr((*field)->ptr);
  DBUG_ASSERT(s->query_length <= (*field)->field_length);
  DBUG_ASSERT((*field)->field_length + 2 == sizeof(s->query));
  memcpy(s->query, (*field)->ptr + 2, s->query_length);
  s->query[s->query_length]= 0;
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
}

/*
  helper function to pack a ndb varchar
*/
static char *ndb_pack_varchar(const NDBCOL *col, char *buf,
                              const char *str, int sz)
{
  switch (col->getArrayType())
  {
    case NDBCOL::ArrayTypeFixed:
      memcpy(buf, str, sz);
      break;
    case NDBCOL::ArrayTypeShortVar:
      *(unsigned char*)buf= (unsigned char)sz;
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
  log query in schema table
*/
int ndbcluster_log_schema_op(THD *thd, NDB_SHARE *share,
                             const char *query, int query_length,
                             const char *db, const char *table_name,
                             uint32 ndb_table_id,
                             uint32 ndb_table_version,
                             enum SCHEMA_OP_TYPE type)
{
  DBUG_ENTER("ndbcluster_log_schema_op");
#ifdef NOT_YET
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
  if (!schema_share || thd_ndb->options & TNO_NO_LOG_SCHEMA_OP)
  {
    DBUG_RETURN(0);
  }

  char tmp_buf2[FN_REFLEN];
  switch (type)
  {
  case SOT_DROP_TABLE:
    /* drop database command, do not log at drop table */
    if (thd->lex->sql_command ==  SQLCOM_DROP_DB)
      DBUG_RETURN(0);
    /* redo the drop table query as is may contain several tables */
    query= tmp_buf2;
    query_length= (uint) (strxmov(tmp_buf2, "drop table `",
                                  table_name, "`", NullS) - tmp_buf2);   
    break;
  case SOT_CREATE_TABLE:
    break;
  case SOT_RENAME_TABLE:
    break;
  case SOT_ALTER_TABLE:
    break;
  case SOT_DROP_DB:
    break;
  case SOT_CREATE_DB:
    break;
  case SOT_ALTER_DB:
    break;
  default:
    abort(); /* should not happen, programming error */
  }

  const NdbError *ndb_error= 0;
  uint32 node_id= g_ndb_cluster_connection->node_id();
  Uint64 epoch= 0;
  MY_BITMAP schema_subscribers;
  uint32 bitbuf[sizeof(schema_share->slock)/4];
  {
    int i;
    bitmap_init(&schema_subscribers, bitbuf, sizeof(bitbuf)*8, false);
    bitmap_set_all(&schema_subscribers);
    (void) pthread_mutex_lock(&schema_share->mutex);
    for (i= 0; i < ndb_number_of_storage_nodes; i++)
    {
      MY_BITMAP *table_subscribers= &schema_share->subscriber_bitmap[i];
      if (!bitmap_is_clear_all(table_subscribers))
        bitmap_intersect(&schema_subscribers,
                         table_subscribers);
    }
    (void) pthread_mutex_unlock(&schema_share->mutex);
    bitmap_clear_bit(&schema_subscribers, node_id);
      
    if (share)
    {
      (void) pthread_mutex_lock(&share->mutex);
      memcpy(share->slock, schema_subscribers.bitmap, sizeof(share->slock));
      (void) pthread_mutex_unlock(&share->mutex);
    }

    DBUG_DUMP("schema_subscribers", (char*)schema_subscribers.bitmap,
              no_bytes_in_map(&schema_subscribers));
    DBUG_PRINT("info", ("bitmap_is_clear_all(&schema_subscribers): %d",
                        bitmap_is_clear_all(&schema_subscribers)));
  }

  Ndb *ndb= thd_ndb->ndb;
  char old_db[128];
  strcpy(old_db, ndb->getDatabaseName());

  char tmp_buf[SCHEMA_QUERY_SIZE];
  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(NDB_REP_DB);
  const NDBTAB *ndbtab= dict->getTable(NDB_SCHEMA_TABLE);
  NdbTransaction *trans= 0;
  int retries= 100;
  const NDBCOL *col[SCHEMA_SIZE];
  unsigned sz[SCHEMA_SIZE];

  if (ndbtab == 0)
  {
    if (strcmp(NDB_REP_DB, db) != 0 ||
        strcmp(NDB_SCHEMA_TABLE, table_name))
    {
      ndb_error= &dict->getNdbError();
      goto end;
    }
    DBUG_RETURN(0);
  }

  {
    uint i;
    for (i= 0; i < SCHEMA_SIZE; i++)
    {
      col[i]= ndbtab->getColumn(i);
      sz[i]= col[i]->getLength();
      DBUG_ASSERT(sz[i] <= sizeof(tmp_buf));
    }
  }

  while (1)
  {
    if ((trans= ndb->startTransaction()) == 0)
      goto err;
    {
      NdbOperation *op= 0;
      int r= 0;
      r|= (op= trans->getNdbOperation(ndbtab)) == 0;
      DBUG_ASSERT(r == 0);
      r|= op->writeTuple();
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
      DBUG_ASSERT(sz[SCHEMA_SLOCK_I] == sizeof(bitbuf));
      r|= op->setValue(SCHEMA_SLOCK_I, (char*)schema_subscribers.bitmap);
      DBUG_ASSERT(r == 0);
      /* query */
      ndb_pack_varchar(col[SCHEMA_QUERY_I], tmp_buf, query, query_length);
      r|= op->setValue(SCHEMA_QUERY_I, tmp_buf);
      DBUG_ASSERT(r == 0);
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
      r|= op->setValue(SCHEMA_TYPE_I, (uint32)type);
      DBUG_ASSERT(r == 0);
    }
    if (trans->execute(NdbTransaction::Commit) == 0)
    {
      dict->forceGCPWait();
      DBUG_PRINT("info", ("logged: %s", query));
      break;
    }
err:
    if (trans->getNdbError().status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        ndb->closeTransaction(trans);
        continue; // retry
      }
    }
    ndb_error= &trans->getNdbError();
    break;
  }
end:
  if (ndb_error)
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        ndb_error->code,
                        ndb_error->message,
                        "Could not log query '%s' on other mysqld's");
          
  if (trans)
    ndb->closeTransaction(trans);
  ndb->setDatabaseName(old_db);

  /*
    Wait for other mysqld's to acknowledge the table operation
  */
  if (ndb_error == 0 &&
      (type == SOT_CREATE_TABLE ||
       type == SOT_RENAME_TABLE ||
       type == SOT_ALTER_TABLE) &&
      !bitmap_is_clear_all(&schema_subscribers))
  {
    int max_timeout= 10;
    (void) pthread_mutex_lock(&share->mutex);
    while (1)
    {
      struct timespec abstime;
      int i;
      set_timespec(abstime, 1);
      (void) pthread_cond_timedwait(&injector_cond,
                                    &share->mutex,
                                    &abstime);

      (void) pthread_mutex_lock(&schema_share->mutex);
      for (i= 0; i < ndb_number_of_storage_nodes; i++)
      {
        /* remove any unsubscribed from schema_subscribers */
        MY_BITMAP *tmp= &schema_share->subscriber_bitmap[i];
        if (!bitmap_is_clear_all(tmp))
          bitmap_intersect(&schema_subscribers, tmp);
      }
      (void) pthread_mutex_unlock(&schema_share->mutex);

      /* remove any unsubscribed from share->slock */
      bitmap_intersect(&share->slock_bitmap, &schema_subscribers);

      DBUG_DUMP("share->slock_bitmap.bitmap", (char*)share->slock_bitmap.bitmap,
                no_bytes_in_map(&share->slock_bitmap));

      if (bitmap_is_clear_all(&share->slock_bitmap))
        break;

      max_timeout--;
      if (max_timeout == 0)
      {
        sql_print_error("NDB create table: timed out. Ignoring...");
        break;
      }
      sql_print_information("NDB create table: "
                            "waiting max %u sec for create table %s.",
                            max_timeout, share->key);
    }
    (void) pthread_mutex_unlock(&share->mutex);
  }
#endif
  DBUG_RETURN(0);
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
  if (!schema_share)
  {
    DBUG_RETURN(0);
  }

  const NdbError *ndb_error= 0;
  uint32 node_id= g_ndb_cluster_connection->node_id();
  Ndb *ndb= check_ndb_in_thd(thd);
  char old_db[128];
  strcpy(old_db, ndb->getDatabaseName());

  char tmp_buf[SCHEMA_QUERY_SIZE];
  NDBDICT *dict= ndb->getDictionary();
  ndb->setDatabaseName(NDB_REP_DB);
  const NDBTAB *ndbtab= dict->getTable(NDB_SCHEMA_TABLE);
  NdbTransaction *trans= 0;
  int retries= 100;
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
      sz[i]= col[i]->getLength();
      DBUG_ASSERT(sz[i] <= sizeof(tmp_buf));
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
    if (trans->getNdbError().status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        ndb->closeTransaction(trans);
        continue; // retry
      }
    }
    ndb_error= &trans->getNdbError();
    break;
  }
end:
  if (ndb_error)
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        ndb_error->code,
                        ndb_error->message,
                        "Could not release lock on '%s.%s'",
                        db, table_name);
  if (trans)
    ndb->closeTransaction(trans);
  ndb->setDatabaseName(old_db);
  DBUG_RETURN(0);
}

/*
  Handle _non_ data events from the storage nodes
*/
static int
ndb_handle_schema_change(THD *thd, Ndb *ndb, NdbEventOperation *pOp,
                         NDB_SHARE *share)
{
  int remote_drop_table= 0, do_close_cached_tables= 0;

  if (pOp->getEventType() != NDBEVENT::TE_CLUSTER_FAILURE &&
      pOp->getReqNodeId() != g_ndb_cluster_connection->node_id())
  {
    ndb->setDatabaseName(share->table->s->db.str);
    ha_ndbcluster::invalidate_dictionary_cache(share->table,
                                               ndb,
                                               share->table->s->table_name.str,
                                               TRUE);
    remote_drop_table= 1;
  }

  (void) pthread_mutex_lock(&share->mutex);
  DBUG_ASSERT(share->op == pOp || share->op_old == pOp);
  if (share->op_old == pOp)
    share->op_old= 0;
  else
    share->op= 0;
  // either just us or drop table handling as well
      
  /* Signal ha_ndbcluster::delete/rename_table that drop is done */
  (void) pthread_mutex_unlock(&share->mutex);
  (void) pthread_cond_signal(&injector_cond);

  pthread_mutex_lock(&ndbcluster_mutex);
  free_share(&share, TRUE);
  if (remote_drop_table && share && share->state != NSS_DROPPED)
  {
    DBUG_PRINT("info", ("remote drop table"));
    if (share->use_count != 1)
      do_close_cached_tables= 1;
    share->state= NSS_DROPPED;
    free_share(&share, TRUE);
  }
  pthread_mutex_unlock(&ndbcluster_mutex);

  share= 0;
  pOp->setCustomData(0);
          
  pthread_mutex_lock(&injector_mutex);
  injector_ndb->dropEventOperation(pOp);
  pOp= 0;
  pthread_mutex_unlock(&injector_mutex);

  if (do_close_cached_tables)
    close_cached_tables((THD*) 0, 0, (TABLE_LIST*) 0);
  return 0;
}

static int
ndb_binlog_thread_handle_schema_event(THD *thd, Ndb *ndb,
                                      NdbEventOperation *pOp,
                                      List<Cluster_replication_schema> 
                                      *schema_list, MEM_ROOT *mem_root)
{
  DBUG_ENTER("ndb_binlog_thread_handle_schema_event");
  NDB_SHARE *share= (NDB_SHARE *)pOp->getCustomData();
  if (share && schema_share == share)
  {
    NDBEVENT::TableEvent ev_type= pOp->getEventType();
    DBUG_PRINT("enter", ("%s.%s  ev_type: %d",
                         share->db, share->table_name, ev_type));
    switch (ev_type)
    {
    case NDBEVENT::TE_UPDATE:
    case NDBEVENT::TE_INSERT:
    {
      Cluster_replication_schema *schema= (Cluster_replication_schema *)
        sql_alloc(sizeof(Cluster_replication_schema));
      MY_BITMAP slock;
      bitmap_init(&slock, schema->slock, 8*SCHEMA_SLOCK_SIZE, false);
      uint node_id= g_ndb_cluster_connection->node_id();
      ndbcluster_get_schema(share->table, schema);
      if (schema->node_id != node_id)
      {
        int log_query= 0;
        DBUG_PRINT("info", ("log query_length: %d  query: '%s'",
                            schema->query_length, schema->query));
        switch ((enum SCHEMA_OP_TYPE)schema->type)
        {
        case SOT_DROP_TABLE:
          /* binlog dropping table after any table operations */
          schema_list->push_back(schema, mem_root);
          log_query= 0;
          break;
        case SOT_CREATE_TABLE:
          /* fall through */
        case SOT_RENAME_TABLE:
          /* fall through */
        case SOT_ALTER_TABLE:
          pthread_mutex_lock(&LOCK_open);
          if (ha_create_table_from_engine(thd, schema->db, schema->name))
          {
            sql_print_error("Could not discover table '%s.%s' from "
                            "binlog schema event '%s' from node %d",
                            schema->db, schema->name, schema->query,
                            schema->node_id);
          }
          pthread_mutex_unlock(&LOCK_open);
          {
            /* signal that schema operation has been handled */
            DBUG_DUMP("slock", (char*)schema->slock, schema->slock_length);
            if (bitmap_is_set(&slock, node_id))
              ndbcluster_update_slock(thd, schema->db, schema->name);
          }
          log_query= 1;
          break;
        case SOT_DROP_DB:
          run_query(thd, schema->query,
                    schema->query + schema->query_length,
                    TRUE,    /* print error */
                    TRUE);   /* don't binlog the query */
          /* binlog dropping database after any table operations */
          schema_list->push_back(schema, mem_root);
          log_query= 0;
          break;
        case SOT_CREATE_DB:
          /* fall through */
        case SOT_ALTER_DB:
          run_query(thd, schema->query,
                    schema->query + schema->query_length,
                    TRUE,    /* print error */
                    FALSE);  /* binlog the query */
          log_query= 0;
          break;
        case SOT_CLEAR_SLOCK:
        {
          char key[FN_REFLEN];
          (void)strxnmov(key, FN_REFLEN, share_prefix, schema->db,
                         "/", schema->name, NullS);
          NDB_SHARE *share= get_share(key, 0, false, false);
          if (share)
          {
            pthread_mutex_lock(&share->mutex);
            memcpy(share->slock, schema->slock, sizeof(share->slock));
            DBUG_DUMP("share->slock_bitmap.bitmap",
                      (char*)share->slock_bitmap.bitmap,
                      no_bytes_in_map(&share->slock_bitmap));
            pthread_mutex_unlock(&share->mutex);
            pthread_cond_signal(&injector_cond);
            free_share(&share);
          }
          DBUG_RETURN(0);
        }
        }
        if (log_query)
        {
          char *thd_db_save= thd->db;
          thd->db= schema->db;
          thd->binlog_query(THD::STMT_QUERY_TYPE, schema->query,
                            schema->query_length, FALSE,
                            schema->name[0] == 0);
          thd->db= thd_db_save;
        }
      }
    }
    break;
    case NDBEVENT::TE_DELETE:
      // skip
      break;
    case NDBEVENT::TE_ALTER:
      /* do the rename of the table in the share */
      share->table->s->db.str= share->db;
      share->table->s->db.length= strlen(share->db);
      share->table->s->table_name.str= share->table_name;
      share->table->s->table_name.length= strlen(share->table_name);
      ndb_handle_schema_change(thd, ndb, pOp, share);
      break;
    case NDBEVENT::TE_CLUSTER_FAILURE:
    case NDBEVENT::TE_DROP:
      free_share(&schema_share);
      schema_share= 0;
      ndb_handle_schema_change(thd, ndb, pOp, share);
      break;
    case NDBEVENT::TE_NODE_FAILURE:
    {
      uint8 node_id= g_node_id_map[pOp->getNdbdNodeId()];
      DBUG_ASSERT(node_id != 0xFF);
      (void) pthread_mutex_lock(&share->mutex);
      bitmap_clear_all(&share->subscriber_bitmap[node_id]);
      DBUG_PRINT("info",("NODE_FAILURE UNSUBSCRIBE[%d]", node_id));
      (void) pthread_mutex_unlock(&share->mutex);
      (void) pthread_cond_signal(&injector_cond);
      break;
    }
    case NDBEVENT::TE_SUBSCRIBE:
    {
      uint8 node_id= g_node_id_map[pOp->getNdbdNodeId()];
      uint8 req_id= pOp->getReqNodeId();
      DBUG_ASSERT(req_id != 0 && node_id != 0xFF);
      (void) pthread_mutex_lock(&share->mutex);
      bitmap_set_bit(&share->subscriber_bitmap[node_id], req_id);
      DBUG_PRINT("info",("SUBSCRIBE[%d] %d", node_id, req_id));
      (void) pthread_mutex_unlock(&share->mutex);
      (void) pthread_cond_signal(&injector_cond);
      break;
    }
    case NDBEVENT::TE_UNSUBSCRIBE:
    {
      uint8 node_id= g_node_id_map[pOp->getNdbdNodeId()];
      uint8 req_id= pOp->getReqNodeId();
      DBUG_ASSERT(req_id != 0 && node_id != 0xFF);
      (void) pthread_mutex_lock(&share->mutex);
      bitmap_clear_bit(&share->subscriber_bitmap[node_id], req_id);
      DBUG_PRINT("info",("UNSUBSCRIBE[%d] %d", node_id, req_id));
      (void) pthread_mutex_unlock(&share->mutex);
      (void) pthread_cond_signal(&injector_cond);
      break;
    }
    default:
      sql_print_error("NDB Binlog: unknown non data event %d for %s. "
                      "Ignoring...", (unsigned) ev_type, share->key);
    }
  }
  DBUG_RETURN(0);
}

/*
  Timer class for doing performance measurements
*/

/*********************************************************************
  Internal helper functions for handeling of the cluster replication tables
  - cluster_replication.binlog_index
  - cluster_replication.apply_status
*********************************************************************/

/*
  struct to hold the data to be inserted into the
  cluster_replication.binlog_index table
*/
struct Binlog_index_row {
  ulonglong gci;
  const char *master_log_file;
  ulonglong master_log_pos;
  ulonglong n_inserts;
  ulonglong n_updates;
  ulonglong n_deletes;
  ulonglong n_schemaops;
};

/*
  Open the cluster_replication.binlog_index table
*/
static int open_binlog_index(THD *thd, TABLE_LIST *tables,
                             TABLE **binlog_index)
{
  static char repdb[]= NDB_REP_DB;
  static char reptable[]= NDB_REP_TABLE;
  const char *save_proc_info= thd->proc_info;

  bzero((char*) tables, sizeof(*tables));
  tables->db= repdb;
  tables->alias= tables->table_name= reptable;
  tables->lock_type= TL_WRITE;
  thd->proc_info= "Opening " NDB_REP_DB "." NDB_REP_TABLE;
  tables->required_type= FRMTYPE_TABLE;
  uint counter;
  thd->clear_error();
  if (open_tables(thd, &tables, &counter, MYSQL_LOCK_IGNORE_FLUSH))
  {
    sql_print_error("NDB Binlog: Opening binlog_index: %d, '%s'",
                    thd->net.last_errno,
                    thd->net.last_error ? thd->net.last_error : "");
    thd->proc_info= save_proc_info;
    return -1;
  }
  *binlog_index= tables->table;
  thd->proc_info= save_proc_info;
  return 0;
}

/*
  Insert one row in the cluster_replication.binlog_index

  declared friend in handler.h to be able to call write_row directly
  so that this insert is not replicated
*/
int ndb_add_binlog_index(THD *thd, void *_row)
{
  Binlog_index_row &row= *(Binlog_index_row *) _row;
  int error= 0;
  bool need_reopen;
  for ( ; ; ) /* loop for need_reopen */
  {
    if (!binlog_index && open_binlog_index(thd, &binlog_tables, &binlog_index))
    {
      error= -1;
      goto add_binlog_index_err;
    }

    if (lock_tables(thd, &binlog_tables, 1, &need_reopen))
    {
      if (need_reopen)
      {
        close_tables_for_reopen(thd, &binlog_tables);
	binlog_index= 0;
        continue;
      }
      sql_print_error("NDB Binlog: Unable to lock table binlog_index");
      error= -1;
      goto add_binlog_index_err;
    }
    break;
  }

  binlog_index->field[0]->store(row.master_log_pos);
  binlog_index->field[1]->store(row.master_log_file,
                                strlen(row.master_log_file),
                                &my_charset_bin);
  binlog_index->field[2]->store(row.gci);
  binlog_index->field[3]->store(row.n_inserts);
  binlog_index->field[4]->store(row.n_updates);
  binlog_index->field[5]->store(row.n_deletes);
  binlog_index->field[6]->store(row.n_schemaops);

  int r;
  if ((r= binlog_index->file->write_row(binlog_index->record[0])))
  {
    sql_print_error("NDB Binlog: Writing row to binlog_index: %d", r);
    error= -1;
    goto add_binlog_index_err;
  }

  mysql_unlock_tables(thd, thd->lock);
  thd->lock= 0;
  return 0;
add_binlog_index_err:
  close_thread_tables(thd);
  binlog_index= 0;
  return error;
}

/*********************************************************************
  Functions for start, stop, wait for ndbcluster binlog thread
*********************************************************************/

static int do_ndbcluster_binlog_close_connection= 0;

int ndbcluster_binlog_start()
{
  DBUG_ENTER("ndbcluster_binlog_start");

  pthread_mutex_init(&injector_mutex, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&injector_cond, NULL);

  /* Create injector thread */
  if (pthread_create(&ndb_binlog_thread, &connection_attrib,
                     ndb_binlog_thread_func, 0))
  {
    DBUG_PRINT("error", ("Could not create ndb injector thread"));
    pthread_cond_destroy(&injector_cond);
    pthread_mutex_destroy(&injector_mutex);
    DBUG_RETURN(-1);
  }

  /*
    Wait for the ndb injector thread to finish starting up.
  */
  pthread_mutex_lock(&injector_mutex);
  while (!ndb_binlog_thread_running)
    pthread_cond_wait(&injector_cond, &injector_mutex);
  pthread_mutex_unlock(&injector_mutex);
  
  if (ndb_binlog_thread_running < 0)
    DBUG_RETURN(-1);

  DBUG_RETURN(0);
}

static void ndbcluster_binlog_close_connection(THD *thd)
{
  DBUG_ENTER("ndbcluster_binlog_close_connection");
  const char *save_info= thd->proc_info;
  thd->proc_info= "ndbcluster_binlog_close_connection";
  do_ndbcluster_binlog_close_connection= 1;
  while (ndb_binlog_thread_running > 0)
    sleep(1);
  thd->proc_info= save_info;
  DBUG_VOID_RETURN;
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

/*
  Common function for setting up everything for logging a table at
  create/discover.
*/
int ndbcluster_create_binlog_setup(Ndb *ndb, const char *key,
                                   const char *db,
                                   const char *table_name,
                                   NDB_SHARE *share)
{
  DBUG_ENTER("ndbcluster_create_binlog_setup");

  pthread_mutex_lock(&ndbcluster_mutex);

  /* Handle any trailing share */
  if (share == 0)
  {
    share= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                    (byte*) key, strlen(key));
    if (share)
      handle_trailing_share(share);
  }
  else
    handle_trailing_share(share);
  
  /* Create share which is needed to hold replication information */
  if (!(share= get_share(key, 0, true, true)))
  {
    sql_print_error("NDB Binlog: "
                    "allocating table share for %s failed", key);
  }
  pthread_mutex_unlock(&ndbcluster_mutex);

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
    const NDBTAB *ndbtab= dict->getTable(table_name);
    if (ndbtab == 0)
    {
      if (ndb_extra_logging)
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
    if (!dict->getEvent(event_name.c_ptr()))
    {
      if (ndbcluster_create_event(ndb, ndbtab, event_name.c_ptr(), share))
      {
        sql_print_error("NDB Binlog: "
                        "FAILED CREATE (DISCOVER) TABLE Event: %s",
                        event_name.c_ptr());
        break; // error
      }
      if (ndb_extra_logging)
        sql_print_information("NDB Binlog: "
                              "CREATE (DISCOVER) TABLE Event: %s",
                              event_name.c_ptr());
    }
    else
      if (ndb_extra_logging)
        sql_print_information("NDB Binlog: DISCOVER TABLE Event: %s",
                              event_name.c_ptr());

    /*
      create the event operations for receiving logging events
    */
    if (ndbcluster_create_event_ops(share, ndbtab,
                                    event_name.c_ptr()) < 0)
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
                        const char *event_name, NDB_SHARE *share)
{
  DBUG_ENTER("ndbcluster_create_event");
  if (!share)
  {
    DBUG_PRINT("info", ("share == NULL"));
    DBUG_RETURN(0);
  }
  if (share->flags & NSF_NO_BINLOG)
  {
    DBUG_PRINT("info", ("share->flags & NSF_NO_BINLOG, flags: %x %d", share->flags, share->flags & NSF_NO_BINLOG));
    DBUG_RETURN(0);
  }

  NDBDICT *dict= ndb->getDictionary();
  NDBEVENT my_event(event_name);
  my_event.setTable(*ndbtab);
  my_event.addTableEvent(NDBEVENT::TE_ALL);
  if (share->flags & NSF_HIDDEN_PK)
  {
    /* No primary key, susbscribe for all attributes */
    my_event.setReport(NDBEVENT::ER_ALL);
    DBUG_PRINT("info", ("subscription all"));
  }
  else
  {
    if (schema_share || strcmp(share->db, NDB_REP_DB) ||
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

  /* add all columns to the event */
  int n_cols= ndbtab->getNoOfColumns();
  for(int a= 0; a < n_cols; a++)
    my_event.addEventColumn(a);

  if (dict->createEvent(my_event)) // Add event to database
  {
#ifdef NDB_BINLOG_EXTRA_WARNINGS
    /*
      failed, print a warning
    */
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        dict->getNdbError().code,
                        dict->getNdbError().message, "NDB");
#endif
    if (dict->getNdbError().classification != NdbError::SchemaObjectExists)
    {
      sql_print_error("NDB Binlog: Unable to create event in database. "
                      "Event: %s  Error Code: %d  Message: %s", event_name,
                      dict->getNdbError().code, dict->getNdbError().message);
      DBUG_RETURN(-1);
    }

    /*
      trailing event from before; an error, but try to correct it
    */
    if (dict->dropEvent(my_event.getName()))
    {
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
      sql_print_error("NDB Binlog: Unable to create event in database. "
                      " Attempt to correct with drop ok, but create failed. "
                      "Event: %s Error Code: %d Message: %s",
                      event_name,
                      dict->getNdbError().code,
                      dict->getNdbError().message);
      DBUG_RETURN(-1);
    }
#ifdef NDB_BINLOG_EXTRA_WARNINGS
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
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
  /*
    we are in either create table or rename table so table should be
    locked, hence we can work with the share without locks
  */

  DBUG_ENTER("ndbcluster_create_event_ops");

  DBUG_ASSERT(share != 0);

  if (share->flags & NSF_NO_BINLOG)
  {
    DBUG_PRINT("info", ("share->flags & NSF_NO_BINLOG, flags: %x", share->flags));
    DBUG_RETURN(0);
  }

  if (share->op)
  {
    assert(share->op->getCustomData() == (void *) share);

    DBUG_ASSERT(share->use_count > 1);
    sql_print_error("NDB Binlog: discover reusing old ev op");
    free_share(&share); // old event op already has reference
    DBUG_RETURN(0);
  }

  TABLE *table= share->table;
  if (table)
  {
    /*
      Logging of blob tables is not yet implemented, it would require:
      1. setup of events also on the blob attribute tables
      2. collect the pieces of the blob into one from an epoch to
         provide a full blob to binlog
    */
    if (table->s->blob_fields)
    {
      sql_print_error("NDB Binlog: logging of blob table %s "
                      "is not supported", share->key);
      share->flags|= NSF_NO_BINLOG;
      DBUG_RETURN(0);
    }
  }

  int do_schema_share= 0, do_apply_status_share= 0;
  int retries= 100;
  if (!schema_share && strcmp(share->db, NDB_REP_DB) == 0 &&
      strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
    do_schema_share= 1;
  else if (!apply_status_share && strcmp(share->db, NDB_REP_DB) == 0 &&
           strcmp(share->table_name, NDB_APPLY_TABLE) == 0)
    do_apply_status_share= 1;

  while (1)
  {
    pthread_mutex_lock(&injector_mutex);
    Ndb *ndb= injector_ndb;
    if (do_schema_share)
      ndb= schema_ndb;

    if (ndb == 0)
    {
      pthread_mutex_unlock(&injector_mutex);
      DBUG_RETURN(-1);
    }

    NdbEventOperation *op= ndb->createEventOperation(event_name);
    if (!op)
    {
      pthread_mutex_unlock(&injector_mutex);
      sql_print_error("NDB Binlog: Creating NdbEventOperation failed for"
                      " %s",event_name);
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                          ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                          ndb->getNdbError().code,
                          ndb->getNdbError().message,
                          "NDB");
      DBUG_RETURN(-1);
    }

    int n_columns= ndbtab->getNoOfColumns();
    int n_fields= table ? table->s->fields : 0;
    for (int j= 0; j < n_columns; j++)
    {
      const char *col_name= ndbtab->getColumn(j)->getName();
      NdbRecAttr *attr0, *attr1;
      if (j < n_fields)
      {
        Field *f= share->table->field[j];
        if (is_ndb_compatible_type(f))
        {
          DBUG_PRINT("info", ("%s compatible", col_name));
          attr0= op->getValue(col_name, f->ptr);
          attr1= op->getPreValue(col_name, (f->ptr-share->table->record[0]) +
                                 share->table->record[1]);
        }
        else
        {
          DBUG_PRINT("info", ("%s non compatible", col_name));
          attr0= op->getValue(col_name);
          attr1= op->getPreValue(col_name);
        }
      }
      else
      {
        DBUG_PRINT("info", ("%s hidden key", col_name));
        attr0= op->getValue(col_name);
        attr1= op->getPreValue(col_name);
      }
      share->ndb_value[0][j].rec= attr0;
      share->ndb_value[1][j].rec= attr1;
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
        push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                            ER_GET_ERRMSG, ER(ER_GET_ERRMSG), 
                            op->getNdbError().code, op->getNdbError().message,
                            "NDB");
        sql_print_error("NDB Binlog: ndbevent->execute failed for %s; %d %s",
                        event_name,
                        op->getNdbError().code, op->getNdbError().message);
      }
      ndb->dropEventOperation(op);
      pthread_mutex_unlock(&injector_mutex);
      if (retries)
        continue;
      DBUG_RETURN(-1);
    }
    pthread_mutex_unlock(&injector_mutex);
    break;
  }

  get_share(share);
  if (do_apply_status_share)
    apply_status_share= get_share(share);
  else if (do_schema_share)
    schema_share= get_share(share);

  DBUG_PRINT("info",("%s share->op: 0x%lx, share->use_count: %u",
                     share->key, share->op, share->use_count));

  if (ndb_extra_logging)
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
                             NDB_SHARE *share)
{
  DBUG_ENTER("ndbcluster_handle_drop_table");

  NDBDICT *dict= ndb->getDictionary();
  if (event_name && dict->dropEvent(event_name))
  {
    if (dict->getNdbError().code != 4710)
    {
      /* drop event failed for some reason, issue a warning */
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
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
          dict->getNdbError().code != 4009)
      {
        DBUG_ASSERT(false);
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
#define SYNC_DROP_
#ifdef SYNC_DROP_
  (void) pthread_mutex_lock(&share->mutex);
  int max_timeout= 10;
  while (share->op)
  {
    struct timespec abstime;
    set_timespec(abstime, 1);
    (void) pthread_cond_timedwait(&injector_cond,
                                  &share->mutex,
                                  &abstime);
    max_timeout--;
    if (share->op == 0)
      break;
    if (max_timeout == 0)
    {
      sql_print_error("NDB delete table: timed out. Ignoring...");
      break;
    }
    if (ndb_extra_logging)
      sql_print_information("NDB delete table: "
                            "waiting max %u sec for drop table %s.",
                            max_timeout, share->key);
  }
  (void) pthread_mutex_unlock(&share->mutex);
#else
  (void) pthread_mutex_lock(&share->mutex);
  share->op_old= share->op;
  share->op= 0;
  (void) pthread_mutex_unlock(&share->mutex);
#endif

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
                                          Binlog_index_row &row)
{
  NDB_SHARE *share= (NDB_SHARE *)pOp->getCustomData();
  DBUG_ENTER("ndb_binlog_thread_handle_error");

  int overrun= pOp->isOverrun();
  if (overrun)
  {
    /*
      ToDo: this error should rather clear the binlog_index...
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
      ToDo: this error should rather clear the binlog_index...
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
ndb_binlog_thread_handle_non_data_event(Ndb *ndb, NdbEventOperation *pOp,
                                        Binlog_index_row &row)
{
  NDB_SHARE *share= (NDB_SHARE *)pOp->getCustomData();
  NDBEVENT::TableEvent type= pOp->getEventType();

  /* make sure to flush any pending events as they can be dependent
     on one of the tables being changed below
  */
  injector_thd->binlog_flush_pending_rows_event(true);

  switch (type)
  {
  case NDBEVENT::TE_CLUSTER_FAILURE:
    if (apply_status_share == share)
    {
      free_share(&apply_status_share);
      apply_status_share= 0;
    }
    if (ndb_extra_logging)
      sql_print_information("NDB Binlog: cluster failure for %s.", share->key);
    DBUG_PRINT("info", ("CLUSTER FAILURE EVENT: "
                        "%s  received share: 0x%lx  op: %lx  share op: %lx  "
                        "op_old: %lx",
                       share->key, share, pOp, share->op, share->op_old));
    break;
  case NDBEVENT::TE_ALTER:
    /* ToDo: remove printout */
    if (ndb_extra_logging)
      sql_print_information("NDB Binlog: rename table %s%s/%s -> %s.",
                            share_prefix, share->table->s->db.str,
                            share->table->s->table_name.str,
                            share->key);
    /* do the rename of the table in the share */
    share->table->s->db.str= share->db;
    share->table->s->db.length= strlen(share->db);
    share->table->s->table_name.str= share->table_name;
    share->table->s->table_name.length= strlen(share->table_name);
    goto drop_alter_common;
  case NDBEVENT::TE_DROP:
    if (apply_status_share == share)
    {
      free_share(&apply_status_share);
      apply_status_share= 0;
    }
    /* ToDo: remove printout */
    if (ndb_extra_logging)
      sql_print_information("NDB Binlog: drop table %s.", share->key);
drop_alter_common:
    row.n_schemaops++;
    DBUG_PRINT("info", ("TABLE %s EVENT: %s  received share: 0x%lx  op: %lx  "
                        "share op: %lx  op_old: %lx",
                       type == NDBEVENT::TE_DROP ? "DROP" : "ALTER",
                       share->key, share, pOp, share->op, share->op_old));
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

  ndb_handle_schema_change(injector_thd, ndb, pOp, share);
  return 0;
}

/*
  Handle data events from the storage nodes
*/
static int
ndb_binlog_thread_handle_data_event(Ndb *ndb, NdbEventOperation *pOp,
                                    Binlog_index_row &row,
                                    injector::transaction &trans)
{
  NDB_SHARE *share= (NDB_SHARE*) pOp->getCustomData();
  if (share == apply_status_share)
    return 0;
  TABLE *table= share->table;

  assert(table != 0);

  dbug_print_table("table", table);

  TABLE_SHARE *table_s= table->s;
  uint n_fields= table_s->fields;
  MY_BITMAP b;
  /* Potential buffer for the bitmap */
  uint32 bitbuf[128 / (sizeof(uint32) * 8)];
  bitmap_init(&b, n_fields <= sizeof(bitbuf) * 8 ? bitbuf : NULL, 
              n_fields, false);
  bitmap_set_all(&b);

  /*
   row data is already in table->record[0]
   As we told the NdbEventOperation to do this
   (saves moving data about many times)
  */

  switch(pOp->getEventType())
  {
  case NDBEVENT::TE_INSERT:
    row.n_inserts++;
    DBUG_PRINT("info", ("INSERT INTO %s", share->key));
    {
      ndb_unpack_record(table, share->ndb_value[0], &b, table->record[0]);
      trans.write_row(::server_id, injector::transaction::table(table, true),
                      &b, n_fields, table->record[0]);
    }
    break;
  case NDBEVENT::TE_DELETE:
    row.n_deletes++;
    DBUG_PRINT("info",("DELETE FROM %s", share->key));
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

      ndb_unpack_record(table, share->ndb_value[n], &b, table->record[n]);
      print_records(table, table->record[n]);
      trans.delete_row(::server_id, injector::transaction::table(table, true),
                       &b, n_fields, table->record[n]);
    }
    break;
  case NDBEVENT::TE_UPDATE:
    row.n_updates++;
    DBUG_PRINT("info", ("UPDATE %s", share->key));
    {
      ndb_unpack_record(table, share->ndb_value[0],
                        &b, table->record[0]);
      print_records(table, table->record[0]);
      if (table->s->primary_key != MAX_KEY) 
      {
        /*
          since table has a primary key, we can to a write
          using only after values
	*/
        trans.write_row(::server_id, injector::transaction::table(table, true),
                        &b, n_fields, table->record[0]);// after values
      }
      else
      {
        /*
          mysql server cannot handle the ndb hidden key and
          therefore needs the before image as well
	*/
        ndb_unpack_record(table, share->ndb_value[1], &b, table->record[1]);
        print_records(table, table->record[1]);
        trans.update_row(::server_id,
                         injector::transaction::table(table, true),
                         &b, n_fields,
                         table->record[1], // before values
                         table->record[0]);// after values
      }
    }
    break;
  default:
    /* We should REALLY never get here. */
    DBUG_PRINT("info", ("default - uh oh, a brain exploded."));
    break;
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

pthread_handler_t ndb_binlog_thread_func(void *arg)
{
  THD *thd; /* needs to be first for thread_stack */
  Ndb *ndb= 0;
  Thd_ndb *thd_ndb=0;
  int ndb_update_binlog_index= 1;
  injector *inj= injector::instance();

  pthread_mutex_lock(&injector_mutex);
  /*
    Set up the Thread
  */
  my_thread_init();
  DBUG_ENTER("ndb_binlog_thread");

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  THD_CHECK_SENTRY(thd);

  thd->thread_stack= (char*) &thd; /* remember where our stack is */
  if (thd->store_globals())
  {
    thd->cleanup();
    delete thd;
    ndb_binlog_thread_running= -1;
    pthread_mutex_unlock(&injector_mutex);
    pthread_cond_signal(&injector_cond);
    my_thread_end();
    pthread_exit(0);
    DBUG_RETURN(NULL);
  }

  thd->init_for_queries();
  thd->command= COM_DAEMON;
  thd->system_thread= SYSTEM_THREAD_NDBCLUSTER_BINLOG;
  thd->version= refresh_version;
  thd->set_time();
  thd->main_security_ctx.host_or_ip= "";
  thd->client_capabilities= 0;
  my_net_init(&thd->net, 0);
  thd->main_security_ctx.master_access= ~0;
  thd->main_security_ctx.priv_user= 0;

  /*
    Set up ndb binlog
  */
  sql_print_information("Starting MySQL Cluster Binlog Thread");

  pthread_detach_this_thread();
  thd->real_id= pthread_self();
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thread_id++;
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  thd->lex->start_transaction_opt= 0;

  if (!(schema_ndb= new Ndb(g_ndb_cluster_connection, "")) ||
      schema_ndb->init())
  {
    sql_print_error("NDB Binlog: Getting Schema Ndb object failed");
    goto err;
  }

  if (!(ndb= new Ndb(g_ndb_cluster_connection, "")) ||
      ndb->init())
  {
    sql_print_error("NDB Binlog: Getting Ndb object failed");
    ndb_binlog_thread_running= -1;
    pthread_mutex_unlock(&injector_mutex);
    pthread_cond_signal(&injector_cond);
    goto err;
  }

  /*
    Expose global reference to our ndb object.

    Used by both sql client thread and binlog thread to interact
    with the storage
    pthread_mutex_lock(&injector_mutex);
  */
  injector_thd= thd;
  injector_ndb= ndb;
  ndb_binlog_thread_running= 1;

  /*
    We signal the thread that started us that we've finished
    starting up.
  */
  pthread_mutex_unlock(&injector_mutex);
  pthread_cond_signal(&injector_cond);

  thd->proc_info= "Waiting for ndbcluster to start";

  pthread_mutex_lock(&injector_mutex);
  while (!ndbcluster_util_inited)
  {
    /* ndb not connected yet */
    struct timespec abstime;
    set_timespec(abstime, 1);
    pthread_cond_timedwait(&injector_cond, &injector_mutex, &abstime);
    if (abort_loop)
    {
      pthread_mutex_unlock(&injector_mutex);
      goto err;
    }
  }
  pthread_mutex_unlock(&injector_mutex);

  /*
    Main NDB Injector loop
  */

  DBUG_ASSERT(ndbcluster_hton.slot != ~(uint)0);
  if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
  {
    sql_print_error("Could not allocate Thd_ndb object");
    goto err;
  }
  set_thd_ndb(thd, thd_ndb);
  thd_ndb->options|= TNO_NO_LOG_SCHEMA_OP;
  thd->query_id= 0; // to keep valgrind quiet
  {
    static char db[]= "";
    thd->db= db;
    open_binlog_index(thd, &binlog_tables, &binlog_index);
    if (!apply_status_share)
    {
      sql_print_error("NDB: Could not get apply status share");
    }
    thd->db= db;
  }

#ifdef RUN_NDB_BINLOG_TIMER
  Timer main_timer;
#endif
  for ( ; !((abort_loop || do_ndbcluster_binlog_close_connection) &&
            ndb_latest_handled_binlog_epoch >= g_latest_trans_gci); )
  {

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
    Uint64 gci, schema_gci;
    int res= ndb->pollEvents(1000, &gci);
    int schema_res= schema_ndb->pollEvents(0, &schema_gci);
    ndb_latest_received_binlog_epoch= gci;

    while (gci > schema_gci && schema_res >= 0)
      schema_res= schema_ndb->pollEvents(10, &schema_gci);

    if ((abort_loop || do_ndbcluster_binlog_close_connection) &&
        ndb_latest_handled_binlog_epoch >= g_latest_trans_gci)
      break; /* Shutting down server */

    if (binlog_index && binlog_index->s->version < refresh_version)
    {
      if (binlog_index->s->version < refresh_version)
      {
        close_thread_tables(thd);
        binlog_index= 0;
      }
    }

    MEM_ROOT **root_ptr=
      my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
    MEM_ROOT *old_root= *root_ptr;
    MEM_ROOT mem_root;
    init_sql_alloc(&mem_root, 4096, 0);
    List<Cluster_replication_schema> schema_list;
    *root_ptr= &mem_root;

    if (unlikely(schema_res > 0))
    {
      schema_ndb->
        setReportThreshEventGCISlip(ndb_report_thresh_binlog_epoch_slip);
      schema_ndb->setReportThreshEventFreeMem(ndb_report_thresh_binlog_mem_usage);
      NdbEventOperation *pOp= schema_ndb->nextEvent();
      while (pOp != NULL)
      {
        if (!pOp->hasError())
          ndb_binlog_thread_handle_schema_event(thd, schema_ndb, pOp,
                                                &schema_list, &mem_root);
        else
          sql_print_error("NDB: error %lu (%s) on handling "
                          "binlog schema event",
                          (ulong) pOp->getNdbError().code,
                          pOp->getNdbError().message);
        pOp= schema_ndb->nextEvent();
      }
    }

    if (res > 0)
    {
      DBUG_PRINT("info", ("pollEvents res: %d", res));
#ifdef RUN_NDB_BINLOG_TIMER
      Timer gci_timer, write_timer;
      int event_count= 0;
#endif
      thd->proc_info= "Processing events";
      NdbEventOperation *pOp= ndb->nextEvent();
      Binlog_index_row row;
      while (pOp != NULL)
      {
        ndb->
          setReportThreshEventGCISlip(ndb_report_thresh_binlog_epoch_slip);
        ndb->setReportThreshEventFreeMem(ndb_report_thresh_binlog_mem_usage);

        assert(pOp->getGCI() <= ndb_latest_received_binlog_epoch);
        bzero((char*) &row, sizeof(row));
        injector::transaction trans= inj->new_trans(thd);
        gci= pOp->getGCI();
        if (apply_status_share)
        {
          TABLE *table= apply_status_share->table;
          MY_BITMAP b;
          uint32 bitbuf;
          DBUG_ASSERT(table->s->fields <= sizeof(bitbuf) * 8);
          bitmap_init(&b, &bitbuf, table->s->fields, false);
          bitmap_set_all(&b);
          table->field[0]->store((longlong)::server_id);
          table->field[1]->store((longlong)gci);
          trans.write_row(::server_id,
                          injector::transaction::table(table, true),
                          &b, table->s->fields,
                          table->record[0]);
        }
        else
        {
          sql_print_error("NDB: Could not get apply status share");
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
              ndb_binlog_thread_handle_error(ndb, pOp, row) < 0)
            goto err;

#ifndef DBUG_OFF
          {
            NDB_SHARE *share= (NDB_SHARE*) pOp->getCustomData();
            DBUG_PRINT("info",
                       ("EVENT TYPE:%d  GCI:%lld  last applied: %lld  "
                        "share: 0x%lx", pOp->getEventType(), gci,
                        ndb_latest_applied_binlog_epoch, share));
            DBUG_ASSERT(share != 0);
          }
#endif
          if ((unsigned) pOp->getEventType() <
              (unsigned) NDBEVENT::TE_FIRST_NON_DATA_EVENT)
            ndb_binlog_thread_handle_data_event(ndb, pOp, row, trans);
          else
            ndb_binlog_thread_handle_non_data_event(ndb, pOp, row);

          pOp= ndb->nextEvent();
        } while (pOp && pOp->getGCI() == gci);

        /*
          note! pOp is not referring to an event in the next epoch
          or is == 0
	*/
#ifdef RUN_NDB_BINLOG_TIMER
        write_timer.stop();
#endif

        if (row.n_inserts || row.n_updates
            || row.n_deletes || row.n_schemaops)
        {
          injector::transaction::binlog_pos start= trans.start_pos();
          if (int r= trans.commit())
          {
            sql_print_error("NDB binlog:"
                            "Error during COMMIT of GCI. Error: %d",
                            r);
            /* TODO: Further handling? */
          }
          row.gci= gci;
          row.master_log_file= start.file_name();
          row.master_log_pos= start.file_pos();

          DBUG_PRINT("info",("COMMIT gci %lld",gci));
          if (ndb_update_binlog_index)
            ndb_add_binlog_index(thd, &row);
          ndb_latest_applied_binlog_epoch= gci;
        }
        else
          trans.commit();
        ndb_latest_handled_binlog_epoch= gci;
#ifdef RUN_NDB_BINLOG_TIMER
        gci_timer.stop();
        sql_print_information("gci %ld event_count %d write time "
                              "%ld(%d e/s), total time %ld(%d e/s)",
                              (ulong)gci, event_count,
                              write_timer.elapsed_ms(),
                              event_count / write_timer.elapsed_ms(),
                              gci_timer.elapsed_ms(),
                              event_count / gci_timer.elapsed_ms());
#endif
      }
    }

    {
      Cluster_replication_schema *schema;
      while ((schema= schema_list.pop()))
      {
        char *thd_db_save= thd->db;
        thd->db= schema->db;
        thd->binlog_query(THD::STMT_QUERY_TYPE, schema->query,
                          schema->query_length, FALSE,
                          schema->name[0] == 0);
        thd->db= thd_db_save;
      }
    }
    free_root(&mem_root, MYF(0));
    *root_ptr= old_root;
    ndb_latest_handled_binlog_epoch= ndb_latest_received_binlog_epoch;
  }
err:
  DBUG_PRINT("info",("Shutting down cluster binlog thread"));
  close_thread_tables(thd);
  pthread_mutex_lock(&injector_mutex);
  /* don't mess with the injector_ndb anymore from other threads */
  injector_ndb= 0;
  pthread_mutex_unlock(&injector_mutex);
  thd->db= 0; // as not to try to free memory
  sql_print_information("Stopping Cluster Binlog");

  if (apply_status_share)
    free_share(&apply_status_share);
  if (schema_share)
    free_share(&schema_share);

  /* remove all event operations */
  if (ndb)
  {
    NdbEventOperation *op;
    DBUG_PRINT("info",("removing all event operations"));
    while ((op= ndb->getEventOperation()))
    {
      DBUG_PRINT("info",("removing event operation on %s",
                         op->getEvent()->getName()));
      NDB_SHARE *share= (NDB_SHARE*) op->getCustomData();
      free_share(&share);
      ndb->dropEventOperation(op);
    }
    delete ndb;
    ndb= 0;
  }

  // Placed here to avoid a memory leak; TODO: check if needed
  net_end(&thd->net);
  delete thd;

  ndb_binlog_thread_running= -1;
  (void) pthread_cond_signal(&injector_cond);

  DBUG_PRINT("exit", ("ndb_binlog_thread"));
  my_thread_end();

  pthread_exit(0);
  DBUG_RETURN(NULL);
}

bool
ndbcluster_show_status_binlog(THD* thd, stat_print_fn *stat_print,
                              enum ha_stat_type stat_type)
{
  char buf[IO_SIZE];
  uint buflen;
  ulonglong ndb_latest_epoch= 0;
  DBUG_ENTER("ndbcluster_show_status_binlog");
  
  pthread_mutex_lock(&injector_mutex);
  if (injector_ndb)
  {
    ndb_latest_epoch= injector_ndb->getLatestGCI();
    pthread_mutex_unlock(&injector_mutex);

    buflen=
      snprintf(buf, sizeof(buf),
               "latest_epoch=%llu, "
               "latest_trans_epoch=%llu, "
               "latest_received_binlog_epoch=%llu, "
               "latest_handled_binlog_epoch=%llu, "
               "latest_applied_binlog_epoch=%llu",
               ndb_latest_epoch,
               g_latest_trans_gci,
               ndb_latest_received_binlog_epoch,
               ndb_latest_handled_binlog_epoch,
               ndb_latest_applied_binlog_epoch);
    if (stat_print(thd, ndbcluster_hton.name, strlen(ndbcluster_hton.name),
                   "binlog", strlen("binlog"),
                   buf, buflen))
      DBUG_RETURN(TRUE);
  }
  else
    pthread_mutex_unlock(&injector_mutex);
  DBUG_RETURN(FALSE);
}

#endif /* HAVE_NDB_BINLOG */

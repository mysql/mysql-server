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

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"

#include <my_dir.h>
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include "ha_ndbcluster.h"
#include <ndbapi/NdbApi.hpp>
#include <ndbapi/NdbScanFilter.hpp>
#include <../util/Bitmask.hpp>
#include <ndbapi/NdbIndexStat.hpp>

#include "ha_ndbcluster_binlog.h"
#include "ha_ndbcluster_tables.h"

#include <mysql/plugin.h>

#ifdef ndb_dynamite
#undef assert
#define assert(x) do { if(x) break; ::printf("%s %d: assert failed: %s\n", __FILE__, __LINE__, #x); ::fflush(stdout); ::signal(SIGABRT,SIG_DFL); ::abort(); ::kill(::getpid(),6); ::kill(::getpid(),9); } while (0)
#endif

// options from from mysqld.cc
extern my_bool opt_ndb_optimized_node_selection;
extern const char *opt_ndbcluster_connectstring;

const char *ndb_distribution_names[]= {"KEYHASH", "LINHASH", NullS};
TYPELIB ndb_distribution_typelib= { array_elements(ndb_distribution_names)-1,
                                    "", ndb_distribution_names, NULL };
const char *opt_ndb_distribution= ndb_distribution_names[ND_KEYHASH];
enum ndb_distribution opt_ndb_distribution_id= ND_KEYHASH;

// Default value for parallelism
static const int parallelism= 0;

// Default value for max number of transactions
// createable against NDB from this handler
static const int max_transactions= 3; // should really be 2 but there is a transaction to much allocated when loch table is used

static uint ndbcluster_partition_flags();
static uint ndbcluster_alter_table_flags(uint flags);
static int ndbcluster_init(void);
static int ndbcluster_end(ha_panic_function flag);
static bool ndbcluster_show_status(THD*,stat_print_fn *,enum ha_stat_type);
static int ndbcluster_alter_tablespace(THD* thd, st_alter_tablespace *info);
static int ndbcluster_fill_files_table(THD *thd, TABLE_LIST *tables, COND *cond);

handlerton ndbcluster_hton;

static handler *ndbcluster_create_handler(TABLE_SHARE *table,
                                          MEM_ROOT *mem_root)
{
  return new (mem_root) ha_ndbcluster(table);
}

static uint ndbcluster_partition_flags()
{
  return (HA_CAN_PARTITION | HA_CAN_UPDATE_PARTITION_KEY |
          HA_CAN_PARTITION_UNIQUE | HA_USE_AUTO_PARTITION);
}

static uint ndbcluster_alter_table_flags(uint flags)
{
  if (flags & ALTER_DROP_PARTITION)
    return 0;
  else
    return (HA_ONLINE_ADD_INDEX | HA_ONLINE_DROP_INDEX |
            HA_ONLINE_ADD_UNIQUE_INDEX | HA_ONLINE_DROP_UNIQUE_INDEX |
            HA_PARTITION_FUNCTION_SUPPORTED);

}

#define NDB_AUTO_INCREMENT_RETRIES 10

#define ERR_PRINT(err) \
  DBUG_PRINT("error", ("%d  message: %s", err.code, err.message))

#define ERR_RETURN(err)                  \
{                                        \
  const NdbError& tmp= err;              \
  ERR_PRINT(tmp);                        \
  DBUG_RETURN(ndb_to_mysql_error(&tmp)); \
}

#define ERR_BREAK(err, code)             \
{                                        \
  const NdbError& tmp= err;              \
  ERR_PRINT(tmp);                        \
  code= ndb_to_mysql_error(&tmp);        \
  break;                                 \
}

static int ndbcluster_inited= 0;
int ndbcluster_util_inited= 0;

static Ndb* g_ndb= NULL;
Ndb_cluster_connection* g_ndb_cluster_connection= NULL;
uchar g_node_id_map[max_ndb_nodes];

// Handler synchronization
pthread_mutex_t ndbcluster_mutex;

// Table lock handling
HASH ndbcluster_open_tables;

static byte *ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)));
#ifdef HAVE_NDB_BINLOG
static int rename_share(NDB_SHARE *share, const char *new_key);
#endif
static void ndb_set_fragmentation(NDBTAB &tab, TABLE *table, uint pk_len);

static int ndb_get_table_statistics(Ndb*, const NDBTAB *, 
                                    struct Ndb_statistics *);


// Util thread variables
pthread_t ndb_util_thread;
pthread_mutex_t LOCK_ndb_util_thread;
pthread_cond_t COND_ndb_util_thread;
pthread_handler_t ndb_util_thread_func(void *arg);
ulong ndb_cache_check_time;

/*
  Dummy buffer to read zero pack_length fields
  which are mapped to 1 char
*/
static uint32 dummy_buf;

/*
  Stats that can be retrieved from ndb
*/

struct Ndb_statistics {
  Uint64 row_count;
  Uint64 commit_count;
  Uint64 row_size;
  Uint64 fragment_memory;
};

/* Status variables shown with 'show status like 'Ndb%' */

static long ndb_cluster_node_id= 0;
static const char * ndb_connected_host= 0;
static long ndb_connected_port= 0;
static long ndb_number_of_replicas= 0;
long ndb_number_of_storage_nodes= 0;
long ndb_number_of_ready_storage_nodes= 0;
long ndb_connect_count= 0;

static int update_status_variables(Ndb_cluster_connection *c)
{
  ndb_cluster_node_id=         c->node_id();
  ndb_connected_port=          c->get_connected_port();
  ndb_connected_host=          c->get_connected_host();
  ndb_number_of_replicas=      0;
  ndb_number_of_storage_nodes= c->no_db_nodes();
  ndb_number_of_ready_storage_nodes= c->get_no_ready();
  ndb_connect_count= c->get_connect_count();
  return 0;
}

SHOW_VAR ndb_status_variables[]= {
  {"cluster_node_id",        (char*) &ndb_cluster_node_id,         SHOW_LONG},
  {"config_from_host",         (char*) &ndb_connected_host,      SHOW_CHAR_PTR},
  {"config_from_port",         (char*) &ndb_connected_port,          SHOW_LONG},
//  {"number_of_replicas",     (char*) &ndb_number_of_replicas,      SHOW_LONG},
  {"number_of_storage_nodes",(char*) &ndb_number_of_storage_nodes, SHOW_LONG},
  {NullS, NullS, SHOW_LONG}
};

/*
  Error handling functions
*/

/* Note for merge: old mapping table, moved to storage/ndb/ndberror.c */

static int ndb_to_mysql_error(const NdbError *ndberr)
{
  /* read the mysql mapped error code */
  int error= ndberr->mysql_code;

  switch (error)
  {
    /* errors for which we do not add warnings, just return mapped error code
    */
  case HA_ERR_NO_SUCH_TABLE:
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_FOUND_DUPP_KEY:
    return error;

    /* Mapping missing, go with the ndb error code*/
  case -1:
    error= ndberr->code;
    break;

    /* Mapping exists, go with the mapped code */
  default:
    break;
  }

  /*
    Push the NDB error message as warning
    - Used to be able to use SHOW WARNINGS toget more info on what the error is
    - Used by replication to see if the error was temporary
  */
  if (ndberr->status == NdbError::TemporaryError)
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_TEMPORARY_ERRMSG, ER(ER_GET_TEMPORARY_ERRMSG),
			ndberr->code, ndberr->message, "NDB");
  else
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
			ndberr->code, ndberr->message, "NDB");
  return error;
}

int execute_no_commit_ignore_no_key(ha_ndbcluster *h, NdbTransaction *trans)
{
  int res= trans->execute(NdbTransaction::NoCommit,
                          NdbTransaction::AO_IgnoreError,
                          h->m_force_send);
  if (res == 0)
    return 0;

  const NdbError &err= trans->getNdbError();
  if (err.classification != NdbError::ConstraintViolation &&
      err.classification != NdbError::NoDataFound)
    return res;

  return 0;
}

inline
int execute_no_commit(ha_ndbcluster *h, NdbTransaction *trans,
		      bool force_release)
{
#ifdef NOT_USED
  int m_batch_execute= 0;
  if (m_batch_execute)
    return 0;
#endif
  h->release_completed_operations(trans, force_release);
  return h->m_ignore_no_key ?
    execute_no_commit_ignore_no_key(h,trans) :
    trans->execute(NdbTransaction::NoCommit,
		   NdbTransaction::AbortOnError,
		   h->m_force_send);
}

inline
int execute_commit(ha_ndbcluster *h, NdbTransaction *trans)
{
#ifdef NOT_USED
  int m_batch_execute= 0;
  if (m_batch_execute)
    return 0;
#endif
  return trans->execute(NdbTransaction::Commit,
                        NdbTransaction::AbortOnError,
                        h->m_force_send);
}

inline
int execute_commit(THD *thd, NdbTransaction *trans)
{
#ifdef NOT_USED
  int m_batch_execute= 0;
  if (m_batch_execute)
    return 0;
#endif
  return trans->execute(NdbTransaction::Commit,
                        NdbTransaction::AbortOnError,
                        thd->variables.ndb_force_send);
}

inline
int execute_no_commit_ie(ha_ndbcluster *h, NdbTransaction *trans,
			 bool force_release)
{
#ifdef NOT_USED
  int m_batch_execute= 0;
  if (m_batch_execute)
    return 0;
#endif
  h->release_completed_operations(trans, force_release);
  return trans->execute(NdbTransaction::NoCommit,
                        NdbTransaction::AO_IgnoreError,
                        h->m_force_send);
}

/*
  Place holder for ha_ndbcluster thread specific data
*/
static
byte *thd_ndb_share_get_key(THD_NDB_SHARE *thd_ndb_share, uint *length,
                            my_bool not_used __attribute__((unused)))
{
  *length= sizeof(thd_ndb_share->key);
  return (byte*) &thd_ndb_share->key;
}

Thd_ndb::Thd_ndb()
{
  ndb= new Ndb(g_ndb_cluster_connection, "");
  lock_count= 0;
  count= 0;
  all= NULL;
  stmt= NULL;
  error= 0;
  query_state&= NDB_QUERY_NORMAL;
  options= 0;
  (void) hash_init(&open_tables, &my_charset_bin, 5, 0, 0,
                   (hash_get_key)thd_ndb_share_get_key, 0, 0);
}

Thd_ndb::~Thd_ndb()
{
  if (ndb)
  {
#ifndef DBUG_OFF
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      uint leaked= (uint) tmp.m_created - tmp.m_free;
      if (leaked)
        fprintf(stderr, "NDB: Found %u %s%s that %s not been released\n",
                leaked, tmp.m_name,
                (leaked == 1)?"":"'s",
                (leaked == 1)?"has":"have");
    }
#endif
    delete ndb;
    ndb= NULL;
  }
  changed_tables.empty();
  hash_free(&open_tables);
}

void
Thd_ndb::init_open_tables()
{
  count= 0;
  error= 0;
  my_hash_reset(&open_tables);
}

THD_NDB_SHARE *
Thd_ndb::get_open_table(THD *thd, const void *key)
{
  DBUG_ENTER("Thd_ndb::get_open_table");
  HASH_SEARCH_STATE state;
  THD_NDB_SHARE *thd_ndb_share=
    (THD_NDB_SHARE*)hash_first(&open_tables, (byte *)&key, sizeof(key), &state);
  while (thd_ndb_share && thd_ndb_share->key != key)
    thd_ndb_share= (THD_NDB_SHARE*)hash_next(&open_tables, (byte *)&key, sizeof(key), &state);
  if (thd_ndb_share == 0)
  {
    thd_ndb_share= (THD_NDB_SHARE *) alloc_root(&thd->transaction.mem_root,
                                                sizeof(THD_NDB_SHARE));
    thd_ndb_share->key= key;
    thd_ndb_share->stat.last_count= count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
    my_hash_insert(&open_tables, (byte *)thd_ndb_share);
  }
  else if (thd_ndb_share->stat.last_count != count)
  {
    thd_ndb_share->stat.last_count= count;
    thd_ndb_share->stat.no_uncommitted_rows_count= 0;
    thd_ndb_share->stat.records= ~(ha_rows)0;
  }
  DBUG_PRINT("exit", ("thd_ndb_share: 0x%x  key: 0x%x", thd_ndb_share, key));
  DBUG_RETURN(thd_ndb_share);
}

inline
Ndb *ha_ndbcluster::get_ndb()
{
  return get_thd_ndb(current_thd)->ndb;
}

/*
 * manage uncommitted insert/deletes during transactio to get records correct
 */

void ha_ndbcluster::set_rec_per_key()
{
  DBUG_ENTER("ha_ndbcluster::get_status_const");
  for (uint i=0 ; i < table_share->keys ; i++)
  {
    table->key_info[i].rec_per_key[table->key_info[i].key_parts-1]= 1;
  }
  DBUG_VOID_RETURN;
}

ha_rows ha_ndbcluster::records()
{
  ha_rows retval;
  DBUG_ENTER("ha_ndbcluster::records");
  struct Ndb_local_table_statistics *info= m_table_info;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      info->no_uncommitted_rows_count));

  Ndb *ndb= get_ndb();
  ndb->setDatabaseName(m_dbname);
  struct Ndb_statistics stat;
  if (ndb_get_table_statistics(ndb, m_table, &stat) == 0)
  {
    retval= stat.row_count;
  }
  else
  {
    /**
     * Be consistent with BUG#19914 until we fix it properly
     */
    DBUG_RETURN(-1);
  }

  THD *thd= current_thd;
  if (get_thd_ndb(thd)->error)
    info->no_uncommitted_rows_count= 0;

  DBUG_RETURN(retval + info->no_uncommitted_rows_count);
}

void ha_ndbcluster::records_update()
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::records_update");
  struct Ndb_local_table_statistics *info= m_table_info;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      info->no_uncommitted_rows_count));
  //  if (info->records == ~(ha_rows)0)
  {
    Ndb *ndb= get_ndb();
    struct Ndb_statistics stat;
    ndb->setDatabaseName(m_dbname);
    if (ndb_get_table_statistics(ndb, m_table, &stat) == 0)
    {
      stats.mean_rec_length= stat.row_size;
      stats.data_file_length= stat.fragment_memory;
      info->records= stat.row_count;
    }
  }
  {
    THD *thd= current_thd;
    if (get_thd_ndb(thd)->error)
      info->no_uncommitted_rows_count= 0;
  }
  stats.records= info->records+ info->no_uncommitted_rows_count;
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_execute_failure()
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_execute_failure");
  get_thd_ndb(current_thd)->error= 1;
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_update(int c)
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_update");
  struct Ndb_local_table_statistics *info= m_table_info;
  info->no_uncommitted_rows_count+= c;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      info->no_uncommitted_rows_count));
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_reset(THD *thd)
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_reset");
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  thd_ndb->count++;
  thd_ndb->error= 0;
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::ndb_err(NdbTransaction *trans)
{
  int res;
  NdbError err= trans->getNdbError();
  DBUG_ENTER("ndb_err");
  
  ERR_PRINT(err);
  switch (err.classification) {
  case NdbError::SchemaError:
  {
    // TODO perhaps we need to do more here, invalidate also in the cache
    m_table->setStatusInvalid();
    /* Close other open handlers not used by any thread */
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    table_list.db= m_dbname;
    table_list.alias= table_list.table_name= m_tabname;
    close_cached_tables(current_thd, 0, &table_list);
    break;
  }
  default:
    break;
  }
  res= ndb_to_mysql_error(&err);
  DBUG_PRINT("info", ("transformed ndbcluster error %d to mysql error %d", 
                      err.code, res));
  if (res == HA_ERR_FOUND_DUPP_KEY)
  {
    if (m_rows_to_insert == 1)
      m_dupkey= table_share->primary_key;
    else
    {
      /* We are batching inserts, offending key is not available */
      m_dupkey= (uint) -1;
    }
  }
  DBUG_RETURN(res);
}


/*
  Override the default get_error_message in order to add the 
  error message of NDB 
 */

bool ha_ndbcluster::get_error_message(int error, 
                                      String *buf)
{
  DBUG_ENTER("ha_ndbcluster::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));

  Ndb *ndb= get_ndb();
  if (!ndb)
    DBUG_RETURN(FALSE);

  const NdbError err= ndb->getNdbError(error);
  bool temporary= err.status==NdbError::TemporaryError;
  buf->set(err.message, strlen(err.message), &my_charset_bin);
  DBUG_PRINT("exit", ("message: %s, temporary: %d", buf->ptr(), temporary));
  DBUG_RETURN(temporary);
}


#ifndef DBUG_OFF
/*
  Check if type is supported by NDB.
*/

static bool ndb_supported_type(enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_TINY:        
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_INT24:       
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
  case MYSQL_TYPE_DECIMAL:    
  case MYSQL_TYPE_NEWDECIMAL:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATETIME:    
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_TIME:        
  case MYSQL_TYPE_YEAR:        
  case MYSQL_TYPE_STRING:      
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_BLOB:    
  case MYSQL_TYPE_MEDIUM_BLOB:   
  case MYSQL_TYPE_LONG_BLOB:  
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:         
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_GEOMETRY:
    return TRUE;
  case MYSQL_TYPE_NULL:   
    break;
  }
  return FALSE;
}
#endif /* !DBUG_OFF */


/*
  Instruct NDB to set the value of the hidden primary key
*/

bool ha_ndbcluster::set_hidden_key(NdbOperation *ndb_op,
                                   uint fieldnr, const byte *field_ptr)
{
  DBUG_ENTER("set_hidden_key");
  DBUG_RETURN(ndb_op->equal(fieldnr, (char*)field_ptr) != 0);
}


/*
  Instruct NDB to set the value of one primary key attribute
*/

int ha_ndbcluster::set_ndb_key(NdbOperation *ndb_op, Field *field,
                               uint fieldnr, const byte *field_ptr)
{
  uint32 pack_len= field->pack_length();
  DBUG_ENTER("set_ndb_key");
  DBUG_PRINT("enter", ("%d: %s, ndb_type: %u, len=%d", 
                       fieldnr, field->field_name, field->type(),
                       pack_len));
  DBUG_DUMP("key", (char*)field_ptr, pack_len);
  
  DBUG_ASSERT(ndb_supported_type(field->type()));
  DBUG_ASSERT(! (field->flags & BLOB_FLAG));
  // Common implementation for most field types
  DBUG_RETURN(ndb_op->equal(fieldnr, (char*) field_ptr, pack_len) != 0);
}


/*
 Instruct NDB to set the value of one attribute
*/

int ha_ndbcluster::set_ndb_value(NdbOperation *ndb_op, Field *field, 
                                 uint fieldnr, int row_offset,
                                 bool *set_blob_value)
{
  const byte* field_ptr= field->ptr + row_offset;
  uint32 pack_len= field->pack_length();
  DBUG_ENTER("set_ndb_value");
  DBUG_PRINT("enter", ("%d: %s  type: %u  len=%d  is_null=%s", 
                       fieldnr, field->field_name, field->type(), 
                       pack_len, field->is_null(row_offset) ? "Y" : "N"));
  DBUG_DUMP("value", (char*) field_ptr, pack_len);

  DBUG_ASSERT(ndb_supported_type(field->type()));
  {
    // ndb currently does not support size 0
    uint32 empty_field;
    if (pack_len == 0)
    {
      pack_len= sizeof(empty_field);
      field_ptr= (byte *)&empty_field;
      if (field->is_null(row_offset))
        empty_field= 0;
      else
        empty_field= 1;
    }
    if (! (field->flags & BLOB_FLAG))
    {
      if (field->type() != MYSQL_TYPE_BIT)
      {
        if (field->is_null(row_offset))
        {
          DBUG_PRINT("info", ("field is NULL"));
          // Set value to NULL
          DBUG_RETURN((ndb_op->setValue(fieldnr, (char*)NULL) != 0));
	}
        // Common implementation for most field types
        DBUG_RETURN(ndb_op->setValue(fieldnr, (char*)field_ptr) != 0);
      }
      else // if (field->type() == MYSQL_TYPE_BIT)
      {
        longlong bits= field->val_int();
 
        // Round up bit field length to nearest word boundry
        pack_len= ((pack_len + 3) >> 2) << 2;
        DBUG_ASSERT(pack_len <= 8);
        if (field->is_null(row_offset))
          // Set value to NULL
          DBUG_RETURN((ndb_op->setValue(fieldnr, (char*)NULL) != 0));
        DBUG_PRINT("info", ("bit field"));
        DBUG_DUMP("value", (char*)&bits, pack_len);
#ifdef WORDS_BIGENDIAN
        if (pack_len < 5)
        {
          DBUG_RETURN(ndb_op->setValue(fieldnr, ((char*)&bits)+4) != 0);
        }
#endif
        DBUG_RETURN(ndb_op->setValue(fieldnr, (char*)&bits) != 0);
      }
    }
    // Blob type
    NdbBlob *ndb_blob= ndb_op->getBlobHandle(fieldnr);
    if (ndb_blob != NULL)
    {
      if (field->is_null(row_offset))
        DBUG_RETURN(ndb_blob->setNull() != 0);

      Field_blob *field_blob= (Field_blob*)field;

      // Get length and pointer to data
      uint32 blob_len= field_blob->get_length(field_ptr);
      char* blob_ptr= NULL;
      field_blob->get_ptr(&blob_ptr);

      // Looks like NULL ptr signals length 0 blob
      if (blob_ptr == NULL) {
        DBUG_ASSERT(blob_len == 0);
        blob_ptr= (char*)"";
      }

      DBUG_PRINT("value", ("set blob ptr=%p len=%u",
                           blob_ptr, blob_len));
      DBUG_DUMP("value", (char*)blob_ptr, min(blob_len, 26));

      if (set_blob_value)
        *set_blob_value= TRUE;
      // No callback needed to write value
      DBUG_RETURN(ndb_blob->setValue(blob_ptr, blob_len) != 0);
    }
    DBUG_RETURN(1);
  }
}


/*
  Callback to read all blob values.
  - not done in unpack_record because unpack_record is valid
    after execute(Commit) but reading blobs is not
  - may only generate read operations; they have to be executed
    somewhere before the data is available
  - due to single buffer for all blobs, we let the last blob
    process all blobs (last so that all are active)
  - null bit is still set in unpack_record
  - TODO allocate blob part aligned buffers
*/

NdbBlob::ActiveHook g_get_ndb_blobs_value;

int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg)
{
  DBUG_ENTER("g_get_ndb_blobs_value");
  if (ndb_blob->blobsNextBlob() != NULL)
    DBUG_RETURN(0);
  ha_ndbcluster *ha= (ha_ndbcluster *)arg;
  int ret= get_ndb_blobs_value(ha->table, ha->m_value,
                               ha->m_blobs_buffer, ha->m_blobs_buffer_size,
                               ha->m_blobs_offset);
  DBUG_RETURN(ret);
}

/*
  This routine is shared by injector.  There is no common blobs buffer
  so the buffer and length are passed by reference.  Injector also
  passes a record pointer diff.
 */
int get_ndb_blobs_value(TABLE* table, NdbValue* value_array,
                        byte*& buffer, uint& buffer_size,
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
      if (! (field->flags & BLOB_FLAG))
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
        ERR_RETURN(ndb_blob->getNdbError());
      if (isNull == 0) {
        Uint64 len64= 0;
        if (ndb_blob->getLength(len64) != 0)
          ERR_RETURN(ndb_blob->getNdbError());
        // Align to Uint64
        uint32 size= len64;
        if (size % 8 != 0)
          size+= 8 - size % 8;
        if (loop == 1)
        {
          char *buf= buffer + offset;
          uint32 len= 0xffffffff;  // Max uint32
          if (ndb_blob->readData(buf, len) != 0)
            ERR_RETURN(ndb_blob->getNdbError());
          DBUG_PRINT("info", ("[%u] offset=%u buf=%p len=%u [ptrdiff=%d]",
                              i, offset, buf, len, (int)ptrdiff));
          DBUG_ASSERT(len == len64);
          // Ugly hack assumes only ptr needs to be changed
          field_blob->ptr+= ptrdiff;
          field_blob->set_ptr(len, buf);
          field_blob->ptr-= ptrdiff;
        }
        offset+= size;
      }
      else if (loop == 1) // undefined or null
      {
        // have to set length even in this case
        char *buf= buffer + offset; // or maybe NULL
        uint32 len= 0;
        field_blob->ptr+= ptrdiff;
        field_blob->set_ptr(len, buf);
        field_blob->ptr-= ptrdiff;
        DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
      }
    }
    if (loop == 0 && offset > buffer_size)
    {
      my_free(buffer, MYF(MY_ALLOW_ZERO_PTR));
      buffer_size= 0;
      DBUG_PRINT("info", ("allocate blobs buffer size %u", offset));
      buffer= my_malloc(offset, MYF(MY_WME));
      if (buffer == NULL)
        DBUG_RETURN(-1);
      buffer_size= offset;
    }
  }
  DBUG_RETURN(0);
}


/*
  Instruct NDB to fetch one field
  - data is read directly into buffer provided by field
    if field is NULL, data is read into memory provided by NDBAPI
*/

int ha_ndbcluster::get_ndb_value(NdbOperation *ndb_op, Field *field,
                                 uint fieldnr, byte* buf)
{
  DBUG_ENTER("get_ndb_value");
  DBUG_PRINT("enter", ("fieldnr: %d flags: %o", fieldnr,
                       (int)(field != NULL ? field->flags : 0)));

  if (field != NULL)
  {
      DBUG_ASSERT(buf);
      DBUG_ASSERT(ndb_supported_type(field->type()));
      DBUG_ASSERT(field->ptr != NULL);
      if (! (field->flags & BLOB_FLAG))
      { 
        if (field->type() != MYSQL_TYPE_BIT)
        {
          byte *field_buf;
          if (field->pack_length() != 0)
            field_buf= buf + (field->ptr - table->record[0]);
          else
            field_buf= (byte *)&dummy_buf;
          m_value[fieldnr].rec= ndb_op->getValue(fieldnr, 
                                                 field_buf);
        }
        else // if (field->type() == MYSQL_TYPE_BIT)
        {
          m_value[fieldnr].rec= ndb_op->getValue(fieldnr);
        }
        DBUG_RETURN(m_value[fieldnr].rec == NULL);
      }

      // Blob type
      NdbBlob *ndb_blob= ndb_op->getBlobHandle(fieldnr);
      m_value[fieldnr].blob= ndb_blob;
      if (ndb_blob != NULL)
      {
        // Set callback
	m_blobs_offset= buf - (byte*) table->record[0];
        void *arg= (void *)this;
        DBUG_RETURN(ndb_blob->setActiveHook(g_get_ndb_blobs_value, arg) != 0);
      }
      DBUG_RETURN(1);
  }

  // Used for hidden key only
  m_value[fieldnr].rec= ndb_op->getValue(fieldnr, m_ref);
  DBUG_RETURN(m_value[fieldnr].rec == NULL);
}

/*
  Instruct NDB to fetch the partition id (fragment id)
*/
int ha_ndbcluster::get_ndb_partition_id(NdbOperation *ndb_op)
{
  DBUG_ENTER("get_ndb_partition_id");
  DBUG_RETURN(ndb_op->getValue(NdbDictionary::Column::FRAGMENT, 
                               (char *)&m_part_id) == NULL);
}

/*
  Check if any set or get of blob value in current query.
*/

bool ha_ndbcluster::uses_blob_value()
{
  uint blob_fields;
  MY_BITMAP *bitmap;
  uint *blob_index, *blob_index_end;
  if (table_share->blob_fields == 0)
    return FALSE;

  bitmap= m_write_op ? table->write_set : table->read_set;
  blob_index=     table_share->blob_field;
  blob_index_end= blob_index + table_share->blob_fields;
  do
  {
    if (bitmap_is_set(table->write_set,
                      table->field[*blob_index]->field_index))
      return TRUE;
  } while (++blob_index != blob_index_end);
  return FALSE;
}


/*
  Get metadata for this table from NDB 

  IMPLEMENTATION
    - check that frm-file on disk is equal to frm-file
      of table accessed in NDB

  RETURN
    0    ok
    -2   Meta data has changed; Re-read data and try again
*/

int cmp_frm(const NDBTAB *ndbtab, const void *pack_data,
            uint pack_length)
{
  DBUG_ENTER("cmp_frm");
  /*
    Compare FrmData in NDB with frm file from disk.
  */
  if ((pack_length != ndbtab->getFrmLength()) || 
      (memcmp(pack_data, ndbtab->getFrmData(), pack_length)))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

int ha_ndbcluster::get_metadata(const char *path)
{
  Ndb *ndb= get_ndb();
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *tab;
  int error;
  DBUG_ENTER("get_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s, path: %s", m_tabname, path));

  DBUG_ASSERT(m_table == NULL);
  DBUG_ASSERT(m_table_info == NULL);

  const void *data, *pack_data;
  uint length, pack_length;

  /*
    Compare FrmData in NDB with frm file from disk.
  */
  error= 0;
  if (readfrm(path, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
    
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  if (!(tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

  if (get_ndb_share_state(m_share) != NSS_ALTERED 
      && cmp_frm(tab, pack_data, pack_length))
  {
    DBUG_PRINT("error", 
               ("metadata, pack_length: %d  getFrmLength: %d  memcmp: %d",
                pack_length, tab->getFrmLength(),
                memcmp(pack_data, tab->getFrmData(), pack_length)));
    DBUG_DUMP("pack_data", (char*)pack_data, pack_length);
    DBUG_DUMP("frm", (char*)tab->getFrmData(), tab->getFrmLength());
    error= HA_ERR_TABLE_DEF_CHANGED;
  }
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));

  if (error)
    goto err;

  DBUG_PRINT("info", ("fetched table %s", tab->getName()));
  m_table= tab;
  if ((error= open_indexes(ndb, table, FALSE)) == 0)
  {
    ndbtab_g.release();
    DBUG_RETURN(0);
  }
err:
  ndbtab_g.invalidate();
  m_table= NULL;
  DBUG_RETURN(error);
}

static int fix_unique_index_attr_order(NDB_INDEX_DATA &data,
                                       const NDBINDEX *index,
                                       KEY *key_info)
{
  DBUG_ENTER("fix_unique_index_attr_order");
  unsigned sz= index->getNoOfIndexColumns();

  if (data.unique_index_attrid_map)
    my_free((char*)data.unique_index_attrid_map, MYF(0));
  data.unique_index_attrid_map= (uchar*)my_malloc(sz,MYF(MY_WME));

  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ASSERT(key_info->key_parts == sz);
  for (unsigned i= 0; key_part != end; key_part++, i++) 
  {
    const char *field_name= key_part->field->field_name;
#ifndef DBUG_OFF
   data.unique_index_attrid_map[i]= 255;
#endif
    for (unsigned j= 0; j < sz; j++)
    {
      const NDBCOL *c= index->getColumn(j);
      if (strcmp(field_name, c->getName()) == 0)
      {
        data.unique_index_attrid_map[i]= j;
        break;
      }
    }
    DBUG_ASSERT(data.unique_index_attrid_map[i] != 255);
  }
  DBUG_RETURN(0);
}

/*
  Create all the indexes for a table.
  If any index should fail to be created,
  the error is returned immediately
*/
int ha_ndbcluster::create_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  int error= 0;
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::create_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    error= create_index(index_name, key_info, idx_type, i);
    if (error)
    {
      DBUG_PRINT("error", ("Failed to create index %u", i));
      break;
    }
  }

  DBUG_RETURN(error);
}

static void ndb_init_index(NDB_INDEX_DATA &data)
{
  data.type= UNDEFINED_INDEX;
  data.status= UNDEFINED;
  data.unique_index= NULL;
  data.index= NULL;
  data.unique_index_attrid_map= NULL;
  data.index_stat=NULL;
  data.index_stat_cache_entries=0;
  data.index_stat_update_freq=0;
  data.index_stat_query_count=0;
}

static void ndb_clear_index(NDB_INDEX_DATA &data)
{
  if (data.unique_index_attrid_map)
  {
    my_free((char*)data.unique_index_attrid_map, MYF(0));
  }
  if (data.index_stat)
  {
    delete data.index_stat;
  }
  ndb_init_index(data);
}

/*
  Associate a direct reference to an index handle
  with an index (for faster access)
 */
int ha_ndbcluster::add_index_handle(THD *thd, NDBDICT *dict, KEY *key_info,
                                    const char *index_name, uint index_no)
{
  int error= 0;
  NDB_INDEX_TYPE idx_type= get_index_type_from_table(index_no);
  m_index[index_no].type= idx_type;
  DBUG_ENTER("ha_ndbcluster::add_index_handle");
  DBUG_PRINT("enter", ("table %s", m_tabname));

  if (idx_type != PRIMARY_KEY_INDEX && idx_type != UNIQUE_INDEX)
  {
    DBUG_PRINT("info", ("Get handle to index %s", index_name));
    const NDBINDEX *index;
    do
    {
      index= dict->getIndexGlobal(index_name, *m_table);
      if (!index)
        ERR_RETURN(dict->getNdbError());
      DBUG_PRINT("info", ("index: 0x%x  id: %d  version: %d.%d  status: %d",
                          index,
                          index->getObjectId(),
                          index->getObjectVersion() & 0xFFFFFF,
                          index->getObjectVersion() >> 24,
                          index->getObjectStatus()));
      DBUG_ASSERT(index->getObjectStatus() ==
                  NdbDictionary::Object::Retrieved);
      break;
    } while (1);
    m_index[index_no].index= index;
    // ordered index - add stats
    NDB_INDEX_DATA& d=m_index[index_no];
    delete d.index_stat;
    d.index_stat=NULL;
    if (thd->variables.ndb_index_stat_enable)
    {
      d.index_stat=new NdbIndexStat(index);
      d.index_stat_cache_entries=thd->variables.ndb_index_stat_cache_entries;
      d.index_stat_update_freq=thd->variables.ndb_index_stat_update_freq;
      d.index_stat_query_count=0;
      d.index_stat->alloc_cache(d.index_stat_cache_entries);
      DBUG_PRINT("info", ("index %s stat=on cache_entries=%u update_freq=%u",
                          index->getName(),
                          d.index_stat_cache_entries,
                          d.index_stat_update_freq));
    } else
    {
      DBUG_PRINT("info", ("index %s stat=off", index->getName()));
    }
  }
  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    char unique_index_name[FN_LEN];
    static const char* unique_suffix= "$unique";
    m_has_unique_index= TRUE;
    strxnmov(unique_index_name, FN_LEN, index_name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Get handle to unique_index %s", unique_index_name));
    const NDBINDEX *index;
    do
    {
      index= dict->getIndexGlobal(unique_index_name, *m_table);
      if (!index)
        ERR_RETURN(dict->getNdbError());
      DBUG_PRINT("info", ("index: 0x%x  id: %d  version: %d.%d  status: %d",
                          index,
                          index->getObjectId(),
                          index->getObjectVersion() & 0xFFFFFF,
                          index->getObjectVersion() >> 24,
                          index->getObjectStatus()));
      DBUG_ASSERT(index->getObjectStatus() ==
                  NdbDictionary::Object::Retrieved);
      break;
    } while (1);
    m_index[index_no].unique_index= index;
    error= fix_unique_index_attr_order(m_index[index_no], index, key_info);
  }
  if (!error)
    m_index[index_no].status= ACTIVE;
  
  DBUG_RETURN(error);
}

/*
  Associate index handles for each index of a table
*/
int ha_ndbcluster::open_indexes(Ndb *ndb, TABLE *tab, bool ignore_error)
{
  uint i;
  int error= 0;
  THD *thd=current_thd;
  NDBDICT *dict= ndb->getDictionary();
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  DBUG_ENTER("ha_ndbcluster::open_indexes");
  m_has_unique_index= FALSE;
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    if ((error= add_index_handle(thd, dict, key_info, *key_name, i)))
      if (ignore_error)
        m_index[i].index= m_index[i].unique_index= NULL;
      else
        break;
  }

  if (error && !ignore_error)
  {
    while (i > 0)
    {
      i--;
      if (m_index[i].index)
      {
         dict->removeIndexGlobal(*m_index[i].index, 1);
         m_index[i].index= NULL;
      }
      if (m_index[i].unique_index)
      {
         dict->removeIndexGlobal(*m_index[i].unique_index, 1);
         m_index[i].unique_index= NULL;
      }
    }
  }

  DBUG_ASSERT(error == 0 || error == 4243);

  DBUG_RETURN(error);
}

/*
  Renumber indexes in index list by shifting out
  indexes that are to be dropped
 */
void ha_ndbcluster::renumber_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  const char *index_name;
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::renumber_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == TO_BE_DROPPED) 
    {
      DBUG_PRINT("info", ("Shifting index %s(%i) out of the list", 
                          index_name, i));
      NDB_INDEX_DATA tmp;
      uint j= i + 1;
      // Shift index out of list
      while(j != MAX_KEY && m_index[j].status != UNDEFINED)
      {
        tmp=  m_index[j - 1];
        m_index[j - 1]= m_index[j];
        m_index[j]= tmp;
        j++;
      }
    }
  }

  DBUG_VOID_RETURN;
}

/*
  Drop all indexes that are marked for deletion
*/
int ha_ndbcluster::drop_indexes(Ndb *ndb, TABLE *tab)
{
  uint i;
  int error= 0;
  const char *index_name;
  KEY* key_info= tab->key_info;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::drop_indexes");
  
  for (i= 0; i < tab->s->keys; i++, key_info++)
  {
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (m_index[i].status == TO_BE_DROPPED)
    {
      const NdbDictionary::Index *index= m_index[i].index;
      const NdbDictionary::Index *unique_index= m_index[i].unique_index;
      
      if (index)
      {
        index_name= index->getName();
        DBUG_PRINT("info", ("Dropping index %u: %s", i, index_name));  
        // Drop ordered index from ndb
        error= dict->dropIndexGlobal(*index);
        if (!error)
        {
          dict->removeIndexGlobal(*index, 1);
          m_index[i].index= NULL;
        }
      }
      if (!error && unique_index)
      {
        index_name= unique_index->getName();
        DBUG_PRINT("info", ("Dropping unique index %u: %s", i, index_name));
        // Drop unique index from ndb
        error= dict->dropIndexGlobal(*unique_index);
        if (!error)
        {
          dict->removeIndexGlobal(*unique_index, 1);
          m_index[i].unique_index= NULL;
        }
      }
      if (error)
        DBUG_RETURN(error);
      ndb_clear_index(m_index[i]);
      continue;
    }
  }
  
  DBUG_RETURN(error);
}

/*
  Decode the type of an index from information 
  provided in table object
*/
NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_table(uint inx) const
{
  return get_index_type_from_key(inx, table_share->key_info,
                                 inx == table_share->primary_key);
}

NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_key(uint inx,
                                                      KEY *key_info,
                                                      bool primary) const
{
  bool is_hash_index=  (key_info[inx].algorithm == 
                        HA_KEY_ALG_HASH);
  if (primary)
    return is_hash_index ? PRIMARY_KEY_INDEX : PRIMARY_KEY_ORDERED_INDEX;
  
  return ((key_info[inx].flags & HA_NOSAME) ? 
          (is_hash_index ? UNIQUE_INDEX : UNIQUE_ORDERED_INDEX) :
          ORDERED_INDEX);
} 

int ha_ndbcluster::check_index_fields_not_null(uint inx)
{
  KEY* key_info= table->key_info + inx;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ENTER("ha_ndbcluster::check_index_fields_not_null");
  
  for (; key_part != end; key_part++) 
    {
      Field* field= key_part->field;
      if (field->maybe_null())
      {
        my_printf_error(ER_NULL_COLUMN_IN_INDEX,ER(ER_NULL_COLUMN_IN_INDEX),
                        MYF(0),field->field_name);
        DBUG_RETURN(ER_NULL_COLUMN_IN_INDEX);
      }
    }
  
  DBUG_RETURN(0);
}

void ha_ndbcluster::release_metadata(THD *thd, Ndb *ndb)
{
  uint i;

  DBUG_ENTER("release_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  NDBDICT *dict= ndb->getDictionary();
  int invalidate_indexes= 0;
  if (thd && thd->lex && thd->lex->sql_command == SQLCOM_FLUSH)
  {
    invalidate_indexes = 1;
  }
  if (m_table != NULL)
  {
    if (m_table->getObjectStatus() == NdbDictionary::Object::Invalid)
      invalidate_indexes= 1;
    dict->removeTableGlobal(*m_table, invalidate_indexes);
  }
  // TODO investigate
  DBUG_ASSERT(m_table_info == NULL);
  m_table_info= NULL;

  // Release index list 
  for (i= 0; i < MAX_KEY; i++)
  {
    if (m_index[i].unique_index)
    {
      DBUG_ASSERT(m_table != NULL);
      dict->removeIndexGlobal(*m_index[i].unique_index, invalidate_indexes);
    }
    if (m_index[i].index)
    {
      DBUG_ASSERT(m_table != NULL);
      dict->removeIndexGlobal(*m_index[i].index, invalidate_indexes);
    }
    ndb_clear_index(m_index[i]);
  }

  m_table= NULL;
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::get_ndb_lock_type(enum thr_lock_type type)
{
  if (type >= TL_WRITE_ALLOW_WRITE)
    return NdbOperation::LM_Exclusive;
  if (type ==  TL_READ_WITH_SHARED_LOCKS ||
      uses_blob_value())
    return NdbOperation::LM_Read;
  return NdbOperation::LM_CommittedRead;
}

static const ulong index_type_flags[]=
{
  /* UNDEFINED_INDEX */
  0,                         

  /* PRIMARY_KEY_INDEX */
  HA_ONLY_WHOLE_INDEX, 

  /* PRIMARY_KEY_ORDERED_INDEX */
  /* 
     Enable HA_KEYREAD_ONLY when "sorted" indexes are supported, 
     thus ORDERD BY clauses can be optimized by reading directly 
     through the index.
  */
  // HA_KEYREAD_ONLY | 
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER,

  /* UNIQUE_INDEX */
  HA_ONLY_WHOLE_INDEX,

  /* UNIQUE_ORDERED_INDEX */
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER,

  /* ORDERED_INDEX */
  HA_READ_NEXT |
  HA_READ_PREV |
  HA_READ_RANGE |
  HA_READ_ORDER
};

static const int index_flags_size= sizeof(index_type_flags)/sizeof(ulong);

inline NDB_INDEX_TYPE ha_ndbcluster::get_index_type(uint idx_no) const
{
  DBUG_ASSERT(idx_no < MAX_KEY);
  return m_index[idx_no].type;
}


/*
  Get the flags for an index

  RETURN
    flags depending on the type of the index.
*/

inline ulong ha_ndbcluster::index_flags(uint idx_no, uint part,
                                        bool all_parts) const 
{ 
  DBUG_ENTER("ha_ndbcluster::index_flags");
  DBUG_PRINT("enter", ("idx_no: %u", idx_no));
  DBUG_ASSERT(get_index_type_from_table(idx_no) < index_flags_size);
  DBUG_RETURN(index_type_flags[get_index_type_from_table(idx_no)] | 
              HA_KEY_SCAN_NOT_ROR);
}

static void shrink_varchar(Field* field, const byte* & ptr, char* buf)
{
  if (field->type() == MYSQL_TYPE_VARCHAR && ptr != NULL) {
    Field_varstring* f= (Field_varstring*)field;
    if (f->length_bytes == 1) {
      uint pack_len= field->pack_length();
      DBUG_ASSERT(1 <= pack_len && pack_len <= 256);
      if (ptr[1] == 0) {
        buf[0]= ptr[0];
      } else {
        DBUG_ASSERT(FALSE);
        buf[0]= 255;
      }
      memmove(buf + 1, ptr + 2, pack_len - 1);
      ptr= buf;
    }
  }
}

int ha_ndbcluster::set_primary_key(NdbOperation *op, const byte *key)
{
  KEY* key_info= table->key_info + table_share->primary_key;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ENTER("set_primary_key");

  for (; key_part != end; key_part++) 
  {
    Field* field= key_part->field;
    const byte* ptr= key;
    char buf[256];
    shrink_varchar(field, ptr, buf);
    if (set_ndb_key(op, field, 
                    key_part->fieldnr-1, ptr))
      ERR_RETURN(op->getNdbError());
    key += key_part->store_length;
  }
  DBUG_RETURN(0);
}


int ha_ndbcluster::set_primary_key_from_record(NdbOperation *op, const byte *record)
{
  KEY* key_info= table->key_info + table_share->primary_key;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ENTER("set_primary_key_from_record");

  for (; key_part != end; key_part++) 
  {
    Field* field= key_part->field;
    if (set_ndb_key(op, field, 
		    key_part->fieldnr-1, record+key_part->offset))
      ERR_RETURN(op->getNdbError());
  }
  DBUG_RETURN(0);
}

int ha_ndbcluster::set_index_key_from_record(NdbOperation *op, 
                                             const byte *record, uint keyno)
{
  KEY* key_info= table->key_info + keyno;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  uint i;
  DBUG_ENTER("set_index_key_from_record");
                                                                                
  for (i= 0; key_part != end; key_part++, i++)
  {
    Field* field= key_part->field;
    if (set_ndb_key(op, field, m_index[keyno].unique_index_attrid_map[i],
                    record+key_part->offset))
      ERR_RETURN(m_active_trans->getNdbError());
  }
  DBUG_RETURN(0);
}

int 
ha_ndbcluster::set_index_key(NdbOperation *op, 
                             const KEY *key_info, 
                             const byte * key_ptr)
{
  DBUG_ENTER("ha_ndbcluster::set_index_key");
  uint i;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  
  for (i= 0; key_part != end; key_part++, i++) 
  {
    Field* field= key_part->field;
    const byte* ptr= key_part->null_bit ? key_ptr + 1 : key_ptr;
    char buf[256];
    shrink_varchar(field, ptr, buf);
    if (set_ndb_key(op, field, m_index[active_index].unique_index_attrid_map[i], ptr))
      ERR_RETURN(m_active_trans->getNdbError());
    key_ptr+= key_part->store_length;
  }
  DBUG_RETURN(0);
}

inline 
int ha_ndbcluster::define_read_attrs(byte* buf, NdbOperation* op)
{
  uint i;
  DBUG_ENTER("define_read_attrs");  

  // Define attributes to read
  for (i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (bitmap_is_set(table->read_set, i) ||
        ((field->flags & PRI_KEY_FLAG)))
    {      
      if (get_ndb_value(op, field, i, buf))
        ERR_RETURN(op->getNdbError());
    } 
    else
    {
      m_value[i].ptr= NULL;
    }
  }
    
  if (table_share->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Getting hidden key"));
    // Scanning table with no primary key
    int hidden_no= table_share->fields;      
#ifndef DBUG_OFF
    const NDBTAB *tab= (const NDBTAB *) m_table;    
    if (!tab->getColumn(hidden_no))
      DBUG_RETURN(1);
#endif
    if (get_ndb_value(op, NULL, hidden_no, NULL))
      ERR_RETURN(op->getNdbError());
  }
  DBUG_RETURN(0);
} 


/*
  Read one record from NDB using primary key
*/

int ha_ndbcluster::pk_read(const byte *key, uint key_len, byte *buf,
                           uint32 part_id)
{
  uint no_fields= table_share->fields;
  NdbConnection *trans= m_active_trans;
  NdbOperation *op;

  int res;
  DBUG_ENTER("pk_read");
  DBUG_PRINT("enter", ("key_len: %u", key_len));
  DBUG_DUMP("key", (char*)key, key_len);
  m_write_op= FALSE;

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) || 
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());
  
  if (table_share->primary_key == MAX_KEY) 
  {
    // This table has no primary key, use "hidden" primary key
    DBUG_PRINT("info", ("Using hidden key"));
    DBUG_DUMP("key", (char*)key, 8);    
    if (set_hidden_key(op, no_fields, key))
      ERR_RETURN(trans->getNdbError());
    
    // Read key at the same time, for future reference
    if (get_ndb_value(op, NULL, no_fields, NULL))
      ERR_RETURN(trans->getNdbError());
  } 
  else 
  {
    if ((res= set_primary_key(op, key)))
      return res;
  }
  
  if ((res= define_read_attrs(buf, op)))
    DBUG_RETURN(res);

  if (m_use_partition_function)
  {
    op->setPartitionId(part_id);
    // If table has user defined partitioning
    // and no indexes, we need to read the partition id
    // to support ORDER BY queries
    if (table_share->primary_key == MAX_KEY &&
        get_ndb_partition_id(op))
      ERR_RETURN(trans->getNdbError());
  }

  if (execute_no_commit_ie(this,trans,false) != 0) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }

  // The value have now been fetched from NDB  
  unpack_record(buf);
  table->status= 0;     
  DBUG_RETURN(0);
}

/*
  Read one complementing record from NDB using primary key from old_data
  or hidden key
*/

int ha_ndbcluster::complemented_read(const byte *old_data, byte *new_data,
                                     uint32 old_part_id)
{
  uint no_fields= table_share->fields, i;
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  DBUG_ENTER("complemented_read");
  m_write_op= FALSE;

  if (bitmap_is_set_all(table->read_set))
  {
    // We have allready retrieved all fields, nothing to complement
    DBUG_RETURN(0);
  }

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) || 
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());
  if (table_share->primary_key != MAX_KEY) 
  {
    if (set_primary_key_from_record(op, old_data))
      ERR_RETURN(trans->getNdbError());
  } 
  else 
  {
    // This table has no primary key, use "hidden" primary key
    if (set_hidden_key(op, table->s->fields, m_ref))
      ERR_RETURN(op->getNdbError());
  }

  if (m_use_partition_function)
    op->setPartitionId(old_part_id);
  
  // Read all unreferenced non-key field(s)
  for (i= 0; i < no_fields; i++) 
  {
    Field *field= table->field[i];
    if (!((field->flags & PRI_KEY_FLAG) ||
          bitmap_is_set(table->read_set, i)) &&
        !bitmap_is_set(table->write_set, i))
    {
      if (get_ndb_value(op, field, i, new_data))
        ERR_RETURN(trans->getNdbError());
    }
  }
  
  if (execute_no_commit(this,trans,false) != 0) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }

  // The value have now been fetched from NDB  
  unpack_record(new_data);
  table->status= 0;     

  /**
   * restore m_value
   */
  for (i= 0; i < no_fields; i++) 
  {
    Field *field= table->field[i];
    if (!((field->flags & PRI_KEY_FLAG) ||
          bitmap_is_set(table->read_set, i)))
    {
      m_value[i].ptr= NULL;
    }
  }
  
  DBUG_RETURN(0);
}

/*
 * Check that all operations between first and last all
 * have gotten the errcode
 * If checking for HA_ERR_KEY_NOT_FOUND then update m_dupkey
 * for all succeeding operations
 */
bool ha_ndbcluster::check_all_operations_for_error(NdbTransaction *trans,
                                                   const NdbOperation *first,
                                                   const NdbOperation *last,
                                                   uint errcode)
{
  const NdbOperation *op= first;
  DBUG_ENTER("ha_ndbcluster::check_all_operations_for_error");

  while(op)
  {
    NdbError err= op->getNdbError();
    if (err.status != NdbError::Success)
    {
      if (ndb_to_mysql_error(&err) != (int) errcode)
        DBUG_RETURN(false);
      if (op == last) break;
      op= trans->getNextCompletedOperation(op);
    }
    else
    {
      // We found a duplicate
      if (op->getType() == NdbOperation::UniqueIndexAccess)
      {
        if (errcode == HA_ERR_KEY_NOT_FOUND)
        {
          NdbIndexOperation *iop= (NdbIndexOperation *) op;
          const NDBINDEX *index= iop->getIndex();
          // Find the key_no of the index
          for(uint i= 0; i<table->s->keys; i++)
          {
            if (m_index[i].unique_index == index)
            {
              m_dupkey= i;
              break;
            }
          }
        }
      }
      else
      {
        // Must have been primary key access
        DBUG_ASSERT(op->getType() == NdbOperation::PrimaryKeyAccess);
        if (errcode == HA_ERR_KEY_NOT_FOUND)
          m_dupkey= table->s->primary_key;
      }
      DBUG_RETURN(false);      
    }
  }
  DBUG_RETURN(true);
}


/*
 * Peek to check if any rows already exist with conflicting
 * primary key or unique index values
*/

int ha_ndbcluster::peek_indexed_rows(const byte *record)
{
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  const NdbOperation *first, *last;
  uint i;
  int res;
  DBUG_ENTER("peek_indexed_rows");

  NdbOperation::LockMode lm= NdbOperation::LM_Read;
  first= NULL;
  if (table->s->primary_key != MAX_KEY)
  {
    /*
     * Fetch any row with colliding primary key
     */
    if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) ||
        op->readTuple(lm) != 0)
      ERR_RETURN(trans->getNdbError());
    
    first= op;
    if ((res= set_primary_key_from_record(op, record)))
      ERR_RETURN(trans->getNdbError());

    if (m_use_partition_function)
    {
      uint32 part_id;
      int error;
      longlong func_value;
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
      error= m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
      dbug_tmp_restore_column_map(table->read_set, old_map);
      if (error)
        DBUG_RETURN(error);
      op->setPartitionId(part_id);
    }
  }
  /*
   * Fetch any rows with colliding unique indexes
   */
  KEY* key_info;
  KEY_PART_INFO *key_part, *end;
  for (i= 0, key_info= table->key_info; i < table->s->keys; i++, key_info++)
  {
    if (i != table->s->primary_key &&
        key_info->flags & HA_NOSAME)
    {
      // A unique index is defined on table
      NdbIndexOperation *iop;
      const NDBINDEX *unique_index = m_index[i].unique_index;
      key_part= key_info->key_part;
      end= key_part + key_info->key_parts;
      if (!(iop= trans->getNdbIndexOperation(unique_index, m_table)) ||
          iop->readTuple(lm) != 0)
        ERR_RETURN(trans->getNdbError());

      if (!first)
        first= iop;
      if ((res= set_index_key_from_record(iop, record, i)))
        ERR_RETURN(trans->getNdbError());
    }
  }
  last= trans->getLastDefinedOperation();
  if (first)
    res= execute_no_commit_ie(this,trans,false);
  else
  {
    // Table has no keys
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
  }
  if (check_all_operations_for_error(trans, first, last, 
                                     HA_ERR_KEY_NOT_FOUND))
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  } 
  else
  {
    DBUG_PRINT("info", ("m_dupkey %d", m_dupkey));
  }
  DBUG_RETURN(0);
}


/*
  Read one record from NDB using unique secondary index
*/

int ha_ndbcluster::unique_index_read(const byte *key,
                                     uint key_len, byte *buf)
{
  int res;
  NdbTransaction *trans= m_active_trans;
  NdbIndexOperation *op;
  DBUG_ENTER("ha_ndbcluster::unique_index_read");
  DBUG_PRINT("enter", ("key_len: %u, index: %u", key_len, active_index));
  DBUG_DUMP("key", (char*)key, key_len);
  
  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op= trans->getNdbIndexOperation(m_index[active_index].unique_index, 
                                        m_table)) ||
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());
  
  // Set secondary index key(s)
  if ((res= set_index_key(op, table->key_info + active_index, key)))
    DBUG_RETURN(res);
  
  if ((res= define_read_attrs(buf, op)))
    DBUG_RETURN(res);

  if (execute_no_commit_ie(this,trans,false) != 0) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }
  // The value have now been fetched from NDB
  unpack_record(buf);
  table->status= 0;
  DBUG_RETURN(0);
}

inline int ha_ndbcluster::fetch_next(NdbScanOperation* cursor)
{
  DBUG_ENTER("fetch_next");
  int check;
  NdbTransaction *trans= m_active_trans;
  
  if (m_lock_tuple)
  {
    /*
      Lock level m_lock.type either TL_WRITE_ALLOW_WRITE
      (SELECT FOR UPDATE) or TL_READ_WITH_SHARED_LOCKS (SELECT
      LOCK WITH SHARE MODE) and row was not explictly unlocked 
      with unlock_row() call
    */
      NdbConnection *trans= m_active_trans;
      NdbOperation *op;
      // Lock row
      DBUG_PRINT("info", ("Keeping lock on scanned row"));
      
      if (!(op= m_active_cursor->lockCurrentTuple()))
      {
	m_lock_tuple= false;
	ERR_RETURN(trans->getNdbError());
      }
      m_ops_pending++;
  }
  m_lock_tuple= false;
  
  bool contact_ndb= m_lock.type < TL_WRITE_ALLOW_WRITE &&
                    m_lock.type != TL_READ_WITH_SHARED_LOCKS;;
  do {
    DBUG_PRINT("info", ("Call nextResult, contact_ndb: %d", contact_ndb));
    /*
      We can only handle one tuple with blobs at a time.
    */
    if (m_ops_pending && m_blobs_pending)
    {
      if (execute_no_commit(this,trans,false) != 0)
        DBUG_RETURN(ndb_err(trans));
      m_ops_pending= 0;
      m_blobs_pending= FALSE;
    }
    
    if ((check= cursor->nextResult(contact_ndb, m_force_send)) == 0)
    {
      /*
	Explicitly lock tuple if "select for update" or
	"select lock in share mode"
      */
      m_lock_tuple= (m_lock.type == TL_WRITE_ALLOW_WRITE
		     || 
		     m_lock.type == TL_READ_WITH_SHARED_LOCKS);
      DBUG_RETURN(0);
    } 
    else if (check == 1 || check == 2)
    {
      // 1: No more records
      // 2: No more cached records
      
      /*
        Before fetching more rows and releasing lock(s),
        all pending update or delete operations should 
        be sent to NDB
      */
      DBUG_PRINT("info", ("ops_pending: %d", m_ops_pending));    
      if (m_ops_pending)
      {
        if (m_transaction_on)
        {
          if (execute_no_commit(this,trans,false) != 0)
            DBUG_RETURN(-1);
        }
        else
        {
          if  (execute_commit(this,trans) != 0)
            DBUG_RETURN(-1);
          if (trans->restart() != 0)
          {
            DBUG_ASSERT(0);
            DBUG_RETURN(-1);
          }
        }
        m_ops_pending= 0;
      }
      contact_ndb= (check == 2);
    }
    else
    {
      DBUG_RETURN(-1);
    }
  } while (check == 2);

  DBUG_RETURN(1);
}

/*
  Get the next record of a started scan. Try to fetch
  it locally from NdbApi cached records if possible, 
  otherwise ask NDB for more.

  NOTE
  If this is a update/delete make sure to not contact 
  NDB before any pending ops have been sent to NDB.

*/

inline int ha_ndbcluster::next_result(byte *buf)
{  
  int res;
  DBUG_ENTER("next_result");
    
  if (!m_active_cursor)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  
  if ((res= fetch_next(m_active_cursor)) == 0)
  {
    DBUG_PRINT("info", ("One more record found"));    
    
    unpack_record(buf);
    table->status= 0;
    DBUG_RETURN(0);
  }
  else if (res == 1)
  {
    // No more records
    table->status= STATUS_NOT_FOUND;
    
    DBUG_PRINT("info", ("No more records"));
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  else
  {
    DBUG_RETURN(ndb_err(m_active_trans));
  }
}

/*
  Set bounds for ordered index scan.
*/

int ha_ndbcluster::set_bounds(NdbIndexScanOperation *op,
                              uint inx,
                              bool rir,
                              const key_range *keys[2],
                              uint range_no)
{
  const KEY *const key_info= table->key_info + inx;
  const uint key_parts= key_info->key_parts;
  uint key_tot_len[2];
  uint tot_len;
  uint i, j;

  DBUG_ENTER("set_bounds");
  DBUG_PRINT("info", ("key_parts=%d", key_parts));

  for (j= 0; j <= 1; j++)
  {
    const key_range *key= keys[j];
    if (key != NULL)
    {
      // for key->flag see ha_rkey_function
      DBUG_PRINT("info", ("key %d length=%d flag=%d",
                          j, key->length, key->flag));
      key_tot_len[j]= key->length;
    }
    else
    {
      DBUG_PRINT("info", ("key %d not present", j));
      key_tot_len[j]= 0;
    }
  }
  tot_len= 0;

  for (i= 0; i < key_parts; i++)
  {
    KEY_PART_INFO *key_part= &key_info->key_part[i];
    Field *field= key_part->field;
#ifndef DBUG_OFF
    uint part_len= key_part->length;
#endif
    uint part_store_len= key_part->store_length;
    // Info about each key part
    struct part_st {
      bool part_last;
      const key_range *key;
      const byte *part_ptr;
      bool part_null;
      int bound_type;
      const char* bound_ptr;
    };
    struct part_st part[2];

    for (j= 0; j <= 1; j++)
    {
      struct part_st &p= part[j];
      p.key= NULL;
      p.bound_type= -1;
      if (tot_len < key_tot_len[j])
      {
        p.part_last= (tot_len + part_store_len >= key_tot_len[j]);
        p.key= keys[j];
        p.part_ptr= &p.key->key[tot_len];
        p.part_null= key_part->null_bit && *p.part_ptr;
        p.bound_ptr= (const char *)
          p.part_null ? 0 : key_part->null_bit ? p.part_ptr + 1 : p.part_ptr;

        if (j == 0)
        {
          switch (p.key->flag)
          {
            case HA_READ_KEY_EXACT:
              if (! rir)
                p.bound_type= NdbIndexScanOperation::BoundEQ;
              else // differs for records_in_range
                p.bound_type= NdbIndexScanOperation::BoundLE;
              break;
            // ascending
            case HA_READ_KEY_OR_NEXT:
              p.bound_type= NdbIndexScanOperation::BoundLE;
              break;
            case HA_READ_AFTER_KEY:
              if (! p.part_last)
                p.bound_type= NdbIndexScanOperation::BoundLE;
              else
                p.bound_type= NdbIndexScanOperation::BoundLT;
              break;
            // descending
            case HA_READ_PREFIX_LAST:           // weird
              p.bound_type= NdbIndexScanOperation::BoundEQ;
              break;
            case HA_READ_PREFIX_LAST_OR_PREV:   // weird
              p.bound_type= NdbIndexScanOperation::BoundGE;
              break;
            case HA_READ_BEFORE_KEY:
              if (! p.part_last)
                p.bound_type= NdbIndexScanOperation::BoundGE;
              else
                p.bound_type= NdbIndexScanOperation::BoundGT;
              break;
            default:
              break;
          }
        }
        if (j == 1) {
          switch (p.key->flag)
          {
            // ascending
            case HA_READ_BEFORE_KEY:
              if (! p.part_last)
                p.bound_type= NdbIndexScanOperation::BoundGE;
              else
                p.bound_type= NdbIndexScanOperation::BoundGT;
              break;
            case HA_READ_AFTER_KEY:     // weird
              p.bound_type= NdbIndexScanOperation::BoundGE;
              break;
            default:
              break;
            // descending strangely sets no end key
          }
        }

        if (p.bound_type == -1)
        {
          DBUG_PRINT("error", ("key %d unknown flag %d", j, p.key->flag));
          DBUG_ASSERT(FALSE);
          // Stop setting bounds but continue with what we have
          op->end_of_bound(range_no);
          DBUG_RETURN(0);
        }
      }
    }

    // Seen with e.g. b = 1 and c > 1
    if (part[0].bound_type == NdbIndexScanOperation::BoundLE &&
        part[1].bound_type == NdbIndexScanOperation::BoundGE &&
        memcmp(part[0].part_ptr, part[1].part_ptr, part_store_len) == 0)
    {
      DBUG_PRINT("info", ("replace LE/GE pair by EQ"));
      part[0].bound_type= NdbIndexScanOperation::BoundEQ;
      part[1].bound_type= -1;
    }
    // Not seen but was in previous version
    if (part[0].bound_type == NdbIndexScanOperation::BoundEQ &&
        part[1].bound_type == NdbIndexScanOperation::BoundGE &&
        memcmp(part[0].part_ptr, part[1].part_ptr, part_store_len) == 0)
    {
      DBUG_PRINT("info", ("remove GE from EQ/GE pair"));
      part[1].bound_type= -1;
    }

    for (j= 0; j <= 1; j++)
    {
      struct part_st &p= part[j];
      // Set bound if not done with this key
      if (p.key != NULL)
      {
        DBUG_PRINT("info", ("key %d:%d offset=%d length=%d last=%d bound=%d",
                            j, i, tot_len, part_len, p.part_last, p.bound_type));
        DBUG_DUMP("info", (const char*)p.part_ptr, part_store_len);

        // Set bound if not cancelled via type -1
        if (p.bound_type != -1)
        {
          const char* ptr= p.bound_ptr;
          char buf[256];
          shrink_varchar(field, ptr, buf);
          if (op->setBound(i, p.bound_type, ptr))
            ERR_RETURN(op->getNdbError());
        }
      }
    }

    tot_len+= part_store_len;
  }
  op->end_of_bound(range_no);
  DBUG_RETURN(0);
}

/*
  Start ordered index scan in NDB
*/

int ha_ndbcluster::ordered_index_scan(const key_range *start_key,
                                      const key_range *end_key,
                                      bool sorted, bool descending,
                                      byte* buf, part_id_range *part_spec)
{  
  int res;
  bool restart;
  NdbTransaction *trans= m_active_trans;
  NdbIndexScanOperation *op;

  DBUG_ENTER("ha_ndbcluster::ordered_index_scan");
  DBUG_PRINT("enter", ("index: %u, sorted: %d, descending: %d",
             active_index, sorted, descending));  
  DBUG_PRINT("enter", ("Starting new ordered scan on %s", m_tabname));
  m_write_op= FALSE;

  // Check that sorted seems to be initialised
  DBUG_ASSERT(sorted == 0 || sorted == 1);
  
  if (m_active_cursor == 0)
  {
    restart= FALSE;
    NdbOperation::LockMode lm=
      (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
   bool need_pk = (lm == NdbOperation::LM_Read);
    if (!(op= trans->getNdbIndexScanOperation(m_index[active_index].index, 
                                              m_table)) ||
        op->readTuples(lm, 0, parallelism, sorted, descending, false, need_pk))
      ERR_RETURN(trans->getNdbError());
    if (m_use_partition_function && part_spec != NULL &&
        part_spec->start_part == part_spec->end_part)
      op->setPartitionId(part_spec->start_part);
    m_active_cursor= op;
  } else {
    restart= TRUE;
    op= (NdbIndexScanOperation*)m_active_cursor;
    
    if (m_use_partition_function && part_spec != NULL &&
        part_spec->start_part == part_spec->end_part)
      op->setPartitionId(part_spec->start_part);
    DBUG_ASSERT(op->getSorted() == sorted);
    DBUG_ASSERT(op->getLockMode() == 
                (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type));
    if (op->reset_bounds(m_force_send))
      DBUG_RETURN(ndb_err(m_active_trans));
  }
  
  {
    const key_range *keys[2]= { start_key, end_key };
    res= set_bounds(op, active_index, false, keys);
    if (res)
      DBUG_RETURN(res);
  }

  if (!restart)
  {
    if (generate_scan_filter(m_cond_stack, op))
      DBUG_RETURN(ndb_err(trans));

    if ((res= define_read_attrs(buf, op)))
    {
      DBUG_RETURN(res);
    }
    
    // If table has user defined partitioning
    // and no primary key, we need to read the partition id
    // to support ORDER BY queries
    if (m_use_partition_function &&
        (table_share->primary_key == MAX_KEY) && 
        (get_ndb_partition_id(op)))
      ERR_RETURN(trans->getNdbError());
  }

  if (execute_no_commit(this,trans,false) != 0)
    DBUG_RETURN(ndb_err(trans));
  
  DBUG_RETURN(next_result(buf));
}

/*
  Start full table scan in NDB
 */

int ha_ndbcluster::full_table_scan(byte *buf)
{
  int res;
  NdbScanOperation *op;
  NdbTransaction *trans= m_active_trans;
  part_id_range part_spec;

  DBUG_ENTER("full_table_scan");  
  DBUG_PRINT("enter", ("Starting new scan on %s", m_tabname));
  m_write_op= FALSE;

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  bool need_pk = (lm == NdbOperation::LM_Read);
  if (!(op=trans->getNdbScanOperation(m_table)) ||
      op->readTuples(lm, 
		     (need_pk)?NdbScanOperation::SF_KeyInfo:0, 
		     parallelism))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= op;

  if (m_use_partition_function)
  {
    part_spec.start_part= 0;
    part_spec.end_part= m_part_info->get_tot_partitions() - 1;
    prune_partition_set(table, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part = %u, part_spec.end_part = %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part > part_spec.end_part)
    {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    else if (part_spec.start_part == part_spec.end_part)
    {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      m_active_cursor->setPartitionId(part_spec.start_part);
    }
    // If table has user defined partitioning
    // and no primary key, we need to read the partition id
    // to support ORDER BY queries
    if ((table_share->primary_key == MAX_KEY) && 
        (get_ndb_partition_id(op)))
      ERR_RETURN(trans->getNdbError());
  }

  if (generate_scan_filter(m_cond_stack, op))
    DBUG_RETURN(ndb_err(trans));
  if ((res= define_read_attrs(buf, op)))
    DBUG_RETURN(res);

  if (execute_no_commit(this,trans,false) != 0)
    DBUG_RETURN(ndb_err(trans));
  DBUG_PRINT("exit", ("Scan started successfully"));
  DBUG_RETURN(next_result(buf));
}

/*
  Insert one record into NDB
*/
int ha_ndbcluster::write_row(byte *record)
{
  bool has_auto_increment;
  uint i;
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  int res;
  THD *thd= current_thd;
  longlong func_value= 0;
  DBUG_ENTER("ha_ndbcluster::write_row");

  m_write_op= TRUE;
  has_auto_increment= (table->next_number_field && record == table->record[0]);
  if (table_share->primary_key != MAX_KEY)
  {
    /*
     * Increase any auto_incremented primary key
     */
    if (has_auto_increment) 
    {
      THD *thd= table->in_use;

      m_skip_auto_increment= FALSE;
      update_auto_increment();
      m_skip_auto_increment= (insert_id_for_cur_row == 0);
    }
  }

  /*
   * If IGNORE the ignore constraint violations on primary and unique keys
   */
  if (!m_use_write && m_ignore_dup_key)
  {
    /*
      compare if expression with that in start_bulk_insert()
      start_bulk_insert will set parameters to ensure that each
      write_row is committed individually
    */
    int peek_res= peek_indexed_rows(record);
    
    if (!peek_res) 
    {
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(peek_res);
  }

  statistic_increment(thd->status_var.ha_write_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  if (!(op= trans->getNdbOperation(m_table)))
    ERR_RETURN(trans->getNdbError());

  res= (m_use_write) ? op->writeTuple() :op->insertTuple(); 
  if (res != 0)
    ERR_RETURN(trans->getNdbError());  
 
  if (m_use_partition_function)
  {
    uint32 part_id;
    int error;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    error= m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (error)
      DBUG_RETURN(error);
    op->setPartitionId(part_id);
  }

  if (table_share->primary_key == MAX_KEY) 
  {
    // Table has hidden primary key
    Ndb *ndb= get_ndb();
    int ret;
    Uint64 auto_value;
    uint retries= NDB_AUTO_INCREMENT_RETRIES;
    do {
      Ndb_tuple_id_range_guard g(m_share);
      ret= ndb->getAutoIncrementValue(m_table, g.range, auto_value, 1);
    } while (ret == -1 && 
             --retries &&
             ndb->getNdbError().status == NdbError::TemporaryError);
    if (ret == -1)
      ERR_RETURN(ndb->getNdbError());
    if (set_hidden_key(op, table_share->fields, (const byte*)&auto_value))
      ERR_RETURN(op->getNdbError());
  } 
  else 
  {
    int error;
    if ((error= set_primary_key_from_record(op, record)))
      DBUG_RETURN(error);
  }

  // Set non-key attribute(s)
  bool set_blob_value= FALSE;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
  for (i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (!(field->flags & PRI_KEY_FLAG) &&
	(bitmap_is_set(table->write_set, i) || !m_use_write) &&
        set_ndb_value(op, field, i, record-table->record[0], &set_blob_value))
    {
      m_skip_auto_increment= TRUE;
      dbug_tmp_restore_column_map(table->read_set, old_map);
      ERR_RETURN(op->getNdbError());
    }
  }
  dbug_tmp_restore_column_map(table->read_set, old_map);

  if (m_use_partition_function)
  {
    /*
      We need to set the value of the partition function value in
      NDB since the NDB kernel doesn't have easy access to the function
      to calculate the value.
    */
    if (func_value >= INT_MAX32)
      func_value= INT_MAX32;
    uint32 part_func_value= (uint32)func_value;
    uint no_fields= table_share->fields;
    if (table_share->primary_key == MAX_KEY)
      no_fields++;
    op->setValue(no_fields, part_func_value);
  }

  m_rows_changed++;

  /*
    Execute write operation
    NOTE When doing inserts with many values in 
    each INSERT statement it should not be necessary
    to NoCommit the transaction between each row.
    Find out how this is detected!
  */
  m_rows_inserted++;
  no_uncommitted_rows_update(1);
  m_bulk_insert_not_flushed= TRUE;
  if ((m_rows_to_insert == (ha_rows) 1) || 
      ((m_rows_inserted % m_bulk_insert_rows) == 0) ||
      m_primary_key_update ||
      set_blob_value)
  {
    // Send rows to NDB
    DBUG_PRINT("info", ("Sending inserts to NDB, "\
                        "rows_inserted:%d, bulk_insert_rows: %d", 
                        (int)m_rows_inserted, (int)m_bulk_insert_rows));

    m_bulk_insert_not_flushed= FALSE;
    if (m_transaction_on)
    {
      if (execute_no_commit(this,trans,false) != 0)
      {
        m_skip_auto_increment= TRUE;
        no_uncommitted_rows_execute_failure();
        DBUG_RETURN(ndb_err(trans));
      }
    }
    else
    {
      if (execute_commit(this,trans) != 0)
      {
        m_skip_auto_increment= TRUE;
        no_uncommitted_rows_execute_failure();
        DBUG_RETURN(ndb_err(trans));
      }
      if (trans->restart() != 0)
      {
        DBUG_ASSERT(0);
        DBUG_RETURN(-1);
      }
    }
  }
  if ((has_auto_increment) && (m_skip_auto_increment))
  {
    Ndb *ndb= get_ndb();
    Uint64 next_val= (Uint64) table->next_number_field->val_int() + 1;
    char buff[22];
    DBUG_PRINT("info", 
               ("Trying to set next auto increment value to %s",
                llstr(next_val, buff)));
    Ndb_tuple_id_range_guard g(m_share);
    if (ndb->setAutoIncrementValue(m_table, g.range, next_val, TRUE)
        == -1)
      ERR_RETURN(ndb->getNdbError());
  }
  m_skip_auto_increment= TRUE;

  DBUG_PRINT("exit",("ok"));
  DBUG_RETURN(0);
}


/* Compare if a key in a row has changed */

int ha_ndbcluster::key_cmp(uint keynr, const byte * old_row,
                           const byte * new_row)
{
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for (; key_part != end ; key_part++)
  {
    if (key_part->null_bit)
    {
      if ((old_row[key_part->null_offset] & key_part->null_bit) !=
          (new_row[key_part->null_offset] & key_part->null_bit))
        return 1;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
    {

      if (key_part->field->cmp_binary((char*) (old_row + key_part->offset),
                                      (char*) (new_row + key_part->offset),
                                      (ulong) key_part->length))
        return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
                 key_part->length))
        return 1;
    }
  }
  return 0;
}

/*
  Update one record in NDB using primary key
*/

int ha_ndbcluster::update_row(const byte *old_data, byte *new_data)
{
  THD *thd= current_thd;
  NdbTransaction *trans= m_active_trans;
  NdbScanOperation* cursor= m_active_cursor;
  NdbOperation *op;
  uint i;
  uint32 old_part_id= 0, new_part_id= 0;
  int error;
  longlong func_value;
  DBUG_ENTER("update_row");
  m_write_op= TRUE;
  
  statistic_increment(thd->status_var.ha_update_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
  {
    table->timestamp_field->set_time();
    bitmap_set_bit(table->write_set, table->timestamp_field->field_index);
  }

  if (m_use_partition_function &&
      (error= get_parts_for_update(old_data, new_data, table->record[0],
                                   m_part_info, &old_part_id, &new_part_id,
                                   &func_value)))
  {
    DBUG_RETURN(error);
  }

  /*
   * Check for update of primary key or partition change
   * for special handling
   */  
  if (((table_share->primary_key != MAX_KEY) &&
       key_cmp(table_share->primary_key, old_data, new_data)) ||
      (old_part_id != new_part_id))
  {
    int read_res, insert_res, delete_res, undo_res;

    DBUG_PRINT("info", ("primary key update or partition change, "
                        "doing read+delete+insert"));
    // Get all old fields, since we optimize away fields not in query
    read_res= complemented_read(old_data, new_data, old_part_id);
    if (read_res)
    {
      DBUG_PRINT("info", ("read failed"));
      DBUG_RETURN(read_res);
    }
    // Delete old row
    m_primary_key_update= TRUE;
    delete_res= delete_row(old_data);
    m_primary_key_update= FALSE;
    if (delete_res)
    {
      DBUG_PRINT("info", ("delete failed"));
      DBUG_RETURN(delete_res);
    }     
    // Insert new row
    DBUG_PRINT("info", ("delete succeded"));
    m_primary_key_update= TRUE;
    insert_res= write_row(new_data);
    m_primary_key_update= FALSE;
    if (insert_res)
    {
      DBUG_PRINT("info", ("insert failed"));
      if (trans->commitStatus() == NdbConnection::Started)
      {
        // Undo delete_row(old_data)
        m_primary_key_update= TRUE;
        undo_res= write_row((byte *)old_data);
        if (undo_res)
          push_warning(current_thd, 
                       MYSQL_ERROR::WARN_LEVEL_WARN, 
                       undo_res, 
                       "NDB failed undoing delete at primary key update");
        m_primary_key_update= FALSE;
      }
      DBUG_RETURN(insert_res);
    }
    DBUG_PRINT("info", ("delete+insert succeeded"));
    DBUG_RETURN(0);
  }

  if (cursor)
  {
    /*
      We are scanning records and want to update the record
      that was just found, call updateTuple on the cursor 
      to take over the lock to a new update operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling updateTuple on cursor"));
    if (!(op= cursor->updateCurrentTuple()))
      ERR_RETURN(trans->getNdbError());
    m_lock_tuple= false;
    m_ops_pending++;
    if (uses_blob_value())
      m_blobs_pending= TRUE;
    if (m_use_partition_function)
      cursor->setPartitionId(new_part_id);
  }
  else
  {  
    if (!(op= trans->getNdbOperation(m_table)) ||
        op->updateTuple() != 0)
      ERR_RETURN(trans->getNdbError());  
    
    if (m_use_partition_function)
      op->setPartitionId(new_part_id);
    if (table_share->primary_key == MAX_KEY) 
    {
      // This table has no primary key, use "hidden" primary key
      DBUG_PRINT("info", ("Using hidden key"));
      
      // Require that the PK for this record has previously been 
      // read into m_ref
      DBUG_DUMP("key", m_ref, NDB_HIDDEN_PRIMARY_KEY_LENGTH);
      
      if (set_hidden_key(op, table->s->fields, m_ref))
        ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      int res;
      if ((res= set_primary_key_from_record(op, old_data)))
        DBUG_RETURN(res);
    }
  }

  m_rows_changed++;

  // Set non-key attribute(s)
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
  for (i= 0; i < table_share->fields; i++) 
  {
    Field *field= table->field[i];
    if (bitmap_is_set(table->write_set, i) &&
        (!(field->flags & PRI_KEY_FLAG)) &&
        set_ndb_value(op, field, i, new_data - table->record[0]))
    {
      dbug_tmp_restore_column_map(table->read_set, old_map);
      ERR_RETURN(op->getNdbError());
    }
  }
  dbug_tmp_restore_column_map(table->read_set, old_map);

  if (m_use_partition_function)
  {
    if (func_value >= INT_MAX32)
      func_value= INT_MAX32;
    uint32 part_func_value= (uint32)func_value;
    uint no_fields= table_share->fields;
    if (table_share->primary_key == MAX_KEY)
      no_fields++;
    op->setValue(no_fields, part_func_value);
  }
  // Execute update operation
  if (!cursor && execute_no_commit(this,trans,false) != 0) {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  
  DBUG_RETURN(0);
}


/*
  Delete one record from NDB, using primary key 
*/

int ha_ndbcluster::delete_row(const byte *record)
{
  THD *thd= current_thd;
  NdbTransaction *trans= m_active_trans;
  NdbScanOperation* cursor= m_active_cursor;
  NdbOperation *op;
  uint32 part_id;
  int error;
  DBUG_ENTER("delete_row");
  m_write_op= TRUE;

  statistic_increment(thd->status_var.ha_delete_count,&LOCK_status);
  m_rows_changed++;

  if (m_use_partition_function &&
      (error= get_part_for_delete(record, table->record[0], m_part_info,
                                  &part_id)))
  {
    DBUG_RETURN(error);
  }

  if (cursor)
  {
    /*
      We are scanning records and want to delete the record
      that was just found, call deleteTuple on the cursor 
      to take over the lock to a new delete operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling deleteTuple on cursor"));
    if (cursor->deleteCurrentTuple() != 0)
      ERR_RETURN(trans->getNdbError());     
    m_lock_tuple= false;
    m_ops_pending++;

    if (m_use_partition_function)
      cursor->setPartitionId(part_id);

    no_uncommitted_rows_update(-1);

    if (!m_primary_key_update)
      // If deleting from cursor, NoCommit will be handled in next_result
      DBUG_RETURN(0);
  }
  else
  {
    
    if (!(op=trans->getNdbOperation(m_table)) || 
        op->deleteTuple() != 0)
      ERR_RETURN(trans->getNdbError());
    
    if (m_use_partition_function)
      op->setPartitionId(part_id);

    no_uncommitted_rows_update(-1);
    
    if (table_share->primary_key == MAX_KEY) 
    {
      // This table has no primary key, use "hidden" primary key
      DBUG_PRINT("info", ("Using hidden key"));
      
      if (set_hidden_key(op, table->s->fields, m_ref))
        ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      if ((error= set_primary_key_from_record(op, record)))
        DBUG_RETURN(error);
    }
  }

  // Execute delete operation
  if (execute_no_commit(this,trans,false) != 0) {
    no_uncommitted_rows_execute_failure();
    DBUG_RETURN(ndb_err(trans));
  }
  DBUG_RETURN(0);
}
  
/*
  Unpack a record read from NDB 

  SYNOPSIS
    unpack_record()
    buf                 Buffer to store read row

  NOTE
    The data for each row is read directly into the
    destination buffer. This function is primarily 
    called in order to check if any fields should be 
    set to null.
*/

void ndb_unpack_record(TABLE *table, NdbValue *value,
                       MY_BITMAP *defined, byte *buf)
{
  Field **p_field= table->field, *field= *p_field;
  my_ptrdiff_t row_offset= (my_ptrdiff_t) (buf - table->record[0]);
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_ENTER("ndb_unpack_record");

  // Set null flag(s)
  bzero(buf, table->s->null_bytes);
  for ( ; field;
       p_field++, value++, field= *p_field)
  {
    if ((*value).ptr)
    {
      if (!(field->flags & BLOB_FLAG))
      {
        int is_null= (*value).rec->isNULL();
        if (is_null)
        {
          if (is_null > 0)
          {
            DBUG_PRINT("info",("[%u] NULL",
                               (*value).rec->getColumn()->getColumnNo()));
            field->set_null(row_offset);
          }
          else
          {
            DBUG_PRINT("info",("[%u] UNDEFINED",
                               (*value).rec->getColumn()->getColumnNo()));
            bitmap_clear_bit(defined,
                             (*value).rec->getColumn()->getColumnNo());
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
                                        FALSE);
          }
          else
          {
            DBUG_PRINT("info", ("bit field H'%.8X%.8X",
                                *(Uint32*) (*value).rec->aRef(),
                                *((Uint32*) (*value).rec->aRef()+1)));
            field_bit->Field_bit::store((longlong) (*value).rec->u_64_value(), 
                                        TRUE);
          }
          /*
            Move back internal field pointer to point to original
            value (usually record[0]).
           */
          field_bit->Field_bit::move_field_offset(-row_offset);
          DBUG_PRINT("info",("[%u] SET",
                             (*value).rec->getColumn()->getColumnNo()));
          DBUG_DUMP("info", (const char*) field->ptr, field->pack_length());
        }
        else
        {
          DBUG_PRINT("info",("[%u] SET",
                             (*value).rec->getColumn()->getColumnNo()));
          DBUG_DUMP("info", (const char*) field->ptr, field->pack_length());
        }
      }
      else
      {
        NdbBlob *ndb_blob= (*value).blob;
        uint col_no = ndb_blob->getColumn()->getColumnNo();
        int isNull;
        ndb_blob->getDefined(isNull);
        if (isNull == 1)
        {
          DBUG_PRINT("info",("[%u] NULL", col_no));
          field->set_null(row_offset);
        }
        else if (isNull == -1)
        {
          DBUG_PRINT("info",("[%u] UNDEFINED", col_no));
          bitmap_clear_bit(defined, col_no);
        }
        else
        {
#ifndef DBUG_OFF
          // pointer vas set in get_ndb_blobs_value
          Field_blob *field_blob= (Field_blob*)field;
          char* ptr;
          field_blob->get_ptr(&ptr, row_offset);
          uint32 len= field_blob->get_length(row_offset);
          DBUG_PRINT("info",("[%u] SET ptr=%p len=%u", col_no, ptr, len));
#endif
        }
      }
    }
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::unpack_record(byte *buf)
{
  ndb_unpack_record(table, m_value, 0, buf);
#ifndef DBUG_OFF
  // Read and print all values that was fetched
  if (table_share->primary_key == MAX_KEY)
  {
    // Table with hidden primary key
    int hidden_no= table_share->fields;
    const NDBTAB *tab= m_table;
    char buff[22];
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    const NdbRecAttr* rec= m_value[hidden_no].rec;
    DBUG_ASSERT(rec);
    DBUG_PRINT("hidden", ("%d: %s \"%s\"", hidden_no,
			  hidden_col->getName(),
                          llstr(rec->u_64_value(), buff)));
  }
  //DBUG_EXECUTE("value", print_results(););
#endif
}

/*
  Utility function to print/dump the fetched field
  to avoid unnecessary work, wrap in DBUG_EXECUTE as in:

    DBUG_EXECUTE("value", print_results(););
 */

void ha_ndbcluster::print_results()
{
  DBUG_ENTER("print_results");

#ifndef DBUG_OFF

  char buf_type[MAX_FIELD_WIDTH], buf_val[MAX_FIELD_WIDTH];
  String type(buf_type, sizeof(buf_type), &my_charset_bin);
  String val(buf_val, sizeof(buf_val), &my_charset_bin);
  for (uint f= 0; f < table_share->fields; f++)
  {
    /* Use DBUG_PRINT since DBUG_FILE cannot be filtered out */
    char buf[2000];
    Field *field;
    void* ptr;
    NdbValue value;

    buf[0]= 0;
    field= table->field[f];
    if (!(value= m_value[f]).ptr)
    {
      strmov(buf, "not read");
      goto print_value;
    }

    ptr= field->ptr;

    if (! (field->flags & BLOB_FLAG))
    {
      if (value.rec->isNULL())
      {
        strmov(buf, "NULL");
        goto print_value;
      }
      type.length(0);
      val.length(0);
      field->sql_type(type);
      field->val_str(&val);
      my_snprintf(buf, sizeof(buf), "%s %s", type.c_ptr(), val.c_ptr());
    }
    else
    {
      NdbBlob *ndb_blob= value.blob;
      bool isNull= TRUE;
      ndb_blob->getNull(isNull);
      if (isNull)
        strmov(buf, "NULL");
    }

print_value:
    DBUG_PRINT("value", ("%u,%s: %s", f, field->field_name, buf));
  }
#endif
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::index_init(uint index, bool sorted)
{
  DBUG_ENTER("ha_ndbcluster::index_init");
  DBUG_PRINT("enter", ("index: %u  sorted: %d", index, sorted));
  active_index= index;
  m_sorted= sorted;
  /*
    Locks are are explicitly released in scan
    unless m_lock.type == TL_READ_HIGH_PRIORITY
    and no sub-sequent call to unlock_row()
  */
  m_lock_tuple= false;
    m_lock_tuple= false;
  DBUG_RETURN(0);
}


int ha_ndbcluster::index_end()
{
  DBUG_ENTER("ha_ndbcluster::index_end");
  DBUG_RETURN(close_scan());
}

/**
 * Check if key contains null
 */
static
int
check_null_in_key(const KEY* key_info, const byte *key, uint key_len)
{
  KEY_PART_INFO *curr_part, *end_part;
  const byte* end_ptr= key + key_len;
  curr_part= key_info->key_part;
  end_part= curr_part + key_info->key_parts;
  

  for (; curr_part != end_part && key < end_ptr; curr_part++)
  {
    if (curr_part->null_bit && *key)
      return 1;

    key += curr_part->store_length;
  }
  return 0;
}

int ha_ndbcluster::index_read(byte *buf,
                              const byte *key, uint key_len, 
                              enum ha_rkey_function find_flag)
{
  key_range start_key;
  bool descending= FALSE;
  DBUG_ENTER("ha_ndbcluster::index_read");
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d", 
                       active_index, key_len, find_flag));

  start_key.key= key;
  start_key.length= key_len;
  start_key.flag= find_flag;
  descending= FALSE;
  switch (find_flag) {
  case HA_READ_KEY_OR_PREV:
  case HA_READ_BEFORE_KEY:
  case HA_READ_PREFIX_LAST:
  case HA_READ_PREFIX_LAST_OR_PREV:
    descending= TRUE;
    break;
  default:
    break;
  }
  DBUG_RETURN(read_range_first_to_buf(&start_key, 0, descending,
                                      m_sorted, buf));
}


int ha_ndbcluster::index_read_idx(byte *buf, uint index_no, 
                              const byte *key, uint key_len, 
                              enum ha_rkey_function find_flag)
{
  statistic_increment(current_thd->status_var.ha_read_key_count, &LOCK_status);
  DBUG_ENTER("ha_ndbcluster::index_read_idx");
  DBUG_PRINT("enter", ("index_no: %u, key_len: %u", index_no, key_len));  
  close_scan();
  index_init(index_no, 0);  
  DBUG_RETURN(index_read(buf, key, key_len, find_flag));
}


int ha_ndbcluster::index_next(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_next");
  statistic_increment(current_thd->status_var.ha_read_next_count,
                      &LOCK_status);
  DBUG_RETURN(next_result(buf));
}


int ha_ndbcluster::index_prev(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_prev");
  statistic_increment(current_thd->status_var.ha_read_prev_count,
                      &LOCK_status);
  DBUG_RETURN(next_result(buf));
}


int ha_ndbcluster::index_first(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_first");
  statistic_increment(current_thd->status_var.ha_read_first_count,
                      &LOCK_status);
  // Start the ordered index scan and fetch the first row

  // Only HA_READ_ORDER indexes get called by index_first
  DBUG_RETURN(ordered_index_scan(0, 0, TRUE, FALSE, buf, NULL));
}


int ha_ndbcluster::index_last(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_last");
  statistic_increment(current_thd->status_var.ha_read_last_count,&LOCK_status);
  DBUG_RETURN(ordered_index_scan(0, 0, TRUE, TRUE, buf, NULL));
}

int ha_ndbcluster::index_read_last(byte * buf, const byte * key, uint key_len)
{
  DBUG_ENTER("ha_ndbcluster::index_read_last");
  DBUG_RETURN(index_read(buf, key, key_len, HA_READ_PREFIX_LAST));
}

int ha_ndbcluster::read_range_first_to_buf(const key_range *start_key,
                                           const key_range *end_key,
                                           bool desc, bool sorted,
                                           byte* buf)
{
  part_id_range part_spec;
  ndb_index_type type= get_index_type(active_index);
  const KEY* key_info= table->key_info+active_index;
  int error; 
  DBUG_ENTER("ha_ndbcluster::read_range_first_to_buf");
  DBUG_PRINT("info", ("desc: %d, sorted: %d", desc, sorted));

  if (m_use_partition_function)
  {
    get_partition_set(table, buf, active_index, start_key, &part_spec);
    DBUG_PRINT("info", ("part_spec.start_part = %u, part_spec.end_part = %u",
                        part_spec.start_part, part_spec.end_part));
    /*
      If partition pruning has found no partition in set
      we can return HA_ERR_END_OF_FILE
      If partition pruning has found exactly one partition in set
      we can optimize scan to run towards that partition only.
    */
    if (part_spec.start_part > part_spec.end_part)
    {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    else if (part_spec.start_part == part_spec.end_part)
    {
      /*
        Only one partition is required to scan, if sorted is required we
        don't need it any more since output from one ordered partitioned
        index is always sorted.
      */
      sorted= FALSE;
    }
  }

  m_write_op= FALSE;
  switch (type){
  case PRIMARY_KEY_ORDERED_INDEX:
  case PRIMARY_KEY_INDEX:
    if (start_key && 
        start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT)
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      DBUG_RETURN(pk_read(start_key->key, start_key->length, buf,
                          part_spec.start_part));
    }
    break;
  case UNIQUE_ORDERED_INDEX:
  case UNIQUE_INDEX:
    if (start_key && start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT && 
        !check_null_in_key(key_info, start_key->key, start_key->length))
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      DBUG_RETURN(unique_index_read(start_key->key, start_key->length, buf));
    }
    break;
  default:
    break;
  }
  // Start the ordered index scan and fetch the first row
  DBUG_RETURN(ordered_index_scan(start_key, end_key, sorted, desc, buf,
                                 &part_spec));
}

int ha_ndbcluster::read_range_first(const key_range *start_key,
                                    const key_range *end_key,
                                    bool eq_r, bool sorted)
{
  byte* buf= table->record[0];
  DBUG_ENTER("ha_ndbcluster::read_range_first");
  DBUG_RETURN(read_range_first_to_buf(start_key, end_key, FALSE,
                                      sorted, buf));
}

int ha_ndbcluster::read_range_next()
{
  DBUG_ENTER("ha_ndbcluster::read_range_next");
  DBUG_RETURN(next_result(table->record[0]));
}


int ha_ndbcluster::rnd_init(bool scan)
{
  NdbScanOperation *cursor= m_active_cursor;
  DBUG_ENTER("rnd_init");
  DBUG_PRINT("enter", ("scan: %d", scan));
  // Check if scan is to be restarted
  if (cursor)
  {
    if (!scan)
      DBUG_RETURN(1);
    if (cursor->restart(m_force_send) != 0)
    {
      DBUG_ASSERT(0);
      DBUG_RETURN(-1);
    }
  }
  index_init(table_share->primary_key, 0);
  DBUG_RETURN(0);
}

int ha_ndbcluster::close_scan()
{
  NdbTransaction *trans= m_active_trans;
  DBUG_ENTER("close_scan");

  m_multi_cursor= 0;
  if (!m_active_cursor && !m_multi_cursor)
    DBUG_RETURN(1);

  NdbScanOperation *cursor= m_active_cursor ? m_active_cursor : m_multi_cursor;
  
  if (m_lock_tuple)
  {
    /*
      Lock level m_lock.type either TL_WRITE_ALLOW_WRITE
      (SELECT FOR UPDATE) or TL_READ_WITH_SHARED_LOCKS (SELECT
      LOCK WITH SHARE MODE) and row was not explictly unlocked 
      with unlock_row() call
    */
      NdbOperation *op;
      // Lock row
      DBUG_PRINT("info", ("Keeping lock on scanned row"));
      
      if (!(op= cursor->lockCurrentTuple()))
      {
	m_lock_tuple= false;
	ERR_RETURN(trans->getNdbError());
      }
      m_ops_pending++;      
  }
  m_lock_tuple= false;
  if (m_ops_pending)
  {
    /*
      Take over any pending transactions to the 
      deleteing/updating transaction before closing the scan    
    */
    DBUG_PRINT("info", ("ops_pending: %d", m_ops_pending));    
    if (execute_no_commit(this,trans,false) != 0) {
      no_uncommitted_rows_execute_failure();
      DBUG_RETURN(ndb_err(trans));
    }
    m_ops_pending= 0;
  }
  
  cursor->close(m_force_send, TRUE);
  m_active_cursor= m_multi_cursor= NULL;
  DBUG_RETURN(0);
}

int ha_ndbcluster::rnd_end()
{
  DBUG_ENTER("rnd_end");
  DBUG_RETURN(close_scan());
}


int ha_ndbcluster::rnd_next(byte *buf)
{
  DBUG_ENTER("rnd_next");
  statistic_increment(current_thd->status_var.ha_read_rnd_next_count,
                      &LOCK_status);

  if (!m_active_cursor)
    DBUG_RETURN(full_table_scan(buf));
  DBUG_RETURN(next_result(buf));
}


/*
  An "interesting" record has been found and it's pk 
  retrieved by calling position
  Now it's time to read the record from db once 
  again
*/

int ha_ndbcluster::rnd_pos(byte *buf, byte *pos)
{
  DBUG_ENTER("rnd_pos");
  statistic_increment(current_thd->status_var.ha_read_rnd_count,
                      &LOCK_status);
  // The primary key for the record is stored in pos
  // Perform a pk_read using primary key "index"
  {
    part_id_range part_spec;
    uint key_length= ref_length;
    if (m_use_partition_function)
    {
      if (table_share->primary_key == MAX_KEY)
      {
        /*
          The partition id has been fetched from ndb
          and has been stored directly after the hidden key
        */
        DBUG_DUMP("key+part", (char *)pos, key_length);
        key_length= ref_length - sizeof(m_part_id);
        part_spec.start_part= part_spec.end_part= *(uint32 *)(pos + key_length);
      }
      else
      {
        key_range key_spec;
        KEY *key_info= table->key_info + table_share->primary_key;
        key_spec.key= pos;
        key_spec.length= key_length;
        key_spec.flag= HA_READ_KEY_EXACT;
        get_full_part_id_from_key(table, buf, key_info, 
                                  &key_spec, &part_spec);
        DBUG_ASSERT(part_spec.start_part == part_spec.end_part);
      }
      DBUG_PRINT("info", ("partition id %u", part_spec.start_part));
    }
    DBUG_DUMP("key", (char *)pos, key_length);
    DBUG_RETURN(pk_read(pos, key_length, buf, part_spec.start_part));
  }
}


/*
  Store the primary key of this record in ref 
  variable, so that the row can be retrieved again later
  using "reference" in rnd_pos
*/

void ha_ndbcluster::position(const byte *record)
{
  KEY *key_info;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  byte *buff;
  uint key_length;

  DBUG_ENTER("position");

  if (table_share->primary_key != MAX_KEY) 
  {
    key_length= ref_length;
    key_info= table->key_info + table_share->primary_key;
    key_part= key_info->key_part;
    end= key_part + key_info->key_parts;
    buff= ref;
    
    for (; key_part != end; key_part++) 
    {
      if (key_part->null_bit) {
        /* Store 0 if the key part is a NULL part */      
        if (record[key_part->null_offset]
            & key_part->null_bit) {
          *buff++= 1;
          continue;
        }      
        *buff++= 0;
      }

      size_t len = key_part->length;
      const byte * ptr = record + key_part->offset;
      Field *field = key_part->field;
      if ((field->type() ==  MYSQL_TYPE_VARCHAR) &&
	  ((Field_varstring*)field)->length_bytes == 1)
      {
	/** 
	 * Keys always use 2 bytes length
	 */
	buff[0] = ptr[0];
	buff[1] = 0;
	memcpy(buff+2, ptr + 1, len);	
	len += 2;
      }
      else
      {
	memcpy(buff, ptr, len);
      }
      buff += len;
    }
  } 
  else 
  {
    // No primary key, get hidden key
    DBUG_PRINT("info", ("Getting hidden key"));
    // If table has user defined partition save the partition id as well
    if(m_use_partition_function)
    {
      DBUG_PRINT("info", ("Saving partition id %u", m_part_id));
      key_length= ref_length - sizeof(m_part_id);
      memcpy(ref+key_length, (void *)&m_part_id, sizeof(m_part_id));
    }
    else
      key_length= ref_length;
#ifndef DBUG_OFF
    int hidden_no= table->s->fields;
    const NDBTAB *tab= m_table;  
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    DBUG_ASSERT(hidden_col->getPrimaryKey() && 
                hidden_col->getAutoIncrement() &&
                key_length == NDB_HIDDEN_PRIMARY_KEY_LENGTH);
#endif
    memcpy(ref, m_ref, key_length);
  }
#ifndef DBUG_OFF
  if (table_share->primary_key == MAX_KEY && m_use_partition_function) 
    DBUG_DUMP("key+part", (char*)ref, key_length+sizeof(m_part_id));
#endif
  DBUG_DUMP("ref", (char*)ref, key_length);
  DBUG_VOID_RETURN;
}


void ha_ndbcluster::info(uint flag)
{
  DBUG_ENTER("info");
  DBUG_PRINT("enter", ("flag: %d", flag));
  
  if (flag & HA_STATUS_POS)
    DBUG_PRINT("info", ("HA_STATUS_POS"));
  if (flag & HA_STATUS_NO_LOCK)
    DBUG_PRINT("info", ("HA_STATUS_NO_LOCK"));
  if (flag & HA_STATUS_TIME)
    DBUG_PRINT("info", ("HA_STATUS_TIME"));
  if (flag & HA_STATUS_VARIABLE)
  {
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));
    if (m_table_info)
    {
      if (m_ha_not_exact_count)
        stats.records= 100;
      else
        records_update();
    }
    else
    {
      if ((my_errno= check_ndb_connection()))
        DBUG_VOID_RETURN;
      Ndb *ndb= get_ndb();
      ndb->setDatabaseName(m_dbname);
      struct Ndb_statistics stat;
      ndb->setDatabaseName(m_dbname);
      if (current_thd->variables.ndb_use_exact_count &&
          ndb_get_table_statistics(ndb, m_table, &stat) == 0)
      {
        stats.mean_rec_length= stat.row_size;
        stats.data_file_length= stat.fragment_memory;
        stats.records= stat.row_count;
      }
      else
      {
        stats.mean_rec_length= 0;
        stats.records= 100;
      }
    }
  }
  if (flag & HA_STATUS_CONST)
  {
    DBUG_PRINT("info", ("HA_STATUS_CONST"));
    set_rec_per_key();
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    DBUG_PRINT("info", ("HA_STATUS_ERRKEY"));
    errkey= m_dupkey;
  }
  if (flag & HA_STATUS_AUTO)
  {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    if (m_table)
    {
      Ndb *ndb= get_ndb();
      Ndb_tuple_id_range_guard g(m_share);
      
      Uint64 auto_increment_value64;
      if (ndb->readAutoIncrementValue(m_table, g.range,
                                      auto_increment_value64) == -1)
      {
        const NdbError err= ndb->getNdbError();
        sql_print_error("Error %lu in readAutoIncrementValue(): %s",
                        (ulong) err.code, err.message);
        stats.auto_increment_value= ~(ulonglong)0;
      }
      else
        stats.auto_increment_value= (ulonglong)auto_increment_value64;
    }
  }
  DBUG_VOID_RETURN;
}


void ha_ndbcluster::get_dynamic_partition_info(PARTITION_INFO *stat_info,
                                               uint part_id)
{
  /* 
     This functions should be fixed. Suggested fix: to
     implement ndb function which retrives the statistics
     about ndb partitions.
  */
  bzero((char*) stat_info, sizeof(PARTITION_INFO));
  return;
}


int ha_ndbcluster::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("extra");
  switch (operation) {
  case HA_EXTRA_IGNORE_DUP_KEY:       /* Dup keys don't rollback everything*/
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_DUP_KEY"));
    DBUG_PRINT("info", ("Ignoring duplicate key"));
    m_ignore_dup_key= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
    m_ignore_dup_key= FALSE;
    break;
  case HA_EXTRA_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_NO_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_NO_KEY"));
    DBUG_PRINT("info", ("Turning on AO_IgnoreError at Commit/NoCommit"));
    m_ignore_no_key= FALSE;
    break;
  case HA_EXTRA_WRITE_CAN_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CAN_REPLACE"));
    if (!m_has_unique_index)
    {
      DBUG_PRINT("info", ("Turning ON use of write instead of insert"));
      m_use_write= TRUE;
    }
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CANNOT_REPLACE"));
    DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
    m_use_write= FALSE;
    break;
  default:
    break;
  }
  
  DBUG_RETURN(0);
}


int ha_ndbcluster::reset()
{
  DBUG_ENTER("ha_ndbcluster::reset");
  cond_clear();
  /*
    Regular partition pruning will set the bitmap appropriately.
    Some queries like ALTER TABLE doesn't use partition pruning and
    thus the 'used_partitions' bitmap needs to be initialized
  */
  if (m_part_info)
    bitmap_set_all(&m_part_info->used_partitions);
  DBUG_RETURN(0);
}


/* 
   Start of an insert, remember number of rows to be inserted, it will
   be used in write_row and get_autoincrement to send an optimal number
   of rows in each roundtrip to the server

   SYNOPSIS
   rows     number of rows to insert, 0 if unknown

*/

void ha_ndbcluster::start_bulk_insert(ha_rows rows)
{
  int bytes, batch;
  const NDBTAB *tab= m_table;    

  DBUG_ENTER("start_bulk_insert");
  DBUG_PRINT("enter", ("rows: %d", (int)rows));
  
  m_rows_inserted= (ha_rows) 0;
  if (!m_use_write && m_ignore_dup_key)
  {
    /*
      compare if expression with that in write_row
      we have a situation where peek_indexed_rows() will be called
      so we cannot batch
    */
    DBUG_PRINT("info", ("Batching turned off as duplicate key is "
                        "ignored by using peek_row"));
    m_rows_to_insert= 1;
    m_bulk_insert_rows= 1;
    DBUG_VOID_RETURN;
  }
  if (rows == (ha_rows) 0)
  {
    /* We don't know how many will be inserted, guess */
    m_rows_to_insert= m_autoincrement_prefetch;
  }
  else
    m_rows_to_insert= rows; 

  /* 
    Calculate how many rows that should be inserted
    per roundtrip to NDB. This is done in order to minimize the 
    number of roundtrips as much as possible. However performance will 
    degrade if too many bytes are inserted, thus it's limited by this 
    calculation.   
  */
  const int bytesperbatch= 8192;
  bytes= 12 + tab->getRowSizeInBytes() + 4 * tab->getNoOfColumns();
  batch= bytesperbatch/bytes;
  batch= batch == 0 ? 1 : batch;
  DBUG_PRINT("info", ("batch: %d, bytes: %d", batch, bytes));
  m_bulk_insert_rows= batch;

  DBUG_VOID_RETURN;
}

/*
  End of an insert
 */
int ha_ndbcluster::end_bulk_insert()
{
  int error= 0;

  DBUG_ENTER("end_bulk_insert");
  // Check if last inserts need to be flushed
  if (m_bulk_insert_not_flushed)
  {
    NdbTransaction *trans= m_active_trans;
    // Send rows to NDB
    DBUG_PRINT("info", ("Sending inserts to NDB, "\
                        "rows_inserted:%d, bulk_insert_rows: %d", 
                        (int) m_rows_inserted, (int) m_bulk_insert_rows)); 
    m_bulk_insert_not_flushed= FALSE;
    if (m_transaction_on)
    {
      if (execute_no_commit(this, trans,false) != 0)
      {
        no_uncommitted_rows_execute_failure();
        my_errno= error= ndb_err(trans);
      }
    }
    else
    {
      if (execute_commit(this, trans) != 0)
      {
        no_uncommitted_rows_execute_failure();
        my_errno= error= ndb_err(trans);
      }
      else
      {
        int res= trans->restart();
        DBUG_ASSERT(res == 0);
      }
    }
  }

  m_rows_inserted= (ha_rows) 0;
  m_rows_to_insert= (ha_rows) 1;
  DBUG_RETURN(error);
}


int ha_ndbcluster::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  DBUG_ENTER("extra_opt");
  DBUG_PRINT("enter", ("cache_size: %lu", cache_size));
  DBUG_RETURN(extra(operation));
}

static const char *ha_ndbcluster_exts[] = {
 ha_ndb_ext,
 NullS
};

const char** ha_ndbcluster::bas_ext() const
{
  return ha_ndbcluster_exts;
}

/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_ndbcluster::scan_time()
{
  DBUG_ENTER("ha_ndbcluster::scan_time()");
  double res= rows2double(stats.records*1000);
  DBUG_PRINT("exit", ("table: %s value: %f", 
                      m_tabname, res));
  DBUG_RETURN(res);
}

/*
  Convert MySQL table locks into locks supported by Ndb Cluster.
  Note that MySQL Cluster does currently not support distributed
  table locks, so to be safe one should set cluster in Single
  User Mode, before relying on table locks when updating tables
  from several MySQL servers
*/

THR_LOCK_DATA **ha_ndbcluster::store_lock(THD *thd,
                                          THR_LOCK_DATA **to,
                                          enum thr_lock_type lock_type)
{
  DBUG_ENTER("store_lock");
  if (lock_type != TL_IGNORE && m_lock.type == TL_UNLOCK) 
  {

    /* If we are not doing a LOCK TABLE, then allow multiple
       writers */
    
    /* Since NDB does not currently have table locks
       this is treated as a ordinary lock */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables)      
      lock_type= TL_WRITE_ALLOW_WRITE;
    
    /* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
       MySQL would use the lock TL_READ_NO_INSERT on t2, and that
       would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
       to t2. Convert the lock to a normal read lock to allow
       concurrent inserts to t2. */
    
    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type= TL_READ;
    
    m_lock.type=lock_type;
  }
  *to++= &m_lock;

  DBUG_PRINT("exit", ("lock_type: %d", lock_type));
  
  DBUG_RETURN(to);
}

#ifndef DBUG_OFF
#define PRINT_OPTION_FLAGS(t) { \
      if (t->options & OPTION_NOT_AUTOCOMMIT) \
        DBUG_PRINT("thd->options", ("OPTION_NOT_AUTOCOMMIT")); \
      if (t->options & OPTION_BEGIN) \
        DBUG_PRINT("thd->options", ("OPTION_BEGIN")); \
      if (t->options & OPTION_TABLE_LOCK) \
        DBUG_PRINT("thd->options", ("OPTION_TABLE_LOCK")); \
}
#else
#define PRINT_OPTION_FLAGS(t)
#endif


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement, this will be stored in thd_ndb.stmt.
  If not, we have to start a master transaction if there doesn't exist
  one from before, this will be stored in thd_ndb.all
 
  When a table lock is held one transaction will be started which holds
  the table lock and for each statement a hupp transaction will be started  
  If we are locking the table then:
  - save the NdbDictionary::Table for easy access
  - save reference to table statistics
  - refresh list of the indexes for the table if needed (if altered)
 */

int ha_ndbcluster::external_lock(THD *thd, int lock_type)
{
  int error=0;
  NdbTransaction* trans= NULL;
  DBUG_ENTER("external_lock");

  /*
    Check that this handler instance has a connection
    set up to the Ndb object of thd
   */
  if (check_ndb_connection(thd))
    DBUG_RETURN(1);

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;

  DBUG_PRINT("enter", ("this: 0x%lx  thd: 0x%lx  thd_ndb: %lx  "
                       "thd_ndb->lock_count: %d",
                       (long) this, (long) thd, (long) thd_ndb,
                       thd_ndb->lock_count));

  if (lock_type != F_UNLCK)
  {
    DBUG_PRINT("info", ("lock_type != F_UNLCK"));
    if (!thd->transaction.on)
      m_transaction_on= FALSE;
    else
      m_transaction_on= thd->variables.ndb_use_transactions;
    if (!thd_ndb->lock_count++)
    {
      PRINT_OPTION_FLAGS(thd);
      if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) 
      {
        // Autocommit transaction
        DBUG_ASSERT(!thd_ndb->stmt);
        DBUG_PRINT("trans",("Starting transaction stmt"));      

        trans= ndb->startTransaction();
        if (trans == NULL)
          ERR_RETURN(ndb->getNdbError());
        thd_ndb->init_open_tables();
        thd_ndb->stmt= trans;
	thd_ndb->query_state&= NDB_QUERY_NORMAL;
        trans_register_ha(thd, FALSE, &ndbcluster_hton);
      } 
      else 
      { 
        if (!thd_ndb->all)
        {
          // Not autocommit transaction
          // A "master" transaction ha not been started yet
          DBUG_PRINT("trans",("starting transaction, all"));
          
          trans= ndb->startTransaction();
          if (trans == NULL)
            ERR_RETURN(ndb->getNdbError());
          thd_ndb->init_open_tables();
          thd_ndb->all= trans; 
	  thd_ndb->query_state&= NDB_QUERY_NORMAL;
          trans_register_ha(thd, TRUE, &ndbcluster_hton);

          /*
            If this is the start of a LOCK TABLE, a table look 
            should be taken on the table in NDB
           
            Check if it should be read or write lock
           */
          if (thd->options & (OPTION_TABLE_LOCK))
          {
            //lockThisTable();
            DBUG_PRINT("info", ("Locking the table..." ));
          }

        }
      }
    }
    /*
      This is the place to make sure this handler instance
      has a started transaction.
     
      The transaction is started by the first handler on which 
      MySQL Server calls external lock
     
      Other handlers in the same stmt or transaction should use 
      the same NDB transaction. This is done by setting up the m_active_trans
      pointer to point to the NDB transaction. 
     */

    // store thread specific data first to set the right context
    m_force_send=          thd->variables.ndb_force_send;
    m_ha_not_exact_count= !thd->variables.ndb_use_exact_count;
    m_autoincrement_prefetch= 
      (ha_rows) thd->variables.ndb_autoincrement_prefetch_sz;

    m_active_trans= thd_ndb->all ? thd_ndb->all : thd_ndb->stmt;
    DBUG_ASSERT(m_active_trans);
    // Start of transaction
    m_rows_changed= 0;
    m_ops_pending= 0;

    // TODO remove double pointers...
    m_thd_ndb_share= thd_ndb->get_open_table(thd, m_table);
    m_table_info= &m_thd_ndb_share->stat;
  }
  else
  {
    DBUG_PRINT("info", ("lock_type == F_UNLCK"));

    if (ndb_cache_check_time && m_rows_changed)
    {
      DBUG_PRINT("info", ("Rows has changed and util thread is running"));
      if (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
      {
        DBUG_PRINT("info", ("Add share to list of tables to be invalidated"));
        /* NOTE push_back allocates memory using transactions mem_root! */
        thd_ndb->changed_tables.push_back(m_share, &thd->transaction.mem_root);
      }

      pthread_mutex_lock(&m_share->mutex);
      DBUG_PRINT("info", ("Invalidating commit_count"));
      m_share->commit_count= 0;
      m_share->commit_count_lock++;
      pthread_mutex_unlock(&m_share->mutex);
    }

    if (!--thd_ndb->lock_count)
    {
      DBUG_PRINT("trans", ("Last external_lock"));
      PRINT_OPTION_FLAGS(thd);

      if (thd_ndb->stmt)
      {
        /*
          Unlock is done without a transaction commit / rollback.
          This happens if the thread didn't update any rows
          We must in this case close the transaction to release resources
        */
        DBUG_PRINT("trans",("ending non-updating transaction"));
        ndb->closeTransaction(m_active_trans);
        thd_ndb->stmt= NULL;
      }
    }
    m_table_info= NULL;

    /*
      This is the place to make sure this handler instance
      no longer are connected to the active transaction.

      And since the handler is no longer part of the transaction 
      it can't have open cursors, ops or blobs pending.
    */
    m_active_trans= NULL;    

    if (m_active_cursor)
      DBUG_PRINT("warning", ("m_active_cursor != NULL"));
    m_active_cursor= NULL;

    if (m_multi_cursor)
      DBUG_PRINT("warning", ("m_multi_cursor != NULL"));
    m_multi_cursor= NULL;
    
    if (m_blobs_pending)
      DBUG_PRINT("warning", ("blobs_pending != 0"));
    m_blobs_pending= 0;
    
    if (m_ops_pending)
      DBUG_PRINT("warning", ("ops_pending != 0L"));
    m_ops_pending= 0;
  }
  thd->set_current_stmt_binlog_row_based_if_mixed();
  DBUG_RETURN(error);
}

/*
  Unlock the last row read in an open scan.
  Rows are unlocked by default in ndb, but
  for SELECT FOR UPDATE and SELECT LOCK WIT SHARE MODE
  locks are kept if unlock_row() is not called.
*/

void ha_ndbcluster::unlock_row() 
{
  DBUG_ENTER("unlock_row");

  DBUG_PRINT("info", ("Unlocking row"));
  m_lock_tuple= false;
  DBUG_VOID_RETURN;
}

/*
  Start a transaction for running a statement if one is not
  already running in a transaction. This will be the case in
  a BEGIN; COMMIT; block
  When using LOCK TABLE's external_lock will start a transaction
  since ndb does not currently does not support table locking
*/

int ha_ndbcluster::start_stmt(THD *thd, thr_lock_type lock_type)
{
  int error=0;
  DBUG_ENTER("start_stmt");
  PRINT_OPTION_FLAGS(thd);

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbTransaction *trans= (thd_ndb->stmt)?thd_ndb->stmt:thd_ndb->all;
  if (!trans){
    Ndb *ndb= thd_ndb->ndb;
    DBUG_PRINT("trans",("Starting transaction stmt"));  
    trans= ndb->startTransaction();
    if (trans == NULL)
      ERR_RETURN(ndb->getNdbError());
    no_uncommitted_rows_reset(thd);
    thd_ndb->stmt= trans;
    trans_register_ha(thd, FALSE, &ndbcluster_hton);
  }
  thd_ndb->query_state&= NDB_QUERY_NORMAL;
  m_active_trans= trans;

  // Start of statement
  m_ops_pending= 0;    
  thd->set_current_stmt_binlog_row_based_if_mixed();

  DBUG_RETURN(error);
}


/*
  Commit a transaction started in NDB
 */

static int ndbcluster_commit(THD *thd, bool all)
{
  int res= 0;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NdbTransaction *trans= all ? thd_ndb->all : thd_ndb->stmt;

  DBUG_ENTER("ndbcluster_commit");
  DBUG_PRINT("transaction",("%s",
                            trans == thd_ndb->stmt ?
                            "stmt" : "all"));
  DBUG_ASSERT(ndb && trans);

  if (execute_commit(thd,trans) != 0)
  {
    const NdbError err= trans->getNdbError();
    const NdbOperation *error_op= trans->getNdbErrorOperation();
    ERR_PRINT(err);
    res= ndb_to_mysql_error(&err);
    if (res != -1)
      ndbcluster_print_error(res, error_op);
  }
  ndb->closeTransaction(trans);

  if (all)
    thd_ndb->all= NULL;
  else
    thd_ndb->stmt= NULL;

  /* Clear commit_count for tables changed by transaction */
  NDB_SHARE* share;
  List_iterator_fast<NDB_SHARE> it(thd_ndb->changed_tables);
  while ((share= it++))
  {
    pthread_mutex_lock(&share->mutex);
    DBUG_PRINT("info", ("Invalidate commit_count for %s, share->commit_count: %d ",
			share->key, share->commit_count));
    share->commit_count= 0;
    share->commit_count_lock++;
    pthread_mutex_unlock(&share->mutex);
  }
  thd_ndb->changed_tables.empty();

  DBUG_RETURN(res);
}


/*
  Rollback a transaction started in NDB
 */

static int ndbcluster_rollback(THD *thd, bool all)
{
  int res= 0;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  NdbTransaction *trans= all ? thd_ndb->all : thd_ndb->stmt;

  DBUG_ENTER("ndbcluster_rollback");
  DBUG_PRINT("transaction",("%s",
                            trans == thd_ndb->stmt ? 
                            "stmt" : "all"));
  DBUG_ASSERT(ndb && trans);

  if (trans->execute(NdbTransaction::Rollback) != 0)
  {
    const NdbError err= trans->getNdbError();
    const NdbOperation *error_op= trans->getNdbErrorOperation();
    ERR_PRINT(err);     
    res= ndb_to_mysql_error(&err);
    if (res != -1) 
      ndbcluster_print_error(res, error_op);
  }
  ndb->closeTransaction(trans);

  if (all)
    thd_ndb->all= NULL;
  else
    thd_ndb->stmt= NULL;

  /* Clear list of tables changed by transaction */
  thd_ndb->changed_tables.empty();

  DBUG_RETURN(res);
}


/*
  Define NDB column based on Field.
  Returns 0 or mysql error code.
  Not member of ha_ndbcluster because NDBCOL cannot be declared.

  MySQL text types with character set "binary" are mapped to true
  NDB binary types without a character set.  This may change.
 */

static int create_ndb_column(NDBCOL &col,
                             Field *field,
                             HA_CREATE_INFO *info)
{
  // Set name
  col.setName(field->field_name);
  // Get char set
  CHARSET_INFO *cs= field->charset();
  // Set type and sizes
  const enum enum_field_types mysql_type= field->real_type();
  switch (mysql_type) {
  // Numeric types
  case MYSQL_TYPE_TINY:        
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Tinyunsigned);
    else
      col.setType(NDBCOL::Tinyint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_SHORT:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Smallunsigned);
    else
      col.setType(NDBCOL::Smallint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_LONG:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Unsigned);
    else
      col.setType(NDBCOL::Int);
    col.setLength(1);
    break;
  case MYSQL_TYPE_INT24:       
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Mediumunsigned);
    else
      col.setType(NDBCOL::Mediumint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_LONGLONG:
    if (field->flags & UNSIGNED_FLAG)
      col.setType(NDBCOL::Bigunsigned);
    else
      col.setType(NDBCOL::Bigint);
    col.setLength(1);
    break;
  case MYSQL_TYPE_FLOAT:
    col.setType(NDBCOL::Float);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DOUBLE:
    col.setType(NDBCOL::Double);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DECIMAL:    
    {
      Field_decimal *f= (Field_decimal*)field;
      uint precision= f->pack_length();
      uint scale= f->decimals();
      if (field->flags & UNSIGNED_FLAG)
      {
        col.setType(NDBCOL::Olddecimalunsigned);
        precision-= (scale > 0);
      }
      else
      {
        col.setType(NDBCOL::Olddecimal);
        precision-= 1 + (scale > 0);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    }
    break;
  case MYSQL_TYPE_NEWDECIMAL:    
    {
      Field_new_decimal *f= (Field_new_decimal*)field;
      uint precision= f->precision;
      uint scale= f->decimals();
      if (field->flags & UNSIGNED_FLAG)
      {
        col.setType(NDBCOL::Decimalunsigned);
      }
      else
      {
        col.setType(NDBCOL::Decimal);
      }
      col.setPrecision(precision);
      col.setScale(scale);
      col.setLength(1);
    }
    break;
  // Date types
  case MYSQL_TYPE_DATETIME:    
    col.setType(NDBCOL::Datetime);
    col.setLength(1);
    break;
  case MYSQL_TYPE_DATE: // ?
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_NEWDATE:
    col.setType(NDBCOL::Date);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIME:        
    col.setType(NDBCOL::Time);
    col.setLength(1);
    break;
  case MYSQL_TYPE_YEAR:
    col.setType(NDBCOL::Year);
    col.setLength(1);
    break;
  case MYSQL_TYPE_TIMESTAMP:
    col.setType(NDBCOL::Timestamp);
    col.setLength(1);
    break;
  // Char types
  case MYSQL_TYPE_STRING:      
    if (field->pack_length() == 0)
    {
      col.setType(NDBCOL::Bit);
      col.setLength(1);
    }
    else if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
    {
      col.setType(NDBCOL::Binary);
      col.setLength(field->pack_length());
    }
    else
    {
      col.setType(NDBCOL::Char);
      col.setCharset(cs);
      col.setLength(field->pack_length());
    }
    break;
  case MYSQL_TYPE_VAR_STRING: // ?
  case MYSQL_TYPE_VARCHAR:
    {
      Field_varstring* f= (Field_varstring*)field;
      if (f->length_bytes == 1)
      {
        if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Varbinary);
        else {
          col.setType(NDBCOL::Varchar);
          col.setCharset(cs);
        }
      }
      else if (f->length_bytes == 2)
      {
        if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
          col.setType(NDBCOL::Longvarbinary);
        else {
          col.setType(NDBCOL::Longvarchar);
          col.setCharset(cs);
        }
      }
      else
      {
        return HA_ERR_UNSUPPORTED;
      }
      col.setLength(field->field_length);
    }
    break;
  // Blob types (all come in as MYSQL_TYPE_BLOB)
  mysql_type_tiny_blob:
  case MYSQL_TYPE_TINY_BLOB:
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    // No parts
    col.setPartSize(0);
    col.setStripeSize(0);
    break;
  //mysql_type_blob:
  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_BLOB:    
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    // Use "<=" even if "<" is the exact condition
    if (field->max_length() <= (1 << 8))
      goto mysql_type_tiny_blob;
    else if (field->max_length() <= (1 << 16))
    {
      col.setInlineSize(256);
      col.setPartSize(2000);
      col.setStripeSize(16);
    }
    else if (field->max_length() <= (1 << 24))
      goto mysql_type_medium_blob;
    else
      goto mysql_type_long_blob;
    break;
  mysql_type_medium_blob:
  case MYSQL_TYPE_MEDIUM_BLOB:   
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    col.setPartSize(4000);
    col.setStripeSize(8);
    break;
  mysql_type_long_blob:
  case MYSQL_TYPE_LONG_BLOB:  
    if ((field->flags & BINARY_FLAG) && cs == &my_charset_bin)
      col.setType(NDBCOL::Blob);
    else {
      col.setType(NDBCOL::Text);
      col.setCharset(cs);
    }
    col.setInlineSize(256);
    col.setPartSize(8000);
    col.setStripeSize(4);
    break;
  // Other types
  case MYSQL_TYPE_ENUM:
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_SET:         
    col.setType(NDBCOL::Char);
    col.setLength(field->pack_length());
    break;
  case MYSQL_TYPE_BIT:
  {
    int no_of_bits= field->field_length;
    col.setType(NDBCOL::Bit);
    if (!no_of_bits)
      col.setLength(1);
      else
        col.setLength(no_of_bits);
    break;
  }
  case MYSQL_TYPE_NULL:        
    goto mysql_type_unsupported;
  mysql_type_unsupported:
  default:
    return HA_ERR_UNSUPPORTED;
  }
  // Set nullable and pk
  col.setNullable(field->maybe_null());
  col.setPrimaryKey(field->flags & PRI_KEY_FLAG);
  // Set autoincrement
  if (field->flags & AUTO_INCREMENT_FLAG) 
  {
    char buff[22];
    col.setAutoIncrement(TRUE);
    ulonglong value= info->auto_increment_value ?
      info->auto_increment_value : (ulonglong) 1;
    DBUG_PRINT("info", ("Autoincrement key, initial: %s", llstr(value, buff)));
    col.setAutoIncrementInitialValue(value);
  }
  else
    col.setAutoIncrement(FALSE);
  return 0;
}

/*
  Create a table in NDB Cluster
*/

int ha_ndbcluster::create(const char *name, 
                          TABLE *form, 
                          HA_CREATE_INFO *info)
{
  THD *thd= current_thd;
  NDBTAB tab;
  NDBCOL col;
  uint pack_length, length, i, pk_length= 0;
  const void *data, *pack_data;
  bool create_from_engine= (info->table_options & HA_OPTION_CREATE_FROM_ENGINE);
  bool is_truncate= (thd->lex->sql_command == SQLCOM_TRUNCATE);

  DBUG_ENTER("ha_ndbcluster::create");
  DBUG_PRINT("enter", ("name: %s", name));

  DBUG_ASSERT(*fn_rext((char*)name) == 0);
  set_dbname(name);
  set_tabname(name);

  if (is_truncate)
  {
    DBUG_PRINT("info", ("Dropping and re-creating table for TRUNCATE"));
    if ((my_errno= delete_table(name)))
      DBUG_RETURN(my_errno);
  }
  table= form;
  if (create_from_engine)
  {
    /*
      Table already exists in NDB and frm file has been created by 
      caller.
      Do Ndb specific stuff, such as create a .ndb file
    */
    if ((my_errno= write_ndb_file(name)))
      DBUG_RETURN(my_errno);
#ifdef HAVE_NDB_BINLOG
    ndbcluster_create_binlog_setup(get_ndb(), name, strlen(name),
                                   m_dbname, m_tabname, FALSE);
#endif /* HAVE_NDB_BINLOG */
    DBUG_RETURN(my_errno);
  }

#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow table creation unless
    schema distribution table is setup
    ( unless it is a creation of the schema dist table itself )
  */
  if (!schema_share &&
      !(strcmp(m_dbname, NDB_REP_DB) == 0 &&
        strcmp(m_tabname, NDB_SCHEMA_TABLE) == 0))
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
#endif /* HAVE_NDB_BINLOG */

  DBUG_PRINT("table", ("name: %s", m_tabname));  
  tab.setName(m_tabname);
  tab.setLogging(!(info->options & HA_LEX_CREATE_TMP_TABLE));    
   
  // Save frm data for this table
  if (readfrm(name, &data, &length))
    DBUG_RETURN(1);
  if (packfrm(data, length, &pack_data, &pack_length))
  {
    my_free((char*)data, MYF(0));
    DBUG_RETURN(2);
  }

  DBUG_PRINT("info", ("setFrm data=%lx  len=%d", pack_data, pack_length));
  tab.setFrm(pack_data, pack_length);      
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));
  
  for (i= 0; i < form->s->fields; i++) 
  {
    Field *field= form->field[i];
    DBUG_PRINT("info", ("name: %s, type: %u, pack_length: %d", 
                        field->field_name, field->real_type(),
                        field->pack_length()));
    if ((my_errno= create_ndb_column(col, field, info)))
      DBUG_RETURN(my_errno);
 
    if (info->store_on_disk || getenv("NDB_DEFAULT_DISK"))
      col.setStorageType(NdbDictionary::Column::StorageTypeDisk);
    else
      col.setStorageType(NdbDictionary::Column::StorageTypeMemory);

    tab.addColumn(col);
    if (col.getPrimaryKey())
      pk_length += (field->pack_length() + 3) / 4;
  }

  KEY* key_info;
  for (i= 0, key_info= form->key_info; i < form->s->keys; i++, key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    KEY_PART_INFO *end= key_part + key_info->key_parts;
    for (; key_part != end; key_part++)
      tab.getColumn(key_part->fieldnr-1)->setStorageType(
                             NdbDictionary::Column::StorageTypeMemory);
  }

  if (info->store_on_disk)
    if (info->tablespace)
      tab.setTablespace(info->tablespace);
    else
      tab.setTablespace("DEFAULT-TS");
  // No primary key, create shadow key as 64 bit, auto increment  
  if (form->s->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Generating shadow key"));
    col.setName("$PK");
    col.setType(NdbDictionary::Column::Bigunsigned);
    col.setLength(1);
    col.setNullable(FALSE);
    col.setPrimaryKey(TRUE);
    col.setAutoIncrement(TRUE);
    tab.addColumn(col);
    pk_length += 2;
  }
 
  // Make sure that blob tables don't have to big part size
  for (i= 0; i < form->s->fields; i++) 
  {
    /**
     * The extra +7 concists
     * 2 - words from pk in blob table
     * 5 - from extra words added by tup/dict??
     */
    switch (form->field[i]->real_type()) {
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_BLOB:    
    case MYSQL_TYPE_MEDIUM_BLOB:   
    case MYSQL_TYPE_LONG_BLOB: 
    {
      NdbDictionary::Column * col= tab.getColumn(i);
      int size= pk_length + (col->getPartSize()+3)/4 + 7;
      if (size > NDB_MAX_TUPLE_SIZE_IN_WORDS && 
         (pk_length+7) < NDB_MAX_TUPLE_SIZE_IN_WORDS)
      {
        size= NDB_MAX_TUPLE_SIZE_IN_WORDS - pk_length - 7;
        col->setPartSize(4*size);
      }
      /**
       * If size > NDB_MAX and pk_length+7 >= NDB_MAX
       *   then the table can't be created anyway, so skip
       *   changing part size, and have error later
       */ 
    }
    default:
      break;
    }
  }

  // Check partition info
  partition_info *part_info= form->part_info;
  if ((my_errno= set_up_partition_info(part_info, form, (void*)&tab)))
  {
    DBUG_RETURN(my_errno);
  }

  if ((my_errno= check_ndb_connection()))
    DBUG_RETURN(my_errno);
  
  // Create the table in NDB     
  Ndb *ndb= get_ndb();
  NDBDICT *dict= ndb->getDictionary();
  if (dict->createTable(tab) != 0) 
  {
    const NdbError err= dict->getNdbError();
    ERR_PRINT(err);
    my_errno= ndb_to_mysql_error(&err);
    DBUG_RETURN(my_errno);
  }

  Ndb_table_guard ndbtab_g(dict, m_tabname);
  // temporary set m_table during create
  // reset at return
  m_table= ndbtab_g.get_table();
  // TODO check also that we have the same frm...
  if (!m_table)
  {
    /* purecov: begin deadcode */
    const NdbError err= dict->getNdbError();
    ERR_PRINT(err);
    my_errno= ndb_to_mysql_error(&err);
    DBUG_RETURN(my_errno);
    /* purecov: end */
  }

  DBUG_PRINT("info", ("Table %s/%s created successfully", 
                      m_dbname, m_tabname));

  // Create secondary indexes
  my_errno= create_indexes(ndb, form);

  if (!my_errno)
    my_errno= write_ndb_file(name);
  else
  {
    /*
      Failed to create an index,
      drop the table (and all it's indexes)
    */
    while (dict->dropTableGlobal(*m_table))
    {
      switch (dict->getNdbError().status)
      {
        case NdbError::TemporaryError:
          if (!thd->killed) 
            continue; // retry indefinitly
          break;
        default:
          break;
      }
      break;
    }
    m_table = 0;
    DBUG_RETURN(my_errno);
  }

#ifdef HAVE_NDB_BINLOG
  if (!my_errno)
  {
    NDB_SHARE *share= 0;
    pthread_mutex_lock(&ndbcluster_mutex);
    /*
      First make sure we get a "fresh" share here, not an old trailing one...
    */
    {
      uint length= (uint) strlen(name);
      if ((share= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                           (byte*) name, length)))
        handle_trailing_share(share);
    }
    /*
      get a new share
    */

    if (!(share= get_share(name, form, true, true)))
    {
      sql_print_error("NDB: allocating table share for %s failed", name);
      /* my_errno is set */
    }
    pthread_mutex_unlock(&ndbcluster_mutex);

    while (!IS_TMP_PREFIX(m_tabname))
    {
      String event_name(INJECTOR_EVENT_LEN);
      ndb_rep_event_name(&event_name,m_dbname,m_tabname);
      int do_event_op= ndb_binlog_running;

      if (!schema_share &&
          strcmp(share->db, NDB_REP_DB) == 0 &&
          strcmp(share->table_name, NDB_SCHEMA_TABLE) == 0)
        do_event_op= 1;

      /*
        Always create an event for the table, as other mysql servers
        expect it to be there.
      */
      if (!ndbcluster_create_event(ndb, m_table, event_name.c_ptr(), share,
                                   share && do_event_op ? 2 : 1/* push warning */))
      {
        if (ndb_extra_logging)
          sql_print_information("NDB Binlog: CREATE TABLE Event: %s",
                                event_name.c_ptr());
        if (share && do_event_op &&
            ndbcluster_create_event_ops(share, m_table, event_name.c_ptr()))
        {
          sql_print_error("NDB Binlog: FAILED CREATE TABLE event operations."
                          " Event: %s", name);
          /* a warning has been issued to the client */
        }
      }
      /*
        warning has been issued if ndbcluster_create_event failed
        and (share && do_event_op)
      */
      if (share && !do_event_op)
        share->flags|= NSF_NO_BINLOG;
      ndbcluster_log_schema_op(thd, share,
                               thd->query, thd->query_length,
                               share->db, share->table_name,
                               m_table->getObjectId(),
                               m_table->getObjectVersion(),
                               (is_truncate) ?
			       SOT_TRUNCATE_TABLE : SOT_CREATE_TABLE, 
			       0, 0, 1);
      break;
    }
  }
#endif /* HAVE_NDB_BINLOG */

  m_table= 0;
  DBUG_RETURN(my_errno);
}

int ha_ndbcluster::create_handler_files(const char *file,
                                        const char *old_name,
                                        int action_flag,
                                        HA_CREATE_INFO *info) 
{ 
  char path[FN_REFLEN];
  const char *name;
  Ndb* ndb;
  const NDBTAB *tab;
  const void *data, *pack_data;
  uint length, pack_length;
  int error= 0;

  DBUG_ENTER("create_handler_files");

  if (action_flag != CHF_INDEX_FLAG)
  {
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("enter", ("file: %s", file));
  if (!(ndb= get_ndb()))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  NDBDICT *dict= ndb->getDictionary();
  if (!info->frm_only)
    DBUG_RETURN(0); // Must be a create, ignore since frm is saved in create

  // TODO handle this
  DBUG_ASSERT(m_table != 0);

  set_dbname(file);
  set_tabname(file);
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  DBUG_PRINT("info", ("m_dbname: %s, m_tabname: %s", m_dbname, m_tabname));
  if (!(tab= ndbtab_g.get_table()))
    DBUG_RETURN(0); // Unkown table, must be temporary table

  DBUG_ASSERT(get_ndb_share_state(m_share) == NSS_ALTERED);
  if (readfrm(file, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    DBUG_PRINT("info", ("Missing frm for %s", m_tabname));
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
    error= 1;
  }
  else
  {
    DBUG_PRINT("info", ("Table %s has changed, altering frm in ndb", 
                        m_tabname));
    NdbDictionary::Table new_tab= *tab;
    new_tab.setFrm(pack_data, pack_length);
    if (dict->alterTableGlobal(*tab, new_tab))
    {
      error= ndb_to_mysql_error(&dict->getNdbError());
    }
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
  }
  
  set_ndb_share_state(m_share, NSS_INITIAL);
  free_share(&m_share); // Decrease ref_count

  DBUG_RETURN(error);
}

int ha_ndbcluster::create_index(const char *name, KEY *key_info, 
                                NDB_INDEX_TYPE idx_type, uint idx_no)
{
  int error= 0;
  char unique_name[FN_LEN];
  static const char* unique_suffix= "$unique";
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_PRINT("info", ("Creating index %u: %s", idx_no, name));  

  if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
  {
    strxnmov(unique_name, FN_LEN, name, unique_suffix, NullS);
    DBUG_PRINT("info", ("Created unique index name \'%s\' for index %d",
                        unique_name, idx_no));
  }
    
  switch (idx_type){
  case PRIMARY_KEY_INDEX:
    // Do nothing, already created
    break;
  case PRIMARY_KEY_ORDERED_INDEX:
    error= create_ordered_index(name, key_info);
    break;
  case UNIQUE_ORDERED_INDEX:
    if (!(error= create_ordered_index(name, key_info)))
      error= create_unique_index(unique_name, key_info);
    break;
  case UNIQUE_INDEX:
    if (!(error= check_index_fields_not_null(idx_no)))
      error= create_unique_index(unique_name, key_info);
    break;
  case ORDERED_INDEX:
    error= create_ordered_index(name, key_info);
    break;
  default:
    DBUG_ASSERT(FALSE);
    break;
  }
  
  DBUG_RETURN(error);
}

int ha_ndbcluster::create_ordered_index(const char *name, 
                                        KEY *key_info)
{
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_RETURN(create_ndb_index(name, key_info, FALSE));
}

int ha_ndbcluster::create_unique_index(const char *name, 
                                       KEY *key_info)
{

  DBUG_ENTER("ha_ndbcluster::create_unique_index");
  DBUG_RETURN(create_ndb_index(name, key_info, TRUE));
}


/*
  Create an index in NDB Cluster
 */

int ha_ndbcluster::create_ndb_index(const char *name, 
                                     KEY *key_info,
                                     bool unique)
{
  Ndb *ndb= get_ndb();
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part + key_info->key_parts;
  
  DBUG_ENTER("ha_ndbcluster::create_index");
  DBUG_PRINT("enter", ("name: %s ", name));

  NdbDictionary::Index ndb_index(name);
  if (unique)
    ndb_index.setType(NdbDictionary::Index::UniqueHashIndex);
  else 
  {
    ndb_index.setType(NdbDictionary::Index::OrderedIndex);
    // TODO Only temporary ordered indexes supported
    ndb_index.setLogging(FALSE); 
  }
  ndb_index.setTable(m_tabname);

  for (; key_part != end; key_part++) 
  {
    Field *field= key_part->field;
    DBUG_PRINT("info", ("attr: %s", field->field_name));
    ndb_index.addColumnName(field->field_name);
  }
  
  if (dict->createIndex(ndb_index, *m_table))
    ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  DBUG_RETURN(0);  
}

/*
 Prepare for an on-line alter table
*/ 
void ha_ndbcluster::prepare_for_alter()
{
  ndbcluster_get_share(m_share); // Increase ref_count
  set_ndb_share_state(m_share, NSS_ALTERED);
}

/*
  Add an index on-line to a table
*/
int ha_ndbcluster::add_index(TABLE *table_arg, 
                             KEY *key_info, uint num_of_keys)
{
  DBUG_ENTER("ha_ndbcluster::add_index");
  DBUG_PRINT("info", ("ha_ndbcluster::add_index to table %s", 
                      table_arg->s->table_name));
  int error= 0;
  uint idx;

  DBUG_ASSERT(m_share->state == NSS_ALTERED);
  for (idx= 0; idx < num_of_keys; idx++)
  {
    KEY *key= key_info + idx;
    KEY_PART_INFO *key_part= key->key_part;
    KEY_PART_INFO *end= key_part + key->key_parts;
    NDB_INDEX_TYPE idx_type= get_index_type_from_key(idx, key, false);
    DBUG_PRINT("info", ("Adding index: '%s'", key_info[idx].name));
    // Add fields to key_part struct
    for (; key_part != end; key_part++)
      key_part->field= table->field[key_part->fieldnr];
    // Check index type
    // Create index in ndb
    if((error= create_index(key_info[idx].name, key, idx_type, idx)))
      break;
  }
  if (error)
  {
    set_ndb_share_state(m_share, NSS_INITIAL);
    free_share(&m_share); // Decrease ref_count
  }
  DBUG_RETURN(error);  
}

/*
  Mark one or several indexes for deletion. and
  renumber the remaining indexes
*/
int ha_ndbcluster::prepare_drop_index(TABLE *table_arg, 
                                      uint *key_num, uint num_of_keys)
{
  DBUG_ENTER("ha_ndbcluster::prepare_drop_index");
  DBUG_ASSERT(m_share->state == NSS_ALTERED);
  // Mark indexes for deletion
  uint idx;
  for (idx= 0; idx < num_of_keys; idx++)
  {
    DBUG_PRINT("info", ("ha_ndbcluster::prepare_drop_index %u", *key_num));
    m_index[*key_num++].status= TO_BE_DROPPED;
  }
  // Renumber indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  renumber_indexes(ndb, table_arg);
  DBUG_RETURN(0);
}
 
/*
  Really drop all indexes marked for deletion
*/
int ha_ndbcluster::final_drop_index(TABLE *table_arg)
{
  int error;
  DBUG_ENTER("ha_ndbcluster::final_drop_index");
  DBUG_PRINT("info", ("ha_ndbcluster::final_drop_index"));
  // Really drop indexes
  THD *thd= current_thd;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  Ndb *ndb= thd_ndb->ndb;
  if((error= drop_indexes(ndb, table_arg)))
  {
    m_share->state= NSS_INITIAL;
    free_share(&m_share); // Decrease ref_count
  }
  DBUG_RETURN(error);
}

/*
  Rename a table in NDB Cluster
*/

int ha_ndbcluster::rename_table(const char *from, const char *to)
{
  NDBDICT *dict;
  char old_dbname[FN_HEADLEN];
  char new_dbname[FN_HEADLEN];
  char new_tabname[FN_HEADLEN];
  const NDBTAB *orig_tab;
  int result;
  bool recreate_indexes= FALSE;
  NDBDICT::List index_list;

  DBUG_ENTER("ha_ndbcluster::rename_table");
  DBUG_PRINT("info", ("Renaming %s to %s", from, to));
  set_dbname(from, old_dbname);
  set_dbname(to, new_dbname);
  set_tabname(from);
  set_tabname(to, new_tabname);

  if (check_ndb_connection())
    DBUG_RETURN(my_errno= HA_ERR_NO_CONNECTION);

  Ndb *ndb= get_ndb();
  ndb->setDatabaseName(old_dbname);
  dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, m_tabname);
  if (!(orig_tab= ndbtab_g.get_table()))
    ERR_RETURN(dict->getNdbError());

#ifdef HAVE_NDB_BINLOG
  int ndb_table_id= orig_tab->getObjectId();
  int ndb_table_version= orig_tab->getObjectVersion();

  NDB_SHARE *share= get_share(from, 0, false);
  if (share)
  {
    int r= rename_share(share, to);
    DBUG_ASSERT(r == 0);
  }
#endif
  if (my_strcasecmp(system_charset_info, new_dbname, old_dbname))
  {
    dict->listIndexes(index_list, *orig_tab);    
    recreate_indexes= TRUE;
  }
  // Change current database to that of target table
  set_dbname(to);
  ndb->setDatabaseName(m_dbname);

  NdbDictionary::Table new_tab= *orig_tab;
  new_tab.setName(new_tabname);
  if (dict->alterTableGlobal(*orig_tab, new_tab) != 0)
  {
    NdbError ndb_error= dict->getNdbError();
#ifdef HAVE_NDB_BINLOG
    if (share)
    {
      int r= rename_share(share, from);
      DBUG_ASSERT(r == 0);
      free_share(&share);
    }
#endif
    ERR_RETURN(ndb_error);
  }
  
  // Rename .ndb file
  if ((result= handler::rename_table(from, to)))
  {
    // ToDo in 4.1 should rollback alter table...
#ifdef HAVE_NDB_BINLOG
    if (share)
      free_share(&share);
#endif
    DBUG_RETURN(result);
  }

#ifdef HAVE_NDB_BINLOG
  int is_old_table_tmpfile= 1;
  if (share && share->op)
    dict->forceGCPWait();

  /* handle old table */
  if (!IS_TMP_PREFIX(m_tabname))
  {
    is_old_table_tmpfile= 0;
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, from + sizeof(share_prefix) - 1, 0);
    ndbcluster_handle_drop_table(ndb, event_name.c_ptr(), share,
                                 "rename table");
  }

  if (!result && !IS_TMP_PREFIX(new_tabname))
  {
    /* always create an event for the table */
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, to + sizeof(share_prefix) - 1, 0);
    Ndb_table_guard ndbtab_g2(dict, new_tabname);
    const NDBTAB *ndbtab= ndbtab_g2.get_table();

    if (!ndbcluster_create_event(ndb, ndbtab, event_name.c_ptr(), share,
                                 share && ndb_binlog_running ? 2 : 1/* push warning */))
    {
      if (ndb_extra_logging)
        sql_print_information("NDB Binlog: RENAME Event: %s",
                              event_name.c_ptr());
      if (share && ndb_binlog_running &&
          ndbcluster_create_event_ops(share, ndbtab, event_name.c_ptr()))
      {
        sql_print_error("NDB Binlog: FAILED create event operations "
                        "during RENAME. Event %s", event_name.c_ptr());
        /* a warning has been issued to the client */
      }
    }
    /*
      warning has been issued if ndbcluster_create_event failed
      and (share && ndb_binlog_running)
    */
    if (!is_old_table_tmpfile)
      ndbcluster_log_schema_op(current_thd, share,
                               current_thd->query, current_thd->query_length,
                               old_dbname, m_tabname,
                               ndb_table_id, ndb_table_version,
                               SOT_RENAME_TABLE,
                               m_dbname, new_tabname, 1);
  }

  // If we are moving tables between databases, we need to recreate
  // indexes
  if (recreate_indexes)
  {
    for (unsigned i = 0; i < index_list.count; i++) 
    {
        NDBDICT::List::Element& index_el = index_list.elements[i];
	// Recreate any indexes not stored in the system database
	if (my_strcasecmp(system_charset_info, 
			  index_el.database, NDB_SYSTEM_DATABASE))
	{
	  set_dbname(from);
	  ndb->setDatabaseName(m_dbname);
	  const NDBINDEX * index= dict->getIndexGlobal(index_el.name,  new_tab);
	  DBUG_PRINT("info", ("Creating index %s/%s",
			      index_el.database, index->getName()));
	  dict->createIndex(*index, new_tab);
	  DBUG_PRINT("info", ("Dropping index %s/%s",
			      index_el.database, index->getName()));
	  set_dbname(from);
	  ndb->setDatabaseName(m_dbname);
	  dict->dropIndexGlobal(*index);
	}
    }
  }
  if (share)
    free_share(&share);
#endif

  DBUG_RETURN(result);
}


/*
  Delete table from NDB Cluster

 */

/* static version which does not need a handler */

int
ha_ndbcluster::delete_table(ha_ndbcluster *h, Ndb *ndb,
                            const char *path,
                            const char *db,
                            const char *table_name)
{
  THD *thd= current_thd;
  DBUG_ENTER("ha_ndbcluster::ndbcluster_delete_table");
  NDBDICT *dict= ndb->getDictionary();
  int ndb_table_id= 0;
  int ndb_table_version= 0;
#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow drop table unless
    schema distribution table is setup
  */
  if (!schema_share)
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  NDB_SHARE *share= get_share(path, 0, false);
#endif

  /* Drop the table from NDB */
  
  int res= 0;
  if (h && h->m_table)
  {
retry_temporary_error1:
    if (dict->dropTableGlobal(*h->m_table) == 0)
    {
      ndb_table_id= h->m_table->getObjectId();
      ndb_table_version= h->m_table->getObjectVersion();
    }
    else
    {
      switch (dict->getNdbError().status)
      {
        case NdbError::TemporaryError:
          if (!thd->killed) 
            goto retry_temporary_error1; // retry indefinitly
          break;
        default:
          break;
      }
      res= ndb_to_mysql_error(&dict->getNdbError());
    }
    h->release_metadata(thd, ndb);
  }
  else
  {
    ndb->setDatabaseName(db);
    while (1)
    {
      Ndb_table_guard ndbtab_g(dict, table_name);
      if (ndbtab_g.get_table())
      {
    retry_temporary_error2:
        if (dict->dropTableGlobal(*ndbtab_g.get_table()) == 0)
        {
          ndb_table_id= ndbtab_g.get_table()->getObjectId();
          ndb_table_version= ndbtab_g.get_table()->getObjectVersion();
        }
        else
        {
          switch (dict->getNdbError().status)
          {
            case NdbError::TemporaryError:
              if (!thd->killed) 
                goto retry_temporary_error2; // retry indefinitly
              break;
            default:
              if (dict->getNdbError().code == NDB_INVALID_SCHEMA_OBJECT)
              {
                ndbtab_g.invalidate();
                continue;
              }
              break;
          }
        }
      }
      else
        res= ndb_to_mysql_error(&dict->getNdbError());
      break;
    }
  }

  if (res)
  {
#ifdef HAVE_NDB_BINLOG
    /* the drop table failed for some reason, drop the share anyways */
    if (share)
    {
      pthread_mutex_lock(&ndbcluster_mutex);
      if (share->state != NSS_DROPPED)
      {
        /*
          The share kept by the server has not been freed, free it
        */
        share->state= NSS_DROPPED;
        free_share(&share, TRUE);
      }
      /* free the share taken above */
      free_share(&share, TRUE);
      pthread_mutex_unlock(&ndbcluster_mutex);
    }
#endif
    DBUG_RETURN(res);
  }

#ifdef HAVE_NDB_BINLOG
  /* stop the logging of the dropped table, and cleanup */

  /*
    drop table is successful even if table does not exist in ndb
    and in case table was actually not dropped, there is no need
    to force a gcp, and setting the event_name to null will indicate
    that there is no event to be dropped
  */
  int table_dropped= dict->getNdbError().code != 709;

  if (!IS_TMP_PREFIX(table_name) && share &&
      current_thd->lex->sql_command != SQLCOM_TRUNCATE)
  {
    ndbcluster_log_schema_op(thd, share,
                             thd->query, thd->query_length,
                             share->db, share->table_name,
                             ndb_table_id, ndb_table_version,
                             SOT_DROP_TABLE, 0, 0, 1);
  }
  else if (table_dropped && share && share->op) /* ndbcluster_log_schema_op
                                                   will do a force GCP */
    dict->forceGCPWait();

  if (!IS_TMP_PREFIX(table_name))
  {
    String event_name(INJECTOR_EVENT_LEN);
    ndb_rep_event_name(&event_name, path + sizeof(share_prefix) - 1, 0);
    ndbcluster_handle_drop_table(ndb,
                                 table_dropped ? event_name.c_ptr() : 0,
                                 share, "delete table");
  }

  if (share)
  {
    pthread_mutex_lock(&ndbcluster_mutex);
    if (share->state != NSS_DROPPED)
    {
      /*
        The share kept by the server has not been freed, free it
      */
      share->state= NSS_DROPPED;
      free_share(&share, TRUE);
    }
    /* free the share taken above */
    free_share(&share, TRUE);
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
#endif
  DBUG_RETURN(0);
}

int ha_ndbcluster::delete_table(const char *name)
{
  DBUG_ENTER("ha_ndbcluster::delete_table");
  DBUG_PRINT("enter", ("name: %s", name));
  set_dbname(name);
  set_tabname(name);

#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow drop table unless
    schema distribution table is setup
  */
  if (!schema_share)
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
#endif

  if (check_ndb_connection())
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  /* Call ancestor function to delete .ndb file */
  handler::delete_table(name);

  DBUG_RETURN(delete_table(this, get_ndb(),name, m_dbname, m_tabname));
}


void ha_ndbcluster::get_auto_increment(ulonglong offset, ulonglong increment,
                                       ulonglong nb_desired_values,
                                       ulonglong *first_value,
                                       ulonglong *nb_reserved_values)
{  
  int cache_size;
  Uint64 auto_value;
  DBUG_ENTER("get_auto_increment");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));
  Ndb *ndb= get_ndb();
   
  if (m_rows_inserted > m_rows_to_insert)
  {
    /* We guessed too low */
    m_rows_to_insert+= m_autoincrement_prefetch;
  }
  cache_size= 
    (int) ((m_rows_to_insert - m_rows_inserted < m_autoincrement_prefetch) ?
           m_rows_to_insert - m_rows_inserted :
           ((m_rows_to_insert > m_autoincrement_prefetch) ?
            m_rows_to_insert : m_autoincrement_prefetch));
  int ret;
  uint retries= NDB_AUTO_INCREMENT_RETRIES;
  do {
    Ndb_tuple_id_range_guard g(m_share);
    ret=
      m_skip_auto_increment ? 
      ndb->readAutoIncrementValue(m_table, g.range, auto_value) :
      ndb->getAutoIncrementValue(m_table, g.range, auto_value, cache_size);
  } while (ret == -1 && 
           --retries &&
           ndb->getNdbError().status == NdbError::TemporaryError);
  if (ret == -1)
  {
    const NdbError err= ndb->getNdbError();
    sql_print_error("Error %lu in ::get_auto_increment(): %s",
                    (ulong) err.code, err.message);
    *first_value= ~(ulonglong) 0;
    DBUG_VOID_RETURN;
  }
  *first_value= (longlong)auto_value;
  /* From the point of view of MySQL, NDB reserves one row at a time */
  *nb_reserved_values= 1;
  DBUG_VOID_RETURN;
}


/*
  Constructor for the NDB Cluster table handler 
 */

#define HA_NDBCLUSTER_TABLE_FLAGS \
                HA_REC_NOT_IN_SEQ | \
                HA_NULL_IN_KEY | \
                HA_AUTO_PART_KEY | \
                HA_NO_PREFIX_CHAR_KEYS | \
                HA_NEED_READ_RANGE_BUFFER | \
                HA_CAN_GEOMETRY | \
                HA_CAN_BIT_FIELD | \
                HA_PRIMARY_KEY_REQUIRED_FOR_POSITION | \
                HA_PRIMARY_KEY_REQUIRED_FOR_DELETE | \
                HA_PARTIAL_COLUMN_READ | \
                HA_HAS_OWN_BINLOGGING | \
                HA_HAS_RECORDS

ha_ndbcluster::ha_ndbcluster(TABLE_SHARE *table_arg):
  handler(&ndbcluster_hton, table_arg),
  m_active_trans(NULL),
  m_active_cursor(NULL),
  m_table(NULL),
  m_table_info(NULL),
  m_table_flags(HA_NDBCLUSTER_TABLE_FLAGS),
  m_share(0),
  m_part_info(NULL),
  m_use_partition_function(FALSE),
  m_sorted(FALSE),
  m_use_write(FALSE),
  m_ignore_dup_key(FALSE),
  m_has_unique_index(FALSE),
  m_primary_key_update(FALSE),
  m_ignore_no_key(FALSE),
  m_rows_to_insert((ha_rows) 1),
  m_rows_inserted((ha_rows) 0),
  m_bulk_insert_rows((ha_rows) 1024),
  m_rows_changed((ha_rows) 0),
  m_bulk_insert_not_flushed(FALSE),
  m_ops_pending(0),
  m_skip_auto_increment(TRUE),
  m_blobs_pending(0),
  m_blobs_offset(0),
  m_blobs_buffer(0),
  m_blobs_buffer_size(0),
  m_dupkey((uint) -1),
  m_ha_not_exact_count(FALSE),
  m_force_send(TRUE),
  m_autoincrement_prefetch((ha_rows) 32),
  m_transaction_on(TRUE),
  m_cond_stack(NULL),
  m_multi_cursor(NULL)
{
  int i;
 
  DBUG_ENTER("ha_ndbcluster");

  m_tabname[0]= '\0';
  m_dbname[0]= '\0';

  stats.records= ~(ha_rows)0; // uninitialized
  stats.block_size= 1024;

  for (i= 0; i < MAX_KEY; i++)
    ndb_init_index(m_index[i]);

  DBUG_VOID_RETURN;
}


int ha_ndbcluster::ha_initialise()
{
  DBUG_ENTER("ha_ndbcluster::ha_initialise");
  if (check_ndb_in_thd(current_thd))
  {
    DBUG_RETURN(FALSE);
  }
  DBUG_RETURN(TRUE);
}

/*
  Destructor for NDB Cluster table handler
 */

ha_ndbcluster::~ha_ndbcluster() 
{
  THD *thd= current_thd;
  Ndb *ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
  DBUG_ENTER("~ha_ndbcluster");

  if (m_share)
  {
    free_share(&m_share);
  }
  release_metadata(thd, ndb);
  my_free(m_blobs_buffer, MYF(MY_ALLOW_ZERO_PTR));
  m_blobs_buffer= 0;

  // Check for open cursor/transaction
  if (m_active_cursor) {
  }
  DBUG_ASSERT(m_active_cursor == NULL);
  if (m_active_trans) {
  }
  DBUG_ASSERT(m_active_trans == NULL);

  // Discard the condition stack
  DBUG_PRINT("info", ("Clearing condition stack"));
  cond_clear();

  DBUG_VOID_RETURN;
}



/*
  Open a table for further use
  - fetch metadata for this table from NDB
  - check that table exists

  RETURN
    0    ok
    < 0  Table has changed
*/

int ha_ndbcluster::open(const char *name, int mode, uint test_if_locked)
{
  int res;
  KEY *key;
  DBUG_ENTER("ha_ndbcluster::open");
  DBUG_PRINT("enter", ("name: %s  mode: %d  test_if_locked: %d",
                       name, mode, test_if_locked));
  
  /*
    Setup ref_length to make room for the whole 
    primary key to be written in the ref variable
  */
  
  if (table_share->primary_key != MAX_KEY) 
  {
    key= table->key_info+table_share->primary_key;
    ref_length= key->key_length;
  }
  else // (table_share->primary_key == MAX_KEY) 
  {
    if (m_use_partition_function)
    {
      ref_length+= sizeof(m_part_id);
    }
  }

  DBUG_PRINT("info", ("ref_length: %d", ref_length));

  // Init table lock structure 
  if (!(m_share=get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&m_share->lock,&m_lock,(void*) 0);
  
  set_dbname(name);
  set_tabname(name);
  
  if (check_ndb_connection()) {
    free_share(&m_share);
    m_share= 0;
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  
  res= get_metadata(name);
  if (!res)
    info(HA_STATUS_VARIABLE | HA_STATUS_CONST);

#ifdef HAVE_NDB_BINLOG
  if (!ndb_binlog_tables_inited && ndb_binlog_running)
    table->db_stat|= HA_READ_ONLY;
#endif

  DBUG_RETURN(res);
}

/*
  Set partition info

  SYNOPSIS
    set_part_info()
    part_info

  RETURN VALUE
    NONE

  DESCRIPTION
    Set up partition info when handler object created
*/

void ha_ndbcluster::set_part_info(partition_info *part_info)
{
  m_part_info= part_info;
  if (!(m_part_info->part_type == HASH_PARTITION &&
        m_part_info->list_of_part_fields &&
        !m_part_info->is_sub_partitioned()))
    m_use_partition_function= TRUE;
}

/*
  Close the table
  - release resources setup by open()
 */

int ha_ndbcluster::close(void)
{
  DBUG_ENTER("close");
  THD *thd= current_thd;
  Ndb *ndb= thd ? check_ndb_in_thd(thd) : g_ndb;
  free_share(&m_share);
  m_share= 0;
  release_metadata(thd, ndb);
  DBUG_RETURN(0);
}


Thd_ndb* ha_ndbcluster::seize_thd_ndb()
{
  Thd_ndb *thd_ndb;
  DBUG_ENTER("seize_thd_ndb");

  thd_ndb= new Thd_ndb();
  if (thd_ndb->ndb->init(max_transactions) != 0)
  {
    ERR_PRINT(thd_ndb->ndb->getNdbError());
    /*
      TODO 
      Alt.1 If init fails because to many allocated Ndb 
      wait on condition for a Ndb object to be released.
      Alt.2 Seize/release from pool, wait until next release 
    */
    delete thd_ndb;
    thd_ndb= NULL;
  }
  DBUG_RETURN(thd_ndb);
}


void ha_ndbcluster::release_thd_ndb(Thd_ndb* thd_ndb)
{
  DBUG_ENTER("release_thd_ndb");
  delete thd_ndb;
  DBUG_VOID_RETURN;
}


/*
  If this thread already has a Thd_ndb object allocated
  in current THD, reuse it. Otherwise
  seize a Thd_ndb object, assign it to current THD and use it.
 
*/

Ndb* check_ndb_in_thd(THD* thd)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb)
  {
    if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
      return NULL;
    set_thd_ndb(thd, thd_ndb);
  }
  return thd_ndb->ndb;
}



int ha_ndbcluster::check_ndb_connection(THD* thd)
{
  Ndb *ndb;
  DBUG_ENTER("check_ndb_connection");
  
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  ndb->setDatabaseName(m_dbname);
  DBUG_RETURN(0);
}


static int ndbcluster_close_connection(THD *thd)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ENTER("ndbcluster_close_connection");
  if (thd_ndb)
  {
    ha_ndbcluster::release_thd_ndb(thd_ndb);
    set_thd_ndb(thd, NULL); // not strictly required but does not hurt either
  }
  DBUG_RETURN(0);
}


/*
  Try to discover one table from NDB
 */

int ndbcluster_discover(THD* thd, const char *db, const char *name,
                        const void** frmblob, uint* frmlen)
{
  int error= 0;
  NdbError ndb_error;
  uint len;
  const void* data;
  Ndb* ndb;
  char key[FN_REFLEN];
  DBUG_ENTER("ndbcluster_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name)); 

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);  
  ndb->setDatabaseName(db);
  NDBDICT* dict= ndb->getDictionary();
  build_table_filename(key, sizeof(key), db, name, "", 0);
  NDB_SHARE *share= get_share(key, 0, false);
  if (share && get_ndb_share_state(share) == NSS_ALTERED)
  {
    // Frm has been altered on disk, but not yet written to ndb
    if (readfrm(key, &data, &len))
    {
      DBUG_PRINT("error", ("Could not read frm"));
      error= 1;
      goto err;
    }
  }
  else
  {
    Ndb_table_guard ndbtab_g(dict, name);
    const NDBTAB *tab= ndbtab_g.get_table();
    if (!tab)
    {
      const NdbError err= dict->getNdbError();
      if (err.code == 709 || err.code == 723)
        error= -1;
      else
        ndb_error= err;
      goto err;
    }
    DBUG_PRINT("info", ("Found table %s", tab->getName()));
    
    len= tab->getFrmLength();  
    if (len == 0 || tab->getFrmData() == NULL)
    {
      DBUG_PRINT("error", ("No frm data found."));
      error= 1;
      goto err;
    }
    
    if (unpackfrm(&data, &len, tab->getFrmData()))
    {
      DBUG_PRINT("error", ("Could not unpack table"));
      error= 1;
      goto err;
    }
  }

  *frmlen= len;
  *frmblob= data;
  
  if (share)
    free_share(&share);

  DBUG_RETURN(0);
err:
  if (share)
    free_share(&share);
  if (ndb_error.code)
  {
    ERR_RETURN(ndb_error);
  }
  DBUG_RETURN(error);
}

/*
  Check if a table exists in NDB

 */

int ndbcluster_table_exists_in_engine(THD* thd, const char *db,
                                      const char *name)
{
  Ndb* ndb;
  DBUG_ENTER("ndbcluster_table_exists_in_engine");
  DBUG_PRINT("enter", ("db: %s  name: %s", db, name));

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  NDBDICT* dict= ndb->getDictionary();
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(dict->getNdbError());
  for (uint i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    if (my_strcasecmp(system_charset_info, elmt.database, db))
      continue;
    if (my_strcasecmp(system_charset_info, elmt.name, name))
      continue;
    DBUG_PRINT("info", ("Found table"));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}



extern "C" byte* tables_get_key(const char *entry, uint *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= strlen(entry);
  return (byte*) entry;
}


/*
  Drop a database in NDB Cluster
  NOTE add a dummy void function, since stupid handlerton is returning void instead of int...
*/

int ndbcluster_drop_database_impl(const char *path)
{
  DBUG_ENTER("ndbcluster_drop_database");
  THD *thd= current_thd;
  char dbname[FN_HEADLEN];
  Ndb* ndb;
  NdbDictionary::Dictionary::List list;
  uint i;
  char *tabname;
  List<char> drop_list;
  int ret= 0;
  ha_ndbcluster::set_dbname(path, (char *)&dbname);
  DBUG_PRINT("enter", ("db: %s", dbname));
  
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(-1);
  
  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
    DBUG_RETURN(-1);
  for (i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& elmt= list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));     
    
    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, elmt.database, dbname))
      continue;
    DBUG_PRINT("info", ("%s must be dropped", elmt.name));     
    drop_list.push_back(thd->strdup(elmt.name));
  }
  // Drop any tables belonging to database
  char full_path[FN_REFLEN];
  char *tmp= full_path +
    build_table_filename(full_path, sizeof(full_path), dbname, "", "", 0);

  ndb->setDatabaseName(dbname);
  List_iterator_fast<char> it(drop_list);
  while ((tabname=it++))
  {
    tablename_to_filename(tabname, tmp, FN_REFLEN - (tmp - full_path)-1);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (ha_ndbcluster::delete_table(0, ndb, full_path, dbname, tabname))
    {
      const NdbError err= dict->getNdbError();
      if (err.code != 709 && err.code != 723)
      {
        ERR_PRINT(err);
        ret= ndb_to_mysql_error(&err);
      }
    }
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  DBUG_RETURN(ret);      
}

static void ndbcluster_drop_database(char *path)
{
  THD *thd= current_thd;
  DBUG_ENTER("ndbcluster_drop_database");
#ifdef HAVE_NDB_BINLOG
  /*
    Don't allow drop database unless
    schema distribution table is setup
  */
  if (!schema_share)
  {
    DBUG_PRINT("info", ("Schema distribution table not setup"));
    DBUG_VOID_RETURN;
    //DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
#endif
  ndbcluster_drop_database_impl(path);
#ifdef HAVE_NDB_BINLOG
  char db[FN_REFLEN];
  ha_ndbcluster::set_dbname(path, db);
  ndbcluster_log_schema_op(thd, 0,
                           thd->query, thd->query_length,
                           db, "", 0, 0, SOT_DROP_DB, 0, 0, 0);
#endif
  DBUG_VOID_RETURN;
}
/*
  find all tables in ndb and discover those needed
*/
int ndb_create_table_from_engine(THD *thd, const char *db,
                                 const char *table_name)
{
  LEX *old_lex= thd->lex, newlex;
  thd->lex= &newlex;
  newlex.current_select= NULL;
  lex_start(thd, (const uchar*) "", 0);
  int res= ha_create_table_from_engine(thd, db, table_name);
  thd->lex= old_lex;
  return res;
}

int ndbcluster_find_all_files(THD *thd)
{
  DBUG_ENTER("ndbcluster_find_all_files");
  Ndb* ndb;
  char key[FN_REFLEN];

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  NDBDICT *dict= ndb->getDictionary();

  int unhandled, retries= 5, skipped;
  LINT_INIT(unhandled);
  LINT_INIT(skipped);
  do
  {
    NdbDictionary::Dictionary::List list;
    if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
      ERR_RETURN(dict->getNdbError());
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
        build_table_filename(key, sizeof(key), elmt.database, "", "", 0);
      if (my_access(key, F_OK))
      {
        /* no such database defined, skip table */
        continue;
      }
      /* finalize construction of path */
      end+= tablename_to_filename(elmt.name, end,
                                  sizeof(key)-(end-key));
      const void *data= 0, *pack_data= 0;
      uint length, pack_length;
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
        NDB_SHARE *share= get_share(key, 0, false);
        if (!share || get_ndb_share_state(share) != NSS_ALTERED)
        {
          discover= 1;
          sql_print_information("NDB: mismatch in frm for %s.%s, discovering...",
                                elmt.database, elmt.name);
        }
        if (share)
          free_share(&share);
      }
      my_free((char*) data, MYF(MY_ALLOW_ZERO_PTR));
      my_free((char*) pack_data, MYF(MY_ALLOW_ZERO_PTR));

      pthread_mutex_lock(&LOCK_open);
      if (discover)
      {
        /* ToDo 4.1 database needs to be created if missing */
        if (ndb_create_table_from_engine(thd, elmt.database, elmt.name))
        {
          /* ToDo 4.1 handle error */
        }
      }
#ifdef HAVE_NDB_BINLOG
      else
      {
        /* set up replication for this table */
        ndbcluster_create_binlog_setup(ndb, key, end-key,
                                       elmt.database, elmt.name,
                                       TRUE);
      }
#endif
      pthread_mutex_unlock(&LOCK_open);
    }
  }
  while (unhandled && retries);

  DBUG_RETURN(-(skipped + unhandled));
}

int ndbcluster_find_files(THD *thd,const char *db,const char *path,
                          const char *wild, bool dir, List<char> *files)
{
  DBUG_ENTER("ndbcluster_find_files");
  DBUG_PRINT("enter", ("db: %s", db));
  { // extra bracket to avoid gcc 2.95.3 warning
  uint i;
  Ndb* ndb;
  char name[FN_REFLEN];
  HASH ndb_tables, ok_tables;
  NDBDICT::List list;

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  if (dir)
    DBUG_RETURN(0); // Discover of databases not yet supported

  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(dict->getNdbError());

  if (hash_init(&ndb_tables, system_charset_info,list.count,0,0,
                (hash_get_key)tables_get_key,0,0))
  {
    DBUG_PRINT("error", ("Failed to init HASH ndb_tables"));
    DBUG_RETURN(-1);
  }

  if (hash_init(&ok_tables, system_charset_info,32,0,0,
                (hash_get_key)tables_get_key,0,0))
  {
    DBUG_PRINT("error", ("Failed to init HASH ok_tables"));
    hash_free(&ndb_tables);
    DBUG_RETURN(-1);
  }  

  for (i= 0 ; i < list.count ; i++)
  {
    NDBDICT::List::Element& elmt= list.elements[i];
    if (IS_TMP_PREFIX(elmt.name) || IS_NDB_BLOB_PREFIX(elmt.name))
    {
      DBUG_PRINT("info", ("Skipping %s.%s in NDB", elmt.database, elmt.name));
      continue;
    }
    DBUG_PRINT("info", ("Found %s/%s in NDB", elmt.database, elmt.name));

    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, elmt.database, db))
      continue;

    // Apply wildcard to list of tables in NDB
    if (wild)
    {
      if (lower_case_table_names)
      {
        if (wild_case_compare(files_charset_info, elmt.name, wild))
          continue;
      }
      else if (wild_compare(elmt.name,wild,0))
        continue;
    }
    DBUG_PRINT("info", ("Inserting %s into ndb_tables hash", elmt.name));     
    my_hash_insert(&ndb_tables, (byte*)thd->strdup(elmt.name));
  }

  char *file_name;
  List_iterator<char> it(*files);
  List<char> delete_list;
  while ((file_name=it++))
  {
    DBUG_PRINT("info", ("%s", file_name));     
    if (hash_search(&ndb_tables, file_name, strlen(file_name)))
    {
      DBUG_PRINT("info", ("%s existed in NDB _and_ on disk ", file_name));
      // File existed in NDB and as frm file, put in ok_tables list
      my_hash_insert(&ok_tables, (byte*)file_name);
      continue;
    }
    
    // File is not in NDB, check for .ndb file with this name
    build_table_filename(name, sizeof(name), db, file_name, ha_ndb_ext, 0);
    DBUG_PRINT("info", ("Check access for %s", name));
    if (my_access(name, F_OK))
    {
      DBUG_PRINT("info", ("%s did not exist on disk", name));     
      // .ndb file did not exist on disk, another table type
      continue;
    }

    DBUG_PRINT("info", ("%s existed on disk", name));     
    // The .ndb file exists on disk, but it's not in list of tables in ndb
    // Verify that handler agrees table is gone.
    if (ndbcluster_table_exists_in_engine(thd, db, file_name) == 0)    
    {
      DBUG_PRINT("info", ("NDB says %s does not exists", file_name));     
      it.remove();
      // Put in list of tables to remove from disk
      delete_list.push_back(thd->strdup(file_name));
    }
  }

#ifdef HAVE_NDB_BINLOG
  /* setup logging to binlog for all discovered tables */
  {
    char *end, *end1= name +
      build_table_filename(name, sizeof(name), db, "", "", 0);
    for (i= 0; i < ok_tables.records; i++)
    {
      file_name= (char*)hash_element(&ok_tables, i);
      end= end1 +
        tablename_to_filename(file_name, end1, sizeof(name) - (end1 - name));
      pthread_mutex_lock(&LOCK_open);
      ndbcluster_create_binlog_setup(ndb, name, end-name,
                                     db, file_name, TRUE);
      pthread_mutex_unlock(&LOCK_open);
    }
  }
#endif

  // Check for new files to discover
  DBUG_PRINT("info", ("Checking for new files to discover"));       
  List<char> create_list;
  for (i= 0 ; i < ndb_tables.records ; i++)
  {
    file_name= hash_element(&ndb_tables, i);
    if (!hash_search(&ok_tables, file_name, strlen(file_name)))
    {
      build_table_filename(name, sizeof(name), db, file_name, reg_ext, 0);
      if (my_access(name, F_OK))
      {
        DBUG_PRINT("info", ("%s must be discovered", file_name));
        // File is in list of ndb tables and not in ok_tables
        // This table need to be created
        create_list.push_back(thd->strdup(file_name));
      }
    }
  }

  // Lock mutex before deleting and creating frm files
  pthread_mutex_lock(&LOCK_open);

  if (!global_read_lock)
  {
    // Delete old files
    List_iterator_fast<char> it3(delete_list);
    while ((file_name=it3++))
    {
      DBUG_PRINT("info", ("Remove table %s/%s", db, file_name));
      // Delete the table and all related files
      TABLE_LIST table_list;
      bzero((char*) &table_list,sizeof(table_list));
      table_list.db= (char*) db;
      table_list.alias= table_list.table_name= (char*)file_name;
      (void)mysql_rm_table_part2(thd, &table_list,
                                                                 /* if_exists */ FALSE,
                                                                 /* drop_temporary */ FALSE,
                                                                 /* drop_view */ FALSE,
                                                                 /* dont_log_query*/ TRUE);
      /* Clear error message that is returned when table is deleted */
      thd->clear_error();
    }
  }

  // Create new files
  List_iterator_fast<char> it2(create_list);
  while ((file_name=it2++))
  {  
    DBUG_PRINT("info", ("Table %s need discovery", file_name));
    if (ndb_create_table_from_engine(thd, db, file_name) == 0)
      files->push_back(thd->strdup(file_name)); 
  }

  pthread_mutex_unlock(&LOCK_open);
  
  hash_free(&ok_tables);
  hash_free(&ndb_tables);
  } // extra bracket to avoid gcc 2.95.3 warning
  DBUG_RETURN(0);    
}


/*
  Initialise all gloal variables before creating 
  a NDB Cluster table handler
 */

/* Call back after cluster connect */
static int connect_callback()
{
  update_status_variables(g_ndb_cluster_connection);

  uint node_id, i= 0;
  Ndb_cluster_connection_node_iter node_iter;
  memset((void *)g_node_id_map, 0xFFFF, sizeof(g_node_id_map));
  while ((node_id= g_ndb_cluster_connection->get_next_node(node_iter)))
    g_node_id_map[node_id]= i++;

  pthread_cond_signal(&COND_ndb_util_thread);
  return 0;
}

extern int ndb_dictionary_is_mysqld;

static int ndbcluster_init()
{
  int res;
  DBUG_ENTER("ndbcluster_init");

  ndb_dictionary_is_mysqld= 1;

  {
    handlerton &h= ndbcluster_hton;
    h.state=            have_ndbcluster;
    h.db_type=          DB_TYPE_NDBCLUSTER;
    h.close_connection= ndbcluster_close_connection;
    h.commit=           ndbcluster_commit;
    h.rollback=         ndbcluster_rollback;
    h.create=           ndbcluster_create_handler; /* Create a new handler */
    h.drop_database=    ndbcluster_drop_database;  /* Drop a database */
    h.panic=            ndbcluster_end;            /* Panic call */
    h.show_status=      ndbcluster_show_status;    /* Show status */
    h.alter_tablespace= ndbcluster_alter_tablespace;    /* Show status */
    h.partition_flags=  ndbcluster_partition_flags; /* Partition flags */
    h.alter_table_flags=ndbcluster_alter_table_flags; /* Alter table flags */
    h.fill_files_table= ndbcluster_fill_files_table;
#ifdef HAVE_NDB_BINLOG
    ndbcluster_binlog_init_handlerton();
#endif
    h.flags=            HTON_CAN_RECREATE | HTON_TEMPORARY_NOT_SUPPORTED;
  }

  if (have_ndbcluster != SHOW_OPTION_YES)
    DBUG_RETURN(0); // nothing else to do

  // Set connectstring if specified
  if (opt_ndbcluster_connectstring != 0)
    DBUG_PRINT("connectstring", ("%s", opt_ndbcluster_connectstring));     
  if ((g_ndb_cluster_connection=
       new Ndb_cluster_connection(opt_ndbcluster_connectstring)) == 0)
  {
    DBUG_PRINT("error",("Ndb_cluster_connection(%s)",
                        opt_ndbcluster_connectstring));
    goto ndbcluster_init_error;
  }
  {
    char buf[128];
    my_snprintf(buf, sizeof(buf), "mysqld --server-id=%d", server_id);
    g_ndb_cluster_connection->set_name(buf);
  }
  g_ndb_cluster_connection->set_optimized_node_selection
    (opt_ndb_optimized_node_selection);

  // Create a Ndb object to open the connection  to NDB
  if ( (g_ndb= new Ndb(g_ndb_cluster_connection, "sys")) == 0 )
  {
    DBUG_PRINT("error", ("failed to create global ndb object"));
    goto ndbcluster_init_error;
  }
  if (g_ndb->init() != 0)
  {
    ERR_PRINT (g_ndb->getNdbError());
    goto ndbcluster_init_error;
  }

  if ((res= g_ndb_cluster_connection->connect(0,0,0)) == 0)
  {
    connect_callback();
    DBUG_PRINT("info",("NDBCLUSTER storage engine at %s on port %d",
                       g_ndb_cluster_connection->get_connected_host(),
                       g_ndb_cluster_connection->get_connected_port()));
    g_ndb_cluster_connection->wait_until_ready(10,3);
  } 
  else if (res == 1)
  {
    if (g_ndb_cluster_connection->start_connect_thread(connect_callback)) 
    {
      DBUG_PRINT("error", ("g_ndb_cluster_connection->start_connect_thread()"));
      goto ndbcluster_init_error;
    }
#ifndef DBUG_OFF
    {
      char buf[1024];
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine not started, "
                  "will connect using %s",
                  g_ndb_cluster_connection->
                  get_connectstring(buf,sizeof(buf))));
    }
#endif
  }
  else
  {
    DBUG_ASSERT(res == -1);
    DBUG_PRINT("error", ("permanent error"));
    goto ndbcluster_init_error;
  }
  
  (void) hash_init(&ndbcluster_open_tables,system_charset_info,32,0,0,
                   (hash_get_key) ndbcluster_get_key,0,0);
  pthread_mutex_init(&ndbcluster_mutex,MY_MUTEX_INIT_FAST);
#ifdef HAVE_NDB_BINLOG
  /* start the ndb injector thread */
  if (ndbcluster_binlog_start())
    goto ndbcluster_init_error;
#endif /* HAVE_NDB_BINLOG */

  pthread_mutex_init(&LOCK_ndb_util_thread, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_ndb_util_thread, NULL);


  // Create utility thread
  pthread_t tmp;
  if (pthread_create(&tmp, &connection_attrib, ndb_util_thread_func, 0))
  {
    DBUG_PRINT("error", ("Could not create ndb utility thread"));
    hash_free(&ndbcluster_open_tables);
    pthread_mutex_destroy(&ndbcluster_mutex);
    pthread_mutex_destroy(&LOCK_ndb_util_thread);
    pthread_cond_destroy(&COND_ndb_util_thread);
    goto ndbcluster_init_error;
  }

  ndbcluster_inited= 1;
  DBUG_RETURN(FALSE);

ndbcluster_init_error:
  if (g_ndb)
    delete g_ndb;
  g_ndb= NULL;
  if (g_ndb_cluster_connection)
    delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;
  have_ndbcluster= SHOW_OPTION_DISABLED;	// If we couldn't use handler
  DBUG_RETURN(TRUE);
}

static int ndbcluster_end(ha_panic_function type)
{
  DBUG_ENTER("ndbcluster_end");

  if (!ndbcluster_inited)
    DBUG_RETURN(0);

#ifdef HAVE_NDB_BINLOG
  {
    pthread_mutex_lock(&ndbcluster_mutex);
    while (ndbcluster_open_tables.records)
    {
      NDB_SHARE *share=
        (NDB_SHARE*) hash_element(&ndbcluster_open_tables, 0);
#ifndef DBUG_OFF
      fprintf(stderr, "NDB: table share %s with use_count %d not freed\n",
              share->key, share->use_count);
#endif
      real_free_share(&share);
    }
    pthread_mutex_unlock(&ndbcluster_mutex);
  }
#endif
  hash_free(&ndbcluster_open_tables);

  if (g_ndb)
  {
#ifndef DBUG_OFF
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (g_ndb->get_free_list_usage(&tmp))
    {
      uint leaked= (uint) tmp.m_created - tmp.m_free;
      if (leaked)
        fprintf(stderr, "NDB: Found %u %s%s that %s not been released\n",
                leaked, tmp.m_name,
                (leaked == 1)?"":"'s",
                (leaked == 1)?"has":"have");
    }
#endif
    delete g_ndb;
    g_ndb= NULL;
  }
  delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;

  pthread_mutex_destroy(&ndbcluster_mutex);
  pthread_mutex_destroy(&LOCK_ndb_util_thread);
  pthread_cond_destroy(&COND_ndb_util_thread);
  ndbcluster_inited= 0;
  DBUG_RETURN(0);
}

void ha_ndbcluster::print_error(int error, myf errflag)
{
  DBUG_ENTER("ha_ndbcluster::print_error");
  DBUG_PRINT("enter", ("error = %d", error));

  if (error == HA_ERR_NO_PARTITION_FOUND)
    m_part_info->print_no_partition_found(table);
  else
    handler::print_error(error, errflag);
  DBUG_VOID_RETURN;
}


/*
  Static error print function called from
  static handler method ndbcluster_commit
  and ndbcluster_rollback
*/

void ndbcluster_print_error(int error, const NdbOperation *error_op)
{
  DBUG_ENTER("ndbcluster_print_error");
  TABLE_SHARE share;
  const char *tab_name= (error_op) ? error_op->getTableName() : "";
  share.db.str= (char*) "";
  share.db.length= 0;
  share.table_name.str= (char *) tab_name;
  share.table_name.length= strlen(tab_name);
  ha_ndbcluster error_handler(&share);
  error_handler.print_error(error, MYF(0));
  DBUG_VOID_RETURN;
}

/**
 * Set a given location from full pathname to database name
 *
 */
void ha_ndbcluster::set_dbname(const char *path_name, char *dbname)
{
  char *end, *ptr, *tmp_name;
  char tmp_buff[FN_REFLEN];
 
  tmp_name= tmp_buff;
  /* Scan name from the end */
  ptr= strend(path_name)-1;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  ptr--;
  end= ptr;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(tmp_name, ptr + 1, name_len);
  tmp_name[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  
  ptr= tmp_name;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
  filename_to_tablename(tmp_name, dbname, FN_REFLEN);
}

/*
  Set m_dbname from full pathname to table file
 */

void ha_ndbcluster::set_dbname(const char *path_name)
{
  set_dbname(path_name, m_dbname);
}

/**
 * Set a given location from full pathname to table file
 *
 */
void
ha_ndbcluster::set_tabname(const char *path_name, char * tabname)
{
  char *end, *ptr, *tmp_name;
  char tmp_buff[FN_REFLEN];

  tmp_name= tmp_buff;
  /* Scan name from the end */
  end= strend(path_name)-1;
  ptr= end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  ptr= tmp_name;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
  filename_to_tablename(tmp_name, tabname, FN_REFLEN);
}

/*
  Set m_tabname from full pathname to table file 
 */

void ha_ndbcluster::set_tabname(const char *path_name)
{
  set_tabname(path_name, m_tabname);
}


ha_rows 
ha_ndbcluster::records_in_range(uint inx, key_range *min_key,
                                key_range *max_key)
{
  KEY *key_info= table->key_info + inx;
  uint key_length= key_info->key_length;
  NDB_INDEX_TYPE idx_type= get_index_type(inx);  

  DBUG_ENTER("records_in_range");
  // Prevent partial read of hash indexes by returning HA_POS_ERROR
  if ((idx_type == UNIQUE_INDEX || idx_type == PRIMARY_KEY_INDEX) &&
      ((min_key && min_key->length < key_length) ||
       (max_key && max_key->length < key_length)))
    DBUG_RETURN(HA_POS_ERROR);
  
  // Read from hash index with full key
  // This is a "const" table which returns only one record!      
  if ((idx_type != ORDERED_INDEX) &&
      ((min_key && min_key->length == key_length) || 
       (max_key && max_key->length == key_length)))
    DBUG_RETURN(1);
  
  if ((idx_type == PRIMARY_KEY_ORDERED_INDEX ||
       idx_type == UNIQUE_ORDERED_INDEX ||
       idx_type == ORDERED_INDEX) &&
    m_index[inx].index_stat != NULL)
  {
    NDB_INDEX_DATA& d=m_index[inx];
    const NDBINDEX* index= d.index;
    Ndb* ndb=get_ndb();
    NdbTransaction* trans=NULL;
    NdbIndexScanOperation* op=NULL;
    int res=0;
    Uint64 rows;

    do
    {
      // We must provide approx table rows
      Uint64 table_rows=0;
      Ndb_local_table_statistics *info= m_table_info;
      if (info->records != ~(ha_rows)0 && info->records != 0)
      {
        table_rows = info->records;
        DBUG_PRINT("info", ("use info->records: %llu", table_rows));
      }
      else
      {
        Ndb_statistics stat;
        if ((res=ndb_get_table_statistics(ndb, m_table, &stat)) != 0)
          break;
        table_rows=stat.row_count;
        DBUG_PRINT("info", ("use db row_count: %llu", table_rows));
        if (table_rows == 0) {
          // Problem if autocommit=0
#ifdef ndb_get_table_statistics_uses_active_trans
          rows=0;
          break;
#endif
        }
      }

      // Define scan op for the range
      if ((trans=m_active_trans) == NULL || 
	  trans->commitStatus() != NdbTransaction::Started)
      {
        DBUG_PRINT("info", ("no active trans"));
        if (! (trans=ndb->startTransaction()))
          ERR_BREAK(ndb->getNdbError(), res);
      }
      if (! (op=trans->getNdbIndexScanOperation(index, (NDBTAB*)m_table)))
        ERR_BREAK(trans->getNdbError(), res);
      if ((op->readTuples(NdbOperation::LM_CommittedRead)) == -1)
        ERR_BREAK(op->getNdbError(), res);
      const key_range *keys[2]={ min_key, max_key };
      if ((res=set_bounds(op, inx, true, keys)) != 0)
        break;

      // Decide if db should be contacted
      int flags=0;
      if (d.index_stat_query_count < d.index_stat_cache_entries ||
          (d.index_stat_update_freq != 0 &&
           d.index_stat_query_count % d.index_stat_update_freq == 0))
      {
        DBUG_PRINT("info", ("force stat from db"));
        flags|=NdbIndexStat::RR_UseDb;
      }
      if (d.index_stat->records_in_range(index, op, table_rows, &rows, flags) == -1)
        ERR_BREAK(d.index_stat->getNdbError(), res);
      d.index_stat_query_count++;
    } while (0);

    if (trans != m_active_trans && rows == 0)
      rows = 1;
    if (trans != m_active_trans && trans != NULL)
      ndb->closeTransaction(trans);
    if (res != 0)
      DBUG_RETURN(HA_POS_ERROR);
    DBUG_RETURN(rows);
  }

  DBUG_RETURN(10); /* Good guess when you don't know anything */
}

ulonglong ha_ndbcluster::table_flags(void) const
{
  if (m_ha_not_exact_count)
    return m_table_flags & ~HA_STATS_RECORDS_IS_EXACT;
  return m_table_flags;
}
const char * ha_ndbcluster::table_type() const 
{
  return("NDBCLUSTER");
}
uint ha_ndbcluster::max_supported_record_length() const
{ 
  return NDB_MAX_TUPLE_SIZE;
}
uint ha_ndbcluster::max_supported_keys() const
{
  return MAX_KEY;
}
uint ha_ndbcluster::max_supported_key_parts() const 
{
  return NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY;
}
uint ha_ndbcluster::max_supported_key_length() const
{
  return NDB_MAX_KEY_SIZE;
}
uint ha_ndbcluster::max_supported_key_part_length() const
{
  return NDB_MAX_KEY_SIZE;
}
bool ha_ndbcluster::low_byte_first() const
{ 
#ifdef WORDS_BIGENDIAN
  return FALSE;
#else
  return TRUE;
#endif
}
const char* ha_ndbcluster::index_type(uint key_number)
{
  switch (get_index_type(key_number)) {
  case ORDERED_INDEX:
  case UNIQUE_ORDERED_INDEX:
  case PRIMARY_KEY_ORDERED_INDEX:
    return "BTREE";
  case UNIQUE_INDEX:
  case PRIMARY_KEY_INDEX:
  default:
    return "HASH";
  }
}

uint8 ha_ndbcluster::table_cache_type()
{
  DBUG_ENTER("ha_ndbcluster::table_cache_type=HA_CACHE_TBL_ASKTRANSACT");
  DBUG_RETURN(HA_CACHE_TBL_ASKTRANSACT);
}


uint ndb_get_commitcount(THD *thd, char *dbname, char *tabname,
                         Uint64 *commit_count)
{
  char name[FN_REFLEN];
  NDB_SHARE *share;
  DBUG_ENTER("ndb_get_commitcount");

  build_table_filename(name, sizeof(name), dbname, tabname, "", 0);
  DBUG_PRINT("enter", ("name: %s", name));
  pthread_mutex_lock(&ndbcluster_mutex);
  if (!(share=(NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                       (byte*) name,
                                       strlen(name))))
  {
    pthread_mutex_unlock(&ndbcluster_mutex);
    DBUG_PRINT("info", ("Table %s not found in ndbcluster_open_tables", name));
    DBUG_RETURN(1);
  }
  share->use_count++;
  pthread_mutex_unlock(&ndbcluster_mutex);

  pthread_mutex_lock(&share->mutex);
  if (ndb_cache_check_time > 0)
  {
    if (share->commit_count != 0)
    {
      *commit_count= share->commit_count;
      char buff[22];
      DBUG_PRINT("info", ("Getting commit_count: %s from share",
                          llstr(share->commit_count, buff)));
      pthread_mutex_unlock(&share->mutex);
      free_share(&share);
      DBUG_RETURN(0);
    }
  }
  DBUG_PRINT("info", ("Get commit_count from NDB"));
  Ndb *ndb;
  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(1);
  ndb->setDatabaseName(dbname);
  uint lock= share->commit_count_lock;
  pthread_mutex_unlock(&share->mutex);

  struct Ndb_statistics stat;
  {
    Ndb_table_guard ndbtab_g(ndb->getDictionary(), tabname);
    if (ndbtab_g.get_table() == 0
        || ndb_get_table_statistics(ndb, ndbtab_g.get_table(), &stat))
    {
      free_share(&share);
      DBUG_RETURN(1);
    }
  }

  pthread_mutex_lock(&share->mutex);
  if (share->commit_count_lock == lock)
  {
    char buff[22];
    DBUG_PRINT("info", ("Setting commit_count to %s",
                        llstr(stat.commit_count, buff)));
    share->commit_count= stat.commit_count;
    *commit_count= stat.commit_count;
  }
  else
  {
    DBUG_PRINT("info", ("Discarding commit_count, comit_count_lock changed"));
    *commit_count= 0;
  }
  pthread_mutex_unlock(&share->mutex);
  free_share(&share);
  DBUG_RETURN(0);
}


/*
  Check if a cached query can be used.
  This is done by comparing the supplied engine_data to commit_count of
  the table.
  The commit_count is either retrieved from the share for the table, where
  it has been cached by the util thread. If the util thread is not started,
  NDB has to be contacetd to retrieve the commit_count, this will introduce
  a small delay while waiting for NDB to answer.


  SYNOPSIS
  ndbcluster_cache_retrieval_allowed
    thd            thread handle
    full_name      concatenation of database name,
                   the null character '\0', and the table
                   name
    full_name_len  length of the full name,
                   i.e. len(dbname) + len(tablename) + 1

    engine_data    parameter retrieved when query was first inserted into
                   the cache. If the value of engine_data is changed,
                   all queries for this table should be invalidated.

  RETURN VALUE
    TRUE  Yes, use the query from cache
    FALSE No, don't use the cached query, and if engine_data
          has changed, all queries for this table should be invalidated

*/

static my_bool
ndbcluster_cache_retrieval_allowed(THD *thd,
                                   char *full_name, uint full_name_len,
                                   ulonglong *engine_data)
{
  Uint64 commit_count;
  bool is_autocommit= !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
  char *dbname= full_name;
  char *tabname= dbname+strlen(dbname)+1;
  char buff[22], buff2[22];
  DBUG_ENTER("ndbcluster_cache_retrieval_allowed");
  DBUG_PRINT("enter", ("dbname: %s, tabname: %s, is_autocommit: %d",
                       dbname, tabname, is_autocommit));

  if (!is_autocommit)
  {
    DBUG_PRINT("exit", ("No, don't use cache in transaction"));
    DBUG_RETURN(FALSE);
  }

  if (ndb_get_commitcount(thd, dbname, tabname, &commit_count))
  {
    *engine_data= 0; /* invalidate */
    DBUG_PRINT("exit", ("No, could not retrieve commit_count"));
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("info", ("*engine_data: %s, commit_count: %s",
                      llstr(*engine_data, buff), llstr(commit_count, buff2)));
  if (commit_count == 0)
  {
    *engine_data= 0; /* invalidate */
    DBUG_PRINT("exit", ("No, local commit has been performed"));
    DBUG_RETURN(FALSE);
  }
  else if (*engine_data != commit_count)
  {
    *engine_data= commit_count; /* invalidate */
     DBUG_PRINT("exit", ("No, commit_count has changed"));
     DBUG_RETURN(FALSE);
   }

  DBUG_PRINT("exit", ("OK to use cache, engine_data: %s",
                      llstr(*engine_data, buff)));
  DBUG_RETURN(TRUE);
}


/**
   Register a table for use in the query cache. Fetch the commit_count
   for the table and return it in engine_data, this will later be used
   to check if the table has changed, before the cached query is reused.

   SYNOPSIS
   ha_ndbcluster::can_query_cache_table
    thd            thread handle
    full_name      concatenation of database name,
                   the null character '\0', and the table
                   name
    full_name_len  length of the full name,
                   i.e. len(dbname) + len(tablename) + 1
    qc_engine_callback  function to be called before using cache on this table
    engine_data    out, commit_count for this table

  RETURN VALUE
    TRUE  Yes, it's ok to cahce this query
    FALSE No, don't cach the query

*/

my_bool
ha_ndbcluster::register_query_cache_table(THD *thd,
                                          char *full_name, uint full_name_len,
                                          qc_engine_callback *engine_callback,
                                          ulonglong *engine_data)
{
  Uint64 commit_count;
  char buff[22];
  bool is_autocommit= !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
  DBUG_ENTER("ha_ndbcluster::register_query_cache_table");
  DBUG_PRINT("enter",("dbname: %s, tabname: %s, is_autocommit: %d",
		      m_dbname, m_tabname, is_autocommit));

  if (!is_autocommit)
  {
    DBUG_PRINT("exit", ("Can't register table during transaction"));
    DBUG_RETURN(FALSE);
  }

  if (ndb_get_commitcount(thd, m_dbname, m_tabname, &commit_count))
  {
    *engine_data= 0;
    DBUG_PRINT("exit", ("Error, could not get commitcount"));
    DBUG_RETURN(FALSE);
  }
  *engine_data= commit_count;
  *engine_callback= ndbcluster_cache_retrieval_allowed;
  DBUG_PRINT("exit", ("commit_count: %s", llstr(commit_count, buff)));
  DBUG_RETURN(commit_count > 0);
}


/*
  Handling the shared NDB_SHARE structure that is needed to
  provide table locking.
  It's also used for sharing data with other NDB handlers
  in the same MySQL Server. There is currently not much
  data we want to or can share.
 */

static byte *ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= share->key_length;
  return (byte*) share->key;
}

#ifndef DBUG_OFF
static void dbug_print_open_tables()
{
  DBUG_ENTER("dbug_print_open_tables");
  for (uint i= 0; i < ndbcluster_open_tables.records; i++)
  {
    NDB_SHARE *share= (NDB_SHARE*) hash_element(&ndbcluster_open_tables, i);
    DBUG_PRINT("share",
               ("[%d] 0x%lx key: %s  key_length: %d",
                i, share, share->key, share->key_length));
    DBUG_PRINT("share",
               ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
                share->db, share->table_name,
                share->use_count, share->commit_count));
#ifdef HAVE_NDB_BINLOG
    if (share->table)
      DBUG_PRINT("share",
                 ("table->s->db.table_name: %s.%s",
                  share->table->s->db.str, share->table->s->table_name.str));
#endif
  }
  DBUG_VOID_RETURN;
}
#else
#define dbug_print_open_tables()
#endif

#ifdef HAVE_NDB_BINLOG
/*
  For some reason a share is still around, try to salvage the situation
  by closing all cached tables. If the share still exists, there is an
  error somewhere but only report this to the error log.  Keep this
  "trailing share" but rename it since there are still references to it
  to avoid segmentation faults.  There is a risk that the memory for
  this trailing share leaks.
  
  Must be called with previous pthread_mutex_lock(&ndbcluster_mutex)
*/
int handle_trailing_share(NDB_SHARE *share)
{
  THD *thd= current_thd;
  static ulong trailing_share_id= 0;
  DBUG_ENTER("handle_trailing_share");

  ++share->use_count;
  pthread_mutex_unlock(&ndbcluster_mutex);

  TABLE_LIST table_list;
  bzero((char*) &table_list,sizeof(table_list));
  table_list.db= share->db;
  table_list.alias= table_list.table_name= share->table_name;
  close_cached_tables(thd, 0, &table_list, TRUE);

  pthread_mutex_lock(&ndbcluster_mutex);
  if (!--share->use_count)
  {
    DBUG_PRINT("info", ("NDB_SHARE: close_cashed_tables %s freed share.",
               share->key)); 
    real_free_share(&share);
    DBUG_RETURN(0);
  }

  /*
    share still exists, if share has not been dropped by server
    release that share
  */
  if (share->state != NSS_DROPPED && !--share->use_count)
  {
    DBUG_PRINT("info", ("NDB_SHARE: %s already exists, "
                        "use_count=%d  state != NSS_DROPPED.",
                        share->key, share->use_count)); 
    real_free_share(&share);
    DBUG_RETURN(0);
  }
  DBUG_PRINT("error", ("NDB_SHARE: %s already exists  use_count=%d.",
                       share->key, share->use_count));

  sql_print_error("NDB_SHARE: %s already exists  use_count=%d."
                  " Moving away for safety, but possible memleak.",
                  share->key, share->use_count);
  dbug_print_open_tables();

  /*
    Ndb share has not been released as it should
  */
  DBUG_ASSERT(FALSE);

  /*
    This is probably an error.  We can however save the situation
    at the cost of a possible mem leak, by "renaming" the share
    - First remove from hash
  */
  hash_delete(&ndbcluster_open_tables, (byte*) share);

  /*
    now give it a new name, just a running number
    if space is not enough allocate some more
  */
  {
    const uint min_key_length= 10;
    if (share->key_length < min_key_length)
    {
      share->key= alloc_root(&share->mem_root, min_key_length + 1);
      share->key_length= min_key_length;
    }
    share->key_length=
      my_snprintf(share->key, min_key_length + 1, "#leak%d",
                  trailing_share_id++);
  }
  /* Keep it for possible the future trailing free */
  my_hash_insert(&ndbcluster_open_tables, (byte*) share);

  DBUG_RETURN(0);
}

/*
  Rename share is used during rename table.
*/
static int rename_share(NDB_SHARE *share, const char *new_key)
{
  NDB_SHARE *tmp;
  pthread_mutex_lock(&ndbcluster_mutex);
  uint new_length= (uint) strlen(new_key);
  DBUG_PRINT("rename_share", ("old_key: %s  old__length: %d",
                              share->key, share->key_length));
  if ((tmp= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                     (byte*) new_key, new_length)))
    handle_trailing_share(tmp);

  /* remove the share from hash */
  hash_delete(&ndbcluster_open_tables, (byte*) share);
  dbug_print_open_tables();

  /* save old stuff if insert should fail */
  uint old_length= share->key_length;
  char *old_key= share->key;

  /*
    now allocate and set the new key, db etc
    enough space for key, db, and table_name
  */
  share->key= alloc_root(&share->mem_root, 2 * (new_length + 1));
  strmov(share->key, new_key);
  share->key_length= new_length;

  if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
  {
    // ToDo free the allocated stuff above?
    DBUG_PRINT("error", ("rename_share: my_hash_insert %s failed",
                         share->key));
    share->key= old_key;
    share->key_length= old_length;
    if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
    {
      sql_print_error("rename_share: failed to recover %s", share->key);
      DBUG_PRINT("error", ("rename_share: my_hash_insert %s failed",
                           share->key));
    }
    dbug_print_open_tables();
    pthread_mutex_unlock(&ndbcluster_mutex);
    return -1;
  }
  dbug_print_open_tables();

  share->db= share->key + new_length + 1;
  ha_ndbcluster::set_dbname(new_key, share->db);
  share->table_name= share->db + strlen(share->db) + 1;
  ha_ndbcluster::set_tabname(new_key, share->table_name);

  DBUG_PRINT("rename_share",
             ("0x%lx key: %s  key_length: %d",
              share, share->key, share->key_length));
  DBUG_PRINT("rename_share",
             ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
              share->db, share->table_name,
              share->use_count, share->commit_count));
  if (share->table)
  {
    DBUG_PRINT("rename_share",
               ("table->s->db.table_name: %s.%s",
                share->table->s->db.str, share->table->s->table_name.str));

    if (share->op == 0)
    {
      share->table->s->db.str= share->db;
      share->table->s->db.length= strlen(share->db);
      share->table->s->table_name.str= share->table_name;
      share->table->s->table_name.length= strlen(share->table_name);
    }
  }
  /* else rename will be handled when the ALTER event comes */
  share->old_names= old_key;
  // ToDo free old_names after ALTER EVENT

  pthread_mutex_unlock(&ndbcluster_mutex);
  return 0;
}
#endif

/*
  Increase refcount on existing share.
  Always returns share and cannot fail.
*/
NDB_SHARE *ndbcluster_get_share(NDB_SHARE *share)
{
  pthread_mutex_lock(&ndbcluster_mutex);
  share->use_count++;

  dbug_print_open_tables();

  DBUG_PRINT("get_share",
             ("0x%lx key: %s  key_length: %d",
              share, share->key, share->key_length));
  DBUG_PRINT("get_share",
             ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
              share->db, share->table_name,
              share->use_count, share->commit_count));
  pthread_mutex_unlock(&ndbcluster_mutex);
  return share;
}


/*
  Get a share object for key

  Returns share for key, and increases the refcount on the share.

  create_if_not_exists == TRUE:
    creates share if it does not alreade exist
    returns 0 only due to out of memory, and then sets my_error

  create_if_not_exists == FALSE:
    returns 0 if share does not exist

  have_lock == TRUE, pthread_mutex_lock(&ndbcluster_mutex) already taken
*/

NDB_SHARE *ndbcluster_get_share(const char *key, TABLE *table,
                                bool create_if_not_exists,
                                bool have_lock)
{
  THD *thd= current_thd;
  NDB_SHARE *share;
  uint length= (uint) strlen(key);
  DBUG_ENTER("ndbcluster_get_share");
  DBUG_PRINT("enter", ("key: '%s'", key));

  if (!have_lock)
    pthread_mutex_lock(&ndbcluster_mutex);
  if (!(share= (NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                        (byte*) key,
                                        length)))
  {
    if (!create_if_not_exists)
    {
      DBUG_PRINT("error", ("get_share: %s does not exist", key));
      if (!have_lock)
        pthread_mutex_unlock(&ndbcluster_mutex);
      DBUG_RETURN(0);
    }
    if ((share= (NDB_SHARE*) my_malloc(sizeof(*share),
                                       MYF(MY_WME | MY_ZEROFILL))))
    {
      MEM_ROOT **root_ptr=
        my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
      MEM_ROOT *old_root= *root_ptr;
      init_sql_alloc(&share->mem_root, 1024, 0);
      *root_ptr= &share->mem_root; // remember to reset before return
      share->state= NSS_INITIAL;
      /* enough space for key, db, and table_name */
      share->key= alloc_root(*root_ptr, 2 * (length + 1));
      share->key_length= length;
      strmov(share->key, key);
      if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
      {
        free_root(&share->mem_root, MYF(0));
        my_free((gptr) share, 0);
        *root_ptr= old_root;
        if (!have_lock)
          pthread_mutex_unlock(&ndbcluster_mutex);
        DBUG_RETURN(0);
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
      share->commit_count= 0;
      share->commit_count_lock= 0;
      share->db= share->key + length + 1;
      ha_ndbcluster::set_dbname(key, share->db);
      share->table_name= share->db + strlen(share->db) + 1;
      ha_ndbcluster::set_tabname(key, share->table_name);
#ifdef HAVE_NDB_BINLOG
      ndbcluster_binlog_init_share(share, table);
#endif
      *root_ptr= old_root;
    }
    else
    {
      DBUG_PRINT("error", ("get_share: failed to alloc share"));
      if (!have_lock)
        pthread_mutex_unlock(&ndbcluster_mutex);
      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(*share));
      DBUG_RETURN(0);
    }
  }
  share->use_count++;

  dbug_print_open_tables();

  DBUG_PRINT("info",
             ("0x%lx key: %s  key_length: %d  key: %s",
              share, share->key, share->key_length, key));
  DBUG_PRINT("info",
             ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
              share->db, share->table_name,
              share->use_count, share->commit_count));
  if (!have_lock)
    pthread_mutex_unlock(&ndbcluster_mutex);
  DBUG_RETURN(share);
}


void ndbcluster_real_free_share(NDB_SHARE **share)
{
  DBUG_ENTER("ndbcluster_real_free_share");
  DBUG_PRINT("real_free_share",
             ("0x%lx key: %s  key_length: %d",
              (*share), (*share)->key, (*share)->key_length));
  DBUG_PRINT("real_free_share",
             ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
              (*share)->db, (*share)->table_name,
              (*share)->use_count, (*share)->commit_count));

  hash_delete(&ndbcluster_open_tables, (byte*) *share);
  thr_lock_delete(&(*share)->lock);
  pthread_mutex_destroy(&(*share)->mutex);

#ifdef HAVE_NDB_BINLOG
  if ((*share)->table)
  {
    // (*share)->table->mem_root is freed by closefrm
    closefrm((*share)->table, 0);
    // (*share)->table_share->mem_root is freed by free_table_share
    free_table_share((*share)->table_share);
#ifndef DBUG_OFF
    bzero((gptr)(*share)->table_share, sizeof(*(*share)->table_share));
    bzero((gptr)(*share)->table, sizeof(*(*share)->table));
    (*share)->table_share= 0;
    (*share)->table= 0;
#endif
  }
#endif
  free_root(&(*share)->mem_root, MYF(0));
  my_free((gptr) *share, MYF(0));
  *share= 0;

  dbug_print_open_tables();
  DBUG_VOID_RETURN;
}

/*
  decrease refcount of share
  calls real_free_share when refcount reaches 0

  have_lock == TRUE, pthread_mutex_lock(&ndbcluster_mutex) already taken
*/
void ndbcluster_free_share(NDB_SHARE **share, bool have_lock)
{
  if (!have_lock)
    pthread_mutex_lock(&ndbcluster_mutex);
  if ((*share)->util_lock == current_thd)
    (*share)->util_lock= 0;
  if (!--(*share)->use_count)
  {
    real_free_share(share);
  }
  else
  {
    dbug_print_open_tables();
    DBUG_PRINT("free_share",
               ("0x%lx key: %s  key_length: %d",
                *share, (*share)->key, (*share)->key_length));
    DBUG_PRINT("free_share",
               ("db.tablename: %s.%s  use_count: %d  commit_count: %d",
                (*share)->db, (*share)->table_name,
                (*share)->use_count, (*share)->commit_count));
  }
  if (!have_lock)
    pthread_mutex_unlock(&ndbcluster_mutex);
}


static 
int
ndb_get_table_statistics(Ndb* ndb, const NDBTAB *ndbtab,
                         struct Ndb_statistics * ndbstat)
{
  NdbTransaction* pTrans;
  NdbError error;
  int retries= 10;
  int retry_sleep= 30 * 1000; /* 30 milliseconds */
  char buff[22], buff2[22], buff3[22], buff4[22];
  DBUG_ENTER("ndb_get_table_statistics");
  DBUG_PRINT("enter", ("table: %s", ndbtab->getName()));

  DBUG_ASSERT(ndbtab != 0);

  do
  {
    Uint64 rows, commits, fixed_mem, var_mem;
    Uint32 size;
    Uint32 count= 0;
    Uint64 sum_rows= 0;
    Uint64 sum_commits= 0;
    Uint64 sum_row_size= 0;
    Uint64 sum_mem= 0;
    NdbScanOperation*pOp;
    NdbResultSet *rs;
    int check;

    if ((pTrans= ndb->startTransaction()) == NULL)
    {
      error= ndb->getNdbError();
      goto retry;
    }
      
    if ((pOp= pTrans->getNdbScanOperation(ndbtab)) == NULL)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    if (pOp->readTuples(NdbOperation::LM_CommittedRead))
    {
      error= pOp->getNdbError();
      goto retry;
    }
    
    if (pOp->interpret_exit_last_row() == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }
    
    pOp->getValue(NdbDictionary::Column::ROW_COUNT, (char*)&rows);
    pOp->getValue(NdbDictionary::Column::COMMIT_COUNT, (char*)&commits);
    pOp->getValue(NdbDictionary::Column::ROW_SIZE, (char*)&size);
    pOp->getValue(NdbDictionary::Column::FRAGMENT_FIXED_MEMORY, 
		  (char*)&fixed_mem);
    pOp->getValue(NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY, 
		  (char*)&var_mem);
    
    if (pTrans->execute(NdbTransaction::NoCommit,
                        NdbTransaction::AbortOnError,
                        TRUE) == -1)
    {
      error= pTrans->getNdbError();
      goto retry;
    }
    
    while ((check= pOp->nextResult(TRUE, TRUE)) == 0)
    {
      sum_rows+= rows;
      sum_commits+= commits;
      if (sum_row_size < size)
        sum_row_size= size;
      sum_mem+= fixed_mem + var_mem;
      count++;
    }
    
    if (check == -1)
    {
      error= pOp->getNdbError();
      goto retry;
    }

    pOp->close(TRUE);

    ndb->closeTransaction(pTrans);

    ndbstat->row_count= sum_rows;
    ndbstat->commit_count= sum_commits;
    ndbstat->row_size= sum_row_size;
    ndbstat->fragment_memory= sum_mem;

    DBUG_PRINT("exit", ("records: %s  commits: %s "
                        "row_size: %s  mem: %s count: %u",
			llstr(sum_rows, buff),
                        llstr(sum_commits, buff2),
                        llstr(sum_row_size, buff3),
                        llstr(sum_mem, buff4),
                        count));

    DBUG_RETURN(0);
retry:
    if (pTrans)
    {
      ndb->closeTransaction(pTrans);
      pTrans= NULL;
    }
    if (error.status == NdbError::TemporaryError && retries--)
    {
      my_sleep(retry_sleep);
      continue;
    }
    break;
  } while(1);
  DBUG_PRINT("exit", ("failed, error %u(%s)", error.code, error.message));
  ERR_RETURN(error);
}

/*
  Create a .ndb file to serve as a placeholder indicating 
  that the table with this name is a ndb table
*/

int ha_ndbcluster::write_ndb_file(const char *name)
{
  File file;
  bool error=1;
  char path[FN_REFLEN];
  
  DBUG_ENTER("write_ndb_file");
  DBUG_PRINT("enter", ("name: %s", name));

  (void)strxnmov(path, FN_REFLEN-1, 
                 mysql_data_home,"/",name,ha_ndb_ext,NullS);

  if ((file=my_create(path, CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    // It's an empty file
    error=0;
    my_close(file,MYF(0));
  }
  DBUG_RETURN(error);
}

void 
ha_ndbcluster::release_completed_operations(NdbTransaction *trans,
					    bool force_release)
{
  if (trans->hasBlobOperation())
  {
    /* We are reading/writing BLOB fields, 
       releasing operation records is unsafe
    */
    return;
  }
  if (!force_release)
  {
    if (get_thd_ndb(current_thd)->query_state & NDB_QUERY_MULTI_READ_RANGE)
    {
      /* We are batching reads and have not consumed all fetched
	 rows yet, releasing operation records is unsafe 
      */
      return;
    }
  }
  trans->releaseCompletedOperations();
}

int
ha_ndbcluster::read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                                      KEY_MULTI_RANGE *ranges, 
                                      uint range_count,
                                      bool sorted, 
                                      HANDLER_BUFFER *buffer)
{
  DBUG_ENTER("ha_ndbcluster::read_multi_range_first");
  m_write_op= FALSE;
  
  int res;
  KEY* key_info= table->key_info + active_index;
  NDB_INDEX_TYPE index_type= get_index_type(active_index);
  ulong reclength= table_share->reclength;
  NdbOperation* op;
  Thd_ndb *thd_ndb= get_thd_ndb(current_thd);

  if (uses_blob_value())
  {
    /**
     * blobs can't be batched currently
     */
    m_disable_multi_read= TRUE;
    DBUG_RETURN(handler::read_multi_range_first(found_range_p, 
                                                ranges, 
                                                range_count,
                                                sorted, 
                                                buffer));
  }
  thd_ndb->query_state|= NDB_QUERY_MULTI_READ_RANGE;
  m_disable_multi_read= FALSE;

  /**
   * Copy arguments into member variables
   */
  m_multi_ranges= ranges;
  multi_range_curr= ranges;
  multi_range_end= ranges+range_count;
  multi_range_sorted= sorted;
  multi_range_buffer= buffer;

  /**
   * read multi range will read ranges as follows (if not ordered)
   *
   * input    read order
   * ======   ==========
   * pk-op 1  pk-op 1
   * pk-op 2  pk-op 2
   * range 3  range (3,5) NOTE result rows will be intermixed
   * pk-op 4  pk-op 4
   * range 5
   * pk-op 6  pk-ok 6
   */   

  /**
   * Variables for loop
   */
  byte *curr= (byte*)buffer->buffer;
  byte *end_of_buffer= (byte*)buffer->buffer_end;
  NdbOperation::LockMode lm= 
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  bool need_pk = (lm == NdbOperation::LM_Read);
  const NDBTAB *tab= m_table;
  const NDBINDEX *unique_idx= m_index[active_index].unique_index;
  const NDBINDEX *idx= m_index[active_index].index; 
  const NdbOperation* lastOp= m_active_trans->getLastDefinedOperation();
  NdbIndexScanOperation* scanOp= 0;
  for (; multi_range_curr<multi_range_end && curr+reclength <= end_of_buffer; 
       multi_range_curr++)
  {
    part_id_range part_spec;
    if (m_use_partition_function)
    {
      get_partition_set(table, curr, active_index,
                        &multi_range_curr->start_key,
                        &part_spec);
      DBUG_PRINT("info", ("part_spec.start_part = %u, part_spec.end_part = %u",
                          part_spec.start_part, part_spec.end_part));
      /*
        If partition pruning has found no partition in set
        we can skip this scan
      */
      if (part_spec.start_part > part_spec.end_part)
      {
        /*
          We can skip this partition since the key won't fit into any
          partition
        */
        curr += reclength;
        multi_range_curr->range_flag |= SKIP_RANGE;
        continue;
      }
    }
    switch(index_type){
    case PRIMARY_KEY_ORDERED_INDEX:
      if (!(multi_range_curr->start_key.length == key_info->key_length &&
          multi_range_curr->start_key.flag == HA_READ_KEY_EXACT))
        goto range;
      // else fall through
    case PRIMARY_KEY_INDEX:
    {
      multi_range_curr->range_flag |= UNIQUE_RANGE;
      if ((op= m_active_trans->getNdbOperation(tab)) && 
          !op->readTuple(lm) && 
          !set_primary_key(op, multi_range_curr->start_key.key) &&
          !define_read_attrs(curr, op) &&
          (op->setAbortOption(AO_IgnoreError), TRUE) &&
          (!m_use_partition_function ||
           (op->setPartitionId(part_spec.start_part), true)))
        curr += reclength;
      else
        ERR_RETURN(op ? op->getNdbError() : m_active_trans->getNdbError());
      break;
    }
    break;
    case UNIQUE_ORDERED_INDEX:
      if (!(multi_range_curr->start_key.length == key_info->key_length &&
          multi_range_curr->start_key.flag == HA_READ_KEY_EXACT &&
          !check_null_in_key(key_info, multi_range_curr->start_key.key,
                             multi_range_curr->start_key.length)))
        goto range;
      // else fall through
    case UNIQUE_INDEX:
    {
      multi_range_curr->range_flag |= UNIQUE_RANGE;
      if ((op= m_active_trans->getNdbIndexOperation(unique_idx, tab)) && 
          !op->readTuple(lm) && 
          !set_index_key(op, key_info, multi_range_curr->start_key.key) &&
          !define_read_attrs(curr, op) &&
          (op->setAbortOption(AO_IgnoreError), TRUE))
        curr += reclength;
      else
        ERR_RETURN(op ? op->getNdbError() : m_active_trans->getNdbError());
      break;
    }
    case ORDERED_INDEX: {
  range:
      multi_range_curr->range_flag &= ~(uint)UNIQUE_RANGE;
      if (scanOp == 0)
      {
        if (m_multi_cursor)
        {
          scanOp= m_multi_cursor;
          DBUG_ASSERT(scanOp->getSorted() == sorted);
          DBUG_ASSERT(scanOp->getLockMode() == 
                      (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type));
          if (scanOp->reset_bounds(m_force_send))
            DBUG_RETURN(ndb_err(m_active_trans));
          
          end_of_buffer -= reclength;
        }
        else if ((scanOp= m_active_trans->getNdbIndexScanOperation(idx, tab)) 
                 &&!scanOp->readTuples(lm, 0, parallelism, sorted, 
				       FALSE, TRUE, need_pk)
                 &&!generate_scan_filter(m_cond_stack, scanOp)
                 &&!define_read_attrs(end_of_buffer-reclength, scanOp))
        {
          m_multi_cursor= scanOp;
          m_multi_range_cursor_result_ptr= end_of_buffer-reclength;
        }
        else
        {
          ERR_RETURN(scanOp ? scanOp->getNdbError() : 
                     m_active_trans->getNdbError());
        }
      }

      const key_range *keys[2]= { &multi_range_curr->start_key, 
                                  &multi_range_curr->end_key };
      if ((res= set_bounds(scanOp, active_index, false, keys,
                           multi_range_curr-ranges)))
        DBUG_RETURN(res);
      break;
    }
    case UNDEFINED_INDEX:
      DBUG_ASSERT(FALSE);
      DBUG_RETURN(1);
      break;
    }
  }
  
  if (multi_range_curr != multi_range_end)
  {
    /**
     * Mark that we're using entire buffer (even if might not) as
     *   we haven't read all ranges for some reason
     * This as we don't want mysqld to reuse the buffer when we read
     *   the remaining ranges
     */
    buffer->end_of_used_area= (byte*)buffer->buffer_end;
  }
  else
  {
    buffer->end_of_used_area= curr;
  }
  
  /**
   * Set first operation in multi range
   */
  m_current_multi_operation= 
    lastOp ? lastOp->next() : m_active_trans->getFirstDefinedOperation();
  if (!(res= execute_no_commit_ie(this, m_active_trans, true)))
  {
    m_multi_range_defined= multi_range_curr;
    multi_range_curr= ranges;
    m_multi_range_result_ptr= (byte*)buffer->buffer;
    DBUG_RETURN(read_multi_range_next(found_range_p));
  }
  ERR_RETURN(m_active_trans->getNdbError());
}

#if 0
#define DBUG_MULTI_RANGE(x) DBUG_PRINT("info", ("read_multi_range_next: case %d\n", x));
#else
#define DBUG_MULTI_RANGE(x)
#endif

int
ha_ndbcluster::read_multi_range_next(KEY_MULTI_RANGE ** multi_range_found_p)
{
  DBUG_ENTER("ha_ndbcluster::read_multi_range_next");
  if (m_disable_multi_read)
  {
    DBUG_MULTI_RANGE(11);
    DBUG_RETURN(handler::read_multi_range_next(multi_range_found_p));
  }
  
  int res;
  int range_no;
  ulong reclength= table_share->reclength;
  const NdbOperation* op= m_current_multi_operation;
  for (;multi_range_curr < m_multi_range_defined; multi_range_curr++)
  {
    DBUG_MULTI_RANGE(12);
    if (multi_range_curr->range_flag & SKIP_RANGE)
      continue;
    if (multi_range_curr->range_flag & UNIQUE_RANGE)
    {
      if (op->getNdbError().code == 0)
      {
        DBUG_MULTI_RANGE(13);
        goto found_next;
      }
      
      op= m_active_trans->getNextCompletedOperation(op);
      m_multi_range_result_ptr += reclength;
      continue;
    } 
    else if (m_multi_cursor && !multi_range_sorted)
    {
      DBUG_MULTI_RANGE(1);
      if ((res= fetch_next(m_multi_cursor)) == 0)
      {
        DBUG_MULTI_RANGE(2);
        range_no= m_multi_cursor->get_range_no();
        goto found;
      } 
      else
      {
        DBUG_MULTI_RANGE(14);
        goto close_scan;
      }
    }
    else if (m_multi_cursor && multi_range_sorted)
    {
      if (m_active_cursor && (res= fetch_next(m_multi_cursor)))
      {
        DBUG_MULTI_RANGE(3);
        goto close_scan;
      }
      
      range_no= m_multi_cursor->get_range_no();
      uint current_range_no= multi_range_curr - m_multi_ranges;
      if ((uint) range_no == current_range_no)
      {
        DBUG_MULTI_RANGE(4);
        // return current row
        goto found;
      }
      else if (range_no > (int)current_range_no)
      {
        DBUG_MULTI_RANGE(5);
        // wait with current row
        m_active_cursor= 0;
        continue;
      }
      else 
      {
        DBUG_MULTI_RANGE(6);
        // First fetch from cursor
        DBUG_ASSERT(range_no == -1);
        if ((res= m_multi_cursor->nextResult(true)))
        {
          DBUG_MULTI_RANGE(15);
          goto close_scan;
        }
        multi_range_curr--; // Will be increased in for-loop
        continue;
      }
    }
    else /** m_multi_cursor == 0 */
    {
      DBUG_MULTI_RANGE(7);
      /**
       * Corresponds to range 5 in example in read_multi_range_first
       */
      (void)1;
      continue;
    }
    
    DBUG_ASSERT(FALSE); // Should only get here via goto's
close_scan:
    if (res == 1)
    {
      m_multi_cursor->close(FALSE, TRUE);
      m_active_cursor= m_multi_cursor= 0;
      DBUG_MULTI_RANGE(8);
      continue;
    } 
    else 
    {
      DBUG_MULTI_RANGE(9);
      DBUG_RETURN(ndb_err(m_active_trans));
    }
  }
  
  if (multi_range_curr == multi_range_end)
  {
    DBUG_MULTI_RANGE(16);
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  
  /**
   * Read remaining ranges
   */
  DBUG_RETURN(read_multi_range_first(multi_range_found_p, 
                                     multi_range_curr,
                                     multi_range_end - multi_range_curr, 
                                     multi_range_sorted,
                                     multi_range_buffer));
  
found:
  /**
   * Found a record belonging to a scan
   */
  m_active_cursor= m_multi_cursor;
  * multi_range_found_p= m_multi_ranges + range_no;
  memcpy(table->record[0], m_multi_range_cursor_result_ptr, reclength);
  setup_recattr(m_active_cursor->getFirstRecAttr());
  unpack_record(table->record[0]);
  table->status= 0;     
  DBUG_RETURN(0);
  
found_next:
  /**
   * Found a record belonging to a pk/index op,
   *   copy result and move to next to prepare for next call
   */
  * multi_range_found_p= multi_range_curr;
  memcpy(table->record[0], m_multi_range_result_ptr, reclength);
  setup_recattr(op->getFirstRecAttr());
  unpack_record(table->record[0]);
  table->status= 0;
  
  multi_range_curr++;
  m_current_multi_operation= m_active_trans->getNextCompletedOperation(op);
  m_multi_range_result_ptr += reclength;
  DBUG_RETURN(0);
}

int
ha_ndbcluster::setup_recattr(const NdbRecAttr* curr)
{
  DBUG_ENTER("setup_recattr");

  Field **field, **end;
  NdbValue *value= m_value;
  
  end= table->field + table_share->fields;
  
  for (field= table->field; field < end; field++, value++)
  {
    if ((* value).ptr)
    {
      DBUG_ASSERT(curr != 0);
      NdbValue* val= m_value + curr->getColumn()->getColumnNo();
      DBUG_ASSERT(val->ptr);
      val->rec= curr;
      curr= curr->next();
    }
  }
  
  DBUG_RETURN(0);
}

char*
ha_ndbcluster::update_table_comment(
                                /* out: table comment + additional */
        const char*     comment)/* in:  table comment defined by user */
{
  uint length= strlen(comment);
  if (length > 64000 - 3)
  {
    return((char*)comment); /* string too long */
  }

  Ndb* ndb;
  if (!(ndb= get_ndb()))
  {
    return((char*)comment);
  }

  ndb->setDatabaseName(m_dbname);
  NDBDICT* dict= ndb->getDictionary();
  const NDBTAB* tab= m_table;
  DBUG_ASSERT(tab != NULL);

  char *str;
  const char *fmt="%s%snumber_of_replicas: %d";
  const unsigned fmt_len_plus_extra= length + strlen(fmt);
  if ((str= my_malloc(fmt_len_plus_extra, MYF(0))) == NULL)
  {
    return (char*)comment;
  }

  my_snprintf(str,fmt_len_plus_extra,fmt,comment,
              length > 0 ? " ":"",
              tab->getReplicaCount());
  return str;
}


// Utility thread main loop
pthread_handler_t ndb_util_thread_func(void *arg __attribute__((unused)))
{
  THD *thd; /* needs to be first for thread_stack */
  Ndb* ndb;
  struct timespec abstime;
  List<NDB_SHARE> util_open_tables;

  my_thread_init();
  DBUG_ENTER("ndb_util_thread");
  DBUG_PRINT("enter", ("ndb_cache_check_time: %d", ndb_cache_check_time));

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  THD_CHECK_SENTRY(thd);
  ndb= new Ndb(g_ndb_cluster_connection, "");

  pthread_detach_this_thread();
  ndb_util_thread= pthread_self();

  thd->thread_stack= (char*)&thd; /* remember where our stack is */
  if (thd->store_globals() || (ndb->init() != 0))
  {
    thd->cleanup();
    delete thd;
    delete ndb;
    DBUG_RETURN(NULL);
  }
  thd->init_for_queries();
  thd->version=refresh_version;
  thd->set_time();
  thd->main_security_ctx.host_or_ip= "";
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->main_security_ctx.master_access= ~0;
  thd->main_security_ctx.priv_user = 0;
  thd->current_stmt_binlog_row_based= TRUE;     // If in mixed mode

  /*
    wait for mysql server to start
  */
  pthread_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
    pthread_cond_wait(&COND_server_started, &LOCK_server_started);
  pthread_mutex_unlock(&LOCK_server_started);

  ndbcluster_util_inited= 1;

  /*
    Wait for cluster to start
  */
  pthread_mutex_lock(&LOCK_ndb_util_thread);
  while (!ndb_cluster_node_id && (ndbcluster_hton.slot != ~(uint)0))
  {
    /* ndb not connected yet */
    set_timespec(abstime, 1);
    pthread_cond_timedwait(&COND_ndb_util_thread,
                           &LOCK_ndb_util_thread,
                           &abstime);
    if (abort_loop)
    {
      pthread_mutex_unlock(&LOCK_ndb_util_thread);
      goto ndb_util_thread_end;
    }
  }
  pthread_mutex_unlock(&LOCK_ndb_util_thread);

  {
    Thd_ndb *thd_ndb;
    if (!(thd_ndb= ha_ndbcluster::seize_thd_ndb()))
    {
      sql_print_error("Could not allocate Thd_ndb object");
      goto ndb_util_thread_end;
    }
    set_thd_ndb(thd, thd_ndb);
    thd_ndb->options|= TNO_NO_LOG_SCHEMA_OP;
  }

#ifdef HAVE_NDB_BINLOG
  if (ndb_extra_logging && ndb_binlog_running)
    sql_print_information("NDB Binlog: Ndb tables initially read only.");
  /* create tables needed by the replication */
  ndbcluster_setup_binlog_table_shares(thd);
#else
  /*
    Get all table definitions from the storage node
  */
  ndbcluster_find_all_files(thd);
#endif

  set_timespec(abstime, 0);
  for (;!abort_loop;)
  {
    pthread_mutex_lock(&LOCK_ndb_util_thread);
    pthread_cond_timedwait(&COND_ndb_util_thread,
                           &LOCK_ndb_util_thread,
                           &abstime);
    pthread_mutex_unlock(&LOCK_ndb_util_thread);
#ifdef NDB_EXTRA_DEBUG_UTIL_THREAD
    DBUG_PRINT("ndb_util_thread", ("Started, ndb_cache_check_time: %d",
                                   ndb_cache_check_time));
#endif
    if (abort_loop)
      break; /* Shutting down server */

#ifdef HAVE_NDB_BINLOG
    /*
      Check that the apply_status_share and schema_share has been created.
      If not try to create it
    */
    if (!ndb_binlog_tables_inited)
      ndbcluster_setup_binlog_table_shares(thd);
#endif

    if (ndb_cache_check_time == 0)
    {
      /* Wake up in 1 second to check if value has changed */
      set_timespec(abstime, 1);
      continue;
    }

    /* Lock mutex and fill list with pointers to all open tables */
    NDB_SHARE *share;
    pthread_mutex_lock(&ndbcluster_mutex);
    for (uint i= 0; i < ndbcluster_open_tables.records; i++)
    {
      share= (NDB_SHARE *)hash_element(&ndbcluster_open_tables, i);
#ifdef HAVE_NDB_BINLOG
      if ((share->use_count - (int) (share->op != 0) - (int) (share->op != 0))
          <= 0)
        continue; // injector thread is the only user, skip statistics
      share->util_lock= current_thd; // Mark that util thread has lock
#endif /* HAVE_NDB_BINLOG */
      share->use_count++; /* Make sure the table can't be closed */
      DBUG_PRINT("ndb_util_thread",
                 ("Found open table[%d]: %s, use_count: %d",
                  i, share->table_name, share->use_count));

      /* Store pointer to table */
      util_open_tables.push_back(share);
    }
    pthread_mutex_unlock(&ndbcluster_mutex);

    /* Iterate through the  open files list */
    List_iterator_fast<NDB_SHARE> it(util_open_tables);
    while ((share= it++))
    {
#ifdef HAVE_NDB_BINLOG
      if ((share->use_count - (int) (share->op != 0) - (int) (share->op != 0))
          <= 1)
      {
        /*
          Util thread and injector thread is the only user, skip statistics
	*/
        free_share(&share);
        continue;
      }
#endif /* HAVE_NDB_BINLOG */
      DBUG_PRINT("ndb_util_thread",
                 ("Fetching commit count for: %s",
                  share->key));

      /* Contact NDB to get commit count for table */
      ndb->setDatabaseName(share->db);
      struct Ndb_statistics stat;

      uint lock;
      pthread_mutex_lock(&share->mutex);
      lock= share->commit_count_lock;
      pthread_mutex_unlock(&share->mutex);

      {
        Ndb_table_guard ndbtab_g(ndb->getDictionary(), share->table_name);
        if (ndbtab_g.get_table() &&
            ndb_get_table_statistics(ndb, ndbtab_g.get_table(), &stat) == 0)
        {
          char buff[22], buff2[22];
          DBUG_PRINT("ndb_util_thread",
                     ("Table: %s, commit_count: %llu, rows: %llu",
                      share->key,
                      llstr(stat.commit_count, buff),
                      llstr(stat.row_count, buff2)));
        }
        else
        {
          DBUG_PRINT("ndb_util_thread",
                     ("Error: Could not get commit count for table %s",
                      share->key));
          stat.commit_count= 0;
        }
      }

      pthread_mutex_lock(&share->mutex);
      if (share->commit_count_lock == lock)
        share->commit_count= stat.commit_count;
      pthread_mutex_unlock(&share->mutex);

      /* Decrease the use count and possibly free share */
      free_share(&share);
    }

    /* Clear the list of open tables */
    util_open_tables.empty();

    /* Calculate new time to wake up */
    int secs= 0;
    int msecs= ndb_cache_check_time;

    struct timeval tick_time;
    gettimeofday(&tick_time, 0);
    abstime.tv_sec=  tick_time.tv_sec;
    abstime.tv_nsec= tick_time.tv_usec * 1000;

    if (msecs >= 1000){
      secs=  msecs / 1000;
      msecs= msecs % 1000;
    }

    abstime.tv_sec+=  secs;
    abstime.tv_nsec+= msecs * 1000000;
    if (abstime.tv_nsec >= 1000000000) {
      abstime.tv_sec+=  1;
      abstime.tv_nsec-= 1000000000;
    }
  }
ndb_util_thread_end:
  sql_print_information("Stopping Cluster Utility thread");
  net_end(&thd->net);
  thd->cleanup();
  delete thd;
  delete ndb;
  DBUG_PRINT("exit", ("ndb_util_thread"));
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(NULL);
}

/*
  Condition pushdown
*/
/*
  Push a condition to ndbcluster storage engine for evaluation 
  during table   and index scans. The conditions will be stored on a stack
  for possibly storing several conditions. The stack can be popped
  by calling cond_pop, handler::extra(HA_EXTRA_RESET) (handler::reset())
  will clear the stack.
  The current implementation supports arbitrary AND/OR nested conditions
  with comparisons between columns and constants (including constant
  expressions and function calls) and the following comparison operators:
  =, !=, >, >=, <, <=, "is null", and "is not null".
  
  RETURN
    NULL The condition was supported and will be evaluated for each 
    row found during the scan
    cond The condition was not supported and all rows will be returned from
         the scan for evaluation (and thus not saved on stack)
*/
const 
COND* 
ha_ndbcluster::cond_push(const COND *cond) 
{ 
  DBUG_ENTER("cond_push");
  Ndb_cond_stack *ndb_cond = new Ndb_cond_stack();
  DBUG_EXECUTE("where",print_where((COND *)cond, m_tabname););
  if (m_cond_stack)
    ndb_cond->next= m_cond_stack;
  else
    ndb_cond->next= NULL;
  m_cond_stack= ndb_cond;
  
  if (serialize_cond(cond, ndb_cond))
  {
    DBUG_RETURN(NULL);
  }
  else
  {
    cond_pop();
  }
  DBUG_RETURN(cond); 
}

/*
  Pop the top condition from the condition stack of the handler instance.
*/
void 
ha_ndbcluster::cond_pop() 
{ 
  Ndb_cond_stack *ndb_cond_stack= m_cond_stack;  
  if (ndb_cond_stack)
  {
    m_cond_stack= ndb_cond_stack->next;
    delete ndb_cond_stack;
  }
}

/*
  Clear the condition stack
*/
void
ha_ndbcluster::cond_clear()
{
  DBUG_ENTER("cond_clear");
  while (m_cond_stack)
    cond_pop();

  DBUG_VOID_RETURN;
}

/*
  Serialize the item tree into a linked list represented by Ndb_cond
  for fast generation of NbdScanFilter. Adds information such as
  position of fields that is not directly available in the Item tree.
  Also checks if condition is supported.
*/
void ndb_serialize_cond(const Item *item, void *arg)
{
  Ndb_cond_traverse_context *context= (Ndb_cond_traverse_context *) arg;
  DBUG_ENTER("ndb_serialize_cond");  

  // Check if we are skipping arguments to a function to be evaluated
  if (context->skip)
  {
    DBUG_PRINT("info", ("Skiping argument %d", context->skip));
    context->skip--;
    switch (item->type()) {
    case Item::FUNC_ITEM:
    {
      Item_func *func_item= (Item_func *) item;
      context->skip+= func_item->argument_count();
      break;
    }
    case Item::INT_ITEM:
    case Item::REAL_ITEM:
    case Item::STRING_ITEM:
    case Item::VARBIN_ITEM:
    case Item::DECIMAL_ITEM:
      break;
    default:
      context->supported= FALSE;
      break;
    }
    
    DBUG_VOID_RETURN;
  }
  
  if (context->supported)
  {
    Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
    const Item_func *func_item;
    // Check if we are rewriting some unsupported function call
    if (rewrite_context &&
        (func_item= rewrite_context->func_item) &&
        rewrite_context->count++ == 0)
    {
      switch (func_item->functype()) {
      case Item_func::BETWEEN:
        /*
          Rewrite 
          <field>|<const> BETWEEN <const1>|<field1> AND <const2>|<field2>
          to <field>|<const> > <const1>|<field1> AND 
          <field>|<const> < <const2>|<field2>
          or actually in prefix format
          BEGIN(AND) GT(<field>|<const>, <const1>|<field1>), 
          LT(<field>|<const>, <const2>|<field2>), END()
        */
      case Item_func::IN_FUNC:
      {
        /*
          Rewrite <field>|<const> IN(<const1>|<field1>, <const2>|<field2>,..)
          to <field>|<const> = <const1>|<field1> OR 
          <field> = <const2>|<field2> ...
          or actually in prefix format
          BEGIN(OR) EQ(<field>|<const>, <const1><field1>), 
          EQ(<field>|<const>, <const2>|<field2>), ... END()
          Each part of the disjunction is added for each call
          to ndb_serialize_cond and end of rewrite statement 
          is wrapped in end of ndb_serialize_cond
        */
        if (context->expecting(item->type()))
        {
          // This is the <field>|<const> item, save it in the rewrite context
          rewrite_context->left_hand_item= item;
          if (item->type() == Item::FUNC_ITEM)
          {
            Item_func *func_item= (Item_func *) item;
            if (func_item->functype() == Item_func::UNKNOWN_FUNC &&
                func_item->const_item())
            {
              // Skip any arguments since we will evaluate function instead
              DBUG_PRINT("info", ("Skip until end of arguments marker"));
              context->skip= func_item->argument_count();
            }
            else
            {
              DBUG_PRINT("info", ("Found unsupported functional expression in BETWEEN|IN"));
              context->supported= FALSE;
              DBUG_VOID_RETURN;
              
            }
          }
        }
        else
        {
          // Non-supported BETWEEN|IN expression
          DBUG_PRINT("info", ("Found unexpected item of type %u in BETWEEN|IN",
                              item->type()));
          context->supported= FALSE;
          DBUG_VOID_RETURN;
        }
        break;
      }
      default:
        context->supported= FALSE;
        break;
      }
      DBUG_VOID_RETURN;
    }
    else
    {
      Ndb_cond_stack *ndb_stack= context->stack_ptr;
      Ndb_cond *prev_cond= context->cond_ptr;
      Ndb_cond *curr_cond= context->cond_ptr= new Ndb_cond();
      if (!ndb_stack->ndb_cond)
        ndb_stack->ndb_cond= curr_cond;
      curr_cond->prev= prev_cond;
      if (prev_cond) prev_cond->next= curr_cond;
    // Check if we are rewriting some unsupported function call
      if (context->rewrite_stack)
      {
        Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
        const Item_func *func_item= rewrite_context->func_item;
        switch (func_item->functype()) {
        case Item_func::BETWEEN:
        {
          /*
            Rewrite 
            <field>|<const> BETWEEN <const1>|<field1> AND <const2>|<field2>
            to <field>|<const> > <const1>|<field1> AND 
            <field>|<const> < <const2>|<field2>
            or actually in prefix format
            BEGIN(AND) GT(<field>|<const>, <const1>|<field1>), 
            LT(<field>|<const>, <const2>|<field2>), END()
          */
          if (rewrite_context->count == 2)
          {
            // Lower limit of BETWEEN
            DBUG_PRINT("info", ("GE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(Item_func::GE_FUNC, 2);
          }
          else if (rewrite_context->count == 3)
          {
            // Upper limit of BETWEEN
            DBUG_PRINT("info", ("LE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(Item_func::LE_FUNC, 2);
          }
          else
          {
            // Illegal BETWEEN expression
            DBUG_PRINT("info", ("Illegal BETWEEN expression"));
            context->supported= FALSE;
            DBUG_VOID_RETURN;
          }
          break;
        }
        case Item_func::IN_FUNC:
        {
          /*
            Rewrite <field>|<const> IN(<const1>|<field1>, <const2>|<field2>,..)
            to <field>|<const> = <const1>|<field1> OR 
            <field> = <const2>|<field2> ...
            or actually in prefix format
            BEGIN(OR) EQ(<field>|<const>, <const1><field1>), 
            EQ(<field>|<const>, <const2>|<field2>), ... END()
            Each part of the disjunction is added for each call
            to ndb_serialize_cond and end of rewrite statement 
            is wrapped in end of ndb_serialize_cond
          */
          DBUG_PRINT("info", ("EQ_FUNC"));      
          curr_cond->ndb_item= new Ndb_item(Item_func::EQ_FUNC, 2);
          break;
        }
        default:
          context->supported= FALSE;
        }
        // Handle left hand <field>|<const>
        context->rewrite_stack= NULL; // Disable rewrite mode
        context->expect_only(Item::FIELD_ITEM);
        context->expect_field_result(STRING_RESULT);
        context->expect_field_result(REAL_RESULT);
        context->expect_field_result(INT_RESULT);
        context->expect_field_result(DECIMAL_RESULT);
        context->expect(Item::INT_ITEM);
        context->expect(Item::STRING_ITEM);
        context->expect(Item::VARBIN_ITEM);
        context->expect(Item::FUNC_ITEM);
        ndb_serialize_cond(rewrite_context->left_hand_item, arg);
        context->skip= 0; // Any FUNC_ITEM expression has already been parsed
        context->rewrite_stack= rewrite_context; // Enable rewrite mode
        if (!context->supported)
          DBUG_VOID_RETURN;

        prev_cond= context->cond_ptr;
        curr_cond= context->cond_ptr= new Ndb_cond();
        prev_cond->next= curr_cond;
      }
      
      // Check for end of AND/OR expression
      if (!item)
      {
        // End marker for condition group
        DBUG_PRINT("info", ("End of condition group"));
        curr_cond->ndb_item= new Ndb_item(NDB_END_COND);
      }
      else
      {
        switch (item->type()) {
        case Item::FIELD_ITEM:
        {
          Item_field *field_item= (Item_field *) item;
          Field *field= field_item->field;
          enum_field_types type= field->type();
          /*
            Check that the field is part of the table of the handler
            instance and that we expect a field with of this result type.
          */
          if (context->table == field->table)
          {       
            const NDBTAB *tab= (const NDBTAB *) context->ndb_table;
            DBUG_PRINT("info", ("FIELD_ITEM"));
            DBUG_PRINT("info", ("table %s", tab->getName()));
            DBUG_PRINT("info", ("column %s", field->field_name));
            DBUG_PRINT("info", ("result type %d", field->result_type()));
            
            // Check that we are expecting a field and with the correct
            // result type
            if (context->expecting(Item::FIELD_ITEM) &&
                (context->expecting_field_result(field->result_type()) ||
                 // Date and year can be written as string or int
                 ((type == MYSQL_TYPE_TIME ||
                   type == MYSQL_TYPE_DATE || 
                   type == MYSQL_TYPE_YEAR ||
                   type == MYSQL_TYPE_DATETIME)
                  ? (context->expecting_field_result(STRING_RESULT) ||
                     context->expecting_field_result(INT_RESULT))
                  : true)) &&
                // Bit fields no yet supported in scan filter
                type != MYSQL_TYPE_BIT &&
                // No BLOB support in scan filter
                type != MYSQL_TYPE_TINY_BLOB &&
                type != MYSQL_TYPE_MEDIUM_BLOB &&
                type != MYSQL_TYPE_LONG_BLOB &&
                type != MYSQL_TYPE_BLOB)
            {
              const NDBCOL *col= tab->getColumn(field->field_name);
              DBUG_ASSERT(col);
              curr_cond->ndb_item= new Ndb_item(field, col->getColumnNo());
              context->dont_expect(Item::FIELD_ITEM);
              context->expect_no_field_result();
              if (context->expect_mask)
              {
                // We have not seen second argument yet
                if (type == MYSQL_TYPE_TIME ||
                    type == MYSQL_TYPE_DATE || 
                    type == MYSQL_TYPE_YEAR ||
                    type == MYSQL_TYPE_DATETIME)
                {
                  context->expect_only(Item::STRING_ITEM);
                  context->expect(Item::INT_ITEM);
                }
                else
                  switch (field->result_type()) {
                  case STRING_RESULT:
                    // Expect char string or binary string
                    context->expect_only(Item::STRING_ITEM);
                    context->expect(Item::VARBIN_ITEM);
                    context->expect_collation(field_item->collation.collation);
                    break;
                  case REAL_RESULT:
                    context->expect_only(Item::REAL_ITEM);
                    context->expect(Item::DECIMAL_ITEM);
                    context->expect(Item::INT_ITEM);
                    break;
                  case INT_RESULT:
                    context->expect_only(Item::INT_ITEM);
                    context->expect(Item::VARBIN_ITEM);
                    break;
                  case DECIMAL_RESULT:
                    context->expect_only(Item::DECIMAL_ITEM);
                    context->expect(Item::REAL_ITEM);
                    context->expect(Item::INT_ITEM);
                    break;
                  default:
                    break;
                  }    
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
                // Check that field and string constant collations are the same
                if ((field->result_type() == STRING_RESULT) &&
                    !context->expecting_collation(item->collation.collation)
                    && type != MYSQL_TYPE_TIME
                    && type != MYSQL_TYPE_DATE
                    && type != MYSQL_TYPE_YEAR
                    && type != MYSQL_TYPE_DATETIME)
                {
                  DBUG_PRINT("info", ("Found non-matching collation %s",  
                                      item->collation.collation->name)); 
                  context->supported= FALSE;                
                }
              }
              break;
            }
            else
            {
              DBUG_PRINT("info", ("Was not expecting field of type %u(%u)",
                                  field->result_type(), type));
              context->supported= FALSE;
            }
          }
          else
          {
            DBUG_PRINT("info", ("Was not expecting field from table %s (%s)",
                                context->table->s->table_name.str, 
                                field->table->s->table_name.str));
            context->supported= FALSE;
          }
          break;
        }
        case Item::FUNC_ITEM:
        {
          Item_func *func_item= (Item_func *) item;
          // Check that we expect a function or functional expression here
          if (context->expecting(Item::FUNC_ITEM) || 
              func_item->functype() == Item_func::UNKNOWN_FUNC)
            context->expect_nothing();
          else
          {
            // Did not expect function here
            context->supported= FALSE;
            break;
          }
          
          switch (func_item->functype()) {
          case Item_func::EQ_FUNC:
          {
            DBUG_PRINT("info", ("EQ_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(), 
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::NE_FUNC:
          {
            DBUG_PRINT("info", ("NE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::LT_FUNC:
          {
            DBUG_PRINT("info", ("LT_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::LE_FUNC:
          {
            DBUG_PRINT("info", ("LE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::GE_FUNC:
          {
            DBUG_PRINT("info", ("GE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::GT_FUNC:
          {
            DBUG_PRINT("info", ("GT_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::LIKE_FUNC:
          {
            DBUG_PRINT("info", ("LIKE_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect(Item::FUNC_ITEM);
            break;
          }
          case Item_func::ISNULL_FUNC:
          {
            DBUG_PRINT("info", ("ISNULL_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);      
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::ISNOTNULL_FUNC:
          {
            DBUG_PRINT("info", ("ISNOTNULL_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);     
            context->expect(Item::FIELD_ITEM);
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::NOT_FUNC:
          {
            DBUG_PRINT("info", ("NOT_FUNC"));      
            curr_cond->ndb_item= new Ndb_item(func_item->functype(),
                                              func_item);     
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::COND_ITEM);
            break;
          }
          case Item_func::BETWEEN:
          {
            DBUG_PRINT("info", ("BETWEEN, rewriting using AND"));
            Item_func_between *between_func= (Item_func_between *) func_item;
            Ndb_rewrite_context *rewrite_context= 
              new Ndb_rewrite_context(func_item);
            rewrite_context->next= context->rewrite_stack;
            context->rewrite_stack= rewrite_context;
            if (between_func->negated)
            {
              DBUG_PRINT("info", ("NOT_FUNC"));
              curr_cond->ndb_item= new Ndb_item(Item_func::NOT_FUNC, 1);
              prev_cond= curr_cond;
              curr_cond= context->cond_ptr= new Ndb_cond();
              curr_cond->prev= prev_cond;
              prev_cond->next= curr_cond;
            }
            DBUG_PRINT("info", ("COND_AND_FUNC"));
            curr_cond->ndb_item= 
              new Ndb_item(Item_func::COND_AND_FUNC, 
                           func_item->argument_count() - 1);
            context->expect_only(Item::FIELD_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FUNC_ITEM);
            break;
          }
          case Item_func::IN_FUNC:
          {
            DBUG_PRINT("info", ("IN_FUNC, rewriting using OR"));
            Item_func_in *in_func= (Item_func_in *) func_item;
            Ndb_rewrite_context *rewrite_context= 
              new Ndb_rewrite_context(func_item);
            rewrite_context->next= context->rewrite_stack;
            context->rewrite_stack= rewrite_context;
            if (in_func->negated)
            {
              DBUG_PRINT("info", ("NOT_FUNC"));
              curr_cond->ndb_item= new Ndb_item(Item_func::NOT_FUNC, 1);
              prev_cond= curr_cond;
              curr_cond= context->cond_ptr= new Ndb_cond();
              curr_cond->prev= prev_cond;
              prev_cond->next= curr_cond;
            }
            DBUG_PRINT("info", ("COND_OR_FUNC"));
            curr_cond->ndb_item= new Ndb_item(Item_func::COND_OR_FUNC, 
                                              func_item->argument_count() - 1);
            context->expect_only(Item::FIELD_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FUNC_ITEM);
            break;
          }
          case Item_func::UNKNOWN_FUNC:
          {
            DBUG_PRINT("info", ("UNKNOWN_FUNC %s", 
                                func_item->const_item()?"const":""));  
            DBUG_PRINT("info", ("result type %d", func_item->result_type()));
            if (func_item->const_item())
            {
              switch (func_item->result_type()) {
              case STRING_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::STRING_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item); 
                if (context->expect_field_result_mask)
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(STRING_RESULT);
                  context->expect_collation(func_item->collation.collation);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                  // Check that string result have correct collation
                  if (!context->expecting_collation(item->collation.collation))
                  {
                    DBUG_PRINT("info", ("Found non-matching collation %s",  
                                        item->collation.collation->name));
                    context->supported= FALSE;
                  }
                }
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case REAL_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::REAL_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
                if (context->expect_field_result_mask) 
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(REAL_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case INT_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::INT_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
                if (context->expect_field_result_mask) 
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(INT_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case DECIMAL_RESULT:
              {
                NDB_ITEM_QUALIFICATION q;
                q.value_type= Item::DECIMAL_ITEM;
                curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
                if (context->expect_field_result_mask) 
                {
                  // We have not seen the field argument yet
                  context->expect_only(Item::FIELD_ITEM);
                  context->expect_only_field_result(DECIMAL_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              default:
                break;
              }
            }
            else
              // Function does not return constant expression
              context->supported= FALSE;
            break;
          }
          default:
          {
            DBUG_PRINT("info", ("Found func_item of type %d", 
                                func_item->functype()));
            context->supported= FALSE;
          }
          }
          break;
        }
        case Item::STRING_ITEM:
          DBUG_PRINT("info", ("STRING_ITEM")); 
          if (context->expecting(Item::STRING_ITEM)) 
          {
#ifndef DBUG_OFF
            char buff[256];
            String str(buff,(uint32) sizeof(buff), system_charset_info);
            str.length(0);
            Item_string *string_item= (Item_string *) item;
            DBUG_PRINT("info", ("value \"%s\"", 
                                string_item->val_str(&str)->ptr()));
#endif
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::STRING_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);      
            if (context->expect_field_result_mask)
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(STRING_RESULT);
              context->expect_collation(item->collation.collation);
            }
            else 
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
              // Check that we are comparing with a field with same collation
              if (!context->expecting_collation(item->collation.collation))
              {
                DBUG_PRINT("info", ("Found non-matching collation %s",  
                                    item->collation.collation->name));
                context->supported= FALSE;
              }
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::INT_ITEM:
          DBUG_PRINT("info", ("INT_ITEM"));
          if (context->expecting(Item::INT_ITEM)) 
          {
            Item_int *int_item= (Item_int *) item;      
            DBUG_PRINT("info", ("value %d", int_item->value));
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::INT_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
            if (context->expect_field_result_mask) 
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(INT_RESULT);
              context->expect_field_result(REAL_RESULT);
              context->expect_field_result(DECIMAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::REAL_ITEM:
          DBUG_PRINT("info", ("REAL_ITEM %s"));
          if (context->expecting(Item::REAL_ITEM)) 
          {
            Item_float *float_item= (Item_float *) item;      
            DBUG_PRINT("info", ("value %f", float_item->value));
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::REAL_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
            if (context->expect_field_result_mask) 
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(REAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::VARBIN_ITEM:
          DBUG_PRINT("info", ("VARBIN_ITEM"));
          if (context->expecting(Item::VARBIN_ITEM)) 
          {
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::VARBIN_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);      
            if (context->expect_field_result_mask)
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(STRING_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::DECIMAL_ITEM:
          DBUG_PRINT("info", ("DECIMAL_ITEM %s"));
          if (context->expecting(Item::DECIMAL_ITEM)) 
          {
            Item_decimal *decimal_item= (Item_decimal *) item;      
            DBUG_PRINT("info", ("value %f", decimal_item->val_real()));
            NDB_ITEM_QUALIFICATION q;
            q.value_type= Item::DECIMAL_ITEM;
            curr_cond->ndb_item= new Ndb_item(NDB_VALUE, q, item);
            if (context->expect_field_result_mask) 
            {
              // We have not seen the field argument yet
              context->expect_only(Item::FIELD_ITEM);
              context->expect_only_field_result(REAL_RESULT);
              context->expect_field_result(DECIMAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= FALSE;
          break;
        case Item::COND_ITEM:
        {
          Item_cond *cond_item= (Item_cond *) item;
          
          if (context->expecting(Item::COND_ITEM))
          {
            switch (cond_item->functype()) {
            case Item_func::COND_AND_FUNC:
              DBUG_PRINT("info", ("COND_AND_FUNC"));
              curr_cond->ndb_item= new Ndb_item(cond_item->functype(),
                                                cond_item);      
              break;
            case Item_func::COND_OR_FUNC:
              DBUG_PRINT("info", ("COND_OR_FUNC"));
              curr_cond->ndb_item= new Ndb_item(cond_item->functype(),
                                                cond_item);      
              break;
            default:
              DBUG_PRINT("info", ("COND_ITEM %d", cond_item->functype()));
              context->supported= FALSE;
              break;
            }
          }
          else
          {
            /* Did not expect condition */
            context->supported= FALSE;          
          }
          break;
        }
        default:
        {
          DBUG_PRINT("info", ("Found item of type %d", item->type()));
          context->supported= FALSE;
        }
        }
      }
      if (context->supported && context->rewrite_stack)
      {
        Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
        if (rewrite_context->count == 
            rewrite_context->func_item->argument_count())
        {
          // Rewrite is done, wrap an END() at the en
          DBUG_PRINT("info", ("End of condition group"));
          prev_cond= curr_cond;
          curr_cond= context->cond_ptr= new Ndb_cond();
          curr_cond->prev= prev_cond;
          prev_cond->next= curr_cond;
          curr_cond->ndb_item= new Ndb_item(NDB_END_COND);
          // Pop rewrite stack
          context->rewrite_stack=  rewrite_context->next;
          rewrite_context->next= NULL;
          delete(rewrite_context);
        }
      }
    }
  }
 
  DBUG_VOID_RETURN;
}

bool
ha_ndbcluster::serialize_cond(const COND *cond, Ndb_cond_stack *ndb_cond)
{
  DBUG_ENTER("serialize_cond");
  Item *item= (Item *) cond;
  Ndb_cond_traverse_context context(table, (void *)m_table, ndb_cond);
  // Expect a logical expression
  context.expect(Item::FUNC_ITEM);
  context.expect(Item::COND_ITEM);
  item->traverse_cond(&ndb_serialize_cond, (void *) &context, Item::PREFIX);
  DBUG_PRINT("info", ("The pushed condition is %ssupported", (context.supported)?"":"not "));

  DBUG_RETURN(context.supported);
}

int
ha_ndbcluster::build_scan_filter_predicate(Ndb_cond * &cond, 
                                           NdbScanFilter *filter,
                                           bool negated)
{
  DBUG_ENTER("build_scan_filter_predicate");  
  switch (cond->ndb_item->type) {
  case NDB_FUNCTION:
  {
    if (!cond->next)
      break;
    Ndb_item *a= cond->next->ndb_item;
    Ndb_item *b, *field, *value= NULL;
    LINT_INIT(field);

    switch (cond->ndb_item->argument_count()) {
    case 1:
      field= 
        (a->type == NDB_FIELD)? a : NULL;
      break;
    case 2:
      if (!cond->next->next)
        break;
      b= cond->next->next->ndb_item;
      value= 
        (a->type == NDB_VALUE)? a
        : (b->type == NDB_VALUE)? b
        : NULL;
      field= 
        (a->type == NDB_FIELD)? a
        : (b->type == NDB_FIELD)? b
        : NULL;
      break;
    default:
      field= NULL; //Keep compiler happy
      DBUG_ASSERT(0);
      break;
    }
    switch ((negated) ? 
            Ndb_item::negate(cond->ndb_item->qualification.function_type)
            : cond->ndb_item->qualification.function_type) {
    case NDB_EQ_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      DBUG_PRINT("info", ("Generating EQ filter"));
      if (filter->cmp(NdbScanFilter::COND_EQ, 
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_NE_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      DBUG_PRINT("info", ("Generating NE filter"));
      if (filter->cmp(NdbScanFilter::COND_NE, 
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_LT_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating LT filter")); 
        if (filter->cmp(NdbScanFilter::COND_LT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      else
      {
        DBUG_PRINT("info", ("Generating GT filter")); 
        if (filter->cmp(NdbScanFilter::COND_GT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_LE_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating LE filter")); 
        if (filter->cmp(NdbScanFilter::COND_LE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);       
      }
      else
      {
        DBUG_PRINT("info", ("Generating GE filter")); 
        if (filter->cmp(NdbScanFilter::COND_GE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_GE_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating GE filter")); 
        if (filter->cmp(NdbScanFilter::COND_GE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      else
      {
        DBUG_PRINT("info", ("Generating LE filter")); 
        if (filter->cmp(NdbScanFilter::COND_LE, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_GT_FUNC:
    {
      if (!value || !field) break;
      // Save value in right format for the field type
      value->save_in_field(field);
      if (a == field)
      {
        DBUG_PRINT("info", ("Generating GT filter"));
        if (filter->cmp(NdbScanFilter::COND_GT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      else
      {
        DBUG_PRINT("info", ("Generating LT filter"));
        if (filter->cmp(NdbScanFilter::COND_LT, 
                        field->get_field_no(),
                        field->get_val(),
                        field->pack_length()) == -1)
          DBUG_RETURN(1);
      }
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_LIKE_FUNC:
    {
      if (!value || !field) break;
      if ((value->qualification.value_type != Item::STRING_ITEM) &&
          (value->qualification.value_type != Item::VARBIN_ITEM))
          break;
      // Save value in right format for the field type
      value->save_in_field(field);
      DBUG_PRINT("info", ("Generating LIKE filter: like(%d,%s,%d)", 
                          field->get_field_no(), value->get_val(), 
                          value->pack_length()));
      if (filter->cmp(NdbScanFilter::COND_LIKE, 
                      field->get_field_no(),
                      value->get_val(),
                      value->pack_length()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_NOTLIKE_FUNC:
    {
      if (!value || !field) break;
      if ((value->qualification.value_type != Item::STRING_ITEM) &&
          (value->qualification.value_type != Item::VARBIN_ITEM))
          break;
      // Save value in right format for the field type
      value->save_in_field(field);
      DBUG_PRINT("info", ("Generating NOTLIKE filter: notlike(%d,%s,%d)", 
                          field->get_field_no(), value->get_val(), 
                          value->pack_length()));
      if (filter->cmp(NdbScanFilter::COND_NOT_LIKE, 
                      field->get_field_no(),
                      value->get_val(),
                      value->pack_length()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next->next;
      DBUG_RETURN(0);
    }
    case NDB_ISNULL_FUNC:
      if (!field)
        break;
      DBUG_PRINT("info", ("Generating ISNULL filter"));
      if (filter->isnull(field->get_field_no()) == -1)
        DBUG_RETURN(1);
      cond= cond->next->next;
      DBUG_RETURN(0);
    case NDB_ISNOTNULL_FUNC:
    {
      if (!field)
        break;
      DBUG_PRINT("info", ("Generating ISNOTNULL filter"));
      if (filter->isnotnull(field->get_field_no()) == -1)
        DBUG_RETURN(1);         
      cond= cond->next->next;
      DBUG_RETURN(0);
    }
    default:
      break;
    }
    break;
  }
  default:
    break;
  }
  DBUG_PRINT("info", ("Found illegal condition"));
  DBUG_RETURN(1);
}

int
ha_ndbcluster::build_scan_filter_group(Ndb_cond* &cond, NdbScanFilter *filter)
{
  uint level=0;
  bool negated= FALSE;
  DBUG_ENTER("build_scan_filter_group");

  do
  {
    if (!cond)
      DBUG_RETURN(1);
    switch (cond->ndb_item->type) {
    case NDB_FUNCTION:
    {
      switch (cond->ndb_item->qualification.function_type) {
      case NDB_COND_AND_FUNC:
      {
        level++;
        DBUG_PRINT("info", ("Generating %s group %u", (negated)?"NAND":"AND",
                            level));
        if ((negated) ? filter->begin(NdbScanFilter::NAND)
            : filter->begin(NdbScanFilter::AND) == -1)
          DBUG_RETURN(1);
        negated= FALSE;
        cond= cond->next;
        break;
      }
      case NDB_COND_OR_FUNC:
      {
        level++;
        DBUG_PRINT("info", ("Generating %s group %u", (negated)?"NOR":"OR",
                            level));
        if ((negated) ? filter->begin(NdbScanFilter::NOR)
            : filter->begin(NdbScanFilter::OR) == -1)
          DBUG_RETURN(1);
        negated= FALSE;
        cond= cond->next;
        break;
      }
      case NDB_NOT_FUNC:
      {
        DBUG_PRINT("info", ("Generating negated query"));
        cond= cond->next;
        negated= TRUE;
        break;
      }
      default:
        if (build_scan_filter_predicate(cond, filter, negated))
          DBUG_RETURN(1);
        negated= FALSE;
        break;
      }
      break;
    }
    case NDB_END_COND:
      DBUG_PRINT("info", ("End of group %u", level));
      level--;
      if (cond) cond= cond->next;
      if (filter->end() == -1)
        DBUG_RETURN(1);
      if (!negated)
        break;
      // else fall through (NOT END is an illegal condition)
    default:
    {
      DBUG_PRINT("info", ("Illegal scan filter"));
    }
    }
  }  while (level > 0 || negated);
  
  DBUG_RETURN(0);
}

int
ha_ndbcluster::build_scan_filter(Ndb_cond * &cond, NdbScanFilter *filter)
{
  bool simple_cond= TRUE;
  DBUG_ENTER("build_scan_filter");  

    switch (cond->ndb_item->type) {
    case NDB_FUNCTION:
      switch (cond->ndb_item->qualification.function_type) {
      case NDB_COND_AND_FUNC:
      case NDB_COND_OR_FUNC:
        simple_cond= FALSE;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  if (simple_cond && filter->begin() == -1)
    DBUG_RETURN(1);
  if (build_scan_filter_group(cond, filter))
    DBUG_RETURN(1);
  if (simple_cond && filter->end() == -1)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

int
ha_ndbcluster::generate_scan_filter(Ndb_cond_stack *ndb_cond_stack,
                                    NdbScanOperation *op)
{
  DBUG_ENTER("generate_scan_filter");
  if (ndb_cond_stack)
  {
    DBUG_PRINT("info", ("Generating scan filter"));
    NdbScanFilter filter(op);
    bool multiple_cond= FALSE;
    // Wrap an AND group around multiple conditions
    if (ndb_cond_stack->next) {
      multiple_cond= TRUE;
      if (filter.begin() == -1)
        DBUG_RETURN(1); 
    }
    for (Ndb_cond_stack *stack= ndb_cond_stack; 
         (stack); 
         stack= stack->next)
      {
        Ndb_cond *cond= stack->ndb_cond;

        if (build_scan_filter(cond, &filter))
        {
          DBUG_PRINT("info", ("build_scan_filter failed"));
          DBUG_RETURN(1);
        }
      }
    if (multiple_cond && filter.end() == -1)
      DBUG_RETURN(1);
  }
  else
  {  
    DBUG_PRINT("info", ("Empty stack"));
  }

  DBUG_RETURN(0);
}


void 
ha_ndbcluster::release_completed_operations(NdbTransaction *trans,
					    bool force_release)
{
  if (!force_release)
  {
    if (get_thd_ndb(current_thd)->query_state & NDB_QUERY_MULTI_READ_RANGE)
    {
      /* We are batching reads and have not consumed all fetched
	 rows yet, releasing operation records is unsafe 
      */
      return;
    }
  }
  trans->releaseCompletedOperations();
}

/*
  get table space info for SHOW CREATE TABLE
*/
char* ha_ndbcluster::get_tablespace_name(THD *thd)
{
  Ndb *ndb= check_ndb_in_thd(thd);
  NDBDICT *ndbdict= ndb->getDictionary();
  NdbError ndberr;
  Uint32 id;
  ndb->setDatabaseName(m_dbname);
  const NDBTAB *ndbtab= m_table;
  DBUG_ASSERT(ndbtab != NULL);
  if (!ndbtab->getTablespace(&id))
  {
    return 0;
  }
  {
    NdbDictionary::Tablespace ts= ndbdict->getTablespace(id);
    ndberr= ndbdict->getNdbError();
    if(ndberr.classification != NdbError::NoError)
      goto err;
    return (my_strdup(ts.getName(), MYF(0)));
  }
err:
  if (ndberr.status == NdbError::TemporaryError)
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_TEMPORARY_ERRMSG, ER(ER_GET_TEMPORARY_ERRMSG),
			ndberr.code, ndberr.message, "NDB");
  else
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
			ndberr.code, ndberr.message, "NDB");
  return 0;
}

/*
  Implements the SHOW NDB STATUS command.
*/
bool
ndbcluster_show_status(THD* thd, stat_print_fn *stat_print,
                       enum ha_stat_type stat_type)
{
  char buf[IO_SIZE];
  uint buflen;
  DBUG_ENTER("ndbcluster_show_status");
  
  if (have_ndbcluster != SHOW_OPTION_YES) 
  {
    DBUG_RETURN(FALSE);
  }
  if (stat_type != HA_ENGINE_STATUS)
  {
    DBUG_RETURN(FALSE);
  }

  update_status_variables(g_ndb_cluster_connection);
  buflen=
    my_snprintf(buf, sizeof(buf),
                "cluster_node_id=%u, "
                "connected_host=%s, "
                "connected_port=%u, "
                "number_of_storage_nodes=%u, "
                "number_of_ready_storage_nodes=%u, "
                "connect_count=%u",
                ndb_cluster_node_id,
                ndb_connected_host,
                ndb_connected_port,
                ndb_number_of_storage_nodes,
                ndb_number_of_ready_storage_nodes,
                ndb_connect_count);
  if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                 STRING_WITH_LEN("connection"), buf, buflen))
    DBUG_RETURN(TRUE);

  if (get_thd_ndb(thd) && get_thd_ndb(thd)->ndb)
  {
    Ndb* ndb= (get_thd_ndb(thd))->ndb;
    Ndb::Free_list_usage tmp;
    tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      buflen=
        my_snprintf(buf, sizeof(buf),
                  "created=%u, free=%u, sizeof=%u",
                  tmp.m_created, tmp.m_free, tmp.m_sizeof);
      if (stat_print(thd, ndbcluster_hton_name, ndbcluster_hton_name_length,
                     tmp.m_name, strlen(tmp.m_name), buf, buflen))
        DBUG_RETURN(TRUE);
    }
  }
#ifdef HAVE_NDB_BINLOG
  ndbcluster_show_status_binlog(thd, stat_print, stat_type);
#endif

  DBUG_RETURN(FALSE);
}


/*
  Create a table in NDB Cluster
 */
static uint get_no_fragments(ulonglong max_rows)
{
#if MYSQL_VERSION_ID >= 50000
  uint acc_row_size= 25 + /*safety margin*/ 2;
#else
  uint acc_row_size= pk_length*4;
  /* add acc overhead */
  if (pk_length <= 8)  /* main page will set the limit */
    acc_row_size+= 25 + /*safety margin*/ 2;
  else                /* overflow page will set the limit */
    acc_row_size+= 4 + /*safety margin*/ 4;
#endif
  ulonglong acc_fragment_size= 512*1024*1024;
#if MYSQL_VERSION_ID >= 50100
  return (max_rows*acc_row_size)/acc_fragment_size+1;
#else
  return ((max_rows*acc_row_size)/acc_fragment_size+1
	  +1/*correct rounding*/)/2;
#endif
}


/*
  Routine to adjust default number of partitions to always be a multiple
  of number of nodes and never more than 4 times the number of nodes.

*/
static bool adjusted_frag_count(uint no_fragments, uint no_nodes,
                                uint &reported_frags)
{
  uint i= 0;
  reported_frags= no_nodes;
  while (reported_frags < no_fragments && ++i < 4 &&
         (reported_frags + no_nodes) < MAX_PARTITIONS) 
    reported_frags+= no_nodes;
  return (reported_frags < no_fragments);
}

int ha_ndbcluster::get_default_no_partitions(HA_CREATE_INFO *info)
{
  ha_rows max_rows, min_rows;
  if (info)
  {
    max_rows= info->max_rows;
    min_rows= info->min_rows;
  }
  else
  {
    max_rows= table_share->max_rows;
    min_rows= table_share->min_rows;
  }
  uint reported_frags;
  uint no_fragments=
    get_no_fragments(max_rows >= min_rows ? max_rows : min_rows);
  uint no_nodes= g_ndb_cluster_connection->no_db_nodes();
  if (adjusted_frag_count(no_fragments, no_nodes, reported_frags))
  {
    push_warning(current_thd,
                 MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
    "Ndb might have problems storing the max amount of rows specified");
  }
  return (int)reported_frags;
}


/*
  Set-up auto-partitioning for NDB Cluster

  SYNOPSIS
    set_auto_partitions()
    part_info                  Partition info struct to set-up
 
  RETURN VALUE
    NONE

  DESCRIPTION
    Set-up auto partitioning scheme for tables that didn't define any
    partitioning. We'll use PARTITION BY KEY() in this case which
    translates into partition by primary key if a primary key exists
    and partition by hidden key otherwise.
*/

void ha_ndbcluster::set_auto_partitions(partition_info *part_info)
{
  DBUG_ENTER("ha_ndbcluster::set_auto_partitions");
  part_info->list_of_part_fields= TRUE;
  part_info->part_type= HASH_PARTITION;
  switch (opt_ndb_distribution_id)
  {
  case ND_KEYHASH:
    part_info->linear_hash_ind= FALSE;
    break;
  case ND_LINHASH:
    part_info->linear_hash_ind= TRUE;
    break;
  }
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::set_range_data(void *tab_ref, partition_info *part_info)
{
  NDBTAB *tab= (NDBTAB*)tab_ref;
  int32 *range_data= (int32*)my_malloc(part_info->no_parts*sizeof(int32),
                                       MYF(0));
  uint i;
  int error= 0;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("set_range_data");

  if (!range_data)
  {
    mem_alloc_error(part_info->no_parts*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (i= 0; i < part_info->no_parts; i++)
  {
    longlong range_val= part_info->range_int_array[i];
    if (unsigned_flag)
      range_val-= 0x8000000000000000ULL;
    if (range_val < INT_MIN32 || range_val >= INT_MAX32)
    {
      if ((i != part_info->no_parts - 1) ||
          (range_val != LONGLONG_MAX))
      {
        my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
        error= 1;
        goto error;
      }
      range_val= INT_MAX32;
    }
    range_data[i]= (int32)range_val;
  }
  tab->setRangeListData(range_data, sizeof(int32)*part_info->no_parts);
error:
  my_free((char*)range_data, MYF(0));
  DBUG_RETURN(error);
}

int ha_ndbcluster::set_list_data(void *tab_ref, partition_info *part_info)
{
  NDBTAB *tab= (NDBTAB*)tab_ref;
  int32 *list_data= (int32*)my_malloc(part_info->no_list_values * 2
                                      * sizeof(int32), MYF(0));
  uint32 *part_id, i;
  int error= 0;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("set_list_data");

  if (!list_data)
  {
    mem_alloc_error(part_info->no_list_values*2*sizeof(int32));
    DBUG_RETURN(1);
  }
  for (i= 0; i < part_info->no_list_values; i++)
  {
    LIST_PART_ENTRY *list_entry= &part_info->list_array[i];
    longlong list_val= list_entry->list_value;
    if (unsigned_flag)
      list_val-= 0x8000000000000000ULL;
    if (list_val < INT_MIN32 || list_val > INT_MAX32)
    {
      my_error(ER_LIMITED_PART_RANGE, MYF(0), "NDB");
      error= 1;
      goto error;
    }
    list_data[2*i]= (int32)list_val;
    part_id= (uint32*)&list_data[2*i+1];
    *part_id= list_entry->partition_id;
  }
  tab->setRangeListData(list_data, 2*sizeof(int32)*part_info->no_list_values);
error:
  my_free((char*)list_data, MYF(0));
  DBUG_RETURN(error);
}

/*
  User defined partitioning set-up. We need to check how many fragments the
  user wants defined and which node groups to put those into. Later we also
  want to attach those partitions to a tablespace.

  All the functionality of the partition function, partition limits and so
  forth are entirely handled by the MySQL Server. There is one exception to
  this rule for PARTITION BY KEY where NDB handles the hash function and
  this type can thus be handled transparently also by NDB API program.
  For RANGE, HASH and LIST and subpartitioning the NDB API programs must
  implement the function to map to a partition.
*/

uint ha_ndbcluster::set_up_partition_info(partition_info *part_info,
                                          TABLE *table,
                                          void *tab_par)
{
  uint16 frag_data[MAX_PARTITIONS];
  char *ts_names[MAX_PARTITIONS];
  ulong ts_index= 0, fd_index= 0, i, j;
  NDBTAB *tab= (NDBTAB*)tab_par;
  NDBTAB::FragmentType ftype= NDBTAB::UserDefined;
  partition_element *part_elem;
  bool first= TRUE;
  uint ts_id, ts_version, part_count= 0, tot_ts_name_len;
  List_iterator<partition_element> part_it(part_info->partitions);
  int error;
  char *name_ptr;
  DBUG_ENTER("ha_ndbcluster::set_up_partition_info");

  if (part_info->part_type == HASH_PARTITION &&
      part_info->list_of_part_fields == TRUE)
  {
    Field **fields= part_info->part_field_array;

    if (part_info->linear_hash_ind)
      ftype= NDBTAB::DistrKeyLin;
    else
      ftype= NDBTAB::DistrKeyHash;

    for (i= 0; i < part_info->part_field_list.elements; i++)
    {
      NDBCOL *col= tab->getColumn(fields[i]->field_index);
      DBUG_PRINT("info",("setting dist key on %s", col->getName()));
      col->setPartitionKey(TRUE);
    }
  }
  else 
  {
    if (!current_thd->variables.new_mode)
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          ER(ER_ILLEGAL_HA_CREATE_OPTION),
                          ndbcluster_hton_name,
                          "LIST, RANGE and HASH partition disabled by default,"
                          " use --new option to enable");
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
   /*
      Create a shadow field for those tables that have user defined
      partitioning. This field stores the value of the partition
      function such that NDB can handle reorganisations of the data
      even when the MySQL Server isn't available to assist with
      calculation of the partition function value.
    */
    NDBCOL col;
    DBUG_PRINT("info", ("Generating partition func value field"));
    col.setName("$PART_FUNC_VALUE");
    col.setType(NdbDictionary::Column::Int);
    col.setLength(1);
    col.setNullable(FALSE);
    col.setPrimaryKey(FALSE);
    col.setAutoIncrement(FALSE);
    tab->addColumn(col);
    if (part_info->part_type == RANGE_PARTITION)
    {
      if ((error= set_range_data((void*)tab, part_info)))
      {
        DBUG_RETURN(error);
      }
    }
    else if (part_info->part_type == LIST_PARTITION)
    {
      if ((error= set_list_data((void*)tab, part_info)))
      {
        DBUG_RETURN(error);
      }
    }
  }
  tab->setFragmentType(ftype);
  i= 0;
  tot_ts_name_len= 0;
  do
  {
    uint ng;
    part_elem= part_it++;
    if (!part_info->is_sub_partitioned())
    {
      ng= part_elem->nodegroup_id;
      if (first && ng == UNDEF_NODEGROUP)
        ng= 0;
      ts_names[fd_index]= part_elem->tablespace_name;
      frag_data[fd_index++]= ng;
    }
    else
    {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      j= 0;
      do
      {
        part_elem= sub_it++;
        ng= part_elem->nodegroup_id;
        if (first && ng == UNDEF_NODEGROUP)
          ng= 0;
        ts_names[fd_index]= part_elem->tablespace_name;
        frag_data[fd_index++]= ng;
      } while (++j < part_info->no_subparts);
    }
    first= FALSE;
  } while (++i < part_info->no_parts);
  tab->setDefaultNoPartitionsFlag(part_info->use_default_no_partitions);
  tab->setLinearFlag(part_info->linear_hash_ind);
  {
    ha_rows max_rows= table_share->max_rows;
    ha_rows min_rows= table_share->min_rows;
    if (max_rows < min_rows)
      max_rows= min_rows;
    if (max_rows != (ha_rows)0) /* default setting, don't set fragmentation */
    {
      tab->setMaxRows(max_rows);
      tab->setMinRows(min_rows);
    }
  }
  tab->setTablespaceNames(ts_names, fd_index*sizeof(char*));
  tab->setFragmentCount(fd_index);
  tab->setFragmentData(&frag_data, fd_index*2);
  DBUG_RETURN(0);
}


bool ha_ndbcluster::check_if_incompatible_data(HA_CREATE_INFO *info,
					       uint table_changes)
{
  DBUG_ENTER("ha_ndbcluster::check_if_incompatible_data");
  uint i;
  const NDBTAB *tab= (const NDBTAB *) m_table;

  if (current_thd->variables.ndb_use_copying_alter_table)
  {
    DBUG_PRINT("info", ("On-line alter table disabled"));
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  }

  for (i= 0; i < table->s->fields; i++) 
  {
    Field *field= table->field[i];
    const NDBCOL *col= tab->getColumn(i);
    if (field->flags & FIELD_IS_RENAMED)
    {
      DBUG_PRINT("info", ("Field has been renamed, copy table"));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
    if ((field->flags & FIELD_IN_ADD_INDEX) &&
        col->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
    {
      DBUG_PRINT("info", ("add/drop index not supported for disk stored column"));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
  }
  if (table_changes != IS_EQUAL_YES)
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  
  /* Check that auto_increment value was not changed */
  if ((info->used_fields & HA_CREATE_USED_AUTO) &&
      info->auto_increment_value != 0)
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  
  /* Check that row format didn't change */
  if ((info->used_fields & HA_CREATE_USED_AUTO) &&
      get_row_type() != info->row_type)
    DBUG_RETURN(COMPATIBLE_DATA_NO);

  DBUG_RETURN(COMPATIBLE_DATA_YES);
}

bool set_up_tablespace(st_alter_tablespace *info,
                       NdbDictionary::Tablespace *ndb_ts)
{
  ndb_ts->setName(info->tablespace_name);
  ndb_ts->setExtentSize(info->extent_size);
  ndb_ts->setDefaultLogfileGroup(info->logfile_group_name);
  return false;
}

bool set_up_datafile(st_alter_tablespace *info,
                     NdbDictionary::Datafile *ndb_df)
{
  if (info->max_size > 0)
  {
    my_error(ER_TABLESPACE_AUTO_EXTEND_ERROR, MYF(0));
    return true;
  }
  ndb_df->setPath(info->data_file_name);
  ndb_df->setSize(info->initial_size);
  ndb_df->setTablespace(info->tablespace_name);
  return false;
}

bool set_up_logfile_group(st_alter_tablespace *info,
                          NdbDictionary::LogfileGroup *ndb_lg)
{
  ndb_lg->setName(info->logfile_group_name);
  ndb_lg->setUndoBufferSize(info->undo_buffer_size);
  return false;
}

bool set_up_undofile(st_alter_tablespace *info,
                     NdbDictionary::Undofile *ndb_uf)
{
  ndb_uf->setPath(info->undo_file_name);
  ndb_uf->setSize(info->initial_size);
  ndb_uf->setLogfileGroup(info->logfile_group_name);
  return false;
}

int ndbcluster_alter_tablespace(THD* thd, st_alter_tablespace *info)
{
  DBUG_ENTER("ha_ndbcluster::alter_tablespace");

  int is_tablespace= 0;
  Ndb *ndb= check_ndb_in_thd(thd);
  if (ndb == NULL)
  {
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }

  NdbError err;
  NDBDICT *dict= ndb->getDictionary();
  int error;
  const char * errmsg;
  LINT_INIT(errmsg);

  switch (info->ts_cmd_type){
  case (CREATE_TABLESPACE):
  {
    error= ER_CREATE_FILEGROUP_FAILED;
    
    NdbDictionary::Tablespace ndb_ts;
    NdbDictionary::Datafile ndb_df;
    NdbDictionary::ObjectId objid;
    if (set_up_tablespace(info, &ndb_ts))
    {
      DBUG_RETURN(1);
    }
    if (set_up_datafile(info, &ndb_df))
    {
      DBUG_RETURN(1);
    }
    errmsg= "TABLESPACE";
    if (dict->createTablespace(ndb_ts, &objid))
    {
      DBUG_PRINT("error", ("createTablespace returned %d", error));
      goto ndberror;
    }
    DBUG_PRINT("info", ("Successfully created Tablespace"));
    errmsg= "DATAFILE";
    if (dict->createDatafile(ndb_df))
    {
      err= dict->getNdbError();
      NdbDictionary::Tablespace tmp= dict->getTablespace(ndb_ts.getName());
      if (dict->getNdbError().code == 0 &&
	  tmp.getObjectId() == objid.getObjectId() &&
	  tmp.getObjectVersion() == objid.getObjectVersion())
      {
	dict->dropTablespace(tmp);
      }
      
      DBUG_PRINT("error", ("createDatafile returned %d", error));
      goto ndberror2;
    }
    is_tablespace= 1;
    break;
  }
  case (ALTER_TABLESPACE):
  {
    error= ER_ALTER_FILEGROUP_FAILED;
    if (info->ts_alter_tablespace_type == ALTER_TABLESPACE_ADD_FILE)
    {
      NdbDictionary::Datafile ndb_df;
      if (set_up_datafile(info, &ndb_df))
      {
	DBUG_RETURN(1);
      }
      errmsg= " CREATE DATAFILE";
      if (dict->createDatafile(ndb_df))
      {
	goto ndberror;
      }
    }
    else if(info->ts_alter_tablespace_type == ALTER_TABLESPACE_DROP_FILE)
    {
      NdbDictionary::Tablespace ts= dict->getTablespace(info->tablespace_name);
      NdbDictionary::Datafile df= dict->getDatafile(0, info->data_file_name);
      NdbDictionary::ObjectId objid;
      df.getTablespaceId(&objid);
      if (ts.getObjectId() == objid.getObjectId() && 
	  strcmp(df.getPath(), info->data_file_name) == 0)
      {
	errmsg= " DROP DATAFILE";
	if (dict->dropDatafile(df))
	{
	  goto ndberror;
	}
      }
      else
      {
	DBUG_PRINT("error", ("No such datafile"));
	my_error(ER_ALTER_FILEGROUP_FAILED, MYF(0), " NO SUCH FILE");
	DBUG_RETURN(1);
      }
    }
    else
    {
      DBUG_PRINT("error", ("Unsupported alter tablespace: %d", 
			   info->ts_alter_tablespace_type));
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    is_tablespace= 1;
    break;
  }
  case (CREATE_LOGFILE_GROUP):
  {
    error= ER_CREATE_FILEGROUP_FAILED;
    NdbDictionary::LogfileGroup ndb_lg;
    NdbDictionary::Undofile ndb_uf;
    NdbDictionary::ObjectId objid;
    if (info->undo_file_name == NULL)
    {
      /*
	REDO files in LOGFILE GROUP not supported yet
      */
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    if (set_up_logfile_group(info, &ndb_lg))
    {
      DBUG_RETURN(1);
    }
    errmsg= "LOGFILE GROUP";
    if (dict->createLogfileGroup(ndb_lg, &objid))
    {
      goto ndberror;
    }
    DBUG_PRINT("info", ("Successfully created Logfile Group"));
    if (set_up_undofile(info, &ndb_uf))
    {
      DBUG_RETURN(1);
    }
    errmsg= "UNDOFILE";
    if (dict->createUndofile(ndb_uf))
    {
      err= dict->getNdbError();
      NdbDictionary::LogfileGroup tmp= dict->getLogfileGroup(ndb_lg.getName());
      if (dict->getNdbError().code == 0 &&
	  tmp.getObjectId() == objid.getObjectId() &&
	  tmp.getObjectVersion() == objid.getObjectVersion())
      {
	dict->dropLogfileGroup(tmp);
      }
      goto ndberror2;
    }
    break;
  }
  case (ALTER_LOGFILE_GROUP):
  {
    error= ER_ALTER_FILEGROUP_FAILED;
    if (info->undo_file_name == NULL)
    {
      /*
	REDO files in LOGFILE GROUP not supported yet
      */
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
    }
    NdbDictionary::Undofile ndb_uf;
    if (set_up_undofile(info, &ndb_uf))
    {
      DBUG_RETURN(1);
    }
    errmsg= "CREATE UNDOFILE";
    if (dict->createUndofile(ndb_uf))
    {
      goto ndberror;
    }
    break;
  }
  case (DROP_TABLESPACE):
  {
    error= ER_DROP_FILEGROUP_FAILED;
    errmsg= "TABLESPACE";
    if (dict->dropTablespace(dict->getTablespace(info->tablespace_name)))
    {
      goto ndberror;
    }
    is_tablespace= 1;
    break;
  }
  case (DROP_LOGFILE_GROUP):
  {
    error= ER_DROP_FILEGROUP_FAILED;
    errmsg= "LOGFILE GROUP";
    if (dict->dropLogfileGroup(dict->getLogfileGroup(info->logfile_group_name)))
    {
      goto ndberror;
    }
    break;
  }
  case (CHANGE_FILE_TABLESPACE):
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  case (ALTER_ACCESS_MODE_TABLESPACE):
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  default:
  {
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  }
#ifdef HAVE_NDB_BINLOG
  if (is_tablespace)
    ndbcluster_log_schema_op(thd, 0,
                             thd->query, thd->query_length,
                             "", info->tablespace_name,
                             0, 0,
                             SOT_TABLESPACE, 0, 0, 0);
  else
    ndbcluster_log_schema_op(thd, 0,
                             thd->query, thd->query_length,
                             "", info->logfile_group_name,
                             0, 0,
                             SOT_LOGFILE_GROUP, 0, 0, 0);
#endif
  DBUG_RETURN(FALSE);

ndberror:
  err= dict->getNdbError();
ndberror2:
  ERR_PRINT(err);
  ndb_to_mysql_error(&err);
  
  my_error(error, MYF(0), errmsg);
  DBUG_RETURN(1);
}


bool ha_ndbcluster::get_no_parts(const char *name, uint *no_parts)
{
  Ndb *ndb;
  NDBDICT *dict;
  const NDBTAB *tab;
  int err;
  DBUG_ENTER("ha_ndbcluster::get_no_parts");
  LINT_INIT(err);

  set_dbname(name);
  set_tabname(name);
  for (;;)
  {
    if (check_ndb_connection())
    {
      err= HA_ERR_NO_CONNECTION;
      break;
    }
    ndb= get_ndb();
    ndb->setDatabaseName(m_dbname);
    Ndb_table_guard ndbtab_g(dict= ndb->getDictionary(), m_tabname);
    if (!ndbtab_g.get_table())
      ERR_BREAK(dict->getNdbError(), err);
    *no_parts= ndbtab_g.get_table()->getFragmentCount();
    DBUG_RETURN(FALSE);
  }

  print_error(err, MYF(0));
  DBUG_RETURN(TRUE);
}

static int ndbcluster_fill_files_table(THD *thd, TABLE_LIST *tables,
                                       COND *cond)
{
  TABLE* table= tables->table;
  Ndb *ndb= check_ndb_in_thd(thd);
  NdbDictionary::Dictionary* dict= ndb->getDictionary();
  NdbDictionary::Dictionary::List dflist;
  NdbError ndberr;
  uint i;
  DBUG_ENTER("ndbcluster_fill_files_table");

  dict->listObjects(dflist, NdbDictionary::Object::Datafile);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < dflist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt = dflist.elements[i];
    Ndb_cluster_connection_node_iter iter;
    uint id;
    
    g_ndb_cluster_connection->init_get_next_node(iter);

    while ((id= g_ndb_cluster_connection->get_next_node(iter)))
    {
      uint c= 0;
      NdbDictionary::Datafile df= dict->getDatafile(id, elt.name);
      ndberr= dict->getNdbError();
      if(ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }
      NdbDictionary::Tablespace ts= dict->getTablespace(df.getTablespace());
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }

      table->field[c++]->set_null(); // FILE_ID
      table->field[c++]->store(elt.name, strlen(elt.name),
                               system_charset_info);
      table->field[c++]->store("DATAFILE",8,system_charset_info);
      table->field[c++]->store(df.getTablespace(), strlen(df.getTablespace()),
                               system_charset_info);
      table->field[c++]->set_null(); // TABLE_CATALOG
      table->field[c++]->set_null(); // TABLE_SCHEMA
      table->field[c++]->set_null(); // TABLE_NAME

      // LOGFILE_GROUP_NAME
      table->field[c++]->store(ts.getDefaultLogfileGroup(),
                               strlen(ts.getDefaultLogfileGroup()),
                               system_charset_info);
      table->field[c++]->set_null(); // LOGFILE_GROUP_NUMBER
      table->field[c++]->store(ndbcluster_hton_name,
                               ndbcluster_hton_name_length,
                               system_charset_info); // ENGINE

      table->field[c++]->set_null(); // FULLTEXT_KEYS
      table->field[c++]->set_null(); // DELETED_ROWS
      table->field[c++]->set_null(); // UPDATE_COUNT
      table->field[c++]->store(df.getFree() / ts.getExtentSize()); // FREE_EXTENTS
      table->field[c++]->store(df.getSize() / ts.getExtentSize()); // TOTAL_EXTENTS
      table->field[c++]->store(ts.getExtentSize()); // EXTENT_SIZE

      table->field[c++]->store(df.getSize()); // INITIAL_SIZE
      table->field[c++]->store(df.getSize()); // MAXIMUM_SIZE
      table->field[c++]->set_null(); // AUTOEXTEND_SIZE

      table->field[c++]->set_null(); // CREATION_TIME
      table->field[c++]->set_null(); // LAST_UPDATE_TIME
      table->field[c++]->set_null(); // LAST_ACCESS_TIME
      table->field[c++]->set_null(); // RECOVER_TIME
      table->field[c++]->set_null(); // TRANSACTION_COUNTER

      table->field[c++]->store(df.getObjectVersion()); // VERSION

      table->field[c++]->store("FIXED", 5, system_charset_info); // ROW_FORMAT

      table->field[c++]->set_null(); // TABLE_ROWS
      table->field[c++]->set_null(); // AVG_ROW_LENGTH
      table->field[c++]->set_null(); // DATA_LENGTH
      table->field[c++]->set_null(); // MAX_DATA_LENGTH
      table->field[c++]->set_null(); // INDEX_LENGTH
      table->field[c++]->set_null(); // DATA_FREE
      table->field[c++]->set_null(); // CREATE_TIME
      table->field[c++]->set_null(); // UPDATE_TIME
      table->field[c++]->set_null(); // CHECK_TIME
      table->field[c++]->set_null(); // CHECKSUM

      table->field[c++]->store("NORMAL", 6, system_charset_info);

      char extra[30];
      int len= my_snprintf(extra, sizeof(extra), "CLUSTER_NODE=%u", id);
      table->field[c]->store(extra, len, system_charset_info);
      schema_table_store_record(thd, table);
    }
  }

  NdbDictionary::Dictionary::List uflist;
  dict->listObjects(uflist, NdbDictionary::Object::Undofile);
  ndberr= dict->getNdbError();
  if (ndberr.classification != NdbError::NoError)
    ERR_RETURN(ndberr);

  for (i= 0; i < uflist.count; i++)
  {
    NdbDictionary::Dictionary::List::Element& elt= uflist.elements[i];
    Ndb_cluster_connection_node_iter iter;
    unsigned id;

    g_ndb_cluster_connection->init_get_next_node(iter);

    while ((id= g_ndb_cluster_connection->get_next_node(iter)))
    {
      NdbDictionary::Undofile uf= dict->getUndofile(id, elt.name);
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }
      NdbDictionary::LogfileGroup lfg=
        dict->getLogfileGroup(uf.getLogfileGroup());
      ndberr= dict->getNdbError();
      if (ndberr.classification != NdbError::NoError)
      {
        if (ndberr.classification == NdbError::SchemaError)
          continue;
        ERR_RETURN(ndberr);
      }

      int c= 0;
      table->field[c++]->set_null(); // FILE_ID
      table->field[c++]->store(elt.name, strlen(elt.name),
                               system_charset_info);
      table->field[c++]->store("UNDO LOG", 8, system_charset_info);
      table->field[c++]->set_null(); // TABLESPACE NAME
      table->field[c++]->set_null(); // TABLE_CATALOG
      table->field[c++]->set_null(); // TABLE_SCHEMA
      table->field[c++]->set_null(); // TABLE_NAME

      // LOGFILE_GROUP_NAME
      NdbDictionary::ObjectId objid;
      uf.getLogfileGroupId(&objid);
      table->field[c++]->store(uf.getLogfileGroup(),
                               strlen(uf.getLogfileGroup()),
                               system_charset_info);
      table->field[c++]->store(objid.getObjectId()); // LOGFILE_GROUP_NUMBER
      table->field[c++]->store(ndbcluster_hton_name,
                               ndbcluster_hton_name_length,
                               system_charset_info); // ENGINE

      table->field[c++]->set_null(); // FULLTEXT_KEYS
      table->field[c++]->set_null(); // DELETED_ROWS
      table->field[c++]->set_null(); // UPDATE_COUNT
      table->field[c++]->store(lfg.getUndoFreeWords()); // FREE_EXTENTS
      table->field[c++]->store(uf.getSize()/4); // TOTAL_EXTENTS
      table->field[c++]->store(4); // EXTENT_SIZE

      table->field[c++]->store(uf.getSize()); // INITIAL_SIZE
      table->field[c++]->store(uf.getSize()); // MAXIMUM_SIZE
      table->field[c++]->set_null(); // AUTOEXTEND_SIZE

      table->field[c++]->set_null(); // CREATION_TIME
      table->field[c++]->set_null(); // LAST_UPDATE_TIME
      table->field[c++]->set_null(); // LAST_ACCESS_TIME
      table->field[c++]->set_null(); // RECOVER_TIME
      table->field[c++]->set_null(); // TRANSACTION_COUNTER

      table->field[c++]->store(uf.getObjectVersion()); // VERSION

      table->field[c++]->set_null(); // ROW FORMAT

      table->field[c++]->set_null(); // TABLE_ROWS
      table->field[c++]->set_null(); // AVG_ROW_LENGTH
      table->field[c++]->set_null(); // DATA_LENGTH
      table->field[c++]->set_null(); // MAX_DATA_LENGTH
      table->field[c++]->set_null(); // INDEX_LENGTH
      table->field[c++]->set_null(); // DATA_FREE
      table->field[c++]->set_null(); // CREATE_TIME
      table->field[c++]->set_null(); // UPDATE_TIME
      table->field[c++]->set_null(); // CHECK_TIME
      table->field[c++]->set_null(); // CHECKSUM

      table->field[c++]->store("NORMAL", 6, system_charset_info);

      char extra[100];
      int len= my_snprintf(extra,sizeof(extra),"CLUSTER_NODE=%u;UNDO_BUFFER_SIZE=%lu",id,lfg.getUndoBufferSize());
      table->field[c]->store(extra, len, system_charset_info);
      schema_table_store_record(thd, table);
    }
  }
  DBUG_RETURN(0);
}

struct st_mysql_storage_engine ndbcluster_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION, &ndbcluster_hton };

mysql_declare_plugin(ndbcluster)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &ndbcluster_storage_engine,
  ndbcluster_hton_name,
  "MySQL AB",
  "Clustered, fault-tolerant tables",
  ndbcluster_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  0
}
mysql_declare_plugin_end;

#endif

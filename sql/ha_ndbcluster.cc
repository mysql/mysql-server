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

#ifdef HAVE_NDBCLUSTER_DB
#include <my_dir.h>
#include "ha_ndbcluster.h"
#include <ndbapi/NdbApi.hpp>
#include <ndbapi/NdbScanFilter.hpp>

// options from from mysqld.cc
extern my_bool opt_ndb_optimized_node_selection;
extern const char *opt_ndbcluster_connectstring;

// Default value for parallelism
static const int parallelism= 0;

// Default value for max number of transactions
// createable against NDB from this handler
static const int max_transactions= 2;

static const char *ha_ndb_ext=".ndb";

static int ndbcluster_close_connection(THD *thd);
static int ndbcluster_commit(THD *thd, bool all);
static int ndbcluster_rollback(THD *thd, bool all);

handlerton ndbcluster_hton = {
  "ndbcluster",
  0, /* slot */
  0, /* savepoint size */
  ndbcluster_close_connection,
  NULL, /* savepoint_set */
  NULL, /* savepoint_rollback */
  NULL, /* savepoint_release */
  ndbcluster_commit,
  ndbcluster_rollback,
  NULL, /* prepare */
  NULL, /* recover */
  NULL, /* commit_by_xid */
  NULL, /* rollback_by_xid */
  NULL, /* create_cursor_read_view */
  NULL, /* set_cursor_read_view */
  NULL, /* close_cursor_read_view */
  HTON_NO_FLAGS
};

#define NDB_HIDDEN_PRIMARY_KEY_LENGTH 8

#define NDB_FAILED_AUTO_INCREMENT ~(Uint64)0
#define NDB_AUTO_INCREMENT_RETRIES 10

#define ERR_PRINT(err) \
  DBUG_PRINT("error", ("%d  message: %s", err.code, err.message))

#define ERR_RETURN(err)                  \
{                                        \
  const NdbError& tmp= err;              \
  ERR_PRINT(tmp);                        \
  DBUG_RETURN(ndb_to_mysql_error(&tmp)); \
}

// Typedefs for long names
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Index  NDBINDEX;
typedef NdbDictionary::Dictionary  NDBDICT;

bool ndbcluster_inited= FALSE;

static Ndb* g_ndb= NULL;
static Ndb_cluster_connection* g_ndb_cluster_connection= NULL;

// Handler synchronization
pthread_mutex_t ndbcluster_mutex;

// Table lock handling
static HASH ndbcluster_open_tables;

static byte *ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)));
static NDB_SHARE *get_share(const char *table_name);
static void free_share(NDB_SHARE *share);

static int packfrm(const void *data, uint len, const void **pack_data, uint *pack_len);
static int unpackfrm(const void **data, uint *len,
                     const void* pack_data);

static int ndb_get_table_statistics(Ndb*, const char *, 
                                    struct Ndb_statistics *);

// Util thread variables
static pthread_t ndb_util_thread;
pthread_mutex_t LOCK_ndb_util_thread;
pthread_cond_t COND_ndb_util_thread;
extern "C" pthread_handler_decl(ndb_util_thread_func, arg);
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
static long ndb_number_of_storage_nodes= 0;

static int update_status_variables(Ndb_cluster_connection *c)
{
  ndb_cluster_node_id=         c->node_id();
  ndb_connected_port=          c->get_connected_port();
  ndb_connected_host=          c->get_connected_host();
  ndb_number_of_replicas=      0;
  ndb_number_of_storage_nodes= c->no_db_nodes();
  return 0;
}

struct show_var_st ndb_status_variables[]= {
  {"cluster_node_id",        (char*) &ndb_cluster_node_id,         SHOW_LONG},
  {"connected_host",         (char*) &ndb_connected_host,      SHOW_CHAR_PTR},
  {"connected_port",         (char*) &ndb_connected_port,          SHOW_LONG},
//  {"number_of_replicas",     (char*) &ndb_number_of_replicas,      SHOW_LONG},
  {"number_of_storage_nodes",(char*) &ndb_number_of_storage_nodes, SHOW_LONG},
  {NullS, NullS, SHOW_LONG}
};

/*
  Error handling functions
*/

struct err_code_mapping
{
  int ndb_err;
  int my_err;
  int show_warning;
};

static const err_code_mapping err_map[]= 
{
  { 626, HA_ERR_KEY_NOT_FOUND, 0 },
  { 630, HA_ERR_FOUND_DUPP_KEY, 0 },
  { 893, HA_ERR_FOUND_DUPP_KEY, 0 },
  { 721, HA_ERR_TABLE_EXIST, 1 },
  { 4244, HA_ERR_TABLE_EXIST, 1 },

  { 709, HA_ERR_NO_SUCH_TABLE, 0 },

  { 266, HA_ERR_LOCK_WAIT_TIMEOUT, 1 },
  { 274, HA_ERR_LOCK_WAIT_TIMEOUT, 1 },
  { 296, HA_ERR_LOCK_WAIT_TIMEOUT, 1 },
  { 297, HA_ERR_LOCK_WAIT_TIMEOUT, 1 },
  { 237, HA_ERR_LOCK_WAIT_TIMEOUT, 1 },

  { 623, HA_ERR_RECORD_FILE_FULL, 1 },
  { 624, HA_ERR_RECORD_FILE_FULL, 1 },
  { 625, HA_ERR_RECORD_FILE_FULL, 1 },
  { 826, HA_ERR_RECORD_FILE_FULL, 1 },
  { 827, HA_ERR_RECORD_FILE_FULL, 1 },
  { 832, HA_ERR_RECORD_FILE_FULL, 1 },

  { 284, HA_ERR_TABLE_DEF_CHANGED, 0 },

  { 0, 1, 0 },

  { -1, -1, 1 }
};


static int ndb_to_mysql_error(const NdbError *err)
{
  uint i;
  for (i=0; err_map[i].ndb_err != err->code && err_map[i].my_err != -1; i++);
  if (err_map[i].show_warning)
  {
    // Push the NDB error message as warning
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_GET_ERRMSG, ER(ER_GET_ERRMSG),
                        err->code, err->message, "NDB");
  }
  if (err_map[i].my_err == -1)
    return err->code;
  return err_map[i].my_err;
}



inline
int execute_no_commit(ha_ndbcluster *h, NdbTransaction *trans)
{
#ifdef NOT_USED
  int m_batch_execute= 0;
  if (m_batch_execute)
    return 0;
#endif
  return trans->execute(NdbTransaction::NoCommit,
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
int execute_no_commit_ie(ha_ndbcluster *h, NdbTransaction *trans)
{
#ifdef NOT_USED
  int m_batch_execute= 0;
  if (m_batch_execute)
    return 0;
#endif
  return trans->execute(NdbTransaction::NoCommit,
                        NdbTransaction::AO_IgnoreError,
                        h->m_force_send);
}

/*
  Place holder for ha_ndbcluster thread specific data
*/
Thd_ndb::Thd_ndb()
{
  ndb= new Ndb(g_ndb_cluster_connection, "");
  lock_count= 0;
  count= 0;
  all= NULL;
  stmt= NULL;
  error= 0;
}

Thd_ndb::~Thd_ndb()
{
  if (ndb)
    delete ndb;
  ndb= NULL;
  changed_tables.empty();
}

inline
Thd_ndb *
get_thd_ndb(THD *thd) { return (Thd_ndb *) thd->ha_data[ndbcluster_hton.slot]; }

inline
void
set_thd_ndb(THD *thd, Thd_ndb *thd_ndb) { thd->ha_data[ndbcluster_hton.slot]= thd_ndb; }

inline
Ndb *ha_ndbcluster::get_ndb()
{
  return get_thd_ndb(current_thd)->ndb;
}

/*
 * manage uncommitted insert/deletes during transactio to get records correct
 */

struct Ndb_local_table_statistics {
  int no_uncommitted_rows_count;
  ulong last_count;
  ha_rows records;
};

void ha_ndbcluster::set_rec_per_key()
{
  DBUG_ENTER("ha_ndbcluster::get_status_const");
  for (uint i=0 ; i < table->s->keys ; i++)
  {
    table->key_info[i].rec_per_key[table->key_info[i].key_parts-1]= 1;
  }
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::records_update()
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::records_update");
  struct Ndb_local_table_statistics *info= 
    (struct Ndb_local_table_statistics *)m_table_info;
  DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                      ((const NDBTAB *)m_table)->getTableId(),
                      info->no_uncommitted_rows_count));
  //  if (info->records == ~(ha_rows)0)
  {
    Ndb *ndb= get_ndb();
    struct Ndb_statistics stat;
    if (ndb_get_table_statistics(ndb, m_tabname, &stat) == 0){
      mean_rec_length= stat.row_size;
      data_file_length= stat.fragment_memory;
      info->records= stat.row_count;
    }
  }
  {
    THD *thd= current_thd;
    if (get_thd_ndb(thd)->error)
      info->no_uncommitted_rows_count= 0;
  }
  records= info->records+ info->no_uncommitted_rows_count;
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

void ha_ndbcluster::no_uncommitted_rows_init(THD *thd)
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_init");
  struct Ndb_local_table_statistics *info= 
    (struct Ndb_local_table_statistics *)m_table_info;
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (info->last_count != thd_ndb->count)
  {
    info->last_count= thd_ndb->count;
    info->no_uncommitted_rows_count= 0;
    info->records= ~(ha_rows)0;
    DBUG_PRINT("info", ("id=%d, no_uncommitted_rows_count=%d",
                        ((const NDBTAB *)m_table)->getTableId(),
                        info->no_uncommitted_rows_count));
  }
  DBUG_VOID_RETURN;
}

void ha_ndbcluster::no_uncommitted_rows_update(int c)
{
  if (m_ha_not_exact_count)
    return;
  DBUG_ENTER("ha_ndbcluster::no_uncommitted_rows_update");
  struct Ndb_local_table_statistics *info=
    (struct Ndb_local_table_statistics *)m_table_info;
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

/*
  Take care of the error that occured in NDB

  RETURN
    0   No error
    #   The mapped error code
*/

void ha_ndbcluster::invalidate_dictionary_cache(bool global)
{
  NDBDICT *dict= get_ndb()->getDictionary();
  DBUG_ENTER("invalidate_dictionary_cache");
  DBUG_PRINT("info", ("invalidating %s", m_tabname));

  if (global)
  {
    const NDBTAB *tab= dict->getTable(m_tabname);
    if (!tab)
      DBUG_VOID_RETURN;
    if (tab->getObjectStatus() == NdbDictionary::Object::Invalid)
    {
      // Global cache has already been invalidated
      dict->removeCachedTable(m_tabname);
      global= FALSE;
    }
    else
      dict->invalidateTable(m_tabname);
  }
  else
    dict->removeCachedTable(m_tabname);
  table->s->version=0L;			/* Free when thread is ready */
  /* Invalidate indexes */
  for (uint i= 0; i < table->s->keys; i++)
  {
    NDBINDEX *index = (NDBINDEX *) m_index[i].index;
    NDBINDEX *unique_index = (NDBINDEX *) m_index[i].unique_index;
    NDB_INDEX_TYPE idx_type= m_index[i].type;

    switch (idx_type) {
    case PRIMARY_KEY_ORDERED_INDEX:
    case ORDERED_INDEX:
      if (global)
        dict->invalidateIndex(index->getName(), m_tabname);
      else
        dict->removeCachedIndex(index->getName(), m_tabname);
      break;
    case UNIQUE_ORDERED_INDEX:
      if (global)
        dict->invalidateIndex(index->getName(), m_tabname);
      else
        dict->removeCachedIndex(index->getName(), m_tabname);
    case UNIQUE_INDEX:
      if (global)
        dict->invalidateIndex(unique_index->getName(), m_tabname);
      else
        dict->removeCachedIndex(unique_index->getName(), m_tabname);
      break;
    case PRIMARY_KEY_INDEX:
    case UNDEFINED_INDEX:
      break;
    }
  }
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
    invalidate_dictionary_cache(TRUE);

    if (err.code==284)
    {
      /*
         Check if the table is _really_ gone or if the table has
         been alterend and thus changed table id
       */
      NDBDICT *dict= get_ndb()->getDictionary();
      DBUG_PRINT("info", ("Check if table %s is really gone", m_tabname));
      if (!(dict->getTable(m_tabname)))
      {
        err= dict->getNdbError();
        DBUG_PRINT("info", ("Table not found, error: %d", err.code));
        if (err.code != 709)
          DBUG_RETURN(1);
      }
      DBUG_PRINT("info", ("Table exists but must have changed"));
    }
    break;
  default:
    break;
  }
  res= ndb_to_mysql_error(&err);
  DBUG_PRINT("info", ("transformed ndbcluster error %d to mysql error %d", 
                      err.code, res));
  if (res == HA_ERR_FOUND_DUPP_KEY)
  {
    if (m_rows_to_insert == 1)
      m_dupkey= table->s->primary_key;
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
    return TRUE;
  case MYSQL_TYPE_NULL:   
  case MYSQL_TYPE_GEOMETRY:
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
  DBUG_RETURN(ndb_op->equal(fieldnr, (char*)field_ptr,
                            NDB_HIDDEN_PRIMARY_KEY_LENGTH) != 0);
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
                                 uint fieldnr, bool *set_blob_value)
{
  const byte* field_ptr= field->ptr;
  uint32 pack_len=  field->pack_length();
  DBUG_ENTER("set_ndb_value");
  DBUG_PRINT("enter", ("%d: %s, type: %u, len=%d, is_null=%s", 
                       fieldnr, field->field_name, field->type(), 
                       pack_len, field->is_null()?"Y":"N"));
  DBUG_DUMP("value", (char*) field_ptr, pack_len);

  DBUG_ASSERT(ndb_supported_type(field->type()));
  {
    // ndb currently does not support size 0
    uint32 empty_field;
    if (pack_len == 0)
    {
      pack_len= sizeof(empty_field);
      field_ptr= (byte *)&empty_field;
      if (field->is_null())
        empty_field= 0;
      else
        empty_field= 1;
    }
    if (! (field->flags & BLOB_FLAG))
    {
      if (field->type() != MYSQL_TYPE_BIT)
      {
        if (field->is_null())
          // Set value to NULL
          DBUG_RETURN((ndb_op->setValue(fieldnr, 
                                        (char*)NULL, pack_len) != 0));
        // Common implementation for most field types
        DBUG_RETURN(ndb_op->setValue(fieldnr, 
                                     (char*)field_ptr, pack_len) != 0);
      }
      else // if (field->type() == MYSQL_TYPE_BIT)
      {
        longlong bits= field->val_int();
 
        // Round up bit field length to nearest word boundry
        pack_len= ((pack_len + 3) >> 2) << 2;
        DBUG_ASSERT(pack_len <= 8);
        if (field->is_null())
          // Set value to NULL
          DBUG_RETURN((ndb_op->setValue(fieldnr, (char*)NULL, pack_len) != 0));
        DBUG_PRINT("info", ("bit field"));
        DBUG_DUMP("value", (char*)&bits, pack_len);
#ifdef WORDS_BIGENDIAN
        if (pack_len < 5)
        {
          DBUG_RETURN(ndb_op->setValue(fieldnr, 
                                       ((char*)&bits)+4, pack_len) != 0);
        }
#endif
        DBUG_RETURN(ndb_op->setValue(fieldnr, (char*)&bits, pack_len) != 0);
      }
    }
    // Blob type
    NdbBlob *ndb_blob= ndb_op->getBlobHandle(fieldnr);
    if (ndb_blob != NULL)
    {
      if (field->is_null())
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
  DBUG_RETURN(ha->get_ndb_blobs_value(ndb_blob));
}

int ha_ndbcluster::get_ndb_blobs_value(NdbBlob *last_ndb_blob)
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
      NdbValue value= m_value[i];
      if (value.ptr != NULL && (field->flags & BLOB_FLAG))
      {
        Field_blob *field_blob= (Field_blob *)field;
        NdbBlob *ndb_blob= value.blob;
        Uint64 blob_len= 0;
        if (ndb_blob->getLength(blob_len) != 0)
          DBUG_RETURN(-1);
        // Align to Uint64
        uint32 blob_size= blob_len;
        if (blob_size % 8 != 0)
          blob_size+= 8 - blob_size % 8;
        if (loop == 1)
        {
          char *buf= m_blobs_buffer + offset;
          uint32 len= 0xffffffff;  // Max uint32
          DBUG_PRINT("value", ("read blob ptr=%x len=%u",
                               (UintPtr)buf, (uint)blob_len));
          if (ndb_blob->readData(buf, len) != 0)
            DBUG_RETURN(-1);
          DBUG_ASSERT(len == blob_len);
          field_blob->set_ptr(len, buf);
        }
        offset+= blob_size;
      }
    }
    if (loop == 0 && offset > m_blobs_buffer_size)
    {
      my_free(m_blobs_buffer, MYF(MY_ALLOW_ZERO_PTR));
      m_blobs_buffer_size= 0;
      DBUG_PRINT("value", ("allocate blobs buffer size %u", offset));
      m_blobs_buffer= my_malloc(offset, MYF(MY_WME));
      if (m_blobs_buffer == NULL)
        DBUG_RETURN(-1);
      m_blobs_buffer_size= offset;
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
        void *arg= (void *)this;
        DBUG_RETURN(ndb_blob->setActiveHook(g_get_ndb_blobs_value, arg) != 0);
      }
      DBUG_RETURN(1);
  }

  // Used for hidden key only
  m_value[fieldnr].rec= ndb_op->getValue(fieldnr, NULL);
  DBUG_RETURN(m_value[fieldnr].rec == NULL);
}


/*
  Check if any set or get of blob value in current query.
*/
bool ha_ndbcluster::uses_blob_value(bool all_fields)
{
  if (table->s->blob_fields == 0)
    return FALSE;
  if (all_fields)
    return TRUE;
  {
    uint no_fields= table->s->fields;
    int i;
    THD *thd= current_thd;
    // They always put blobs at the end..
    for (i= no_fields - 1; i >= 0; i--)
    {
      Field *field= table->field[i];
      if (thd->query_id == field->query_id)
      {
        return TRUE;
      }
    }
  }
  return FALSE;
}


/*
  Get metadata for this table from NDB 

  IMPLEMENTATION
    - check that frm-file on disk is equal to frm-file
      of table accessed in NDB
*/

int ha_ndbcluster::get_metadata(const char *path)
{
  Ndb *ndb= get_ndb();
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *tab;
  int error;
  bool invalidating_ndb_table= FALSE;

  DBUG_ENTER("get_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s, path: %s", m_tabname, path));

  do {
    const void *data, *pack_data;
    uint length, pack_length;

    if (!(tab= dict->getTable(m_tabname)))
      ERR_RETURN(dict->getNdbError());
    // Check if thread has stale local cache
    if (tab->getObjectStatus() == NdbDictionary::Object::Invalid)
    {
      invalidate_dictionary_cache(FALSE);
      if (!(tab= dict->getTable(m_tabname)))
         ERR_RETURN(dict->getNdbError());
      DBUG_PRINT("info", ("Table schema version: %d", tab->getObjectVersion()));
    }
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
    
    if ((pack_length != tab->getFrmLength()) || 
        (memcmp(pack_data, tab->getFrmData(), pack_length)))
    {
      if (!invalidating_ndb_table)
      {
        DBUG_PRINT("info", ("Invalidating table"));
        invalidate_dictionary_cache(TRUE);
        invalidating_ndb_table= TRUE;
      }
      else
      {
        DBUG_PRINT("error", 
                   ("metadata, pack_length: %d getFrmLength: %d memcmp: %d", 
                    pack_length, tab->getFrmLength(),
                    memcmp(pack_data, tab->getFrmData(), pack_length)));      
        DBUG_DUMP("pack_data", (char*)pack_data, pack_length);
        DBUG_DUMP("frm", (char*)tab->getFrmData(), tab->getFrmLength());
        error= 3;
        invalidating_ndb_table= FALSE;
      }
    }
    else
    {
      invalidating_ndb_table= FALSE;
    }
    my_free((char*)data, MYF(0));
    my_free((char*)pack_data, MYF(0));
  } while (invalidating_ndb_table);

  if (error)
    DBUG_RETURN(error);
  
  m_table_version= tab->getObjectVersion();
  m_table= (void *)tab; 
  m_table_info= NULL; // Set in external lock
  
  DBUG_RETURN(build_index_list(ndb, table, ILBP_OPEN));
}

static int fix_unique_index_attr_order(NDB_INDEX_DATA &data,
                                       const NDBINDEX *index,
                                       KEY *key_info)
{
  DBUG_ENTER("fix_unique_index_attr_order");
  unsigned sz= index->getNoOfIndexColumns();

  if (data.unique_index_attrid_map)
    my_free((char*)data.unique_index_attrid_map, MYF(0));
  data.unique_index_attrid_map= (unsigned char*)my_malloc(sz,MYF(MY_WME));

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

int ha_ndbcluster::build_index_list(Ndb *ndb, TABLE *tab, enum ILBP phase)
{
  uint i;
  int error= 0;
  const char *index_name;
  char unique_index_name[FN_LEN];
  static const char* unique_suffix= "$unique";
  KEY* key_info= tab->key_info;
  const char **key_name= tab->s->keynames.type_names;
  NDBDICT *dict= ndb->getDictionary();
  DBUG_ENTER("ha_ndbcluster::build_index_list");
  
  // Save information about all known indexes
  for (i= 0; i < tab->s->keys; i++, key_info++, key_name++)
  {
    index_name= *key_name;
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_index[i].type= idx_type;
    if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
    {
      strxnmov(unique_index_name, FN_LEN, index_name, unique_suffix, NullS);
      DBUG_PRINT("info", ("Created unique index name \'%s\' for index %d",
                          unique_index_name, i));
    }
    // Create secondary indexes if in create phase
    if (phase == ILBP_CREATE)
    {
      DBUG_PRINT("info", ("Creating index %u: %s", i, index_name));      
      switch (idx_type){
        
      case PRIMARY_KEY_INDEX:
        // Do nothing, already created
        break;
      case PRIMARY_KEY_ORDERED_INDEX:
        error= create_ordered_index(index_name, key_info);
        break;
      case UNIQUE_ORDERED_INDEX:
        if (!(error= create_ordered_index(index_name, key_info)))
          error= create_unique_index(unique_index_name, key_info);
        break;
      case UNIQUE_INDEX:
        if (!(error= check_index_fields_not_null(i)))
          error= create_unique_index(unique_index_name, key_info);
        break;
      case ORDERED_INDEX:
        error= create_ordered_index(index_name, key_info);
        break;
      default:
        DBUG_ASSERT(FALSE);
        break;
      }
      if (error)
      {
        DBUG_PRINT("error", ("Failed to create index %u", i));
        drop_table();
        break;
      }
    }
    // Add handles to index objects
    if (idx_type != PRIMARY_KEY_INDEX && idx_type != UNIQUE_INDEX)
    {
      DBUG_PRINT("info", ("Get handle to index %s", index_name));
      const NDBINDEX *index= dict->getIndex(index_name, m_tabname);
      if (!index) DBUG_RETURN(1);
      m_index[i].index= (void *) index;
    }
    if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
    {
      DBUG_PRINT("info", ("Get handle to unique_index %s", unique_index_name));
      const NDBINDEX *index= dict->getIndex(unique_index_name, m_tabname);
      if (!index) DBUG_RETURN(1);
      m_index[i].unique_index= (void *) index;
      error= fix_unique_index_attr_order(m_index[i], index, key_info);
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
  bool is_hash_index=  (table->key_info[inx].algorithm == HA_KEY_ALG_HASH);
  if (inx == table->s->primary_key)
    return is_hash_index ? PRIMARY_KEY_INDEX : PRIMARY_KEY_ORDERED_INDEX;

  return ((table->key_info[inx].flags & HA_NOSAME) ? 
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

void ha_ndbcluster::release_metadata()
{
  uint i;

  DBUG_ENTER("release_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  m_table= NULL;
  m_table_info= NULL;

  // Release index list 
  for (i= 0; i < MAX_KEY; i++)
  {
    m_index[i].unique_index= NULL;      
    m_index[i].index= NULL;      
    if (m_index[i].unique_index_attrid_map)
    {
      my_free((char *)m_index[i].unique_index_attrid_map, MYF(0));
      m_index[i].unique_index_attrid_map= NULL;
    }
  }

  DBUG_VOID_RETURN;
}

int ha_ndbcluster::get_ndb_lock_type(enum thr_lock_type type)
{
  if (type >= TL_WRITE_ALLOW_WRITE)
    return NdbOperation::LM_Exclusive;
  else if (uses_blob_value(m_retrieve_all_fields))
    return NdbOperation::LM_Read;
  else
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
  DBUG_PRINT("info", ("idx_no: %d", idx_no));
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
  KEY* key_info= table->key_info + table->s->primary_key;
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
  KEY* key_info= table->key_info + table->s->primary_key;
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
  THD *thd= current_thd;

  DBUG_ENTER("define_read_attrs");  

  // Define attributes to read
  for (i= 0; i < table->s->fields; i++) 
  {
    Field *field= table->field[i];
    if ((thd->query_id == field->query_id) ||
        ((field->flags & PRI_KEY_FLAG)) || 
        m_retrieve_all_fields)
    {      
      if (get_ndb_value(op, field, i, buf))
        ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      m_value[i].ptr= NULL;
    }
  }
    
  if (table->s->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Getting hidden key"));
    // Scanning table with no primary key
    int hidden_no= table->s->fields;      
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

int ha_ndbcluster::pk_read(const byte *key, uint key_len, byte *buf) 
{
  uint no_fields= table->s->fields;
  NdbConnection *trans= m_active_trans;
  NdbOperation *op;

  int res;
  DBUG_ENTER("pk_read");
  DBUG_PRINT("enter", ("key_len: %u", key_len));
  DBUG_DUMP("key", (char*)key, key_len);

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) || 
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());
  
  if (table->s->primary_key == MAX_KEY) 
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
  
  if (execute_no_commit_ie(this,trans) != 0) 
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
*/

int ha_ndbcluster::complemented_pk_read(const byte *old_data, byte *new_data)
{
  uint no_fields= table->s->fields, i;
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  THD *thd= current_thd;
  DBUG_ENTER("complemented_pk_read");

  if (m_retrieve_all_fields)
    // We have allready retrieved all fields, nothing to complement
    DBUG_RETURN(0);

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) || 
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());
  int res;
  if ((res= set_primary_key_from_record(op, old_data)))
    ERR_RETURN(trans->getNdbError());
  // Read all unreferenced non-key field(s)
  for (i= 0; i < no_fields; i++) 
  {
    Field *field= table->field[i];
    if (!((field->flags & PRI_KEY_FLAG) ||
          (thd->query_id == field->query_id)))
    {
      if (get_ndb_value(op, field, i, new_data))
        ERR_RETURN(trans->getNdbError());
    }
  }
  
  if (execute_no_commit(this,trans) != 0) 
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
          (thd->query_id == field->query_id)))
    {
      m_value[i].ptr= NULL;
    }
  }
  
  DBUG_RETURN(0);
}

/*
  Peek to check if a particular row already exists
*/

int ha_ndbcluster::peek_row(const byte *record)
{
  NdbTransaction *trans= m_active_trans;
  NdbOperation *op;
  DBUG_ENTER("peek_row");

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) ||
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());

  int res;
  if ((res= set_primary_key_from_record(op, record)))
    ERR_RETURN(trans->getNdbError());

  if (execute_no_commit_ie(this,trans) != 0)
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
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
  if (!(op= trans->getNdbIndexOperation((NDBINDEX *) 
                                        m_index[active_index].unique_index, 
                                        (const NDBTAB *) m_table)) ||
      op->readTuple(lm) != 0)
    ERR_RETURN(trans->getNdbError());
  
  // Set secondary index key(s)
  if ((res= set_index_key(op, table->key_info + active_index, key)))
    DBUG_RETURN(res);
  
  if ((res= define_read_attrs(buf, op)))
    DBUG_RETURN(res);

  if (execute_no_commit_ie(this,trans) != 0) 
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
  
  bool contact_ndb= m_lock.type < TL_WRITE_ALLOW_WRITE;
  do {
    DBUG_PRINT("info", ("Call nextResult, contact_ndb: %d", contact_ndb));
    /*
      We can only handle one tuple with blobs at a time.
    */
    if (m_ops_pending && m_blobs_pending)
    {
      if (execute_no_commit(this,trans) != 0)
        DBUG_RETURN(ndb_err(trans));
      m_ops_pending= 0;
      m_blobs_pending= FALSE;
    }
    
    if ((check= cursor->nextResult(contact_ndb, m_force_send)) == 0)
    {
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
          if (execute_no_commit(this,trans) != 0)
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
                              const key_range *keys[2],
                              uint range_no)
{
  const KEY *const key_info= table->key_info + active_index;
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
              p.bound_type= NdbIndexScanOperation::BoundEQ;
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
                                      bool sorted, bool descending, byte* buf)
{  
  int res;
  bool restart;
  NdbTransaction *trans= m_active_trans;
  NdbIndexScanOperation *op;

  DBUG_ENTER("ha_ndbcluster::ordered_index_scan");
  DBUG_PRINT("enter", ("index: %u, sorted: %d, descending: %d",
             active_index, sorted, descending));  
  DBUG_PRINT("enter", ("Starting new ordered scan on %s", m_tabname));

  // Check that sorted seems to be initialised
  DBUG_ASSERT(sorted == 0 || sorted == 1);
  
  if (m_active_cursor == 0)
  {
    restart= FALSE;
    NdbOperation::LockMode lm=
      (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
    if (!(op= trans->getNdbIndexScanOperation((NDBINDEX *)
                                              m_index[active_index].index, 
                                              (const NDBTAB *) m_table)) ||
        op->readTuples(lm, 0, parallelism, sorted, descending))
      ERR_RETURN(trans->getNdbError());
    m_active_cursor= op;
  } else {
    restart= TRUE;
    op= (NdbIndexScanOperation*)m_active_cursor;
    
    DBUG_ASSERT(op->getSorted() == sorted);
    DBUG_ASSERT(op->getLockMode() == 
                (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type));
    if (op->reset_bounds(m_force_send))
      DBUG_RETURN(ndb_err(m_active_trans));
  }
  
  {
    const key_range *keys[2]= { start_key, end_key };
    res= set_bounds(op, keys);
    if (res)
      DBUG_RETURN(res);
  }

  if (!restart && generate_scan_filter(m_cond_stack, op))
    DBUG_RETURN(ndb_err(trans));
  
  if (!restart && (res= define_read_attrs(buf, op)))
  {
    DBUG_RETURN(res);
  }

  if (execute_no_commit(this,trans) != 0)
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

  DBUG_ENTER("full_table_scan");  
  DBUG_PRINT("enter", ("Starting new scan on %s", m_tabname));

  NdbOperation::LockMode lm=
    (NdbOperation::LockMode)get_ndb_lock_type(m_lock.type);
  if (!(op=trans->getNdbScanOperation((const NDBTAB *) m_table)) ||
      op->readTuples(lm, 0, parallelism))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= op;
  if (generate_scan_filter(m_cond_stack, op))
    DBUG_RETURN(ndb_err(trans));
  if ((res= define_read_attrs(buf, op)))
    DBUG_RETURN(res);

  if (execute_no_commit(this,trans) != 0)
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

  DBUG_ENTER("write_row");

  if (m_ignore_dup_key && table->s->primary_key != MAX_KEY)
  {
    int peek_res= peek_row(record);
    
    if (!peek_res) 
    {
      m_dupkey= table->s->primary_key;
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }
    if (peek_res != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(peek_res);
  }

  statistic_increment(thd->status_var.ha_write_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  has_auto_increment= (table->next_number_field && record == table->record[0]);

  if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)))
    ERR_RETURN(trans->getNdbError());

  res= (m_use_write) ? op->writeTuple() :op->insertTuple(); 
  if (res != 0)
    ERR_RETURN(trans->getNdbError());  
 
  if (table->s->primary_key == MAX_KEY) 
  {
    // Table has hidden primary key
    Ndb *ndb= get_ndb();
    Uint64 auto_value= NDB_FAILED_AUTO_INCREMENT;
    uint retries= NDB_AUTO_INCREMENT_RETRIES;
    do {
      auto_value= ndb->getAutoIncrementValue((const NDBTAB *) m_table);
    } while (auto_value == NDB_FAILED_AUTO_INCREMENT && 
             --retries &&
             ndb->getNdbError().status == NdbError::TemporaryError);
    if (auto_value == NDB_FAILED_AUTO_INCREMENT)
      ERR_RETURN(ndb->getNdbError());
    if (set_hidden_key(op, table->s->fields, (const byte*)&auto_value))
      ERR_RETURN(op->getNdbError());
  } 
  else 
  {
    int res;

    if (has_auto_increment) 
    {
      THD *thd= table->in_use;

      m_skip_auto_increment= FALSE;
      update_auto_increment();
      /* Ensure that handler is always called for auto_increment values */
      thd->next_insert_id= 0;
      m_skip_auto_increment= !auto_increment_column_changed;
    }

    if ((res= set_primary_key_from_record(op, record)))
      return res;  
  }

  // Set non-key attribute(s)
  bool set_blob_value= FALSE;
  for (i= 0; i < table->s->fields; i++) 
  {
    Field *field= table->field[i];
    if (!(field->flags & PRI_KEY_FLAG) &&
        set_ndb_value(op, field, i, &set_blob_value))
    {
      m_skip_auto_increment= TRUE;
      ERR_RETURN(op->getNdbError());
    }
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
      if (execute_no_commit(this,trans) != 0)
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
    DBUG_PRINT("info", 
               ("Trying to set next auto increment value to %lu",
                (ulong) next_val));
    if (ndb->setAutoIncrementValue((const NDBTAB *) m_table, next_val, TRUE))
      DBUG_PRINT("info", 
                 ("Setting next auto increment value to %u", next_val));  
  }
  m_skip_auto_increment= TRUE;

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
  DBUG_ENTER("update_row");
  
  statistic_increment(thd->status_var.ha_update_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
  {
    table->timestamp_field->set_time();
    // Set query_id so that field is really updated
    table->timestamp_field->query_id= thd->query_id;
  }

  /* Check for update of primary key for special handling */  
  if ((table->s->primary_key != MAX_KEY) &&
      (key_cmp(table->s->primary_key, old_data, new_data)))
  {
    int read_res, insert_res, delete_res, undo_res;

    DBUG_PRINT("info", ("primary key update, doing pk read+delete+insert"));
    // Get all old fields, since we optimize away fields not in query
    read_res= complemented_pk_read(old_data, new_data);
    if (read_res)
    {
      DBUG_PRINT("info", ("pk read failed"));
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
    m_ops_pending++;
    if (uses_blob_value(FALSE))
      m_blobs_pending= TRUE;
  }
  else
  {  
    if (!(op= trans->getNdbOperation((const NDBTAB *) m_table)) ||
        op->updateTuple() != 0)
      ERR_RETURN(trans->getNdbError());  
    
    if (table->s->primary_key == MAX_KEY) 
    {
      // This table has no primary key, use "hidden" primary key
      DBUG_PRINT("info", ("Using hidden key"));
      
      // Require that the PK for this record has previously been 
      // read into m_value
      uint no_fields= table->s->fields;
      const NdbRecAttr* rec= m_value[no_fields].rec;
      DBUG_ASSERT(rec);
      DBUG_DUMP("key", (char*)rec->aRef(), NDB_HIDDEN_PRIMARY_KEY_LENGTH);
      
      if (set_hidden_key(op, no_fields, rec->aRef()))
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
  for (i= 0; i < table->s->fields; i++) 
  {
    Field *field= table->field[i];
    if (((thd->query_id == field->query_id) || m_retrieve_all_fields) &&
        (!(field->flags & PRI_KEY_FLAG)) &&
        set_ndb_value(op, field, i))
      ERR_RETURN(op->getNdbError());
  }

  // Execute update operation
  if (!cursor && execute_no_commit(this,trans) != 0) {
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
  DBUG_ENTER("delete_row");

  statistic_increment(thd->status_var.ha_delete_count,&LOCK_status);
  m_rows_changed++;

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
    m_ops_pending++;

    no_uncommitted_rows_update(-1);

    if (!m_primary_key_update)
      // If deleting from cursor, NoCommit will be handled in next_result
      DBUG_RETURN(0);
  }
  else
  {
    
    if (!(op=trans->getNdbOperation((const NDBTAB *) m_table)) || 
        op->deleteTuple() != 0)
      ERR_RETURN(trans->getNdbError());
    
    no_uncommitted_rows_update(-1);
    
    if (table->s->primary_key == MAX_KEY) 
    {
      // This table has no primary key, use "hidden" primary key
      DBUG_PRINT("info", ("Using hidden key"));
      uint no_fields= table->s->fields;
      const NdbRecAttr* rec= m_value[no_fields].rec;
      DBUG_ASSERT(rec != NULL);
      
      if (set_hidden_key(op, no_fields, rec->aRef()))
        ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      int res;
      if ((res= set_primary_key_from_record(op, record)))
        return res;  
    }
  }

  // Execute delete operation
  if (execute_no_commit(this,trans) != 0) {
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

void ha_ndbcluster::unpack_record(byte* buf)
{
  uint row_offset= (uint) (buf - table->record[0]);
  Field **field, **end;
  NdbValue *value= m_value;
  DBUG_ENTER("unpack_record");

  end= table->field + table->s->fields;
  
  // Set null flag(s)
  bzero(buf, table->s->null_bytes);
  for (field= table->field;
       field < end;
       field++, value++)
  {
    if ((*value).ptr)
    {
      if (! ((*field)->flags & BLOB_FLAG))
      {
        if ((*value).rec->isNULL())
         (*field)->set_null(row_offset);
        else if ((*field)->type() == MYSQL_TYPE_BIT)
        {
          uint pack_len= (*field)->pack_length();
          if (pack_len < 5)
          {
            DBUG_PRINT("info", ("bit field H'%.8X", 
                                (*value).rec->u_32_value()));
            ((Field_bit *) *field)->store((longlong) 
                                          (*value).rec->u_32_value(),
                                          FALSE);
          }
          else
          {
            DBUG_PRINT("info", ("bit field H'%.8X%.8X",
                                *(Uint32 *)(*value).rec->aRef(),
                                *((Uint32 *)(*value).rec->aRef()+1)));
            ((Field_bit *) *field)->store((longlong)
                                          (*value).rec->u_64_value(), TRUE);
          }
        }
      }
      else
      {
        NdbBlob* ndb_blob= (*value).blob;
        bool isNull= TRUE;
#ifndef DBUG_OFF
        int ret= 
#endif
          ndb_blob->getNull(isNull);
        DBUG_ASSERT(ret == 0);
        if (isNull)
          (*field)->set_null(row_offset);
      }
    }
  }
  
#ifndef DBUG_OFF
  // Read and print all values that was fetched
  if (table->s->primary_key == MAX_KEY)
  {
    // Table with hidden primary key
    int hidden_no= table->s->fields;
    const NDBTAB *tab= (const NDBTAB *) m_table;
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    const NdbRecAttr* rec= m_value[hidden_no].rec;
    DBUG_ASSERT(rec);
    DBUG_PRINT("hidden", ("%d: %s \"%llu\"", hidden_no, 
                          hidden_col->getName(), rec->u_64_value()));
  } 
  //print_results();
#endif
  DBUG_VOID_RETURN;
}

/*
  Utility function to print/dump the fetched field
 */

void ha_ndbcluster::print_results()
{
  DBUG_ENTER("print_results");

#ifndef DBUG_OFF
  const NDBTAB *tab= (const NDBTAB*) m_table;

  if (!_db_on_)
    DBUG_VOID_RETURN;

  char buf_type[MAX_FIELD_WIDTH], buf_val[MAX_FIELD_WIDTH];
  String type(buf_type, sizeof(buf_type), &my_charset_bin);
  String val(buf_val, sizeof(buf_val), &my_charset_bin);
  for (uint f= 0; f < table->s->fields; f++)
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


int ha_ndbcluster::index_init(uint index)
{
  DBUG_ENTER("ha_ndbcluster::index_init");
  DBUG_PRINT("enter", ("index: %u", index));
  DBUG_RETURN(handler::index_init(index));
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
  DBUG_ENTER("ha_ndbcluster::index_read");
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d", 
                       active_index, key_len, find_flag));

  int error;
  ndb_index_type type= get_index_type(active_index);
  const KEY* key_info= table->key_info+active_index;
  switch (type){
  case PRIMARY_KEY_ORDERED_INDEX:
  case PRIMARY_KEY_INDEX:
    if (find_flag == HA_READ_KEY_EXACT && key_info->key_length == key_len)
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      DBUG_RETURN(pk_read(key, key_len, buf));
    }
    else if (type == PRIMARY_KEY_INDEX)
    {
      DBUG_RETURN(1);
    }
    break;
  case UNIQUE_ORDERED_INDEX:
  case UNIQUE_INDEX:
    if (find_flag == HA_READ_KEY_EXACT && key_info->key_length == key_len &&
        !check_null_in_key(key_info, key, key_len))
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      DBUG_RETURN(unique_index_read(key, key_len, buf));
    }
    else if (type == UNIQUE_INDEX)
    {
      DBUG_RETURN(1);
    }
    break;
  case ORDERED_INDEX:
    break;
  default:
  case UNDEFINED_INDEX:
    DBUG_ASSERT(FALSE);
    DBUG_RETURN(1);
    break;
  }
  
  key_range start_key;
  start_key.key= key;
  start_key.length= key_len;
  start_key.flag= find_flag;
  bool descending= FALSE;
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
  error= ordered_index_scan(&start_key, 0, TRUE, descending, buf);  
  DBUG_RETURN(error == HA_ERR_END_OF_FILE ? HA_ERR_KEY_NOT_FOUND : error);
}


int ha_ndbcluster::index_read_idx(byte *buf, uint index_no, 
                              const byte *key, uint key_len, 
                              enum ha_rkey_function find_flag)
{
  statistic_increment(current_thd->status_var.ha_read_key_count, &LOCK_status);
  DBUG_ENTER("ha_ndbcluster::index_read_idx");
  DBUG_PRINT("enter", ("index_no: %u, key_len: %u", index_no, key_len));  
  index_init(index_no);  
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
  DBUG_RETURN(ordered_index_scan(0, 0, TRUE, FALSE, buf));
}


int ha_ndbcluster::index_last(byte *buf)
{
  DBUG_ENTER("ha_ndbcluster::index_last");
  statistic_increment(current_thd->status_var.ha_read_last_count,&LOCK_status);
  DBUG_RETURN(ordered_index_scan(0, 0, TRUE, TRUE, buf));
}

int ha_ndbcluster::index_read_last(byte * buf, const byte * key, uint key_len)
{
  DBUG_ENTER("ha_ndbcluster::index_read_last");
  DBUG_RETURN(index_read(buf, key, key_len, HA_READ_PREFIX_LAST));
}

inline
int ha_ndbcluster::read_range_first_to_buf(const key_range *start_key,
                                           const key_range *end_key,
                                           bool eq_r, bool sorted,
                                           byte* buf)
{
  KEY* key_info;
  int error= 1; 
  DBUG_ENTER("ha_ndbcluster::read_range_first_to_buf");
  DBUG_PRINT("info", ("eq_r: %d, sorted: %d", eq_r, sorted));

  switch (get_index_type(active_index)){
  case PRIMARY_KEY_ORDERED_INDEX:
  case PRIMARY_KEY_INDEX:
    key_info= table->key_info + active_index;
    if (start_key && 
        start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT)
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      error= pk_read(start_key->key, start_key->length, buf);      
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    break;
  case UNIQUE_ORDERED_INDEX:
  case UNIQUE_INDEX:
    key_info= table->key_info + active_index;
    if (start_key && start_key->length == key_info->key_length &&
        start_key->flag == HA_READ_KEY_EXACT && 
        !check_null_in_key(key_info, start_key->key, start_key->length))
    {
      if (m_active_cursor && (error= close_scan()))
        DBUG_RETURN(error);
      error= unique_index_read(start_key->key, start_key->length, buf);
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    break;
  default:
    break;
  }

  // Start the ordered index scan and fetch the first row
  error= ordered_index_scan(start_key, end_key, sorted, FALSE, buf);
  DBUG_RETURN(error);
}


int ha_ndbcluster::read_range_first(const key_range *start_key,
                                    const key_range *end_key,
                                    bool eq_r, bool sorted)
{
  byte* buf= table->record[0];
  DBUG_ENTER("ha_ndbcluster::read_range_first");
  
  DBUG_RETURN(read_range_first_to_buf(start_key,
                                      end_key,
                                      eq_r, 
                                      sorted,
                                      buf));
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
  index_init(table->s->primary_key);
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
  
  if (m_ops_pending)
  {
    /*
      Take over any pending transactions to the 
      deleteing/updating transaction before closing the scan    
    */
    DBUG_PRINT("info", ("ops_pending: %d", m_ops_pending));    
    if (execute_no_commit(this,trans) != 0) {
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
  DBUG_RETURN(pk_read(pos, ref_length, buf));  
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
  DBUG_ENTER("position");

  if (table->s->primary_key != MAX_KEY) 
  {
    key_info= table->key_info + table->s->primary_key;
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
      memcpy(buff, record + key_part->offset, key_part->length);
      buff += key_part->length;
    }
  } 
  else 
  {
    // No primary key, get hidden key
    DBUG_PRINT("info", ("Getting hidden key"));
    int hidden_no= table->s->fields;
    const NdbRecAttr* rec= m_value[hidden_no].rec;
    memcpy(ref, (const void*)rec->aRef(), ref_length);
#ifndef DBUG_OFF
    const NDBTAB *tab= (const NDBTAB *) m_table;  
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    DBUG_ASSERT(hidden_col->getPrimaryKey() && 
                hidden_col->getAutoIncrement() &&
                rec != NULL && 
                ref_length == NDB_HIDDEN_PRIMARY_KEY_LENGTH);
#endif
  }
  
  DBUG_DUMP("ref", (char*)ref, ref_length);
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
        records= 100;
      else
        records_update();
    }
    else
    {
      if ((my_errno= check_ndb_connection()))
        DBUG_VOID_RETURN;
      Ndb *ndb= get_ndb();
      struct Ndb_statistics stat;
      if (current_thd->variables.ndb_use_exact_count &&
          ndb_get_table_statistics(ndb, m_tabname, &stat) == 0)
      {
        mean_rec_length= stat.row_size;
        data_file_length= stat.fragment_memory;
        records= stat.row_count;
      }
      else
      {
        mean_rec_length= 0;
        records= 100;
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
      
      auto_increment_value= 
        ndb->readAutoIncrementValue((const NDBTAB *) m_table);
    }
  }
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("extra");
  switch (operation) {
  case HA_EXTRA_NORMAL:              /* Optimize for space (def) */
    DBUG_PRINT("info", ("HA_EXTRA_NORMAL"));
    break;
  case HA_EXTRA_QUICK:                 /* Optimize for speed */
    DBUG_PRINT("info", ("HA_EXTRA_QUICK"));
    break;
  case HA_EXTRA_RESET:                 /* Reset database to after open */
    DBUG_PRINT("info", ("HA_EXTRA_RESET"));
    DBUG_PRINT("info", ("Clearing condition stack"));
    cond_clear();
    break;
  case HA_EXTRA_CACHE:                 /* Cash record in HA_rrnd() */
    DBUG_PRINT("info", ("HA_EXTRA_CACHE"));
    break;
  case HA_EXTRA_NO_CACHE:              /* End cacheing of records (def) */
    DBUG_PRINT("info", ("HA_EXTRA_NO_CACHE"));
    break;
  case HA_EXTRA_NO_READCHECK:          /* No readcheck on update */
    DBUG_PRINT("info", ("HA_EXTRA_NO_READCHECK"));
    break;
  case HA_EXTRA_READCHECK:             /* Use readcheck (def) */
    DBUG_PRINT("info", ("HA_EXTRA_READCHECK"));
    break;
  case HA_EXTRA_KEYREAD:               /* Read only key to database */
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD"));
    break;
  case HA_EXTRA_NO_KEYREAD:            /* Normal read of records (def) */
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEYREAD"));
    break;
  case HA_EXTRA_NO_USER_CHANGE:        /* No user is allowed to write */
    DBUG_PRINT("info", ("HA_EXTRA_NO_USER_CHANGE"));
    break;
  case HA_EXTRA_KEY_CACHE:
    DBUG_PRINT("info", ("HA_EXTRA_KEY_CACHE"));
    break;
  case HA_EXTRA_NO_KEY_CACHE:
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEY_CACHE"));
    break;
  case HA_EXTRA_WAIT_LOCK:            /* Wait until file is avalably (def) */
    DBUG_PRINT("info", ("HA_EXTRA_WAIT_LOCK"));
    break;
  case HA_EXTRA_NO_WAIT_LOCK:         /* If file is locked, return quickly */
    DBUG_PRINT("info", ("HA_EXTRA_NO_WAIT_LOCK"));
    break;
  case HA_EXTRA_WRITE_CACHE:           /* Use write cache in ha_write() */
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CACHE"));
    break;
  case HA_EXTRA_FLUSH_CACHE:           /* flush write_record_cache */
    DBUG_PRINT("info", ("HA_EXTRA_FLUSH_CACHE"));
    break;
  case HA_EXTRA_NO_KEYS:               /* Remove all update of keys */
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEYS"));
    break;
  case HA_EXTRA_KEYREAD_CHANGE_POS:         /* Keyread, but change pos */
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD_CHANGE_POS")); /* xxxxchk -r must be used */
    break;                                  
  case HA_EXTRA_REMEMBER_POS:          /* Remember pos for next/prev */
    DBUG_PRINT("info", ("HA_EXTRA_REMEMBER_POS"));
    break;
  case HA_EXTRA_RESTORE_POS:
    DBUG_PRINT("info", ("HA_EXTRA_RESTORE_POS"));
    break;
  case HA_EXTRA_REINIT_CACHE:          /* init cache from current record */
    DBUG_PRINT("info", ("HA_EXTRA_REINIT_CACHE"));
    break;
  case HA_EXTRA_FORCE_REOPEN:          /* Datafile have changed on disk */
    DBUG_PRINT("info", ("HA_EXTRA_FORCE_REOPEN"));
    break;
  case HA_EXTRA_FLUSH:                 /* Flush tables to disk */
    DBUG_PRINT("info", ("HA_EXTRA_FLUSH"));
    break;
  case HA_EXTRA_NO_ROWS:               /* Don't write rows */
    DBUG_PRINT("info", ("HA_EXTRA_NO_ROWS"));
    break;
  case HA_EXTRA_RESET_STATE:           /* Reset positions */
    DBUG_PRINT("info", ("HA_EXTRA_RESET_STATE"));
    break;
  case HA_EXTRA_IGNORE_DUP_KEY:       /* Dup keys don't rollback everything*/
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_DUP_KEY"));
    if (current_thd->lex->sql_command == SQLCOM_REPLACE)
    {
      DBUG_PRINT("info", ("Turning ON use of write instead of insert"));
      m_use_write= TRUE;
    } else 
    {
      DBUG_PRINT("info", ("Ignoring duplicate key"));
      m_ignore_dup_key= TRUE;
    }
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
    DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
    m_use_write= FALSE;
    m_ignore_dup_key= FALSE;
    break;
  case HA_EXTRA_RETRIEVE_ALL_COLS:    /* Retrieve all columns, not just those
                                         where field->query_id is the same as
                                         the current query id */
    DBUG_PRINT("info", ("HA_EXTRA_RETRIEVE_ALL_COLS"));
    m_retrieve_all_fields= TRUE;
    break;
  case HA_EXTRA_PREPARE_FOR_DELETE:
    DBUG_PRINT("info", ("HA_EXTRA_PREPARE_FOR_DELETE"));
    break;
  case HA_EXTRA_PREPARE_FOR_UPDATE:     /* Remove read cache if problems */
    DBUG_PRINT("info", ("HA_EXTRA_PREPARE_FOR_UPDATE"));
    break;
  case HA_EXTRA_PRELOAD_BUFFER_SIZE: 
    DBUG_PRINT("info", ("HA_EXTRA_PRELOAD_BUFFER_SIZE"));
    break;
  case HA_EXTRA_RETRIEVE_PRIMARY_KEY: 
    DBUG_PRINT("info", ("HA_EXTRA_RETRIEVE_PRIMARY_KEY"));
    m_retrieve_primary_key= TRUE;
    break;
  case HA_EXTRA_CHANGE_KEY_TO_UNIQUE: 
    DBUG_PRINT("info", ("HA_EXTRA_CHANGE_KEY_TO_UNIQUE"));
    break;
  case HA_EXTRA_CHANGE_KEY_TO_DUP: 
    DBUG_PRINT("info", ("HA_EXTRA_CHANGE_KEY_TO_DUP"));
  case HA_EXTRA_KEYREAD_PRESERVE_FIELDS:
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD_PRESERVE_FIELDS"));
    break;

  }
  
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
  const NDBTAB *tab= (const NDBTAB *) m_table;    

  DBUG_ENTER("start_bulk_insert");
  DBUG_PRINT("enter", ("rows: %d", (int)rows));
  
  m_rows_inserted= (ha_rows) 0;
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
    if (execute_no_commit(this,trans) != 0) {
      no_uncommitted_rows_execute_failure();
      my_errno= error= ndb_err(trans);
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
  double res= rows2double(records*1000);
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

  DBUG_PRINT("enter", ("thd: %x, thd_ndb: %x, thd_ndb->lock_count: %d",
                       thd, thd_ndb, thd_ndb->lock_count));

  if (lock_type != F_UNLCK)
  {
    DBUG_PRINT("info", ("lock_type != F_UNLCK"));
    if (!thd_ndb->lock_count++)
    {
      PRINT_OPTION_FLAGS(thd);
      if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK))) 
      {
        // Autocommit transaction
        DBUG_ASSERT(!thd_ndb->stmt);
        DBUG_PRINT("trans",("Starting transaction stmt"));      

        trans= ndb->startTransaction();
        if (trans == NULL)
          ERR_RETURN(ndb->getNdbError());
        no_uncommitted_rows_reset(thd);
        thd_ndb->stmt= trans;
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
          no_uncommitted_rows_reset(thd);
          thd_ndb->all= trans; 
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
    if (!thd->transaction.on)
      m_transaction_on= FALSE;
    else
      m_transaction_on= thd->variables.ndb_use_transactions;

    m_active_trans= thd_ndb->all ? thd_ndb->all : thd_ndb->stmt;
    DBUG_ASSERT(m_active_trans);
    // Start of transaction
    m_rows_changed= 0;
    m_retrieve_all_fields= FALSE;
    m_retrieve_primary_key= FALSE;
    m_ops_pending= 0;
    {
      NDBDICT *dict= ndb->getDictionary();
      const NDBTAB *tab;
      void *tab_info;
      if (!(tab= dict->getTable(m_tabname, &tab_info)))
        ERR_RETURN(dict->getNdbError());
      DBUG_PRINT("info", ("Table schema version: %d", 
                          tab->getObjectVersion()));
      // Check if thread has stale local cache
      // New transaction must not use old tables... (trans != 0)
      // Running might...
      if ((trans && tab->getObjectStatus() != NdbDictionary::Object::Retrieved)
	  || tab->getObjectStatus() == NdbDictionary::Object::Invalid)
      {
        invalidate_dictionary_cache(FALSE);
        if (!(tab= dict->getTable(m_tabname, &tab_info)))
          ERR_RETURN(dict->getNdbError());
        DBUG_PRINT("info", ("Table schema version: %d", 
                            tab->getObjectVersion()));
      }
      if (m_table != (void *)tab || m_table_version < tab->getObjectVersion())
      {
        /*
          The table has been altered, refresh the index list
        */
        build_index_list(ndb, table, ILBP_OPEN);  
        m_table= (void *)tab;
        m_table_version = tab->getObjectVersion();
      }
      m_table_info= tab_info;
    }
    no_uncommitted_rows_init(thd);
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
  DBUG_RETURN(error);
}

/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
  Ndb doesn't currently support table locks, and will do ordinary
  startTransaction for each transaction/statement.
*/

int ha_ndbcluster::start_stmt(THD *thd, thr_lock_type lock_type)
{
  int error=0;
  DBUG_ENTER("start_stmt");
  PRINT_OPTION_FLAGS(thd);

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbTransaction *trans= thd_ndb->stmt;
  if (!trans){
    Ndb *ndb= thd_ndb->ndb;
    DBUG_PRINT("trans",("Starting transaction stmt"));  

#if 0    
    NdbTransaction *tablock_trans= thd_ndb->all;
    DBUG_PRINT("info", ("tablock_trans: %x", (UintPtr)tablock_trans));
    DBUG_ASSERT(tablock_trans);
//    trans= ndb->hupp(tablock_trans);
#endif
    trans= ndb->startTransaction();
    if (trans == NULL)
      ERR_RETURN(ndb->getNdbError());
    no_uncommitted_rows_reset(thd);
    thd_ndb->stmt= trans;
    trans_register_ha(thd, FALSE, &ndbcluster_hton);
  }
  m_active_trans= trans;

  // Start of statement
  m_retrieve_all_fields= FALSE;
  m_retrieve_primary_key= FALSE;
  m_ops_pending= 0;    
  
  DBUG_RETURN(error);
}


/*
  Commit a transaction started in NDB
 */

int ndbcluster_commit(THD *thd, bool all)
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
    DBUG_PRINT("info", ("Invalidate commit_count for %s, share->commit_count: %d ", share->table_name, share->commit_count));
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

int ndbcluster_rollback(THD *thd, bool all)
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
    int no_of_bits= field->field_length*8 + ((Field_bit *) field)->bit_len;
    col.setType(NDBCOL::Bit);
    if (!no_of_bits)
      col.setLength(1);
      else
        col.setLength(no_of_bits);
    break;
  }
  case MYSQL_TYPE_NULL:        
  case MYSQL_TYPE_GEOMETRY:
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
    col.setAutoIncrement(TRUE);
    ulonglong value= info->auto_increment_value ?
      info->auto_increment_value : (ulonglong) 1;
    DBUG_PRINT("info", ("Autoincrement key, initial: %llu", value));
    col.setAutoIncrementInitialValue(value);
  }
  else
    col.setAutoIncrement(FALSE);
  return 0;
}

/*
  Create a table in NDB Cluster
 */

static void ndb_set_fragmentation(NDBTAB &tab, TABLE *form, uint pk_length)
{
  if (form->s->max_rows == (ha_rows) 0) /* default setting, don't set fragmentation */
    return;
  /**
   * get the number of fragments right
   */
  uint no_fragments;
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
    ulonglong max_rows= form->s->max_rows;
#if MYSQL_VERSION_ID >= 50100
    no_fragments= (max_rows*acc_row_size)/acc_fragment_size+1;
#else
    no_fragments= ((max_rows*acc_row_size)/acc_fragment_size+1
                   +1/*correct rounding*/)/2;
#endif
  }
  {
    uint no_nodes= g_ndb_cluster_connection->no_db_nodes();
    NDBTAB::FragmentType ftype;
    if (no_fragments > 2*no_nodes)
    {
      ftype= NDBTAB::FragAllLarge;
      if (no_fragments > 4*no_nodes)
        push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                     "Ndb might have problems storing the max amount of rows specified");
    }
    else if (no_fragments > no_nodes)
      ftype= NDBTAB::FragAllMedium;
    else
      ftype= NDBTAB::FragAllSmall;
    tab.setFragmentType(ftype);
  }
}

int ha_ndbcluster::create(const char *name, 
                          TABLE *form, 
                          HA_CREATE_INFO *info)
{
  NDBTAB tab;
  NDBCOL col;
  uint pack_length, length, i, pk_length= 0;
  const void *data, *pack_data;
  char name2[FN_HEADLEN];
  bool create_from_engine= (info->table_options & HA_CREATE_FROM_ENGINE);
   
  DBUG_ENTER("ha_ndbcluster::create");
  DBUG_PRINT("enter", ("name: %s", name));
  fn_format(name2, name, "", "",2);       // Remove the .frm extension
  set_dbname(name2);
  set_tabname(name2);    

  if (create_from_engine)
  {
    /*
      Table alreay exists in NDB and frm file has been created by 
      caller.
      Do Ndb specific stuff, such as create a .ndb file
    */
    my_errno= write_ndb_file();
    DBUG_RETURN(my_errno);
  }

  DBUG_PRINT("table", ("name: %s", m_tabname));  
  tab.setName(m_tabname);
  tab.setLogging(!(info->options & HA_LEX_CREATE_TMP_TABLE));    
   
  // Save frm data for this table
  if (readfrm(name, &data, &length))
    DBUG_RETURN(1);
  if (packfrm(data, length, &pack_data, &pack_length))
    DBUG_RETURN(2);
  
  DBUG_PRINT("info", ("setFrm data=%x, len=%d", pack_data, pack_length));
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
    tab.addColumn(col);
    if (col.getPrimaryKey())
      pk_length += (field->pack_length() + 3) / 4;
  }
  
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

  ndb_set_fragmentation(tab, form, pk_length);

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
  DBUG_PRINT("info", ("Table %s/%s created successfully", 
                      m_dbname, m_tabname));

  // Create secondary indexes
  my_errno= build_index_list(ndb, form, ILBP_CREATE);

  if (!my_errno)
    my_errno= write_ndb_file();

  DBUG_RETURN(my_errno);
}


int ha_ndbcluster::create_ordered_index(const char *name, 
                                        KEY *key_info)
{
  DBUG_ENTER("ha_ndbcluster::create_ordered_index");
  DBUG_RETURN(create_index(name, key_info, FALSE));
}

int ha_ndbcluster::create_unique_index(const char *name, 
                                       KEY *key_info)
{

  DBUG_ENTER("ha_ndbcluster::create_unique_index");
  DBUG_RETURN(create_index(name, key_info, TRUE));
}


/*
  Create an index in NDB Cluster
 */

int ha_ndbcluster::create_index(const char *name, 
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
  
  if (dict->createIndex(ndb_index))
    ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  DBUG_RETURN(0);  
}


/*
  Rename a table in NDB Cluster
*/

int ha_ndbcluster::rename_table(const char *from, const char *to)
{
  NDBDICT *dict;
  char new_tabname[FN_HEADLEN];
  const NDBTAB *orig_tab;
  int result;

  DBUG_ENTER("ha_ndbcluster::rename_table");
  DBUG_PRINT("info", ("Renaming %s to %s", from, to));
  set_dbname(from);
  set_tabname(from);
  set_tabname(to, new_tabname);

  if (check_ndb_connection())
    DBUG_RETURN(my_errno= HA_ERR_NO_CONNECTION);

  Ndb *ndb= get_ndb();
  dict= ndb->getDictionary();
  if (!(orig_tab= dict->getTable(m_tabname)))
    ERR_RETURN(dict->getNdbError());
  // Check if thread has stale local cache
  if (orig_tab->getObjectStatus() == NdbDictionary::Object::Invalid)
  {
    dict->removeCachedTable(m_tabname);
    if (!(orig_tab= dict->getTable(m_tabname)))
      ERR_RETURN(dict->getNdbError());
  }
  m_table= (void *)orig_tab;
  // Change current database to that of target table
  set_dbname(to);
  ndb->setDatabaseName(m_dbname);
  if (!(result= alter_table_name(new_tabname)))
  {
    // Rename .ndb file
    result= handler::rename_table(from, to);
  }

  DBUG_RETURN(result);
}


/*
  Rename a table in NDB Cluster using alter table
 */

int ha_ndbcluster::alter_table_name(const char *to)
{
  Ndb *ndb= get_ndb();
  NDBDICT *dict= ndb->getDictionary();
  const NDBTAB *orig_tab= (const NDBTAB *) m_table;
  DBUG_ENTER("alter_table_name_table");

  NdbDictionary::Table new_tab= *orig_tab;
  new_tab.setName(to);
  if (dict->alterTable(new_tab) != 0)
    ERR_RETURN(dict->getNdbError());

  m_table= NULL;
  m_table_info= NULL;
                                                                             
  DBUG_RETURN(0);
}


/*
  Delete table from NDB Cluster

 */

int ha_ndbcluster::delete_table(const char *name)
{
  DBUG_ENTER("ha_ndbcluster::delete_table");
  DBUG_PRINT("enter", ("name: %s", name));
  set_dbname(name);
  set_tabname(name);

  if (check_ndb_connection())
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  /* Call ancestor function to delete .ndb file */
  handler::delete_table(name);
  
  /* Drop the table from NDB */
  DBUG_RETURN(drop_table());
}


/*
  Drop table in NDB Cluster
 */

int ha_ndbcluster::drop_table()
{
  Ndb *ndb= get_ndb();
  NdbDictionary::Dictionary *dict= ndb->getDictionary();

  DBUG_ENTER("drop_table");
  DBUG_PRINT("enter", ("Deleting %s", m_tabname));

  release_metadata();
  if (dict->dropTable(m_tabname))
    ERR_RETURN(dict->getNdbError());
  DBUG_RETURN(0);
}


ulonglong ha_ndbcluster::get_auto_increment()
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
  auto_value= NDB_FAILED_AUTO_INCREMENT;
  uint retries= NDB_AUTO_INCREMENT_RETRIES;
  do {
    auto_value=
      (m_skip_auto_increment) ? 
      ndb->readAutoIncrementValue((const NDBTAB *) m_table)
      : ndb->getAutoIncrementValue((const NDBTAB *) m_table, cache_size);
  } while (auto_value == NDB_FAILED_AUTO_INCREMENT && 
           --retries &&
           ndb->getNdbError().status == NdbError::TemporaryError);
  if (auto_value == NDB_FAILED_AUTO_INCREMENT)
    ERR_RETURN(ndb->getNdbError());
  DBUG_RETURN((longlong)auto_value);
}


/*
  Constructor for the NDB Cluster table handler 
 */

ha_ndbcluster::ha_ndbcluster(TABLE *table_arg):
  handler(&ndbcluster_hton, table_arg),
  m_active_trans(NULL),
  m_active_cursor(NULL),
  m_table(NULL),
  m_table_version(-1),
  m_table_info(NULL),
  m_table_flags(HA_REC_NOT_IN_SEQ |
                HA_NULL_IN_KEY |
                HA_AUTO_PART_KEY |
                HA_NO_PREFIX_CHAR_KEYS |
                HA_NEED_READ_RANGE_BUFFER |
                HA_CAN_BIT_FIELD),
  m_share(0),
  m_use_write(FALSE),
  m_ignore_dup_key(FALSE),
  m_primary_key_update(FALSE),
  m_retrieve_all_fields(FALSE),
  m_retrieve_primary_key(FALSE),
  m_rows_to_insert((ha_rows) 1),
  m_rows_inserted((ha_rows) 0),
  m_bulk_insert_rows((ha_rows) 1024),
  m_rows_changed((ha_rows) 0),
  m_bulk_insert_not_flushed(FALSE),
  m_ops_pending(0),
  m_skip_auto_increment(TRUE),
  m_blobs_pending(0),
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

  records= ~(ha_rows)0; // uninitialized
  block_size= 1024;

  for (i= 0; i < MAX_KEY; i++)
  {
    m_index[i].type= UNDEFINED_INDEX;
    m_index[i].unique_index= NULL;
    m_index[i].index= NULL;
    m_index[i].unique_index_attrid_map= NULL;
  }

  DBUG_VOID_RETURN;
}


/*
  Destructor for NDB Cluster table handler
 */

ha_ndbcluster::~ha_ndbcluster() 
{
  DBUG_ENTER("~ha_ndbcluster");

  if (m_share)
    free_share(m_share);
  release_metadata();
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
*/

int ha_ndbcluster::open(const char *name, int mode, uint test_if_locked)
{
  int res;
  KEY *key;
  DBUG_ENTER("open");
  DBUG_PRINT("enter", ("name: %s mode: %d test_if_locked: %d",
                       name, mode, test_if_locked));
  
  // Setup ref_length to make room for the whole 
  // primary key to be written in the ref variable
  
  if (table->s->primary_key != MAX_KEY) 
  {
    key= table->key_info+table->s->primary_key;
    ref_length= key->key_length;
    DBUG_PRINT("info", (" ref_length: %d", ref_length));
  }
  // Init table lock structure 
  if (!(m_share=get_share(name)))
    DBUG_RETURN(1);
  thr_lock_data_init(&m_share->lock,&m_lock,(void*) 0);
  
  set_dbname(name);
  set_tabname(name);
  
  if (check_ndb_connection()) {
    free_share(m_share); m_share= 0;
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  }
  
  res= get_metadata(name);
  if (!res)
    info(HA_STATUS_VARIABLE | HA_STATUS_CONST);

  DBUG_RETURN(res);
}


/*
  Close the table
  - release resources setup by open()
 */

int ha_ndbcluster::close(void)
{
  DBUG_ENTER("close");  
  free_share(m_share); m_share= 0;
  release_metadata();
  DBUG_RETURN(0);
}


Thd_ndb* ha_ndbcluster::seize_thd_ndb()
{
  Thd_ndb *thd_ndb;
  DBUG_ENTER("seize_thd_ndb");

  thd_ndb= new Thd_ndb();
  thd_ndb->ndb->getDictionary()->set_local_table_data_size(
    sizeof(Ndb_local_table_statistics)
    );
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


int ndbcluster_close_connection(THD *thd)
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
  uint len;
  const void* data;
  const NDBTAB* tab;
  Ndb* ndb;
  DBUG_ENTER("ndbcluster_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name)); 

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);  
  ndb->setDatabaseName(db);

  NDBDICT* dict= ndb->getDictionary();
  dict->set_local_table_data_size(sizeof(Ndb_local_table_statistics));
  dict->invalidateTable(name);
  if (!(tab= dict->getTable(name)))
  {    
    const NdbError err= dict->getNdbError();
    if (err.code == 709)
      DBUG_RETURN(-1);
    ERR_RETURN(err);
  }
  DBUG_PRINT("info", ("Found table %s", tab->getName()));
  
  len= tab->getFrmLength();  
  if (len == 0 || tab->getFrmData() == NULL)
  {
    DBUG_PRINT("error", ("No frm data found."));
    DBUG_RETURN(1);
  }
  
  if (unpackfrm(&data, &len, tab->getFrmData()))
  {
    DBUG_PRINT("error", ("Could not unpack table"));
    DBUG_RETURN(1);
  }

  *frmlen= len;
  *frmblob= data;
  
  DBUG_RETURN(0);
}

/*
  Check if a table exists in NDB

 */

int ndbcluster_table_exists_in_engine(THD* thd, const char *db, const char *name)
{
  const NDBTAB* tab;
  Ndb* ndb;
  DBUG_ENTER("ndbcluster_table_exists_in_engine");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));

  if (!(ndb= check_ndb_in_thd(thd)))
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  ndb->setDatabaseName(db);

  NDBDICT* dict= ndb->getDictionary();
  dict->set_local_table_data_size(sizeof(Ndb_local_table_statistics));
  dict->invalidateTable(name);
  if (!(tab= dict->getTable(name)))
  {
    const NdbError err= dict->getNdbError();
    if (err.code == 709)
      DBUG_RETURN(0);
    ERR_RETURN(err);
  }

  DBUG_PRINT("info", ("Found table %s", tab->getName()));
  DBUG_RETURN(1);
}



extern "C" byte* tables_get_key(const char *entry, uint *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= strlen(entry);
  return (byte*) entry;
}


/*
  Drop a database in NDB Cluster
 */

int ndbcluster_drop_database(const char *path)
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
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  
  // List tables in NDB
  NDBDICT *dict= ndb->getDictionary();
  if (dict->listObjects(list, 
                        NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(dict->getNdbError());
  for (i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& t= list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", t.database, t.name));     
    
    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, t.database, dbname))
      continue;
    DBUG_PRINT("info", ("%s must be dropped", t.name));     
    drop_list.push_back(thd->strdup(t.name));
  }
  // Drop any tables belonging to database
  ndb->setDatabaseName(dbname);
  List_iterator_fast<char> it(drop_list);
  while ((tabname=it++))
  {
    if (dict->dropTable(tabname))
    {
      const NdbError err= dict->getNdbError();
      if (err.code != 709)
      {
        ERR_PRINT(err);
        ret= ndb_to_mysql_error(&err);
      }
    }
  }
  DBUG_RETURN(ret);      
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
  NdbDictionary::Dictionary::List list;

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
    NdbDictionary::Dictionary::List::Element& t= list.elements[i];
    DBUG_PRINT("info", ("Found %s/%s in NDB", t.database, t.name));     

    // Add only tables that belongs to db
    if (my_strcasecmp(system_charset_info, t.database, db))
      continue;

    // Apply wildcard to list of tables in NDB
    if (wild)
    {
      if (lower_case_table_names)
      {
        if (wild_case_compare(files_charset_info, t.name, wild))
          continue;
      }
      else if (wild_compare(t.name,wild,0))
        continue;
    }
    DBUG_PRINT("info", ("Inserting %s into ndb_tables hash", t.name));     
    my_hash_insert(&ndb_tables, (byte*)thd->strdup(t.name));
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
    (void)strxnmov(name, FN_REFLEN, 
                   mysql_data_home,"/",db,"/",file_name,ha_ndb_ext,NullS);
    DBUG_PRINT("info", ("Check access for %s", name));
    if (access(name, F_OK))
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

  // Check for new files to discover
  DBUG_PRINT("info", ("Checking for new files to discover"));       
  List<char> create_list;
  for (i= 0 ; i < ndb_tables.records ; i++)
  {
    file_name= hash_element(&ndb_tables, i);
    if (!hash_search(&ok_tables, file_name, strlen(file_name)))
    {
      DBUG_PRINT("info", ("%s must be discovered", file_name));       
      // File is in list of ndb tables and not in ok_tables
      // This table need to be created
      create_list.push_back(thd->strdup(file_name));
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
    if (ha_create_table_from_engine(thd, db, file_name) == 0)
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
  return 0;
}

handlerton *
ndbcluster_init()
{
  int res;
  DBUG_ENTER("ndbcluster_init");
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

  g_ndb_cluster_connection->set_optimized_node_selection
    (opt_ndb_optimized_node_selection);

  // Create a Ndb object to open the connection  to NDB
  if ( (g_ndb= new Ndb(g_ndb_cluster_connection, "sys")) == 0 )
  {
    DBUG_PRINT("error", ("failed to create global ndb object"));
    goto ndbcluster_init_error;
  }
  g_ndb->getDictionary()->set_local_table_data_size(sizeof(Ndb_local_table_statistics));
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
  DBUG_RETURN(&ndbcluster_hton);

 ndbcluster_init_error:
  if (g_ndb)
    delete g_ndb;
  g_ndb= NULL;
  if (g_ndb_cluster_connection)
    delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;
  DBUG_RETURN(NULL);
}


/*
  End use of the NDB Cluster table handler
  - free all global variables allocated by 
    ndbcluster_init()
*/

bool ndbcluster_end()
{
  DBUG_ENTER("ndbcluster_end");

  if (!ndbcluster_inited)
    DBUG_RETURN(0);

  // Kill ndb utility thread
  (void) pthread_mutex_lock(&LOCK_ndb_util_thread);  
  DBUG_PRINT("exit",("killing ndb util thread: %lx", ndb_util_thread));
  (void) pthread_cond_signal(&COND_ndb_util_thread);
  (void) pthread_mutex_unlock(&LOCK_ndb_util_thread);

  if (g_ndb)
    delete g_ndb;
  g_ndb= NULL;
  if (g_ndb_cluster_connection)
    delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;

  hash_free(&ndbcluster_open_tables);
  pthread_mutex_destroy(&ndbcluster_mutex);
  pthread_mutex_destroy(&LOCK_ndb_util_thread);
  pthread_cond_destroy(&COND_ndb_util_thread);
  ndbcluster_inited= 0;
  DBUG_RETURN(0);
}

/*
  Static error print function called from
  static handler method ndbcluster_commit
  and ndbcluster_rollback
*/

void ndbcluster_print_error(int error, const NdbOperation *error_op)
{
  DBUG_ENTER("ndbcluster_print_error");
  TABLE tab;
  const char *tab_name= (error_op) ? error_op->getTableName() : "";
  tab.alias= (char *) tab_name;
  ha_ndbcluster error_handler(&tab);
  tab.file= &error_handler;
  error_handler.print_error(error, MYF(0));
  DBUG_VOID_RETURN;
}

/**
 * Set a given location from full pathname to database name
 *
 */
void ha_ndbcluster::set_dbname(const char *path_name, char *dbname)
{
  char *end, *ptr;
  
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
  memcpy(dbname, ptr + 1, name_len);
  dbname[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  
  ptr= dbname;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
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
  char *end, *ptr;
  
  /* Scan name from the end */
  end= strend(path_name)-1;
  ptr= end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(tabname, ptr + 1, end - ptr);
  tabname[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  ptr= tabname;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
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
  
  DBUG_RETURN(10); /* Good guess when you don't know anything */
}

ulong ha_ndbcluster::table_flags(void) const
{
  if (m_ha_not_exact_count)
    return m_table_flags | HA_NOT_EXACT_COUNT;
  else
    return m_table_flags;
}
const char * ha_ndbcluster::table_type() const 
{
  return("ndbcluster");
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
bool ha_ndbcluster::low_byte_first() const
{ 
#ifdef WORDS_BIGENDIAN
  return FALSE;
#else
  return TRUE;
#endif
}
bool ha_ndbcluster::has_transactions()
{
  return m_transaction_on;
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
  DBUG_ENTER("ndb_get_commitcount");

  char name[FN_REFLEN];
  NDB_SHARE *share;
  (void)strxnmov(name, FN_REFLEN, "./",dbname,"/",tabname,NullS);
  DBUG_PRINT("enter", ("name: %s", name));
  pthread_mutex_lock(&ndbcluster_mutex);
  if (!(share=(NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                       (byte*) name,
                                       strlen(name))))
  {
    pthread_mutex_unlock(&ndbcluster_mutex);
    DBUG_PRINT("info", ("Table %s not found in ndbcluster_open_tables",
                        name));
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
      DBUG_PRINT("info", ("Getting commit_count: %llu from share",
                          share->commit_count));
      pthread_mutex_unlock(&share->mutex);
      free_share(share);
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
  if (ndb_get_table_statistics(ndb, tabname, &stat))
  {
    free_share(share);
    DBUG_RETURN(1);
  }

  pthread_mutex_lock(&share->mutex);
  if (share->commit_count_lock == lock)
  {
    DBUG_PRINT("info", ("Setting commit_count to %llu", stat.commit_count));
    share->commit_count= stat.commit_count;
    *commit_count= stat.commit_count;
  }
  else
  {
    DBUG_PRINT("info", ("Discarding commit_count, comit_count_lock changed"));
    *commit_count= 0;
  }
  pthread_mutex_unlock(&share->mutex);
  free_share(share);
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
  DBUG_ENTER("ndbcluster_cache_retrieval_allowed");

  Uint64 commit_count;
  bool is_autocommit= !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
  char *dbname= full_name;
  char *tabname= dbname+strlen(dbname)+1;

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
  DBUG_PRINT("info", ("*engine_data: %llu, commit_count: %llu",
                      *engine_data, commit_count));
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

  DBUG_PRINT("exit", ("OK to use cache, engine_data: %llu", *engine_data));
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
  DBUG_ENTER("ha_ndbcluster::register_query_cache_table");

  bool is_autocommit= !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));

  DBUG_PRINT("enter",("dbname: %s, tabname: %s, is_autocommit: %d",
		      m_dbname, m_tabname, is_autocommit));

  if (!is_autocommit)
  {
    DBUG_PRINT("exit", ("Can't register table during transaction"))
    DBUG_RETURN(FALSE);
  }

  Uint64 commit_count;
  if (ndb_get_commitcount(thd, m_dbname, m_tabname, &commit_count))
  {
    *engine_data= 0;
    DBUG_PRINT("exit", ("Error, could not get commitcount"))
    DBUG_RETURN(FALSE);
  }
  *engine_data= commit_count;
  *engine_callback= ndbcluster_cache_retrieval_allowed;
  DBUG_PRINT("exit", ("commit_count: %llu", commit_count));
  DBUG_RETURN(commit_count > 0);
}


/*
  Handling the shared NDB_SHARE structure that is needed to
  provide table locking.
  It's also used for sharing data with other NDB handlers
  in the same MySQL Server. There is currently not much
  data we want to or can share.
 */

static byte* ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}

static NDB_SHARE* get_share(const char *table_name)
{
  NDB_SHARE *share;
  pthread_mutex_lock(&ndbcluster_mutex);
  uint length=(uint) strlen(table_name);
  if (!(share=(NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                       (byte*) table_name,
                                       length)))
  {
    if ((share=(NDB_SHARE *) my_malloc(sizeof(*share)+length+1,
                                       MYF(MY_WME | MY_ZEROFILL))))
    {
      share->table_name_length=length;
      share->table_name=(char*) (share+1);
      strmov(share->table_name,table_name);
      if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
      {
        pthread_mutex_unlock(&ndbcluster_mutex);
        my_free((gptr) share,0);
        return 0;
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
      share->commit_count= 0;
      share->commit_count_lock= 0;
    }
    else
    {
      DBUG_PRINT("error", ("Failed to alloc share"));
      pthread_mutex_unlock(&ndbcluster_mutex);
      return 0;
    }
  }
  share->use_count++;

  DBUG_PRINT("share",
	     ("table_name: %s, length: %d, use_count: %d, commit_count: %d",
	      share->table_name, share->table_name_length, share->use_count,
	      share->commit_count));
  pthread_mutex_unlock(&ndbcluster_mutex);
  return share;
}


static void free_share(NDB_SHARE *share)
{
  pthread_mutex_lock(&ndbcluster_mutex);
  if (!--share->use_count)
  {
     hash_delete(&ndbcluster_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&ndbcluster_mutex);
}



/*
  Internal representation of the frm blob
   
*/

struct frm_blob_struct 
{
  struct frm_blob_header 
  {
    uint ver;      // Version of header
    uint orglen;   // Original length of compressed data
    uint complen;  // Compressed length of data, 0=uncompressed
  } head;
  char data[1];  
};



static int packfrm(const void *data, uint len, 
                   const void **pack_data, uint *pack_len)
{
  int error;
  ulong org_len, comp_len;
  uint blob_len;
  frm_blob_struct* blob;
  DBUG_ENTER("packfrm");
  DBUG_PRINT("enter", ("data: %x, len: %d", data, len));
  
  error= 1;
  org_len= len;
  if (my_compress((byte*)data, &org_len, &comp_len))
    goto err;
  
  DBUG_PRINT("info", ("org_len: %d, comp_len: %d", org_len, comp_len));
  DBUG_DUMP("compressed", (char*)data, org_len);
  
  error= 2;
  blob_len= sizeof(frm_blob_struct::frm_blob_header)+org_len;
  if (!(blob= (frm_blob_struct*) my_malloc(blob_len,MYF(MY_WME))))
    goto err;
  
  // Store compressed blob in machine independent format
  int4store((char*)(&blob->head.ver), 1);
  int4store((char*)(&blob->head.orglen), comp_len);
  int4store((char*)(&blob->head.complen), org_len);
  
  // Copy frm data into blob, already in machine independent format
  memcpy(blob->data, data, org_len);  
  
  *pack_data= blob;
  *pack_len= blob_len;
  error= 0;
  
  DBUG_PRINT("exit", ("pack_data: %x, pack_len: %d", *pack_data, *pack_len));
err:
  DBUG_RETURN(error);
  
}


static int unpackfrm(const void **unpack_data, uint *unpack_len,
                    const void *pack_data)
{
   const frm_blob_struct *blob= (frm_blob_struct*)pack_data;
   byte *data;
   ulong complen, orglen, ver;
   DBUG_ENTER("unpackfrm");
   DBUG_PRINT("enter", ("pack_data: %x", pack_data));

   complen=     uint4korr((char*)&blob->head.complen);
   orglen=      uint4korr((char*)&blob->head.orglen);
   ver=         uint4korr((char*)&blob->head.ver);
 
   DBUG_PRINT("blob",("ver: %d complen: %d orglen: %d",
                     ver,complen,orglen));
   DBUG_DUMP("blob->data", (char*) blob->data, complen);
 
   if (ver != 1)
     DBUG_RETURN(1);
   if (!(data= my_malloc(max(orglen, complen), MYF(MY_WME))))
     DBUG_RETURN(2);
   memcpy(data, blob->data, complen);
 
   if (my_uncompress(data, &complen, &orglen))
   {
     my_free((char*)data, MYF(0));
     DBUG_RETURN(3);
   }

   *unpack_data= data;
   *unpack_len= complen;

   DBUG_PRINT("exit", ("frmdata: %x, len: %d", *unpack_data, *unpack_len));

   DBUG_RETURN(0);
}

static 
int
ndb_get_table_statistics(Ndb* ndb, const char * table,
                         struct Ndb_statistics * ndbstat)
{
  DBUG_ENTER("ndb_get_table_statistics");
  DBUG_PRINT("enter", ("table: %s", table));
  NdbTransaction* pTrans= ndb->startTransaction();
  do 
  {
    if (pTrans == NULL)
      break;
      
    NdbScanOperation* pOp= pTrans->getNdbScanOperation(table);
    if (pOp == NULL)
      break;
    
    if (pOp->readTuples(NdbOperation::LM_CommittedRead))
      break;
    
    int check= pOp->interpret_exit_last_row();
    if (check == -1)
      break;
    
    Uint64 rows, commits, mem;
    Uint32 size;
    pOp->getValue(NdbDictionary::Column::ROW_COUNT, (char*)&rows);
    pOp->getValue(NdbDictionary::Column::COMMIT_COUNT, (char*)&commits);
    pOp->getValue(NdbDictionary::Column::ROW_SIZE, (char*)&size);
    pOp->getValue(NdbDictionary::Column::FRAGMENT_MEMORY, (char*)&mem);
    
    check= pTrans->execute(NdbTransaction::NoCommit,
                           NdbTransaction::AbortOnError,
                           TRUE);
    if (check == -1)
      break;
    
    Uint32 count= 0;
    Uint64 sum_rows= 0;
    Uint64 sum_commits= 0;
    Uint64 sum_row_size= 0;
    Uint64 sum_mem= 0;
    while ((check= pOp->nextResult(TRUE, TRUE)) == 0)
    {
      sum_rows+= rows;
      sum_commits+= commits;
      if (sum_row_size < size)
        sum_row_size= size;
      sum_mem+= mem;
      count++;
    }
    
    if (check == -1)
      break;

    pOp->close(TRUE);

    ndb->closeTransaction(pTrans);

    ndbstat->row_count= sum_rows;
    ndbstat->commit_count= sum_commits;
    ndbstat->row_size= sum_row_size;
    ndbstat->fragment_memory= sum_mem;

    DBUG_PRINT("exit", ("records: %llu commits: %llu "
                        "row_size: %llu mem: %llu count: %u",
			sum_rows, sum_commits, sum_row_size,
                        sum_mem, count));

    DBUG_RETURN(0);
  } while (0);

  if (pTrans)
    ndb->closeTransaction(pTrans);
  DBUG_PRINT("exit", ("failed"));
  DBUG_RETURN(-1);
}

/*
  Create a .ndb file to serve as a placeholder indicating 
  that the table with this name is a ndb table
*/

int ha_ndbcluster::write_ndb_file()
{
  File file;
  bool error=1;
  char path[FN_REFLEN];
  
  DBUG_ENTER("write_ndb_file");
  DBUG_PRINT("enter", ("db: %s, name: %s", m_dbname, m_tabname));

  (void)strxnmov(path, FN_REFLEN, 
                 mysql_data_home,"/",m_dbname,"/",m_tabname,ha_ndb_ext,NullS);

  if ((file=my_create(path, CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    // It's an empty file
    error=0;
    my_close(file,MYF(0));
  }
  DBUG_RETURN(error);
}

int
ha_ndbcluster::read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                                      KEY_MULTI_RANGE *ranges, 
                                      uint range_count,
                                      bool sorted, 
                                      HANDLER_BUFFER *buffer)
{
  DBUG_ENTER("ha_ndbcluster::read_multi_range_first");
  
  int res;
  KEY* key_info= table->key_info + active_index;
  NDB_INDEX_TYPE index_type= get_index_type(active_index);
  ulong reclength= table->s->reclength;
  NdbOperation* op;

  if (uses_blob_value(m_retrieve_all_fields))
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
  const NDBTAB *tab= (const NDBTAB *) m_table;
  const NDBINDEX *unique_idx= (NDBINDEX *) m_index[active_index].unique_index;
  const NDBINDEX *idx= (NDBINDEX *) m_index[active_index].index; 
  const NdbOperation* lastOp= m_active_trans->getLastDefinedOperation();
  NdbIndexScanOperation* scanOp= 0;
  for (; multi_range_curr<multi_range_end && curr+reclength <= end_of_buffer; 
       multi_range_curr++)
  {
    switch (index_type){
    case PRIMARY_KEY_ORDERED_INDEX:
      if (!(multi_range_curr->start_key.length == key_info->key_length &&
            multi_range_curr->start_key.flag == HA_READ_KEY_EXACT))
      goto range;
      /* fall through */
    case PRIMARY_KEY_INDEX:
    {
      multi_range_curr->range_flag |= UNIQUE_RANGE;
      if ((op= m_active_trans->getNdbOperation(tab)) && 
          !op->readTuple(lm) && 
          !set_primary_key(op, multi_range_curr->start_key.key) &&
          !define_read_attrs(curr, op) &&
          (op->setAbortOption(AO_IgnoreError), TRUE))
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
      /* fall through */
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
    case ORDERED_INDEX:
    {
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
                 &&!scanOp->readTuples(lm, 0, parallelism, sorted, FALSE, TRUE)
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
      if ((res= set_bounds(scanOp, keys, multi_range_curr-ranges)))
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
  if (!(res= execute_no_commit_ie(this, m_active_trans)))
  {
    m_multi_range_defined= multi_range_curr;
    multi_range_curr= ranges;
    m_multi_range_result_ptr= (byte*)buffer->buffer;
    DBUG_RETURN(read_multi_range_next(found_range_p));
  }
  ERR_RETURN(m_active_trans->getNdbError());
}

#if 0
#define DBUG_MULTI_RANGE(x) printf("read_multi_range_next: case %d\n", x);
#else
#define DBUG_MULTI_RANGE(x)
#endif

int
ha_ndbcluster::read_multi_range_next(KEY_MULTI_RANGE ** multi_range_found_p)
{
  DBUG_ENTER("ha_ndbcluster::read_multi_range_next");
  if (m_disable_multi_read)
  {
    DBUG_RETURN(handler::read_multi_range_next(multi_range_found_p));
  }
  
  int res;
  int range_no;
  ulong reclength= table->s->reclength;
  const NdbOperation* op= m_current_multi_operation;
  for (;multi_range_curr < m_multi_range_defined; multi_range_curr++)
  {
    if (multi_range_curr->range_flag & UNIQUE_RANGE)
    {
      if (op->getNdbError().code == 0)
        goto found_next;
      
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
      DBUG_RETURN(ndb_err(m_active_trans));
    }
  }
  
  if (multi_range_curr == multi_range_end)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  
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
  
  end= table->field + table->s->fields;
  
  for (field= table->field; field < end; field++, value++)
  {
    if ((* value).ptr)
    {
      DBUG_ASSERT(curr != 0);
      (* value).rec= curr;
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
  const NDBTAB* tab;
  if (!(tab= dict->getTable(m_tabname)))
  {
    return((char*)comment);
  }

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
extern "C" pthread_handler_decl(ndb_util_thread_func,
                                arg __attribute__((unused)))
{
  THD *thd; /* needs to be first for thread_stack */
  Ndb* ndb;
  int error= 0;
  struct timespec abstime;

  my_thread_init();
  DBUG_ENTER("ndb_util_thread");
  DBUG_PRINT("enter", ("ndb_cache_check_time: %d", ndb_cache_check_time));

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  THD_CHECK_SENTRY(thd);
  ndb= new Ndb(g_ndb_cluster_connection, "");

  pthread_detach_this_thread();
  ndb_util_thread= pthread_self();

  thd->thread_stack= (char*)&thd; /* remember where our stack is */
  if (thd->store_globals() && (ndb->init() != -1))
  {
    thd->cleanup();
    delete thd;
    delete ndb;
    DBUG_RETURN(NULL);
  }

  List<NDB_SHARE> util_open_tables;
  set_timespec(abstime, 0);
  for (;;)
  {

    pthread_mutex_lock(&LOCK_ndb_util_thread);
    error= pthread_cond_timedwait(&COND_ndb_util_thread,
                                  &LOCK_ndb_util_thread,
                                  &abstime);
    pthread_mutex_unlock(&LOCK_ndb_util_thread);

    DBUG_PRINT("ndb_util_thread", ("Started, ndb_cache_check_time: %d",
                                   ndb_cache_check_time));

    if (abort_loop)
      break; /* Shutting down server */

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
      /* Split tab- and dbname */
      char buf[FN_REFLEN];
      char *tabname, *db;
      uint length= dirname_length(share->table_name);
      tabname= share->table_name+length;
      memcpy(buf, share->table_name, length-1);
      buf[length-1]= 0;
      db= buf+dirname_length(buf);
      DBUG_PRINT("ndb_util_thread",
                 ("Fetching commit count for: %s",
                  share->table_name));

      /* Contact NDB to get commit count for table */
      ndb->setDatabaseName(db);
      struct Ndb_statistics stat;

      uint lock;
      pthread_mutex_lock(&share->mutex);
      lock= share->commit_count_lock;
      pthread_mutex_unlock(&share->mutex);

      if (ndb_get_table_statistics(ndb, tabname, &stat) == 0)
      {
        DBUG_PRINT("ndb_util_thread",
                   ("Table: %s, commit_count: %llu, rows: %llu",
                    share->table_name, stat.commit_count, stat.row_count));
      }
      else
      {
        DBUG_PRINT("ndb_util_thread",
                   ("Error: Could not get commit count for table %s",
                    share->table_name));
        stat.commit_count= 0;
      }

      pthread_mutex_lock(&share->mutex);
      if (share->commit_count_lock == lock)
        share->commit_count= stat.commit_count;
      pthread_mutex_unlock(&share->mutex);

      /* Decrease the use count and possibly free share */
      free_share(share);
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
                type != MYSQL_TYPE_BIT)
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
            DBUG_PRINT("info", ("Was not expecting field from table %s(%s)",
                                context->table->s->table_name, 
                                field->table->s->table_name));
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
          context->rewrite_stack= context->rewrite_stack->next;
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

int
ndbcluster_show_status(THD* thd)
{
  Protocol *protocol= thd->protocol;
  
  DBUG_ENTER("ndbcluster_show_status");
  
  if (have_ndbcluster != SHOW_OPTION_YES) 
  {
    my_message(ER_NOT_SUPPORTED_YET,
	       "Cannot call SHOW NDBCLUSTER STATUS because skip-ndbcluster is defined",
	       MYF(0));
    DBUG_RETURN(TRUE);
  }
  
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("free_list", 255));
  field_list.push_back(new Item_return_int("created", 10,MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("free", 10,MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("sizeof", 10,MYSQL_TYPE_LONG));

  if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);
  
  if (get_thd_ndb(thd) && get_thd_ndb(thd)->ndb)
  {
    Ndb* ndb= (get_thd_ndb(thd))->ndb;
    Ndb::Free_list_usage tmp; tmp.m_name= 0;
    while (ndb->get_free_list_usage(&tmp))
    {
      protocol->prepare_for_resend();
      
      protocol->store(tmp.m_name, &my_charset_bin);
      protocol->store((uint)tmp.m_created);
      protocol->store((uint)tmp.m_free);
      protocol->store((uint)tmp.m_sizeof);
      if (protocol->write())
	DBUG_RETURN(TRUE);
    }
  }
  send_eof(thd);
  
  DBUG_RETURN(FALSE);
}

#endif /* HAVE_NDBCLUSTER_DB */

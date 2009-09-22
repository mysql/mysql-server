/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/


/**
  @file ha_ibmdb2i.cc

  @brief
  The ha_ibmdb2i storage engine provides an interface from MySQL to IBM DB2 for i.

*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation        // gcc: Class implementation
#endif

#include "ha_ibmdb2i.h"
#include "mysql_priv.h"
#include <mysql/plugin.h>
#include "db2i_ileBridge.h"
#include "db2i_charsetSupport.h"
#include <sys/utsname.h>
#include "db2i_safeString.h"

static const char __NOT_NULL_VALUE_EBCDIC = 0xF0; // '0'
static const char __NULL_VALUE_EBCDIC = 0xF1; // '1'
static const char __DEFAULT_VALUE_EBCDIC = 0xC4; // 'D'
static const char BlankASPName[19] = "                  ";
static const int DEFAULT_MAX_ROWS_TO_BUFFER = 4096;

static const char SAVEPOINT_PREFIX[] = {0xD4, 0xE8, 0xE2, 0xD7}; // MYSP (in EBCDIC)

OSVersion osVersion;


// ================================================================
// ================================================================
// System variables
static char* ibmdb2i_rdb_name;
static MYSQL_SYSVAR_STR(rdb_name, ibmdb2i_rdb_name,
  PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY,
  "The name of the RDB to use",
  NULL, 
  NULL,
  BlankASPName);

static MYSQL_THDVAR_BOOL(transaction_unsafe,
  0,
  "Disable support for commitment control",
  NULL, 
  NULL, 
  FALSE);

static MYSQL_THDVAR_UINT(lob_alloc_size,
  0,
  "Baseline allocation for lob read buffer",
  NULL, 
  NULL, 
  2*1024*1024,
  64*1024,
  128*1024*1024,
  1);

static MYSQL_THDVAR_UINT(max_read_buffer_size,
  0,
  "Maximum size of buffers used for read-ahead.",
  NULL,
  NULL,
  1*1024*1024,
  32*1024,
  16*1024*1024,
  1);
  
static MYSQL_THDVAR_UINT(max_write_buffer_size,
  0,
  "Maximum size of buffers used for bulk writes.",
  NULL,
  NULL,
  8*1024*1024,
  32*1024,
  64*1024*1024,
  1);

static MYSQL_THDVAR_BOOL(compat_opt_time_as_duration,
  0,
  "Control how new TIME columns should be defined in DB2. 0=time-of-day (default), 1=duration.",
  NULL, 
  NULL, 
  FALSE);

static MYSQL_THDVAR_UINT(compat_opt_year_as_int,
  0,
  "Control how new YEAR columns should be defined in DB2. 0=CHAR(4) (default), 1=SMALLINT.",
  NULL, 
  NULL, 
  0,
  0,
  1,
  1);

static MYSQL_THDVAR_UINT(compat_opt_blob_cols,
  0,
  "Control how new TEXT and BLOB columns should be defined in DB2. 0=CLOB/BLOB (default), 1=VARCHAR/VARBINARY",
  NULL, 
  NULL, 
  0,
  0,
  1,
  1);

static MYSQL_THDVAR_UINT(compat_opt_allow_zero_date_vals,
  0,
  "Allow substitute values to be used when storing a column with a 0000-00-00 date component. 0=No substitution (default), 1=Substitute '0001-01-01'",
  NULL, 
  NULL, 
  0,
  0,
  1,
  1);

static MYSQL_THDVAR_BOOL(propagate_default_col_vals,
  0,
  "Should DEFAULT column values be propagated to the DB2 table definition.",
  NULL, 
  NULL, 
  TRUE);

static my_bool ibmdb2i_assume_exclusive_use;
static MYSQL_SYSVAR_BOOL(assume_exclusive_use, ibmdb2i_assume_exclusive_use,
  0,
  "Can MySQL assume that this process is the only one modifying the DB2 tables. ",
  NULL, 
  NULL, 
  FALSE);

static MYSQL_THDVAR_BOOL(async_enabled,
  0,
  "Should reads be done asynchronously when possible",
  NULL, 
  NULL, 
  TRUE);

static MYSQL_THDVAR_UINT(create_index_option,
  0,
  "Control whether additional indexes are created. 0=No (default), 1=Create additional *HEX-based index",
  NULL,
  NULL,
  0,
  0,
  1,
  1);

/* static MYSQL_THDVAR_UINT(discovery_mode,
  0,
  "Unsupported",
  NULL,
  NULL,
  0,
  0,
  1,
  1); */

static uint32 ibmdb2i_system_trace;
static MYSQL_SYSVAR_UINT(system_trace_level, ibmdb2i_system_trace,
  0,
  "Set system tracing level",
  NULL, 
  NULL, 
  0,
  0,
  63,
  1);


inline uint8 ha_ibmdb2i::getCommitLevel(THD* thd)
{
  if (!THDVAR(thd, transaction_unsafe))
  {    
    switch (thd_tx_isolation(thd))
    {
      case ISO_READ_UNCOMMITTED: 
        return (accessIntent == QMY_READ_ONLY ? QMY_READ_UNCOMMITTED : QMY_REPEATABLE_READ);
      case ISO_READ_COMMITTED: 
        return (accessIntent == QMY_READ_ONLY ? QMY_READ_COMMITTED : QMY_REPEATABLE_READ);
      case ISO_REPEATABLE_READ: 
        return QMY_REPEATABLE_READ;
      case ISO_SERIALIZABLE: 
        return QMY_SERIALIZABLE;
    }
  }

  return QMY_NONE;
}

inline uint8 ha_ibmdb2i::getCommitLevel()
{
  return getCommitLevel(ha_thd());
}

//=====================================================================

static handler *ibmdb2i_create_handler(handlerton *hton,
                                       TABLE_SHARE *table, 
                                       MEM_ROOT *mem_root);
static void ibmdb2i_drop_database(handlerton *hton, char* path);
static int ibmdb2i_savepoint_set(handlerton *hton, THD* thd, void *sv);
static int ibmdb2i_savepoint_rollback(handlerton *hton, THD* thd, void *sv);
static int ibmdb2i_savepoint_release(handlerton *hton, THD* thd, void *sv);
static uint ibmdb2i_alter_table_flags(uint flags);

handlerton *ibmdb2i_hton;
static bool was_ILE_inited;

/* Tracks the number of open tables */
static HASH ibmdb2i_open_tables;

/* Mutex used to synchronize initialization of the hash */
static pthread_mutex_t ibmdb2i_mutex;


/**
  Create hash key for tracking open tables.
*/

static uchar* ibmdb2i_get_key(IBMDB2I_SHARE *share,size_t *length,
                             bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (uchar*) share->table_name;
}


int ibmdb2i_close_connection(handlerton* hton, THD *thd)
{
  DBUG_PRINT("ha_ibmdb2i::close_connection", ("Closing %d", thd->thread_id));
  db2i_ileBridge::getBridgeForThread(thd)->closeConnection(thd->thread_id);
  db2i_ileBridge::destroyBridgeForThread(thd);
  
  return 0;  
}


static int ibmdb2i_init_func(void *p)
{
  DBUG_ENTER("ibmdb2i_init_func");
  
  utsname tempName;
  uname(&tempName);
  osVersion.v = atoi(tempName.version);
  osVersion.r = atoi(tempName.release);
  
  was_ILE_inited = false;
  ibmdb2i_hton= (handlerton *)p;
  VOID(pthread_mutex_init(&ibmdb2i_mutex,MY_MUTEX_INIT_FAST));
  (void) hash_init(&ibmdb2i_open_tables,system_charset_info,32,0,0,
                   (hash_get_key) ibmdb2i_get_key,0,0);

  ibmdb2i_hton->state=   SHOW_OPTION_YES;
  ibmdb2i_hton->create=  ibmdb2i_create_handler;
  ibmdb2i_hton->drop_database= ibmdb2i_drop_database;
  ibmdb2i_hton->commit=  ha_ibmdb2i::doCommit;
  ibmdb2i_hton->rollback= ha_ibmdb2i::doRollback;
  ibmdb2i_hton->savepoint_offset= 0;
  ibmdb2i_hton->savepoint_set= ibmdb2i_savepoint_set;
  ibmdb2i_hton->savepoint_rollback= ibmdb2i_savepoint_rollback;
  ibmdb2i_hton->savepoint_release= ibmdb2i_savepoint_release;
  ibmdb2i_hton->alter_table_flags=ibmdb2i_alter_table_flags;
  ibmdb2i_hton->close_connection=ibmdb2i_close_connection;

  int rc;
  
  rc = initCharsetSupport();
    
  if (!rc)
    rc = db2i_ileBridge::setup();
  
  if (!rc)
  {
    int nameLen = strlen(ibmdb2i_rdb_name);
    for (int i = 0; i < nameLen; ++i)
    {
      ibmdb2i_rdb_name[i] = my_toupper(system_charset_info, (uchar)ibmdb2i_rdb_name[i]);
    }    
    
    rc = db2i_ileBridge::initILE(ibmdb2i_rdb_name, (uint16*)(((char*)&ibmdb2i_system_trace)+2));
    if (rc == 0)
    {
      was_ILE_inited = true;
    }
  }
  
  DBUG_RETURN(rc);
}


static int ibmdb2i_done_func(void *p)
{
  int error= 0;
  DBUG_ENTER("ibmdb2i_done_func");

  if (ibmdb2i_open_tables.records)
    error= 1;  
  
  if (was_ILE_inited)
    db2i_ileBridge::exitILE();

  db2i_ileBridge::takedown();
  
  doneCharsetSupport();

  hash_free(&ibmdb2i_open_tables);
  pthread_mutex_destroy(&ibmdb2i_mutex);
  
  DBUG_RETURN(0);
}


IBMDB2I_SHARE *ha_ibmdb2i::get_share(const char *table_name, TABLE *table)
{
  IBMDB2I_SHARE *share;
  uint length;
  char *tmp_name;
  
  pthread_mutex_lock(&ibmdb2i_mutex);
  length=(uint) strlen(table_name);

  if (!(share=(IBMDB2I_SHARE*) hash_search(&ibmdb2i_open_tables,
                                           (uchar*)table_name,
                                           length)))
  {
    if (!(share=(IBMDB2I_SHARE *)
          my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                          &share, sizeof(*share),
                          &tmp_name, length+1,
                          NullS)))
    {
      pthread_mutex_unlock(&ibmdb2i_mutex);
      return NULL;
    }

    share->use_count=0;
    share->table_name_length=length;
    share->table_name=tmp_name;
    strmov(share->table_name,table_name);
    if (my_hash_insert(&ibmdb2i_open_tables, (uchar*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutexattr_t mutexattr = MY_MUTEX_INIT_FAST;
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&share->mutex, &mutexattr);
    
    share->db2Table = new db2i_table(table->s, table_name);
    int32 rc = share->db2Table->initDB2Objects(table_name);
    
    if (rc)
    {
      delete share->db2Table;
      hash_delete(&ibmdb2i_open_tables, (uchar*) share);
      thr_lock_delete(&share->lock);
      my_errno = rc;
      goto error;
    }
    
    memset(&share->cachedStats, 0, sizeof(share->cachedStats));
  }
  share->use_count++;
  pthread_mutex_unlock(&ibmdb2i_mutex);

  db2Table = share->db2Table;
  
  return share;

error:
  pthread_mutex_destroy(&share->mutex);
  my_free((uchar*) share, MYF(0));
  pthread_mutex_unlock(&ibmdb2i_mutex);

  return NULL;
}



int ha_ibmdb2i::free_share(IBMDB2I_SHARE *share)
{
  pthread_mutex_lock(&ibmdb2i_mutex);
  if (!--share->use_count)
  {
    delete share->db2Table;
    db2Table = NULL;

    hash_delete(&ibmdb2i_open_tables, (uchar*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free(share, MYF(0));
    pthread_mutex_unlock(&ibmdb2i_mutex);
    return 1;
  }
  pthread_mutex_unlock(&ibmdb2i_mutex);

  return 0;
}

static handler* ibmdb2i_create_handler(handlerton *hton,
                                       TABLE_SHARE *table, 
                                       MEM_ROOT *mem_root)
{
  return new (mem_root) ha_ibmdb2i(hton, table);
}

static void ibmdb2i_drop_database(handlerton *hton, char* path)
{
  DBUG_ENTER("ha_ibmdb2i::ibmdb2i_drop_database");
  int rc = 0;
  char queryBuffer[200];
  String query(queryBuffer, sizeof(queryBuffer), system_charset_info);
  query.length(0);
  query.append(STRING_WITH_LEN(" DROP SCHEMA \""));
  query.append(path+2, strchr(path+2, '/')-(path+2));
  query.append('"');
  
  SqlStatementStream sqlStream(query);
  
  rc = db2i_ileBridge::getBridgeForThread()->execSQL(sqlStream.getPtrToData(),
                                              sqlStream.getStatementCount(),
                                              QMY_NONE,
                                              FALSE,
                                              TRUE);
  DBUG_VOID_RETURN;
}

inline static void genSavepointName(const void* sv, char* out)
{
  *(uint32*)out = *(uint32*)SAVEPOINT_PREFIX;
  DBUG_ASSERT(sizeof(SAVEPOINT_PREFIX) == 4);
  out += sizeof(SAVEPOINT_PREFIX);
  
  longlong2str((longlong)sv, out, 10);
  while (*out)
  {
    out += 0xF0;
    ++out;
  }
}


/*********************************************************************
Sets a transaction savepoint. */
static int ibmdb2i_savepoint_set(handlerton* hton, THD* thd, void* sv) 
{
  DBUG_ENTER("ibmdb2i_savepoint_set");
  int rc = 0;
  if (!THDVAR(thd ,transaction_unsafe))
  {
    char name[64];
    genSavepointName(sv, name);
    DBUG_PRINT("ibmdb2i_savepoint_set",("Setting %s", name));
    rc = ha_ibmdb2i::doSavepointSet(thd, name);
  } 
  DBUG_RETURN(rc);
}


/*********************************************************************
Rollback a savepoint. */
static int ibmdb2i_savepoint_rollback(handlerton* hton, THD* thd, void* sv) 
{
  DBUG_ENTER("ibmdb2i_savepoint_rollback");
  int rc = 0;
  if (!THDVAR(thd,transaction_unsafe))
  {
    char name[64];
    genSavepointName(sv, name);
    DBUG_PRINT("ibmdb2i_savepoint_rollback",("Rolling back %s", name));
    rc = ha_ibmdb2i::doSavepointRollback(thd, name);
  } 
  DBUG_RETURN(rc);  
}


/*********************************************************************
Release a savepoint. */
static int ibmdb2i_savepoint_release(handlerton* hton, THD* thd, void* sv) 
{
  DBUG_ENTER("ibmdb2i_savepoint_release");
  int rc = 0;
  if (!THDVAR(thd,transaction_unsafe))
  {
    char name[64];
    genSavepointName(sv, name);
    DBUG_PRINT("ibmdb2i_savepoint_release",("Releasing %s", name));
    rc = ha_ibmdb2i::doSavepointRelease(thd, name);
  } 
  DBUG_RETURN(rc);
}

/* Thse flags allow for the online add and drop of an index via the CREATE INDEX,
   DROP INDEX, and ALTER TABLE statements. These flags indicate that MySQL is not
   required to lock the table before calling the storage engine to add or drop the
   index(s).  */                    
static uint ibmdb2i_alter_table_flags(uint flags)
{
  return (HA_ONLINE_ADD_INDEX | HA_ONLINE_DROP_INDEX |
          HA_ONLINE_ADD_UNIQUE_INDEX | HA_ONLINE_DROP_UNIQUE_INDEX |
          HA_ONLINE_ADD_PK_INDEX | HA_ONLINE_DROP_PK_INDEX); 
}

ha_ibmdb2i::ha_ibmdb2i(handlerton *hton, TABLE_SHARE *table_arg)
  :share(NULL), handler(hton, table_arg),
   activeHandle(0), dataHandle(0),
    activeReadBuf(NULL), activeWriteBuf(NULL),
    blobReadBuffers(NULL), accessIntent(QMY_UPDATABLE), currentRRN(0),
    releaseRowNeeded(FALSE),
    indexReadSizeEstimates(NULL),
    outstanding_start_bulk_insert(false),
    last_rnd_init_rc(0),
    last_index_init_rc(0),
    last_start_bulk_insert_rc(0),
    autoIncLockAcquired(false),
    got_auto_inc_values(false),
    next_identity_value(0),
    indexHandles(0),
    returnDupKeysImmediately(false),
    onDupUpdate(false), 
    blobWriteBuffers(NULL),
    forceSingleRowRead(false)
 {
   activeReferences = 0;
   ref_length = sizeof(currentRRN);
   if (table_share && table_share->keys > 0)
   {
     indexHandles = (FILE_HANDLE*)my_malloc(table_share->keys * sizeof(FILE_HANDLE), MYF(MY_WME | MY_ZEROFILL));
   }
   clear_alloc_root(&conversionBufferMemroot);
 }


ha_ibmdb2i::~ha_ibmdb2i()
{
  DBUG_ASSERT(activeReferences == 0 || outstanding_start_bulk_insert);
    
  if (indexHandles)
    my_free(indexHandles, MYF(0));
  if (indexReadSizeEstimates)
    my_free(indexReadSizeEstimates, MYF(0));
  
  cleanupBuffers();
}
 
 
static const char *ha_ibmdb2i_exts[] = {
  FID_EXT,
  NullS
};

const char **ha_ibmdb2i::bas_ext() const
{
  return ha_ibmdb2i_exts;
}


int ha_ibmdb2i::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_ibmdb2i::open");

  initBridge();
  
  dataHandle = bridge()->findAndRemovePreservedHandle(name, &share);
  
  if (share)
    db2Table = share->db2Table;
  
  if (!share && (!(share = get_share(name, table))))
    DBUG_RETURN(my_errno);
  thr_lock_data_init(&share->lock,&lock,NULL);

  info(HA_STATUS_NO_LOCK | HA_STATUS_CONST | HA_STATUS_VARIABLE);

    
  DBUG_RETURN(0);
}




int ha_ibmdb2i::close(void)
{
  DBUG_ENTER("ha_ibmdb2i::close");
  int32 rc = 0;
  bool preserveShare = false;
  
  db2i_ileBridge* bridge = db2i_ileBridge::getBridgeForThread();
  
  if (dataHandle)
  {
    if (bridge->expectErrors(QMY_ERR_PEND_LOCKS)->deallocateFile(dataHandle, FALSE) == QMY_ERR_PEND_LOCKS)
    {
      bridge->preserveHandle(share->table_name, dataHandle, share);
      preserveShare = true;
    }
    dataHandle = 0;
  }

  for (int idx = 0; idx < table_share->keys; ++idx)
  {
    if (indexHandles[idx] != 0)
    {
      bridge->deallocateFile(indexHandles[idx], FALSE);
    }
  }
  
  cleanupBuffers();
    
  if (!preserveShare)
  {
    if (free_share(share))
      share = NULL;
  }
  
  DBUG_RETURN(rc);
}



int ha_ibmdb2i::write_row(uchar * buf)
{  

  DBUG_ENTER("ha_ibmdb2i::write_row");
  
  if (last_start_bulk_insert_rc)
    DBUG_RETURN( last_start_bulk_insert_rc );
  
  ha_statistic_increment(&SSV::ha_write_count);
  int rc = 0;

  bool fileHandleNeedsRelease = false;
  
  if (!activeHandle)
  {
    rc = useDataFile();
    if (rc) DBUG_RETURN(rc);
    fileHandleNeedsRelease = true;
  }
      
  if (!outstanding_start_bulk_insert)
    rc = prepWriteBuffer(1, getFileForActiveHandle());
  
  if (!rc)
  {
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
      table->timestamp_field->set_time();

    char* writeBuffer = activeWriteBuf->addRow();
    rc = prepareRowForWrite(writeBuffer, 
                            writeBuffer+activeWriteBuf->getRowNullOffset(),
                            true);
    if (rc == 0)
    {
      // If we are doing block inserts, if the MI is supposed to generate an auto_increment
      // (i.e. identity column) value for this record, and if this is not the first record in
      // the block, then store the value (that the MI will generate for the identity column)
      // into the MySQL write buffer. We can predetermine the value because the file is locked.    

      if ((autoIncLockAcquired) && (default_identity_value) && (got_auto_inc_values))
      { 
        if (unlikely((next_identity_value - 1) == 
                      maxValueForField(table->next_number_field)))
        {
          rc = QMY_ERR_MAXVALUE;
        }
        else
        {
          rc = table->next_number_field->store((longlong) next_identity_value, TRUE);
          next_identity_value = next_identity_value + incrementByValue;
        }
      }
      // If the buffer is full, or if we locked the file and this is the first or last row
      // of a blocked insert, then flush the buffer.    
      if (!rc && (activeWriteBuf->endOfBuffer()) ||
          ((autoIncLockAcquired) &&
           ((!got_auto_inc_values))) ||
          (returnDupKeysImmediately))
        rc = flushWrite(activeHandle, buf);
    }
    else
      activeWriteBuf->deleteRow();
  }
      
  if (fileHandleNeedsRelease)
    releaseActiveHandle();
  
  DBUG_RETURN(rc);
}

/**
  @brief 
  Helper function used by write_row and update_row to prepare the MySQL
  row for insertion into DB2.
*/
int ha_ibmdb2i::prepareRowForWrite(char* data, char* nulls, bool honorIdentCols)
{
  int rc = 0;
  
  // set null map all to non nulls
  memset(nulls,__NOT_NULL_VALUE_EBCDIC, table->s->fields);
  default_identity_value = FALSE;  
    
  ulong sql_mode = ha_thd()->variables.sql_mode;
  
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
  for (Field **field = table->field; *field && !rc; ++field)
  {  
    int fieldIndex = (*field)->field_index;
    if ((*field)->Field::is_null())                        
    {                                                  
      nulls[fieldIndex] = __NULL_VALUE_EBCDIC;         
    }                                                 
    if (honorIdentCols && ((*field)->flags & AUTO_INCREMENT_FLAG) &&
        *field == table->next_number_field) 
//     && ((!autoIncLockAcquired) || (!got_auto_inc_values)))
    {
      if (sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO)
      {
        if (!table->auto_increment_field_not_null)
        {
          nulls[fieldIndex] = __DEFAULT_VALUE_EBCDIC;
          default_identity_value = TRUE; 
        }
      }
      else if ((*field)->val_int() == 0)
      {
        nulls[fieldIndex] = __DEFAULT_VALUE_EBCDIC;
        default_identity_value = TRUE; 
      }
    }   
    
    DB2Field& db2Field = db2Table->db2Field(fieldIndex);
    if (nulls[fieldIndex] == __NOT_NULL_VALUE_EBCDIC ||
        db2Field.isBlob())
    {
      rc = convertMySQLtoDB2(*field, db2Field, data + db2Field.getBufferOffset());
    }
  }  
  
  if (!rc && db2Table->hasBlobs())
    rc = db2i_ileBridge::getBridgeForThread()->objectOverride(activeHandle,
                                                            activeWriteBuf->ptr());  

  dbug_tmp_restore_column_map(table->read_set, old_map);

  return rc; 
}



int ha_ibmdb2i::update_row(const uchar * old_data, uchar * new_data)
{
  DBUG_ENTER("ha_ibmdb2i::update_row");
  ha_statistic_increment(&SSV::ha_update_count);
  int rc;
  
  bool fileHandleNeedsRelease = false;
  
  if (!activeHandle)
  {
    rc = useFileByHandle(QMY_UPDATABLE, rrnAssocHandle);
    if (rc) DBUG_RETURN(rc);
    fileHandleNeedsRelease = true;
  }
    
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  
  char* writeBuf = activeWriteBuf->addRow();
  rc = prepareRowForWrite(writeBuf, 
                          writeBuf+activeWriteBuf->getRowNullOffset(),
                          onDupUpdate);

  char* lastDupKeyNamePtr = NULL;
  uint32 lastDupKeyNameLen = 0;
  
  if (!rc)
  {
    rc = db2i_ileBridge::getBridgeForThread()->updateRow(activeHandle,
                                                       currentRRN,
                                                       activeWriteBuf->ptr(),
                                                       &lastDupKeyRRN,
                                                       &lastDupKeyNamePtr,
                                                       &lastDupKeyNameLen);
  }
  
  if (lastDupKeyNameLen)
  {
    lastDupKeyID = getKeyFromName(lastDupKeyNamePtr, lastDupKeyNameLen);
    rrnAssocHandle = activeHandle;
  }

  if (fileHandleNeedsRelease)
    releaseActiveHandle();
  
  activeWriteBuf->resetAfterWrite();
    
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::delete_row(const uchar * buf)
{
  DBUG_ENTER("ha_ibmdb2i::delete_row");
  ha_statistic_increment(&SSV::ha_delete_count);
  
  bool needReleaseFile = false;
  int rc = 0;
  
  if (!activeHandle) // In some circumstances, MySQL comes here after
  {                  // closing the active handle. We need to re-open.
    rc = useFileByHandle(QMY_UPDATABLE, rrnAssocHandle);    
    needReleaseFile = true;
  }
      
  if (likely(!rc))
  {
    rc = db2i_ileBridge::getBridgeForThread()->deleteRow(activeHandle,
                                                       currentRRN);  
    invalidateCachedStats();
    if (needReleaseFile)
      releaseActiveHandle();
  }
 
  DBUG_RETURN(rc);
}



int ha_ibmdb2i::index_init(uint idx, bool sorted)
{
  DBUG_ENTER("ha_ibmdb2i::index_init");
  
  int& rc = last_index_init_rc;
  rc = 0;
  
  invalidDataFound=false;
  tweakReadSet();
    
  active_index=idx;

  rc = useIndexFile(idx);
  
  if (!rc)
  {
//     THD* thd = ha_thd();
//     if (accessIntent == QMY_UPDATABLE &&
//         thd_tx_isolation(thd) == ISO_REPEATABLE_READ &&
//         !THDVAR(thd, transaction_unsafe))
//     {
//       readAccessIntent = QMY_READ_ONLY;
//     }
//     else
//     {
      readAccessIntent = accessIntent;
//     }
    
    if (!rc && accessIntent != QMY_READ_ONLY)
      rc = prepWriteBuffer(1, db2Table->indexFile(idx));
    
    if (rc)
      releaseIndexFile(idx);
  }
  
  rrnAssocHandle= 0;

  DBUG_RETURN(rc); 
}



int ha_ibmdb2i::index_read(uchar * buf, const uchar * key,
                           uint key_len,
                           enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_ibmdb2i::index_read");

  if (unlikely(last_index_init_rc)) DBUG_RETURN(last_index_init_rc);

  int rc;
  
  ha_rows estimatedRows = getIndexReadEstimate(active_index);
  rc = prepReadBuffer(estimatedRows, db2Table->indexFile(active_index), readAccessIntent);  
  if (unlikely(rc)) DBUG_RETURN(rc);
  
  DBUG_ASSERT(activeReadBuf);
  
  keyBuf.allocBuf(activeReadBuf->getRowLength(), 
                  activeReadBuf->getRowNullOffset(), 
                  activeReadBuf->getRowLength());
  keyBuf.zeroBuf();
  
  char* db2KeyBufPtr = keyBuf.ptr();
  char* nullKeyMap = db2KeyBufPtr + activeReadBuf->getRowNullOffset();
  
  const uchar* keyBegin = key;
  int partsInUse;
  
  KEY& curKey = table->key_info[active_index];
  
  for (partsInUse = 0; partsInUse < curKey.key_parts, key - keyBegin < key_len; ++partsInUse)
  {
    Field* field = curKey.key_part[partsInUse].field;     
    if ((curKey.key_part[partsInUse].null_bit) &&
        (char*)key[0])
    {
      if (field->flags & AUTO_INCREMENT_FLAG)
      {
        table->status = STATUS_NOT_FOUND; 
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }
      else
      {
        nullKeyMap[partsInUse] = __NULL_VALUE_EBCDIC;
      }
    }
    else
    {
      nullKeyMap[partsInUse] = __NOT_NULL_VALUE_EBCDIC;
      convertMySQLtoDB2(field, 
                        db2Table->db2Field(field->field_index),
                        db2KeyBufPtr, 
                        (uchar*)key+((curKey.key_part[partsInUse].null_bit)? 1 : 0) ); // + (curKey.key_parts+7) / 8);
    }
   
    db2KeyBufPtr += db2Table->db2Field(field->field_index).getByteLengthInRecord();
    key += curKey.key_part[partsInUse].store_length;
  }
  
  keyLen = db2KeyBufPtr - (char*)keyBuf.ptr();
  
  DBUG_PRINT("ha_ibmdb2i::index_read", ("find_flag: %d", find_flag));

  char readDirection = QMY_NEXT;
    
  switch (find_flag)
  {
    case HA_READ_AFTER_KEY:
      doInitialRead(QMY_AFTER_EQUAL, estimatedRows,
                    keyBuf.ptr(), keyLen, partsInUse);
      break;
    case HA_READ_BEFORE_KEY:
      doInitialRead(QMY_BEFORE_EQUAL, estimatedRows,
                    keyBuf.ptr(), keyLen, partsInUse);
      break;
    case HA_READ_KEY_OR_NEXT:
      doInitialRead(QMY_AFTER_OR_EQUAL, estimatedRows,
                    keyBuf.ptr(), keyLen, partsInUse);
      break;
    case HA_READ_KEY_OR_PREV:
      DBUG_ASSERT(0); // This function is unused      
      doInitialRead(QMY_BEFORE_OR_EQUAL, estimatedRows,
                    keyBuf.ptr(), keyLen, partsInUse);
      break;
    case HA_READ_PREFIX_LAST_OR_PREV:
      doInitialRead(QMY_LAST_PREVIOUS, estimatedRows,
                    keyBuf.ptr(), keyLen, partsInUse);
      readDirection = QMY_PREVIOUS;
      break; 
    case HA_READ_PREFIX_LAST:
      doInitialRead(QMY_PREFIX_LAST, estimatedRows,
                    keyBuf.ptr(), keyLen, partsInUse);
      readDirection = QMY_PREVIOUS;
      break; 
    case HA_READ_KEY_EXACT:
      doInitialRead(QMY_EQUAL, estimatedRows, keyBuf.ptr(), keyLen, partsInUse);
      break;
    default: 
        DBUG_ASSERT(0);
        return HA_ERR_GENERIC;
      break;
  }
  
  ha_statistic_increment(&SSV::ha_read_key_count);
  rc = readFromBuffer(buf, readDirection);
  
  table->status= (rc ? STATUS_NOT_FOUND: 0);
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::index_next(uchar * buf)
{
  DBUG_ENTER("ha_ibmdb2i::index_next");
  ha_statistic_increment(&SSV::ha_read_next_count);
  
  int rc = readFromBuffer(buf, QMY_NEXT);
  
  table->status= (rc ? STATUS_NOT_FOUND: 0);
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::index_next_same(uchar *buf, const uchar *key, uint keylen)
{
  DBUG_ENTER("ha_ibmdb2i::index_next_same");
  ha_statistic_increment(&SSV::ha_read_next_count);
  
  int rc = readFromBuffer(buf, QMY_NEXT_EQUAL);
  
  if (rc == HA_ERR_KEY_NOT_FOUND)
  {
    rc = HA_ERR_END_OF_FILE;
  }

  table->status= (rc ? STATUS_NOT_FOUND: 0);
  DBUG_RETURN(rc);
}

int ha_ibmdb2i::index_read_last(uchar * buf, const uchar * key, uint key_len)
{
  DBUG_ENTER("ha_ibmdb2i::index_read_last");
  DBUG_RETURN(index_read(buf, key, key_len, HA_READ_PREFIX_LAST));  
}



int ha_ibmdb2i::index_prev(uchar * buf)
{
  DBUG_ENTER("ha_ibmdb2i::index_prev");
  ha_statistic_increment(&SSV::ha_read_prev_count);
  
  int rc = readFromBuffer(buf, QMY_PREVIOUS);

  table->status= (rc ? STATUS_NOT_FOUND: 0);  
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::index_first(uchar * buf)
{
  DBUG_ENTER("ha_ibmdb2i::index_first");

  if (unlikely(last_index_init_rc)) DBUG_RETURN(last_index_init_rc);
    
  int rc = prepReadBuffer(DEFAULT_MAX_ROWS_TO_BUFFER, 
                          db2Table->indexFile(active_index), 
                          readAccessIntent);
  
  if (rc == 0)
  {
    doInitialRead(QMY_FIRST, DEFAULT_MAX_ROWS_TO_BUFFER);
    ha_statistic_increment(&SSV::ha_read_first_count);
    rc = readFromBuffer(buf, QMY_NEXT);
  }
  
  table->status= (rc ? STATUS_NOT_FOUND: 0);
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::index_last(uchar * buf)
{
  DBUG_ENTER("ha_ibmdb2i::index_last");
  
  if (unlikely(last_index_init_rc)) DBUG_RETURN(last_index_init_rc);
  
  int rc = prepReadBuffer(DEFAULT_MAX_ROWS_TO_BUFFER, 
                          db2Table->indexFile(active_index), 
                          readAccessIntent);
  
  if (rc == 0)
  {
    doInitialRead(QMY_LAST, DEFAULT_MAX_ROWS_TO_BUFFER);
    ha_statistic_increment(&SSV::ha_read_last_count);
    rc = readFromBuffer(buf, QMY_PREVIOUS);
  }
  
  table->status= (rc ? STATUS_NOT_FOUND: 0);
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::rnd_init(bool scan)
{
  DBUG_ENTER("ha_ibmdb2i::rnd_init");

  int& rc = last_rnd_init_rc;
  rc = 0;
      
  tweakReadSet();
  invalidDataFound=false;
  
  uint32 rowsToBlockOnRead;

  if (!scan)
  { 
    rowsToBlockOnRead = 1;
  }
  else
  {
    rowsToBlockOnRead = DEFAULT_MAX_ROWS_TO_BUFFER;
  }
  
  rc = useDataFile(); 
  
  if (!rc)
  {
//     THD* thd = ha_thd();
//     if (accessIntent == QMY_UPDATABLE &&
//         thd_tx_isolation(thd) == ISO_REPEATABLE_READ &&
//         !THDVAR(thd, transaction_unsafe))
//     {
//       readAccessIntent = QMY_READ_ONLY;
//     }
//     else
//     {
      readAccessIntent = accessIntent;
//     }

    rc = prepReadBuffer(rowsToBlockOnRead, db2Table->dataFile(), readAccessIntent);

    if (!rc && accessIntent != QMY_READ_ONLY)
      rc = prepWriteBuffer(1, db2Table->dataFile());

    if (!rc && scan)
      doInitialRead(QMY_FIRST, rowsToBlockOnRead);
    
    if (rc)
      releaseDataFile();
  }
  
  rrnAssocHandle= 0;

  DBUG_RETURN(0); // MySQL sometimes does not check the return code, causing 
                  // an assert in ha_rnd_end later on if we return a non-zero
                  // value here. 
}

int ha_ibmdb2i::rnd_end()
{
  DBUG_ENTER("ha_ibmdb2i::rnd_end");
  
  warnIfInvalidData();
  if (likely(activeReadBuf))
    activeReadBuf->endRead();
  if (last_rnd_init_rc == 0)
    releaseActiveHandle();
  last_rnd_init_rc = 0;
  DBUG_RETURN(0);
}


int32 ha_ibmdb2i::mungeDB2row(uchar* record, const char* dataPtr, const char* nullMapPtr, bool skipLOBs)
{
  DBUG_ASSERT(dataPtr);
  
  my_bitmap_map *old_write_map= dbug_tmp_use_all_columns(table, table->write_set);
  my_bitmap_map *old_read_map;
  
  if (unlikely(readAllColumns))
    old_read_map = tmp_use_all_columns(table, table->read_set);

  resetCharacterConversionBuffers();
  
  my_ptrdiff_t old_ptr= (my_ptrdiff_t) (record - table->record[0]); 
  int fieldIndex = 0;
  for (Field **field = table->field; *field; ++field, ++fieldIndex)
  {  
    if (unlikely(old_ptr))   
      (*field)->move_field_offset(old_ptr);
    if (nullMapPtr[fieldIndex] == __NULL_VALUE_EBCDIC ||
        (!bitmap_is_set(table->read_set, fieldIndex)) ||
        (skipLOBs && db2Table->db2Field(fieldIndex).isBlob()))
    {
      (*field)->set_null();
    }
    else
    {
      (*field)->set_notnull();
      convertDB2toMySQL(db2Table->db2Field(fieldIndex), *field, dataPtr);
    }
    if (unlikely(old_ptr))
      (*field)->move_field_offset(-old_ptr);
    
  }
    
  if (unlikely(readAllColumns))
    tmp_restore_column_map(table->read_set, old_read_map);
  dbug_tmp_restore_column_map(table->write_set, old_write_map);
  
  return 0;
}


int ha_ibmdb2i::rnd_next(uchar *buf)
{
  DBUG_ENTER("ha_ibmdb2i::rnd_next");

  if (unlikely(last_rnd_init_rc)) DBUG_RETURN(last_rnd_init_rc);
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  
  int rc;      
  
  rc = readFromBuffer(buf, QMY_NEXT);
    
  table->status= (rc ? STATUS_NOT_FOUND: 0);
  DBUG_RETURN(rc);
}


void ha_ibmdb2i::position(const uchar *record)
{
  DBUG_ENTER("ha_ibmdb2i::position");
  my_store_ptr(ref, ref_length, currentRRN);
  DBUG_VOID_RETURN;
}


int ha_ibmdb2i::rnd_pos(uchar * buf, uchar *pos)
{
  DBUG_ENTER("ha_ibmdb2i::rnd_pos");
  if (unlikely(last_rnd_init_rc)) DBUG_RETURN( last_rnd_init_rc);
  ha_statistic_increment(&SSV::ha_read_rnd_count);

  currentRRN = my_get_ptr(pos, ref_length);

  tweakReadSet();  
  
  int rc = 0;

  if (rrnAssocHandle &&
      (activeHandle != rrnAssocHandle))
  {
    if (activeHandle) releaseActiveHandle();
    rc = useFileByHandle(QMY_UPDATABLE, rrnAssocHandle);    
  }
  
  if (likely(rc == 0))
  {
    rc = prepReadBuffer(1, getFileForActiveHandle(), accessIntent);

    if (likely(rc == 0) && accessIntent == QMY_UPDATABLE)
      rc = prepWriteBuffer(1, getFileForActiveHandle());

    if (likely(rc == 0))
    {
      rc = db2i_ileBridge::getBridgeForThread()->readByRRN(activeHandle, 
                                                         activeReadBuf->ptr(),
                                                         currentRRN,
                                                         accessIntent,
                                                         getCommitLevel());

      if (likely(rc == 0))
      {
        rrnAssocHandle = activeHandle;
        const char* readBuf = activeReadBuf->getRowN(0);
        rc = mungeDB2row(buf, readBuf, readBuf + activeReadBuf->getRowNullOffset(), false);
        releaseRowNeeded = TRUE;
      }
    }    
  }
  
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::info(uint flag)
{
  DBUG_ENTER("ha_ibmdb2i::info");

  uint16 infoRequested = 0;
  ValidatedPointer<char> rowKeySpcPtr;  // Space pointer passed to DB2
  uint32 rowKeySpcLen;                  // Length of space passed to DB2
  THD* thd = ha_thd();
  int command = thd_sql_command(thd);
  
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value = (ulonglong) 0;

  if (flag & HA_STATUS_ERRKEY)
  {
    errkey = lastDupKeyID;
    my_store_ptr(dup_ref, ref_length, lastDupKeyRRN);
  }
  
  if (flag & HA_STATUS_TIME)
  {
    if ((flag & HA_STATUS_NO_LOCK) && 
        ibmdb2i_assume_exclusive_use &&
        share &&
        (share->cachedStats.isInited(lastModTime)))
      stats.update_time = share->cachedStats.getUpdateTime();
    else
      infoRequested |= lastModTime;
  }
  
  if (flag & HA_STATUS_CONST)
  {
    stats.block_size=4096;
    infoRequested |= createTime;
    
    if (table->s->keys)
    {
      infoRequested |= rowsPerKey;
      rowKeySpcLen = (table->s->keys) * MAX_DB2_KEY_PARTS * sizeof(uint64);
      rowKeySpcPtr.alloc(rowKeySpcLen);
      memset(rowKeySpcPtr, 0, rowKeySpcLen);               // Clear the allocated space
    }
  }
  
  if (flag & HA_STATUS_VARIABLE)
  {
    if ((flag & HA_STATUS_NO_LOCK) &&
        (command != SQLCOM_SHOW_TABLE_STATUS) &&
        ibmdb2i_assume_exclusive_use &&
        share &&
        (share->cachedStats.isInited(rowCount | deletedRowCount | meanRowLen | ioCount)) &&
        (share->cachedStats.getRowCount() >= 2))
    {
      stats.records = share->cachedStats.getRowCount();
      stats.deleted = share->cachedStats.getDelRowCount();
      stats.mean_rec_length = share->cachedStats.getMeanLength();
      stats.data_file_length = share->cachedStats.getAugmentedDataLength();
    }
    else
    {
      infoRequested |= rowCount | deletedRowCount | meanRowLen;            
      if (command == SQLCOM_SHOW_TABLE_STATUS)
        infoRequested |= objLength;
      else
        infoRequested |= ioCount;
    }
  }

  int rc = 0;
          
  if (infoRequested)
  {
    DBUG_PRINT("ha_ibmdb2i::info",("Retrieving fresh stats %d", flag));

    initBridge(thd);
    rc = bridge()->retrieveTableInfo((dataHandle  ? dataHandle : db2Table->dataFile()->getMasterDefnHandle()),
                                     infoRequested,
                                     stats,
                                     rowKeySpcPtr);
    
    if (!rc)
    {
      if ((flag & HA_STATUS_VARIABLE) &&
          (command != SQLCOM_SHOW_TABLE_STATUS))
        stats.data_file_length = stats.data_file_length * IO_SIZE;

      if ((ibmdb2i_assume_exclusive_use) &&
          (share) && 
          (command != SQLCOM_SHOW_TABLE_STATUS))
      {
        if (flag & HA_STATUS_VARIABLE) 
        {
          share->cachedStats.cacheRowCount(stats.records);
          share->cachedStats.cacheDelRowCount(stats.deleted);
          share->cachedStats.cacheMeanLength(stats.mean_rec_length);
          share->cachedStats.cacheAugmentedDataLength(stats.data_file_length);
        }

        if (flag & HA_STATUS_TIME)
        {
          share->cachedStats.cacheUpdateTime(stats.update_time);
        }
      }

      if (flag & HA_STATUS_CONST)
      {
        ulong i;                 // Loop counter for indexes
        ulong j;                 // Loop counter for key parts
        RowKey* rowKeyPtr;       // Pointer to 'number of unique rows' array for this index

        rowKeyPtr = (RowKey_t*)(void*)rowKeySpcPtr;    // Address first array of DB2 row counts
        for (i = 0; i < table->s->keys; i++)           // Do for each index, including primary
        {
          for (j = 0; j < table->key_info[i].key_parts; j++)   
          {
            table->key_info[i].rec_per_key[j]= rowKeyPtr->RowKeyArray[j];
          }
          rowKeyPtr = rowKeyPtr + 1;                   // Address next array of DB2 row counts 
        }
      }
    }
    else if (rc == HA_ERR_LOCK_WAIT_TIMEOUT && share)
    {
      // If we couldn't retrieve the info because the object was locked,
      // we'll do our best by returning the most recently cached data.
      if ((infoRequested & rowCount) &&
          share->cachedStats.isInited(rowCount))
        stats.records = share->cachedStats.getRowCount();
      if ((infoRequested & deletedRowCount) &&
          share->cachedStats.isInited(deletedRowCount))
        stats.deleted = share->cachedStats.getDelRowCount();
      if ((infoRequested & meanRowLen) &&
          share->cachedStats.isInited(meanRowLen))
        stats.mean_rec_length = share->cachedStats.getMeanLength();
      if ((infoRequested & lastModTime) &&
          share->cachedStats.isInited(lastModTime))
        stats.update_time = share->cachedStats.getUpdateTime();
      
      rc = 0;
    }
  }

  DBUG_RETURN(rc);
}


ha_rows ha_ibmdb2i::records()
{
  DBUG_ENTER("ha_ibmdb2i::records");
  int rc;
  rc = bridge()->retrieveTableInfo((dataHandle ? dataHandle : db2Table->dataFile()->getMasterDefnHandle()),
                                                             rowCount,
                                                             stats);

  if (unlikely(rc))
  {
    if (rc == HA_ERR_LOCK_WAIT_TIMEOUT && 
        share && 
        (share->cachedStats.isInited(rowCount)))
      DBUG_RETURN(share->cachedStats.getRowCount());
    else
      DBUG_RETURN(HA_POS_ERROR);
  }
  else if (share)
  {
    share->cachedStats.cacheRowCount(stats.records);
  }

 DBUG_RETURN(stats.records); 
}


int ha_ibmdb2i::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_ibmdb2i::extra");
  
  switch(operation)
  {
    // Can these first five flags be replaced by attending to HA_EXTRA_WRITE_CACHE?
    case HA_EXTRA_NO_IGNORE_DUP_KEY: 
    case HA_EXTRA_WRITE_CANNOT_REPLACE: 
      {                                                
        returnDupKeysImmediately = false;
        onDupUpdate = false;                         
      }                                              
      break;
    case HA_EXTRA_INSERT_WITH_UPDATE:                
      {                                              
        returnDupKeysImmediately = true;             
        onDupUpdate = true;                          
      }                                               
      break;                            
    case HA_EXTRA_IGNORE_DUP_KEY:  
    case HA_EXTRA_WRITE_CAN_REPLACE: 
      returnDupKeysImmediately = true;
      break;
    case HA_EXTRA_FLUSH_CACHE:
      if (outstanding_start_bulk_insert)
        finishBulkInsert();
      break;
  }

  
  DBUG_RETURN(0);
}

/** 
  @brief  
  The DB2 storage engine will ignore a MySQL generated value and will generate 
  a new value in SLIC. We arbitrarily set first_value to 1, and set the
  interval to infinity for better performance on multi-row inserts.
*/
void ha_ibmdb2i::get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values)
{
  DBUG_ENTER("ha_ibmdb2i::get_auto_increment");
  *first_value= 1;
  *nb_reserved_values= ULONGLONG_MAX;
} 



void ha_ibmdb2i::update_create_info(HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_ibmdb2i::update_create_info");

  if ((!(create_info->used_fields & HA_CREATE_USED_AUTO)) &&
      (table->found_next_number_field != NULL))
  {
    initBridge();
    
    create_info->auto_increment_value= 1; 

    ha_rows rowCount = records();
    
    if (rowCount == 0)
    { 
      create_info->auto_increment_value = db2Table->getStartId();
      DBUG_VOID_RETURN;
    }
    else if (rowCount == HA_POS_ERROR)
    { 
      DBUG_VOID_RETURN;
    }

    getNextIdVal(&create_info->auto_increment_value); 
  }
  DBUG_VOID_RETURN;
}


int ha_ibmdb2i::getNextIdVal(ulonglong *value)
{
  DBUG_ENTER("ha_ibmdb2i::getNextIdVal");
  
  char queryBuffer[MAX_DB2_COLNAME_LENGTH + MAX_DB2_QUALIFIEDNAME_LENGTH + 64];
  strcpy(queryBuffer, " SELECT CAST(MAX( ");
  convertMySQLNameToDB2Name(table->found_next_number_field->field_name, 
                            strend(queryBuffer), 
                            MAX_DB2_COLNAME_LENGTH+1);
  strcat(queryBuffer, ") AS BIGINT) FROM ");    
  db2Table->getDB2QualifiedName(strend(queryBuffer));
  DBUG_ASSERT(strlen(queryBuffer) < sizeof(queryBuffer));
  
  SqlStatementStream sqlStream(queryBuffer);
  DBUG_PRINT("ha_ibmdb2i::getNextIdVal", ("Sent to DB2: %s",queryBuffer));

  int rc = 0;
  FILE_HANDLE fileHandle2;
  uint32 db2RowDataLen2;
  rc = bridge()->prepOpen(sqlStream.getPtrToData(),
                          &fileHandle2,
                          &db2RowDataLen2);
  if (likely(rc == 0))
  {
    IOReadBuffer rowBuffer(1, db2RowDataLen2);
    rc = bridge()->read(fileHandle2, 
                        rowBuffer.ptr(),
                        QMY_READ_ONLY,
                        QMY_NONE,
                        QMY_FIRST);
    
    if (likely(rc == 0))
    {
      /* This check is here for the case where the table is not empty,
         but the auto_increment starting value has been changed since     
         the last record was written.                                */

      longlong maxIdVal = *(longlong*)(rowBuffer.getRowN(0));
      if ((maxIdVal + 1) > db2Table->getStartId())
        *value = maxIdVal + 1; 
      else
        *value = db2Table->getStartId();
    }
    
    bridge()->deallocateFile(fileHandle2);
  }
  DBUG_RETURN(rc);
}


/*
  Updates index cardinalities.                                             
*/
int ha_ibmdb2i::analyze(THD* thd, HA_CHECK_OPT *check_opt)
{
  DBUG_ENTER("ha_ibmdb2i::analyze");
  info(HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE);
  DBUG_RETURN(0);
}

int ha_ibmdb2i::optimize(THD* thd, HA_CHECK_OPT *check_opt)
{
  DBUG_ENTER("ha_ibmdb2i::optimize");

  initBridge(thd);
  
  if (unlikely(records() == 0))
    DBUG_RETURN(0); // DB2 doesn't like to reorganize a table with no data.
  
  quiesceAllFileHandles();
  
  int32 rc = bridge()->optimizeTable(db2Table->dataFile()->getMasterDefnHandle());
  info(HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE);
  
  DBUG_RETURN(rc);
}


/**
  @brief
  Determines if an ALTER TABLE is allowed to switch the storage engine
  for this table. If the table has a foreign key or is referenced by a
  foreign key, then it cannot be switched. 
*/
bool ha_ibmdb2i::can_switch_engines(void)
/*=================================*/
{
  DBUG_ENTER("ha_ibmdb2i::can_switch_engines");

  int rc = 0;
  FILE_HANDLE queryFile = 0;
  uint32 resultRowLen;  
  uint count = 0; 
  bool can_switch = FALSE;   // 1 if changing storage engine is allowed
  
  const char* libName = db2Table->getDB2LibName(db2i_table::ASCII_SQL);
  const char* fileName = db2Table->getDB2TableName(db2i_table::ASCII_SQL);
  
  String query(256);
  query.append(STRING_WITH_LEN(" SELECT COUNT(*) FROM SYSIBM.SQLFOREIGNKEYS WHERE ((PKTABLE_SCHEM = '"));
  query.append(libName+1, strlen(libName)-2);            // Remove quotes from parent schema name
  query.append(STRING_WITH_LEN("' AND PKTABLE_NAME = '"));                   
  query.append(fileName+1,strlen(fileName)-2);           // Remove quotes from file name
  query.append(STRING_WITH_LEN("') OR (FKTABLE_SCHEM = '"));                                              
  query.append(libName+1,strlen(libName)-2);             // Remove quotes from child schema
  query.append(STRING_WITH_LEN("' AND FKTABLE_NAME = '"));          
  query.append(fileName+1,strlen(fileName)-2);           // Remove quotes from child name 
  query.append(STRING_WITH_LEN("'))"));
                                               
  SqlStatementStream sqlStream(query);
  
  rc = bridge()->prepOpen(sqlStream.getPtrToData(),
                        &queryFile,
                        &resultRowLen);
  if (rc == 0)
  {
    IOReadBuffer rowBuffer(1, resultRowLen);

    rc =   bridge()->read(queryFile, 
                        rowBuffer.ptr(),
                        QMY_READ_ONLY, 
                        QMY_NONE,
                        QMY_FIRST);
    if (!rc)
    {
       count = *(uint*)(rowBuffer.getRowN(0));
       if (count == 0)
         can_switch = TRUE;
    }

    bridge()->deallocateFile(queryFile);
  }
  DBUG_RETURN(can_switch);
}



bool ha_ibmdb2i::check_if_incompatible_data(HA_CREATE_INFO *info,
                                         uint table_changes)
{
  DBUG_ENTER("ha_ibmdb2i::check_if_incompatible_data");
  uint i;
  /* Check that auto_increment value and field definitions were
     not changed. */
  if ((info->used_fields & HA_CREATE_USED_AUTO &&
       info->auto_increment_value != 0) ||
       table_changes != IS_EQUAL_YES)
    DBUG_RETURN(COMPATIBLE_DATA_NO);
  /* Check if any fields were renamed. */
  for (i= 0; i < table->s->fields; i++)
  {
   Field *field= table->field[i];
   if (field->flags & FIELD_IS_RENAMED)
    {
      DBUG_PRINT("info", ("Field has been renamed, copy table"));
      DBUG_RETURN(COMPATIBLE_DATA_NO);
    }
  }
  DBUG_RETURN(COMPATIBLE_DATA_YES);
}

int ha_ibmdb2i::reset_auto_increment(ulonglong value)
 {
  DBUG_ENTER("ha_ibmdb2i::reset_auto_increment");
  
  int rc = 0;

  quiesceAllFileHandles();

  const char* libName = db2Table->getDB2LibName(db2i_table::ASCII_SQL);
  const char* fileName = db2Table->getDB2TableName(db2i_table::ASCII_SQL);

  String query(512);
  query.append(STRING_WITH_LEN(" ALTER TABLE "));
  query.append(libName);
  query.append('.');
  query.append(fileName);
  query.append(STRING_WITH_LEN(" ALTER COLUMN "));
  char colName[MAX_DB2_COLNAME_LENGTH+1];
  convertMySQLNameToDB2Name(table->found_next_number_field->field_name, 
                            colName, 
                            sizeof(colName));
  query.append(colName);
  
  char restart_value[22];  
  CHARSET_INFO *cs= &my_charset_bin;
  uint len = (uint)(cs->cset->longlong10_to_str)(cs,restart_value,sizeof(restart_value), 10, value);  
  restart_value[len] = 0;
  
  query.append(STRING_WITH_LEN(" RESTART WITH "));
  query.append(restart_value);
  
  SqlStatementStream sqlStream(query);
  DBUG_PRINT("ha_ibmdb2i::reset_auto_increment", ("Sent to DB2: %s",query.c_ptr()));

  rc = db2i_ileBridge::getBridgeForThread()->execSQL(sqlStream.getPtrToData(),
                                                     sqlStream.getStatementCount(),
                                                     QMY_NONE, //getCommitLevel(),
                                                     FALSE,
                                                     FALSE,
                                                     TRUE, //FALSE,
                                                     dataHandle);
  if (rc == 0)
    db2Table->updateStartId(value); 

  DBUG_RETURN(rc);
}


/**
  @brief
  This function receives an error code that was previously set by the handler.
  It returns to MySQL the error string associated with that error.   
*/
bool ha_ibmdb2i::get_error_message(int error, String *buf)
{
  DBUG_ENTER("ha_ibmdb2i::get_error_message");
  if ((error >= DB2I_FIRST_ERR && error <= DB2I_LAST_ERR) ||
      (error >= QMY_ERR_MIN && error <= QMY_ERR_MAX))
  {
    db2i_ileBridge* bridge = db2i_ileBridge::getBridgeForThread(ha_thd());
    char* errMsg = bridge->getErrorStorage();
    buf->copy(errMsg, strlen(errMsg),system_charset_info);
    bridge->freeErrorStorage();
  }
  DBUG_RETURN(FALSE);                          
}


int ha_ibmdb2i::delete_all_rows()
{
  DBUG_ENTER("ha_ibmdb2i::delete_all_rows");
  int rc = 0;
  char queryBuffer[MAX_DB2_QUALIFIEDNAME_LENGTH + 64];
  strcpy(queryBuffer, " DELETE FROM ");
  db2Table->getDB2QualifiedName(strend(queryBuffer));
  DBUG_ASSERT(strlen(queryBuffer) < sizeof(queryBuffer));
  
  SqlStatementStream sqlStream(queryBuffer);
  DBUG_PRINT("ha_ibmdb2i::delete_all_rows", ("Sent to DB2: %s",queryBuffer));
  rc = bridge()->execSQL(sqlStream.getPtrToData(),
                         sqlStream.getStatementCount(),
                         getCommitLevel(),
                         false,
                         false,
                         true,
                         dataHandle);
  
 /* If this method was called on behalf of a TRUNCATE TABLE statement, and if */
 /* the table has an auto_increment field, then reset the starting value for  */
 /* the auto_increment field to 1.
                                            */
  if (rc == 0 && thd_sql_command(ha_thd()) == SQLCOM_TRUNCATE &&
      table->found_next_number_field )
    rc = reset_auto_increment(1);

  invalidateCachedStats();
  
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::external_lock(THD *thd, int lock_type)
{
  int rc = 0;

  DBUG_ENTER("ha_ibmdb2i::external_lock");
  DBUG_PRINT("ha_ibmdb2i::external_lock",("Lock type: %d", lock_type));
  
  if (lock_type == F_RDLCK)
    accessIntent = QMY_READ_ONLY;
  else if (lock_type == F_WRLCK)
    accessIntent = QMY_UPDATABLE;
  
  initBridge(thd);
  int command = thd_sql_command(thd);
  
  if (!THDVAR(thd,transaction_unsafe))
  {
    if (lock_type != F_UNLCK)
    {
      if (autoCommitIsOn(thd) == QMY_YES)
      {
        trans_register_ha(thd, FALSE, ibmdb2i_hton);
      }
      else 
      { 
        trans_register_ha(thd, TRUE, ibmdb2i_hton);
        if (likely(command != SQLCOM_CREATE_TABLE))
        {
          trans_register_ha(thd, FALSE, ibmdb2i_hton);
          bridge()->beginStmtTx();
        }
      }
    }    
  }

  if (command == SQLCOM_LOCK_TABLES ||
      command == SQLCOM_ALTER_TABLE ||
      command == SQLCOM_UNLOCK_TABLES ||
      (accessIntent == QMY_UPDATABLE &&
       (command == SQLCOM_UPDATE ||
        command == SQLCOM_UPDATE_MULTI ||
        command == SQLCOM_DELETE ||
        command == SQLCOM_DELETE_MULTI ||
        command == SQLCOM_REPLACE ||
        command == SQLCOM_REPLACE_SELECT) &&
       getCommitLevel(thd) == QMY_NONE))
  {
    char action;
    char type;
    if (lock_type == F_UNLCK)
    { 
      action = QMY_UNLOCK;
      type = accessIntent == QMY_READ_ONLY ? QMY_LSRD : QMY_LENR;
    }
    else
    {
      action = QMY_LOCK;
      type = lock_type == F_RDLCK ? QMY_LSRD : QMY_LENR;
    }

    DBUG_PRINT("ha_ibmdb2i::external_lock",("%socking table", action==QMY_LOCK ? "L" : "Unl"));

    if (!dataHandle)
      rc = db2Table->dataFile()->allocateNewInstance(&dataHandle, curConnection);  

    rc = bridge()->lockObj(dataHandle, 
                           0,
                           action,               
                           type,
                           (command == SQLCOM_LOCK_TABLES ? QMY_NO : QMY_YES)); 
    
  } 
  
  // Cache this away so we don't have to access it on each row operation
  cachedZeroDateOption = (enum_ZeroDate)THDVAR(thd, compat_opt_allow_zero_date_vals);
  
  DBUG_RETURN(rc);
}


THR_LOCK_DATA **ha_ibmdb2i::store_lock(THD *thd,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !(thd->in_lock_tables && thd_sql_command(thd) == SQLCOM_LOCK_TABLES))
      lock_type= TL_WRITE_ALLOW_WRITE;
    lock.type=lock_type;
  }
  *to++= &lock;
  return to;
}


int ha_ibmdb2i::delete_table(const char *name)
{
  DBUG_ENTER("ha_ibmdb2i::delete_table");
  THD* thd = ha_thd();
  db2i_ileBridge* bridge = db2i_ileBridge::getBridgeForThread(thd);  
  
  char db2Name[MAX_DB2_QUALIFIEDNAME_LENGTH];
  db2i_table::getDB2QualifiedNameFromPath(name, db2Name);

  String query(128);
  query.append(STRING_WITH_LEN(" DROP TABLE "));
  query.append(db2Name);

  if (thd_sql_command(thd) == SQLCOM_DROP_TABLE &&
      thd->lex->drop_mode == DROP_RESTRICT)
    query.append(STRING_WITH_LEN(" RESTRICT "));  
  DBUG_PRINT("ha_ibmdb2i::delete_table", ("Sent to DB2: %s",query.c_ptr()));

  SqlStatementStream sqlStream(query);

  db2i_table::getDB2LibNameFromPath(name, db2Name);  
  bool isTemporary = (strcmp(db2Name, DB2I_TEMP_TABLE_SCHEMA) == 0 ? TRUE : FALSE);

  int rc = bridge->execSQL(sqlStream.getPtrToData(),
                       sqlStream.getStatementCount(),
                       (isTemporary ? QMY_NONE : getCommitLevel(thd)),
                       FALSE,
                       FALSE,
                       isTemporary);

  if (rc == HA_ERR_NO_SUCH_TABLE)
  {
     warning(thd, DB2I_ERR_TABLE_NOT_FOUND, name);
     rc = 0;
  }
  
  if (rc == 0)
  {
    db2i_table::deleteAssocFiles(name);
  }
  
  FILE_HANDLE savedHandle = bridge->findAndRemovePreservedHandle(name, &share);
  while (savedHandle)
  {
    bridge->deallocateFile(savedHandle, TRUE);
    DBUG_ASSERT(share);
    if (free_share(share))
      share = NULL;   
    savedHandle = bridge->findAndRemovePreservedHandle(name, &share);
  }
    
  my_errno = rc;
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::rename_table(const char * from, const char * to)
{
  DBUG_ENTER("ha_ibmdb2i::rename_table ");
    
  char db2FromFileName[MAX_DB2_FILENAME_LENGTH + 1];
  char db2ToFileName[MAX_DB2_FILENAME_LENGTH+1];
  char db2FromLibName[MAX_DB2_SCHEMANAME_LENGTH+1];
  char db2ToLibName[MAX_DB2_SCHEMANAME_LENGTH+1];

  db2i_table::getDB2LibNameFromPath(from, db2FromLibName);
  db2i_table::getDB2LibNameFromPath(to, db2ToLibName);

  if (strcmp(db2FromLibName, db2ToLibName) != 0 )
  {
    getErrTxt(DB2I_ERR_RENAME_MOVE,from,to);
    DBUG_RETURN(DB2I_ERR_RENAME_MOVE);
  }
 
  db2i_table::getDB2FileNameFromPath(from, db2FromFileName, db2i_table::ASCII_NATIVE);
  db2i_table::getDB2FileNameFromPath(to, db2ToFileName);

  char escapedFromFileName[2 * MAX_DB2_FILENAME_LENGTH + 1];
    
  uint o = 0;
  uint i = 1;
  do
  {
    escapedFromFileName[o++] = db2FromFileName[i];
    if (db2FromFileName[i] == '+')
      escapedFromFileName[o++] = '+';
  } while (db2FromFileName[++i]);
  escapedFromFileName[o-1] = 0;

  
  int rc = 0;
  
  char queryBuffer[sizeof(db2FromLibName) + 2 * sizeof(db2FromFileName) + 256];
  SafeString selectQuery(queryBuffer, sizeof(queryBuffer));
  selectQuery.strncat(STRING_WITH_LEN("SELECT CAST(INDEX_NAME AS VARCHAR(128) CCSID 1208) FROM QSYS2.SYSINDEXES WHERE INDEX_NAME LIKE '%+_+_+_%"));
  selectQuery.strcat(escapedFromFileName);
  selectQuery.strncat(STRING_WITH_LEN("' ESCAPE '+' AND TABLE_NAME='"));
  selectQuery.strncat(db2FromFileName+1, strlen(db2FromFileName)-2);
  selectQuery.strncat(STRING_WITH_LEN("' AND TABLE_SCHEMA='"));
  selectQuery.strncat(db2FromLibName+1, strlen(db2FromLibName)-2);
  selectQuery.strcat('\'');
  DBUG_ASSERT(!selectQuery.overflowed());
  
  SqlStatementStream indexQuery(selectQuery.ptr());

  FILE_HANDLE queryFile = 0;
  uint32 resultRowLen;
    
  initBridge();
  rc = bridge()->prepOpen(indexQuery.getPtrToData(),
                        &queryFile,
                        &resultRowLen);

  if (unlikely(rc))
    DBUG_RETURN(rc);
    
  IOReadBuffer rowBuffer(1, resultRowLen);  
     
  int tableNameLen = strlen(db2FromFileName) - 2;
  
  SqlStatementStream renameQuery(64);
  String query;
  while (rc == 0)
  {
    query.length(0);

    rc = bridge()->read(queryFile,
                      rowBuffer.ptr(),
                      QMY_READ_ONLY,
                      QMY_NONE,
                      QMY_NEXT);

    if (!rc)
    {
      const char* rowData = rowBuffer.getRowN(0);
      char indexFileName[MAX_DB2_FILENAME_LENGTH];
      memset(indexFileName, 0, sizeof(indexFileName));
      
      uint16 fileNameLen = *(uint16*)(rowData);
      strncpy(indexFileName, rowData + sizeof(uint16), fileNameLen);
            
      int bytesToRetain = fileNameLen - tableNameLen;
      if (bytesToRetain <= 0)
      /* We can't handle index names in which the MySQL index name and
         the table name together are longer than the max index name.      */
      {
        getErrTxt(DB2I_ERR_INVALID_NAME,"index","*generated*");
        DBUG_RETURN(DB2I_ERR_INVALID_NAME);     
      }
      char indexName[MAX_DB2_FILENAME_LENGTH];
      memset(indexName, 0, sizeof(indexName));

      strncpy(indexName, 
              indexFileName, 
              bytesToRetain);
      
      char db2IndexName[MAX_DB2_FILENAME_LENGTH+1];
      
      convertMySQLNameToDB2Name(indexFileName, db2IndexName, sizeof(db2IndexName));

      query.append(STRING_WITH_LEN("RENAME INDEX "));
      query.append(db2FromLibName);
      query.append('.');
      query.append(db2IndexName);
      query.append(STRING_WITH_LEN(" TO "));
      if (db2i_table::appendQualifiedIndexFileName(indexName, db2ToFileName, query, db2i_table::ASCII_SQL, typeNone) == -1)
      {
        getErrTxt(DB2I_ERR_INVALID_NAME,"index","*generated*");
        DBUG_RETURN(DB2I_ERR_INVALID_NAME );
      }
      renameQuery.addStatement(query);      
      DBUG_PRINT("ha_ibmdb2i::rename_table", ("Sent to DB2: %s",query.c_ptr_safe()));
    }    
  }

    
  if (queryFile)
    bridge()->deallocateFile(queryFile);
  
  if (rc != HA_ERR_END_OF_FILE)
    DBUG_RETURN(rc);
  
  char db2Name[MAX_DB2_QUALIFIEDNAME_LENGTH];
          
  /* Rename the table */
  query.length(0);
  query.append(STRING_WITH_LEN(" RENAME TABLE "));
  db2i_table::getDB2QualifiedNameFromPath(from, db2Name);
  query.append(db2Name);  
  query.append(STRING_WITH_LEN(" TO "));
  query.append(db2ToFileName);
  DBUG_PRINT("ha_ibmdb2i::rename_table", ("Sent to DB2: %s",query.c_ptr_safe()));
  renameQuery.addStatement(query);
  rc = bridge()->execSQL(renameQuery.getPtrToData(),
                       renameQuery.getStatementCount(),
                       getCommitLevel());
  
  if (!rc)
    db2i_table::renameAssocFiles(from, to);
  
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::create(const char *name, TABLE *table_arg,
                       HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_ibmdb2i::create");

  int rc;
  char fileSortSequence[11] = "*HEX";
  char fileSortSequenceLibrary[11] = "";
  char fileSortSequenceType = ' ';
  char libName[MAX_DB2_SCHEMANAME_LENGTH+1];
  char fileName[MAX_DB2_FILENAME_LENGTH+1];
  char colName[MAX_DB2_COLNAME_LENGTH+1];
  bool isTemporary;
  ulong auto_inc_value;

  db2i_table::getDB2LibNameFromPath(name, libName);
  db2i_table::getDB2FileNameFromPath(name, fileName);

  if (osVersion.v < 6)
  {
    if (strlen(libName) > 
         MAX_DB2_V5R4_LIBNAME_LENGTH + (isOrdinaryIdentifier(libName) ? 2 : 0))
    {
      getErrTxt(DB2I_ERR_TOO_LONG_SCHEMA,libName, MAX_DB2_V5R4_LIBNAME_LENGTH);
      DBUG_RETURN(DB2I_ERR_TOO_LONG_SCHEMA);
    }
  }
  else if (strlen(libName) > MAX_DB2_V6R1_LIBNAME_LENGTH)
  {
    getErrTxt(DB2I_ERR_TOO_LONG_SCHEMA,libName, MAX_DB2_V6R1_LIBNAME_LENGTH);
    DBUG_RETURN(DB2I_ERR_TOO_LONG_SCHEMA);
  }
  
  String query(256);
  
  if (strcmp(libName, DB2I_TEMP_TABLE_SCHEMA))
  {
    query.append(STRING_WITH_LEN("CREATE TABLE "));
    query.append(libName);
    query.append('.');
    query.append(fileName);
    isTemporary = FALSE;
  }
  else
  {
    query.append(STRING_WITH_LEN("DECLARE GLOBAL TEMPORARY TABLE "));
    query.append(fileName);
    isTemporary = TRUE;
  }
  query.append(STRING_WITH_LEN(" ("));
  
  THD* thd = ha_thd();
  enum_TimeFormat timeFormat = (enum_TimeFormat)(THDVAR(thd, compat_opt_time_as_duration));
  enum_YearFormat yearFormat = (enum_YearFormat)(THDVAR(thd, compat_opt_year_as_int));
  enum_BlobMapping blobMapping = (enum_BlobMapping)(THDVAR(thd, compat_opt_blob_cols));
  enum_ZeroDate zeroDate = (enum_ZeroDate)(THDVAR(thd, compat_opt_allow_zero_date_vals));
  bool propagateDefaults = THDVAR(thd, propagate_default_col_vals);
  
  Field **field;
  for (field= table_arg->field; *field; field++)
  {  
    if ( field != table_arg->field ) // Not the first one
      query.append(STRING_WITH_LEN(" , "));

    if (!convertMySQLNameToDB2Name((*field)->field_name, colName, sizeof(colName)))
    {
      getErrTxt(DB2I_ERR_INVALID_NAME,"field",(*field)->field_name);
      DBUG_RETURN(DB2I_ERR_INVALID_NAME );
    }

    query.append(colName);    
    query.append(' ');

    if (rc = getFieldTypeMapping(*field, 
                                 query, 
                                 timeFormat, 
                                 blobMapping,
                                 zeroDate,
                                 propagateDefaults,
                                 yearFormat))
      DBUG_RETURN(rc);

    if ( (*field)->flags & NOT_NULL_FLAG )
    {
      query.append(STRING_WITH_LEN(" NOT NULL "));
    }
    if ( (*field)->flags & AUTO_INCREMENT_FLAG )     
    {
#ifdef WITH_PARTITION_STORAGE_ENGINE      
      if (table_arg->part_info)
      {
        getErrTxt(DB2I_ERR_PART_AUTOINC);
        DBUG_RETURN(DB2I_ERR_PART_AUTOINC);
      }
#endif
      query.append(STRING_WITH_LEN(" GENERATED BY DEFAULT AS IDENTITY ") );
      if (create_info->auto_increment_value != 0) 
      {
        /* Query was ALTER TABLE...AUTO_INCREMENT = x; or
        CREATE TABLE ...AUTO_INCREMENT = x;  Set the starting
                    value for the auto_increment column.  */          
        char stringValue[22];  
        CHARSET_INFO *cs= &my_charset_bin;
        uint len = (uint)(cs->cset->longlong10_to_str)(cs,stringValue,sizeof(stringValue), 10, create_info->auto_increment_value);  
        stringValue[len] = 0;
        query.append(STRING_WITH_LEN(" (START WITH "));
        query.append(stringValue);

        uint64 maxValue=maxValueForField(*field);
        
        if (maxValue)
        {
          len = (uint)(cs->cset->longlong10_to_str)(cs,stringValue,sizeof(stringValue), 10, maxValue);  
          stringValue[len] = 0;
          query.append(STRING_WITH_LEN(" MAXVALUE "));
          query.append(stringValue);
        }
        
        query.append(STRING_WITH_LEN(") "));
      }

    } 
  }
  
  String fieldDefinition(128);
  
  if (table_arg->s->primary_key != MAX_KEY && !isTemporary)
  {
    query.append(STRING_WITH_LEN(", PRIMARY KEY "));
    rc = buildIndexFieldList(fieldDefinition, 
                             table_arg->key_info[table_arg->s->primary_key],
                             true,
                             &fileSortSequenceType,
                             fileSortSequence,
                             fileSortSequenceLibrary);
    if (rc) DBUG_RETURN(rc);
    query.append(fieldDefinition);
  }

  rc = buildDB2ConstraintString(thd->lex, 
                                query, 
                                name,
                                table_arg->field,
                                &fileSortSequenceType,
                                fileSortSequence,
                                fileSortSequenceLibrary);  
  if (rc) DBUG_RETURN (rc);
  
  query.append(STRING_WITH_LEN(" ) "));
  
  if (isTemporary)
    query.append(STRING_WITH_LEN(" ON COMMIT PRESERVE ROWS "));
  
  if (create_info->alias)
    generateAndAppendRCDFMT(create_info->alias, query);
  else if (((TABLE_LIST*)(thd->lex->select_lex.table_list.first))->table_name)  
    generateAndAppendRCDFMT((char*)((TABLE_LIST*)(thd->lex->select_lex.table_list.first))->table_name, query);
        
  DBUG_PRINT("ha_ibmdb2i::create", ("Sent to DB2: %s",query.c_ptr()));
  SqlStatementStream sqlStream(query.length());
  sqlStream.addStatement(query,fileSortSequence,fileSortSequenceLibrary);
  
  if (table_arg->s->primary_key != MAX_KEY && 
      !isTemporary &&
      (THDVAR(thd, create_index_option)==1) &&
      (fileSortSequenceType != 'B') &&
      (fileSortSequenceType != ' '))
  {
    rc = generateShadowIndex(sqlStream, 
                             table_arg->key_info[table_arg->s->primary_key], 
                             libName, 
                             fileName, 
                             fieldDefinition);
    if (rc) DBUG_RETURN(rc);
  }
  for (uint i = 0; i < table_arg->s->keys; ++i)
  {
    if (i != table_arg->s->primary_key || isTemporary)
    {
      rc = buildCreateIndexStatement(sqlStream, 
                                table_arg->key_info[i], 
                                false,
                                libName,
                                fileName);
      if (rc) DBUG_RETURN (rc);
    }
  }
  
  bool noCommit = isTemporary || ((!autoCommitIsOn(thd)) && (thd_sql_command(thd) == SQLCOM_ALTER_TABLE));
  
  initBridge();
  
//   if (THDVAR(thd, discovery_mode) == 1)
//     bridge()->expectErrors(QMY_ERR_TABLE_EXISTS);
  
  rc = bridge()->execSQL(sqlStream.getPtrToData(),
                         sqlStream.getStatementCount(),
                         (isTemporary ? QMY_NONE : getCommitLevel(thd)),
                         TRUE,
                         FALSE,
                         noCommit );
  
  if (unlikely(rc == QMY_ERR_MSGID) &&
      memcmp(bridge()->getErrorMsgID(), DB2I_SQL0350, 7) == 0)
  {
    my_error(ER_BLOB_USED_AS_KEY, MYF(0), "*unknown*");
    rc = ER_BLOB_USED_AS_KEY;
  }
/*   else if (unlikely(rc == QMY_ERR_TABLE_EXISTS) &&
            THDVAR(thd, discovery_mode) == 1)
  {
    db2i_table* temp = new db2i_table(table_arg->s, name);
    int32 rc = temp->fastInitForCreate(name);
    delete temp;
    
    if (!rc)
      warning(thd, DB2I_ERR_WARN_CREATE_DISCOVER);
    
    DBUG_RETURN(rc);
  }   
*/
  
  if (!rc && !isTemporary)
  {
    db2i_table* temp = new db2i_table(table_arg->s, name);
    rc = temp->fastInitForCreate(name);
    delete temp;
    if (rc) 
      delete_table(name);
  }
  
  DBUG_RETURN(rc);
}


/**
  @brief
  Add an index on-line to a table. This method is called on behalf of
  a CREATE INDEX or ALTER TABLE statement. 
  It is implemented via a composed DDL statement passed to DB2.
*/
int ha_ibmdb2i::add_index(TABLE *table_arg, 
                          KEY *key_info,
                          uint num_of_keys)
{
  DBUG_ENTER("ha_ibmdb2i::add_index");

  int rc;
  SqlStatementStream sqlStream(256);
  const char* libName = db2Table->getDB2LibName(db2i_table::ASCII_SQL);
  const char* fileName = db2Table->getDB2TableName(db2i_table::ASCII_SQL);
  
  quiesceAllFileHandles();
  
  uint primaryKey = MAX_KEY;
  if (table_arg->s->primary_key >= MAX_KEY && !db2Table->isTemporary())
  {  
    for (int i = 0; i < num_of_keys; ++i)
    {
      if (strcmp(key_info[i].name, "PRIMARY") == 0)
      {
        primaryKey = i;
        break;
      }
      else if (primaryKey == MAX_KEY &&
               key_info[i].flags & HA_NOSAME)
      {
        primaryKey = i;
        for (int j=0 ; j < key_info[i].key_parts ;j++)
        {
          uint fieldnr= key_info[i].key_part[j].fieldnr;
          if (table_arg->s->field[fieldnr]->null_ptr ||
              table_arg->s->field[fieldnr]->key_length() !=
              key_info[i].key_part[j].length)
          {
            primaryKey = MAX_KEY;
            break;
          }
        }
      }
    }
  }
        
        
  for (int i = 0; i < num_of_keys; ++i)
  {
    KEY& curKey= key_info[i];
    rc = buildCreateIndexStatement(sqlStream, 
                              curKey, 
                              (i == primaryKey),
                              libName,
                              fileName);
    if (rc) DBUG_RETURN (rc);
  }
  
  rc = bridge()->execSQL(sqlStream.getPtrToData(),
                         sqlStream.getStatementCount(),
                         getCommitLevel(),
                         FALSE,
                         FALSE,
                         FALSE,
                         dataHandle);

  /* Handle the case where a unique index is being created but an error occurs
     because the file contains duplicate key values.                           */ 
  if (rc == ER_DUP_ENTRY)
    print_keydup_error(MAX_KEY,ER(ER_DUP_ENTRY_WITH_KEY_NAME));

  DBUG_RETURN(rc);
}

/**
  @brief
  Drop an index on-line from a table. This method is called on behalf of
  a DROP INDEX or ALTER TABLE statement. 
  It is implemented via a composed DDL statement passed to DB2.
*/
int ha_ibmdb2i::prepare_drop_index(TABLE *table_arg, 
                                   uint *key_num, uint num_of_keys)
{
  DBUG_ENTER("ha_ibmdb2i::prepare_drop_index");
  int rc;
  int i = 0;
  String query(64);
  SqlStatementStream sqlStream(64 * num_of_keys);
  SqlStatementStream shadowStream(64 * num_of_keys);

  quiesceAllFileHandles();
  
  const char* libName = db2Table->getDB2LibName(db2i_table::ASCII_SQL);
  const char* fileName = db2Table->getDB2TableName(db2i_table::ASCII_SQL);

  while (i < num_of_keys)
  {
    query.length(0);
    DBUG_PRINT("info", ("ha_ibmdb2i::prepare_drop_index %u", key_num[i]));
    KEY& curKey= table_arg->key_info[key_num[i]];
    if (key_num[i] == table->s->primary_key && !db2Table->isTemporary())
    {
      query.append(STRING_WITH_LEN("ALTER TABLE "));
      query.append(libName);
      query.append(STRING_WITH_LEN("."));
      query.append(fileName);
      query.append(STRING_WITH_LEN(" DROP PRIMARY KEY"));
    }
    else
    {
      query.append(STRING_WITH_LEN("DROP INDEX "));
      query.append(libName);
      query.append(STRING_WITH_LEN("."));
      db2i_table::appendQualifiedIndexFileName(curKey.name, fileName, query);
    }
    DBUG_PRINT("ha_ibmdb2i::prepare_drop_index", ("Sent to DB2: %s",query.c_ptr_safe()));
    sqlStream.addStatement(query);
    
    query.length(0);
    query.append(STRING_WITH_LEN("DROP INDEX "));
    query.append(libName);
    query.append(STRING_WITH_LEN("."));
    db2i_table::appendQualifiedIndexFileName(curKey.name, fileName, query, db2i_table::ASCII_SQL, typeHex);
    
    DBUG_PRINT("ha_ibmdb2i::prepare_drop_index", ("Sent to DB2: %s",query.c_ptr_safe()));
    shadowStream.addStatement(query);
    
    ++i;
   }
  
  rc = bridge()->execSQL(sqlStream.getPtrToData(),
                         sqlStream.getStatementCount(),
                         getCommitLevel(),
                         FALSE,
                         FALSE,
                         FALSE,
                         dataHandle);
  
  if (rc == 0)
    bridge()->execSQL(shadowStream.getPtrToData(),
                         shadowStream.getStatementCount(),
                         getCommitLevel());
  
  DBUG_RETURN(rc);
}


void
ha_ibmdb2i::unlock_row()
{
  DBUG_ENTER("ha_ibmdb2i::unlock_row");
  DBUG_VOID_RETURN;
}    

int
ha_ibmdb2i::index_end()
{
  DBUG_ENTER("ha_ibmdb2i::index_end");
  warnIfInvalidData();
  last_index_init_rc = 0;
  if (likely(activeReadBuf))
    activeReadBuf->endRead();
  if (likely(!last_index_init_rc))
    releaseIndexFile(active_index);
  active_index= MAX_KEY;
  DBUG_RETURN (0);
}

int ha_ibmdb2i::doCommit(handlerton *hton, THD *thd, bool all)
{
  if (!THDVAR(thd, transaction_unsafe))
  {
    if (all || autoCommitIsOn(thd))
    {
      DBUG_PRINT("ha_ibmdb2i::doCommit",("Committing all"));
      return (db2i_ileBridge::getBridgeForThread(thd)->commitmentControl(QMY_COMMIT));
    }
    else
    {
      DBUG_PRINT("ha_ibmdb2i::doCommit",("Committing stmt"));
      return (db2i_ileBridge::getBridgeForThread(thd)->commitStmtTx());
    }
  }
  
  return (0);
} 


int ha_ibmdb2i::doRollback(handlerton *hton, THD *thd, bool all)
{
  if (!THDVAR(thd,transaction_unsafe))
  {
    if (all || autoCommitIsOn(thd))
    {
      DBUG_PRINT("ha_ibmdb2i::doRollback",("Rolling back all"));
      return ( db2i_ileBridge::getBridgeForThread(thd)->commitmentControl(QMY_ROLLBACK));
    }
    else
    {
      DBUG_PRINT("ha_ibmdb2i::doRollback",("Rolling back stmt"));
      return (db2i_ileBridge::getBridgeForThread(thd)->rollbackStmtTx());
    }
  }
  return (0);
}


void ha_ibmdb2i::start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("ha_ibmdb2i::start_bulk_insert");
  DBUG_PRINT("ha_ibmdb2i::start_bulk_insert",("Rows hinted %d", rows));
  int rc;
  THD* thd = ha_thd();
  int command = thd_sql_command(thd);
  
  if (db2Table->hasBlobs() || 
      (command == SQLCOM_REPLACE || command == SQLCOM_REPLACE_SELECT))
    rows = 1;
  else if (rows == 0)
    rows = DEFAULT_MAX_ROWS_TO_BUFFER; // Shoot the moon
  
 // If we're doing a multi-row insert, binlogging is active, and the table has an
 // auto_increment column, then we'll attempt to lock the file while we perform a 'fast path' blocked
 // insert.  If we can't get the lock, then we'll do a row-by-row 'slow path' insert instead.  The reason is
 // because the MI generates the auto_increment (identity value), and if we can't lock the file,
 // then we can't predetermine what that value will be for insertion into the MySQL write buffer.

  if ((rows > 1) &&                               // Multi-row insert
      (thd->options & OPTION_BIN_LOG) &&          // Binlogging is on
      (table->found_next_number_field))           // Table has an auto_increment column
  {
    if (!dataHandle)
      rc = db2Table->dataFile()->allocateNewInstance(&dataHandle, curConnection);  

    rc = bridge()->lockObj(dataHandle, 1, QMY_LOCK, QMY_LEAR, QMY_YES);
    if (rc==0)                                     // Got the lock
    {
      autoIncLockAcquired = TRUE;               
      got_auto_inc_values = FALSE;                    
    }
    else                                          // Didn't get the lock
      rows = 1;                                   // No problem, but don't block inserts
  }
  
  if (activeHandle == 0)
  {
    last_start_bulk_insert_rc = useDataFile();
    if (last_start_bulk_insert_rc == 0)
      last_start_bulk_insert_rc = prepWriteBuffer(rows, db2Table->dataFile());
  }

  if (last_start_bulk_insert_rc == 0)
    outstanding_start_bulk_insert = true;
  else
  {
    if (autoIncLockAcquired == TRUE)
    {
      bridge()->lockObj(dataHandle,  0, QMY_UNLOCK, QMY_LEAR, QMY_YES);
      autoIncLockAcquired = FALSE;
    }
  }

  DBUG_VOID_RETURN;
}


int ha_ibmdb2i::end_bulk_insert()
{
  DBUG_ENTER("ha_ibmdb2i::end_bulk_insert");
  int rc = 0;
  
  if (outstanding_start_bulk_insert)
  {
    rc = finishBulkInsert();
  }

  my_errno = rc;
    
  DBUG_RETURN(rc);
}

  
int ha_ibmdb2i::prepReadBuffer(ha_rows rowsToRead, const db2i_file* file, char intent)    
{
  DBUG_ENTER("ha_ibmdb2i::prepReadBuffer");
  DBUG_ASSERT(rowsToRead > 0);

  THD* thd = ha_thd();
  char cmtLvl = getCommitLevel(thd);
  
  const db2i_file::RowFormat* format;
  int rc = file->obtainRowFormat(activeHandle, intent, cmtLvl, &format);
  
  if (unlikely(rc)) DBUG_RETURN(rc);
  
  if (lobFieldsRequested())
  {
    forceSingleRowRead = true;
    rowsToRead = 1;
  }
  
  rowsToRead = min(stats.records+1,min(rowsToRead, DEFAULT_MAX_ROWS_TO_BUFFER));
  
  uint bufSize = min((format->readRowLen * rowsToRead), THDVAR(thd, max_read_buffer_size));
  multiRowReadBuf.allocBuf(format->readRowLen, format->readRowNullOffset, bufSize);
  activeReadBuf = &multiRowReadBuf;
    
  if (db2Table->hasBlobs())
  {
    if (!blobReadBuffers)
      blobReadBuffers = new BlobCollection(db2Table, THDVAR(thd, lob_alloc_size));  
    rc = prepareReadBufferForLobs();
    if (rc) DBUG_RETURN(rc);
  }
  
//   if (accessIntent == QMY_UPDATABLE &&
//       thd_tx_isolation(thd) == ISO_REPEATABLE_READ &&
//       !THDVAR(thd, transaction_unsafe))
//     activeReadBuf->update(QMY_READ_ONLY, &releaseRowNeeded, QMY_REPEATABLE_READ);
//   else
    activeReadBuf->update(intent, &releaseRowNeeded, cmtLvl);

  DBUG_RETURN(rc);
}

 
int ha_ibmdb2i::prepWriteBuffer(ha_rows rowsToWrite, const db2i_file* file)
{
  DBUG_ENTER("ha_ibmdb2i::prepWriteBuffer");
  DBUG_ASSERT(accessIntent == QMY_UPDATABLE && rowsToWrite > 0);
  
  const db2i_file::RowFormat* format;
  int rc = file->obtainRowFormat(activeHandle,
                                 QMY_UPDATABLE,
                                 getCommitLevel(ha_thd()),
                                 &format);

  if (unlikely(rc)) DBUG_RETURN(rc);
  
  rowsToWrite = min(rowsToWrite, DEFAULT_MAX_ROWS_TO_BUFFER);
  
  uint bufSize = min((format->writeRowLen * rowsToWrite), THDVAR(ha_thd(), max_write_buffer_size));
  multiRowWriteBuf.allocBuf(format->writeRowLen, format->writeRowNullOffset, bufSize);
  activeWriteBuf = &multiRowWriteBuf;

  if (!blobWriteBuffers && db2Table->hasBlobs())
  {
    blobWriteBuffers = new ValidatedPointer<char>[db2Table->getBlobCount()];
  }    
  DBUG_RETURN(rc);
}


int ha_ibmdb2i::flushWrite(FILE_HANDLE fileHandle, uchar* buf )
{
  DBUG_ENTER("ha_ibmdb2i::flushWrite");
  int rc;
  int64 generatedIdValue = 0;
  bool IdValueWasGenerated = FALSE;
  char* lastDupKeyNamePtr = NULL;
  uint32 lastDupKeyNameLen = 0;
  int loopCnt = 0; 
  bool retry_dup = FALSE; 

 while (loopCnt == 0 || retry_dup == TRUE) 
 {
  rc = bridge()->writeRows(fileHandle,
                           activeWriteBuf->ptr(),
                           getCommitLevel(),  
                           &generatedIdValue,
                           &IdValueWasGenerated,
                           &lastDupKeyRRN,
                           &lastDupKeyNamePtr,
                           &lastDupKeyNameLen,
                           &incrementByValue);
  loopCnt++;  
  retry_dup = FALSE;
  invalidateCachedStats();
  if (lastDupKeyNameLen)
  {
    rrnAssocHandle = fileHandle;
    
    int command = thd_sql_command(ha_thd());

    if (command == SQLCOM_REPLACE ||
        command == SQLCOM_REPLACE_SELECT)
      lastDupKeyID = 0;
    else
    {
      lastDupKeyID = getKeyFromName(lastDupKeyNamePtr, lastDupKeyNameLen);
      
      if (likely(lastDupKeyID != MAX_KEY))
      {
        uint16 failedRow = activeWriteBuf->rowsWritten()+1; 

        if (buf && (failedRow != activeWriteBuf->rowCount()))
        {
          const char* badRow = activeWriteBuf->getRowN(failedRow-1);
          bool savedReadAllColumns = readAllColumns;
          readAllColumns = true;
          mungeDB2row(buf, 
                      badRow, 
                      badRow + activeWriteBuf->getRowNullOffset(),
                      true);
          readAllColumns = savedReadAllColumns;

          if (table->found_next_number_field)
          {
            table->next_number_field->store(next_identity_value - (incrementByValue * (activeWriteBuf->rowCount() - (failedRow - 1))));
          }
        }

        if (default_identity_value &&                 // Table has ID colm and generating a value
           (!autoIncLockAcquired || !got_auto_inc_values) &&
                                                      // Writing first or only row in block
            loopCnt == 1 &&                           // Didn't already retry
            lastDupKeyID == table->s->next_number_index) // Autoinc column is in failed index
 	{  
          if (alterStartWith() == 0)                  // Reset next Identity value to max+1
            retry_dup = TRUE;                         // Rtry the write operation
	} 
      }
      else
      {
        char unknownIndex[MAX_DB2_FILENAME_LENGTH+1];
        convFromEbcdic(lastDupKeyNamePtr, unknownIndex, min(lastDupKeyNameLen, MAX_DB2_FILENAME_LENGTH));
        unknownIndex[min(lastDupKeyNameLen, MAX_DB2_FILENAME_LENGTH)] = 0;        
        getErrTxt(DB2I_ERR_UNKNOWN_IDX, unknownIndex);
      }
    }
  }
 } 

  if ((rc == 0 || rc == HA_ERR_FOUND_DUPP_KEY)
         && default_identity_value && IdValueWasGenerated &&
     (!autoIncLockAcquired || !got_auto_inc_values))
  {
    /* Save the generated identity value for the MySQL last_insert_id() function. */
    insert_id_for_cur_row = generatedIdValue;
 
    /* Store the value into MySQL's buf for row-based replication
       or for an 'on duplicate key update' clause.                      */
    table->next_number_field->store((longlong) generatedIdValue, TRUE);
    if (autoIncLockAcquired)
    {
      got_auto_inc_values = TRUE;
      next_identity_value = generatedIdValue + incrementByValue;
    }
  } 
  else
  {
    if (!autoIncLockAcquired)      // Don't overlay value for first row of a block  
      insert_id_for_cur_row = 0;                                        
  }
  

  activeWriteBuf->resetAfterWrite();
  DBUG_RETURN(rc);
}

int ha_ibmdb2i::alterStartWith() 
{ 
  DBUG_ENTER("ha_ibmdb2i::alterStartWith");  
  int rc = 0; 
  ulonglong nextIdVal; 
  if (!dataHandle) 
     rc = db2Table->dataFile()->allocateNewInstance(&dataHandle, curConnection);
  if (!rc) {rc = bridge()->lockObj(dataHandle, 1, QMY_LOCK, QMY_LENR, QMY_YES);}
  if (!rc) 
  {  
    rc = getNextIdVal(&nextIdVal); 
    if (!rc) {rc = reset_auto_increment(nextIdVal);} 
    bridge()->lockObj(dataHandle,  0, QMY_UNLOCK, QMY_LENR, QMY_YES); 
  } 
  DBUG_RETURN(rc); 
}

bool ha_ibmdb2i::lobFieldsRequested()
{
  if (!db2Table->hasBlobs())
  {
    DBUG_PRINT("ha_ibmdb2i::lobFieldsRequested",("No LOBs"));
    return (false);
  }

  if (readAllColumns)
  {
    DBUG_PRINT("ha_ibmdb2i::lobFieldsRequested",("All cols requested"));
    return (true);
  }
    
  for (int i = 0; i < db2Table->getBlobCount(); ++i)
  {
    if (bitmap_is_set(table->read_set, db2Table->blobFields[i]))
    {
      DBUG_PRINT("ha_ibmdb2i::lobFieldsRequested",("LOB requested"));
      return (true);
    }
  }
  
  DBUG_PRINT("ha_ibmdb2i::lobFieldsRequested",("No LOBs requested"));
  return (false);
}


int ha_ibmdb2i::prepareReadBufferForLobs()
{
  DBUG_ENTER("ha_ibmdb2i::prepareReadBufferForLobs");
  DBUG_ASSERT(db2Table->hasBlobs());
  
  uint32 activeLobFields = 0;
  DB2LobField* lobField;
  uint16 blobCount = db2Table->getBlobCount();
    
  char* readBuf = activeReadBuf->getRowN(0);
  
  for (int i = 0; i < blobCount; ++i)
  {
    int fieldID = db2Table->blobFields[i];
    DB2Field& db2Field = db2Table->db2Field(fieldID);
    lobField = db2Field.asBlobField(readBuf);
    if (readAllColumns ||
        bitmap_is_set(table->read_set, fieldID))
    {
      lobField->dataHandle = (ILEMemHandle)blobReadBuffers->getBufferPtr(fieldID);
      activeLobFields++;
    }
    else
    {
      lobField->dataHandle = NULL;
    }
  }
  
  if (activeLobFields == 0)
  {
    for (int i = 0; i < blobCount; ++i)
    {
      DB2Field& db2Field = db2Table->db2Field(db2Table->blobFields[i]);
      uint16 offset = db2Field.getBufferOffset() + db2Field.calcBlobPad();

      for (int r = 1; r < activeReadBuf->getRowCapacity(); ++r)
      { 
        lobField = (DB2LobField*)(activeReadBuf->getRowN(r) + offset);
        lobField->dataHandle = NULL;
      }      
    }
  }

  activeReadBuf->setRowsToProcess((activeLobFields ? 1 : activeReadBuf->getRowCapacity()));
  int rc = bridge()->objectOverride(activeHandle,
                                    activeReadBuf->ptr(),
                                    activeReadBuf->getRowLength());
  DBUG_RETURN(rc);
}


uint32 ha_ibmdb2i::adjustLobBuffersForRead()
{
  DBUG_ENTER("ha_ibmdb2i::adjustLobBuffersForRead");

  char* readBuf = activeReadBuf->getRowN(0);
   
  for (int i = 0; i < db2Table->getBlobCount(); ++i)
  {
    DB2Field& db2Field = db2Table->db2Field(db2Table->blobFields[i]);
    DB2LobField* lobField = db2Field.asBlobField(readBuf);
    if (readAllColumns || 
        bitmap_is_set(table->read_set, db2Table->blobFields[i]))
    {
      lobField->dataHandle = (ILEMemHandle)blobReadBuffers->reallocBuffer(db2Table->blobFields[i], lobField->length);

      if (lobField->dataHandle == NULL)
        DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }      
    else
    {
      lobField->dataHandle = 0;
    }
  }
  
  int32 rc = bridge()->objectOverride(activeHandle,
                                      activeReadBuf->ptr());
  DBUG_RETURN(rc);
}



int ha_ibmdb2i::reset()
{
  DBUG_ENTER("ha_ibmdb2i::reset");

  if (outstanding_start_bulk_insert)
  {
    finishBulkInsert();
  }  
  
  if (activeHandle != 0)
  {
    releaseActiveHandle();
  }
  
  cleanupBuffers();
  
  db2i_ileBridge::getBridgeForThread(ha_thd())->freeErrorStorage();
    
  last_rnd_init_rc = last_index_init_rc = last_start_bulk_insert_rc = 0;

  returnDupKeysImmediately = false;
  onDupUpdate = false;
  forceSingleRowRead = false; 

#ifndef DBUG_OFF
  cachedBridge=NULL;
#endif      
      
  DBUG_RETURN(0);
}


int32 ha_ibmdb2i::buildCreateIndexStatement(SqlStatementStream& sqlStream, 
                                           KEY& key,
                                           bool isPrimary,
                                           const char* db2LibName,    
                                           const char* db2FileName)
{
  DBUG_ENTER("ha_ibmdb2i::buildCreateIndexStatement");

  char fileSortSequence[11] = "*HEX";
  char fileSortSequenceLibrary[11] = "";
  char fileSortSequenceType = ' ';
  String query(256);
  query.length(0);
  int rc = 0;
  
  if (isPrimary)
  {
    query.append(STRING_WITH_LEN("ALTER TABLE "));
    query.append(db2LibName);
    query.append('.');
    query.append(db2FileName);
    query.append(STRING_WITH_LEN(" ADD PRIMARY KEY "));    
  }
  else
  {
    query.append(STRING_WITH_LEN("CREATE"));

    if (key.flags & HA_NOSAME)
      query.append(STRING_WITH_LEN(" UNIQUE WHERE NOT NULL"));

    query.append(STRING_WITH_LEN(" INDEX "));

    query.append(db2LibName);
    query.append('.');
    if (db2i_table::appendQualifiedIndexFileName(key.name, db2FileName, query))
    {
      getErrTxt(DB2I_ERR_INVALID_NAME,"index","*generated*");
      DBUG_RETURN(DB2I_ERR_INVALID_NAME );
    }

    query.append(STRING_WITH_LEN(" ON "));

    query.append(db2LibName);
    query.append('.');
    query.append(db2FileName);
  }
  
  String fieldDefinition(128);
  rc = buildIndexFieldList(fieldDefinition,
                           key,
                           isPrimary,
                           &fileSortSequenceType, 
                           fileSortSequence,
                           fileSortSequenceLibrary);
  
  if (rc) DBUG_RETURN(rc);
   
  query.append(fieldDefinition);
  
  if ((THDVAR(ha_thd(), create_index_option)==1) &&
      (fileSortSequenceType != 'B') &&
      (fileSortSequenceType != ' '))
  {
    rc = generateShadowIndex(sqlStream, 
                             key, 
                             db2LibName, 
                             db2FileName, 
                             fieldDefinition);
    if (rc) DBUG_RETURN(rc);
  }
    
  DBUG_PRINT("ha_ibmdb2i::buildCreateIndexStatement", ("Sent to DB2: %s",query.c_ptr_safe()));
  sqlStream.addStatement(query,fileSortSequence,fileSortSequenceLibrary);

  DBUG_RETURN(0);
}

/**
  Generate the SQL syntax for the list of fields to be assigned to the 
  specified key. The corresponding sort sequence is also calculated.
      
  @param[out] appendHere  The string to receive the generated SQL
  @param key  The key to evaluate
  @param isPrimary  True if this is being generated on behalf of the primary key
  @param[out] fileSortSequenceType  The type of the associated sort sequence
  @param[out] fileSortSequence  The name of the associated sort sequence
  @param[out] fileSortSequenceLibrary  The library of the associated sort sequence
  
  @return  0 if successful; error value otherwise
*/
int32 ha_ibmdb2i::buildIndexFieldList(String& appendHere,
                                      const KEY& key,
                                      bool isPrimary,
                                      char* fileSortSequenceType, 
                                      char* fileSortSequence, 
                                      char* fileSortSequenceLibrary)
{
  DBUG_ENTER("ha_ibmdb2i::buildIndexFieldList");
  appendHere.append(STRING_WITH_LEN(" ( "));
  for (int j = 0; j < key.key_parts; ++j)
  {
    char colName[MAX_DB2_COLNAME_LENGTH+1];
    if (j != 0)
    {
      appendHere.append(STRING_WITH_LEN(" , "));
    }
    
    KEY_PART_INFO& kpi = key.key_part[j];
    Field* field = kpi.field;
    
    convertMySQLNameToDB2Name(field->field_name, 
                              colName, 
                              sizeof(colName));
    appendHere.append(colName);
    
    int32 rc;
    rc = updateAssociatedSortSequence(field->charset(),
                                      fileSortSequenceType,
                                      fileSortSequence,
                                      fileSortSequenceLibrary);
    if (rc) DBUG_RETURN (rc);
  }
    
  appendHere.append(STRING_WITH_LEN(" ) "));
  
  DBUG_RETURN(0);
}


/**
  Generate an SQL statement that defines a *HEX sorted index to implement 
  the ibmdb2i_create_index.
      
  @param[out] stream  The stream to append the generated statement to
  @param key  The key to evaluate
  @param[out] libName  The library containg the table
  @param[out] fileName  The DB2-compatible name of the table 
  @param[out] fieldDefinition  The list of the fields in the index, in SQL syntax
  
  @return  0 if successful; error value otherwise
*/
int32 ha_ibmdb2i::generateShadowIndex(SqlStatementStream& stream, 
                                      const KEY& key,
                                      const char* libName,
                                      const char* fileName,
                                      const String& fieldDefinition)
{
  String shadowQuery(256);
  shadowQuery.length(0);
  shadowQuery.append(STRING_WITH_LEN("CREATE INDEX "));
  shadowQuery.append(libName);
  shadowQuery.append('.');
  if (db2i_table::appendQualifiedIndexFileName(key.name, fileName, shadowQuery, db2i_table::ASCII_SQL, typeHex))
  {
    getErrTxt(DB2I_ERR_INVALID_NAME,"index","*generated*");
    return DB2I_ERR_INVALID_NAME;
  }
  shadowQuery.append(STRING_WITH_LEN(" ON "));
  shadowQuery.append(libName);
  shadowQuery.append('.');
  shadowQuery.append(fileName);
  shadowQuery.append(fieldDefinition);
  DBUG_PRINT("ha_ibmdb2i::generateShadowIndex", ("Sent to DB2: %s",shadowQuery.c_ptr_safe()));
  stream.addStatement(shadowQuery,"*HEX","QSYS");
  return 0;
}
  
  
void ha_ibmdb2i::doInitialRead(char orientation,
                                uint32 rowsToBuffer,
                                ILEMemHandle key,
                                int keyLength,
                                int keyParts)
{
  DBUG_ENTER("ha_ibmdb2i::doInitialRead");
  
  if (forceSingleRowRead)
    rowsToBuffer = 1;
  else
    rowsToBuffer = min(rowsToBuffer, activeReadBuf->getRowCapacity());
        
  activeReadBuf->newReadRequest(activeHandle,
                                    orientation,
                                    rowsToBuffer,
                                    THDVAR(ha_thd(), async_enabled),
                                    key, 
                                    keyLength,
                                    keyParts);
  DBUG_VOID_RETURN;
}


int ha_ibmdb2i::start_stmt(THD *thd, thr_lock_type lock_type)
{
  DBUG_ENTER("ha_ibmdb2i::start_stmt");
  initBridge(thd);
  if (!THDVAR(thd, transaction_unsafe))
  {
    trans_register_ha(thd, FALSE, ibmdb2i_hton);
    
    if (!autoCommitIsOn(thd))
    {
      bridge()->beginStmtTx();
    }
  }

  DBUG_RETURN(0);
}

int32 ha_ibmdb2i::handleLOBReadOverflow()
{
  DBUG_ENTER("ha_ibmdb2i::handleLOBReadOverflow");
  DBUG_ASSERT(db2Table->hasBlobs() && (activeReadBuf->getRowCapacity() == 1));

  int32 rc = adjustLobBuffersForRead();

  if (!rc)
  {
    activeReadBuf->rewind();
    rc = bridge()->expectErrors(QMY_ERR_END_OF_BLOCK)
                 ->read(activeHandle, 
                        activeReadBuf->ptr(),
                        accessIntent,
                        getCommitLevel(),
                        QMY_SAME);
    releaseRowNeeded = TRUE;

  }
  DBUG_RETURN(rc);
}


int32 ha_ibmdb2i::finishBulkInsert()
{
  int32 rc = 0;

  if (activeWriteBuf->rowCount() && activeHandle)
    rc = flushWrite(activeHandle, table->record[0]);

  if (activeHandle)
    releaseActiveHandle();

  if (autoIncLockAcquired == TRUE)
  {
   // We could check the return code on the unlock, but beware not
   // to overlay the return code from the flushwrite or we will mask
   // duplicate key errors..
    bridge()->lockObj(dataHandle, 0, QMY_UNLOCK, QMY_LEAR, QMY_YES);
    autoIncLockAcquired = FALSE;
  } 
  outstanding_start_bulk_insert = false;
  multiRowWriteBuf.freeBuf();    
  last_start_bulk_insert_rc = 0;

  resetCharacterConversionBuffers();

  return rc;
}

int ha_ibmdb2i::getKeyFromName(const char* name, size_t len)  
{
  for (int i = 0; i < table_share->keys; ++i)
  {
    const char* indexName = db2Table->indexFile(i)->getDB2FileName();
    if ((strncmp(name, indexName, len) == 0) &&
        (strlen(indexName) == len))
    {
      return i;        
    }
  }
  return MAX_KEY;
}

/*                                                                       
Determine the number of I/O's it takes to read through the table.        
                                                                      */
double ha_ibmdb2i::scan_time()
  {
    DBUG_ENTER("ha_ibmdb2i::scan_time");
    DBUG_RETURN(ulonglong2double((stats.data_file_length)/IO_SIZE));
  }


/**
  Estimate the number of I/O's it takes to read a set of ranges through
  an index.                                                            
  
  @param index  
  @param ranges  
  @param rows  
  
  @return The estimate number of I/Os
*/

double ha_ibmdb2i::read_time(uint index, uint ranges, ha_rows rows)
{
  DBUG_ENTER("ha_ibmdb2i::read_time");
  int rc;
  uint64 idxPageCnt = 0;
  double cost;
  
  if (unlikely(rows == HA_POS_ERROR))
    DBUG_RETURN(double(rows) + ranges);

  rc = bridge()->retrieveIndexInfo(db2Table->indexFile(index)->getMasterDefnHandle(),
                                                        &idxPageCnt);                     
  if (!rc)
  {
     if ((idxPageCnt == 1) ||            // Retrieving rows in requested order or
         (ranges == rows))               // 'Sweep' full records retrieval           
       cost = idxPageCnt/4;
     else
     {  
       uint64 totalRecords = stats.records + 1;
       double dataPageCount = stats.data_file_length/IO_SIZE;
                  
       cost = (rows * dataPageCount / totalRecords) + 
               min(idxPageCnt, (log_2(idxPageCnt) * ranges + 
                                rows * (log_2(idxPageCnt) + log_2(rows) - log_2(totalRecords))));
     } 
  }
  else
  {
     cost = rows2double(ranges+rows);    // Use default costing
  }
  DBUG_RETURN(cost);
}

int ha_ibmdb2i::useIndexFile(int idx)
{
  DBUG_ENTER("ha_ibmdb2i::useIndexFile");

  if (activeHandle)
    releaseActiveHandle();

  int rc = 0;

  if (!indexHandles[idx])
    rc = db2Table->indexFile(idx)->allocateNewInstance(&indexHandles[idx], curConnection);

  if (rc == 0)
  {
      activeHandle = indexHandles[idx];
      bumpInUseCounter(1);
  }

   DBUG_RETURN(rc);
}


ulong ha_ibmdb2i::index_flags(uint inx, uint part, bool all_parts) const
{
  return  HA_READ_NEXT | HA_READ_PREV | HA_KEYREAD_ONLY | HA_READ_ORDER | HA_READ_RANGE;
}


static struct st_mysql_sys_var* ibmdb2i_system_variables[] = {
  MYSQL_SYSVAR(rdb_name),
  MYSQL_SYSVAR(transaction_unsafe),
  MYSQL_SYSVAR(lob_alloc_size),
  MYSQL_SYSVAR(max_read_buffer_size),
  MYSQL_SYSVAR(max_write_buffer_size),
  MYSQL_SYSVAR(async_enabled),
  MYSQL_SYSVAR(assume_exclusive_use),
  MYSQL_SYSVAR(compat_opt_blob_cols),
  MYSQL_SYSVAR(compat_opt_time_as_duration),
  MYSQL_SYSVAR(compat_opt_allow_zero_date_vals),
  MYSQL_SYSVAR(compat_opt_year_as_int),
  MYSQL_SYSVAR(propagate_default_col_vals),
  MYSQL_SYSVAR(create_index_option),
//   MYSQL_SYSVAR(discovery_mode),
  MYSQL_SYSVAR(system_trace_level),
  NULL
};


struct st_mysql_storage_engine ibmdb2i_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(ibmdb2i)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &ibmdb2i_storage_engine,
  "IBMDB2I",
  "The IBM development team in Rochester, Minnesota",
  "IBM DB2 for i Storage Engine",
  PLUGIN_LICENSE_GPL,
  ibmdb2i_init_func,                            /* Plugin Init */
  ibmdb2i_done_func,                            /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                                         /* status variables */
  ibmdb2i_system_variables,                       /* system variables */
  NULL                                          /* config options */
}
mysql_declare_plugin_end;

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

/** @file ha_ibmdb2i.h

    @brief

    @note

   @see
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                        /* gcc class implementation */
#endif

#include "as400_types.h"
#include "as400_protos.h"
#include "db2i_global.h"
#include "db2i_ileBridge.h"
#include "builtins.h"
#include "db2i_misc.h"
#include "db2i_file.h"
#include "db2i_blobCollection.h"
#include "db2i_collationSupport.h"
#include "db2i_validatedPointer.h"
#include "db2i_ioBuffers.h"
#include "db2i_errors.h"
#include "db2i_sqlStatementStream.h"
  
/** @brief
  IBMDB2I_SHARE is a structure that will be shared among all open handlers.
  It is used to describe the underlying table definition, and it caches
  table statistics.
*/
struct IBMDB2I_SHARE {
  char *table_name;
  uint table_name_length,use_count;
  pthread_mutex_t mutex;
  THR_LOCK lock;
  
  db2i_table* db2Table;

  class CStats
  {
    public:
      void cacheUpdateTime(time_t time) 
        {update_time = time; initFlag |= lastModTime;}
      time_t getUpdateTime() const 
        {return update_time;}
      void cacheRowCount(ha_rows rows) 
        {records = rows; initFlag |= rowCount;}
      ha_rows getRowCount() const 
        {return records;}
      void cacheDelRowCount(ha_rows rows) 
        {deleted = rows; initFlag |= deletedRowCount;}
      ha_rows getDelRowCount() const 
        {return deleted;}
      void cacheMeanLength(ulong len) 
        {mean_rec_length = len; initFlag |= meanRowLen;}
      ulong getMeanLength()
        {return mean_rec_length;}
      void cacheAugmentedDataLength(ulong len)
        {data_file_length = len; initFlag |= ioCount;}
      ulong getAugmentedDataLength()
        {return data_file_length;}
      bool isInited(uint flags)
        {return initFlag & flags;}
      void invalidate(uint flags)
        {initFlag &= ~flags;}
    
    private:
    uint initFlag;
    time_t update_time;
    ha_rows records;
    ha_rows deleted;
    ulong mean_rec_length;
    ulong data_file_length;
  } cachedStats;
  
};

class ha_ibmdb2i: public handler
{
  THR_LOCK_DATA lock;      ///< MySQL lock
  IBMDB2I_SHARE *share;    ///< Shared lock info

  // The record we are positioned on, together with the handle used to get
  // i.
  uint32 currentRRN;
  uint32 rrnAssocHandle;  
  
  // Dup key values needed by info()
  uint32 lastDupKeyRRN;
  uint32 lastDupKeyID;
  
  bool returnDupKeysImmediately;

  // Dup key value need by update()
  bool onDupUpdate; 

  
  db2i_table* db2Table;

  // The file handle of the PF or LF being accessed by the current operation.
  FILE_HANDLE activeHandle;
  
  // The file handle of the underlying PF
  FILE_HANDLE dataHandle;
  
  // Array of file handles belonging to the underlying LFs
  FILE_HANDLE* indexHandles;

  // Flag to indicate whether a call needs to be made to unlock a row when
  // a read operation has ended. DB2 will handle row unlocking as we move
  // through rows, but if an operation ends before we reach the end of a file,
  // DB2 needs to know to unlock the last row read.
  bool releaseRowNeeded;
  
  // Pointer to a definition of the layout of the row buffer for the file
  // described by activeHandle
  const db2i_file::RowFormat* activeFormat;
   
  IORowBuffer keyBuf;
  uint32 keyLen;
  
  IOWriteBuffer multiRowWriteBuf;
  IOAsyncReadBuffer multiRowReadBuf;
  
  IOAsyncReadBuffer* activeReadBuf;
  IOWriteBuffer* activeWriteBuf;
      
  BlobCollection* blobReadBuffers; // Dynamically allocated per query and used
                               // to manage the buffers used for reading LOBs
  ValidatedPointer<char>* blobWriteBuffers;
  
  // Return codes are not used/honored  by rnd_init and start_bulk_insert
  // so we need a way to signal the failure "downstream" to subsequent
  // functions. 
  int last_rnd_init_rc;
  int last_index_init_rc;
  int last_start_bulk_insert_rc;

  // end_bulk_insert may get called twice for a single start_bulk_insert
  // This is our way to do cleanup only once.  
  bool outstanding_start_bulk_insert;

  // Auto_increment 'increment by' value needed by write_row()
  uint32 incrementByValue;
  bool default_identity_value;

  // Flags and values used during write operations for auto_increment processing 
  bool autoIncLockAcquired;
  bool got_auto_inc_values;
  uint64 next_identity_value;
  
  // The access intent indicated by the last external_locks() call.
  // May be either QMY_READ or QMY_UPDATABLE
  char accessIntent;
  char readAccessIntent;

  ha_rows* indexReadSizeEstimates;

  MEM_ROOT conversionBufferMemroot;
  
  bool forceSingleRowRead;

  bool readAllColumns;
  
  bool invalidDataFound;

  db2i_ileBridge* cachedBridge;
  
  ValidatedObject<volatile uint32> curConnection;
  uint16 activeReferences;
  
public:
  
  ha_ibmdb2i(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_ibmdb2i();

  const char *table_type() const { return "IBMDB2I"; }
  const char *index_type(uint inx) { return "RADIX"; }
  const key_map *keys_to_use_for_scanning() { return &key_map_full; }
  const char **bas_ext() const;

  ulonglong table_flags() const
  {
    return HA_NULL_IN_KEY | HA_REC_NOT_IN_SEQ | HA_AUTO_PART_KEY |
           HA_PARTIAL_COLUMN_READ |                      
           HA_DUPLICATE_POS | HA_NO_PREFIX_CHAR_KEYS |
           HA_HAS_RECORDS | HA_BINLOG_ROW_CAPABLE | HA_REQUIRES_KEY_COLUMNS_FOR_DELETE |
           HA_CAN_INDEX_BLOBS;
  }

  ulong index_flags(uint inx, uint part, bool all_parts) const;

// Note that we do not implement max_supported_record_length.
// We'll let create fail accordingly if the row is
// too long. This allows us to hide the fact that varchars > 32K are being
// implemented as DB2 LOBs.

  uint max_supported_keys()          const { return 4000; }
  uint max_supported_key_parts()     const { return MAX_DB2_KEY_PARTS; }
  uint max_supported_key_length()    const { return 32767; }
  uint max_supported_key_part_length() const { return 32767; }
  double read_time(uint index, uint ranges, ha_rows rows);
  double scan_time();
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(uchar * buf);
  int update_row(const uchar * old_data, uchar * new_data);
  int delete_row(const uchar * buf);
  int index_init(uint idx, bool sorted);
  int index_read(uchar * buf, const uchar * key,
                 uint key_len, enum ha_rkey_function find_flag);
  int index_next(uchar * buf);
  int index_read_last(uchar * buf, const uchar * key, uint key_len);
  int index_next_same(uchar *buf, const uchar *key, uint keylen);
  int index_prev(uchar * buf);
  int index_first(uchar * buf);
  int index_last(uchar * buf);
  int rnd_init(bool scan);                        
  int rnd_end();
  int rnd_next(uchar *buf);                       
  int rnd_pos(uchar * buf, uchar *pos);           
  void position(const uchar *record);             
  int info(uint);
  ha_rows records();                              
  int extra(enum ha_extra_function operation);
  int external_lock(THD *thd, int lock_type);     
  int delete_all_rows(void);
  ha_rows records_in_range(uint inx, key_range *min_key,
                           key_range *max_key);
  int delete_table(const char *from);
  int rename_table(const char * from, const char * to);
  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info);                    
  int updateFrm(TABLE *table_def, File file);
  int openTableDef(TABLE *table_def);
  int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys);
  int prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys);
  int final_drop_index(TABLE *table_arg) {return 0;}
  void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  int reset_auto_increment(ulonglong value);
  void restore_auto_increment(ulonglong prev_insert_id) {return;}
  void update_create_info(HA_CREATE_INFO *create_info);
  int getNextIdVal(ulonglong *value);
  int analyze(THD* thd,HA_CHECK_OPT* check_opt);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  bool can_switch_engines();
  void free_foreign_key_create_info(char* str);
  char* get_foreign_key_create_info();
  int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list);
  uint referenced_by_foreign_key();
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);
  virtual bool get_error_message(int error, String *buf);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);   
                                                              
  bool low_byte_first() const { return 0; }
  void unlock_row();
  int index_end();
  int reset();
  static int doCommit(handlerton *hton, THD *thd, bool all);
  static int doRollback(handlerton *hton, THD *thd, bool all);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  int start_stmt(THD *thd, thr_lock_type lock_type);

  void initBridge(THD* thd = NULL)
  {
    if (thd == NULL) thd = ha_thd();
    DBUG_PRINT("ha_ibmdb2i::initBridge",("Initing bridge. Conn ID=%d", thd->thread_id));
    cachedBridge = db2i_ileBridge::getBridgeForThread(thd);
  }
  
  db2i_ileBridge* bridge() {DBUG_ASSERT(cachedBridge); return cachedBridge;}
  
  static uint8 autoCommitIsOn(THD* thd)
  { return (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) ? QMY_NO : QMY_YES); }
  
  uint8 getCommitLevel();
  uint8 getCommitLevel(THD* thd);

  static int doSavepointSet(THD* thd, char* name)
  {
    return db2i_ileBridge::getBridgeForThread(thd)->savepoint(QMY_SET_SAVEPOINT,
                                                  name);
  }
  
  static int doSavepointRollback(THD* thd, char* name)
  {
    return db2i_ileBridge::getBridgeForThread(thd)->savepoint(QMY_ROLLBACK_SAVEPOINT,
                                                  name);
  }
  
  static int doSavepointRelease(THD* thd, char* name)
  {
    return db2i_ileBridge::getBridgeForThread(thd)->savepoint(QMY_RELEASE_SAVEPOINT,
                                                  name);
  }
  
  // We can't guarantee that the rows we know about when this is called
  // will be the same number of rows that read returns (since DB2 activity
  // may insert additional rows). Therefore, we do as the Federated SE and 
  // return the max possible.
  ha_rows estimate_rows_upper_bound()
  {
    return HA_POS_ERROR;
  }
     
  
private:

  enum enum_TimeFormat
  {
    TIME_OF_DAY,
    DURATION
  };
    
  enum enum_BlobMapping
  {
    AS_BLOB,
    AS_VARCHAR
  };
    
  enum enum_ZeroDate
  {
    NO_SUBSTITUTE,
    SUBSTITUTE_0001_01_01
  };
  
  enum enum_YearFormat
  {
    CHAR4,
    SMALLINT
  };
  
  enum_ZeroDate cachedZeroDateOption;
    
  IBMDB2I_SHARE *get_share(const char *table_name, TABLE *table);       
  int free_share(IBMDB2I_SHARE *share);
  int32 mungeDB2row(uchar* record, const char* dataPtr, const char* nullMapPtr, bool skipLOBs);
  int prepareRowForWrite(char* data, char* nulls, bool honorIdentCols);
  int prepareReadBufferForLobs();
  int32 prepareWriteBufferForLobs();
  uint32 adjustLobBuffersForRead();
  bool lobFieldsRequested();
  int convertFieldChars(enum_conversionDirection direction, 
                        uint16 fieldID, 
                        const char* input, 
                        char* output, 
                        size_t ilen, 
                        size_t olen, 
                        size_t* outDataLen,
                        bool tacitErrors=FALSE,
                        size_t* substChars=NULL);

  /**
    Fast integer log2 function
  */
  uint64 log_2(uint64 val)
  {
    uint64 exp = 0;
    while( (val >> exp) != 0)
    {
      exp++;
    }
    DBUG_ASSERT(exp-1 == (uint64)log2(val));
    return exp-1;
  }

  void bumpInUseCounter(uint16 amount)
  {
    activeReferences += amount;
    DBUG_PRINT("ha_ibmdb2i::bumpInUseCounter", ("activeReferences = %d", activeReferences));
    if (activeReferences)
      curConnection = (uint32)(ha_thd()->thread_id);
    else
      curConnection = 0;
  }
  
        
  int useDataFile()
  {
    DBUG_ENTER("ha_ibmdb2i::useDataFile");    
    
    int rc = 0;
    if (!dataHandle)
      rc = db2Table->dataFile()->allocateNewInstance(&dataHandle, curConnection);
    else if (activeHandle == dataHandle)
      DBUG_RETURN(0);
    
    DBUG_ASSERT(activeHandle == 0);
    
    if (likely(rc == 0))
    {
      activeHandle = dataHandle;
      bumpInUseCounter(1);
    }

    DBUG_RETURN(rc);
  }
  
  void releaseAnyLockedRows()
  {
    if (releaseRowNeeded)
    {
      DBUG_PRINT("ha_ibmdb2i::releaseAnyLockedRows", ("Releasing rows"));
      db2i_ileBridge::getBridgeForThread()->rrlslck(activeHandle, accessIntent);
      releaseRowNeeded = FALSE;
    }
  }

  
  void releaseDataFile()
  {
    DBUG_ENTER("ha_ibmdb2i::releaseDataFile");
    releaseAnyLockedRows();
    bumpInUseCounter(-1);
    DBUG_ASSERT((volatile int)activeReferences >= 0);
    activeHandle = 0;
    DBUG_VOID_RETURN;
  }
 
  int useIndexFile(int idx);
  
  void releaseIndexFile(int idx)
  {
    DBUG_ENTER("ha_ibmdb2i::releaseIndexFile");
    releaseAnyLockedRows();
    bumpInUseCounter(-1);
    DBUG_ASSERT((volatile int)activeReferences >= 0);
    activeHandle = 0;    
    DBUG_VOID_RETURN;
  }
  
  FILE_HANDLE allocateFileHandle(char* database, char* table, int* activityReference, bool hasBlobs);
  
  int updateBuffers(const db2i_file::RowFormat* format, uint rowsToRead, uint rowsToWrite);

  int flushWrite(FILE_HANDLE fileHandle, uchar* buf = NULL);

  int alterStartWith(); 
      
  int buildDB2ConstraintString(LEX* lex, 
                               String& appendHere, 
                               const char* database,
                               Field** fields,
                               char* fileSortSequenceType, 
                               char* fileSortSequence, 
                               char* fileSortSequenceLibrary);
  
  void releaseWriteBuffer();

  void setIndexReadEstimate(uint index, ha_rows rows)
  {
    if (!indexReadSizeEstimates)
    {
      indexReadSizeEstimates = (ha_rows*)my_malloc(sizeof(ha_rows) * table->s->keys, MYF(MY_WME | MY_ZEROFILL));
    }
    indexReadSizeEstimates[index] = rows;
  }
  
  ha_rows getIndexReadEstimate(uint index)
  {
    if (indexReadSizeEstimates)
      return max(indexReadSizeEstimates[index], 1);
    
    return 10000; // Assume index scan if no estimate exists.
  }
  
  
  void quiesceAllFileHandles()
  {
    db2i_ileBridge* bridge = db2i_ileBridge::getBridgeForThread();
    if (dataHandle)
    {
      bridge->quiesceFileInstance(dataHandle);
    }

    for (int idx = 0; idx < table_share->keys; ++idx)
    {
      if (indexHandles[idx] != 0)
      {
        bridge->quiesceFileInstance(indexHandles[idx]);
      }
    }
  }
    
  int32 buildCreateIndexStatement(SqlStatementStream& sqlStream, 
                                 KEY& key,
                                 bool isPrimary,
                                 const char* db2LibName,    
                                 const char* db2FileName);
  
  int32 buildIndexFieldList(String& appendHere,
                            const KEY& key,
                            bool isPrimary,
                            char* fileSortSequenceType, 
                            char* fileSortSequence, 
                            char* fileSortSequenceLibrary);

  // Specify NULL for data when using the data pointed to by field
  int32 convertMySQLtoDB2(Field* field, const DB2Field& db2Field, char* db2Buf, const uchar* data = NULL); 

  int32 convertDB2toMySQL(const DB2Field& db2Field, Field* field, const char* buf);
  int getFieldTypeMapping(Field* field, 
                          String& mapping, 
                          enum_TimeFormat timeFormate,
                          enum_BlobMapping blobMapping,
                          enum_ZeroDate zeroDateHandling,
                          bool propagateDefaults,
                          enum_YearFormat yearFormat);

  int getKeyFromName(const char* name, size_t len);

  void releaseActiveHandle()
  {
    if (activeHandle == dataHandle)
      releaseDataFile();
    else
      releaseIndexFile(active_index);
  }

      
  int32 finishBulkInsert();
  
  void doInitialRead(char orientation,
                       uint32 rowsToBuffer,
                       ILEMemHandle key = 0,
                       int keyLength = 0,
                       int keyParts = 0);
  
  
  int32 readFromBuffer(uchar* destination, char orientation)
  {
    char* row;
    int32 rc = 0;
    row = activeReadBuf->readNextRow(orientation, currentRRN);
    
    if (unlikely(!row))
    {
      rc = activeReadBuf->lastrc();
      if (rc == QMY_ERR_LOB_SPACE_TOO_SMALL)
      {
        rc = handleLOBReadOverflow();
        if (rc == 0)        
        {
          DBUG_ASSERT(activeReadBuf->rowCount() == 1);
          row = activeReadBuf->readNextRow(orientation, currentRRN);
          
          if (unlikely(!row))
            rc = activeReadBuf->lastrc();
        }
      }
    }

    if (likely(rc == 0))
    {
      rrnAssocHandle = activeHandle;
      rc = mungeDB2row(destination, row, row+activeReadBuf->getRowNullOffset(), false);
    }   
    return rc;
  }
  
  int32 handleLOBReadOverflow();

  char* getCharacterConversionBuffer(int fieldId, int length)
  {
    if (unlikely(!alloc_root_inited(&conversionBufferMemroot)))
        init_alloc_root(&conversionBufferMemroot, 8192, 0);
    
    return (char*)alloc_root(&conversionBufferMemroot, length);;
  }      
      
  void resetCharacterConversionBuffers()
  {
    if (alloc_root_inited(&conversionBufferMemroot))
    {
      free_root(&conversionBufferMemroot, MYF(MY_MARK_BLOCKS_FREE));
    }    
  }
  
  void tweakReadSet()
  {
    THD* thd = ha_thd();
    int command = thd_sql_command(thd);
    if ((command == SQLCOM_UPDATE || 
         command == SQLCOM_UPDATE_MULTI) ||
        ((command == SQLCOM_DELETE ||
          command == SQLCOM_DELETE_MULTI) &&
         thd->options & OPTION_BIN_LOG))
      readAllColumns = TRUE;
    else
      readAllColumns = FALSE;
  }
   
  /**
    
  */
  int useFileByHandle(char intent, 
                      FILE_HANDLE handle)
  {
    DBUG_ENTER("ha_ibmdb2i::useFileByHandle");
    
    const db2i_file* file;
    if (handle == dataHandle)
      file = db2Table->dataFile();
    else
    {
      for (uint i = 0; i < table_share->keys; ++i)
      {
        if (indexHandles[i] == handle)
        {
          file = db2Table->indexFile(i);
          active_index = i;
        }
      }
    }
    
    int rc = file->obtainRowFormat(handle, intent, getCommitLevel(), &activeFormat);
    if (likely(rc == 0))
    {
      activeHandle = handle;
      bumpInUseCounter(1);
    }
    
    DBUG_RETURN(rc);   
  }
  
  const db2i_file* getFileForActiveHandle() const
  {
    if (activeHandle == dataHandle)
      return db2Table->dataFile();
    else
      for (uint i = 0; i < table_share->keys; ++i)
        if (indexHandles[i] == activeHandle)
          return db2Table->indexFile(i);
    DBUG_ASSERT(0);
    return NULL;
  }
  
  int prepReadBuffer(ha_rows rowsToRead, const db2i_file* file, char intent);
  int prepWriteBuffer(ha_rows rowsToWrite, const db2i_file* file);
  
  void invalidateCachedStats()
  {
    share->cachedStats.invalidate(rowCount | deletedRowCount | objLength | 
                                  meanRowLen | ioCount);
  }
    
  void warnIfInvalidData()
  {
    if (unlikely(invalidDataFound))
    {
      warning(ha_thd(), DB2I_ERR_INVALID_DATA, table->alias);
    }
  }
  
  /**
    Calculate the maximum value that a particular field can hold.
    
    This is used to anticipate overflows in the auto_increment processing.
  
    @param field  The Field to be analyzed
    
    @return  The maximum value
  */
  static uint64 maxValueForField(const Field* field)
  {
    uint64 maxValue=0;
    switch (field->type())
    {
      case MYSQL_TYPE_TINY: 
        if (((const Field_num*)field)->unsigned_flag)
          maxValue = (1 << 8) - 1;
        else
          maxValue = (1 << 7) - 1;
        break;
      case MYSQL_TYPE_SHORT: 
        if (((const Field_num*)field)->unsigned_flag)
          maxValue = (1 << 16) - 1;
        else
          maxValue = (1 << 15) - 1;
        break;
      case MYSQL_TYPE_INT24: 
        if (((const Field_num*)field)->unsigned_flag)
          maxValue = (1 << 24) - 1;
        else
          maxValue = (1 << 23) - 1;
        break;
      case MYSQL_TYPE_LONG: 
        if (((const Field_num*)field)->unsigned_flag)
          maxValue = (1LL << 32) - 1;
        else
          maxValue = (1 << 31) - 1;
        break;
      case MYSQL_TYPE_LONGLONG: 
        if (((const Field_num*)field)->unsigned_flag)
          maxValue = ~(0LL);
        else
          maxValue = 1 << 63 - 1;
        break;
    }
    
    return maxValue;
  }
  
  void cleanupBuffers()
  {
    if (blobReadBuffers)
    {
      delete blobReadBuffers;
      blobReadBuffers = NULL;
    }
    if (blobWriteBuffers)
    {
      delete[] blobWriteBuffers;
      blobWriteBuffers = NULL;
    }
    if (alloc_root_inited(&conversionBufferMemroot))
    {
      free_root(&conversionBufferMemroot, MYF(0));
    }    
  }
  
  
/**
  Generate a valid RCDFMT name based on the name of the table.
  
  The RCDFMT name is devised by munging the name of the table,
  uppercasing all ascii alpha-numeric characters and replacing all other
  characters with underscores until up to ten characters have been generated.
    
  @param tableName  The name of the table, as given on the MySQL
                    CREATE TABLE statement
  @param[out] query  The string to receive the generated RCDFMT name
*/
  static void generateAndAppendRCDFMT(const char* tableName, String& query)
  {
    char rcdfmt[11];
    
    // The RCDFMT name must begin with an alpha character.
    // We enforce this by skipping to the first alpha character in the table
    // name. If no alpha character exists, we use 'X' for the RCDFMT name;
    
    while (*tableName &&
           (!my_isascii(*tableName) ||
            !my_isalpha(system_charset_info, *tableName)))
    {
      tableName += my_mbcharlen(system_charset_info, *tableName);
    }
    
    if (unlikely(!(*tableName)))
    { 
      rcdfmt[0]= 'X';
      rcdfmt[1]= 0;
    }
    else
    {
      int r= 0;
      while ((r < sizeof(rcdfmt)-1) && *tableName)
      {
        if (my_isascii(*tableName) &&
            my_isalnum(system_charset_info, *tableName))
          rcdfmt[r] = my_toupper(system_charset_info, *tableName);
        else
          rcdfmt[r] = '_';
        
        ++r;
        tableName += my_mbcharlen(system_charset_info, *tableName);
      }
      rcdfmt[r]= 0;
    }
    query.append(STRING_WITH_LEN(" RCDFMT "));
    query.append(rcdfmt);
  }
  
  int32 generateShadowIndex(SqlStatementStream& stream, 
                           const KEY& key,
                           const char* libName,
                           const char* fileName,
                           const String& fieldDefinition);
};

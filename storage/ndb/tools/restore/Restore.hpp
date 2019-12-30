/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

// Restore

#ifndef RESTORE_H
#define RESTORE_H

#include <ndb_global.h>
#include "my_byteorder.h"
#include <NdbOut.hpp>
#include "../src/kernel/blocks/backup/BackupFormat.hpp"
#include <NdbApi.hpp>
#include <util/ndbzio.h>
#include <util/UtilBuffer.hpp>

#include <ndb_version.h>
#include <version.h>
#include <NdbMutex.h>

#define NDB_RESTORE_STAGING_SUFFIX "$ST"
#ifdef ERROR_INSERT
#define NDB_RESTORE_ERROR_INSERT_SMALL_BUFFER 1
#endif

enum TableChangesMask
{
  /**
   * Allow attribute type promotion
   */
  TCM_ATTRIBUTE_PROMOTION = 0x1,

  /**
   * Allow missing columns
   */
  TCM_EXCLUDE_MISSING_COLUMNS = 0x2,

  /**
   * Allow attribute type demotion and integral signed/unsigned type changes.
   */
  TCM_ATTRIBUTE_DEMOTION = 0x4
};

typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Column NDBCOL;
typedef  void* (*AttrConvertFunc)(const void *old_data, 
                                  void *parameter,
                                  bool &truncated);

struct AttributeData {
  bool null;
  Uint32 size;
  union {
    Int8 * int8_value;
    Uint8 * u_int8_value;
    
    Int16 * int16_value;
    Uint16 * u_int16_value;

    Int32 * int32_value;
    Uint32 * u_int32_value;
    
    Int64 * int64_value;
    Uint64 * u_int64_value;

    char * string_value;
    
    void* void_value;
  };
};

struct AttributeDesc {
  //private:
  friend class TupleS;
  friend class TableS;
  friend class RestoreDataIterator;
  friend class RestoreMetaData;
  friend class AttributeS;
  Uint32 size; // bits       
  Uint32 arraySize;
  Uint32 attrId;
  NdbDictionary::Column *m_column;

  bool m_exclude;
  Uint32 m_nullBitIndex;
  AttrConvertFunc convertFunc;
  void *parameter;
  Uint32 parameterSz; 
  bool truncation_detected;
  bool staging;

public:
  
  AttributeDesc(NdbDictionary::Column *column);
  AttributeDesc();

  Uint32 getSizeInWords() const { return (size * arraySize + 31)/ 32;}
  Uint32 getSizeInBytes() const {
    assert(size >= 8);
    return (size / 8) * arraySize;
  }
}; // AttributeDesc

class AttributeS {
public:
  AttributeDesc * Desc;
  AttributeData Data;
  void printAttributeValue() const;
};

class TupleS {
private:
  friend class RestoreDataIterator;
  
  class TableS *m_currentTable;
  AttributeData *allAttrData;
  bool prepareRecord(TableS &);
  
public:
  TupleS() {
    m_currentTable= 0;
    allAttrData= 0;
  }
  ~TupleS()
  {
    if (allAttrData)
      delete [] allAttrData;
  }
  TupleS(const TupleS& tuple); // disable copy constructor
  TupleS & operator=(const TupleS& tuple);
  int getNoOfAttributes() const;
  TableS * getTable() const;
  AttributeDesc * getDesc(int i) const;
  AttributeData * getData(int i) const;
}; // class TupleS

struct FragmentInfo
{
  Uint32 fragmentNo;
  Uint64 noOfRecords;
  Uint32 filePosLow;
  Uint32 filePosHigh;
};

class TableS {
  
  friend class TupleS;
  friend class RestoreMetaData;
  friend class RestoreDataIterator;
  
  Uint32 schemaVersion;
  Uint32 backupVersion;
  Vector<AttributeDesc *> allAttributesDesc;
  Vector<AttributeDesc *> m_fixedKeys;
  //Vector<AttributeDesc *> m_variableKey; 
  Vector<AttributeDesc *> m_fixedAttribs;
  Vector<AttributeDesc *> m_variableAttribs;
  
  Uint32 m_noOfNullable;
  Uint32 m_nullBitmaskSize;

  AttributeDesc * m_auto_val_attrib;
  Uint64 m_max_auto_val;

  bool m_isSysTable;
  bool m_isSYSTAB_0;
  bool m_broken;

  TableS *m_main_table;
  Uint32 m_main_column_id;
  Uint32 m_local_id;

  Uint64 m_noOfRecords;
  Vector<FragmentInfo *> m_fragmentInfo;

  void createAttr(NdbDictionary::Column *column);

public:
  class NdbDictionary::Table* m_dictTable;
  TableS (Uint32 version, class NdbTableImpl* dictTable);
  ~TableS();

  Uint32 getTableId() const { 
    return m_dictTable->getTableId(); 
  }
  Uint32 getLocalId() const { 
    return m_local_id; 
  }
  Uint64 getNoOfRecords() const { 
    return m_noOfRecords; 
  }
  /*
  void setMysqlTableName(char * tableName) {
    strpcpy(mysqlTableName, tableName);
  }
  
  char * 
  void setMysqlDatabaseName(char * databaseName) {
    strpcpy(mysqlDatabaseName, databaseName);
  }

  table.setMysqlDatabaseName(database);
  */
  void setBackupVersion(Uint32 version) { 
    backupVersion = version;
  }
  
  Uint32 getBackupVersion() const { 
    return backupVersion;
  }
  
  const char * getTableName() const { 
    return m_dictTable->getName();
  }
  
  int getNoOfAttributes() const { 
    return allAttributesDesc.size();
  }
  
  bool have_auto_inc() const {
    return m_auto_val_attrib != 0;
  }

  bool have_auto_inc(Uint32 id) const {
    return (m_auto_val_attrib ? m_auto_val_attrib->attrId == id : false);
  }

  bool get_auto_data(const TupleS & tuple, Uint32 * syskey, Uint64 * nextid) const;

  /**
   * Get attribute descriptor
   */
  const AttributeDesc * operator[](int attributeId) const { 
    return allAttributesDesc[attributeId]; 
  }

  AttributeDesc *getAttributeDesc(int attributeId) const {
    return allAttributesDesc[attributeId];
  }

  bool getSysTable() const {
    return m_isSysTable;
  }

  const TableS *getMainTable() const {
    return m_main_table;
  }
 
  Uint32 getMainColumnId() const {
    return m_main_column_id;
  }

  TableS& operator=(TableS& org) ;

  bool isSYSTAB_0() const {
    return m_isSYSTAB_0;
  } 

  inline
  bool isBroken() const {
    return m_broken || (m_main_table && m_main_table->isBroken());
  }

  bool m_staging;
  BaseString m_stagingName;
  NdbDictionary::Table* m_stagingTable;
  int m_stagingFlags;
}; // TableS;

class RestoreLogIterator;

class BackupFile {
protected:
  ndbzio_stream m_file;
  char m_path[PATH_MAX];
  char m_fileName[PATH_MAX];
  bool m_hostByteOrder;
  BackupFormat::FileHeader m_fileHeader;
  BackupFormat::FileHeader m_expectedFileHeader;
  
  Uint32 m_nodeId;
  
  void * m_buffer;
  void * m_buffer_ptr;
  Uint32 m_buffer_sz;
  Uint32 m_buffer_data_left;
#ifdef ERROR_INSERT
  unsigned m_error_insert;
#endif
  Uint64 m_file_size;
  Uint64 m_file_pos;
  
  UtilBuffer m_twiddle_buffer;

  bool  m_is_undolog;
  void (* free_data_callback)(void*);
  void *m_ctx; // context for callback function

  virtual void reset_buffers() {}

  // In case of multiple backup parts, each backup part is
  // identified by a unique part ID, which is m_part_id.
  Uint32 m_part_id;
  Uint32 m_part_count;

  bool openFile();
  void setCtlFile(Uint32 nodeId, Uint32 backupId, const char * path);
  void setDataFile(const BackupFile & bf, Uint32 no);
  void setLogFile(const BackupFile & bf, Uint32 no);
  
  Uint32 buffer_get_ptr(void **p_buf_ptr, Uint32 size, Uint32 nmemb);
  Uint32 buffer_read(void *ptr, Uint32 size, Uint32 nmemb);
  Uint32 buffer_get_ptr_ahead(void **p_buf_ptr, Uint32 size, Uint32 nmemb);
  Uint32 buffer_read_ahead(void *ptr, Uint32 size, Uint32 nmemb);

  void setName(const char * path, const char * name);

  BackupFile(void (* free_data_callback)(void*) = 0, void *ctx = 0);
  virtual ~BackupFile();

public:
  bool readHeader();
  bool validateFooter();
  bool validateBackupFile();

  const char * getPath() const { return m_path;}
  const char * getFilename() const { return m_fileName;}
  Uint32 getNodeId() const { return m_nodeId;}
  const BackupFormat::FileHeader & getFileHeader() const { return m_fileHeader;}
  bool Twiddle(const AttributeDesc * const attr_desc,
               AttributeData * attr_data);

  Uint64 get_file_size() const { return m_file_size; }
  /**
   * get_file_size() and get_file_pos() are used to calculate restore
   * progress percentage and works fine in normal mode.
   *
   * But, when compressed backup is enabled, m_file_pos gives the current file
   * position in uncompressed state and m_file_size gives the backup file size
   * in compressed state. So, Instead of m_file_pos, ndbzio_stream's m_file.in
   * parameter is used to get current position in compressed state.This
   * parameter also works when compressed backup is disabled.
   */
  Uint64 get_file_pos() const { return m_file.in; }
#ifdef ERROR_INSERT
  void error_insert(unsigned int code); 
#endif
  static const Uint32 BUFFER_SIZE = 128*1024;
private:
  void
  twiddle_atribute(const AttributeDesc * const attr_desc,
                   AttributeData* attr_data);
};

struct DictObject {
  Uint32 m_objType;
  void * m_objPtr;
};

class RestoreMetaData : public BackupFile {

  Vector<TableS *> allTables;
  bool readMetaFileHeader();
  bool readMetaTableDesc();
  bool markSysTables();
  bool fixBlobs();
		
  bool readGCPEntry();
  bool readFragmentInfo();
  Uint32 readMetaTableList();

  Uint32 m_startGCP;
  Uint32 m_stopGCP;
  
  bool parseTableDescriptor(const Uint32 * data, Uint32 len);

  Vector<DictObject> m_objects;
  
public:
  RestoreMetaData(const char * path, Uint32 nodeId, Uint32 bNo,
                  Uint32 partId, Uint32 partCount);
  virtual ~RestoreMetaData();
  
  int loadContent();
		  
  Uint32 getNoOfTables() const { return allTables.size();}
  
  const TableS * operator[](int i) const { return allTables[i];}
  TableS * operator[](int i) { return allTables[i];}
  TableS * getTable(Uint32 tableId) const;

  Uint32 getNoOfObjects() const { return m_objects.size();}
  Uint32 getObjType(Uint32 i) const { return m_objects[i].m_objType; }
  void* getObjPtr(Uint32 i) const { return m_objects[i].m_objPtr; }
  
  Uint32 getStartGCP() const;
  Uint32 getStopGCP() const;
  Uint32 getNdbVersion() const { return m_fileHeader.NdbVersion; }
}; // RestoreMetaData


class RestoreDataIterator : public BackupFile {
  const RestoreMetaData & m_metaData;
  Uint32 m_count;
  TableS* m_currentTable;
  TupleS m_tuple;

public:

  // Constructor
  RestoreDataIterator(const RestoreMetaData &,
                      void (* free_data_callback)(void*), void*);
  virtual ~RestoreDataIterator();
  
  // Read data file fragment header
  bool readFragmentHeader(int & res, Uint32 *fragmentId);
  bool validateFragmentFooter();
  bool validateRestoreDataIterator();

  const TupleS *getNextTuple(int & res);
  TableS *getCurrentTable();

private:
  void init_bitfield_storage(const NdbDictionary::Table*);
  void free_bitfield_storage();
  void reset_bitfield_storage();
  Uint32* get_bitfield_storage(Uint32 len);
  Uint32 get_free_bitfield_storage() const;

  Uint32 m_row_bitfield_len; // in words
  Uint32* m_bitfield_storage_ptr;
  Uint32* m_bitfield_storage_curr_ptr;
  Uint32 m_bitfield_storage_len; // In words

protected:
  virtual void reset_buffers() { reset_bitfield_storage();}

  int readTupleData_old(Uint32 *buf_ptr, Uint32 dataLength);
  int readTupleData_packed(Uint32 *buf_ptr, Uint32 dataLength);

  int readVarData(Uint32 *buf_ptr, Uint32 *ptr, Uint32 dataLength);
  int readVarData_drop6(Uint32 *buf_ptr, Uint32 *ptr, Uint32 dataLength);
};

class LogEntry {
public:
  enum EntryType {
    LE_INSERT,
    LE_DELETE,
    LE_UPDATE
  };
  Uint32 m_frag_id;
  EntryType m_type;
  TableS * m_table;
  Vector<AttributeS*> m_values;
  Vector<AttributeS*> m_values_e;
  AttributeS *add_attr() {
    AttributeS * attr;
    if (m_values_e.size() > 0) {
      attr = m_values_e[m_values_e.size()-1];
      m_values_e.erase(m_values_e.size()-1);
    }
    else
    {
      attr = new AttributeS;
    }
    m_values.push_back(attr);
    return attr;
  }
  void clear() {
    for(Uint32 i= 0; i < m_values.size(); i++)
      m_values_e.push_back(m_values[i]);
    m_values.clear();
  }
  LogEntry() {}
  ~LogEntry()
  {
    Uint32 i;
    for(i= 0; i< m_values.size(); i++)
      delete m_values[i];
    for(i= 0; i< m_values_e.size(); i++)
      delete m_values_e[i];
  }
  Uint32 size() const { return m_values.size(); }
  const AttributeS * operator[](int i) const { return m_values[i];}
  void printSqlLog() const;
};

class RestoreLogIterator : public BackupFile {
  /* The BackupFile buffer need to be big enough for biggest log entry data,
   * not including log entry header.
   * No harm in require space for a few extra words to header too.
   */
  static_assert(BackupFile::BUFFER_SIZE >=
                  BackupFormat::LogFile::LogEntry::MAX_SIZE,
                "");
private:
  const RestoreMetaData & m_metaData;

  Uint32 m_count;  
  Uint32 m_last_gci;
  LogEntry m_logEntry;
public:
  RestoreLogIterator(const RestoreMetaData &);
  virtual ~RestoreLogIterator() {}

  bool isSnapshotstartBackup()
  {
    return m_is_undolog;
  }
  const LogEntry * getNextLogEntry(int & res);
};

class RestoreLogger {
public:
  RestoreLogger();
  ~RestoreLogger();
  void log_info(const char* fmt, ...)
         ATTRIBUTE_FORMAT(printf, 2, 3);
  void log_debug(const char* fmt, ...)
         ATTRIBUTE_FORMAT(printf, 2, 3);
  void log_error(const char* fmt, ...)
         ATTRIBUTE_FORMAT(printf, 2, 3);
  void setThreadPrefix(const char* prefix);
  const char* getThreadPrefix() const;
private:
  NdbMutex *m_mutex;
};

NdbOut& operator<<(NdbOut& ndbout, const TableS&);
NdbOut& operator<<(NdbOut& ndbout, const TupleS&);
NdbOut& operator<<(NdbOut& ndbout, const LogEntry&);
NdbOut& operator<<(NdbOut& ndbout, const RestoreMetaData&);

bool readSYSTAB_0(const TupleS & tup, Uint32 * syskey, Uint64 * nextid);

#endif



/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef RESTORE_H
#define RESTORE_H

#include <ndb_global.h>
#include <NdbOut.hpp>
#include "../src/kernel/blocks/backup/BackupFormat.hpp"
#include "../src/ndbapi/NdbDictionaryImpl.hpp"
#include <NdbApi.hpp>

#include <ndb_version.h>
#include <version.h>

const int FileNameLenC = 256;
const int TableNameLenC = 256;
const int AttrNameLenC = 256;
const Uint32 timeToWaitForNdbC = 10000;
const Uint32 opsDefaultC = 1000;

// Forward declarations
//class AttributeDesc;
struct AttributeDesc;
struct AttributeData;
struct AttributeS;

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
  friend struct AttributeS;
  Uint32 size; // bits       
  Uint32 arraySize;
  Uint32 attrId;
  NdbDictionary::Column *m_column;

  Uint32 m_nullBitIndex;
public:
  
  AttributeDesc(NdbDictionary::Column *column);
  AttributeDesc();

  Uint32 getSizeInWords() const { return (size * arraySize + 31)/ 32;}
}; // AttributeDesc

struct AttributeS {
  const AttributeDesc * Desc;
  AttributeData Data;
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
  };
  ~TupleS()
  {
    if (allAttrData)
      delete [] allAttrData;
  };
  TupleS(const TupleS& tuple); // disable copy constructor
  TupleS & operator=(const TupleS& tuple);
  int getNoOfAttributes() const;
  TableS * getTable() const;
  const AttributeDesc * getDesc(int i) const;
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

  Uint32 m_auto_val_id;
  Uint64 m_max_auto_val;

  bool isSysTable;
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
  Uint32 getNoOfRecords() const { 
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
  };
  
  bool have_auto_inc() const {
    return m_auto_val_id != ~(Uint32)0;
  };

  bool have_auto_inc(Uint32 id) const {
    return m_auto_val_id == id;
  };

  Uint64 get_max_auto_val() const {
    return m_max_auto_val;
  };

  void update_max_auto_val(const char *data, int size) {
    union {
      Uint8  u8;
      Uint16 u16;
      Uint32 u32;
    } val;
    Uint64 v;
    switch(size){
    case 64:
      memcpy(&v,data,8);
      break;
    case 32:
      memcpy(&val.u32,data,4);
      v= val.u32;
      break;
    case 24:
      v= uint3korr((unsigned char*)data);
      break;
    case 16:
      memcpy(&val.u16,data,2);
      v= val.u16;
      break;
    case 8:
      memcpy(&val.u8,data,1);
      v= val.u8;
      break;
    default:
      return;
    };
    if(v > m_max_auto_val)
      m_max_auto_val= v;
  };
  /**
   * Get attribute descriptor
   */
  const AttributeDesc * operator[](int attributeId) const { 
    return allAttributesDesc[attributeId]; 
  }

  bool getSysTable() const {
    return isSysTable;
  }

  const TableS *getMainTable() const {
    return m_main_table;
  }

  TableS& operator=(TableS& org) ; 
}; // TableS;

class RestoreLogIterator;

class BackupFile {
protected:
  FILE * m_file;
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

  Uint64 m_file_size;
  Uint64 m_file_pos;

  void (* free_data_callback)();

  bool openFile();
  void setCtlFile(Uint32 nodeId, Uint32 backupId, const char * path);
  void setDataFile(const BackupFile & bf, Uint32 no);
  void setLogFile(const BackupFile & bf, Uint32 no);
  
  Uint32 buffer_get_ptr(void **p_buf_ptr, Uint32 size, Uint32 nmemb);
  Uint32 buffer_read(void *ptr, Uint32 size, Uint32 nmemb);
  Uint32 buffer_get_ptr_ahead(void **p_buf_ptr, Uint32 size, Uint32 nmemb);
  Uint32 buffer_read_ahead(void *ptr, Uint32 size, Uint32 nmemb);

  void setName(const char * path, const char * name);

  BackupFile(void (* free_data_callback)() = 0);
  ~BackupFile();
public:
  bool readHeader();
  bool validateFooter();

  const char * getPath() const { return m_path;}
  const char * getFilename() const { return m_fileName;}
  Uint32 getNodeId() const { return m_nodeId;}
  const BackupFormat::FileHeader & getFileHeader() const { return m_fileHeader;}
  bool Twiddle(const AttributeDesc *  attr_desc, AttributeData * attr_data, Uint32 arraySize = 0);

  Uint64 get_file_size() const { return m_file_size; }
  Uint64 get_file_pos() const { return m_file_pos; }
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
  RestoreMetaData(const char * path, Uint32 nodeId, Uint32 bNo);
  virtual ~RestoreMetaData();
  
  int loadContent();
		  
  Uint32 getNoOfTables() const { return allTables.size();}
  
  const TableS * operator[](int i) const { return allTables[i];}
  TableS * getTable(Uint32 tableId) const;

  Uint32 getNoOfObjects() const { return m_objects.size();}
  Uint32 getObjType(Uint32 i) const { return m_objects[i].m_objType; }
  void* getObjPtr(Uint32 i) const { return m_objects[i].m_objPtr; }
  
  Uint32 getStopGCP() const;
}; // RestoreMetaData


class RestoreDataIterator : public BackupFile {
  const RestoreMetaData & m_metaData;
  Uint32 m_count;
  TableS* m_currentTable;
  TupleS m_tuple;

public:

  // Constructor
  RestoreDataIterator(const RestoreMetaData &, void (* free_data_callback)());
  ~RestoreDataIterator() {};
  
  // Read data file fragment header
  bool readFragmentHeader(int & res, Uint32 *fragmentId);
  bool validateFragmentFooter();

  const TupleS *getNextTuple(int & res);

private:

  int readTupleData(Uint32 *buf_ptr, Uint32 *ptr, Uint32 dataLength);
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
};

class RestoreLogIterator : public BackupFile {
private:
  const RestoreMetaData & m_metaData;

  Uint32 m_count;  
  Uint32 m_last_gci;
  LogEntry m_logEntry;
public:
  RestoreLogIterator(const RestoreMetaData &);
  virtual ~RestoreLogIterator() {};

  const LogEntry * getNextLogEntry(int & res);
};

NdbOut& operator<<(NdbOut& ndbout, const TableS&);
NdbOut& operator<<(NdbOut& ndbout, const TupleS&);
NdbOut& operator<<(NdbOut& ndbout, const LogEntry&);
NdbOut& operator<<(NdbOut& ndbout, const RestoreMetaData&);

#endif



/* Copyright (C) 2003 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef RESTORE_H
#define RESTORE_H

#include <ndb_global.h>
#include <BackupFormat.hpp>
#include <NdbApi.hpp>
#include <NdbSchemaCon.hpp>
#include "myVector.hpp"

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
  // TODO (sometimes): use a temporary variable in DTIMAP so we can
  //                   hide AttributeDesc private variables
  friend class TupleS;
  friend class TableS;
  friend class RestoreDataIterator;
  friend class RestoreMetaData;
  friend struct AttributeS;
  char name[AttrNameLenC];
  Uint32 attrId;
  AttrType type;
  bool nullable;
  KeyType key; 
  Uint32 size; // bits       
  Uint32 arraySize;

  Uint32 m_nullBitIndex;
public:
  
  AttributeDesc() {
    name[0] = 0;
  } 

  const TableS * m_table;
  Uint32 getSizeInWords() const { return (size * arraySize + 31)/ 32;}
}; // AttributeDesc

struct AttributeS {
  const AttributeDesc * Desc;
  AttributeData Data;
};

class TupleS {
private:
  friend class RestoreDataIterator;
  
  const TableS * m_currentTable;
  myVector<AttributeS*> allAttributes;
  Uint32 * dataRecord;
  bool prepareRecord(const TableS &);
  
public:
  TupleS() {dataRecord = NULL;};
  ~TupleS() {if(dataRecord != NULL) delete [] dataRecord;};
  int getNoOfAttributes() const { return allAttributes.size(); };  
  const TableS * getTable() const { return m_currentTable;};
  const AttributeS * operator[](int i) const { return allAttributes[i];};
  Uint32 * getDataRecord() { return dataRecord;};
  void createDataRecord(Uint32 bytes) { dataRecord = new Uint32[bytes];};
}; // class TupleS

class TableS {
  
  friend class TupleS;
  friend class RestoreMetaData;
  friend class RestoreDataIterator;
  
  Uint32 tableId;
  char tableName[TableNameLenC];
  Uint32 schemaVersion;
  Uint32 backupVersion;
  myVector<AttributeDesc *> allAttributesDesc;
  myVector<AttributeDesc *> m_fixedKeys;
  //myVector<AttributeDesc *> m_variableKey; 
  myVector<AttributeDesc *> m_fixedAttribs;
  myVector<AttributeDesc *> m_variableAttribs;
  
  Uint32 m_noOfNullable;
  Uint32 m_nullBitmaskSize;

  int pos;
  char create_string[2048];
  /*
  char mysqlTableName[1024];
  char mysqlDatabaseName[1024];
  */

  void createAttr(const char* name, 
		  const AttrType type, 
		  const unsigned int size, // in bits 
		  const unsigned int arraySize, 
		  const bool nullable,
		  const KeyType key);

public:
  class NdbDictionary::Table* m_dictTable;
  TableS (const char * name){
    snprintf(tableName, sizeof(tableName), name);
    m_noOfNullable = m_nullBitmaskSize = 0;
  }

  void setTableId (Uint32 id) { 
    tableId = id; 
  }
  
  Uint32 getTableId() const { 
    return tableId; 
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
  
  /**
   * Get attribute descriptor
   */
  const AttributeDesc * operator[](int attributeId) const { 
    return allAttributesDesc[attributeId]; 
  }

  TableS& operator=(TableS& org) ; 
}; // TableS;

class BackupFile {
protected:
  FILE * m_file;
  char m_path[PATH_MAX];
  char m_fileName[PATH_MAX];
  bool m_hostByteOrder;
  BackupFormat::FileHeader m_fileHeader;
  BackupFormat::FileHeader m_expectedFileHeader;
  
  Uint32 m_nodeId;
  Uint32 * m_buffer;
  Uint32 m_bufferSize;
  Uint32 * createBuffer(Uint32 bytes);
  
  bool openFile();
  void setCtlFile(Uint32 nodeId, Uint32 backupId, const char * path);
  void setDataFile(const BackupFile & bf, Uint32 no);
  void setLogFile(const BackupFile & bf, Uint32 no);
  
  void setName(const char * path, const char * name);

  BackupFile();
  ~BackupFile();
public:
  bool readHeader();
  bool validateFooter();

  const char * getPath() const { return m_path;}
  const char * getFilename() const { return m_fileName;}
  Uint32 getNodeId() const { return m_nodeId;}
  const BackupFormat::FileHeader & getFileHeader() const { return m_fileHeader;}
  bool Twiddle(AttributeS * attr, Uint32 arraySize = 0);
};

class RestoreMetaData : public BackupFile {

  myVector<TableS *> allTables;
  bool readMetaFileHeader();
  bool readMetaTableDesc();
		
  bool readGCPEntry();
  Uint32 readMetaTableList();

  Uint32 m_startGCP;
  Uint32 m_stopGCP;
  
  bool parseTableDescriptor(const Uint32 * data, Uint32 len);

public:

  RestoreMetaData(const char * path, Uint32 nodeId, Uint32 bNo);
  ~RestoreMetaData();
  
  int loadContent();
		  
		
  
  Uint32 getNoOfTables() const { return allTables.size();}
  
  const TableS * operator[](int i) const { return allTables[i];}
  const TableS * getTable(Uint32 tableId) const;

  Uint32 getStopGCP() const;
}; // RestoreMetaData


class RestoreDataIterator : public BackupFile {
  const RestoreMetaData & m_metaData;

  Uint32 m_count;
  TupleS  m_tuple;
  const TableS* m_currentTable;
public:

  // Constructor
  RestoreDataIterator(const RestoreMetaData &);
  ~RestoreDataIterator();
  
  // Read data file fragment header
  bool readFragmentHeader(int & res);
  bool validateFragmentFooter();
  
  const TupleS *getNextTuple(int & res);
};

class LogEntry {
public:
  enum EntryType {
    LE_INSERT,
    LE_DELETE,
    LE_UPDATE
  };
  EntryType m_type;
  const TableS * m_table;  
  myVector<AttributeS*> m_values;
  

};

class RestoreLogIterator : public BackupFile {
private:
  const RestoreMetaData & m_metaData;

  Uint32 m_count;  
  LogEntry m_logEntry;
public:
  RestoreLogIterator(const RestoreMetaData &);
  
  const LogEntry * getNextLogEntry(int & res);
};

#endif



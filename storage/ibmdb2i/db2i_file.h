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


#ifndef DB2I_FILE_H
#define DB2I_FILE_H

#include "db2i_global.h"
#include "db2i_ileBridge.h"
#include "db2i_validatedPointer.h"
#include "db2i_iconv.h"
#include "db2i_charsetSupport.h"

const char FID_EXT[] = ".FID";

class db2i_file;
  
#pragma pack(1)
struct DB2LobField
{
  char reserved1;
  uint32 length;
  char reserved2[4];
  uint32 ordinal;
  ILEMemHandle dataHandle;
  char reserved3[8];
};
#pragma pack(pop)

class DB2Field
{ 
  public:
    uint16 getType() const { return *(uint16*)(&definition.ColType); }
    uint16 getByteLengthInRecord() const { return definition.ColLen; }
    uint16 getDataLengthInRecord() const
    { 
      return (getType() == QMY_VARCHAR || getType() == QMY_VARGRAPHIC ? definition.ColLen - 2 : definition.ColLen); 
    }
    uint16 getCCSID() const { return *(uint16*)(&definition.ColCCSID); }
    bool isBlob() const
    { 
      uint16 type = getType();
      return (type == QMY_BLOBCLOB || type == QMY_DBCLOB); 
    }
    uint16 getBufferOffset() const { return definition.ColBufOff; }
    uint16 calcBlobPad() const
    {
      DBUG_ASSERT(isBlob());
      return getByteLengthInRecord() - sizeof (DB2LobField);
    }
    DB2LobField* asBlobField(char* buf) const
    {
      DBUG_ASSERT(isBlob());
      return (DB2LobField*)(buf + getBufferOffset() + calcBlobPad());
    }
  private:
   col_def_t definition;
};
  

/**
  @class db2i_table
  
  @details 
  This class describes the logical SQL table provided by DB2. 
  It stores "table-scoped" information such as the name of the
  DB2 schema, BLOB descriptions, and the corresponding MySQL table definition.
  Only one instance exists per SQL table.
*/
class db2i_table
{
  public: 
  enum NameFormatFlags
  {
    ASCII_SQL,
    ASCII_NATIVE,
    EBCDIC_NATIVE
  };
    
  db2i_table(const TABLE_SHARE* myTable, const char* path = NULL);
  
  ~db2i_table();

  int32 initDB2Objects(const char* path);

  const TABLE_SHARE* getMySQLTable() const
  {
    return mysqlTable;
  }
  
  uint64 getStartId() const
  {
    return db2StartId;
  }

  void updateStartId(uint64 newStartId)
  {
     db2StartId = newStartId;
  }

  bool hasBlobs() const
  {
    return (blobFieldCount > 0);
  }
  
  uint16 getBlobCount() const
  {
    return blobFieldCount;
  }
  
  uint getBlobFieldActualSize(uint fieldIndex) const
  {
    return blobFieldActualSizes[getBlobIdFromField(fieldIndex)];
  }

  void updateBlobFieldActualSize(uint fieldIndex, uint32 newSize)
  {
    // It's OK that this isn't threadsafe, since this is just an advisory
    // value. If a race condition causes the lesser of two values to be stored,
    // that's OK.
    uint16 blobID = getBlobIdFromField(fieldIndex);
    DBUG_ASSERT(blobID < blobFieldCount);
    
    if (blobFieldActualSizes[blobID] < newSize)
    {
      blobFieldActualSizes[blobID] = newSize;
    }
  }

  
  
  const char* getDB2LibName(NameFormatFlags format = EBCDIC_NATIVE)
  {
    switch (format)
    {
      case EBCDIC_NATIVE:
        return db2LibNameEbcdic; break;
      case ASCII_NATIVE:
        return db2LibNameAscii; break;
      case ASCII_SQL:
        return db2LibNameSQLAscii; break;
      default:
        DBUG_ASSERT(0);
    }
    return NULL;
  }
  
  const char* getDB2TableName(NameFormatFlags format = EBCDIC_NATIVE) const
  {
    switch (format)
    {
      case EBCDIC_NATIVE:
        return db2TableNameEbcdic; break;
      case ASCII_NATIVE:
        return db2TableNameAscii; break;
      case ASCII_SQL:
        return db2TableNameAscii; break;
        break;
      default:
        DBUG_ASSERT(0);
    }
    return NULL;
  }
  
  DB2Field& db2Field(int fieldID) const { return db2Fields[fieldID]; }
  DB2Field& db2Field(const Field* field) const { return db2Field(field->field_index); }

  void processFormatSpace();
  
  void* getFormatSpace(size_t& spaceNeeded)
  {
    DBUG_ASSERT(formatSpace == NULL);
    spaceNeeded = sizeof(format_hdr_t) + mysqlTable->fields * sizeof(DB2Field);
    formatSpace.alloc(spaceNeeded);
    return (void*)formatSpace;
  }  
  
  bool isTemporary() const
  {
    return isTemporaryTable;
  }
  
  void getDB2QualifiedName(char* to);
  static void getDB2LibNameFromPath(const char* path, char* lib, NameFormatFlags format=ASCII_SQL);
  static void getDB2FileNameFromPath(const char* path, char* file, NameFormatFlags format=ASCII_SQL);
  static void getDB2QualifiedNameFromPath(const char* path, char* to);
  static int32 appendQualifiedIndexFileName(const char* indexName, 
                                            const char* tableName, 
                                            String& to, 
                                            NameFormatFlags format=ASCII_SQL,
                                            enum_DB2I_INDEX_TYPE type=typeDefault);
  
  uint16 getBlobIdFromField(uint16 fieldID) const
  {
    for (int i = 0; i < blobFieldCount; ++i)
    {
      if (blobFields[i] == fieldID)
        return i;
    }
    DBUG_ASSERT(0);
    return 0;
  }
    
  iconv_t& getConversionDefinition(enum_conversionDirection direction,
                                   uint16 fieldID)
  {
    if (conversionDefinitions[direction][fieldID] == (iconv_t)(-1))
      findConversionDefinition(direction, fieldID);
    
    return conversionDefinitions[direction][fieldID];
  }
  
  const db2i_file* dataFile() const
  {
    return physicalFile;
  }
  
  const db2i_file* indexFile(uint idx) const
  {    
    return logicalFiles[idx];
  }
  
  const char* getFileLevelID() const
  {
    return fileLevelID;
  }

  static void deleteAssocFiles(const char* name);
  static void renameAssocFiles(const char* from, const char* to);

  int fastInitForCreate(const char* path);
  int initDiscoveredTable(const char* path);
      
  uint16* blobFields;
 
private: 

  void findConversionDefinition(enum_conversionDirection direction, uint16 fieldID);
  static void filenameToTablename(const char* in, char* out, size_t outlen);  
  static size_t smartFilenameToTableName(const char *in, char* out, size_t outlen);
  void convertNativeToSQLName(const char* input, 
                              char* output) 
  {
    
    output[0] = input[0];
    
    uint o = 1;
    uint i = 1;
    do
    {
      output[o++] = input[i];
      if (input[i] == '"' && input[i+1])
        output[o++] = '"';
    } while (input[++i]);

    output[o] = 0; // This isn't the most user-friendly way to handle overflows,
                                    // but at least its safe.
  }

  bool doFileIDsMatch(const char* path);
    
  ValidatedPointer<format_hdr_t> formatSpace;
  DB2Field* db2Fields;
  uint64 db2StartId;          // Starting value for identity column
  uint16 blobFieldCount; // Count of LOB fields in the DB2 table
  uint* blobFieldActualSizes; // Array of LOB field lengths (actual vs. allocated).
                              // This is updated as LOBs are read and will contain
                              // the length of the longest known LOB in that field.
  iconv_t* conversionDefinitions[2];
  
  const TABLE_SHARE* mysqlTable;
  uint16 logicalFileCount;
  char* db2LibNameEbcdic; // Quoted and in EBCDIC
  char* db2LibNameAscii;
  char* db2TableNameEbcdic;
  char* db2TableNameAscii;
  char* db2TableNameSQLAscii;
  char* db2LibNameSQLAscii;
          
  db2i_file* physicalFile;
  db2i_file** logicalFiles;
  
  bool isTemporaryTable;
  char fileLevelID[13];
};

/**
  @class db2i_file

  @details  This class describes a file object underlaying a particular SQL
  table. Both "physical files" (data) and "logical files" (indices) are
  described by this class. Only one instance of the class exists per DB2 file
  object. The single instance is responsible for de/allocating the multiple
  handles used by the handlers.
*/
class db2i_file
{

public: 
  struct RowFormat
  {
    uint16 readRowLen;
    uint16 readRowNullOffset;
    uint16 writeRowLen;
    uint16 writeRowNullOffset;
    char inited;
  };
  
public:

  // Construct an instance for a physical file.
  db2i_file(db2i_table* table);
    
  // Construct an instance for a logical file.
  db2i_file(db2i_table* table, int index);
   
  ~db2i_file()
  {
    if (masterDefn)
      db2i_ileBridge::getBridgeForThread()->deallocateFile(masterDefn);
    
    if (db2FileName != (char*)db2Table->getDB2TableName(db2i_table::EBCDIC_NATIVE))
      my_free(db2FileName, MYF(0));    
  }

  // This is roughly equivalent to an "open". It tells ILE to allocate a descriptor
  // for the file. The associated handle is returned to the caller.
  int allocateNewInstance(FILE_HANDLE* newHandle, ILEMemHandle inuseSpace) const
  {
    int rc;
    
    rc = db2i_ileBridge::getBridgeForThread()->allocateFileInstance(masterDefn,
                                                                    inuseSpace,
                                                                    newHandle);
    
    if (rc) *newHandle = 0;
       
    return rc;
  }
  
  // This obtains the row layout associated with a particular access intent for
  // an open instance of the file.
  int obtainRowFormat(FILE_HANDLE instanceHandle, 
                       char intent,
                       char commitLevel,
                       const RowFormat** activeFormat) const
  {
    DBUG_ENTER("db2i_file::obtainRowFormat");    
    RowFormat* rowFormat;
        
    if (intent == QMY_UPDATABLE)
      rowFormat = &(formats[readWrite]);
    else if (intent == QMY_READ_ONLY)
      rowFormat = &(formats[readOnly]);
        
    if (unlikely(!rowFormat->inited))
    {
      int rc = db2i_ileBridge::getBridgeForThread()->
                                 initFileForIO(instanceHandle,
                                               intent,
                                               commitLevel,
                                               &(rowFormat->writeRowLen),
                                               &(rowFormat->writeRowNullOffset),
                                               &(rowFormat->readRowLen),
                                               &(rowFormat->readRowNullOffset));
      if (rc) DBUG_RETURN(rc);
      rowFormat->inited = 1;
    }

    *activeFormat = rowFormat;
    DBUG_RETURN(0);
  }  
    
  const char* getDB2FileName() const
  {
    return db2FileName; 
  }
  
  void fillILEDefn(ShrDef* defn, bool readInArrivalSeq);

  void setMasterDefnHandle(FILE_HANDLE handle)
  {
    masterDefn = handle;
  }
  
  FILE_HANDLE getMasterDefnHandle() const 
  {
    return masterDefn;
  }
  
private:  
  enum RowFormats
  {
    readOnly = 0,
    readWrite,
    maxRowFormats
  };
    
  mutable RowFormat formats[maxRowFormats];
  
  void commonCtorInit();
  
  char* db2FileName; // Quoted and in EBCDIC

  db2i_table* db2Table;  // The logical SQL table contained by this file.
  
  bool db2CanSort;
  
  FILE_HANDLE masterDefn;
};


#endif

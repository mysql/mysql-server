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



#include "db2i_file.h"
#include "db2i_charsetSupport.h"
#include "db2i_collationSupport.h"
#include "db2i_misc.h"
#include "db2i_errors.h"
#include "my_dir.h"

db2i_table::db2i_table(const TABLE_SHARE* myTable, const char* path) : 
    mysqlTable(myTable),
    db2StartId(0),
    blobFieldCount(0),
    blobFields(NULL),
    blobFieldActualSizes(NULL),
    logicalFiles(NULL),
    physicalFile(NULL),
    db2TableNameSQLAscii(NULL),
    db2LibNameSQLAscii(NULL)
{
  char asciiLibName[MAX_DB2_SCHEMANAME_LENGTH + 1];
  getDB2LibNameFromPath(path, asciiLibName, ASCII_NATIVE);
  
  char asciiFileName[MAX_DB2_FILENAME_LENGTH + 1];
  getDB2FileNameFromPath(path, asciiFileName, ASCII_NATIVE);
  
  size_t libNameLen = strlen(asciiLibName);
  size_t fileNameLen = strlen(asciiFileName);
  
  db2LibNameEbcdic=(char *)
          my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                          &db2LibNameEbcdic, libNameLen+1,
                          &db2LibNameAscii, libNameLen+1,
                          &db2LibNameSQLAscii, libNameLen*2 + 1,
                          &db2TableNameEbcdic, fileNameLen+1,
                          &db2TableNameAscii, fileNameLen+1,
                          &db2TableNameSQLAscii, fileNameLen*2 + 1,
                          NullS);  
  
  if (likely(db2LibNameEbcdic))
  {
    memcpy(db2LibNameAscii, asciiLibName, libNameLen);
    convertNativeToSQLName(db2LibNameAscii, db2LibNameSQLAscii);   
    convToEbcdic(db2LibNameAscii, db2LibNameEbcdic, libNameLen);
    memcpy(db2TableNameAscii, asciiFileName, fileNameLen);
    convertNativeToSQLName(db2TableNameAscii, db2TableNameSQLAscii);   
    convToEbcdic(db2TableNameAscii, db2TableNameEbcdic, fileNameLen);
  }
  
  conversionDefinitions[toMySQL] = NULL;
  conversionDefinitions[toDB2] = NULL;
  
  isTemporaryTable = (strstr(mysqlTable->path.str, mysql_tmpdir) == mysqlTable->path.str);
}


int32 db2i_table::initDB2Objects(const char* path)
{
  uint fileObjects = 1 + mysqlTable->keys;
  ValidatedPointer<ShrDef> fileDefnSpace(sizeof(ShrDef) * fileObjects);
  
  physicalFile = new db2i_file(this);
  physicalFile->fillILEDefn(&fileDefnSpace[0], true);

  logicalFileCount = mysqlTable->keys;
  if (logicalFileCount > 0)
  {
    logicalFiles = new db2i_file*[logicalFileCount];
    for (int k = 0; k < logicalFileCount; k++)
    {
      logicalFiles[k] = new db2i_file(this, k);
      logicalFiles[k]->fillILEDefn(&fileDefnSpace[k+1], false);
    }
  }
  
  ValidatedPointer<FILE_HANDLE> fileDefnHandles(sizeof(FILE_HANDLE) * fileObjects);
  size_t formatSpaceLen = sizeof(format_hdr_t) + mysqlTable->fields * sizeof(DB2Field);
  formatSpace.alloc(formatSpaceLen);

  int rc = db2i_ileBridge::getBridgeForThread()->
                             expectErrors(QMY_ERR_RTNFMT)->
                             allocateFileDefn(fileDefnSpace,
                                              fileDefnHandles,
                                              fileObjects,
                                              db2LibNameEbcdic,
                                              strlen(db2LibNameEbcdic),
                                              formatSpace,
                                              formatSpaceLen);
  
  if (rc)
  {
    // We have to handle a format space error as a special case of a FID
    // mismatch. We should only get the space error if columns have been added
    // to the DB2 table without MySQL's knowledge, which is effectively a 
    // FID problem.
    if (rc == QMY_ERR_RTNFMT)
    {
      rc = QMY_ERR_LVLID_MISMATCH;
      getErrTxt(rc);
    }
    return rc;
  }

  convFromEbcdic(((format_hdr_t*)formatSpace)->FilLvlId, fileLevelID, sizeof(fileLevelID));

  if (!doFileIDsMatch(path))
  {
    getErrTxt(QMY_ERR_LVLID_MISMATCH);
    return QMY_ERR_LVLID_MISMATCH;
  }
  
  physicalFile->setMasterDefnHandle(fileDefnHandles[0]);
  for (int k = 0; k < mysqlTable->keys; k++)
  {
    logicalFiles[k]->setMasterDefnHandle(fileDefnHandles[k+1]);
  }
  
  db2StartId = (uint64)(((format_hdr_t*)formatSpace)->StartIdVal); 
  db2Fields = (DB2Field*)((char*)(void*)formatSpace + ((format_hdr_t*)formatSpace)->ColDefOff);

  uint fields = mysqlTable->fields;
  for (int i = 0; i < fields; ++i)
  {
    if (db2Field(i).isBlob())
    {
      blobFieldCount++;
    }
  }

  if (blobFieldCount)
  {
    blobFieldActualSizes = (uint*)my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                                                  &blobFieldActualSizes, blobFieldCount * sizeof(uint),
                                                  &blobFields, blobFieldCount * sizeof(uint16),
                                                  NullS);

    int b = 0;
    for (int i = 0; i < fields; ++i)
    {
      if (db2Field(i).isBlob())
      {
        blobFields[b++] = i;
      }
    }
  } 

  my_multi_malloc(MYF(MY_WME),
                  &conversionDefinitions[toMySQL], fields * sizeof(iconv_t),
                  &conversionDefinitions[toDB2], fields * sizeof(iconv_t),
                  NullS);
  for (int i = 0; i < fields; ++i)
  {
    conversionDefinitions[toMySQL][i] = (iconv_t)(-1);
    conversionDefinitions[toDB2][i] = (iconv_t)(-1);
  }
    
  return 0; 
}

int db2i_table::fastInitForCreate(const char* path)
{
  ValidatedPointer<ShrDef> fileDefnSpace(sizeof(ShrDef));
  
  physicalFile = new db2i_file(this);
  physicalFile->fillILEDefn(fileDefnSpace, true);
  
  ValidatedPointer<FILE_HANDLE> fileDefnHandles(sizeof(FILE_HANDLE));
  
  size_t formatSpaceLen = sizeof(format_hdr_t) + 
                          mysqlTable->fields * sizeof(DB2Field);
  formatSpace.alloc(formatSpaceLen);
      
  int rc = db2i_ileBridge::getBridgeForThread()->allocateFileDefn(fileDefnSpace,
                                                       fileDefnHandles,
                                                       1,
                                                       db2LibNameEbcdic,
                                                       strlen(db2LibNameEbcdic),
                                                       formatSpace,
                                                       formatSpaceLen); 
  
  if (rc)
    return rc;
  
  convFromEbcdic(((format_hdr_t*)formatSpace)->FilLvlId, fileLevelID, sizeof(fileLevelID));
  doFileIDsMatch(path);
  
  return 0;
}

bool db2i_table::doFileIDsMatch(const char* path)
{
  char name_buff[FN_REFLEN];
  
  fn_format(name_buff, path, "", FID_EXT, (MY_REPLACE_EXT | MY_UNPACK_FILENAME));

  File fd = my_open(name_buff, O_RDONLY, MYF(0));

  if (fd == -1)
  {
    if (errno == ENOENT)
    {
      fd = my_create(name_buff, 0, O_WRONLY, MYF(MY_WME));

      if (fd == -1)
      {
        // TODO: Report errno here
        return false;  
      }
      my_write(fd, (uchar*)fileLevelID, sizeof(fileLevelID), MYF(MY_WME));
      my_close(fd, MYF(0));
      return true;
    }
    else
    {
      // TODO: Report errno here
      return false;
    }
  }

  char diskFID[sizeof(fileLevelID)];

  bool match = false;
  
  if (my_read(fd, (uchar*)diskFID, sizeof(diskFID), MYF(MY_WME)) == sizeof(diskFID) &&
      (memcmp(diskFID, fileLevelID, sizeof(diskFID)) == 0))
    match = true;

  my_close(fd, MYF(0));
  
  return match;
}

void db2i_table::deleteAssocFiles(const char* name)
{
  char name_buff[FN_REFLEN];
  fn_format(name_buff, name, "", FID_EXT, (MY_REPLACE_EXT | MY_UNPACK_FILENAME));
  my_delete(name_buff, MYF(0));
}

void db2i_table::renameAssocFiles(const char* from, const char* to)
{
  rename_file_ext(from, to, FID_EXT);
}


db2i_table::~db2i_table()
{
  if (blobFieldActualSizes)
    my_free(blobFieldActualSizes, MYF(0));

  if (conversionDefinitions[toMySQL])
    my_free(conversionDefinitions[toMySQL], MYF(0));
      
  if (logicalFiles)
  {      
    for (int k = 0; k < logicalFileCount; ++k)
    {
      delete logicalFiles[k];
    }

    delete[] logicalFiles;
  }
  delete physicalFile;
  
  my_free(db2LibNameEbcdic, 0);  
}

void db2i_table::getDB2QualifiedName(char* to)
{
  strcat(to, getDB2LibName(ASCII_SQL));
  strcat(to, ".");
  strcat(to, getDB2TableName(ASCII_SQL));
}


void db2i_table::getDB2QualifiedNameFromPath(const char* path, char* to)
{ 
  getDB2LibNameFromPath(path, to);
  strcat(to, ".");
  getDB2FileNameFromPath(path, strend(to));
}


size_t db2i_table::smartFilenameToTableName(const char *in, char* out, size_t outlen)
{
  if (strchr(in, '@') == NULL)
  {
    return filename_to_tablename(in, out, outlen);
  }
  
  char* test = (char*) my_malloc(outlen, MYF(MY_WME));
  
  filename_to_tablename(in, test, outlen);

  char* cur = test;
  
  while (*cur)
  {
    if ((*cur <= 0x20) || (*cur >= 0x80))
    {
      strncpy(out, in, outlen);
      my_free(test, MYF(0));
      return min(outlen, strlen(out));
    }
    ++cur;
  }

  strncpy(out, test, outlen);
  my_free(test, MYF(0));
  return min(outlen, strlen(out));
}

void db2i_table::filenameToTablename(const char* in, char* out, size_t outlen)
{
  if (strchr(in, '#') == NULL)
  {
    smartFilenameToTableName(in, out, outlen);
    return;
  }
  
  char* temp = (char*)sql_alloc(outlen);
  
  const char* part1, *part2, *part3, *part4;
  part1 = in;
  part2 = strstr(part1, "#P#");
  if (part2);
  {
    part3 = part2 + 3;
    part4 = strchr(part3, '#');
    if (!part4)
      part4 = strend(in);
  }
  
  memcpy(temp, part1, min(outlen, part2 - part1));
  temp[min(outlen-1, part2-part1)] = 0;
    
  int32 accumLen = smartFilenameToTableName(temp, out, outlen);
  
  if (part2 && (accumLen + 4 < outlen))
  {
    strcat(out, "#P#");
    accumLen += 4;
    
    memset(temp, 0, min(outlen, part2-part1));
    memcpy(temp, part3, min(outlen, part4-part3));
    temp[min(outlen-1, part4-part3)] = 0;

    accumLen += smartFilenameToTableName(temp, strend(out), outlen-accumLen);
    
    if (part4 && (accumLen + (strend(in) - part4 + 1) < outlen))
    {
      strcat(out, part4);
    }
  }
}

void db2i_table::getDB2LibNameFromPath(const char* path, char* lib, NameFormatFlags format)
{
  if (strstr(path, mysql_tmpdir) == path)
  {
    strcpy(lib, DB2I_TEMP_TABLE_SCHEMA);
  }
  else
  {  
    const char* c = strend(path) - 1;
    while (c > path && *c != '\\' && *c != '/')
      --c;

    if (c != path)
    {
      const char* dbEnd = c;
      do {
        --c;
      } while (c >= path && *c != '\\' && *c != '/');

      if (c >= path)
      {
        const char* dbStart = c+1;
        char fileName[FN_REFLEN];
        memcpy(fileName, dbStart, dbEnd - dbStart);
        fileName[dbEnd-dbStart] = 0;
        
        char dbName[MAX_DB2_SCHEMANAME_LENGTH+1];
        filenameToTablename(fileName, dbName , sizeof(dbName));
        
        convertMySQLNameToDB2Name(dbName, lib, sizeof(dbName), true, (format==ASCII_SQL) );
      }
      else
        DBUG_ASSERT(0); // This should never happen!
    }
  }
}

void db2i_table::getDB2FileNameFromPath(const char* path, char* file, NameFormatFlags format)
{
  const char* fileEnd = strend(path);
  const char* c = fileEnd;
  while (c > path && *c != '\\' && *c != '/')
    --c;

  if (c != path)
  {
    const char* fileStart = c+1;
    char fileName[FN_REFLEN];
    memcpy(fileName, fileStart, fileEnd - fileStart);
    fileName[fileEnd - fileStart] = 0;
    char db2Name[MAX_DB2_FILENAME_LENGTH+1];
    filenameToTablename(fileName, db2Name, sizeof(db2Name));
    convertMySQLNameToDB2Name(db2Name, file, sizeof(db2Name), true, (format==ASCII_SQL) );
  }
}

// Generates the DB2 index name when given the MySQL index and table names.
int32 db2i_table::appendQualifiedIndexFileName(const char* indexName, 
                                               const char* tableName,
                                               String& to,
                                               NameFormatFlags format,
                                               enum_DB2I_INDEX_TYPE type)
{
  char generatedName[MAX_DB2_FILENAME_LENGTH+1];
  strncpy(generatedName, indexName, DB2I_INDEX_NAME_LENGTH_TO_PRESERVE);
  generatedName[DB2I_INDEX_NAME_LENGTH_TO_PRESERVE] = 0;
  char* endOfGeneratedName;
  
  if (type == typeDefault)
  {
    strcat(generatedName, DB2I_DEFAULT_INDEX_NAME_DELIMITER);
    endOfGeneratedName = strend(generatedName);
  }
  else if (type != typeNone)
  {
    strcat(generatedName, DB2I_ADDL_INDEX_NAME_DELIMITER);
    endOfGeneratedName = strend(generatedName);
    *(endOfGeneratedName-2) = char(type);
  }

  uint lenWithoutFile = endOfGeneratedName - generatedName;
  
  char strippedTableName[MAX_DB2_FILENAME_LENGTH+1];
  if (format == ASCII_SQL)
  {
    strcpy(strippedTableName, tableName);
    stripExtraQuotes(strippedTableName+1, sizeof(strippedTableName));
    tableName = strippedTableName;
  }

  if (strlen(tableName) > (MAX_DB2_FILENAME_LENGTH-lenWithoutFile))
    return -1;
  
  strncat(generatedName, 
          tableName+1,
          min(strlen(tableName), (MAX_DB2_FILENAME_LENGTH-lenWithoutFile))-2 );

  char finalName[MAX_DB2_FILENAME_LENGTH+1];
  convertMySQLNameToDB2Name(generatedName, finalName, sizeof(finalName), true, (format==ASCII_SQL));
  to.append(finalName);
  
  return 0;
}


void db2i_table::findConversionDefinition(enum_conversionDirection direction, uint16 fieldID)
{
  getConversion(direction, 
                mysqlTable->field[fieldID]->charset(), 
                db2Field(fieldID).getCCSID(), 
                conversionDefinitions[direction][fieldID]);
}


db2i_file::db2i_file(db2i_table* table) : db2Table(table)
{
  commonCtorInit();

  DBUG_ASSERT(table->getMySQLTable()->table_name.length <= MAX_DB2_FILENAME_LENGTH-2); 
  
  db2FileName = (char*)table->getDB2TableName(db2i_table::EBCDIC_NATIVE);
}  

db2i_file::db2i_file(db2i_table* table, int index) : db2Table(table)
{
  commonCtorInit();

  if ((index == table->getMySQLTable()->primary_key) && !table->isTemporary())
  {
    db2FileName = (char*)table->getDB2TableName(db2i_table::EBCDIC_NATIVE);  
  }
  else
  {
    // Generate the index name (in index___table form); quote and EBCDICize it.
    String qualifiedPath;
    qualifiedPath.length(0);

    const char* asciiFileName = table->getDB2TableName(db2i_table::ASCII_NATIVE);

    db2i_table::appendQualifiedIndexFileName(table->getMySQLTable()->key_info[index].name,
                                           asciiFileName,
                                           qualifiedPath,
                                           db2i_table::ASCII_NATIVE,
                                           typeDefault);

    db2FileName = (char*)my_malloc(qualifiedPath.length()+1, MYF(MY_WME | MY_ZEROFILL));
    convToEbcdic(qualifiedPath.ptr(), db2FileName, qualifiedPath.length());
  }  
} 

void db2i_file::commonCtorInit()
{
  masterDefn = 0;
  memset(&formats, 0, maxRowFormats*sizeof(RowFormat));
}


void db2i_file::fillILEDefn(ShrDef* defn, bool readInArrivalSeq)
{
  defn->ObjNamLen = strlen(db2FileName);
  DBUG_ASSERT(defn->ObjNamLen <= sizeof(defn->ObjNam));
  memcpy(defn->ObjNam, db2FileName, defn->ObjNamLen);
  defn->ArrSeq[0] = (readInArrivalSeq ? QMY_YES : QMY_NO);
}


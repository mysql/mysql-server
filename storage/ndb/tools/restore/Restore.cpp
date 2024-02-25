/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <algorithm>

#include "Restore.hpp"
#include <NdbTCP.h>
#include <OutputStream.hpp>
#include <Bitmask.hpp>

#include <AttributeHeader.hpp>
#include <trigger_definitions.h>
#include <SimpleProperties.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <ndb_limits.h>
#include <NdbAutoPtr.hpp>
#include "../src/kernel/blocks/backup/BackupFormat.hpp"
#include "../src/ndbapi/NdbDictionaryImpl.hpp"

#include "restore_tables.h"
#include <NdbThread.h>
#include "../src/kernel/vm/Emulator.hpp"
#include "kernel/signaldata/FsOpenReq.hpp"
#include "portlib/ndb_file.h"
#include "portlib/NdbMem.h"
#include "util/ndb_opts.h"

using byte = unsigned char;

extern thread_local EmulatedJamBuffer* NDB_THREAD_TLS_JAM;

extern NdbRecordPrintFormat g_ndbrecord_print_format;
extern bool ga_skip_unknown_objects;
extern bool ga_skip_broken_objects;
extern bool opt_include_stored_grants;

extern ndb_password_state g_backup_password_state;

#define LOG_MSGLEN 1024

Uint16 Twiddle16(Uint16 in); // Byte shift 16-bit data
Uint32 Twiddle32(Uint32 in); // Byte shift 32-bit data
Uint64 Twiddle64(Uint64 in); // Byte shift 64-bit data


/*
  TwiddleUtil

  Utility class used when swapping byteorder
  of one attribute in a table

*/

class TwiddleUtil {
  Uint32 m_twiddle_size;
  Uint32 m_twiddle_array_size;
public:
  TwiddleUtil(); // Not implemented
  TwiddleUtil(const TwiddleUtil&); // Not implemented

  TwiddleUtil(const AttributeDesc * const attr_desc) {
    const NdbDictionary::Column::Type attribute_type =
      attr_desc->m_column->getType();

    switch(attribute_type){
    case NdbDictionary::Column::Datetime:
      // Datetime is stored as 8x8, should be twiddled as 64 bit
      assert(attr_desc->size == 8);
      assert(attr_desc->arraySize == 8);
      m_twiddle_size = 64;
      m_twiddle_array_size = 1;
      break;

    case NdbDictionary::Column::Timestamp:
      // Timestamp is stored as 4x8, should be twiddled as 32 bit
      assert(attr_desc->size == 8);
      assert(attr_desc->arraySize == 4);
      m_twiddle_size = 32;
      m_twiddle_array_size = 1;
      break;

    case NdbDictionary::Column::Blob:
    case NdbDictionary::Column::Text:
      if (attr_desc->m_column->getArrayType() ==
          NdbDictionary::Column::ArrayTypeFixed)
      {
        // Length of fixed size blob which is stored in first 64 bit's
        // has to be twiddled, the remaining byte stream left as is
        assert(attr_desc->size == 8);
        assert(attr_desc->arraySize > 8);
        m_twiddle_size = 64;
        m_twiddle_array_size = 1;
        break;
      }
      // Fall through - for blob/text with ArrayTypeVar
      [[fallthrough]];
    default:
      // Default twiddling parameters
      m_twiddle_size = attr_desc->size;
      m_twiddle_array_size = attr_desc->arraySize;
      break;
    }

    assert(m_twiddle_array_size);
    assert(m_twiddle_size);
  }

  bool is_aligned (void* data_ptr) const {
    switch (m_twiddle_size){
    case 8:
      // Always aligned
      return true;
      break;
    case 16:
      return ((((size_t)data_ptr) & 1) == 0);
      break;
    case 32:
      return ((((size_t)data_ptr) & 3) == 0);
      break;
    case 64:
      return ((((size_t)data_ptr) & 7) == 0);
      break;
    default:
      abort();
      break;
    }
    return false; // Never reached
  }

  void twiddle_aligned(void* const data_ptr) const {
    // Make sure the data pointer is properly aligned
    assert(is_aligned(data_ptr));

    switch(m_twiddle_size){
    case 8:
      // Nothing to swap
      break;
    case 16:
    {
      Uint16* ptr = (Uint16*)data_ptr;
      for (Uint32 i = 0; i < m_twiddle_array_size; i++){
        *ptr = Twiddle16(*ptr);
        ptr++;
      }
      break;
    }
    case 32:
    {
      Uint32* ptr = (Uint32*)data_ptr;
      for (Uint32 i = 0; i < m_twiddle_array_size; i++){
        *ptr = Twiddle32(*ptr);
        ptr++;
      }
      break;
    }
    case 64:
    {
      Uint64* ptr = (Uint64*)data_ptr;
      for (Uint32 i = 0; i < m_twiddle_array_size; i++){
        *ptr = Twiddle64(*ptr);
        ptr++;
      }
      break;
    }
    default:
      abort();
    } // switch
  }
};


/*
  BackupFile::twiddle_attribute

  Swap the byte order of one attribute whose data may or may not
  be properly aligned for the current datatype

*/

void
BackupFile::twiddle_atribute(const AttributeDesc * const attr_desc,
                             AttributeData* attr_data)
{
  TwiddleUtil map(attr_desc);

  // Check if data is aligned properly
  void* data_ptr = (char*)attr_data->void_value;
  Uint32 data_sz = attr_desc->getSizeInBytes();
  bool aligned= map.is_aligned(data_ptr);
  if (!aligned)
  {
    // The pointer is not properly aligned, copy the data
    // to aligned memory before twiddling
    m_twiddle_buffer.assign(data_ptr, data_sz);
    data_ptr = m_twiddle_buffer.get_data();
  }

  // Swap the byteorder of the aligned data
  map.twiddle_aligned(data_ptr);

  if (!aligned)
  {
    // Copy data back from aligned memory
    memcpy(attr_data->void_value,
           m_twiddle_buffer.get_data(),
           data_sz);
  }
}


/*
  BackupFile::Twiddle

  Swap the byteorder for one attribute if it was stored
  in different byteorder than current host

*/

bool
BackupFile::Twiddle(const AttributeDesc * const attr_desc,
                    AttributeData* attr_data)
{
  // Check parameters are not NULL
  assert(attr_desc);
  assert(attr_data);

  // Make sure there is data to fiddle with
  assert(!attr_data->null);
  assert(attr_data->void_value);

  if(unlikely(!m_hostByteOrder))
  {
    // The data file is not in host byte order, the
    // attribute need byte order swapped
    twiddle_atribute(attr_desc, attr_data);
  }
#ifdef VM_TRACE
  else
  {
    // Increase test converage in debug mode by doing
    // a double byte order swap to prove that both ways work
    twiddle_atribute(attr_desc, attr_data);
    twiddle_atribute(attr_desc, attr_data);
  }
#endif

  return true;
}


FilteredNdbOut err(* new FileOutputStream(stderr), 0, 0);
FilteredNdbOut info(* new FileOutputStream(stdout), 1, 1);
FilteredNdbOut debug(* new FileOutputStream(stdout), 2, 0);
RestoreLogger restoreLogger;

// To decide in what byte order data is
const Uint32 magicByteOrder = 0x12345678;
const Uint32 swappedMagicByteOrder = 0x78563412;

RestoreMetaData::RestoreMetaData(const char* path, Uint32 nodeId,
                                 Uint32 bNo, Uint32 partId, Uint32 partCount)
{
  debug << "RestoreMetaData constructor" << endl;
  m_part_id = partId;
  m_part_count = partCount;
  setCtlFile(nodeId, bNo, path);
}

RestoreMetaData::~RestoreMetaData(){
  for(Uint32 i= 0; i < allTables.size(); i++)
  {
    TableS *table = allTables[i];
    for(Uint32 j= 0; j < table->m_fragmentInfo.size(); j++)
      delete table->m_fragmentInfo[j];
    delete table;
  }

  for (Uint32 i = 0; i < m_objects.size(); i++)
  {
    switch (m_objects[i].m_objType)
    {
    case DictTabInfo::Tablespace:
    {
      NdbDictionary::Tablespace * dst =
        (NdbDictionary::Tablespace *)m_objects[i].m_objPtr;
      delete dst;
      break;
    }
    case DictTabInfo::LogfileGroup:
    {
      NdbDictionary::LogfileGroup * dst =
        (NdbDictionary::LogfileGroup *)m_objects[i].m_objPtr;
      delete dst;
      break;
    }
    case DictTabInfo::Datafile:
    {
      NdbDictionary::Datafile * dst =
        (NdbDictionary::Datafile *)m_objects[i].m_objPtr;
      delete dst;
      break;
    }
    case DictTabInfo::Undofile:
    {
      NdbDictionary::Undofile * dst =
        (NdbDictionary::Undofile *)m_objects[i].m_objPtr;
      delete dst;
      break;
    }
    case DictTabInfo::HashMap:
    {
      NdbDictionary::HashMap * dst =
        (NdbDictionary::HashMap *)m_objects[i].m_objPtr;
      delete dst;
      break;
    }
    case DictTabInfo::ForeignKey:
    {
      NdbDictionary::ForeignKey * dst =
        (NdbDictionary::ForeignKey *)m_objects[i].m_objPtr;
      delete dst;
      break;
    }
    default:
      err << "Unsupported table type!! " << endl;
      assert(false);
      break;
    }
  }
  m_objects.clear();
}

TableS * 
RestoreMetaData::getTable(Uint32 tableId) const {
  for(Uint32 i= 0; i < allTables.size(); i++)
    if(allTables[i]->getTableId() == tableId)
      return allTables[i];
  return NULL;
}

Uint32
RestoreMetaData::getStartGCP() const {
  return m_startGCP;
}

Uint32
RestoreMetaData::getStopGCP() const {
  return m_stopGCP;
}

int
RestoreMetaData::loadContent() 
{
  Uint32 noOfTables = readMetaTableList();
  if(noOfTables == 0) {
    return 1;
  }
  for(Uint32 i = 0; i<noOfTables; i++){
    if(!readMetaTableDesc()){
      return 0;
    }
  }
  if (!markSysTables())
    return 0;
  if (!fixBlobs())
    return 0;
  if(!readGCPEntry())
    return 0;

  if(!readFragmentInfo())
    return 0;
  return 1;
}

Uint32
RestoreMetaData::readMetaTableList() {
  
  Uint32 sectionInfo[2];
  
  int r = buffer_read(&sectionInfo, sizeof(sectionInfo), 1);
  if (r != 1)
  {
    restoreLogger.log_error("readMetaTableList read header error");
    return 0;
  }
  sectionInfo[0] = ntohl(sectionInfo[0]);
  sectionInfo[1] = ntohl(sectionInfo[1]);

  const Uint32 tabCount = sectionInfo[1] - 2;

  void *tmp;
  Uint32 tabsRead = 0;
  while (tabsRead < tabCount){
    int count = buffer_get_ptr(&tmp, 4, tabCount - tabsRead);
    if (count < 0)
    {
      if (!m_xfile.is_encrypted())
      {
        restoreLogger.log_error("readMetaTableList read tabCount error, corrupt file?");
      }
      else
      {
        restoreLogger.log_error("readMetaTableList read tabCount error, wrong password or corrupt file?");
      }
      return 0;
    }
    if(count == 0)
      break;
    tabsRead += count;
  }
  if (tabsRead != tabCount){
    restoreLogger.log_error("readMetaTableList read tabCount error, "
            "expected count = %u, actual count = %u", tabCount, tabsRead);
    return 0;
  }
#ifdef ERROR_INSERT
  if(m_error_insert == NDB_RESTORE_ERROR_INSERT_SMALL_BUFFER)
  {
    // clear error insert
    m_error_insert = 0;
    m_buffer_sz = BUFFER_SIZE;
  }
#endif  
  return tabCount;
}

bool
RestoreMetaData::readMetaTableDesc() {
  
  Uint32 sectionInfo[3];
  
  // Read section header 
  Uint32 sz = sizeof(sectionInfo) >> 2;
  if (m_fileHeader.NdbVersion < NDBD_ROWID_VERSION ||
      ndbd_drop6(m_fileHeader.NdbVersion))
  {
    sz = 2;
    sectionInfo[2] = htonl(DictTabInfo::UserTable);
  }
  int r = buffer_read(&sectionInfo, 4*sz, 1);
  if (r != 1)
  {
    restoreLogger.log_error("readMetaTableDesc read header error");
    return false;
  } // if
  sectionInfo[0] = ntohl(sectionInfo[0]);
  sectionInfo[1] = ntohl(sectionInfo[1]);
  sectionInfo[2] = ntohl(sectionInfo[2]);
  
  assert(sectionInfo[0] == BackupFormat::TABLE_DESCRIPTION);
  
  // Read dictTabInfo buffer
  const Uint32 len = (sectionInfo[1] - sz);
  void *ptr;
  r = buffer_get_ptr(&ptr, 4, len);
  if (r < 0 || Uint32(r) != len)
  {
    restoreLogger.log_error("readMetaTableDesc read error");
    return false;
  } // if
  
  int errcode = 0;
  DictObject obj = { sectionInfo[2], 0 };
  switch(obj.m_objType){
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::OrderedIndex:
    return parseTableDescriptor((Uint32*)ptr, len);	     
    break;
  case DictTabInfo::Tablespace:
  {
    NdbDictionary::Tablespace * dst = new NdbDictionary::Tablespace;
    errcode = 
      NdbDictInterface::parseFilegroupInfo(NdbTablespaceImpl::getImpl(* dst), 
					   (Uint32*)ptr, len);
    if (errcode)
      delete dst;
    obj.m_objPtr = dst;
    restoreLogger.log_debug("%p %u %s", obj.m_objPtr, dst->getObjectId(), dst->getName());
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    NdbDictionary::LogfileGroup * dst = new NdbDictionary::LogfileGroup;
    errcode = 
      NdbDictInterface::parseFilegroupInfo(NdbLogfileGroupImpl::getImpl(* dst),
					   (Uint32*)ptr, len);
    if (errcode)
      delete dst;
    obj.m_objPtr = dst;
    restoreLogger.log_debug("%p %u %s", obj.m_objPtr, dst->getObjectId(), dst->getName());
    break;
  }
  case DictTabInfo::Datafile:
  {
    NdbDictionary::Datafile * dst = new NdbDictionary::Datafile;
    errcode = 
      NdbDictInterface::parseFileInfo(NdbDatafileImpl::getImpl(* dst), 
				      (Uint32*)ptr, len);
    if (errcode)
      delete dst;
    obj.m_objPtr = dst;
    restoreLogger.log_debug("%p %u %s", obj.m_objPtr, dst->getObjectId(), dst->getPath());
    break;
  }
  case DictTabInfo::Undofile:
  {
    NdbDictionary::Undofile * dst = new NdbDictionary::Undofile;
    errcode = 
      NdbDictInterface::parseFileInfo(NdbUndofileImpl::getImpl(* dst), 
				      (Uint32*)ptr, len);
    if (errcode)
      delete dst;
    obj.m_objPtr = dst;
    restoreLogger.log_debug("%p %u %s", obj.m_objPtr, dst->getObjectId(), dst->getPath());
    break;
  }
  case DictTabInfo::HashMap:
  {
    NdbDictionary::HashMap * dst = new NdbDictionary::HashMap;
    errcode =
      NdbDictInterface::parseHashMapInfo(NdbHashMapImpl::getImpl(* dst),
                                         (Uint32*)ptr, len);
    if (errcode)
      delete dst;
    obj.m_objPtr = dst;

    if (!m_hostByteOrder)
    {
      /**
       * Bloody byte-array, need to twiddle
       */
      Vector<Uint32> values;
      Uint32 len = dst->getMapLen();
      Uint32 zero = 0;
      values.fill(len, zero);
      dst->getMapValues(values.getBase(), values.size());
      for (Uint32 i = 0; i<len; i++)
      {
        values[i] = Twiddle16(values[i]);
      }
      dst->setMap(values.getBase(), values.size());
    }

    m_objects.push(obj, 0); // Put first
    return true;
    break;
  }
  case DictTabInfo::ForeignKey:
  {
    NdbDictionary::ForeignKey * dst = new NdbDictionary::ForeignKey;
    errcode =
      NdbDictInterface::parseForeignKeyInfo(NdbForeignKeyImpl::getImpl(* dst),
                                            (const Uint32*)ptr, len);
    if (errcode)
      delete dst;
    obj.m_objPtr = dst;
    restoreLogger.log_debug("%p %u %s", obj.m_objPtr, dst->getObjectId(), dst->getName());
    break;
  }
  default:
    if (ga_skip_unknown_objects)
    {
      restoreLogger.log_info("Skipping schema object with unknown table type %u",
                              sectionInfo[2]);
      return true;
    }
    else
    {
      restoreLogger.log_error("Unsupported table type!! %u", sectionInfo[2]);
      return false;
    }
  }
  if (errcode)
  {
    restoreLogger.log_error("Unable to parse dict info...%u %u",
       sectionInfo[2], errcode);
    return false;
  }

  /**
   * DD objects need to be sorted...
   */
  for(Uint32 i = 0; i<m_objects.size(); i++)
  {
    switch(sectionInfo[2]){
    case DictTabInfo::Tablespace:
      if (DictTabInfo::isFile(m_objects[i].m_objType))
      {
	m_objects.push(obj, i);
	goto end;
      }
      break;
    case DictTabInfo::LogfileGroup:
    {
      if (DictTabInfo::isFile(m_objects[i].m_objType) ||
	  m_objects[i].m_objType == DictTabInfo::Tablespace)
      {
	m_objects.push(obj, i);
	goto end;
      }
      break;
    }
    default:
      m_objects.push_back(obj);
      goto end;
    }
  }
  m_objects.push_back(obj);
  
end:
  return true;
}

#define OLD_NDB_REP_DB  "cluster"
#define OLD_NDB_APPLY_TABLE "apply_status"
#define OLD_NDB_SCHEMA_TABLE "schema"

bool
RestoreMetaData::markSysTables()
{
  Uint32 i;
  for (i = 0; i < getNoOfTables(); i++) {
    TableS* table = allTables[i];
    table->m_local_id = i;
    const char* tableName = table->getTableName();
    if ( // XXX should use type
        strcmp(tableName, "SYSTAB_0") == 0 ||
        strcmp(tableName, "NDB$EVENTS_0") == 0 ||
        strcmp(tableName, "sys/def/SYSTAB_0") == 0 ||
        strcmp(tableName, "sys/def/NDB$EVENTS_0") == 0 ||
        // index stats tables and indexes
        strncmp(tableName, NDB_INDEX_STAT_PREFIX,
                sizeof(NDB_INDEX_STAT_PREFIX)-1) == 0 ||
        strstr(tableName, "/" NDB_INDEX_STAT_PREFIX) != 0 ||
        /*
          The following is for old MySQL versions,
           before we changed the database name of the tables from
           "cluster_replication" -> "cluster" -> "mysql"
        */
        strcmp(tableName, "cluster_replication/def/" OLD_NDB_APPLY_TABLE) == 0 ||
        strcmp(tableName, OLD_NDB_REP_DB "/def/" OLD_NDB_APPLY_TABLE) == 0 ||
        strcmp(tableName, OLD_NDB_REP_DB "/def/" OLD_NDB_SCHEMA_TABLE) == 0 ||
        strcmp(tableName, NDB_REP_DB "/def/" NDB_APPLY_TABLE) == 0 ||
        strcmp(tableName, NDB_REP_DB "/def/" NDB_SCHEMA_TABLE)== 0 ||
        strcmp(tableName, "mysql/def/ndb_schema_result") == 0 ||
        (strcmp(tableName, "mysql/def/ndb_sql_metadata") == 0
         && !opt_include_stored_grants)
       )
    {
      table->m_isSysTable = true;
      if (strcmp(tableName, "SYSTAB_0") == 0 ||
          strcmp(tableName, "sys/def/SYSTAB_0") == 0)
        table->m_isSYSTAB_0 = true;
    }
  }
  for (i = 0; i < getNoOfTables(); i++) {
    TableS* auxTable = allTables[i];
    const char* auxTableName = auxTable->getTableName();
    // Use pattern matching to find blob tables or ordered indexes and
    // associate them with their main tables
    static constexpr const char * indxPattern = "sys/def/%d/";
    static constexpr const char * blobPattern = "%[^/]/%[^/]/NDB$BLOB_%d_%d";
     int id1, id2 = ~(Uint32)0;
    char buf[256];

    if((sscanf(auxTableName, indxPattern, &id1) == 1) ||
       (sscanf(auxTableName, blobPattern, buf, buf, &id1, &id2) == 4)) {
      TableS *mainTable = getTable(id1);
      if(mainTable) {
        auxTable->m_isSysTable = mainTable->m_isSysTable;
        auxTable->m_main_table = mainTable;
        auxTable->m_main_column_id = id2;
      } else {
        restoreLogger.log_error("Restore: Bad primary table id in %s", auxTableName);
        return false;
      }
    }
  }
  return true;
}

bool
RestoreMetaData::fixBlobs()
{
  Uint32 i;
  for (i = 0; i < getNoOfTables(); i++) {
    TableS* table = allTables[i];
    assert(table->m_dictTable != NULL);
    NdbTableImpl& t = NdbTableImpl::getImpl(*table->m_dictTable);
    const Uint32 noOfBlobs = t.m_noOfBlobs;
    if (noOfBlobs == 0)
      continue;
    Uint32 n = 0;
    Uint32 j;
    for (j = 0; n < noOfBlobs; j++) {
      NdbColumnImpl* c = t.getColumn(j);
      assert(c != NULL);
      if (!c->getBlobType())
        continue;
      // tinyblobs are counted in noOfBlobs...
      n++;
      if (c->getPartSize() == 0)
        continue;
      Uint32 k;
      TableS* blobTable = NULL;
      for (k = 0; k < getNoOfTables(); k++) {
        TableS* tmp = allTables[k];
        if (tmp->m_main_table == table &&
            tmp->m_main_column_id == j) {
          blobTable = tmp;
          break;
        }
      }
      if (blobTable == NULL)
      {
        table->m_broken = true;
        /* Corrupt backup, has main table, but no blob table */
        restoreLogger.log_error("Table %s has blob column %u (%s)"
               " with missing parts table in backup.",
               table->m_dictTable->getName(), j, c->m_name.c_str());
        if (ga_skip_broken_objects)
        {
          continue;
        }
        else
        {
          return false;
        }
      }
      assert(blobTable->m_dictTable != NULL);
      assert(blobTable->m_blobTables.size() == 0);
      NdbTableImpl& bt = NdbTableImpl::getImpl(*blobTable->m_dictTable);
      const char* colName = c->m_blobVersion == 1 ? "DATA" : "NDB$DATA";
      const NdbColumnImpl* bc = bt.getColumn(colName);
      assert(bc != NULL);
      assert(c->m_storageType == NDB_STORAGETYPE_MEMORY);
      c->m_storageType = bc->m_storageType;

      table->m_blobTables.push_back(blobTable);
    }
  }
  return true;
}

bool
RestoreMetaData::readGCPEntry() {

  BackupFormat::CtlFile::GCPEntry dst;
  
  int r = buffer_read(&dst, 1, sizeof(dst));
  if(r < 0 || size_t(r) != sizeof(dst))
  {
    restoreLogger.log_error("readGCPEntry read error");
    return false;
  }
  
  dst.SectionType = ntohl(dst.SectionType);
  dst.SectionLength = ntohl(dst.SectionLength);
  
  if(dst.SectionType != BackupFormat::GCP_ENTRY){
    restoreLogger.log_error("readGCPEntry invalid format");
    return false;
  }
  
  dst.StartGCP = ntohl(dst.StartGCP);
  dst.StopGCP = ntohl(dst.StopGCP);
  
  m_startGCP = dst.StartGCP;
  /**
   * Stop GCP is recorded as StopGCP -1 by Backup.cpp
   * We correct this here
   * Backup format not changed
   */
  m_stopGCP = dst.StopGCP + 1;
  return true;
}

bool
RestoreMetaData::readFragmentInfo()
{
  BackupFormat::CtlFile::FragmentInfo fragInfo;
  TableS * table = 0;
  Uint32 tableId = RNIL;

  int r;
  while ((r = buffer_read(&fragInfo, 4, 2)) == 2)
  {
    fragInfo.SectionType = ntohl(fragInfo.SectionType);
    fragInfo.SectionLength = ntohl(fragInfo.SectionLength);

    if (fragInfo.SectionType != BackupFormat::FRAGMENT_INFO)
    {
      restoreLogger.log_error("readFragmentInfo invalid section type: %u",
        fragInfo.SectionType);
      return false;
    }

    int r = buffer_read(&fragInfo.TableId, (fragInfo.SectionLength-2)*4, 1);
    if (r != 1)
    {
      restoreLogger.log_error("readFragmentInfo invalid section length: %u",
        fragInfo.SectionLength);
      return false;
    }

    fragInfo.TableId = ntohl(fragInfo.TableId);
    if (fragInfo.TableId != tableId)
    {
      tableId = fragInfo.TableId;
      table = getTable(tableId);
    }

    FragmentInfo * tmp = new FragmentInfo;
    tmp->fragmentNo = ntohl(fragInfo.FragmentNo);
    tmp->noOfRecords = ntohl(fragInfo.NoOfRecordsLow) +
      (((Uint64)ntohl(fragInfo.NoOfRecordsHigh)) << 32);
    tmp->filePosLow = ntohl(fragInfo.FilePosLow);
    tmp->filePosHigh = ntohl(fragInfo.FilePosHigh);
    tmp->sliceSkip = false; /* Init, set later */

    table->m_fragmentInfo.push_back(tmp);
    table->m_noOfRecords += tmp->noOfRecords;
  }
  if (r < 0)
  {
    restoreLogger.log_error("readFragmentInfo invalid read");
    return false;
  }
  return true;
}

TableS::TableS(Uint32 version, NdbTableImpl* tableImpl)
  : m_dictTable(tableImpl)
{
  m_noOfNullable = m_nullBitmaskSize = 0;
  m_auto_val_attrib = 0;
  m_max_auto_val= 0;
  m_noOfRecords= 0;
  backupVersion = version;
  m_isSysTable = false;
  m_isSYSTAB_0 = false;
  m_broken = false;
  m_main_table = NULL;
  m_main_column_id = ~(Uint32)0;
  m_has_blobs = false;
  
  for (int i = 0; i < tableImpl->getNoOfColumns(); i++)
    createAttr(tableImpl->getColumn(i));

  m_staging = false;
  m_stagingTable = NULL;
  m_stagingFlags = 0;

  m_pk_extended = false;
  m_pk_index = NULL;
}

TableS::~TableS()
{
  for (Uint32 i= 0; i < allAttributesDesc.size(); i++)
  {
    if (allAttributesDesc[i]->parameter)
      free(allAttributesDesc[i]->parameter);
    delete allAttributesDesc[i];
  }
  delete m_stagingTable;
  delete m_dictTable;
}


// Parse dictTabInfo buffer and pushback to to vector storage 
bool
RestoreMetaData::parseTableDescriptor(const Uint32 * data, Uint32 len)
{
  NdbTableImpl* tableImpl = 0;
  int ret = NdbDictInterface::parseTableInfo
    (&tableImpl, data, len, false,
     ndbd_drop6(m_fileHeader.NdbVersion) ? MAKE_VERSION(5,1,2) :
     m_fileHeader.NdbVersion);
  
  if (ret != 0) {
    ndberror_struct err_struct;
    err_struct.code = ret;
    ndberror_update(&err_struct);

    restoreLogger.log_error("parseTableInfo failed with error %u \"%s\"",
        err_struct.code, err_struct.message);

    restoreLogger.log_error("Check version of backup and schema contained in backup.");
    return false;
  }
  if(tableImpl == 0)
    return false;

  restoreLogger.log_debug("parseTableInfo %s done", tableImpl->getName());
  TableS * table = new TableS(m_fileHeader.NdbVersion, tableImpl);
  if(table == NULL) {
    return false;
  }

  restoreLogger.log_debug("Parsed table id %u\nParsed table #attr %u\n"
        "Parsed table schema version not used",
        table->getTableId(),
        table->getNoOfAttributes());

  restoreLogger.log_debug("Pushing table %s\n    with %u attributes",
        table->getTableName(), table->getNoOfAttributes());
  
  allTables.push_back(table);

  return true;
}

// Constructor
RestoreDataIterator::RestoreDataIterator(const RestoreMetaData & md, void (* _free_data_callback)(void*), void *ctx)
  : BackupFile(_free_data_callback, ctx), m_metaData(md),
    m_current_table_has_transforms(false)
{
  restoreLogger.log_debug("RestoreDataIterator constructor");
  setDataFile(md, 0);

  alloc_extra_storage(8192);
  m_row_max_extra_wordlen = 0;
}


bool
RestoreDataIterator::validateRestoreDataIterator()
{
    if (!m_extra_storage_ptr)
    {
        restoreLogger.log_error("m_extra_storage_ptr is NULL");
        return false;
    }
    return true;
}


RestoreDataIterator::~RestoreDataIterator()
{
  free_extra_storage();
}

void
RestoreDataIterator::calc_row_extra_storage_words(const TableS* tableSpec)
{
  const NdbDictionary::Table* tab = tableSpec->m_dictTable;
  Uint32 bitmap_words = 0;
  Uint32 transform_words = 0;
  for (Uint32 i = 0; i<(Uint32)tab->getNoOfColumns(); i++)
  {
    /* Space for bitmap-copy out from PACKED format */
    if (tab->getColumn(i)->getType() == NdbDictionary::Column::Bit)
    {
      bitmap_words += (tab->getColumn(i)->getLength() + 31) >> 5;
    }
    /* Space for output from this column transform */
    const AttributeDesc* attr_desc = tableSpec->getAttributeDesc(i);
    if (attr_desc->transform != NULL)
    {
      transform_words += attr_desc->getSizeInWords();
    }
  }

  m_current_table_has_transforms = (transform_words > 0);

  m_row_max_extra_wordlen = bitmap_words + transform_words;
}

void
RestoreDataIterator::reset_extra_storage()
{
  m_extra_storage_curr_ptr = m_extra_storage_ptr;
}

void
RestoreDataIterator::alloc_extra_storage(Uint32 words)
{
  m_extra_storage_wordlen = words;
  m_extra_storage_ptr = (Uint32*)malloc(4 * words);
  m_extra_storage_curr_ptr = m_extra_storage_ptr;
}

void
RestoreDataIterator::free_extra_storage()
{
  if (m_extra_storage_ptr)
    free(m_extra_storage_ptr);
  m_extra_storage_ptr = 0;
  m_extra_storage_curr_ptr = 0;
}

Uint32
RestoreDataIterator::get_free_extra_storage() const
{

  return Uint32((m_extra_storage_ptr + m_extra_storage_wordlen) -
    m_extra_storage_curr_ptr);
}

void
RestoreDataIterator::check_extra_storage()
{
  assert(m_row_max_extra_wordlen <= m_extra_storage_wordlen);
  if (m_row_max_extra_wordlen >= get_free_extra_storage())
  {
    /**
     * No more space available to buffer rows, flush
     * what is outstanding, then reset buffers and
     * continue.
     */
    flush_and_reset_buffers();
    assert(get_free_extra_storage() > m_row_max_extra_wordlen);
    assert(m_extra_storage_ptr == m_extra_storage_curr_ptr);

    /**
     * We do not want to break up batching due to a lack of
     * extra buffer storage, but that is what has happened
     * here.
     * So to avoid this in future we will take this chance
     * to double the extra storage size, so that batching
     * boundaries are eventually controlled by the file
     * buffering only.
     */
    const Uint32 newWords = m_extra_storage_wordlen * 2;
    free_extra_storage();
    alloc_extra_storage(newWords);
  }
}


Uint32*
RestoreDataIterator::get_extra_storage(Uint32 len)
{
  Uint32 * currptr = m_extra_storage_curr_ptr;
  Uint32 * nextptr = currptr + len;
  Uint32 * endptr = m_extra_storage_ptr + m_extra_storage_wordlen;

  if (nextptr <= endptr)
  {
    m_extra_storage_curr_ptr = nextptr;
    return currptr;
  }
  
  abort();
  return 0;
}

TupleS & TupleS::operator=(const TupleS& tuple)
{
  prepareRecord(*tuple.m_currentTable);

  if (allAttrData)
    memcpy(allAttrData, tuple.allAttrData, getNoOfAttributes()*sizeof(AttributeData));
  
  return *this;
}
int TupleS::getNoOfAttributes() const {
  if (m_currentTable == 0)
    return 0;
  return m_currentTable->getNoOfAttributes();
}

TableS * TupleS::getTable() const {
  return m_currentTable;
}

AttributeDesc * TupleS::getDesc(int i) const {
  return m_currentTable->allAttributesDesc[i];
}

AttributeData * TupleS::getData(int i) const{
  return &(allAttrData[i]);
}

bool
TupleS::prepareRecord(TableS & tab){
  if (allAttrData) {
    if (getNoOfAttributes() == tab.getNoOfAttributes())
    {
      m_currentTable = &tab;
      return true;
    }
    delete [] allAttrData;
    m_currentTable= 0;
  }
  
  allAttrData = new AttributeData[tab.getNoOfAttributes()];
  if (allAttrData == 0)
    return false;
  
  m_currentTable = &tab;

  return true;
}

static
inline
Uint8*
pad(Uint8* src, Uint32 align, Uint32 bitPos)
{
  UintPtr ptr = UintPtr(src);
  switch(align){
  case DictTabInfo::aBit:
  case DictTabInfo::a32Bit:
  case DictTabInfo::a64Bit:
  case DictTabInfo::a128Bit:
    return (Uint8*)(((ptr + 3) & ~(UintPtr)3) + 4 * ((bitPos + 31) >> 5));
charpad:
  case DictTabInfo::an8Bit:
  case DictTabInfo::a16Bit:
    return src + 4 * ((bitPos + 31) >> 5);
  default:
#ifdef VM_TRACE
    abort();
#endif
    goto charpad;
  }
}

bool
applyColumnTransform(const NdbDictionary::Column* col,
                     const AttributeDesc* attr_desc,
                     AttributeData* attr_data,
                     void* dst_buf)
{
  assert(attr_desc->transform != NULL);

  void* src_ptr = (attr_data->null? NULL : attr_data->void_value);
  void* dst_ptr = dst_buf;

  if (!attr_desc->transform->apply(col,
                                   src_ptr,
                                   &dst_ptr))
  {
    return false;
  }

  if (dst_ptr == NULL)
  {
    assert(col->getNullable());
    attr_data->null = true;
    attr_data->size = 0;
    attr_data->void_value = NULL;
  }
  else
  {
    const uchar* dst_char = (const uchar*) dst_ptr;
    attr_data->null = false;
    attr_data->void_value = dst_ptr;
    switch(col->getArrayType())
    {
    case NDB_ARRAYTYPE_SHORT_VAR:
      attr_data->size = 1 + size_t(dst_char[0]);
      break;
    case NDB_ARRAYTYPE_MEDIUM_VAR:
      attr_data->size = 2 + size_t(dst_char[0])
        + (256 * size_t(dst_char[1]));
      break;
    default:
      /* No change */
      break;
    }
  }

  /* Check size is within 'word length' of column type */
  assert(attr_data->size <=
         4 * ((((Uint32)col->getSizeInBytes()) +3)/4));

  return true;
}


const TupleS *
RestoreDataIterator::getNextTuple(int  & res, const bool skipFragment)
{
  /* Check that we have space to return another tuple */
  check_extra_storage();

  while (true)
  {
    Uint32  dataLength = 0;
    // Read record length
    int r = buffer_read(&dataLength, sizeof(dataLength), 1);
    if (r != 1)
    {
      restoreLogger.log_error("getNextTuple:Error reading length of data part");
      res = -1;
      return NULL;
    } // if
  
    // Convert length from network byte order
    dataLength = ntohl(dataLength);
    const Uint32 dataLenBytes = 4 * dataLength;
  
    if (dataLength == 0) {
      // Zero length for last tuple
      // End of this data fragment
      restoreLogger.log_debug("End of fragment");
      res = 0;
      return NULL;
    } // if

    // Read tuple data
    void *_buf_ptr;
    r = buffer_get_ptr(&_buf_ptr, 1, dataLenBytes);
    if (r < 0 || Uint32(r) != dataLenBytes)
    {
      restoreLogger.log_error("getNextTuple:Read error: ");
      res = -1;
      return NULL;
    }

    m_count++;

    if (skipFragment)
    {
      /**
       * Skip unpacking work, we just want to read all the tuples up
       * to the end of this fragment
       */
      continue;
    }

    Uint32 *buf_ptr = (Uint32*)_buf_ptr;
    if (m_currentTable->backupVersion >= NDBD_RAW_LCP)
    {
      res = readTupleData_packed(buf_ptr, dataLength);
    }
    else
    {
      res = readTupleData_old(buf_ptr, dataLength);
    }

    if (res)
    {
      return NULL;
    }

    /* Apply column transforms if the table has any defined */
    if (m_current_table_has_transforms)
    {
      for (int i=0; i < m_currentTable->getNoOfAttributes(); i++)
      {
        const AttributeDesc* attr_desc = m_currentTable->getAttributeDesc(i);
        if (attr_desc->transform == NULL)
        {
          continue;
        }
        const NdbDictionary::Column* col = m_currentTable->m_dictTable->getColumn(i);
        void* dst_buf = get_extra_storage(attr_desc->getSizeInWords());
        assert(dst_buf != NULL);

        if (!applyColumnTransform(col,
                                  attr_desc,
                                  m_tuple.getData(i),
                                  dst_buf))
        {
          res = -1;
          return NULL;
        }
      }
    }

    res = 0;
    return &m_tuple;
  }
} // RestoreDataIterator::getNextTuple

TableS *
RestoreDataIterator::getCurrentTable()
{
  return m_currentTable;
}

int
RestoreDataIterator::readTupleData_packed(Uint32 *buf_ptr, 
                                          Uint32 dataLength)
{
  Uint32 * ptr = buf_ptr;
  /**
   * Unpack READ_PACKED header
   */
  Uint32 rp = * ptr;
  if(unlikely(!m_hostByteOrder))
    rp = Twiddle32(rp);

  AttributeHeader ah(rp);
  assert(ah.getAttributeId() == AttributeHeader::READ_PACKED);
  Uint32 bmlen = ah.getByteSize();
  assert((bmlen & 3) == 0);
  Uint32 bmlen32 = bmlen / 4;

  /**
   * Twiddle READ_BACKED header
   */
  if (!m_hostByteOrder)
  {
    for (Uint32 i = 0; i < 1 + bmlen32; i++)
    {
      ptr[i] = Twiddle32(ptr[i]);
    }
  }
  
  const NdbDictionary::Table* tab = m_currentTable->m_dictTable;
  
  // All columns should be present...
  assert(((tab->getNoOfColumns() + 31) >> 5) <= (int)bmlen32);
  
  /**
   * Iterate through attributes...
   */
  const Uint32 * bmptr = ptr + 1;
  Uint8* src = (Uint8*)(bmptr + bmlen32);
  Uint32 bmpos = 0;
  Uint32 bitPos = 0;
  for (Uint32 i = 0; i < (Uint32)tab->getNoOfColumns(); i++, bmpos++)
  {
    // All columns should be present
    assert(BitmaskImpl::get(bmlen32, bmptr, bmpos));
    const NdbColumnImpl & col = NdbColumnImpl::getImpl(* tab->getColumn(i));
    AttributeData * attr_data = m_tuple.getData(i);
    const AttributeDesc * attr_desc = m_tuple.getDesc(i);
    if (col.getNullable())
    {
      bmpos++;
      if (BitmaskImpl::get(bmlen32, bmptr, bmpos))
      {
        attr_data->null = true;
        attr_data->void_value = NULL;
        continue;
      }
    }
    
    attr_data->null = false;
    
    /**
     * Handle padding
     */
    Uint32 align = col.m_orgAttrSize;
    Uint32 attrSize = col.m_attrSize;
    Uint32 array = col.m_arraySize;
    Uint32 len = col.m_length;
    Uint32 sz = attrSize * array;
    Uint32 arrayType = col.m_arrayType;
    
    switch(align){
    case DictTabInfo::aBit:{ // Bit
      src = pad(src, 0, 0);
      Uint32* src32 = (Uint32*)src;
      
      Uint32 len32 = (len + 31) >> 5;
      Uint32* tmp = get_extra_storage(len32);
      attr_data->null = false;
      attr_data->void_value = tmp;
      attr_data->size = 4*len32;
      
      if (m_hostByteOrder)
      {
        BitmaskImpl::getField(1 + len32, src32, bitPos, len, tmp);
      }
      else
      {
        Uint32 ii;
        for (ii = 0; ii< (1 + len32); ii++)
          src32[ii] = Twiddle32(src32[ii]);
        BitmaskImpl::getField(1 + len32, (Uint32*)src, bitPos, len, tmp);
        for (ii = 0; ii< (1 + len32); ii++)
          src32[ii] = Twiddle32(src32[ii]);
      }
      
      src += 4 * ((bitPos + len) >> 5);
      bitPos = (bitPos + len) & 31;
      goto next;
    }
    default:
      src = pad(src, align, bitPos);
    }
    switch(arrayType){
    case NDB_ARRAYTYPE_FIXED:
      break;
    case NDB_ARRAYTYPE_SHORT_VAR:
      sz = 1 + src[0];
      break;
    case NDB_ARRAYTYPE_MEDIUM_VAR:
      sz = 2 + src[0] + 256 * src[1];
      break;
    default:
      abort();
    }
    
    attr_data->void_value = src;
    attr_data->size = sz;
    
    if(!Twiddle(attr_desc, attr_data))
    {
      return -1;
    }
    
    /**
     * Next
     */
    bitPos = 0;
    src += sz;
next:
    (void)1;
  }
  return 0;
}

int
RestoreDataIterator::readTupleData_old(Uint32 *buf_ptr, 
                                       Uint32 dataLength)
{
  Uint32 * ptr = buf_ptr;
  ptr += m_currentTable->m_nullBitmaskSize;
  Uint32 i;
  for(i= 0; i < m_currentTable->m_fixedKeys.size(); i++){
    assert(ptr < buf_ptr + dataLength);
 
    const Uint32 attrId = m_currentTable->m_fixedKeys[i]->attrId;

    AttributeData * attr_data = m_tuple.getData(attrId);
    const AttributeDesc * attr_desc = m_tuple.getDesc(attrId);

    const Uint32 sz = attr_desc->getSizeInWords();

    attr_data->null = false;
    attr_data->void_value = ptr;
    attr_data->size = 4*sz;

    if(!Twiddle(attr_desc, attr_data))
    {
      return -1;
    }
    ptr += sz;
  }
  
  for(i = 0; i < m_currentTable->m_fixedAttribs.size(); i++){
    assert(ptr < buf_ptr + dataLength);

    const Uint32 attrId = m_currentTable->m_fixedAttribs[i]->attrId;

    AttributeData * attr_data = m_tuple.getData(attrId);
    const AttributeDesc * attr_desc = m_tuple.getDesc(attrId);

    const Uint32 sz = attr_desc->getSizeInWords();

    attr_data->null = false;
    attr_data->void_value = ptr;
    attr_data->size = 4*sz;

    if(!Twiddle(attr_desc, attr_data))
    {
      return -1;
    }
    
    ptr += sz;
  }

  // init to NULL
  for(i = 0; i < m_currentTable->m_variableAttribs.size(); i++){
    const Uint32 attrId = m_currentTable->m_variableAttribs[i]->attrId;

    AttributeData * attr_data = m_tuple.getData(attrId);
    
    attr_data->null = true;
    attr_data->void_value = NULL;
  }

  int res;
  if (!ndbd_drop6(m_currentTable->backupVersion))
  {
    if ((res = readVarData(buf_ptr, ptr, dataLength)))
      return res;
  }
  else
  {
    if ((res = readVarData_drop6(buf_ptr, ptr, dataLength)))
      return res;
  }

  return 0;
}

int
RestoreDataIterator::readVarData(Uint32 *buf_ptr, Uint32 *ptr,
                                  Uint32 dataLength)
{
  while (ptr + 2 < buf_ptr + dataLength)
  {
    typedef BackupFormat::DataFile::VariableData VarData;
    VarData * data = (VarData *)ptr;
    Uint32 sz = ntohl(data->Sz);
    Uint32 attrId = ntohl(data->Id); // column_no

    AttributeData * attr_data = m_tuple.getData(attrId);
    const AttributeDesc * attr_desc = m_tuple.getDesc(attrId);
    
    // just a reminder - remove when backwards compat implemented
    if (m_currentTable->backupVersion < MAKE_VERSION(5,1,3) && 
        attr_desc->m_column->getNullable())
    {
      const Uint32 ind = attr_desc->m_nullBitIndex;
      if(BitmaskImpl::get(m_currentTable->m_nullBitmaskSize, 
                          buf_ptr,ind))
      {
        attr_data->null = true;
        attr_data->void_value = NULL;
        continue;
      }
    }

    if (m_currentTable->backupVersion < MAKE_VERSION(5,1,3))
    {
      sz *= 4;
    }
    
    attr_data->null = false;
    attr_data->void_value = &data->Data[0];
    attr_data->size = sz;

    //convert the length of blob(v1) and text(v1)
    if(!Twiddle(attr_desc, attr_data))
    {
      return -1;
    }
    
    ptr += ((sz + 3) >> 2) + 2;
  }

  assert(ptr == buf_ptr + dataLength);

  return 0;
}


int
RestoreDataIterator::readVarData_drop6(Uint32 *buf_ptr, Uint32 *ptr,
                                       Uint32 dataLength)
{
  Uint32 i;
  for (i = 0; i < m_currentTable->m_variableAttribs.size(); i++)
  {
    const Uint32 attrId = m_currentTable->m_variableAttribs[i]->attrId;

    AttributeData * attr_data = m_tuple.getData(attrId);
    const AttributeDesc * attr_desc = m_tuple.getDesc(attrId);

    if(attr_desc->m_column->getNullable())
    {
      const Uint32 ind = attr_desc->m_nullBitIndex;
      if(BitmaskImpl::get(m_currentTable->m_nullBitmaskSize, 
                          buf_ptr,ind))
      {
        attr_data->null = true;
        attr_data->void_value = NULL;
        continue;
      }
    }

    assert(ptr < buf_ptr + dataLength);

    typedef BackupFormat::DataFile::VariableData VarData;
    VarData * data = (VarData *)ptr;
    Uint32 sz = ntohl(data->Sz);
    assert(ntohl(data->Id) == attrId);

    attr_data->null = false;
    attr_data->void_value = &data->Data[0];

    if (!Twiddle(attr_desc, attr_data))
    {
      return -1;
    }
    ptr += (sz + 2);
  }
  assert(ptr == buf_ptr + dataLength);
  return 0;
}

BackupFile::BackupFile(void (* _free_data_callback)(void*), void *ctx)
  : free_data_callback(_free_data_callback), m_ctx(ctx)
{
  m_path[0] = 0;
  m_fileName[0] = 0;

  m_buffer_sz = BUFFER_SIZE;
  m_buffer = malloc(m_buffer_sz);
  m_buffer_ptr = m_buffer;
  m_buffer_data_left = 0;

  m_file_size = 0;
  m_file_pos = 0;
  m_is_undolog = false;

  m_part_count = 1;
#ifdef ERROR_INSERT
  m_error_insert = 0;
#endif
}

bool
BackupFile::validateBackupFile()
{
    if (!m_buffer)
    {
        restoreLogger.log_error("m_buffer is NULL");
        return false;
    }
    return true;
}

BackupFile::~BackupFile()
{
  int r = 0;
  if (m_xfile.is_open())
  {
    r = m_xfile.close(false);
  }

  if (m_file.close() == -1)
  {
    r = -1;
  }

  if (r == -1)
  {
    restoreLogger.log_error("Warning: File did not close correctly.");
  }

  if(m_buffer != 0)
  {
    free(m_buffer);
  }
}

bool
BackupFile::openFile(){
  int r;
  m_file_size = 0;
  m_file_pos = 0;

  info.setLevel(254);
  restoreLogger.log_info("Opening file '%s'", m_fileName);
  r = m_file.open(m_fileName, FsOpenReq::OM_READONLY);

  if(r == -1) {
    return false;
  }

  long long size = m_file.get_size();
  if (size != -1)
  {
    m_file_size = (Uint64)size;
    restoreLogger.log_info("File size %llu bytes", m_file_size);
  }
  else
  {
    restoreLogger.log_info("Progress reporting degraded output since fstat failed,"
         "errno: %u", errno);
    m_file_size = 0;
  }

  r = m_xfile.open(m_file,
                   reinterpret_cast<const byte*>(
                       g_backup_password_state.get_password()),
                   g_backup_password_state.get_password_length());
  bool fail = (r != 0);
  if (g_backup_password_state.get_password() != nullptr)
  {
    if (!m_xfile.is_encrypted())
    {
      restoreLogger.log_error("Decryption requested but file not "
                              "encrypted.");
      fail = true;
    }
    else if (r != 0)
    {
      restoreLogger.log_error("Can not read decrypted file. Might be wrong password.");
    }
  }
  else
  {
    if (m_xfile.is_encrypted())
    {
      restoreLogger.log_error("File is encrypted but no decryption "
                              "requested.");
      fail = true;
    }
    else if (r != 0)
    {
      restoreLogger.log_error("Can not read file. Might be corrupt.");
    }
  }

  if (!fail)
  {
    return true;
  }

  if (r != -1)
  {
    m_xfile.close(false);
  }
  m_file.close();
  return false;
}

int BackupFile::buffer_get_ptr_ahead(void **p_buf_ptr, Uint32 size, Uint32 nmemb)
{
  Uint32 sz = size*nmemb;
  if (sz > m_buffer_data_left) {

    flush_and_reset_buffers();

    if (m_is_undolog)
    {
      /* move the left data to the end of buffer
       */
      int r = 0;
      /* move the left data to the end of buffer
       * m_buffer_ptr point the end of the left data. buffer_data_start point the start of left data
       * m_buffer_data_left is the length of left data.
       */
      Uint64 file_left_entry_data = 0;
      Uint32 buffer_free_space = m_buffer_sz - m_buffer_data_left;
      void * buffer_end = (char *)m_buffer + m_buffer_sz;
      void * buffer_data_start = (char *)m_buffer_ptr - m_buffer_data_left;

      memmove((char *)buffer_end - m_buffer_data_left, buffer_data_start, m_buffer_data_left);
      buffer_data_start = (char *)buffer_end - m_buffer_data_left;
      /*
       * For undo log file we should read log entris backwards from log file.
       *   That mean the first entries should start at sizeof(m_fileHeader).
       *   The end of the last entries should be the end of log file(EOF-1).
       * If there are entries left in log file to read.
       *   m_file_pos should bigger than sizeof(m_fileHeader).
       * If the length of left log entries less than the residual length of buffer,
       *   we just need to read all the left entries from log file into the buffer.
       *   and all the left entries in log file should been read into buffer. Or
       * If the length of left entries is bigger than the residual length of buffer,
       *   we should fill the buffer because the current buffer can't contain
           all the left log entries, we should read more times.
       * 
       */
      if (m_file_pos > sizeof(m_fileHeader))
      {
        /*
         * We read(consume) data from the end of the buffer.
         * If the left data is not enough for next read in buffer,
         *   we move the residual data to the end of buffer.
         *   Then we will fill the start of buffer with new data from log file.
         * eg. If the buffer length is 10. "+" denotes useless content.
         *                          top        end
         *   Bytes in file        abcdefgh0123456789
         *   Byte in buffer       0123456789             --after first read
         *   Consume data...      (6789) (2345)
         *   Bytes in buffer      01++++++++             --after several consumes
         *   Move data to end     ++++++++01
         *   Bytes in buffer      abcdefgh01             --after second read
         */
	file_left_entry_data = m_file_pos - sizeof(m_fileHeader);
        if (file_left_entry_data <= buffer_free_space)
        {
          ndbxfrm_output_reverse_iterator out((unsigned char*)buffer_data_start, (unsigned char*)buffer_data_start - file_left_entry_data, false);
          byte* out_beg = out.begin();
          r = m_xfile.read_backward(&out);
          if (r == -1)
            abort(); // TODO
          r = out_beg - out.begin();
          require(size_t(r) == file_left_entry_data);
        }
        else
        {
	  // Fill remaining space at start of buffer with data from file.
          ndbxfrm_output_reverse_iterator out((unsigned char*)m_buffer + buffer_free_space, (unsigned char*)m_buffer, false);
          byte* out_beg = out.begin();
          r = m_xfile.read_backward(&out);
          if (r == -1)
            abort(); // TODO
          r = out_beg - out.begin();
          require(size_t(r) == buffer_free_space);
        }
      }
      m_file_pos -= r;
      m_buffer_data_left += (Uint32)r;
      //move to the end of buffer
      m_buffer_ptr = buffer_end;
    }
    else
    {
      memmove(m_buffer, m_buffer_ptr, m_buffer_data_left);
      ndbxfrm_output_iterator it = { static_cast<byte*>(m_buffer) + m_buffer_data_left,
                                     static_cast<byte*>(m_buffer) + m_buffer_sz,
                                     false };
      int r = m_xfile.read_forward(&it);
      if (r < 0)
      {
        return r;
      }
      r = it.begin() - (static_cast<byte*>(m_buffer) + m_buffer_data_left);
      m_file_pos += r;
      m_buffer_data_left += r;
      m_buffer_ptr = m_buffer;
    }

    if (sz > m_buffer_data_left)
      sz = size * (m_buffer_data_left / size);
  }

  /*
   * For undolog, the m_buffer_ptr points to the end of the left data.
   * After we get data from the end of buffer, the data-end move forward.
   *   So we should move m_buffer_ptr to the right place.
   */
  if(m_is_undolog)
  {
    *p_buf_ptr = (char *)m_buffer_ptr - sz;
  }
  else
  {
    *p_buf_ptr = m_buffer_ptr;
  }

  require(sz % size == 0);
  return sz/size;
}
int BackupFile::buffer_get_ptr(void **p_buf_ptr, Uint32 size, Uint32 nmemb)
{
  int r = buffer_get_ptr_ahead(p_buf_ptr, size, nmemb);
  if (r < 0)
  {
    return r;
  }

  if(m_is_undolog)
  {
    /* we read from end of buffer to start of buffer.
     * m_buffer_ptr keep at the end of real data in buffer.
     */
    m_buffer_ptr = ((char*)m_buffer_ptr)-(r*size);
    m_buffer_data_left -= (r*size);
  }
  else
  {
    m_buffer_ptr = ((char*)m_buffer_ptr)+(r*size);
    m_buffer_data_left -= (r*size);
  }

  return r;
}

int BackupFile::buffer_read_ahead(void *ptr, Uint32 size, Uint32 nmemb)
{
  void *buf_ptr;
  int r = buffer_get_ptr_ahead(&buf_ptr, size, nmemb);
  if (r < 0)
  {
    return r;
  }
  memcpy(ptr, buf_ptr, r*size);

  return r;
}

int BackupFile::buffer_read(void *ptr, Uint32 size, Uint32 nmemb)
{
  void *buf_ptr;
  int r = buffer_get_ptr(&buf_ptr, size, nmemb);
  if (r < 0)
  {
    return r;
  }
  memcpy(ptr, buf_ptr, r*size);
  return r;
}

void
BackupFile::setCtlFile(Uint32 nodeId, Uint32 backupId, const char * path){
  m_nodeId = nodeId;
  m_expectedFileHeader.BackupId = backupId;
  m_expectedFileHeader.FileType = BackupFormat::CTL_FILE;

  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  BaseString::snprintf(name, sz, "BACKUP-%u.%d.ctl", backupId, nodeId);  

  if (m_part_count > 1)
  {
    char multiset_name[PATH_MAX];
    BaseString::snprintf(multiset_name, sizeof(multiset_name),
                    "BACKUP-%u-PART-%u-OF-%u%s%s", backupId, m_part_id, m_part_count, DIR_SEPARATOR, name);
    setName(path, multiset_name);
  }
  else
  {
    setName(path, name);
  }
}

void
BackupFile::setDataFile(const BackupFile & bf, Uint32 no){
  m_nodeId = bf.m_nodeId;
  m_expectedFileHeader = bf.m_fileHeader;
  m_expectedFileHeader.FileType = BackupFormat::DATA_FILE;
  
  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  Uint32 backupId = m_expectedFileHeader.BackupId;
  if (bf.m_part_count > 1)
  {
    BaseString::snprintf(name, sz, "BACKUP-%u-PART-%d-OF-%d%sBACKUP-%u-%u.%d.Data",
          backupId, bf.m_part_id, bf.m_part_count, DIR_SEPARATOR, backupId, no, m_nodeId);
  }
  else
  {
    BaseString::snprintf(name, sz, "BACKUP-%u-%u.%d.Data",
          backupId, no, m_nodeId);
  }
  setName(bf.m_path, name);
}

void
BackupFile::setLogFile(const BackupFile & bf, Uint32 no){
  m_nodeId = bf.m_nodeId;
  m_expectedFileHeader = bf.m_fileHeader;
  m_expectedFileHeader.FileType = BackupFormat::LOG_FILE;
  
  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  Uint32 backupId = m_expectedFileHeader.BackupId;
  if (bf.m_part_count > 1)
  {
    BaseString::snprintf(name, sz, "BACKUP-%u-PART-%d-OF-%d%sBACKUP-%u.%d.log",
          backupId, bf.m_part_id, bf.m_part_count, DIR_SEPARATOR, backupId, m_nodeId);
  }
  else
  {
    BaseString::snprintf(name, sz, "BACKUP-%u.%d.log",
          backupId, m_nodeId);
  }
  setName(bf.m_path, name);
}

void
BackupFile::setName(const char * p, const char * n){
  const Uint32 sz = sizeof(m_path);
  if(p != 0 && strlen(p) > 0){
    if(p[strlen(p)-1] == DIR_SEPARATOR[0]){
      BaseString::snprintf(m_path, sz, "%s", p);
    } else {
      BaseString::snprintf(m_path, sz, "%s%s", p, DIR_SEPARATOR);
    }
  } else {
    m_path[0] = 0;
  }

  BaseString::snprintf(m_fileName, sizeof(m_fileName), "%s%s", m_path, n);
  restoreLogger.log_debug("Filename = %s", m_fileName);
}

bool
BackupFile::readHeader(){
  if(!openFile()){
    return false;
  }
  
  Uint32 oldsz = sizeof(BackupFormat::FileHeader_pre_backup_version);
  int r = buffer_read(&m_fileHeader, oldsz, 1);
  if(r != 1)
  {
    if (!m_xfile.is_encrypted())
    {
      restoreLogger.log_error("readDataFileHeader: Error reading header");
    }
    else
    {
      restoreLogger.log_error("readDataFileHeader: Error reading header. "
                              "Wrong password?");
    }
    return false;
  }
  
  // Convert from network to host byte order for platform compatibility
  /*
    Due to some optimization going on when using gcc 4.2.3 we
    have to read 'backup_version' into tmp variable. If
    'm_fileHeader.BackupVersion' is used directly in the if statement
    below it will have the wrong value.
  */
  Uint32 backup_version = ntohl(m_fileHeader.BackupVersion);
  m_fileHeader.BackupVersion = backup_version;
  m_fileHeader.SectionType = ntohl(m_fileHeader.SectionType);
  m_fileHeader.SectionLength = ntohl(m_fileHeader.SectionLength);
  m_fileHeader.FileType = ntohl(m_fileHeader.FileType);
  m_fileHeader.BackupId = ntohl(m_fileHeader.BackupId);
  m_fileHeader.BackupKey_0 = ntohl(m_fileHeader.BackupKey_0);
  m_fileHeader.BackupKey_1 = ntohl(m_fileHeader.BackupKey_1);

  if (backup_version >= NDBD_RAW_LCP)
  {
    int r = buffer_read(&m_fileHeader.NdbVersion,
                        sizeof(m_fileHeader) - oldsz, 1);
    if (r != 1)
    {
      if (!m_xfile.is_encrypted())
      {
        restoreLogger.log_error("readDataFileHeader: Error reading header");
      }
      else
      {
        restoreLogger.log_error("readDataFileHeader: Error reading header. "
                                "Wrong password?");
      }
      return false;
    }
    
    m_fileHeader.NdbVersion = ntohl(m_fileHeader.NdbVersion);
    m_fileHeader.MySQLVersion = ntohl(m_fileHeader.MySQLVersion);
  }
  else
  {
    m_fileHeader.NdbVersion = m_fileHeader.BackupVersion;
    m_fileHeader.MySQLVersion = 0;
  }
  
  restoreLogger.log_debug("FileHeader: %s %u %u %u %u %u %u %u %u",
                          m_fileHeader.Magic,
                          m_fileHeader.BackupVersion,
                          m_fileHeader.SectionType,
                          m_fileHeader.SectionLength,
                          m_fileHeader.FileType,
                          m_fileHeader.BackupId,
                          m_fileHeader.BackupKey_0,
                          m_fileHeader.BackupKey_1,
                          m_fileHeader.ByteOrder);

  restoreLogger.log_debug("ByteOrder is %u", m_fileHeader.ByteOrder);
  restoreLogger.log_debug("magicByteOrder is %u", magicByteOrder);
  

  if (m_fileHeader.FileType != m_expectedFileHeader.FileType &&
      !(m_expectedFileHeader.FileType == BackupFormat::LOG_FILE &&
      m_fileHeader.FileType == BackupFormat::UNDO_FILE)){
    // UNDO_FILE will do in case where we expect LOG_FILE
    abort();
  }
  
  if(m_fileHeader.FileType == BackupFormat::UNDO_FILE)
  {
    m_is_undolog = true;
    /* move pointer to end of data part. 
       move 4 bytes from the end of file 
       because footer contain 4 bytes 0 at the end of file.
       we discard the remain data stored in m_buffer.
    */
    m_file_size = m_xfile.move_to_end();
    m_file_pos = m_file_size; // TODO??
    require((int)m_file_size != -1);

    size_t sz = m_buffer_sz;
    if (m_file_size - sizeof(m_fileHeader) < sz)
    {
      sz = m_file_size - sizeof(m_fileHeader);
    }
    ndbxfrm_output_reverse_iterator out((unsigned char*)m_buffer + m_buffer_sz, (unsigned char*)m_buffer + m_buffer_sz - sz, false);
    byte* out_beg = out.begin();
    int r = m_xfile.read_backward(&out);
    if (r == -1)
    {
      abort(); // TODO
    }
    int n = out_beg - out.begin();
    require(n >= 4);
    uint32_t last_word;
    memcpy(&last_word, (char*)m_buffer + m_buffer_sz - 4, 4);
    require(last_word == 0);

    m_file_size = m_xfile.get_size();
    require(n <= m_xfile.get_size());
    m_file_pos = m_file_size - n;
    if (m_file_pos < sizeof(m_fileHeader))
    {
      int d = sizeof(m_fileHeader) - m_file_pos;
      m_file_pos += d;
      n -= d;
    }
    m_buffer_data_left = n - 4;
    m_buffer_ptr = (char*)m_buffer + m_buffer_sz - 4;
  }

  // Check for BackupFormat::FileHeader::ByteOrder if swapping is needed
  if (m_fileHeader.ByteOrder == magicByteOrder) {
    m_hostByteOrder = true;
  } else if (m_fileHeader.ByteOrder == swappedMagicByteOrder){
    m_hostByteOrder = false;
  } else {
    abort();
  }
  
  return true;
} // BackupFile::readHeader

bool
BackupFile::validateFooter(){
  return true;
}

#ifdef ERROR_INSERT
void BackupFile::error_insert(unsigned int code)
{
  m_error_insert = code;
  if(code == NDB_RESTORE_ERROR_INSERT_SMALL_BUFFER)
  {
    // Reduce size of buffer to test buffer overflow
    // handling. The buffer must still be large enough to
    // accommodate the file header.
    m_buffer_sz = 256;
  }
}
#endif

bool RestoreDataIterator::readFragmentHeader(int & ret, Uint32 *fragmentId)
{
  BackupFormat::DataFile::FragmentHeader Header;
  
  restoreLogger.log_debug("RestoreDataIterator::getNextFragment");
  
  while (1)
  {
    /* read first part of header */
    int r = buffer_read(&Header, 8, 1);
    if (r < 0)
    {
      if (!m_xfile.is_encrypted())
      {
        restoreLogger.log_error("readFragmentHeader: Error reading header");
      }
      else
      {
        restoreLogger.log_error("readFragmentHeader: Error reading header. "
                                "Wrong password?");
      }
      ret = -2;
      return false;
    }
    if (r != 1)
    {
      ret = 0;
      return false;
    } // if

    /* skip if EMPTY_ENTRY */
    Header.SectionType  = ntohl(Header.SectionType);
    Header.SectionLength  = ntohl(Header.SectionLength);
    if (Header.SectionType == BackupFormat::EMPTY_ENTRY)
    {
      void *tmp;
      if (Header.SectionLength < 2)
      {
        restoreLogger.log_error("getFragmentFooter:Error reading fragment footer");
        return false;
      }
      if (Header.SectionLength > 2)
      {
        int r = buffer_get_ptr(&tmp, Header.SectionLength * 4 - 8, 1);
        if (r < 0)
        {
          ret = -2;
          return false;
        }
      }
      continue;
    }
    break;
  }
  /* read rest of header */
  require(Header.SectionLength >= 2);
  int r = buffer_read(((char*)&Header) + 8, Header.SectionLength * 4 - 8, 1);
  if (r != 1)
  {
    ret = 0;
    return false;
  }
  Header.TableId  = ntohl(Header.TableId);
  Header.FragmentNo  = ntohl(Header.FragmentNo);
  Header.ChecksumType  = ntohl(Header.ChecksumType);
  
  restoreLogger.log_debug("FragmentHeader: %u %u %u %u %u",
                           Header.SectionType,
                           Header.SectionLength,
                           Header.TableId,
                           Header.FragmentNo,
                           Header.ChecksumType);
  
  m_currentTable = m_metaData.getTable(Header.TableId);
  if(m_currentTable == 0){
    ret = -1;
    return false;
  }
  
  if(!m_tuple.prepareRecord(*m_currentTable))
  {
    ret =-1;
    return false;
  }

  calc_row_extra_storage_words(m_currentTable);
  info.setLevel(254);
  restoreLogger.log_info("_____________________________________________________"
                         "\nProcessing data in table: %s(%u) fragment %u",
                           m_currentTable->getTableName(),
                           Header.TableId, Header.FragmentNo);
  
  m_count = 0;
  ret = 0;
  *fragmentId = Header.FragmentNo;
  return true;
} // RestoreDataIterator::getNextFragment


bool
RestoreDataIterator::validateFragmentFooter() {
  BackupFormat::DataFile::FragmentFooter footer;
  
  int r = buffer_read(&footer, sizeof(footer), 1);
  if (r != 1)
  {
    restoreLogger.log_error("getFragmentFooter:Error reading fragment footer");
    return false;
  } 
  
  // TODO: Handle footer, nothing yet
  footer.SectionType  = ntohl(footer.SectionType);
  footer.SectionLength  = ntohl(footer.SectionLength);
  footer.TableId  = ntohl(footer.TableId);
  footer.FragmentNo  = ntohl(footer.FragmentNo);
  footer.NoOfRecords  = ntohl(footer.NoOfRecords);
  footer.Checksum  = ntohl(footer.Checksum);

  assert(m_count == footer.NoOfRecords);
  
  return true;
} // RestoreDataIterator::getFragmentFooter

AttributeDesc::AttributeDesc(NdbDictionary::Column *c)
  : m_column(c), transform(NULL), truncation_detected(false)
{
  size = 8*NdbColumnImpl::getImpl(* c).m_attrSize;
  arraySize = NdbColumnImpl::getImpl(* c).m_arraySize;
  staging = false;
  parameterSz = 0;
}

AttributeDesc::~AttributeDesc()
{
  delete transform;
  transform = NULL;
}

void TableS::createAttr(NdbDictionary::Column *column)
{
  AttributeDesc * d = new AttributeDesc(column);
  if(d == NULL) {
    restoreLogger.log_error("Restore: Failed to allocate memory");
    abort();
  }
  d->attrId = allAttributesDesc.size();
  d->convertFunc = NULL;
  d->parameter = NULL;
  d->m_exclude = false;
  allAttributesDesc.push_back(d);

  if (d->m_column->getAutoIncrement())
    m_auto_val_attrib = d;

  if(d->m_column->getPrimaryKey() && backupVersion <= MAKE_VERSION(4,1,7))
  {
    m_fixedKeys.push_back(d);
    return;
  }
  
  if (d->m_column->getArrayType() == NDB_ARRAYTYPE_FIXED &&
      ! d->m_column->getNullable())
  {
    m_fixedAttribs.push_back(d);
    return;
  }

  // just a reminder - does not solve backwards compat
  if (backupVersion < MAKE_VERSION(5,1,3) || ndbd_drop6(backupVersion))
  {
    d->m_nullBitIndex = m_noOfNullable; 
    m_noOfNullable++;
    m_nullBitmaskSize = (m_noOfNullable + 31) / 32;
  }
  if ((d->m_column->getType() == NdbDictionary::Column::Blob) ||
      (d->m_column->getType() == NdbDictionary::Column::Text))
  {
    if (d->m_column->getPartSize() > 0)
    {
      m_has_blobs = true;
    }
  }
  m_variableAttribs.push_back(d);
} // TableS::createAttr

bool
TableS::get_auto_data(const TupleS & tuple, Uint32 * syskey, Uint64 * nextid) const
{
  /*
    Read current (highest) auto_increment value for
    a table. Currently there can only be one per table.
    The values are stored in sustable SYSTAB_0 as
    {SYSKEY,NEXTID} values where SYSKEY (32-bit) is
    the table_id and NEXTID (64-bit) is the next auto_increment
    value in the sequence (note though that sequences of
    values can have been fetched and that are cached in NdbAPI).
    SYSTAB_0 can contain other data so we need to check that
    the found SYSKEY value is a valid table_id (< 0x10000000).
   */
  AttributeData * attr_data = tuple.getData(0);
  AttributeDesc * attr_desc = tuple.getDesc(0);
  const AttributeS attr1 = {attr_desc, *attr_data};
  memcpy(syskey ,attr1.Data.u_int32_value, sizeof(Uint32));
  attr_data = tuple.getData(1);
  attr_desc = tuple.getDesc(1);
  const AttributeS attr2 = {attr_desc, *attr_data};
  memcpy(nextid, attr2.Data.u_int64_value, sizeof(Uint64));
  if (*syskey < 0x10000000)
  {
    return true;
  }
  return false;
}

Uint16 Twiddle16(Uint16 in)
{
  Uint16 retVal = 0;

  retVal = ((in & 0xFF00) >> 8) |
    ((in & 0x00FF) << 8);

  return(retVal);
} // Twiddle16

Uint32 Twiddle32(Uint32 in)
{
  Uint32 retVal = 0;

  retVal = ((in & 0x000000FF) << 24) | 
    ((in & 0x0000FF00) << 8)  |
    ((in & 0x00FF0000) >> 8)  |
    ((in & 0xFF000000) >> 24);
  
  return(retVal);
} // Twiddle32

Uint64 Twiddle64(Uint64 in)
{
  Uint64 retVal = 0;

  retVal = 
    ((in & (Uint64)0x00000000000000FFLL) << 56) | 
    ((in & (Uint64)0x000000000000FF00LL) << 40) | 
    ((in & (Uint64)0x0000000000FF0000LL) << 24) | 
    ((in & (Uint64)0x00000000FF000000LL) << 8) | 
    ((in & (Uint64)0x000000FF00000000LL) >> 8) | 
    ((in & (Uint64)0x0000FF0000000000LL) >> 24) | 
    ((in & (Uint64)0x00FF000000000000LL) >> 40) | 
    ((in & (Uint64)0xFF00000000000000LL) >> 56);

  return(retVal);
} // Twiddle64

RestoreLogIterator::RestoreLogIterator(const RestoreMetaData & md)
  : m_metaData(md) 
{
  restoreLogger.log_debug("RestoreLog constructor");
  setLogFile(md, 0);

  m_count = 0;
  m_last_gci = 0;
  m_rowBuffIndex = 0;
}

const LogEntry *
RestoreLogIterator::getNextLogEntry(int & res) {
  // Read record length
  const Uint32 startGCP = m_metaData.getStartGCP();
  const Uint32 stopGCP = m_metaData.getStopGCP();
  Uint32 tableId;
  Uint32 triggerEvent;
  Uint32 frag_id;
  Uint32 *attr_data;
  Uint32 attr_data_len;
  bool skip_entry = false;
  do {
    Uint32 len;
    Uint32 *logEntryPtr;
    if(m_is_undolog){
      int read_result = 0;
      read_result = buffer_read(&len, sizeof(Uint32), 1);
      //no more log data to read
      if (read_result == 0 ) {
        res = 0;
        return 0;
      }
      if (read_result < 0 || read_result != 1)
      {
        res= -1;
        return 0;
      }
    }
    else{
      if (buffer_read_ahead(&len, sizeof(Uint32), 1) != 1){
        res= -1;
        return 0;
      }
    }
    len= ntohl(len);

    Uint32 data_len = sizeof(Uint32) + len*4;
    int r = buffer_get_ptr((void **)(&logEntryPtr), 1, data_len);
    if (r < 0 || Uint32(r) != data_len)
    {
      res= -2;
      return 0;
    }
    
    if(len == 0){
      res= 0;
      return 0;
    }

    const Uint32 backup_file_version = m_metaData.getFileHeader().NdbVersion;
    if (unlikely(!ndbd_backup_file_fragid(backup_file_version)))
    {
      /*
        FragId was introduced in LogEntry in version
        5.1.6
        We set FragId to 0 in older versions (these versions
        do not support restore of user defined partitioned
        tables.
      */
      typedef BackupFormat::LogFile::LogEntry_no_fragid LogE_no_fragid;
      LogE_no_fragid * logE_no_fragid= (LogE_no_fragid *)logEntryPtr;
      tableId= ntohl(logE_no_fragid->TableId);
      triggerEvent= ntohl(logE_no_fragid->TriggerEvent);
      frag_id= 0;
      attr_data= &logE_no_fragid->Data[0];
      attr_data_len=
        len - BackupFormat::LogFile::LogEntry_no_fragid::HEADER_LENGTH_WORDS;
    }
    else /* normal case */
    {
      typedef BackupFormat::LogFile::LogEntry LogE;
      LogE * logE= (LogE *)logEntryPtr;
      tableId= ntohl(logE->TableId);
      triggerEvent= ntohl(logE->TriggerEvent);
      frag_id= ntohl(logE->FragId);
      attr_data= &logE->Data[0];
      attr_data_len=
        len - BackupFormat::LogFile::LogEntry::HEADER_LENGTH_WORDS;
    }
    
    const bool hasGcp= (triggerEvent & 0x10000) != 0;
    triggerEvent &= 0xFFFF;

    if(hasGcp){
      // last attr_data is gci info
      attr_data_len--;
      m_last_gci = ntohl(*(attr_data + attr_data_len));
    }
    if (m_is_undolog)
    {
      // Do not apply anything from startGCP or lower
      skip_entry = (m_last_gci <= startGCP);
    }
    else
    {
      // Do not apply anything after stopGCP
      skip_entry = (m_last_gci > stopGCP);
    }
    // Skip entries instead of stopping scan since entries are not ordered
    // by GCP. Entries from different GCPs may be interleaved, so scan till
    // EOF to read all matching entries.
  } while (skip_entry);

  m_logEntry.m_table = m_metaData.getTable(tableId);
  /* We should 'invert' the operation type when we restore an Undo log.
   *   To undo an insert operation, a delete is required.
   *   To undo a delete operation, an insert is required.
   * The backup have collected 'before values' for undoing 'delete+update' to make this work.
   * To undo insert, we only need primary key.
   */
  switch(triggerEvent){
  case TriggerEvent::TE_INSERT:
    if(m_is_undolog)
      m_logEntry.m_type = LogEntry::LE_DELETE;
    else
      m_logEntry.m_type = LogEntry::LE_INSERT;
    break;
  case TriggerEvent::TE_UPDATE:
    m_logEntry.m_type = LogEntry::LE_UPDATE;
    break;
  case TriggerEvent::TE_DELETE:
    if(m_is_undolog)
      m_logEntry.m_type = LogEntry::LE_INSERT;
    else
      m_logEntry.m_type = LogEntry::LE_DELETE;
    break;
  default:
    res = -1;
    return NULL;
  }

  const TableS * tab = m_logEntry.m_table;
  m_logEntry.clear();
  m_rowBuffIndex = 0;

  AttributeHeader * ah = (AttributeHeader *)attr_data;
  AttributeHeader *end = (AttributeHeader *)(attr_data + attr_data_len);
  AttributeS * attr;
  m_logEntry.m_frag_id = frag_id;
  while(ah < end){
    attr = m_logEntry.add_attr();
    if(attr == NULL) {
      restoreLogger.log_error("Restore: Failed to allocate memory");
      res = -1;
      return 0;
    }

    if(unlikely(!m_hostByteOrder))
      *(Uint32*)ah = Twiddle32(*(Uint32*)ah);

    attr->Desc = tab->getAttributeDesc(ah->getAttributeId());
    assert(attr->Desc != 0);

    const Uint32 sz = ah->getByteSize();
    if(sz == 0){
      attr->Data.null = true;
      attr->Data.void_value = NULL;
      attr->Data.size = 0;
    } else {
      attr->Data.null = false;
      attr->Data.void_value = ah->getDataPtr();
      attr->Data.size = sz;
      Twiddle(attr->Desc, &(attr->Data));
    }

    if (attr->Desc->transform)
    {
      const int col_idx = ah->getAttributeId();
      const NdbDictionary::Column* col = tab->m_dictTable->getColumn(col_idx);
      void* dst_buf = m_rowBuff + m_rowBuffIndex;
      m_rowBuffIndex+= attr->Desc->getSizeInWords();
      assert(m_rowBuffIndex <= RowBuffWords);

      if (!applyColumnTransform(col,
                                attr->Desc,
                                &attr->Data,
                                dst_buf))
      {
        res = -1;
        return 0;
      }
    }
    
    ah = ah->getNext();
  }

  m_count ++;
  res = 0;
  return &m_logEntry;
}

NdbOut &
operator<<(NdbOut& ndbout, const AttributeS& attr){
  const AttributeData & data = attr.Data;
  const AttributeDesc & desc = *(attr.Desc);

  if (data.null)
  {
    ndbout << g_ndbrecord_print_format.null_string;
    return ndbout;
  }
  
  NdbRecAttr tmprec(0);
  tmprec.setup(desc.m_column, 0);

  assert(desc.size % 8 == 0);
#ifndef NDEBUG
  const Uint32 length = (desc.size)/8 * (desc.arraySize);
#endif
  assert((desc.m_column->getArrayType() == NdbDictionary::Column::ArrayTypeFixed)
         ? (data.size == length)
         : (data.size <= length));

  tmprec.receive_data((Uint32*)data.void_value, data.size);

  ndbrecattr_print_formatted(ndbout, tmprec, g_ndbrecord_print_format);

  return ndbout;
}

// Print tuple data
NdbOut& 
operator<<(NdbOut& ndbout, const TupleS& tuple)
{
  for (int i = 0; i < tuple.getNoOfAttributes(); i++) 
  {
    if (i > 0)
      ndbout << g_ndbrecord_print_format.fields_terminated_by;
    AttributeData * attr_data = tuple.getData(i);
    AttributeDesc * attr_desc = tuple.getDesc(i);
    const AttributeS attr = {attr_desc, *attr_data};
    debug << i << " " << attr_desc->m_column->getName();
    ndbout << attr;
  } // for
  return ndbout;
}

// Print tuple data
NdbOut& 
operator<<(NdbOut& ndbout, const LogEntry& logE)
{
  switch(logE.m_type)
  {
  case LogEntry::LE_INSERT:
    ndbout << "INSERT " << logE.m_table->getTableName() << " ";
    break;
  case LogEntry::LE_DELETE:
    ndbout << "DELETE " << logE.m_table->getTableName() << " ";
    break;
  case LogEntry::LE_UPDATE:
    ndbout << "UPDATE " << logE.m_table->getTableName() << " ";
    break;
  default:
    ndbout << "Unknown log entry type (not insert, delete or update)" ;
  }
  
  for (Uint32 i= 0; i < logE.size();i++) 
  {
    const AttributeS * attr = logE[i];
    ndbout << attr->Desc->m_column->getName() << "=";
    ndbout << (* attr);
    if (i < (logE.size() - 1))
      ndbout << ", ";
  }
  return ndbout;
}

void
AttributeS::printAttributeValue() const {
  NdbDictionary::Column::Type columnType =
      this->Desc->m_column->getType();
  switch(columnType)
  {
    case NdbDictionary::Column::Char:
    case NdbDictionary::Column::Varchar:
    case NdbDictionary::Column::Binary:
    case NdbDictionary::Column::Varbinary:
    case NdbDictionary::Column::Datetime:
    case NdbDictionary::Column::Date:
    case NdbDictionary::Column::Longvarchar:
    case NdbDictionary::Column::Longvarbinary:
    case NdbDictionary::Column::Time:
    case NdbDictionary::Column::Timestamp:
    case NdbDictionary::Column::Time2:
    case NdbDictionary::Column::Datetime2:
    case NdbDictionary::Column::Timestamp2:
      ndbout << "\'" << (* this) << "\'";
      break;
    default:
      ndbout << (* this);
  }
}

void
LogEntry::printSqlLog() const {
  /* Extract the table name from log entry which is stored in
   * database/schema/table and convert to database.table format
   */
  BaseString tableName(m_table->getTableName());
  Vector<BaseString> tableNameParts;
  Uint32 noOfPK = m_table->m_dictTable->getNoOfPrimaryKeys();
  tableName.split(tableNameParts, "/");
  tableName.assign("");
  tableName.assign(tableNameParts[0]);
  tableName.append(".");
  tableName.append(tableNameParts[2]);
  switch(m_type)
  {
    case LE_INSERT:
      ndbout << "INSERT INTO " << tableName.c_str() << " VALUES(";
      for (Uint32 i = noOfPK; i < size(); i++)
      {
        /* Skip the first field(s) which contains additional
         * instance of the primary key */
        const AttributeS * attr = m_values[i];
        attr->printAttributeValue();
        if (i < (size() - 1))
          ndbout << ",";
      }
      ndbout << ")";
      break;
    case LE_DELETE:
      ndbout << "DELETE FROM " << tableName.c_str() << " WHERE ";
      for (Uint32 i = 0; i < size();i++)
      {
        /* Primary key(s) clauses */
        const AttributeS * attr = m_values[i];
        const char* columnName = attr->Desc->m_column->getName();
        ndbout << columnName << "=";
        attr->printAttributeValue();
        if (i < (size() - 1))
          ndbout << " AND ";
      }
      break;
    case LE_UPDATE:
      ndbout << "UPDATE " << tableName.c_str() << " SET ";
      for (Uint32 i = noOfPK; i < size(); i++)
      {
        /* Print column(s) being set*/
        const AttributeS * attr = m_values[i];
        const char* columnName = attr->Desc->m_column->getName();
        ndbout << columnName << "=";
        attr->printAttributeValue();
        if (i < (size() - 1))
          ndbout << ", ";
      }
      /*Print where clause with primary key(s)*/
      ndbout << " WHERE ";
      for (Uint32 i = 0; i < noOfPK; i++)
      {
        const AttributeS * attr = m_values[i];
        const char* columnName = attr->Desc->m_column->getName();
        ndbout << columnName << "=";
        attr->printAttributeValue();
        if(i < noOfPK-1)
          ndbout << " AND ";
      }
      break;
    default:
      ndbout << "Unknown log entry type (not insert, delete or update)" ;
  }
  ndbout << ";";
}

RestoreLogger::RestoreLogger():print_timestamp(true)
{
  m_mutex = NdbMutex_Create();
}

RestoreLogger::~RestoreLogger()
{
  NdbMutex_Destroy(m_mutex);
}

void RestoreLogger::log_error(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[LOG_MSGLEN];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  NdbMutex_Lock(m_mutex);
  if (print_timestamp) {
    Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    err << timestamp << " ";
  }

  err << getThreadPrefix() << buf << endl;
  NdbMutex_Unlock(m_mutex);
}

void RestoreLogger::log_info(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[LOG_MSGLEN];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  NdbMutex_Lock(m_mutex);
  if (print_timestamp) {
    Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    info << timestamp << " ";
  }

  info << getThreadPrefix() << buf << endl;
  NdbMutex_Unlock(m_mutex);
}

void RestoreLogger::log_debug(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[LOG_MSGLEN];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  NdbMutex_Lock(m_mutex);
  if (print_timestamp) {
    Logger::format_timestamp(time(NULL), timestamp, sizeof(timestamp));
    debug << timestamp << " ";
  }

  debug << getThreadPrefix() << buf << endl;
  NdbMutex_Unlock(m_mutex);
}

void
RestoreLogger::setThreadPrefix(const char* prefix)
{
   /* Reuse 'JAM buffer' Tls key for a per-thread prefix string buffer pointer */
   NDB_THREAD_TLS_JAM = (EmulatedJamBuffer*)prefix;
}

const char*
RestoreLogger::getThreadPrefix() const
{
   const char* prefix = (const char*) NDB_THREAD_TLS_JAM;
   if (prefix == NULL)
   {
      prefix =  "";
    }
   return prefix;
}

void
RestoreLogger::set_print_timestamp(bool print_TS) {
  print_timestamp = print_TS;
}

bool
RestoreLogger::get_print_timestamp() {
  return print_timestamp;
}

NdbOut &
operator<<(NdbOut& ndbout, const TableS & table)
{
  ndbout << "-- " << table.getTableName() << " --" << endl;
  ndbout << *(table.m_dictTable) << endl;
  return ndbout;
}

template class Vector<TableS*>;
template class Vector<AttributeS*>;
template class Vector<AttributeDesc*>;
template class Vector<FragmentInfo*>;
template class Vector<DictObject>;

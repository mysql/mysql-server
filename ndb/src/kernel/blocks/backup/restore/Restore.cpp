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

#include <assert.h>
#include "Restore.hpp"
#include "BackupFormat.hpp"
#include <NdbTCP.h>
#include <NdbStdio.h>
#include <OutputStream.hpp>
#include <Bitmask.hpp>

#include <AttributeHeader.hpp>
#include <trigger_definitions.h>
#include <SimpleProperties.hpp>
#include <signaldata/DictTabInfo.hpp>

// from src/ndbapi
#include <NdbDictionaryImpl.hpp>

Uint16 Twiddle16(Uint16 in); // Byte shift 16-bit data
Uint32 Twiddle32(Uint32 in); // Byte shift 32-bit data
Uint64 Twiddle64(Uint64 in); // Byte shift 64-bit data

bool
BackupFile::Twiddle(AttributeS* attr, Uint32 arraySize){

  if(m_hostByteOrder)
    return true;
  
  if(arraySize == 0){
    arraySize = attr->Desc->arraySize;
  }
  
  switch(attr->Desc->size){
  case 8:
    
    return true;
  case 16:
    for(unsigned i = 0; i<arraySize; i++){
      attr->Data.u_int16_value[i] = Twiddle16(attr->Data.u_int16_value[i]);
    }
    return true;
  case 32:
    for(unsigned i = 0; i<arraySize; i++){
      attr->Data.u_int32_value[i] = Twiddle32(attr->Data.u_int32_value[i]);
    }
    return true;
  case 64:
    for(unsigned i = 0; i<arraySize; i++){
      attr->Data.u_int64_value[i] = Twiddle64(attr->Data.u_int64_value[i]);
    }
    return true;
  default:
    return false;
  } // switch

} // Twiddle

FilteredNdbOut err(* new FileOutputStream(stderr), 0, 0);
FilteredNdbOut info(* new FileOutputStream(stdout), 1, 1);
FilteredNdbOut debug(* new FileOutputStream(stdout), 2, 0);

// To decide in what byte order data is
const Uint32 magicByteOrder = 0x12345678;
const Uint32 swappedMagicByteOrder = 0x78563412;

RestoreMetaData::RestoreMetaData(const char* path, Uint32 nodeId, Uint32 bNo) {
  
  debug << "RestoreMetaData constructor" << endl;
  setCtlFile(nodeId, bNo, path);
}

RestoreMetaData::~RestoreMetaData(){
  for(int i = 0; i<allTables.size(); i++)
    delete allTables[i];
  allTables.clear();
}

const TableS * 
RestoreMetaData::getTable(Uint32 tableId) const {
  for(int i = 0; i<allTables.size(); i++)
    if(allTables[i]->getTableId() == tableId)
      return allTables[i];
  return NULL;
}

Uint32
RestoreMetaData::getStopGCP() const {
  return m_stopGCP;
}

int
RestoreMetaData::loadContent(const char * catalog, 
			     const char * schema) 
{
			
#if NDB_VERSION_MAJOR >= VERSION_3X
  if(getMajor(m_fileHeader.NdbVersion) < VERSION_3X) {
    if(catalog == NULL) 
      return -1;
    if(schema == NULL) 
      return -1;
  }
  

  /**
   * if backup is of version 3 or higher, then
   * return -2 to indicate for the user that he
   * cannot restore tables to a certain catalog/schema
   */
  if(getMajor(m_fileHeader.NdbVersion) >= VERSION_3X && 
     (catalog != NULL || 
      schema != NULL)) {
    return -2;
  }
#endif 
#if NDB_VERSION_MAJOR < VERSION_3X
  if(getMajor(m_fileHeader.NdbVersion) >= VERSION_3X)
  {
    return -2;
  }
#endif

  Uint32 noOfTables = readMetaTableList();
  if(noOfTables == 0)
    return -3;
  for(Uint32 i = 0; i<noOfTables; i++){
    if(!readMetaTableDesc(catalog, schema)){
      return 0;
    }
  }
  if(!readGCPEntry())
    return 0;
  return 1;
}

Uint32
RestoreMetaData::readMetaTableList() {
  
  Uint32 sectionInfo[2];
  
  if (fread(&sectionInfo, sizeof(sectionInfo), 1, m_file) != 1){
    return 0;
  }
  sectionInfo[0] = ntohl(sectionInfo[0]);
  sectionInfo[1] = ntohl(sectionInfo[1]);

  const Uint32 tabCount = sectionInfo[1] - 2;

  const Uint32 len = 4 * tabCount;
  if(createBuffer(len) == 0)
    abort();

  if (fread(m_buffer, 1, len, m_file) != len){
    return 0;
  }
  
  return tabCount;
}

bool
RestoreMetaData::readMetaTableDesc(const char * catalog, 
				   const char * schema) {
  
  Uint32 sectionInfo[2];
  
  // Read section header 
  if (fread(&sectionInfo, sizeof(sectionInfo), 1, m_file) != 1){
    err << "readMetaTableDesc read header error" << endl;
    return false;
  } // if
  sectionInfo[0] = ntohl(sectionInfo[0]);
  sectionInfo[1] = ntohl(sectionInfo[1]);
  
  assert(sectionInfo[0] == BackupFormat::TABLE_DESCRIPTION);
  
  // Allocate temporary storage for dictTabInfo buffer
  const Uint32 len = (sectionInfo[1] - 2);
  if (createBuffer(4 * (len+1)) == NULL) {
    err << "readMetaTableDesc allocation error" << endl;
    return false;
  } // if
  
  // Read dictTabInfo buffer
  if (fread(m_buffer, 4, len, m_file) != len){
    err << "readMetaTableDesc read error" << endl;
    return false;
  } // if
  
  return parseTableDescriptor(m_buffer, 
			      len,
			      catalog,
			      schema);	     
}

bool
RestoreMetaData::readGCPEntry() {

  Uint32 data[4];
  
  
  BackupFormat::CtlFile::GCPEntry * dst = 
    (BackupFormat::CtlFile::GCPEntry *)&data[0];
  
  if(fread(dst, 4, 4, m_file) != 4){
    err << "readGCPEntry read error" << endl;
    return false;
  }
  
  dst->SectionType = ntohl(dst->SectionType);
  dst->SectionLength = ntohl(dst->SectionLength);
  
  if(dst->SectionType != BackupFormat::GCP_ENTRY){
    err << "readGCPEntry invalid format" << endl;
    return false;
  }
  
  dst->StartGCP = ntohl(dst->StartGCP);
  dst->StopGCP = ntohl(dst->StopGCP);
  
  m_startGCP = dst->StartGCP;
  m_stopGCP = dst->StopGCP;
  return true;
}


struct tmpTableS {
  Uint32 tableId;
  Uint32 schemaVersion;
  Uint32 noOfAttributes;
}; // tmpTableS

static const
SimpleProperties::SP2StructMapping
RestoreTabMap[] = {
  // Map the basic stuff to begin with
  DTIMAP(tmpTableS, TableId, tableId),
  DTIMAP(tmpTableS, TableVersion, schemaVersion),
  DTIMAP(tmpTableS, NoOfAttributes, noOfAttributes),
  
  DTIBREAK(AttributeName)
}; // RestoreTabMap

static const Uint32
TabMapSize = sizeof(RestoreTabMap) 
  / sizeof(SimpleProperties::SP2StructMapping);

/**
 * Use a temporary struct to keep variables in AttributeDesc private
 *     and DTIMAP requires all Uint32
 */
struct tmpAttrS {
  // Just the basic needed stuff is yet implemented
  char name[AttrNameLenC];
  Uint32 attrId;
  Uint32 type;
  Uint32 nullable;
  Uint32 key;
  Uint32 size;
  Uint32 arraySize;
};

static const
SimpleProperties::SP2StructMapping
RestoreAttrMap[] = {
  // Map the most basic properties 
  DTIMAP(tmpAttrS, AttributeId, attrId),
  DTIMAP(tmpAttrS, AttributeType, type),
  DTIMAP(tmpAttrS, AttributeNullableFlag, nullable),
  DTIMAP(tmpAttrS, AttributeKeyFlag, key),
  DTIMAP(tmpAttrS, AttributeSize, size),
  DTIMAP(tmpAttrS, AttributeArraySize, arraySize),
  DTIBREAK(AttributeEnd)
}; // RestoreAttrMap
static const Uint32
AttrMapSize = sizeof(RestoreAttrMap) 
  / sizeof(SimpleProperties::SP2StructMapping);

struct v2xKernel_to_v3xAPIMapping {
  Int32 kernelConstant;
  Int32 apiConstant;
};

enum v2xKernelTypes {
  ExtUndefined=0,// Undefined 
  ExtInt,        // 32 bit
  ExtUnsigned,   // 32 bit
  ExtBigint,     // 64 bit
  ExtBigunsigned,// 64 Bit
  ExtFloat,      // 32-bit float
  ExtDouble,     // 64-bit float
  ExtDecimal,    // Precision, Scale
  ExtChar,       // Len
  ExtVarchar,    // Max len
  ExtBinary,     // Len
  ExtVarbinary,  // Max len
  ExtDatetime,   // Precision down to 1 sec  (sizeof(Datetime) == 8 bytes )
  ExtTimespec    // Precision down to 1 nsec (sizeof(Datetime) == 12 bytes )
};

const
v2xKernel_to_v3xAPIMapping 
columnTypeMapping[] = {
  { ExtInt,             NdbDictionary::Column::Int },
  { ExtUnsigned,        NdbDictionary::Column::Unsigned },
  { ExtBigint,          NdbDictionary::Column::Bigint },
  { ExtBigunsigned,     NdbDictionary::Column::Bigunsigned },
  { ExtFloat,           NdbDictionary::Column::Float },
  { ExtDouble,          NdbDictionary::Column::Double },
  { ExtDecimal,         NdbDictionary::Column::Decimal },
  { ExtChar,            NdbDictionary::Column::Char },
  { ExtVarchar,         NdbDictionary::Column::Varchar },
  { ExtBinary,          NdbDictionary::Column::Binary },
  { ExtVarbinary,       NdbDictionary::Column::Varbinary },
  { ExtDatetime,        NdbDictionary::Column::Datetime },
  { ExtTimespec,        NdbDictionary::Column::Timespec },
  { -1, -1 }
};

static
NdbDictionary::Column::Type
convertToV3x(Int32 kernelConstant, const v2xKernel_to_v3xAPIMapping   map[], 
	       Int32 def)
{
  int i = 0;
  while(map[i].kernelConstant != kernelConstant){
    if(map[i].kernelConstant == -1 &&
       map[i].apiConstant == -1){
      return (NdbDictionary::Column::Type)def;
    }
    i++;
  }
  return (NdbDictionary::Column::Type)map[i].apiConstant;
}



// Parse dictTabInfo buffer and pushback to to vector storage 
// Using SimpleProperties (here we don't need ntohl, ref:ejonore)
bool
RestoreMetaData::parseTableDescriptor(const Uint32 * data, 
				      Uint32 len,
				      const char * catalog,
				      const char * schema)   {
  SimplePropertiesLinearReader it(data, len);
  SimpleProperties::UnpackStatus spStatus;
  
  // Parse table name
  if (it.getKey() != DictTabInfo::TableName) {
    err << "readMetaTableDesc getKey table name error" << endl;
    return false;
  } // if

  /**
   * if backup was taken in v21x then there is no info about catalog,
   * and schema. This infomration is concatenated to the tableName.
   *
   */
  char tableName[MAX_TAB_NAME_SIZE*2]; // * 2 for db and schema.-.  
  

  char tmpTableName[MAX_TAB_NAME_SIZE]; 
  it.getString(tmpTableName);
#if NDB_VERSION_MAJOR >= VERSION_3X
  /**
   * only mess with name in version 3.
   */
  /*  switch(getMajor(m_fileHeader.NdbVersion)) {
   */
  if(getMajor(m_fileHeader.NdbVersion) < VERSION_3X) 
    {

      if(strcmp(tmpTableName, "SYSTAB_0") == 0 ||
	 strcmp(tmpTableName, "NDB$EVENTS_0") == 0)
	{
	  sprintf(tableName,"sys/def/%s",tmpTableName);   
	} 
      else { 
	if(catalog == NULL && schema == NULL)
	{
	  sprintf(tableName,"%s",tmpTableName);      
	} 
	else 
	{
	  sprintf(tableName,"%s/%s/%s",catalog,schema,tmpTableName);      
	}
      }
    }
  else
    sprintf(tableName,"%s",tmpTableName);
#elif NDB_VERSION_MAJOR < VERSION_3X
  /**
   * this is version two!
   */
    sprintf(tableName,"%s",tmpTableName);
#endif
  if (strlen(tableName) == 0) {
    err << "readMetaTableDesc getString table name error" << endl;
    return false;
  } // if

  TableS * table = new TableS(tableName);
  if(table == NULL) {
    return false;
  }

  table->setBackupVersion(m_fileHeader.NdbVersion);
  tmpTableS tmpTable;
  spStatus = SimpleProperties::unpack(it, &tmpTable, 
				      RestoreTabMap, TabMapSize, true, true);
  if ((spStatus != SimpleProperties::Break) ||
      it.getKey() != DictTabInfo::AttributeName) {
    err << "readMetaTableDesc sp.unpack error" << endl;
    delete table;
    return false;
  } // if

  debug << "Parsed table id " << tmpTable.tableId << endl;
  table->setTableId(tmpTable.tableId);
  debug << "Parsed table #attr " << tmpTable.noOfAttributes << endl;
  debug << "Parsed table schema version not used " << endl;

  for (Uint32 i = 0; i < tmpTable.noOfAttributes; i++) {
    if (it.getKey() != DictTabInfo::AttributeName) {
      err << "readMetaTableDesc error " << endl;
      delete table;
      return false;
    } // if

    tmpAttrS tmpAttr;
    if(it.getValueLen() > AttrNameLenC){
      err << "readMetaTableDesc attribute name too long??" << endl;
      delete table;
      return false;
    }
    it.getString(tmpAttr.name);

    spStatus = SimpleProperties::unpack(it, &tmpAttr, RestoreAttrMap, 
					AttrMapSize, true, true);
    if ((spStatus != SimpleProperties::Break) ||
	(it.getKey() != DictTabInfo::AttributeEnd)) {
      err << "readMetaTableDesc sp unpack attribute " << i << " error" 
	  << endl;
      delete table;
      return false;
    } // if
    
    debug << "Creating attribute " << i << " " << tmpAttr.name << endl;
    
    bool thisNullable = (bool)(tmpAttr.nullable); // Really not needed (now)
    KeyType thisKey = (KeyType)(tmpAttr.key); // These are identical (right now)
    // Convert attribute size from enum to Uint32
    // The static consts are really enum taking the value in DictTabInfo
    // e.g. 3 is not ...0011 but rather ...0100 
    //TODO: rather do a switch if the constants should change
    Uint32 thisSize = 1 << tmpAttr.size;
    // Convert attribute type to AttrType
    AttrType thisType;
    switch (tmpAttr.type) {
    case 0: // SignedType
      thisType = Signed;
      break;
    case 1: // UnSignedType
      thisType = UnSigned;
      break;
    case 2: // FloatingPointType
      thisType = Float;
      break;
    case 3: // StringType:
      debug << "String type detected " << endl;
      thisType = String;
      break;
    default:
      // What, default to unsigned?
      thisType = UnSigned;
      break;
    } // switch
    /*    ndbout_c << "  type: " << thisType << " size: " << thisSize <<" arraySize: "
	     << tmpAttr.arraySize << " nullable: " << thisNullable << " key: " 
	     << thisKey << endl;
    */
    table->createAttr(tmpAttr.name, thisType, 
		     thisSize, tmpAttr.arraySize, 
		     thisNullable, thisKey);
    if (!it.next()) {
      break;
      // Check number of created attributes and compare with expected
      //ndbout << "readMetaTableDesc expecting more attributes" << endl;
      //return false;
    } // if
  } // for 
  
  debug << "Pushing table " << tableName << endl;
  debug << "   with " << table->getNoOfAttributes() << " attributes" << endl;
  allTables.push_back(table);

#ifndef restore_old_types
  NdbTableImpl* tableImpl = 0;
  int ret = NdbDictInterface::parseTableInfo(&tableImpl, data, len);
#if NDB_VERSION_MAJOR >= VERSION_3X
  NdbDictionary::Column::Type type;
  if(getMajor(m_fileHeader.NdbVersion) < VERSION_3X) {
    tableImpl->setName(tableName);   
    Uint32 noOfColumns = tableImpl->getNoOfColumns();
    for(Uint32 i = 0 ; i < noOfColumns; i++) {
      type = convertToV3x(tableImpl->getColumn(i)->m_extType, 
			  columnTypeMapping,
			  -1);
      if(type == -1) 
      {
	ndbout_c("Restore: Was not able to map external type %d (in v2x) "
		 " to a proper type in v3x", tableImpl->getColumn(i)->m_extType);
	return false;
      }
      else 
      {
        tableImpl->getColumn(i)->m_type =  type;
      }

      

     
    }
  }
#endif
  if (ret != 0) {
    err << "parseTableInfo " << tableName << " failed" << endl;
    return false;
  }
  if(tableImpl == 0)
    return false;
  debug << "parseTableInfo " << tableName << " done" << endl;
  table->m_dictTable = tableImpl;
#endif
  return true;
}

// Constructor
RestoreDataIterator::RestoreDataIterator(const RestoreMetaData & md)
  : m_metaData(md) 
{
  debug << "RestoreDataIterator constructor" << endl;
  setDataFile(md, 0);
}

RestoreDataIterator::~RestoreDataIterator(){
}

bool
TupleS::prepareRecord(const TableS & tab){
  m_currentTable = &tab;
  for(int i = 0; i<allAttributes.size(); i++) {
    if(allAttributes[i] != NULL)
      delete allAttributes[i];
  }
  allAttributes.clear();
  AttributeS * a;
  for(int i = 0; i<tab.getNoOfAttributes(); i++){
    a = new AttributeS;
    if(a == NULL) {
      ndbout_c("Restore: Failed to allocate memory");
      return false;
    }
    a->Desc = tab[i];
    allAttributes.push_back(a);
  }
  return true;
}

const TupleS *
RestoreDataIterator::getNextTuple(int  & res) {
  TupleS * tup = new TupleS();
  if(tup == NULL) {
    ndbout_c("Restore: Failed to allocate memory");
    res = -1;
    return NULL;
  }
  if(!tup->prepareRecord(* m_currentTable)) {
    res =-1;
    return NULL;
  }
    

  Uint32  dataLength = 0;
  // Read record length
  if (fread(&dataLength, sizeof(dataLength), 1, m_file) != 1){
    err << "getNextTuple:Error reading length  of data part" << endl;
    delete tup;
    res = -1;
    return NULL;
  } // if
  
  // Convert length from network byte order
  dataLength = ntohl(dataLength);
  const Uint32 dataLenBytes = 4 * dataLength;
  
  if (dataLength == 0) {
    // Zero length for last tuple
    // End of this data fragment
    debug << "End of fragment" << endl;
    res = 0;
    delete tup;
    return NULL;
  } // if
  
  tup->createDataRecord(dataLenBytes);
  // Read tuple data
  if (fread(tup->getDataRecord(), 1, dataLenBytes, m_file) != dataLenBytes) {
    err << "getNextTuple:Read error: " << endl;
    delete tup;
    res = -1;
    return NULL;
  }
  
  Uint32 * ptr = tup->getDataRecord();
  ptr += m_currentTable->m_nullBitmaskSize;

  for(int i = 0; i < m_currentTable->m_fixedKeys.size(); i++){
    assert(ptr < tup->getDataRecord() + dataLength);
    
    const Uint32 attrId = m_currentTable->m_fixedKeys[i]->attrId;
    AttributeS * attr = tup->allAttributes[attrId];

    const Uint32 sz = attr->Desc->getSizeInWords();

    attr->Data.null = false;
    attr->Data.void_value = ptr;

    if(!Twiddle(attr))
      {
	res = -1;
	return NULL;
      }
    ptr += sz;
  }

  for(int i = 0; i<m_currentTable->m_fixedAttribs.size(); i++){
    assert(ptr < tup->getDataRecord() + dataLength);

    const Uint32 attrId = m_currentTable->m_fixedAttribs[i]->attrId;
    AttributeS * attr = tup->allAttributes[attrId];

    const Uint32 sz = attr->Desc->getSizeInWords();

    attr->Data.null = false;
    attr->Data.void_value = ptr;

    if(!Twiddle(attr))
      {
	res = -1;
	return NULL;
      }

    ptr += sz;
  }

  for(int i = 0; i<m_currentTable->m_variableAttribs.size(); i++){
    const Uint32 attrId = m_currentTable->m_variableAttribs[i]->attrId;
    AttributeS * attr = tup->allAttributes[attrId];
    
    if(attr->Desc->nullable){
      const Uint32 ind = attr->Desc->m_nullBitIndex;
      if(BitmaskImpl::get(m_currentTable->m_nullBitmaskSize, 
			  tup->getDataRecord(),ind)){
	attr->Data.null = true;
	attr->Data.void_value = NULL;
	continue;
      }
    }

    assert(ptr < tup->getDataRecord() + dataLength);

    typedef BackupFormat::DataFile::VariableData VarData;
    VarData * data = (VarData *)ptr;
    Uint32 sz = ntohl(data->Sz);
    Uint32 id = ntohl(data->Id);
    assert(id == attrId);
    
    attr->Data.null = false;
    attr->Data.void_value = &data->Data[0];

    /**
     * Compute array size
     */
    const Uint32 arraySize = (4 * sz) / (attr->Desc->size / 8);
    assert(arraySize >= attr->Desc->arraySize);
    if(!Twiddle(attr, attr->Desc->arraySize))
      {
	res = -1;
	return NULL;
      }

    ptr += (sz + 2);
  }

  m_count ++;  
  res = 0;
  return tup;
} // RestoreDataIterator::getNextTuple

BackupFile::BackupFile(){
  m_file = 0;
  m_path[0] = 0;
  m_fileName[0] = 0;
  m_buffer = 0;
  m_bufferSize = 0;
}

BackupFile::~BackupFile(){
  if(m_file != 0)
    fclose(m_file);
  if(m_buffer != 0)
    free(m_buffer);
}

bool
BackupFile::openFile(){
  if(m_file != NULL){
    fclose(m_file);
    m_file = 0;
  }
  
  m_file = fopen(m_fileName, "r");
  return m_file != 0;
}

Uint32 *
BackupFile::createBuffer(Uint32 bytes){
  if(bytes > m_bufferSize){
    if(m_buffer != 0)
      free(m_buffer);
    m_bufferSize = m_bufferSize + 2 * bytes;
    m_buffer = (Uint32*)malloc(m_bufferSize);
  }
  return m_buffer;
}

void
BackupFile::setCtlFile(Uint32 nodeId, Uint32 backupId, const char * path){
  m_nodeId = nodeId;
  m_expectedFileHeader.BackupId = backupId;
  m_expectedFileHeader.FileType = BackupFormat::CTL_FILE;

  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  snprintf(name, sz, "BACKUP-%d.%d.ctl", backupId, nodeId);  
  setName(path, name);
}

void
BackupFile::setDataFile(const BackupFile & bf, Uint32 no){
  m_nodeId = bf.m_nodeId;
  m_expectedFileHeader = bf.m_fileHeader;
  m_expectedFileHeader.FileType = BackupFormat::DATA_FILE;
  
  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  snprintf(name, sz, "BACKUP-%d-%d.%d.Data", 
	   m_expectedFileHeader.BackupId, no, m_nodeId);
  setName(bf.m_path, name);
}

void
BackupFile::setLogFile(const BackupFile & bf, Uint32 no){
  m_nodeId = bf.m_nodeId;
  m_expectedFileHeader = bf.m_fileHeader;
  m_expectedFileHeader.FileType = BackupFormat::LOG_FILE;
  
  char name[PATH_MAX]; const Uint32 sz = sizeof(name);
  snprintf(name, sz, "BACKUP-%d.%d.log", 
	   m_expectedFileHeader.BackupId, m_nodeId);
  setName(bf.m_path, name);
}

void
BackupFile::setName(const char * p, const char * n){
  const Uint32 sz = sizeof(m_path);
  if(p != 0 && strlen(p) > 0){
    if(p[strlen(p)-1] == '/'){
      snprintf(m_path, sz, "%s", p);
    } else {
      snprintf(m_path, sz, "%s%s", p, "/");
    }
  } else {
    m_path[0] = 0;
  }

  snprintf(m_fileName, sizeof(m_fileName), "%s%s", m_path, n);
  debug << "Filename = " << m_fileName << endl;
}

bool
BackupFile::readHeader(){
  if(!openFile()){
    return false;
  }
  
  if(fread(&m_fileHeader, sizeof(m_fileHeader), 1, m_file) != 1){
    err << "readDataFileHeader: Error reading header" << endl;
    return false;
  }
  
  // Convert from network to host byte order for platform compatibility
  m_fileHeader.NdbVersion  = ntohl(m_fileHeader.NdbVersion);
  m_fileHeader.SectionType = ntohl(m_fileHeader.SectionType);
  m_fileHeader.SectionLength = ntohl(m_fileHeader.SectionLength);
  m_fileHeader.FileType = ntohl(m_fileHeader.FileType);
  m_fileHeader.BackupId = ntohl(m_fileHeader.BackupId);
  m_fileHeader.BackupKey_0 = ntohl(m_fileHeader.BackupKey_0);
  m_fileHeader.BackupKey_1 = ntohl(m_fileHeader.BackupKey_1);

  debug << "FileHeader: " << m_fileHeader.Magic << " " <<
    m_fileHeader.NdbVersion << " " <<
    m_fileHeader.SectionType << " " <<
    m_fileHeader.SectionLength << " " <<
    m_fileHeader.FileType << " " <<
    m_fileHeader.BackupId << " " <<
    m_fileHeader.BackupKey_0 << " " <<
    m_fileHeader.BackupKey_1 << " " <<
    m_fileHeader.ByteOrder << endl;
  
  debug << "ByteOrder is " << m_fileHeader.ByteOrder << endl;
  debug << "magicByteOrder is " << magicByteOrder << endl;
  
  if (m_fileHeader.FileType != m_expectedFileHeader.FileType){
    abort();
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

bool
RestoreDataIterator::readFragmentHeader(int & ret)
{
  BackupFormat::DataFile::FragmentHeader Header;
  
  debug << "RestoreDataIterator::getNextFragment" << endl;
  
  if (fread(&Header, sizeof(Header), 1, m_file) != 1){
    ret = 0;
    return false;
  } // if
  
  Header.SectionType  = ntohl(Header.SectionType);
  Header.SectionLength  = ntohl(Header.SectionLength);
  Header.TableId  = ntohl(Header.TableId);
  Header.FragmentNo  = ntohl(Header.FragmentNo);
  Header.ChecksumType  = ntohl(Header.ChecksumType);
  
  debug << "FragmentHeader: " << Header.SectionType 
	<< " " << Header.SectionLength 
	<< " " << Header.TableId 
	<< " " << Header.FragmentNo 
	<< " " << Header.ChecksumType << endl;
  
  m_currentTable = m_metaData.getTable(Header.TableId);
  if(m_currentTable == 0){
    ret = -1;
    return false;
  }
  
  info << "_____________________________________________________" << endl
       << "Restoring data in table: " << m_currentTable->getTableName() 
       << "(" << Header.TableId << ") fragment " 
       << Header.FragmentNo << endl;
  
  m_count = 0;
  ret = 0;
  return true;
} // RestoreDataIterator::getNextFragment


bool
RestoreDataIterator::validateFragmentFooter() {
  BackupFormat::DataFile::FragmentFooter footer;
  
  if (fread(&footer, sizeof(footer), 1, m_file) != 1){
    err << "getFragmentFooter:Error reading fragment footer" << endl;
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

void TableS::createAttr(const char* name, 
			const AttrType type, 
			const unsigned int size, // in bytes
			const unsigned int arraySize, 
			const bool nullable,
			const KeyType key)
{
  AttributeDesc desc;

  strncpy(desc.name, name, AttrNameLenC);
  desc.type = type;
  desc.size = size;
  desc.arraySize = arraySize;
  desc.nullable = nullable;
  desc.key = key;
  desc.attrId = allAttributesDesc.size();

  AttributeDesc * d = new AttributeDesc(desc);
  if(d == NULL) {
    ndbout_c("Restore: Failed to allocate memory");
    abort();
  }
  d->m_table = this;
  allAttributesDesc.push_back(d);

  if(desc.key != NoKey /* && not variable */){
    m_fixedKeys.push_back(d);
    return;
  }
  if(!nullable){
    m_fixedAttribs.push_back(d);
    return;
  }
  if(nullable){
    d->m_nullBitIndex = m_noOfNullable; 
    m_noOfNullable++;
    m_nullBitmaskSize = (m_noOfNullable + 31) / 32;
  }
  m_variableAttribs.push_back(d);
} // TableS::createAttr

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
  debug << "RestoreLog constructor" << endl;
  setLogFile(md, 0);

  m_count = 0;
}

const LogEntry *
RestoreLogIterator::getNextLogEntry(int & res) {
  // Read record length
  typedef BackupFormat::LogFile::LogEntry LogE;

  Uint32 gcp = 0;
  LogE * logE = 0;
  Uint32 len = ~0;
  const Uint32 stopGCP = m_metaData.getStopGCP();
  do {
    
    if(createBuffer(4) == 0) {
      res = -1;
      return NULL;
    }
     

    if (fread(m_buffer, sizeof(Uint32), 1, m_file) != 1){
      res = -1;
      return NULL;
    }
    
    m_buffer[0] = ntohl(m_buffer[0]);
    len = m_buffer[0];
    if(len == 0){
      res = 0;
      return 0;
    }

    if(createBuffer(4 * (len + 1)) == 0){
      res = -1;
      return NULL;
    }
    
    if (fread(&m_buffer[1], 4, len, m_file) != len) {
      res = -1;
      return NULL;
    }
    
    logE = (LogE *)&m_buffer[0];
    logE->TableId = ntohl(logE->TableId);
    logE->TriggerEvent = ntohl(logE->TriggerEvent);
    
    const bool hasGcp = (logE->TriggerEvent & 0x10000) != 0;
    logE->TriggerEvent &= 0xFFFF;
    
    if(hasGcp){
      len--;
      gcp = ntohl(logE->Data[len-2]);
    }
  } while(gcp > stopGCP + 1);

  for(int i=0; i<m_logEntry.m_values.size();i++)
    delete m_logEntry.m_values[i];
  m_logEntry.m_values.clear();
  m_logEntry.m_table = m_metaData.getTable(logE->TableId);
  switch(logE->TriggerEvent){
  case TriggerEvent::TE_INSERT:
    m_logEntry.m_type = LogEntry::LE_INSERT;
    break;
  case TriggerEvent::TE_UPDATE:
    m_logEntry.m_type = LogEntry::LE_UPDATE;
    break;
  case TriggerEvent::TE_DELETE:
    m_logEntry.m_type = LogEntry::LE_DELETE;
    break;
  default:
    res = -1;
    return NULL;
  }

  const TableS * tab = m_logEntry.m_table;

  AttributeHeader * ah = (AttributeHeader *)&logE->Data[0];
  AttributeHeader *end = (AttributeHeader *)&logE->Data[len - 2];
  AttributeS *  attr;
  while(ah < end){
    attr = new AttributeS;
    if(attr == NULL) {
      ndbout_c("Restore: Failed to allocate memory");
      res = -1;
      return NULL;
    }
    attr->Desc = (* tab)[ah->getAttributeId()];
    assert(attr->Desc != 0);

    const Uint32 sz = ah->getDataSize();
    if(sz == 0){
      attr->Data.null = true;
      attr->Data.void_value = NULL;
    } else {
      attr->Data.null = false;
      attr->Data.void_value = ah->getDataPtr();
    }
    
    Twiddle(attr);
    m_logEntry.m_values.push_back(attr);
    
    ah = ah->getNext();
  }
  
  m_count ++;
  res = 0;
  return &m_logEntry;
}

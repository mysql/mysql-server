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

#include <signaldata/DictTabInfo.hpp>
#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <cstring>

//static 
const
SimpleProperties::SP2StructMapping
DictTabInfo::TableMapping[] = {
  DTI_MAP_STR(Table, TableName, TableName, MAX_TAB_NAME_SIZE),
  DTI_MAP_INT(Table, TableId, TableId),
  DTI_MAP_STR(Table, PrimaryTable, PrimaryTable, MAX_TAB_NAME_SIZE),
  DTI_MAP_INT(Table, PrimaryTableId, PrimaryTableId),
  DTI_MAP_INT(Table, TableLoggedFlag, TableLoggedFlag),
  DTI_MAP_INT(Table, TableTemporaryFlag, TableTemporaryFlag),
  DTI_MAP_INT(Table, ForceVarPartFlag, ForceVarPartFlag),
  DTI_MAP_INT(Table, TableKValue, TableKValue),
  DTI_MAP_INT(Table, MinLoadFactor, MinLoadFactor),
  DTI_MAP_INT(Table, MaxLoadFactor, MaxLoadFactor),
  DTI_MAP_INT(Table, FragmentTypeVal, FragmentType),
  DTI_MAP_INT(Table, TableTypeVal, TableType),
  DTI_MAP_INT(Table, NoOfKeyAttr, NoOfKeyAttr),
  DTI_MAP_INT(Table, NoOfAttributes, NoOfAttributes),
  DTI_MAP_INT(Table, NoOfNullable, NoOfNullable),
  DTI_MAP_INT(Table, NoOfVariable, NoOfVariable),
  DTI_MAP_INT(Table, KeyLength, KeyLength),
  DTI_MAP_INT(Table, TableVersion, TableVersion),
  DTI_MAP_INT(Table, IndexState, IndexState),
  DTI_MAP_INT(Table, InsertTriggerId, InsertTriggerId),
  DTI_MAP_INT(Table, UpdateTriggerId, UpdateTriggerId),
  DTI_MAP_INT(Table, DeleteTriggerId, DeleteTriggerId),
  DTI_MAP_INT(Table, CustomTriggerId, CustomTriggerId),
  DTI_MAP_BIN_EXTERNAL(FrmData, 0),
  DTI_MAP_BIN_EXTERNAL(MysqlDictMetadata, 0),
  DTI_MAP_INT(Table, PartitionBalance, PartitionBalance),
  DTI_MAP_INT(Table, FragmentCount, FragmentCount),
  DTI_MAP_INT(Table, ReplicaDataLen, ReplicaDataLen),
  DTI_MAP_BIN(Table, ReplicaData, ReplicaData, MAX_FRAGMENT_DATA_BYTES, ReplicaDataLen),
  DTI_MAP_INT(Table, FragmentDataLen, FragmentDataLen),
  DTI_MAP_BIN(Table, FragmentData, FragmentData, 6*MAX_NDB_PARTITIONS, FragmentDataLen),
  DTI_MAP_INT(Table, TablespaceDataLen, TablespaceDataLen),
  DTI_MAP_BIN(Table, TablespaceData, TablespaceData, 8*MAX_NDB_PARTITIONS, TablespaceDataLen),
  DTI_MAP_INT(Table, RangeListDataLen, RangeListDataLen),
  DTI_MAP_BIN(Table, RangeListData, RangeListData, 8*MAX_NDB_PARTITIONS, RangeListDataLen),
  DTI_MAP_INT(Table, TablespaceId, TablespaceId),
  DTI_MAP_INT(Table, TablespaceVersion, TablespaceVersion),
  DTI_MAP_INT(Table, MaxRowsLow, MaxRowsLow),
  DTI_MAP_INT(Table, MaxRowsHigh, MaxRowsHigh),
  DTI_MAP_INT(Table, DefaultNoPartFlag, DefaultNoPartFlag),
  DTI_MAP_INT(Table, LinearHashFlag, LinearHashFlag),
  DTI_MAP_INT(Table, TablespaceVersion, TablespaceVersion),
  DTI_MAP_INT(Table, RowGCIFlag, RowGCIFlag),
  DTI_MAP_INT(Table, RowChecksumFlag, RowChecksumFlag),
  DTI_MAP_INT(Table, MaxRowsLow, MaxRowsLow),
  DTI_MAP_INT(Table, MaxRowsHigh, MaxRowsHigh),
  DTI_MAP_INT(Table, MinRowsLow, MinRowsLow),
  DTI_MAP_INT(Table, MinRowsHigh, MinRowsHigh),
  DTI_MAP_INT(Table, SingleUserMode, SingleUserMode),
  DTI_MAP_INT(Table, HashMapObjectId, HashMapObjectId),
  DTI_MAP_INT(Table, HashMapVersion, HashMapVersion),
  DTI_MAP_INT(Table, TableStorageType, TableStorageType),
  DTI_MAP_INT(Table, ExtraRowGCIBits, ExtraRowGCIBits),
  DTI_MAP_INT(Table, ExtraRowAuthorBits, ExtraRowAuthorBits),
  DTI_MAP_INT(Table, ReadBackupFlag, ReadBackupFlag),
  DTI_MAP_INT(Table, FullyReplicatedFlag, FullyReplicatedFlag),
  DTI_MAP_INT(Table, PartitionCount, PartitionCount),
  DTI_MAP_INT(Table, FullyReplicatedTriggerId, FullyReplicatedTriggerId),
  DTIBREAK(AttributeName)
};

//static 
const Uint32 DictTabInfo::TableMappingSize = 
sizeof(DictTabInfo::TableMapping) / sizeof(SimpleProperties::SP2StructMapping);

//static 
const
SimpleProperties::SP2StructMapping
DictTabInfo::AttributeMapping[] = {
  DTI_MAP_STR(Attribute, AttributeName, AttributeName, MAX_ATTR_NAME_SIZE),
  DTI_MAP_INT(Attribute, AttributeId, AttributeId),
  DTI_MAP_INT(Attribute, AttributeType, AttributeType),
  DTI_MAP_INT(Attribute, AttributeSize, AttributeSize),
  DTI_MAP_INT(Attribute, AttributeArraySize, AttributeArraySize),
  DTI_MAP_INT(Attribute, AttributeArrayType, AttributeArrayType),
  DTI_MAP_INT(Attribute, AttributeKeyFlag, AttributeKeyFlag),
  DTI_MAP_INT(Attribute, AttributeNullableFlag, AttributeNullableFlag),
  DTI_MAP_INT(Attribute, AttributeDKey, AttributeDKey),
  DTI_MAP_INT(Attribute, AttributeStorageType, AttributeStorageType),
  DTI_MAP_INT(Attribute, AttributeDynamic, AttributeDynamic),
  DTI_MAP_INT(Attribute, AttributeExtType, AttributeExtType),
  DTI_MAP_INT(Attribute, AttributeExtPrecision, AttributeExtPrecision),
  DTI_MAP_INT(Attribute, AttributeExtScale, AttributeExtScale),
  DTI_MAP_INT(Attribute, AttributeExtLength, AttributeExtLength),
  DTI_MAP_INT(Attribute, AttributeAutoIncrement, AttributeAutoIncrement),

  DTI_MAP_INT(Attribute, AttributeDefaultValueLen, AttributeDefaultValueLen),
  DTI_MAP_BIN(Attribute, AttributeDefaultValue, AttributeDefaultValue,
              MAX_ATTR_DEFAULT_VALUE_SIZE, AttributeDefaultValueLen),

  DTIBREAK(AttributeEnd)
};

//static 
const Uint32 DictTabInfo::AttributeMappingSize = 
sizeof(DictTabInfo::AttributeMapping) / 
sizeof(SimpleProperties::SP2StructMapping);

bool printDICTTABINFO(FILE* output,
                      const Uint32* theData,
                      Uint32 len,
                      Uint16 /*receiverBlockNo*/)
{
  //  const DictTabInfo * const sig = (const DictTabInfo *) theData;

  fprintf(output, "Signal data: ");
  Uint32 i = 0;
  while (i < len)
    fprintf(output, "H\'%.8x ", theData[i++]);
  fprintf(output,"\n");
  return true;
}

void
DictTabInfo::Table::init(){
  std::memset(TableName, 0, sizeof(TableName));//TableName[0] = 0;
  TableId = ~0;
  std::memset(PrimaryTable, 0, sizeof(PrimaryTable));//PrimaryTable[0] = 0; // Only used when "index"
  PrimaryTableId = RNIL;
  TableLoggedFlag = 1;
  TableTemporaryFlag = 0;
  ForceVarPartFlag = 0;
  NoOfKeyAttr = 0;
  NoOfAttributes = 0;
  NoOfNullable = 0;
  NoOfVariable = 0;
  TableKValue = 6;
  MinLoadFactor = 78;
  MaxLoadFactor = 80;
  KeyLength = 0;
  FragmentType = DictTabInfo::HashMapPartition;
  TableType = DictTabInfo::UndefTableType;
  TableVersion = 0;
  IndexState = ~0;
  InsertTriggerId = RNIL;
  UpdateTriggerId = RNIL;
  DeleteTriggerId = RNIL;
  CustomTriggerId = RNIL;
  FragmentDataLen = 0;
  ReplicaDataLen = 0;
  RangeListDataLen = 0;
  TablespaceDataLen = 0;
  std::memset(FragmentData, 0, sizeof(FragmentData));
  std::memset(ReplicaData, 0, sizeof(ReplicaData));
  std::memset(RangeListData, 0, sizeof(RangeListData));
  std::memset(TablespaceData, 0, sizeof(TablespaceData));
  PartitionBalance = NDB_PARTITION_BALANCE_FOR_RP_BY_LDM;
  FragmentCount = 0;
  PartitionCount = 0;
  TablespaceId = RNIL;
  TablespaceVersion = ~0;
  MaxRowsLow = 0;
  MaxRowsHigh = 0;
  DefaultNoPartFlag = 1;
  LinearHashFlag = 1;

  RowGCIFlag = ~0;
  RowChecksumFlag = ~0;

  MaxRowsLow = 0;
  MaxRowsHigh = 0;
  MinRowsLow = 0;
  MinRowsHigh = 0;

  SingleUserMode = 0;

  HashMapObjectId = RNIL;
  HashMapVersion = RNIL;

  TableStorageType = NDB_STORAGETYPE_DEFAULT;

  ExtraRowGCIBits = 0;
  ExtraRowAuthorBits = 0;

  ReadBackupFlag = 0;
  FullyReplicatedFlag = 0;
  FullyReplicatedTriggerId = RNIL;
  PartitionCount = 0;
}

void
DictTabInfo::Attribute::init(){
  std::memset(AttributeName, 0, sizeof(AttributeName));//AttributeName[0] = 0;
  AttributeId = 0xFFFF; // ZNIL
  AttributeType = ~0, // deprecated
  AttributeSize = DictTabInfo::a32Bit;
  AttributeArraySize = 1;
  AttributeArrayType = NDB_ARRAYTYPE_FIXED;
  AttributeKeyFlag = 0;
  AttributeNullableFlag = 0;
  AttributeDKey = 0;
  AttributeExtType = DictTabInfo::ExtUnsigned,
  AttributeExtPrecision = 0,
  AttributeExtScale = 0,
  AttributeExtLength = 0,
  AttributeAutoIncrement = false;
  AttributeStorageType = 0;
  AttributeDynamic = 0;                         // Default is not dynamic
  AttributeDefaultValueLen = 0;                 //Default byte sizes of binary default value is 0
  std::memset(AttributeDefaultValue, 0, sizeof(AttributeDefaultValue));
}

//static 
const
SimpleProperties::SP2StructMapping
DictFilegroupInfo::Mapping[] = {
  DFGI_MAP_STR(Filegroup, FilegroupName, FilegroupName, MAX_TAB_NAME_SIZE),
  DFGI_MAP_INT(Filegroup, FilegroupType, FilegroupType),
  DFGI_MAP_INT(Filegroup,  FilegroupId, FilegroupId),
  DFGI_MAP_INT(Filegroup,  FilegroupVersion, FilegroupVersion),

  DFGI_MAP_INT(Filegroup,  TS_ExtentSize,   TS_ExtentSize),
  DFGI_MAP_INT(Filegroup,  TS_LogfileGroupId, TS_LogfileGroupId),
  DFGI_MAP_INT(Filegroup,  TS_LogfileGroupVersion, TS_LogfileGroupVersion),
  DFGI_MAP_INT(Filegroup,  TS_GrowLimit, TS_DataGrow.GrowLimit),
  DFGI_MAP_INT(Filegroup,  TS_GrowSizeHi, TS_DataGrow.GrowSizeHi),
  DFGI_MAP_INT(Filegroup,  TS_GrowSizeLo, TS_DataGrow.GrowSizeLo),
  DFGI_MAP_STR(Filegroup, TS_GrowPattern, TS_DataGrow.GrowPattern, PATH_MAX),
  DFGI_MAP_INT(Filegroup,  TS_GrowMaxSize, TS_DataGrow.GrowMaxSize),

  DFGI_MAP_INT(Filegroup,  LF_UndoBufferSize, LF_UndoBufferSize),
  DFGI_MAP_INT(Filegroup,  LF_UndoGrowLimit, LF_UndoGrow.GrowLimit),
  DFGI_MAP_INT(Filegroup,  LF_UndoGrowSizeHi, LF_UndoGrow.GrowSizeHi),
  DFGI_MAP_INT(Filegroup,  LF_UndoGrowSizeLo, LF_UndoGrow.GrowSizeLo),
  DFGI_MAP_STR(Filegroup, LF_UndoGrowPattern, LF_UndoGrow.GrowPattern, PATH_MAX),
  DFGI_MAP_INT(Filegroup,  LF_UndoGrowMaxSize, LF_UndoGrow.GrowMaxSize),
  DFGI_MAP_INT(Filegroup,  LF_UndoFreeWordsHi, LF_UndoFreeWordsHi),
  DFGI_MAP_INT(Filegroup,  LF_UndoFreeWordsLo, LF_UndoFreeWordsLo),

  DFGIBREAK(FileName)
};

//static 
const Uint32 DictFilegroupInfo::MappingSize = 
sizeof(DictFilegroupInfo::Mapping) / sizeof(SimpleProperties::SP2StructMapping);

//static 
const
SimpleProperties::SP2StructMapping
DictFilegroupInfo::FileMapping[] = {
  DFGI_MAP_STR(File, FileName, FileName, PATH_MAX),
  DFGI_MAP_INT(File, FileType, FileType),
  DFGI_MAP_INT(File, FileId, FileId),
  DFGI_MAP_INT(File, FileVersion, FileVersion),
  DFGI_MAP_INT(File, FileFGroupId, FilegroupId),
  DFGI_MAP_INT(File, FileFGroupVersion, FilegroupVersion),
  DFGI_MAP_INT(File, FileSizeHi, FileSizeHi),
  DFGI_MAP_INT(File, FileSizeLo, FileSizeLo),
  DFGI_MAP_INT(File, FileFreeExtents, FileFreeExtents),
  DFGIBREAK(FileEnd)
};

//static 
const Uint32 DictFilegroupInfo::FileMappingSize = 
sizeof(DictFilegroupInfo::FileMapping) / 
sizeof(SimpleProperties::SP2StructMapping);

void
DictFilegroupInfo::Filegroup::init(){
  std::memset(FilegroupName, 0, sizeof(FilegroupName));
  FilegroupType = ~0;
  FilegroupId = ~0;
  FilegroupVersion = ~0;

  TS_ExtentSize = 0;
  TS_LogfileGroupId = ~0;
  TS_LogfileGroupVersion = ~0;
  TS_DataGrow.GrowLimit = 0;
  TS_DataGrow.GrowSizeHi = 0;
  TS_DataGrow.GrowSizeLo = 0;
  std::memset(TS_DataGrow.GrowPattern, 0, sizeof(TS_DataGrow.GrowPattern));
  TS_DataGrow.GrowMaxSize = 0;
  LF_UndoFreeWordsHi= 0;
  LF_UndoFreeWordsLo= 0;
}

void
DictFilegroupInfo::File::init(){
  std::memset(FileName, 0, sizeof(FileName));
  FileType = ~0;
  FileId = ~0;
  FileVersion = ~0;
  FilegroupId = ~0;
  FilegroupVersion = ~0;
  FileSizeHi = 0;
  FileSizeLo = 0;
  FileFreeExtents = 0;
}

// blob table name hack

bool
DictTabInfo::isBlobTableName(const char* name, Uint32* ptab_id, Uint32* pcol_no)
{ 
  const char* const prefix = "NDB$BLOB_";
  const char* s = strrchr(name, table_name_separator);
  s = (s == nullptr ? name : s + 1);
  if (strncmp(s, prefix, strlen(prefix)) != 0)
    return false;
  s += strlen(prefix);
  uint i, n;
  for (i = 0, n = 0; '0' <= s[i] && s[i] <= '9'; i++)
    n = 10 * n + (s[i] - '0');
  if (i == 0 || s[i] != '_')
    return false;
  const uint tab_id = n;
  s = &s[i + 1];
  for (i = 0, n = 0; '0' <= s[i] && s[i] <= '9'; i++)
    n = 10 * n + (s[i] - '0');
  if (i == 0 || s[i] != 0)
    return false;
  const uint col_no = n;
  if (ptab_id)
    *ptab_id = tab_id;
  if (pcol_no)
    *pcol_no = col_no;
  return true;
}

/**
 * HashMap
 */
const
SimpleProperties::SP2StructMapping
DictHashMapInfo::Mapping[] = {
  DHMI_MAP_STR(HashMap, HashMapName, HashMapName, MAX_TAB_NAME_SIZE),
  DHMI_MAP_INT(HashMap, HashMapBuckets, HashMapBuckets),
  DTI_MAP_INT(HashMap, HashMapObjectId, HashMapObjectId),
  DTI_MAP_INT(HashMap, HashMapVersion, HashMapVersion),

  /**
   * This *should* change to Uint16 or similar once endian is pushed
   */
  DHMI_MAP_BIN(HashMap, HashMapValues, HashMapValues,
              NDB_MAX_HASHMAP_BUCKETS * sizeof(Uint16), HashMapBuckets)
};

//static
const Uint32 DictHashMapInfo::MappingSize =
  sizeof(DictHashMapInfo::Mapping) / sizeof(SimpleProperties::SP2StructMapping);


void
DictHashMapInfo::HashMap::init()
{
  std::memset(this, 0, sizeof(* this));
}

/**
 * ForeignKey
 */
const
SimpleProperties::SP2StructMapping
DictForeignKeyInfo::Mapping[] = {
  DFKI_MAP_STR(ForeignKey, ForeignKeyName, Name, MAX_TAB_NAME_SIZE),
  DFKI_MAP_STR(ForeignKey, ForeignKeyParentTableName, ParentTableName, MAX_TAB_NAME_SIZE),
  DFKI_MAP_STR(ForeignKey, ForeignKeyParentIndexName, ParentIndexName, MAX_TAB_NAME_SIZE),
  DFKI_MAP_STR(ForeignKey, ForeignKeyChildTableName, ChildTableName, MAX_TAB_NAME_SIZE),
  DFKI_MAP_STR(ForeignKey, ForeignKeyChildIndexName, ChildIndexName, MAX_TAB_NAME_SIZE),
  DFKI_MAP_INT(ForeignKey, ForeignKeyId, ForeignKeyId),
  DFKI_MAP_INT(ForeignKey, ForeignKeyVersion, ForeignKeyVersion),
  DFKI_MAP_INT(ForeignKey, ForeignKeyParentTableId, ParentTableId),
  DFKI_MAP_INT(ForeignKey, ForeignKeyParentTableVersion, ParentTableVersion),
  DFKI_MAP_INT(ForeignKey, ForeignKeyChildTableId, ChildTableId),
  DFKI_MAP_INT(ForeignKey, ForeignKeyChildTableVersion, ChildTableVersion),
  DFKI_MAP_INT(ForeignKey, ForeignKeyParentIndexId, ParentIndexId),
  DFKI_MAP_INT(ForeignKey, ForeignKeyParentIndexVersion, ParentIndexVersion),
  DFKI_MAP_INT(ForeignKey, ForeignKeyChildIndexId, ChildIndexId),
  DFKI_MAP_INT(ForeignKey, ForeignKeyChildIndexVersion, ChildIndexVersion),
  DFKI_MAP_INT(ForeignKey, ForeignKeyOnUpdateAction, OnUpdateAction),
  DFKI_MAP_INT(ForeignKey, ForeignKeyOnDeleteAction, OnDeleteAction),

  DFKI_MAP_INT(ForeignKey, ForeignKeyParentColumnsLength, ParentColumnsLength),
  DFKI_MAP_BIN(ForeignKey, ForeignKeyParentColumns, ParentColumns,
               4*MAX_ATTRIBUTES_IN_INDEX, ParentColumnsLength),

  DFKI_MAP_INT(ForeignKey, ForeignKeyChildColumnsLength, ChildColumnsLength),
  DFKI_MAP_BIN(ForeignKey, ForeignKeyChildColumns, ChildColumns,
               4*MAX_ATTRIBUTES_IN_INDEX, ChildColumnsLength)
};

//static
const Uint32 DictForeignKeyInfo::MappingSize =
  sizeof(DictForeignKeyInfo::Mapping) / sizeof(SimpleProperties::SP2StructMapping);


void
DictForeignKeyInfo::ForeignKey::init()
{
  std::memset(Name, 0, sizeof(Name));
  std::memset(ParentTableName, 0, sizeof(ParentTableName));
  std::memset(ParentIndexName, 0, sizeof(ParentIndexName));
  std::memset(ChildTableName, 0, sizeof(ChildTableName));
  std::memset(ChildIndexName, 0, sizeof(ChildIndexName));
  ForeignKeyId = RNIL;
  ForeignKeyVersion = RNIL;
  ParentTableId = RNIL;
  ParentTableVersion = RNIL;
  ChildTableId = RNIL;
  ChildTableVersion = RNIL;
  ParentIndexId = RNIL;
  ParentIndexVersion = RNIL;
  ChildIndexId = RNIL;
  ChildIndexVersion = RNIL;
  OnUpdateAction = NDB_FK_NO_ACTION;
  OnDeleteAction = NDB_FK_NO_ACTION;
  ParentColumnsLength = 0;
  ChildColumnsLength = 0;
}

void
ndbout_print(const DictForeignKeyInfo::ForeignKey& fk, char* buf, size_t sz)
{
  BaseString::snprintf(buf, sz,
    "fk: name:%s id:%u"
    " parent table: name:%s id:%u"
    " parent index: name:%s id:%u"
    " child table: name:%s id:%u"
    " child index: name:%s id:%u",
    fk.Name, fk.ForeignKeyId,
    fk.ParentTableName, fk.ParentTableId,
    fk.ParentIndexName, fk.ParentIndexId,
    fk.ChildTableName, fk.ChildTableId,
    fk.ChildIndexName, fk.ChildIndexId);
}

NdbOut&
operator<<(NdbOut& out, const DictForeignKeyInfo::ForeignKey& fk)
{
  char buf[2048];
  ndbout_print(fk, buf, sizeof(buf));
  out << buf;
  return out;
}

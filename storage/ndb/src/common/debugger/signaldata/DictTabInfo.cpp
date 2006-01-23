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

#include <signaldata/DictTabInfo.hpp>
#include <ndb_limits.h>

//static 
const
SimpleProperties::SP2StructMapping
DictTabInfo::TableMapping[] = {
  DTIMAPS(Table, TableName, TableName, 0, MAX_TAB_NAME_SIZE),
  DTIMAP(Table, TableId, TableId),
  DTIMAPS(Table, PrimaryTable, PrimaryTable, 0, MAX_TAB_NAME_SIZE),
  DTIMAP(Table, PrimaryTableId, PrimaryTableId),
  DTIMAP2(Table, TableLoggedFlag, TableLoggedFlag, 0, 1),
  DTIMAP2(Table, TableKValue, TableKValue,         6, 6),
  DTIMAP2(Table, MinLoadFactor, MinLoadFactor,     0, 90),
  DTIMAP2(Table, MaxLoadFactor, MaxLoadFactor,    25, 110),
  DTIMAP2(Table, FragmentTypeVal, FragmentType,       0, 3),
  DTIMAP2(Table, TableTypeVal, TableType,         1, 3),
  DTIMAP(Table, NoOfKeyAttr, NoOfKeyAttr),
  DTIMAP2(Table, NoOfAttributes, NoOfAttributes, 1, MAX_ATTRIBUTES_IN_TABLE),
  DTIMAP(Table, NoOfNullable, NoOfNullable),
  DTIMAP2(Table, NoOfVariable, NoOfVariable,       0, 0),
  DTIMAP(Table, KeyLength, KeyLength),
  DTIMAP(Table, TableVersion, TableVersion),
  DTIMAP(Table, IndexState, IndexState),
  DTIMAP(Table, InsertTriggerId, InsertTriggerId),
  DTIMAP(Table, UpdateTriggerId, UpdateTriggerId),
  DTIMAP(Table, DeleteTriggerId, DeleteTriggerId),
  DTIMAP(Table, CustomTriggerId, CustomTriggerId),
  DTIMAP2(Table, FrmLen, FrmLen, 0, MAX_FRM_DATA_SIZE),
  DTIMAPB(Table, FrmData, FrmData, 0, MAX_FRM_DATA_SIZE, FrmLen),
  DTIMAP2(Table, FragmentCount, FragmentCount, 0, MAX_NDB_PARTITIONS),
  DTIMAP2(Table, ReplicaDataLen, ReplicaDataLen, 0, 2*MAX_FRAGMENT_DATA_BYTES),
  DTIMAPB(Table, ReplicaData, ReplicaData, 0, 2*MAX_FRAGMENT_DATA_BYTES, ReplicaDataLen),
  DTIMAP2(Table, FragmentDataLen, FragmentDataLen, 0, 6*MAX_NDB_PARTITIONS),
  DTIMAPB(Table, FragmentData, FragmentData, 0, 6*MAX_NDB_PARTITIONS, FragmentDataLen),
  DTIMAP2(Table, TablespaceDataLen, TablespaceDataLen, 0, 8*MAX_NDB_PARTITIONS),
  DTIMAPB(Table, TablespaceData, TablespaceData, 0, 8*MAX_NDB_PARTITIONS, TablespaceDataLen),
  DTIMAP2(Table, RangeListDataLen, RangeListDataLen, 0, 8*MAX_NDB_PARTITIONS),
  DTIMAPB(Table, RangeListData, RangeListData, 0, 8*MAX_NDB_PARTITIONS, RangeListDataLen),
  DTIMAP(Table, TablespaceId, TablespaceId),
  DTIMAP(Table, TablespaceVersion, TablespaceVersion),
  DTIMAP(Table, MaxRowsLow, MaxRowsLow),
  DTIMAP(Table, MaxRowsHigh, MaxRowsHigh),
  DTIMAP(Table, DefaultNoPartFlag, DefaultNoPartFlag),
  DTIMAP(Table, LinearHashFlag, LinearHashFlag),
  DTIMAP(Table, TablespaceVersion, TablespaceVersion),
  DTIMAP(Table, RowGCIFlag, RowGCIFlag),
  DTIMAP(Table, RowChecksumFlag, RowChecksumFlag),
  DTIBREAK(AttributeName)
};

//static 
const Uint32 DictTabInfo::TableMappingSize = 
sizeof(DictTabInfo::TableMapping) / sizeof(SimpleProperties::SP2StructMapping);

//static 
const
SimpleProperties::SP2StructMapping
DictTabInfo::AttributeMapping[] = {
  DTIMAPS(Attribute, AttributeName, AttributeName, 0, MAX_ATTR_NAME_SIZE),
  DTIMAP(Attribute, AttributeId, AttributeId),
  DTIMAP(Attribute, AttributeType, AttributeType),
  DTIMAP2(Attribute, AttributeSize, AttributeSize,     3, 7),
  DTIMAP2(Attribute, AttributeArraySize, AttributeArraySize, 0, 65535),
  DTIMAP2(Attribute, AttributeArrayType, AttributeArrayType, 0, 3),
  DTIMAP2(Attribute, AttributeKeyFlag, AttributeKeyFlag, 0, 1),
  DTIMAP2(Attribute, AttributeNullableFlag, AttributeNullableFlag, 0, 1),
  DTIMAP2(Attribute, AttributeDKey, AttributeDKey, 0, 1),
  DTIMAP2(Attribute, AttributeStorageType, AttributeStorageType, 0, 1),
  DTIMAP(Attribute, AttributeExtType, AttributeExtType),
  DTIMAP(Attribute, AttributeExtPrecision, AttributeExtPrecision),
  DTIMAP(Attribute, AttributeExtScale, AttributeExtScale),
  DTIMAP(Attribute, AttributeExtLength, AttributeExtLength),
  DTIMAP2(Attribute, AttributeAutoIncrement, AttributeAutoIncrement, 0, 1),
  DTIMAPS(Attribute, AttributeDefaultValue, AttributeDefaultValue,
    0, MAX_ATTR_DEFAULT_VALUE_SIZE),
  DTIBREAK(AttributeEnd)
};

//static 
const Uint32 DictTabInfo::AttributeMappingSize = 
sizeof(DictTabInfo::AttributeMapping) / 
sizeof(SimpleProperties::SP2StructMapping);

bool printDICTTABINFO(FILE * output, const Uint32 * theData, 
		      Uint32 len, Uint16 receiverBlockNo)
{
//  const DictTabInfo * const sig = (DictTabInfo *) theData;

  fprintf(output, "Signal data: ");
  Uint32 i = 0;
  while (i < len)
    fprintf(output, "H\'%.8x ", theData[i++]);
  fprintf(output,"\n");
  return true;
}

void
DictTabInfo::Table::init(){
  memset(TableName, 0, sizeof(TableName));//TableName[0] = 0;
  TableId = ~0;
  memset(PrimaryTable, 0, sizeof(PrimaryTable));//PrimaryTable[0] = 0; // Only used when "index"
  PrimaryTableId = RNIL;
  TableLoggedFlag = 1;
  NoOfKeyAttr = 0;
  NoOfAttributes = 0;
  NoOfNullable = 0;
  NoOfVariable = 0;
  TableKValue = 6;
  MinLoadFactor = 78;
  MaxLoadFactor = 80;
  KeyLength = 0;
  FragmentType = DictTabInfo::AllNodesSmallTable;
  TableType = DictTabInfo::UndefTableType;
  TableVersion = 0;
  IndexState = ~0;
  InsertTriggerId = RNIL;
  UpdateTriggerId = RNIL;
  DeleteTriggerId = RNIL;
  CustomTriggerId = RNIL;
  FrmLen = 0;
  FragmentDataLen = 0;
  ReplicaDataLen = 0;
  RangeListDataLen = 0;
  TablespaceDataLen = 0;
  memset(FrmData, 0, sizeof(FrmData));
  memset(FragmentData, 0, sizeof(FragmentData));
  memset(ReplicaData, 0, sizeof(ReplicaData));
  memset(RangeListData, 0, sizeof(RangeListData));
  memset(TablespaceData, 0, sizeof(TablespaceData));
  FragmentCount = 0;
  TablespaceId = RNIL;
  TablespaceVersion = ~0;
  MaxRowsLow = 0;
  MaxRowsHigh = 0;
  DefaultNoPartFlag = 1;
  LinearHashFlag = 1;

  RowGCIFlag = ~0;
  RowChecksumFlag = ~0;
}

void
DictTabInfo::Attribute::init(){
  memset(AttributeName, 0, sizeof(AttributeName));//AttributeName[0] = 0;
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
  memset(AttributeDefaultValue, 0, sizeof(AttributeDefaultValue));//AttributeDefaultValue[0] = 0;
}

//static 
const
SimpleProperties::SP2StructMapping
DictFilegroupInfo::Mapping[] = {
  DFGIMAPS(Filegroup, FilegroupName, FilegroupName, 0, MAX_TAB_NAME_SIZE),
  DFGIMAP2(Filegroup, FilegroupType, FilegroupType, 0, 1),
  DFGIMAP(Filegroup,  FilegroupId, FilegroupId),
  DFGIMAP(Filegroup,  FilegroupVersion, FilegroupVersion),

  DFGIMAP(Filegroup,  TS_ExtentSize,   TS_ExtentSize),
  DFGIMAP(Filegroup,  TS_LogfileGroupId, TS_LogfileGroupId),
  DFGIMAP(Filegroup,  TS_LogfileGroupVersion, TS_LogfileGroupVersion),
  DFGIMAP(Filegroup,  TS_GrowLimit, TS_DataGrow.GrowLimit),
  DFGIMAP(Filegroup,  TS_GrowSizeHi, TS_DataGrow.GrowSizeHi),
  DFGIMAP(Filegroup,  TS_GrowSizeLo, TS_DataGrow.GrowSizeLo),
  DFGIMAPS(Filegroup, TS_GrowPattern, TS_DataGrow.GrowPattern, 0, PATH_MAX),
  DFGIMAP(Filegroup,  TS_GrowMaxSize, TS_DataGrow.GrowMaxSize),

  DFGIMAP(Filegroup,  LF_UndoBufferSize, LF_UndoBufferSize),
  DFGIMAP(Filegroup,  LF_UndoGrowLimit, LF_UndoGrow.GrowLimit),
  DFGIMAP(Filegroup,  LF_UndoGrowSizeHi, LF_UndoGrow.GrowSizeHi),
  DFGIMAP(Filegroup,  LF_UndoGrowSizeLo, LF_UndoGrow.GrowSizeLo),
  DFGIMAPS(Filegroup, LF_UndoGrowPattern, LF_UndoGrow.GrowPattern, 0,PATH_MAX),
  DFGIMAP(Filegroup,  LF_UndoGrowMaxSize, LF_UndoGrow.GrowMaxSize),
  DFGIMAP(Filegroup,  LF_UndoFreeWordsHi, LF_UndoFreeWordsHi),
  DFGIMAP(Filegroup,  LF_UndoFreeWordsLo, LF_UndoFreeWordsLo),

  DFGIBREAK(FileName)
};

//static 
const Uint32 DictFilegroupInfo::MappingSize = 
sizeof(DictFilegroupInfo::Mapping) / sizeof(SimpleProperties::SP2StructMapping);

//static 
const
SimpleProperties::SP2StructMapping
DictFilegroupInfo::FileMapping[] = {
  DFGIMAPS(File, FileName, FileName, 0, PATH_MAX),
  DFGIMAP2(File, FileType, FileType, 0, 1),
  DFGIMAP(File, FileNo, FileNo),
  DFGIMAP(File, FileId, FileId),
  DFGIMAP(File, FileFGroupId, FilegroupId),
  DFGIMAP(File, FileFGroupVersion, FilegroupVersion),
  DFGIMAP(File, FileSizeHi, FileSizeHi),
  DFGIMAP(File, FileSizeLo, FileSizeLo),
  DFGIMAP(File, FileFreeExtents, FileFreeExtents),
  DFGIBREAK(FileEnd)
};

//static 
const Uint32 DictFilegroupInfo::FileMappingSize = 
sizeof(DictFilegroupInfo::FileMapping) / 
sizeof(SimpleProperties::SP2StructMapping);

void
DictFilegroupInfo::Filegroup::init(){
  memset(FilegroupName, sizeof(FilegroupName), 0);
  FilegroupType = ~0;
  FilegroupId = ~0;
  FilegroupVersion = ~0;

  TS_ExtentSize = 0;
  TS_LogfileGroupId = ~0;
  TS_LogfileGroupVersion = ~0;
  TS_DataGrow.GrowLimit = 0;
  TS_DataGrow.GrowSizeHi = 0;
  TS_DataGrow.GrowSizeLo = 0;
  memset(TS_DataGrow.GrowPattern, sizeof(TS_DataGrow.GrowPattern), 0);
  TS_DataGrow.GrowMaxSize = 0;
}

void
DictFilegroupInfo::File::init(){
  memset(FileName, sizeof(FileName), 0);
  FileType = ~0;
  FileNo = ~0;
  FileId = ~0;
  FilegroupId = ~0;
  FilegroupVersion = ~0;
  FileSizeHi = 0;
  FileSizeLo = 0;
  FileFreeExtents = 0;
}

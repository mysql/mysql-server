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
  DTIMAP(Table, SecondTableId, SecondTableId),
  DTIMAPS(Table, PrimaryTable, PrimaryTable, 0, MAX_TAB_NAME_SIZE),
  DTIMAP(Table, PrimaryTableId, PrimaryTableId),
  DTIMAP2(Table, TableLoggedFlag, TableLoggedFlag, 0, 1),
  DTIMAP2(Table, TableKValue, TableKValue,         6, 6),
  DTIMAP2(Table, MinLoadFactor, MinLoadFactor,     0, 90),
  DTIMAP2(Table, MaxLoadFactor, MaxLoadFactor,    25, 110),
  DTIMAP2(Table, FragmentTypeVal, FragmentType,       0, 3),
  DTIMAP2(Table, TableStorageVal, TableStorage,       0, 0),
  DTIMAP2(Table, ScanOptimised, ScanOptimised,     0, 0),
  DTIMAP2(Table, FragmentKeyTypeVal, FragmentKeyType, 0, 2),
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
  DTIMAP(Table, FragmentCount, FragmentCount),
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
  DTIMAP2(Attribute, AttributeType, AttributeType,    0, 3),
  DTIMAP2(Attribute, AttributeSize, AttributeSize,     3, 7),
  DTIMAP2(Attribute, AttributeArraySize, AttributeArraySize, 0, 65535),
  DTIMAP2(Attribute, AttributeKeyFlag, AttributeKeyFlag, 0, 1),
  DTIMAP2(Attribute, AttributeStorage, AttributeStorage, 0, 0),
  DTIMAP2(Attribute, AttributeNullableFlag, AttributeNullableFlag, 0, 1),
  DTIMAP2(Attribute, AttributeDGroup, AttributeDGroup, 0, 1),
  DTIMAP2(Attribute, AttributeDKey, AttributeDKey, 0, 1),
  DTIMAP2(Attribute, AttributeStoredInd, AttributeStoredInd, 0, 1),
  DTIMAP2(Attribute, AttributeGroup, AttributeGroup, 0, 0),
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
  SecondTableId = ~0;
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
  TableStorage = 0;
  ScanOptimised = 0;
  FragmentKeyType = DictTabInfo::PrimaryKey;
  TableType = DictTabInfo::UndefTableType;
  TableVersion = 0;
  IndexState = ~0;
  InsertTriggerId = RNIL;
  UpdateTriggerId = RNIL;
  DeleteTriggerId = RNIL;
  CustomTriggerId = RNIL;
  FrmLen = 0;
  memset(FrmData, 0, sizeof(FrmData));
  FragmentCount = 0;
}

void
DictTabInfo::Attribute::init(){
  memset(AttributeName, 0, sizeof(AttributeName));//AttributeName[0] = 0;
  AttributeId = 0;
  AttributeType = DictTabInfo::UnSignedType;
  AttributeSize = DictTabInfo::a32Bit;
  AttributeArraySize = 1;
  AttributeKeyFlag = 0;
  AttributeStorage = 1;
  AttributeNullableFlag = 0;
  AttributeDGroup = 0;
  AttributeDKey = 0;
  AttributeStoredInd = 1;
  AttributeGroup = 0;
  AttributeExtType = 0,
  AttributeExtPrecision = 0,
  AttributeExtScale = 0,
  AttributeExtLength = 0,
  AttributeAutoIncrement = false;
  memset(AttributeDefaultValue, 0, sizeof(AttributeDefaultValue));//AttributeDefaultValue[0] = 0;
};

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

#ifndef DICT_TAB_INFO_HPP
#define DICT_TAB_INFO_HPP

#include "SignalData.hpp"
#include <AttributeDescriptor.hpp>
#include <SimpleProperties.hpp>
#include <ndb_limits.h>
#include <trigger_definitions.h>
#include <NdbSqlUtil.hpp>

#define DTIMAP(x, y, z) \
  { DictTabInfo::y, offsetof(x, z), SimpleProperties::Uint32Value, 0, (~0), 0 }

#define DTIMAP2(x, y, z, u, v) \
  { DictTabInfo::y, offsetof(x, z), SimpleProperties::Uint32Value, u, v, 0 }

#define DTIMAPS(x, y, z, u, v) \
  { DictTabInfo::y, offsetof(x, z), SimpleProperties::StringValue, u, v, 0 }

#define DTIMAPB(x, y, z, u, v, l) \
  { DictTabInfo::y, offsetof(x, z), SimpleProperties::BinaryValue, u, v, \
                     offsetof(x, l) }

#define DTIBREAK(x) \
  { DictTabInfo::x, 0, SimpleProperties::InvalidValue, 0, 0 }

class DictTabInfo {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Backup;
  friend class Dbdict;
  friend class Ndbcntr;
  friend class Trix;
  friend class DbUtil;
  // API
  friend class Table;
  friend class NdbSchemaOp;
  
  /**
   * For printing
   */
  friend bool printDICTTABINFO(FILE * output, 
                               const Uint32 * theData, 
                               Uint32 len, 
                               Uint16 receiverBlockNo);

public:
  enum RequestType {
    CreateTableFromAPI  = 1,
    AddTableFromDict    = 2, // Between DICT's
    CopyTable           = 3, // Between DICT's
    ReadTableFromDiskSR = 4, // Local in DICT
    GetTabInfoConf      = 5,
    AlterTableFromAPI  = 6
  };

  enum KeyValues {
    TableName          = 1,  // String, Mandatory
    TableId            = 2,  //Mandatory between DICT's otherwise not allowed
    TableVersion       = 3,  //Mandatory between DICT's otherwise not allowed
    TableLoggedFlag    = 4,  //Default Logged
    NoOfKeyAttr        = 5,  //Default 1
    NoOfAttributes     = 6,  //Mandatory
    NoOfNullable       = 7,  //Deafult 0
    NoOfVariable       = 8,  //Default 0
    TableKValue        = 9,  //Default 6
    MinLoadFactor      = 10, //Default 70
    MaxLoadFactor      = 11, //Default 80
    KeyLength          = 12, //Default 1  (No of words in primary key)
    FragmentTypeVal    = 13, //Default AllNodesSmallTable
    TableStorageVal    = 14, //Default StorageType::MainMemory
    ScanOptimised      = 15, //Default updateOptimised
    FragmentKeyTypeVal = 16, //Default PrimaryKey
    SecondTableId      = 17, //Mandatory between DICT's otherwise not allowed
    TableTypeVal       = 18, //Default TableType::UserTable
    PrimaryTable       = 19, //Mandatory for index otherwise RNIL
    PrimaryTableId     = 20, //ditto
    IndexState         = 21,
    InsertTriggerId    = 22,
    UpdateTriggerId    = 23,
    DeleteTriggerId    = 24,
    CustomTriggerId    = 25,
    FrmLen             = 26,
    FrmData            = 27,
    FragmentCount      = 128, // No of fragments in table (!fragment replicas)
    TableEnd           = 999,
    
    AttributeName          = 1000, // String, Mandatory
    AttributeId        = 1001, //Mandatory between DICT's otherwise not allowed
    AttributeType          = 1002, //Default UnSignedType
    AttributeSize          = 1003, //Default DictTabInfo::a32Bit
    AttributeArraySize     = 1005, //Default 1
    AttributeKeyFlag       = 1006, //Default noKey
    AttributeStorage       = 1007, //Default MainMemory
    AttributeNullableFlag  = 1008, //Default NotNullable
    AttributeDGroup        = 1009, //Default NotDGroup
    AttributeDKey          = 1010, //Default NotDKey
    AttributeStoredInd     = 1011, //Default NotStored
    AttributeGroup         = 1012, //Default 0
    AttributeExtType       = 1013, //Default 0 (undefined)
    AttributeExtPrecision  = 1014, //Default 0
    AttributeExtScale      = 1015, //Default 0
    AttributeExtLength     = 1016, //Default 0
    AttributeAutoIncrement = 1017, //Default false
    AttributeDefaultValue  = 1018, //Default value (printable string)
    AttributeEnd           = 1999  //
  };
  // ----------------------------------------------------------------------
  // Part of the protocol is that we only transfer parameters which do not
  // have a default value. Thus the default values are part of the protocol.
  // ----------------------------------------------------------------------

  // FragmentKeyType constants
  enum FragmentKeyType { 
    PrimaryKey = 0,
    DistributionKey = 1,
    DistributionGroup = 2
  };
  
  // FragmentType constants
  enum FragmentType {
    AllNodesSmallTable = 0,
    AllNodesMediumTable = 1,
    AllNodesLargeTable = 2,
    SingleFragment = 3
  };
  
  // TableStorage AND AttributeStorage constants
  enum StorageType {
    MainMemory = 0,
    DiskMemory = 1
  };

  // TableType constants + objects
  enum TableType {
    UndefTableType = 0,
    SystemTable = 1,
    UserTable = 2,
    UniqueHashIndex = 3,
    HashIndex = 4,
    UniqueOrderedIndex = 5,
    OrderedIndex = 6,
    // constant 10 hardcoded in Dbdict.cpp
    HashIndexTrigger = 10 + TriggerType::SECONDARY_INDEX,
    SubscriptionTrigger = 10 + TriggerType::SUBSCRIPTION,
    ReadOnlyConstraint = 10 + TriggerType::READ_ONLY_CONSTRAINT,
    IndexTrigger = 10 + TriggerType::ORDERED_INDEX
  };
  static inline bool
  isTable(int tableType) {
    return
      tableType == SystemTable ||
      tableType == UserTable;
  }
  static inline bool
  isIndex(int tableType) {
    return
      tableType == UniqueHashIndex ||
      tableType == HashIndex ||
      tableType == UniqueOrderedIndex ||
      tableType == OrderedIndex;
  }
  static inline bool
  isUniqueIndex(int tableType) {
    return
      tableType == UniqueHashIndex ||
      tableType == UniqueOrderedIndex;
  }
  static inline bool
  isNonUniqueIndex(int tableType) {
    return
      tableType == HashIndex ||
      tableType == OrderedIndex;
  }
  static inline bool
  isHashIndex(int tableType) {
    return
      tableType == UniqueHashIndex ||
      tableType == HashIndex;
  }
  static inline bool
  isOrderedIndex(int tableType) {
    return
      tableType == UniqueOrderedIndex ||
      tableType == OrderedIndex;
  }

  // Object state for translating from/to API
  enum ObjectState {
    StateUndefined = 0,
    StateOffline = 1,
    StateBuilding = 2,
    StateDropping = 3,
    StateOnline = 4,
    StateBroken = 9
  };

  // Object store for translating from/to API
  enum ObjectStore {
    StoreUndefined = 0,
    StoreTemporary = 1,
    StorePermanent = 2
  };
  
  // ScanOptimised constants
  static const unsigned updateOptimised = 0;
  static const unsigned scanOptimised = 1;
  
  // AttributeType constants
  static const unsigned SignedType = 0;
  static const unsigned UnSignedType = 1;
  static const unsigned FloatingPointType = 2;
  static const unsigned StringType = 3;
  
  // AttributeSize constants
  static const unsigned an8Bit = 3;
  static const unsigned a16Bit = 4;
  static const unsigned a32Bit = 5;
  static const unsigned a64Bit = 6;
  static const unsigned a128Bit = 7;
  
  // AttributeDGroup constants
  static const unsigned NotDGroup = 0;
  static const unsigned DGroup = 1;
  
  // AttributeDKey constants
  static const unsigned NotDKey = 0;
  static const unsigned DKey = 1;
  
  // AttributeStoredInd constants
  static const unsigned NotStored = 0;
  static const unsigned Stored = 1;
 
  // Table data interpretation
  struct Table {
    char   TableName[MAX_TAB_NAME_SIZE];
    Uint32 TableId;
    Uint32 SecondTableId;
    char   PrimaryTable[MAX_TAB_NAME_SIZE]; // Only used when "index"
    Uint32 PrimaryTableId;
    Uint32 TableLoggedFlag;
    Uint32 NoOfKeyAttr;
    Uint32 NoOfAttributes;
    Uint32 NoOfNullable;
    Uint32 NoOfVariable;
    Uint32 TableKValue;
    Uint32 MinLoadFactor;
    Uint32 MaxLoadFactor;
    Uint32 KeyLength;
    Uint32 FragmentType;
    Uint32 TableStorage;
    Uint32 ScanOptimised;
    Uint32 FragmentKeyType;
    Uint32 TableType;
    Uint32 TableVersion;
    Uint32 IndexState;
    Uint32 InsertTriggerId;
    Uint32 UpdateTriggerId;
    Uint32 DeleteTriggerId;
    Uint32 CustomTriggerId;
    Uint32 FrmLen;
    char   FrmData[MAX_FRM_DATA_SIZE];
    Uint32 FragmentCount;

    void init();
  };

  static const
  SimpleProperties::SP2StructMapping TableMapping[];

  static const Uint32 TableMappingSize;

  // AttributeExtType values
  enum ExtType {
    ExtUndefined = NdbSqlUtil::Type::Undefined,
    ExtTinyint = NdbSqlUtil::Type::Tinyint,
    ExtTinyunsigned = NdbSqlUtil::Type::Tinyunsigned,
    ExtSmallint = NdbSqlUtil::Type::Smallint,
    ExtSmallunsigned = NdbSqlUtil::Type::Smallunsigned,
    ExtMediumint = NdbSqlUtil::Type::Mediumint,
    ExtMediumunsigned = NdbSqlUtil::Type::Mediumunsigned,
    ExtInt = NdbSqlUtil::Type::Int,
    ExtUnsigned = NdbSqlUtil::Type::Unsigned,
    ExtBigint = NdbSqlUtil::Type::Bigint,
    ExtBigunsigned = NdbSqlUtil::Type::Bigunsigned,
    ExtFloat = NdbSqlUtil::Type::Float,
    ExtDouble = NdbSqlUtil::Type::Double,
    ExtDecimal = NdbSqlUtil::Type::Decimal,
    ExtChar = NdbSqlUtil::Type::Char,
    ExtVarchar = NdbSqlUtil::Type::Varchar,
    ExtBinary = NdbSqlUtil::Type::Binary,
    ExtVarbinary = NdbSqlUtil::Type::Varbinary,
    ExtDatetime = NdbSqlUtil::Type::Datetime,
    ExtTimespec = NdbSqlUtil::Type::Timespec,
    ExtBlob = NdbSqlUtil::Type::Blob,
    ExtText = NdbSqlUtil::Type::Text
  };

  // Attribute data interpretation
  struct Attribute {
    char   AttributeName[MAX_TAB_NAME_SIZE];
    Uint32 AttributeId;
    Uint32 AttributeType;
    Uint32 AttributeSize;
    Uint32 AttributeArraySize;
    Uint32 AttributeKeyFlag;
    Uint32 AttributeStorage;
    Uint32 AttributeNullableFlag;
    Uint32 AttributeDGroup;
    Uint32 AttributeDKey;
    Uint32 AttributeStoredInd;
    Uint32 AttributeGroup;
    Uint32 AttributeExtType;
    Uint32 AttributeExtPrecision;
    Uint32 AttributeExtScale;
    Uint32 AttributeExtLength;
    Uint32 AttributeAutoIncrement;
    char   AttributeDefaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];
    
    void init();

    inline
    Uint32 sizeInWords()
    {
      return ((1 << AttributeSize) * AttributeArraySize + 31) >> 5;
    }

    // translate to old kernel types and sizes
    inline bool
    translateExtType() {
      switch (AttributeExtType) {
      case DictTabInfo::ExtUndefined:
        break;
      case DictTabInfo::ExtTinyint:
        AttributeType = DictTabInfo::SignedType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength;
	return true;
      case DictTabInfo::ExtTinyunsigned:
        AttributeType = DictTabInfo::UnSignedType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength;
	return true;
      case DictTabInfo::ExtSmallint:
        AttributeType = DictTabInfo::SignedType;
        AttributeSize = DictTabInfo::a16Bit;
        AttributeArraySize = AttributeExtLength;
	return true;
      case DictTabInfo::ExtSmallunsigned:
        AttributeType = DictTabInfo::UnSignedType;
        AttributeSize = DictTabInfo::a16Bit;
        AttributeArraySize = AttributeExtLength;
	return true;
      case DictTabInfo::ExtMediumint:
        AttributeType = DictTabInfo::SignedType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 3 * AttributeExtLength;
	return true;
      case DictTabInfo::ExtMediumunsigned:
        AttributeType = DictTabInfo::UnSignedType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 3 * AttributeExtLength;
	return true;
      case DictTabInfo::ExtInt:	
        AttributeType = DictTabInfo::SignedType;
        AttributeSize = DictTabInfo::a32Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtUnsigned:
        AttributeType = DictTabInfo::UnSignedType;
        AttributeSize = DictTabInfo::a32Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtBigint:
        AttributeType = DictTabInfo::SignedType;
        AttributeSize = DictTabInfo::a64Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtBigunsigned:
        AttributeType = DictTabInfo::UnSignedType;
        AttributeSize = DictTabInfo::a64Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtFloat:
        AttributeType = DictTabInfo::FloatingPointType;
        AttributeSize = DictTabInfo::a32Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtDouble:
        AttributeType = DictTabInfo::FloatingPointType;
        AttributeSize = DictTabInfo::a64Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtDecimal:
        // not yet implemented anywhere
        break;
      case DictTabInfo::ExtChar:
      case DictTabInfo::ExtBinary:
        AttributeType = DictTabInfo::StringType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength;
        return true;
      case DictTabInfo::ExtVarchar:
      case DictTabInfo::ExtVarbinary:
        AttributeType = DictTabInfo::StringType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength + 2;
        return true;
      case DictTabInfo::ExtDatetime:
        AttributeType = DictTabInfo::StringType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 8 * AttributeExtLength;
        return true;
      case DictTabInfo::ExtTimespec:
        AttributeType = DictTabInfo::StringType;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 12 * AttributeExtLength;
        return true;
      case DictTabInfo::ExtBlob:
      case DictTabInfo::ExtText:
        AttributeType = DictTabInfo::StringType;
        AttributeSize = DictTabInfo::an8Bit;
        // head + inline part [ attr precision ]
        AttributeArraySize = (NDB_BLOB_HEAD_SIZE << 2) + AttributeExtPrecision;
        return true;
      };
      return false;
    }
    
    inline void print(FILE *out) {
      fprintf(out, "AttributeId = %d\n", AttributeId);
      fprintf(out, "AttributeType = %d\n", AttributeType);
      fprintf(out, "AttributeSize = %d\n", AttributeSize);
      fprintf(out, "AttributeArraySize = %d\n", AttributeArraySize);
      fprintf(out, "AttributeKeyFlag = %d\n", AttributeKeyFlag);
      fprintf(out, "AttributeStorage = %d\n", AttributeStorage);
      fprintf(out, "AttributeNullableFlag = %d\n", AttributeNullableFlag);
      fprintf(out, "AttributeDGroup = %d\n", AttributeDGroup);
      fprintf(out, "AttributeDKey = %d\n", AttributeDKey);
      fprintf(out, "AttributeStoredInd = %d\n", AttributeStoredInd);
      fprintf(out, "AttributeGroup = %d\n", AttributeGroup);
      fprintf(out, "AttributeAutoIncrement = %d\n", AttributeAutoIncrement);
      fprintf(out, "AttributeExtType = %d\n", AttributeExtType);
      fprintf(out, "AttributeExtPrecision = %d\n", AttributeExtPrecision);
      fprintf(out, "AttributeExtScale = %d\n", AttributeExtScale);
      fprintf(out, "AttributeExtLength = %d\n", AttributeExtLength);
      fprintf(out, "AttributeDefaultValue = \"%s\"\n",
        AttributeDefaultValue ? AttributeDefaultValue : "");
    }
  };
  
  static const
  SimpleProperties::SP2StructMapping AttributeMapping[];

  static const Uint32 AttributeMappingSize;

  // Signal constants
  STATIC_CONST( DataLength = 20 );
  STATIC_CONST( HeaderLength = 5 );

private:  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 totalLen; 
  Uint32 offset; 
  
  /**
   * Length of this data = signal->length() - HeaderLength
   * Sender block ref = signal->senderBlockRef()
   */
  
  Uint32 tabInfoData[DataLength];
};

#endif

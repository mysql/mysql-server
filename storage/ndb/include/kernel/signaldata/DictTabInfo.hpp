/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DICT_TAB_INFO_HPP
#define DICT_TAB_INFO_HPP

#include "SignalData.hpp"
#include <AttributeDescriptor.hpp>
#include <SimpleProperties.hpp>
#include <ndb_limits.h>
#include <NdbSqlUtil.hpp>
#include <ndb_global.h>

#ifndef my_decimal_h

// sql/my_decimal.h requires many more sql/*.h new to ndb
// for now, copy the bit we need  TODO proper fix

#define DECIMAL_MAX_FIELD_SIZE ((9 * 9) - (2 * 8))

#ifndef DECIMAL_NOT_SPECIFIED
#define DECIMAL_NOT_SPECIFIED 31
#endif

C_MODE_START
extern int decimal_bin_size(int, int);
C_MODE_END

inline int my_decimal_get_binary_size(uint precision, uint scale)
{
  return decimal_bin_size((int)precision, (int)scale);
}

#endif

#define DTIMAP(x, y, z) \
  { DictTabInfo::y, my_offsetof(x, z), SimpleProperties::Uint32Value, 0, (~0), 0 }

#define DTIMAP2(x, y, z, u, v) \
  { DictTabInfo::y, my_offsetof(x, z), SimpleProperties::Uint32Value, u, v, 0 }

#define DTIMAPS(x, y, z, u, v) \
  { DictTabInfo::y, my_offsetof(x, z), SimpleProperties::StringValue, u, v, 0 }

#define DTIMAPB(x, y, z, u, v, l) \
  { DictTabInfo::y, my_offsetof(x, z), SimpleProperties::BinaryValue, u, v, \
                     my_offsetof(x, l) }

#define DTIBREAK(x) \
  { DictTabInfo::x, 0, SimpleProperties::InvalidValue, 0, 0, 0 }

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
    AlterTableFromAPI   = 6
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

    TableTemporaryFlag = 28,  //Default not Temporary
    ForceVarPartFlag   = 29,

    FragmentCount      = 128, // No of fragments in table (!fragment replicas)
    FragmentDataLen    = 129,
    FragmentData       = 130, // CREATE_FRAGMENTATION reply
    TablespaceId       = 131,
    TablespaceVersion  = 132,
    TablespaceDataLen  = 133,
    TablespaceData     = 134,
    RangeListDataLen   = 135,
    RangeListData      = 136,
    ReplicaDataLen     = 137,
    ReplicaData        = 138,
    MaxRowsLow         = 139,
    MaxRowsHigh        = 140,
    DefaultNoPartFlag  = 141,
    LinearHashFlag     = 142,
    MinRowsLow         = 143,
    MinRowsHigh        = 144,

    RowGCIFlag         = 150,
    RowChecksumFlag    = 151,
    
    SingleUserMode     = 152,

    HashMapObjectId    = 153,
    HashMapVersion     = 154,

    TableStorageType   = 155,

    ExtraRowGCIBits    = 156,
    ExtraRowAuthorBits = 157,

    TableEnd           = 999,
    
    AttributeName          = 1000, // String, Mandatory
    AttributeId        = 1001, //Mandatory between DICT's otherwise not allowed
    AttributeType          = 1002, //for osu 4.1->5.0.x
    AttributeSize          = 1003, //Default DictTabInfo::a32Bit
    AttributeArraySize     = 1005, //Default 1
    AttributeKeyFlag       = 1006, //Default noKey
    AttributeStorageType   = 1007, //Default NDB_STORAGETYPE_MEMORY
    AttributeNullableFlag  = 1008, //Default NotNullable
    AttributeDynamic       = 1009, //Default not dynamic
    AttributeDKey          = 1010, //Default NotDKey
    AttributeExtType       = 1013, //Default ExtUnsigned
    AttributeExtPrecision  = 1014, //Default 0
    AttributeExtScale      = 1015, //Default 0
    AttributeExtLength     = 1016, //Default 0
    AttributeAutoIncrement = 1017, //Default false
    AttributeArrayType     = 1019, //Default NDB_ARRAYTYPE_FIXED
    AttributeDefaultValueLen = 1020, //Actual Length saved in AttributeDefaultValue
    /* Default value (Binary type, not printable as string),
     * For backward compatibility, the new keyValue
     * (not use the old keyValue 1018) is added
       when restoring data from low backup version data.
    */
    AttributeDefaultValue = 1021,
    AttributeEnd           = 1999  //
  };
  // ----------------------------------------------------------------------
  // Part of the protocol is that we only transfer parameters which do not
  // have a default value. Thus the default values are part of the protocol.
  // ----------------------------------------------------------------------


  
  // FragmentType constants
  enum FragmentType {
    AllNodesSmallTable = 0,
    AllNodesMediumTable = 1,
    AllNodesLargeTable = 2,
    SingleFragment = 3,
    DistrKeyHash = 4,
    DistrKeyLin = 5,
    UserDefined = 6,
    DistrKeyOrderedIndex = 8, // alias
    HashMapPartition = 9
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
    HashIndexTrigger = 11,
    SubscriptionTrigger = 16,
    ReadOnlyConstraint = 17,
    IndexTrigger = 18,
    ReorgTrigger = 19,
    
    Tablespace = 20,        ///< Tablespace
    LogfileGroup = 21,      ///< Logfile group
    Datafile = 22,          ///< Datafile
    Undofile = 23,          ///< Undofile
    HashMap = 24,

    SchemaTransaction = 30
  };

  // used 1) until type BlobTable added 2) in upgrade code
  static bool
  isBlobTableName(const char* name, Uint32* ptab_id = 0, Uint32* pcol_no = 0);
  
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
  static inline bool
  isTrigger(int tableType) {
    return
      tableType == HashIndexTrigger ||
      tableType == SubscriptionTrigger ||
      tableType == ReadOnlyConstraint ||
      tableType == IndexTrigger ||
      tableType == ReorgTrigger;
  }
  static inline bool
  isFilegroup(int tableType) {
    return
      tableType == Tablespace ||
      tableType == LogfileGroup;
  }

  static inline bool
  isFile(int tableType) {
    return
      tableType == Datafile||
      tableType == Undofile;
  }

  static inline bool
  isHashMap(int tableType) {
    return
      tableType == HashMap;
  }
  
  // Object state for translating from/to API
  enum ObjectState {
    StateUndefined = 0,
    StateOffline = 1,
    StateBuilding = 2,
    StateDropping = 3,
    StateOnline = 4,
    StateBackup = 5,
    StateBroken = 9
  };

  // Object store for translating from/to API
  enum ObjectStore {
    StoreUndefined = 0,
    StoreNotLogged = 1,
    StorePermanent = 2
  };
  
  // AttributeSize constants
  STATIC_CONST( aBit = 0 );
  STATIC_CONST( an8Bit = 3 );
  STATIC_CONST( a16Bit = 4 );
  STATIC_CONST( a32Bit = 5 );
  STATIC_CONST( a64Bit = 6 );
  STATIC_CONST( a128Bit = 7 );
    
  // Table data interpretation
  struct Table {
    char   TableName[MAX_TAB_NAME_SIZE];
    Uint32 TableId;
    char   PrimaryTable[MAX_TAB_NAME_SIZE]; // Only used when "index"
    Uint32 PrimaryTableId;
    Uint32 TableLoggedFlag;
    Uint32 TableTemporaryFlag;
    Uint32 ForceVarPartFlag;
    Uint32 NoOfKeyAttr;
    Uint32 NoOfAttributes;
    Uint32 NoOfNullable;
    Uint32 NoOfVariable;
    Uint32 TableKValue;
    Uint32 MinLoadFactor;
    Uint32 MaxLoadFactor;
    Uint32 KeyLength;
    Uint32 FragmentType;
    Uint32 TableType;
    Uint32 TableVersion;
    Uint32 IndexState;
    Uint32 InsertTriggerId;
    Uint32 UpdateTriggerId;
    Uint32 DeleteTriggerId;
    Uint32 CustomTriggerId;
    Uint32 TablespaceId;
    Uint32 TablespaceVersion;
    Uint32 DefaultNoPartFlag;
    Uint32 LinearHashFlag;
    /*
      TODO RONM:
      We need to replace FRM, Fragment Data, Tablespace Data and in
      very particular RangeListData with dynamic arrays
    */
    Uint32 FrmLen;
    char   FrmData[MAX_FRM_DATA_SIZE];
    Uint32 FragmentCount;
    Uint32 ReplicaDataLen;
    Uint16 ReplicaData[MAX_FRAGMENT_DATA_ENTRIES];
    Uint32 FragmentDataLen;
    Uint16 FragmentData[3*MAX_NDB_PARTITIONS];

    Uint32 MaxRowsLow;
    Uint32 MaxRowsHigh;
    Uint32 MinRowsLow;
    Uint32 MinRowsHigh;

    Uint32 TablespaceDataLen;
    Uint32 TablespaceData[2*MAX_NDB_PARTITIONS];
    Uint32 RangeListDataLen;
    Uint32 RangeListData[2*MAX_NDB_PARTITIONS*2];
    
    Uint32 RowGCIFlag;
    Uint32 RowChecksumFlag;

    Uint32 SingleUserMode;

    Uint32 HashMapObjectId;
    Uint32 HashMapVersion;

    Uint32 TableStorageType;

    Uint32 ExtraRowGCIBits;
    Uint32 ExtraRowAuthorBits;

    Table() {}
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
    ExtOlddecimal = NdbSqlUtil::Type::Olddecimal,
    ExtOlddecimalunsigned = NdbSqlUtil::Type::Olddecimalunsigned,
    ExtDecimal = NdbSqlUtil::Type::Decimal,
    ExtDecimalunsigned = NdbSqlUtil::Type::Decimalunsigned,
    ExtChar = NdbSqlUtil::Type::Char,
    ExtVarchar = NdbSqlUtil::Type::Varchar,
    ExtBinary = NdbSqlUtil::Type::Binary,
    ExtVarbinary = NdbSqlUtil::Type::Varbinary,
    ExtDatetime = NdbSqlUtil::Type::Datetime,
    ExtDate = NdbSqlUtil::Type::Date,
    ExtBlob = NdbSqlUtil::Type::Blob,
    ExtText = NdbSqlUtil::Type::Text,
    ExtBit = NdbSqlUtil::Type::Bit,
    ExtLongvarchar = NdbSqlUtil::Type::Longvarchar,
    ExtLongvarbinary = NdbSqlUtil::Type::Longvarbinary,
    ExtTime = NdbSqlUtil::Type::Time,
    ExtYear = NdbSqlUtil::Type::Year,
    ExtTimestamp = NdbSqlUtil::Type::Timestamp,
    ExtTime2 = NdbSqlUtil::Type::Time2,
    ExtDatetime2 = NdbSqlUtil::Type::Datetime2,
    ExtTimestamp2 = NdbSqlUtil::Type::Timestamp2
  };

  // Attribute data interpretation
  struct Attribute {
    char   AttributeName[MAX_TAB_NAME_SIZE];
    Uint32 AttributeId;
    Uint32 AttributeType; // for osu 4.1->5.0.x
    Uint32 AttributeSize;
    Uint32 AttributeArraySize;
    Uint32 AttributeArrayType;
    Uint32 AttributeKeyFlag;
    Uint32 AttributeNullableFlag;
    Uint32 AttributeDKey;
    Uint32 AttributeExtType;
    Uint32 AttributeExtPrecision;
    Uint32 AttributeExtScale;
    Uint32 AttributeExtLength;
    Uint32 AttributeAutoIncrement;
    Uint32 AttributeStorageType;
    Uint32 AttributeDynamic;
    Uint32 AttributeDefaultValueLen;  //byte sizes
    Uint8  AttributeDefaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];
    
    Attribute() {}
    void init();

    inline
    Uint32 sizeInWords()
    {
      return ((1 << AttributeSize) * AttributeArraySize + 31) >> 5;
    }

    // compute old-sty|e attribute size and array size
    inline bool
    translateExtType() {
      switch (AttributeExtType) {
      case DictTabInfo::ExtUndefined:
        return false;
      case DictTabInfo::ExtTinyint:
      case DictTabInfo::ExtTinyunsigned:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength;
	break;
      case DictTabInfo::ExtSmallint:
      case DictTabInfo::ExtSmallunsigned:
        AttributeSize = DictTabInfo::a16Bit;
        AttributeArraySize = AttributeExtLength;
	break;
      case DictTabInfo::ExtMediumint:
      case DictTabInfo::ExtMediumunsigned:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 3 * AttributeExtLength;
	break;
      case DictTabInfo::ExtInt:	
      case DictTabInfo::ExtUnsigned:
        AttributeSize = DictTabInfo::a32Bit;
        AttributeArraySize = AttributeExtLength;
        break;
      case DictTabInfo::ExtBigint:
      case DictTabInfo::ExtBigunsigned:
        AttributeSize = DictTabInfo::a64Bit;
        AttributeArraySize = AttributeExtLength;
        break;
      case DictTabInfo::ExtFloat:
        AttributeSize = DictTabInfo::a32Bit;
        AttributeArraySize = AttributeExtLength;
        break;
      case DictTabInfo::ExtDouble:
        AttributeSize = DictTabInfo::a64Bit;
        AttributeArraySize = AttributeExtLength;
        break;
      case DictTabInfo::ExtOlddecimal:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize =
          (1 + AttributeExtPrecision + (int(AttributeExtScale) > 0)) *
          AttributeExtLength;
        break;
      case DictTabInfo::ExtOlddecimalunsigned:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize =
          (0 + AttributeExtPrecision + (int(AttributeExtScale) > 0)) *
          AttributeExtLength;
        break;
      case DictTabInfo::ExtDecimal:
      case DictTabInfo::ExtDecimalunsigned:
        {
          // copy from Field_new_decimal ctor
          uint precision = AttributeExtPrecision;
          uint scale = AttributeExtScale;
          if (precision > DECIMAL_MAX_FIELD_SIZE ||
              scale >= DECIMAL_NOT_SPECIFIED)
            return false;
          uint bin_size = my_decimal_get_binary_size(precision, scale);
          AttributeSize = DictTabInfo::an8Bit;
          AttributeArraySize = bin_size * AttributeExtLength;
        }
        break;
      case DictTabInfo::ExtChar:
      case DictTabInfo::ExtBinary:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength;
        break;
      case DictTabInfo::ExtVarchar:
      case DictTabInfo::ExtVarbinary:
        if (AttributeExtLength > 0xff)
          return false;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength + 1;
        break;
      case DictTabInfo::ExtDatetime:
        // to fix
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 8 * AttributeExtLength;
        break;
      case DictTabInfo::ExtDate:
        // to fix
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 3 * AttributeExtLength;
        break;
      case DictTabInfo::ExtBlob:
      case DictTabInfo::ExtText:
        AttributeSize = DictTabInfo::an8Bit;
        if (unlikely(AttributeArrayType == NDB_ARRAYTYPE_FIXED)) {
          // head + inline part (length in precision lower half)
          AttributeArraySize = (NDB_BLOB_V1_HEAD_SIZE << 2) +
                               (AttributeExtPrecision & 0xFFFF);
        } else {
          AttributeArraySize = (NDB_BLOB_V2_HEAD_SIZE << 2) +
                               (AttributeExtPrecision & 0xFFFF);
        }
        break;
      case DictTabInfo::ExtBit:
	AttributeSize = DictTabInfo::aBit;
	AttributeArraySize = AttributeExtLength;
	break;
      case DictTabInfo::ExtLongvarchar:
      case DictTabInfo::ExtLongvarbinary:
        if (AttributeExtLength > 0xffff)
          return false;
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = AttributeExtLength + 2;
        break;
      case DictTabInfo::ExtTime:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 3 * AttributeExtLength;
        break;
      case DictTabInfo::ExtYear:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 1 * AttributeExtLength;
        break;
      case DictTabInfo::ExtTimestamp:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = 4 * AttributeExtLength;
        break;
      // fractional time types, see wl#946
      case DictTabInfo::ExtTime2:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = (3 + (1 + AttributeExtPrecision) / 2)
                             * AttributeExtLength;
        break;
      case DictTabInfo::ExtDatetime2:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = (5 + (1 + AttributeExtPrecision) / 2)
                             * AttributeExtLength;
        break;
      case DictTabInfo::ExtTimestamp2:
        AttributeSize = DictTabInfo::an8Bit;
        AttributeArraySize = (4 + (1 + AttributeExtPrecision) / 2)
                             * AttributeExtLength;
        break;
      default:
        return false;
      };
      return true;
    }
    
    inline void print(FILE *out) {
      fprintf(out, "AttributeId = %d\n", AttributeId);
      fprintf(out, "AttributeType = %d\n", AttributeType);
      fprintf(out, "AttributeSize = %d\n", AttributeSize);
      fprintf(out, "AttributeArraySize = %d\n", AttributeArraySize);
      fprintf(out, "AttributeArrayType = %d\n", AttributeArrayType);
      fprintf(out, "AttributeKeyFlag = %d\n", AttributeKeyFlag);
      fprintf(out, "AttributeStorageType = %d\n", AttributeStorageType);
      fprintf(out, "AttributeNullableFlag = %d\n", AttributeNullableFlag);
      fprintf(out, "AttributeDKey = %d\n", AttributeDKey);
      fprintf(out, "AttributeGroup = %d\n", AttributeGroup);
      fprintf(out, "AttributeAutoIncrement = %d\n", AttributeAutoIncrement);
      fprintf(out, "AttributeExtType = %d\n", AttributeExtType);
      fprintf(out, "AttributeExtPrecision = %d\n", AttributeExtPrecision);
      fprintf(out, "AttributeExtScale = %d\n", AttributeExtScale);
      fprintf(out, "AttributeExtLength = %d\n", AttributeExtLength);
      fprintf(out, "AttributeDefaultValueLen = %d\n",
              AttributeDefaultValueLen);
      fprintf(out, "AttributeDefaultValue: \n");
      for (unsigned int i = 0; i < AttributeDefaultValueLen; i++)
        fprintf(out, "0x%x", AttributeDefaultValue[i]);
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

public:
  enum Deprecated
  {
    AttributeDGroup    = 1009, //Default NotDGroup
    AttributeStoredInd = 1011, //Default NotStored
    TableStorageVal    = 14, //Disk storage specified per attribute
    SecondTableId      = 17, //Mandatory between DICT's otherwise not allowed
    FragmentKeyTypeVal = 16 //Default PrimaryKey
  };
    
  enum Unimplemented
  {
    ScanOptimised      = 15, //Default updateOptimised
    AttributeGroup     = 1012, //Default 0
    FileNo             = 102
  };
};

#define DFGIMAP(x, y, z) \
  { DictFilegroupInfo::y, my_offsetof(x, z), SimpleProperties::Uint32Value, 0, (~0), 0 }

#define DFGIMAP2(x, y, z, u, v) \
  { DictFilegroupInfo::y, my_offsetof(x, z), SimpleProperties::Uint32Value, u, v, 0 }

#define DFGIMAPS(x, y, z, u, v) \
  { DictFilegroupInfo::y, my_offsetof(x, z), SimpleProperties::StringValue, u, v, 0 }

#define DFGIMAPB(x, y, z, u, v, l) \
  { DictFilegroupInfo::y, my_offsetof(x, z), SimpleProperties::BinaryValue, u, v, \
                     my_offsetof(x, l) }

#define DFGIBREAK(x) \
  { DictFilegroupInfo::x, 0, SimpleProperties::InvalidValue, 0, 0, 0 }

struct DictFilegroupInfo {
  enum KeyValues {
    FilegroupName     = 1,
    FilegroupType     = 2,
    FilegroupId       = 3,
    FilegroupVersion  = 4,

    /**
     * File parameters
     */
    FileName          = 100,
    FileType          = 101,
    FileId            = 103,
    FileFGroupId      = 104,
    FileFGroupVersion = 105,
    FileSizeHi        = 106,
    FileSizeLo        = 107,
    FileFreeExtents   = 108,
    FileVersion       = 109,
    FileEnd           = 199, //    

    /**
     * Tablespace parameters
     */
    TS_ExtentSize          = 1000, // specified in bytes
    TS_LogfileGroupId      = 1001, 
    TS_LogfileGroupVersion = 1002, 
    TS_GrowLimit           = 1003, // In bytes
    TS_GrowSizeHi          = 1004,
    TS_GrowSizeLo          = 1005,
    TS_GrowPattern         = 1006,
    TS_GrowMaxSize         = 1007,

    /**
     * Logfile group parameters
     */
    LF_UndoBufferSize  = 2005, // In bytes
    LF_UndoGrowLimit   = 2000, // In bytes
    LF_UndoGrowSizeHi  = 2001,
    LF_UndoGrowSizeLo  = 2002,
    LF_UndoGrowPattern = 2003,
    LF_UndoGrowMaxSize = 2004,
    LF_UndoFreeWordsHi = 2006,
    LF_UndoFreeWordsLo = 2007
  };

  // FragmentType constants
  enum FileTypeValues {
    Datafile = 0,
    Undofile = 1
    //, Redofile
  };
 
  struct GrowSpec {
    Uint32 GrowLimit;
    Uint32 GrowSizeHi;
    Uint32 GrowSizeLo;
    char   GrowPattern[PATH_MAX];
    Uint32 GrowMaxSize;
  };
  
  // Table data interpretation
  struct Filegroup {
    char   FilegroupName[MAX_TAB_NAME_SIZE];
    Uint32 FilegroupType; // ObjType
    Uint32 FilegroupId;
    Uint32 FilegroupVersion;

    union {
      Uint32 TS_ExtentSize;
      Uint32 LF_UndoBufferSize;
    };
    Uint32 TS_LogfileGroupId;
    Uint32 TS_LogfileGroupVersion;
    union {
      GrowSpec TS_DataGrow;
      GrowSpec LF_UndoGrow;
    };
    //GrowSpec LF_RedoGrow;
    Uint32 LF_UndoFreeWordsHi;
    Uint32 LF_UndoFreeWordsLo;
    Filegroup() {}
    void init();
  };
  static const Uint32 MappingSize;
  static const SimpleProperties::SP2StructMapping Mapping[];

  struct File {
    char FileName[PATH_MAX];
    Uint32 FileType;
    Uint32 FileId;
    Uint32 FileVersion;
    Uint32 FilegroupId;
    Uint32 FilegroupVersion;
    Uint32 FileSizeHi;
    Uint32 FileSizeLo;
    Uint32 FileFreeExtents;

    File() {}
    void init();
  };
  static const Uint32 FileMappingSize;
  static const SimpleProperties::SP2StructMapping FileMapping[];
};

#define DHMIMAP(x, y, z) \
  { DictHashMapInfo::y, my_offsetof(x, z), SimpleProperties::Uint32Value, 0, (~0), 0 }

#define DHMIMAP2(x, y, z, u, v) \
  { DictHashMapInfo::y, my_offsetof(x, z), SimpleProperties::Uint32Value, u, v, 0 }

#define DHMIMAPS(x, y, z, u, v) \
  { DictHashMapInfo::y, my_offsetof(x, z), SimpleProperties::StringValue, u, v, 0 }

#define DHMIMAPB(x, y, z, u, v, l) \
  { DictHashMapInfo::y, my_offsetof(x, z), SimpleProperties::BinaryValue, u, v, \
                     my_offsetof(x, l) }

#define DHMIBREAK(x) \
  { DictHashMapInfo::x, 0, SimpleProperties::InvalidValue, 0, 0, 0 }

struct DictHashMapInfo {
  enum KeyValues {
    HashMapName    = 1,
    HashMapBuckets = 2,
    HashMapValues  = 3
  };

  // Table data interpretation
  struct HashMap {
    char   HashMapName[MAX_TAB_NAME_SIZE];
    Uint32 HashMapBuckets;
    Uint16 HashMapValues[NDB_DEFAULT_HASHMAP_BUCKETS];
    Uint32 HashMapObjectId;
    Uint32 HashMapVersion;
    HashMap() {}
    void init();
  };
  static const Uint32 MappingSize;
  static const SimpleProperties::SP2StructMapping Mapping[];
};

#endif

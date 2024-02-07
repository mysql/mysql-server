/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DICT_TAB_INFO_HPP
#define DICT_TAB_INFO_HPP

#include <ndb_global.h>
#include <ndb_limits.h>
#include <AttributeDescriptor.hpp>
#include <NdbSqlUtil.hpp>
#include <SimpleProperties.hpp>
#include "SignalData.hpp"
#include "decimal.h"
#include "sql-common/my_decimal.h"

#define JAM_FILE_ID 87

#define DTI_MAP_INT(x, y, z)                     \
  {                                              \
    DictTabInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::Uint32Value, 0, 0      \
  }

#define DTI_MAP_STR(x, y, z, len)                \
  {                                              \
    DictTabInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::StringValue, len, 0    \
  }

#define DTI_MAP_BIN(x, y, z, len, off)                                    \
  {                                                                       \
    DictTabInfo::y, (unsigned)my_offsetof(x, z),                          \
        SimpleProperties::BinaryValue, len, (unsigned)my_offsetof(x, off) \
  }

#define DTI_MAP_BIN_EXTERNAL(y, len)                       \
  {                                                        \
    DictTabInfo::y, 0, SimpleProperties::BinaryValue, len, \
        SimpleProperties::SP2StructMapping::ExternalData   \
  }

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
  friend class NdbSchemaOp;

  /**
   * For printing
   */
  friend bool printDICTTABINFO(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 receiverBlockNo);

 public:
  enum RequestType {
    CreateTableFromAPI = 1,
    AddTableFromDict = 2,     // Between DICT's
    CopyTable = 3,            // Between DICT's
    ReadTableFromDiskSR = 4,  // Local in DICT
    GetTabInfoConf = 5,
    AlterTableFromAPI = 6
  };

  enum KeyValues {
    TableName = 1,         // String, Mandatory
    TableId = 2,           // Mandatory between DICT's otherwise not allowed
    TableVersion = 3,      // Mandatory between DICT's otherwise not allowed
    TableLoggedFlag = 4,   // Default Logged
    NoOfKeyAttr = 5,       // Default 1
    NoOfAttributes = 6,    // Mandatory
    NoOfNullable = 7,      // Default 0
    NoOfVariable = 8,      // Default 0
    TableKValue = 9,       // Default 6
    MinLoadFactor = 10,    // Default 70
    MaxLoadFactor = 11,    // Default 80
    KeyLength = 12,        // Default 1  (No of words in primary key)
    FragmentTypeVal = 13,  // Default AllNodesSmallTable
    TableTypeVal = 18,     // Default TableType::UserTable
    PrimaryTable = 19,     // Mandatory for index otherwise RNIL
    PrimaryTableId = 20,   // ditto
    IndexState = 21,
    InsertTriggerId = 22,
    UpdateTriggerId = 23,
    DeleteTriggerId = 24,
    CustomTriggerId = 25,
    FrmLen = 26,
    FrmData = 27,
    TableTemporaryFlag = 28,  // Default not Temporary
    ForceVarPartFlag = 29,
    MysqlDictMetadata = 30,

    PartitionBalance = 127,
    FragmentCount = 128,  // No of fragments in table (!fragment replicas)
    FragmentDataLen = 129,
    FragmentData = 130,  // CREATE_FRAGMENTATION reply
    TablespaceId = 131,
    TablespaceVersion = 132,
    TablespaceDataLen = 133,
    TablespaceData = 134,
    RangeListDataLen = 135,
    RangeListData = 136,
    ReplicaDataLen = 137,
    ReplicaData = 138,
    MaxRowsLow = 139,
    MaxRowsHigh = 140,
    DefaultNoPartFlag = 141,
    LinearHashFlag = 142,
    MinRowsLow = 143,
    MinRowsHigh = 144,

    RowGCIFlag = 150,
    RowChecksumFlag = 151,

    SingleUserMode = 152,

    HashMapObjectId = 153,
    HashMapVersion = 154,

    TableStorageType = 155,

    ExtraRowGCIBits = 156,
    ExtraRowAuthorBits = 157,

    ReadBackupFlag = 158,

    FullyReplicatedFlag = 159,
    PartitionCount = 160,
    /**
     * Needed for NR
     */
    FullyReplicatedTriggerId = 161,

    TableEnd = 999,

    AttributeName = 1000,  // String, Mandatory
    AttributeId = 1001,    // Mandatory between DICT's otherwise not allowed
    AttributeType = 1002,  // for osu 4.1->5.0.x
    AttributeSize = 1003,  // Default DictTabInfo::a32Bit
    AttributeArraySize = 1005,      // Default 1
    AttributeKeyFlag = 1006,        // Default noKey
    AttributeStorageType = 1007,    // Default NDB_STORAGETYPE_MEMORY
    AttributeNullableFlag = 1008,   // Default NotNullable
    AttributeDynamic = 1009,        // Default not dynamic
    AttributeDKey = 1010,           // Default NotDKey
    AttributeExtType = 1013,        // Default ExtUnsigned
    AttributeExtPrecision = 1014,   // Default 0
    AttributeExtScale = 1015,       // Default 0
    AttributeExtLength = 1016,      // Default 0
    AttributeAutoIncrement = 1017,  // Default false
    AttributeArrayType = 1019,      // Default NDB_ARRAYTYPE_FIXED
    AttributeDefaultValueLen =
        1020,  // Actual Length saved in AttributeDefaultValue
    /* Default value (Binary type, not printable as string),
     * For backward compatibility, the new keyValue
     * (not use the old keyValue 1018) is added
       when restoring data from low backup version data.
    */
    AttributeDefaultValue = 1021,
    AttributeEnd = 1999  //
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
    DistrKeyOrderedIndex = 8,  // alias
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

    Tablespace = 20,    ///< Tablespace
    LogfileGroup = 21,  ///< Logfile group
    Datafile = 22,      ///< Datafile
    Undofile = 23,      ///< Undofile
    HashMap = 24,

    ForeignKey = 25,  // The definition
    FKParentTrigger = 26,
    FKChildTrigger = 27,

    /**
     * Trigger that propagates DML to all fragments
     */
    FullyReplicatedTrigger = 28,

    SchemaTransaction = 30
  };

  // used 1) until type BlobTable added 2) in upgrade code
  static bool isBlobTableName(const char *name, Uint32 *ptab_id = nullptr,
                              Uint32 *pcol_no = nullptr);

  static inline bool isTable(int tableType) {
    return tableType == SystemTable || tableType == UserTable;
  }
  static inline bool isIndex(int tableType) {
    return tableType == UniqueHashIndex || tableType == HashIndex ||
           tableType == UniqueOrderedIndex || tableType == OrderedIndex;
  }
  static inline bool isUniqueIndex(int tableType) {
    return tableType == UniqueHashIndex || tableType == UniqueOrderedIndex;
  }
  static inline bool isNonUniqueIndex(int tableType) {
    return tableType == HashIndex || tableType == OrderedIndex;
  }
  static inline bool isHashIndex(int tableType) {
    return tableType == UniqueHashIndex || tableType == HashIndex;
  }
  static inline bool isOrderedIndex(int tableType) {
    return tableType == UniqueOrderedIndex || tableType == OrderedIndex;
  }
  static inline bool isTrigger(int tableType) {
    return tableType == HashIndexTrigger || tableType == SubscriptionTrigger ||
           tableType == ReadOnlyConstraint || tableType == IndexTrigger ||
           tableType == ReorgTrigger || tableType == FKParentTrigger ||
           tableType == FKChildTrigger || tableType == FullyReplicatedTrigger;
  }
  static inline bool isFilegroup(int tableType) {
    return tableType == Tablespace || tableType == LogfileGroup;
  }

  static inline bool isFile(int tableType) {
    return tableType == Datafile || tableType == Undofile;
  }

  static inline bool isHashMap(int tableType) { return tableType == HashMap; }

  static inline bool isForeignKey(int tableType) {
    return tableType == ForeignKey;
  }

  // Object state for translating from/to API
  enum ObjectState {
    StateUndefined = 0,
    StateOffline = 1,
    StateBuilding = 2,
    StateDropping = 3,
    StateOnline = 4,
    ObsoleteStateBackup = 5,
    StateBroken = 9
  };

  // Object store for translating from/to API
  enum ObjectStore {
    StoreUndefined = 0,
    StoreNotLogged = 1,
    StorePermanent = 2
  };

  // AttributeSize constants
  static constexpr Uint32 aBit = 0;
  static constexpr Uint32 an8Bit = 3;
  static constexpr Uint32 a16Bit = 4;
  static constexpr Uint32 a32Bit = 5;
  static constexpr Uint32 a64Bit = 6;
  static constexpr Uint32 a128Bit = 7;

  // Table data interpretation
  struct Table {
    char TableName[MAX_TAB_NAME_SIZE];
    Uint32 TableId;
    char PrimaryTable[MAX_TAB_NAME_SIZE];  // Only used when "index"
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
    Uint32 PartitionBalance;
    Uint32 FragmentCount;
    Uint32 ReplicaDataLen;
    Uint16 ReplicaData[MAX_FRAGMENT_DATA_ENTRIES];
    Uint32 FragmentDataLen;
    Uint16 FragmentData[3 * MAX_NDB_PARTITIONS];

    Uint32 MaxRowsLow;
    Uint32 MaxRowsHigh;
    Uint32 MinRowsLow;
    Uint32 MinRowsHigh;

    Uint32 TablespaceDataLen;
    Uint32 TablespaceData[2 * MAX_NDB_PARTITIONS];
    Uint32 RangeListDataLen;
    Uint32 RangeListData[2 * MAX_NDB_PARTITIONS * 2];

    Uint32 RowGCIFlag;
    Uint32 RowChecksumFlag;

    Uint32 SingleUserMode;

    Uint32 HashMapObjectId;
    Uint32 HashMapVersion;

    Uint32 TableStorageType;

    Uint32 ExtraRowGCIBits;
    Uint32 ExtraRowAuthorBits;

    Uint32 ReadBackupFlag;
    Uint32 FullyReplicatedFlag;
    Uint32 FullyReplicatedTriggerId;
    Uint32 PartitionCount;

    Table() {}
    void init();
  };

  static const SimpleProperties::SP2StructMapping TableMapping[];

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
    char AttributeName[MAX_TAB_NAME_SIZE];
    Uint32 AttributeId;
    Uint32 AttributeType;  // for osu 4.1->5.0.x
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
    Uint32 AttributeDefaultValueLen;  // byte sizes
    Uint8 AttributeDefaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];

    Attribute() {}
    void init();

    inline Uint32 sizeInWords() {
      return ((1 << AttributeSize) * AttributeArraySize + 31) >> 5;
    }

    // compute old-sty|e attribute size and array size
    inline bool translateExtType() {
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
        case DictTabInfo::ExtDecimalunsigned: {
          // copy from Field_new_decimal ctor
          uint precision = AttributeExtPrecision;
          uint scale = AttributeExtScale;
          if (precision > DECIMAL_MAX_FIELD_SIZE ||
              scale >= DECIMAL_NOT_SPECIFIED)
            return false;
          uint bin_size = my_decimal_get_binary_size(precision, scale);
          AttributeSize = DictTabInfo::an8Bit;
          AttributeArraySize = bin_size * AttributeExtLength;
        } break;
        case DictTabInfo::ExtChar:
        case DictTabInfo::ExtBinary:
          AttributeSize = DictTabInfo::an8Bit;
          AttributeArraySize = AttributeExtLength;
          break;
        case DictTabInfo::ExtVarchar:
        case DictTabInfo::ExtVarbinary:
          if (AttributeExtLength > 0xff) return false;
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
            AttributeArraySize =
                (NDB_BLOB_V1_HEAD_SIZE << 2) + (AttributeExtPrecision & 0xFFFF);
          } else {
            AttributeArraySize =
                (NDB_BLOB_V2_HEAD_SIZE << 2) + (AttributeExtPrecision & 0xFFFF);
          }
          break;
        case DictTabInfo::ExtBit:
          AttributeSize = DictTabInfo::aBit;
          AttributeArraySize = AttributeExtLength;
          break;
        case DictTabInfo::ExtLongvarchar:
        case DictTabInfo::ExtLongvarbinary:
          if (AttributeExtLength > 0xffff) return false;
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
          AttributeArraySize =
              (3 + (1 + AttributeExtPrecision) / 2) * AttributeExtLength;
          break;
        case DictTabInfo::ExtDatetime2:
          AttributeSize = DictTabInfo::an8Bit;
          AttributeArraySize =
              (5 + (1 + AttributeExtPrecision) / 2) * AttributeExtLength;
          break;
        case DictTabInfo::ExtTimestamp2:
          AttributeSize = DictTabInfo::an8Bit;
          AttributeArraySize =
              (4 + (1 + AttributeExtPrecision) / 2) * AttributeExtLength;
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
      fprintf(out, "AttributeDefaultValueLen = %d\n", AttributeDefaultValueLen);
      fprintf(out, "AttributeDefaultValue: \n");
      for (unsigned int i = 0; i < AttributeDefaultValueLen; i++)
        fprintf(out, "0x%x", AttributeDefaultValue[i]);
    }
  };

  static const SimpleProperties::SP2StructMapping AttributeMapping[];

  static const Uint32 AttributeMappingSize;

  // Signal constants
  static constexpr Uint32 DataLength = 20;
  static constexpr Uint32 HeaderLength = 5;

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
  enum Deprecated {
    AttributeDGroup = 1009,     // Default NotDGroup
    AttributeStoredInd = 1011,  // Default NotStored
    TableStorageVal = 14,       // Disk storage specified per attribute
    SecondTableId = 17,      // Mandatory between DICT's otherwise not allowed
    FragmentKeyTypeVal = 16  // Default PrimaryKey
  };

  enum Unimplemented {
    ScanOptimised = 15,     // Default updateOptimised
    AttributeGroup = 1012,  // Default 0
    FileNo = 102
  };
};

#define DFGI_MAP_INT(x, y, z)                          \
  {                                                    \
    DictFilegroupInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::Uint32Value, 0, 0            \
  }

#define DFGI_MAP_STR(x, y, z, len)                     \
  {                                                    \
    DictFilegroupInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::StringValue, len, 0          \
  }

#define DFGI_MAP_BIN(x, y, z, len, off)                                   \
  {                                                                       \
    DictFilegroupInfo::y, (unsigned)my_offsetof(x, z),                    \
        SimpleProperties::BinaryValue, len, (unsigned)my_offsetof(x, off) \
  }

#define DFGIBREAK(x) \
  { DictFilegroupInfo::x, 0, SimpleProperties::InvalidValue, 0, 0 }

struct DictFilegroupInfo {
  enum KeyValues {
    FilegroupName = 1,
    FilegroupType = 2,
    FilegroupId = 3,
    FilegroupVersion = 4,

    /**
     * File parameters
     */
    FileName = 100,
    FileType = 101,
    FileId = 103,
    FileFGroupId = 104,
    FileFGroupVersion = 105,
    FileSizeHi = 106,
    FileSizeLo = 107,
    FileFreeExtents = 108,
    FileVersion = 109,
    FileEnd = 199,  //

    /**
     * Tablespace parameters
     */
    TS_ExtentSize = 1000,  // specified in bytes
    TS_LogfileGroupId = 1001,
    TS_LogfileGroupVersion = 1002,
    TS_GrowLimit = 1003,  // In bytes
    TS_GrowSizeHi = 1004,
    TS_GrowSizeLo = 1005,
    TS_GrowPattern = 1006,
    TS_GrowMaxSize = 1007,

    /**
     * Logfile group parameters
     */
    LF_UndoBufferSize = 2005,  // In bytes
    LF_UndoGrowLimit = 2000,   // In bytes
    LF_UndoGrowSizeHi = 2001,
    LF_UndoGrowSizeLo = 2002,
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
    char GrowPattern[PATH_MAX];
    Uint32 GrowMaxSize;
  };

  // Table data interpretation
  struct Filegroup {
    char FilegroupName[MAX_TAB_NAME_SIZE];
    Uint32 FilegroupType;  // ObjType
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
    // GrowSpec LF_RedoGrow;
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

#define DHMI_MAP_INT(x, y, z)                        \
  {                                                  \
    DictHashMapInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::Uint32Value, 0, 0          \
  }

#define DHMI_MAP_STR(x, y, z, len)                   \
  {                                                  \
    DictHashMapInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::StringValue, len, 0        \
  }

#define DHMI_MAP_BIN(x, y, z, len, off)                                   \
  {                                                                       \
    DictHashMapInfo::y, (unsigned)my_offsetof(x, z),                      \
        SimpleProperties::BinaryValue, len, (unsigned)my_offsetof(x, off) \
  }

#define DHMIBREAK(x) \
  { DictHashMapInfo::x, 0, SimpleProperties::InvalidValue, 0, 0 }

struct DictHashMapInfo {
  enum KeyValues { HashMapName = 1, HashMapBuckets = 2, HashMapValues = 3 };

  // Table data interpretation
  struct HashMap {
    char HashMapName[MAX_TAB_NAME_SIZE];
    Uint32 HashMapBuckets;
    Uint16 HashMapValues[NDB_MAX_HASHMAP_BUCKETS];
    Uint32 HashMapObjectId;
    Uint32 HashMapVersion;
    HashMap() {}
    void init();
  };
  static const Uint32 MappingSize;
  static const SimpleProperties::SP2StructMapping Mapping[];
};

/**
 * Foreign Keys
 */
struct DictForeignKeyInfo {
  enum KeyValues {
    ForeignKeyName = 1,
    ForeignKeyId = 2,
    ForeignKeyVersion = 3,
    ForeignKeyParentTableId = 4,
    ForeignKeyParentTableVersion = 5,
    ForeignKeyChildTableId = 6,
    ForeignKeyChildTableVersion = 7,
    ForeignKeyParentIndexId = 8,
    ForeignKeyParentIndexVersion = 9,
    ForeignKeyChildIndexId = 10,
    ForeignKeyChildIndexVersion = 11,
    ForeignKeyOnUpdateAction = 12,
    ForeignKeyOnDeleteAction = 13,
    ForeignKeyParentTableName = 14,
    ForeignKeyParentIndexName = 15,
    ForeignKeyChildTableName = 16,
    ForeignKeyChildIndexName = 17,
    ForeignKeyParentColumnsLength = 18,
    ForeignKeyParentColumns = 19,
    ForeignKeyChildColumnsLength = 20,
    ForeignKeyChildColumns = 21
  };

  // Table data interpretation
  struct ForeignKey {
    char Name[MAX_TAB_NAME_SIZE];
    char ParentTableName[MAX_TAB_NAME_SIZE];
    char ParentIndexName[MAX_TAB_NAME_SIZE];
    char ChildTableName[MAX_TAB_NAME_SIZE];
    char ChildIndexName[MAX_TAB_NAME_SIZE];
    Uint32 ForeignKeyId;
    Uint32 ForeignKeyVersion;
    Uint32 ParentTableId;
    Uint32 ParentTableVersion;
    Uint32 ChildTableId;
    Uint32 ChildTableVersion;
    Uint32 ParentIndexId;
    Uint32 ParentIndexVersion;
    Uint32 ChildIndexId;
    Uint32 ChildIndexVersion;
    Uint32 OnUpdateAction;
    Uint32 OnDeleteAction;
    Uint32 ParentColumnsLength;
    Uint32 ParentColumns[MAX_ATTRIBUTES_IN_INDEX];
    Uint32 ChildColumnsLength;
    Uint32 ChildColumns[MAX_ATTRIBUTES_IN_INDEX];
    ForeignKey() {}
    void init();
  };
  static const Uint32 MappingSize;
  static const SimpleProperties::SP2StructMapping Mapping[];
};

void ndbout_print(const DictForeignKeyInfo::ForeignKey &fk, char *buf,
                  size_t sz);

class NdbOut &operator<<(class NdbOut &out,
                         const DictForeignKeyInfo::ForeignKey &fk);

#define DFKI_MAP_INT(x, y, z)                           \
  {                                                     \
    DictForeignKeyInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::Uint32Value, 0, 0             \
  }

#define DFKI_MAP_STR(x, y, z, len)                      \
  {                                                     \
    DictForeignKeyInfo::y, (unsigned)my_offsetof(x, z), \
        SimpleProperties::StringValue, 0, 0             \
  }

#define DFKI_MAP_BIN(x, y, z, len, off)                                   \
  {                                                                       \
    DictForeignKeyInfo::y, (unsigned)my_offsetof(x, z),                   \
        SimpleProperties::BinaryValue, len, (unsigned)my_offsetof(x, off) \
  }

#define DFKIBREAK(x) \
  { DictForeignKeyInfo::x, 0, SimpleProperties::InvalidValue, 0, 0 }

#undef JAM_FILE_ID

#endif

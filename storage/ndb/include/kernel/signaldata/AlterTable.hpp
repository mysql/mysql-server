/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef ALTER_TABLE_HPP
#define ALTER_TABLE_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 112


struct AlterTableReq {
  static constexpr Uint32 SignalLength = 8;
  
  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 changeMask;

  SECTION( DICT_TAB_INFO = 0 );

/**
 * ChangeMask
 */

/*
  n = Changed name
  f = Changed frm
  d = Changed fragment data
  r = Changed range or list array
  t = Changed tablespace name array
  s = Changed tablespace id array
  a = Add attribute
  f = Add fragment(s)
  r = Reorg fragment(s)
  c = Reorg commit flag
  C = Reorg complete
  u = Reorg Suma enable flag
  U = Reorg Suma filter flag
  F = Fragment count type flag
  R = Changed Read Backup flag
  m = Modified attribute
           1111111111222222222233
 01234567890123456789012345678901
 nfdrtsafrcCuUFRm----------------
*/
#define NAME_SHIFT        (0)
#define FRM_SHIFT         (1)
#define FRAG_DATA_SHIFT   (2)
#define RANGE_LIST_SHIFT  (3)
#define TS_NAME_SHIFT     (4)
#define TS_SHIFT          (5)
#define ADD_ATTR_SHIFT    (6)
#define ADD_FRAG_SHIFT    (7)
#define REORG_FRAG_SHIFT  (8)
#define REORG_COMMIT_SHIFT   (9)
#define REORG_COMPLETE_SHIFT (10)
#define REORG_SUMA_ENABLE (11)
#define REORG_SUMA_FILTER (12)
#define PARTITION_BALANCE_SHIFT (13)
#define READ_BACKUP_SHIFT (14)
#define MODIFY_ATTR_SHIFT (15)

 /**
   * Getters and setters
   */ 

  /**
   * These are that flags that can be set from the NDB API
   * as part of an online alter table (inplace).
   * We can change the name of a table,
   * we can change the frm file of a table,
   * we can change the read backup flag of a table,
   * we can change the name of an attribute of a table and
   * we can add attributes to a table and
   * we can change the partition balance of a table,
   * we can add fragments to the table.
   */
  static Uint8 getNameFlag(const UintR & changeMask);
  static void setNameFlag(UintR &  changeMask, Uint32 nameFlg);
  static Uint8 getFrmFlag(const UintR & changeMask);
  static void setFrmFlag(UintR &  changeMask, Uint32 frmFlg);
  static Uint8 getFragDataFlag(const UintR & changeMask);
  static void setFragDataFlag(UintR &  changeMask, Uint32 fragFlg);
  static Uint8 getRangeListFlag(const UintR & changeMask);
  static void setRangeListFlag(UintR &  changeMask, Uint32 rangeFlg);
  static Uint8 getAddAttrFlag(const UintR & changeMask);
  static void setAddAttrFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getModifyAttrFlag(const UintR & changeMask);
  static void setModifyAttrFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getAddFragFlag(const UintR & changeMask);
  static void setAddFragFlag(UintR &  changeMask, Uint32 tsFlg);
  static void setReadBackupFlag(UintR & changeMask, Uint32 tsFlg);
  static Uint8 getReadBackupFlag(const UintR & changeMask);

  /**
   * These flags are never used.
   */
  static Uint8 getTsNameFlag(const UintR & changeMask);
  static void setTsNameFlag(UintR &  changeMask, Uint32 tsNameFlg);
  static Uint8 getTsFlag(const UintR & changeMask);
  static void setTsFlag(UintR &  changeMask, Uint32 tsFlg);

  /**
   * The getReorgFragFlag is set by DICT when the hashmap changes
   * as part of reorganise of partitions. It should not be set
   * by the NDB API, it is set by DICT.
   */
  static Uint8 getReorgFragFlag(const UintR & changeMask);
  static void setReorgFragFlag(UintR &  changeMask, Uint32 tsFlg);

  /**
   * The flags below are al defined as part of DICT subops. This means
   * that they should not be set by the NDB API. They are set in the
   * subops handling in DICT as part of executing the ALTER_TABLE_REQ
   * signal from the NDB API.
   */
  static Uint8 getReorgCommitFlag(const UintR & changeMask);
  static void setReorgCommitFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgCompleteFlag(const UintR & changeMask);
  static void setReorgCompleteFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgSumaEnableFlag(const UintR & changeMask);
  static void setReorgSumaEnableFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgSumaFilterFlag(const UintR & changeMask);
  static void setReorgSumaFilterFlag(UintR &  changeMask, Uint32 tsFlg);
  static void setPartitionBalanceFlag(UintR & changeMask, Uint32 tsFlg);
  static Uint8 getPartitionBalanceFlag(const UintR & changeMask);

  static bool getSubOp(const UintR & changeMask)
  {
    return
      getReorgCommitFlag(changeMask) ||
      getReorgCompleteFlag(changeMask) ||
      getReorgSumaEnableFlag(changeMask) ||
      getReorgSumaFilterFlag(changeMask);
  }

  static bool getReorgSubOp(const UintR & changeMask)
  {
    return
      getReorgCommitFlag(changeMask) ||
      getReorgCompleteFlag(changeMask) ||
      getReorgSumaEnableFlag(changeMask) ||
      getReorgSumaFilterFlag(changeMask);
  }
};

inline
Uint8
AlterTableReq::getTsFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> TS_SHIFT) & 1);
}

inline
void
AlterTableReq::setTsFlag(UintR & changeMask, Uint32 tsFlg){
  changeMask |= (tsFlg << TS_SHIFT);
}

inline
Uint8
AlterTableReq::getNameFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> NAME_SHIFT) & 1);
}

inline
void
AlterTableReq::setNameFlag(UintR & changeMask, Uint32 nameFlg){
  changeMask |= (nameFlg << NAME_SHIFT);
}

inline
Uint8
AlterTableReq::getFrmFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> FRM_SHIFT) & 1);
}

inline
void
AlterTableReq::setFrmFlag(UintR & changeMask, Uint32 frmFlg){
  changeMask |= (frmFlg << FRM_SHIFT);
}

inline
Uint8
AlterTableReq::getFragDataFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> FRAG_DATA_SHIFT) & 1);
}

inline
void
AlterTableReq::setFragDataFlag(UintR & changeMask, Uint32 fragDataFlg){
  changeMask |= (fragDataFlg << FRAG_DATA_SHIFT);
}

inline
Uint8
AlterTableReq::getRangeListFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> RANGE_LIST_SHIFT) & 1);
}

inline
void
AlterTableReq::setRangeListFlag(UintR & changeMask, Uint32 rangeFlg){
  changeMask |= (rangeFlg << RANGE_LIST_SHIFT);
}

inline
Uint8
AlterTableReq::getTsNameFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> TS_NAME_SHIFT) & 1);
}

inline
void
AlterTableReq::setTsNameFlag(UintR & changeMask, Uint32 tsNameFlg){
  changeMask |= (tsNameFlg << TS_NAME_SHIFT);
}

inline
Uint8
AlterTableReq::getAddAttrFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> ADD_ATTR_SHIFT) & 1);
}

inline
void
AlterTableReq::setAddAttrFlag(UintR & changeMask, Uint32 addAttrFlg){
  changeMask |= (addAttrFlg << ADD_ATTR_SHIFT);
}

inline
Uint8
AlterTableReq::getModifyAttrFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> MODIFY_ATTR_SHIFT) & 1);
}

inline
void
AlterTableReq::setModifyAttrFlag(UintR & changeMask, Uint32 addAttrFlg){
  changeMask |= (addAttrFlg << MODIFY_ATTR_SHIFT);
}

inline
Uint8
AlterTableReq::getAddFragFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> ADD_FRAG_SHIFT) & 1);
}

inline
void
AlterTableReq::setAddFragFlag(UintR & changeMask, Uint32 addAttrFlg){
  changeMask |= (addAttrFlg << ADD_FRAG_SHIFT);
}

inline
Uint8
AlterTableReq::getReorgFragFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> REORG_FRAG_SHIFT) & 1);
}

inline
void
AlterTableReq::setReorgFragFlag(UintR & changeMask, Uint32 reorgAttrFlg){
  changeMask |= (reorgAttrFlg << REORG_FRAG_SHIFT);
}

inline
Uint8
AlterTableReq::getReorgCommitFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> REORG_COMMIT_SHIFT) & 1);
}

inline
void
AlterTableReq::setReorgCommitFlag(UintR & changeMask, Uint32 reorgAttrFlg){
  changeMask |= (reorgAttrFlg << REORG_COMMIT_SHIFT);
}


inline
Uint8
AlterTableReq::getReorgCompleteFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> REORG_COMPLETE_SHIFT) & 1);
}

inline
void
AlterTableReq::setReorgCompleteFlag(UintR & changeMask, Uint32 reorgAttrFlg){
  changeMask |= (reorgAttrFlg << REORG_COMPLETE_SHIFT);
}

inline
Uint8
AlterTableReq::getReorgSumaEnableFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> REORG_SUMA_ENABLE) & 1);
}

inline
void
AlterTableReq::setReorgSumaEnableFlag(UintR & changeMask, Uint32 reorgAttrFlg){
  changeMask |= (reorgAttrFlg << REORG_SUMA_ENABLE);
}

inline
Uint8
AlterTableReq::getReorgSumaFilterFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> REORG_SUMA_FILTER) & 1);
}

inline
void
AlterTableReq::setReorgSumaFilterFlag(UintR & changeMask, Uint32 reorgAttrFlg){
  changeMask |= (reorgAttrFlg << REORG_SUMA_FILTER);
}

inline
Uint8
AlterTableReq::getPartitionBalanceFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> PARTITION_BALANCE_SHIFT) & 1);
}

inline
void
AlterTableReq::setPartitionBalanceFlag(UintR & changeMask, Uint32 fctFlag){
  changeMask |= (fctFlag << PARTITION_BALANCE_SHIFT);
}

inline
Uint8
AlterTableReq::getReadBackupFlag(const UintR & changeMask){
  return (Uint8)((changeMask >> READ_BACKUP_SHIFT) & 1);
}

inline
void
AlterTableReq::setReadBackupFlag(UintR & changeMask, Uint32 rbFlag){
  changeMask |= (rbFlag << READ_BACKUP_SHIFT);
}

struct AlterTableConf {
  static constexpr Uint32 SignalLength = 6;

  Uint32 senderRef;
  union {
    Uint32 clientData;
    Uint32 senderData;
  };
  Uint32 transId;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 newTableVersion;
};

struct AlterTableRef {
  static constexpr Uint32 SignalLength = 9;

  enum ErrorCode {
    NoError = 0,
    InvalidTableVersion = 241,
    DropInProgress      = 283,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    InvalidFormat = 703,
    AttributeNameTooLong = 704,
    TableNameTooLong = 705,
    Inconsistency = 706,
    NoMoreTableRecords = 707,
    NoMoreAttributeRecords = 708,
    NoSuchTable = 709,
    AttributeNameTwice = 720,
    TableAlreadyExist = 721,
    ArraySizeTooBig = 737,
    RecordTooBig = 738,
    InvalidPrimaryKeySize  = 739,
    NullablePrimaryKey = 740,
    UnsupportedChange = 741,
    BackupInProgress = 762,
    IncompatibleVersions = 763,
    SingleUser = 299,
    TableDefinitionTooBig = 793
  };

  Uint32 senderRef;
  union {
    Uint32 clientData;
    Uint32 senderData;
  };
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorNodeId;
  Uint32 masterNodeId;
  Uint32 errorStatus;
  Uint32 errorKey;
};

/**
 * Inform API about change of table definition
 */
struct AlterTableRep 
{
  friend bool printALTER_TABLE_REP(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 3;
  
  enum Change_type 
  {
    CT_ALTERED = 0x1,
    CT_DROPPED = 0x2
  };
  
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 changeType;
  
  SECTION( TABLE_NAME = 0 );
};


#undef JAM_FILE_ID

#endif

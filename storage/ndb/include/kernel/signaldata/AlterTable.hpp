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

#ifndef ALTER_TABLE_HPP
#define ALTER_TABLE_HPP

#include "SignalData.hpp"

struct AlterTableReq {
  STATIC_CONST( SignalLength = 8 );
  
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

           1111111111222222222233
 01234567890123456789012345678901
 nfdrtsafr-----------------------
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

 /**
   * Getters and setters
   */ 
  static Uint8 getNameFlag(const UintR & changeMask);
  static void setNameFlag(UintR &  changeMask, Uint32 nameFlg);
  static Uint8 getFrmFlag(const UintR & changeMask);
  static void setFrmFlag(UintR &  changeMask, Uint32 frmFlg);
  static Uint8 getFragDataFlag(const UintR & changeMask);
  static void setFragDataFlag(UintR &  changeMask, Uint32 fragFlg);
  static Uint8 getRangeListFlag(const UintR & changeMask);
  static void setRangeListFlag(UintR &  changeMask, Uint32 rangeFlg);
  static Uint8 getTsNameFlag(const UintR & changeMask);
  static void setTsNameFlag(UintR &  changeMask, Uint32 tsNameFlg);
  static Uint8 getTsFlag(const UintR & changeMask);
  static void setTsFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getAddAttrFlag(const UintR & changeMask);
  static void setAddAttrFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getAddFragFlag(const UintR & changeMask);
  static void setAddFragFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgFragFlag(const UintR & changeMask);
  static void setReorgFragFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgCommitFlag(const UintR & changeMask);
  static void setReorgCommitFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgCompleteFlag(const UintR & changeMask);
  static void setReorgCompleteFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgSumaEnableFlag(const UintR & changeMask);
  static void setReorgSumaEnableFlag(UintR &  changeMask, Uint32 tsFlg);
  static Uint8 getReorgSumaFilterFlag(const UintR & changeMask);
  static void setReorgSumaFilterFlag(UintR &  changeMask, Uint32 tsFlg);

  static bool getReorgSubOp(const UintR & changeMask){
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

struct AlterTableConf {
  STATIC_CONST( SignalLength = 6 );

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
  STATIC_CONST( SignalLength = 9 );

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
  
  STATIC_CONST( SignalLength = 3 );
  
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

#endif

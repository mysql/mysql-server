/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ALTER_TABLE_HPP
#define ALTER_TABLE_HPP

#include "SignalData.hpp"

/**
 * AlterTable
 *
 * This signal is sent by API to DICT/TRIX
 * as a request to alter a secondary index
 * and then from TRIX to TRIX(n) and TRIX to TC.
 */
class AlterTableReq {
  /**
   * Sender(s)
   */
  // API
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbTableImpl;
  friend class NdbEventOperationImpl;
  friend class NdbDictInterface;
  friend class Dbdict;
  friend class Dbtup;
  friend class Suma;

  /**
   * For printing
   */
  friend bool printALTER_TABLE_REQ(FILE*, const Uint32*, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 5 );
  
private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 changeMask;
  Uint32 tableId;
  Uint32 tableVersion;

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

           1111111111222222222233
 01234567890123456789012345678901
 nfdrtsa-------------------------
*/
#define NAME_SHIFT        (0)
#define FRM_SHIFT         (1)
#define FRAG_DATA_SHIFT   (2)
#define RANGE_LIST_SHIFT  (3)
#define TS_NAME_SHIFT     (4)
#define TS_SHIFT          (5)
#define ADD_ATTR_SHIFT    (6)

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


class AlterTableRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend class NdbDictInterface;
  
  /**
   * For printing
   */
  friend bool printALTER_TABLE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 7 );

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
    SingleUser = 299
  };

private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 status;

public:
  Uint32 getErrorCode() const {
    return errorCode;
  }
  Uint32 getErrorLine() const {
    return errorLine;
  }
};

class AlterTableConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend class NdbDictInterface;
  
  /**
   * For printing
   */
  friend bool printALTER_TABLE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 tableVersion;
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

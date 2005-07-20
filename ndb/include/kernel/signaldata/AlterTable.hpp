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
  friend class NdbDictInterface;
  friend class Dbdict;

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

           1111111111222222222233
 01234567890123456789012345678901
 n-------------------------------
*/
#define NAME_SHIFT        (0)

 /**
   * Getters and setters
   */ 
  static Uint8 getNameFlag(const UintR & changeMask);
  static void setNameFlag(UintR &  changeMask, Uint32 nameFlg);
};

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
    BackupInProgress = 746
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

#endif

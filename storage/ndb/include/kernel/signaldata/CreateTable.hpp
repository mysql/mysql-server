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

#ifndef CREATE_TABLE_HPP
#define CREATE_TABLE_HPP

#include "SignalData.hpp"

/**
 * CreateTable
 *
 * This signal is sent by API to DICT/TRIX
 * as a request to create a secondary index
 * and then from TRIX to TRIX(n) and TRIX to TC.
 */
class CreateTableReq {
  /**
   * Sender(s)
   */
  // API
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;
  friend class Ndbcntr;

  /**
   * For printing
   */
  friend bool printCREATE_TABLE_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 2 );
  
private:
  Uint32 senderData;
  Uint32 senderRef;

  SECTION( DICT_TAB_INFO = 0 );
};

class CreateTableRef {
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
  friend bool printCREATE_TABLE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 7 );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    InvalidFormat = 703,
    AttributeNameTooLong = 704,
    TableNameTooLong = 705,
    Inconsistency = 706,
    NoMoreTableRecords = 707,
    NoMoreAttributeRecords = 708,
    AttributeNameTwice = 720,
    TableAlreadyExist = 721,
    InvalidArraySize = 736,
    ArraySizeTooBig = 737,
    RecordTooBig = 738,
    InvalidPrimaryKeySize  = 739,
    NullablePrimaryKey = 740,
    InvalidCharset = 743,
    SingleUser = 299,
    InvalidTablespace = 755,
    VarsizeBitfieldNotSupported = 757,
    NotATablespace = 758,
    InvalidTablespaceVersion = 759,
    OutOfStringBuffer = 773,
    NoLoggingTemporaryTable = 778
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

class CreateTableConf {
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
  friend bool printCREATE_TABLE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 tableVersion;
};

#endif

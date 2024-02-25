/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef CREATE_TABLE_HPP
#define CREATE_TABLE_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 151


struct CreateTableReq {
  static constexpr Uint32 SignalLength = 5;
  
  union { Uint32 clientRef, senderRef; };
  union { Uint32 clientData, senderData; };
  Uint32 requestInfo;
  Uint32 transId;
  Uint32 transKey;

  SECTION( DICT_TAB_INFO = 0 );
};

struct CreateTableConf {
  static constexpr Uint32 SignalLength = 5;

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 tableVersion;
};

struct CreateTableRef {
  static constexpr Uint32 SignalLength = 9;

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    TooManySchemaOps = 783,     //wl3600_todo move the 3 to DictSignal.hpp
    InvalidTransKey = 781,
    InvalidTransId = 782,
    InvalidFormat = 703,
    AttributeNameTooLong = 704,
    TableNameTooLong = 705,
    Inconsistency = 706,
    NoMoreTableRecords = 707,
    NoMoreAttributeRecords = 708,
    NoMoreHashmapRecords = 712,
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
    NoLoggingTemporaryTable = 778,
    InvalidHashMap = 790,
    TableDefinitionTooBig = 793,
    FeatureRequiresUpgrade = 794,
    WrongPartitionBalanceFullyReplicated = 797,
    NoLoggingDiskTable = 798,
    NonDefaultPartitioningWithNoPartitions = 799,
    TooManyFragments = 1224
  };

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorNodeId;
  Uint32 masterNodeId;
  Uint32 errorStatus;
  Uint32 errorKey;

  //wl3600_todo out
  Uint32 getErrorCode() const {
    return errorCode;
  }
  Uint32 getErrorLine() const {
    return errorLine;
  }
};


#undef JAM_FILE_ID

#endif

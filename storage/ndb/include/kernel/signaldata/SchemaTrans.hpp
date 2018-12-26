/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SCHEMA_TRANS_HPP
#define SCHEMA_TRANS_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 123


// begin

struct SchemaTransBeginReq {
  STATIC_CONST( SignalLength = 3 );
  Uint32 clientRef;
  Uint32 transId;
  Uint32 requestInfo;
};

struct SchemaTransBeginConf {
  STATIC_CONST( SignalLength = 3 );
  Uint32 senderRef;
  Uint32 transId;
  Uint32 transKey;
};

struct SchemaTransBeginRef {
  STATIC_CONST( SignalLength = 6 );
  enum ErrorCode {
    NoError = 0,
    NotMaster = 702,
    Busy = 701,
    BusyWithNR = 711,
    TooManySchemaTrans = 780,
    IncompatibleVersions = 763,
    Nodefailure = 786,
    OutOfSchemaTransMemory = 796
  };
  Uint32 senderRef;
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

// commit, abort

struct SchemaTransEndReq {
  // must match NdbDictionary::Dictionary::SchemaTransFlag
  enum Flag {
    SchemaTransAbort = 1,
    SchemaTransBackground = 2,
    SchemaTransPrepare = 4 // Only run prepare
  };
  STATIC_CONST( SignalLength = 5 );
  Uint32 clientRef;
  Uint32 transId;
  Uint32 requestInfo;
  Uint32 transKey;
  Uint32 flags;
};

struct SchemaTransEndConf {
  STATIC_CONST( SignalLength = 2 );
  Uint32 senderRef;
  Uint32 transId;
};

struct SchemaTransEndRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    InvalidTransKey = 781,
    InvalidTransId = 782,
    InvalidTransState = 784
  };
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderRef;
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

struct SchemaTransEndRep {
  enum ErrorCode {
    NoError = 0,
    TransAborted = 787
  };
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderRef;
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};


#undef JAM_FILE_ID

#endif

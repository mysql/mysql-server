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

#ifndef BUILD_INDX_HPP
#define BUILD_INDX_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <signaldata/DictTabInfo.hpp>

struct BuildIndxReq {
  enum RequestType {
    MainOp = 1,
    SubOp = 2   // actual build of hash index
  };

  enum RequestFlag {
    RF_BUILD_OFFLINE = 1 << 8
  };

  STATIC_CONST( SignalLength = 11 );
  STATIC_CONST( INDEX_COLUMNS = 0 );
  STATIC_CONST( KEY_COLUMNS = 1 );
  STATIC_CONST( NoOfSections = 2 );

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 buildId;		// Suma subscription id
  Uint32 buildKey;		// Suma subscription key
  Uint32 tableId;
  Uint32 indexId;
  Uint32 indexType;
  Uint32 parallelism;
};

struct BuildIndxConf {
  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 indexId;
  Uint32 indexType;
};

struct BuildIndxRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    BadRequestType = 4247,
    InvalidPrimaryTable = 4249,
    InvalidIndexType = 4250,
    IndexNotUnique = 4251,
    AllocationFailure = 4252,
    InternalError = 4346,
    IndexNotFound = 4243,
    DeadlockError = 4351
  };

  STATIC_CONST( SignalLength = 10 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 indexId;
  Uint32 indexType;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

/* Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef INDEX_STAT_SIGNAL_DATA_HPP
#define INDEX_STAT_SIGNAL_DATA_HPP

#include "SignalData.hpp"

struct IndexStatReq {
  enum RequestType {
    // update
    RT_UPDATE_STAT = 1,
    RT_CLEAN_NEW = 2,
    RT_SCAN_FRAG = 3,
    RT_CLEAN_OLD = 4,
    RT_START_MON = 5,
    // delete
    RT_DELETE_STAT = 6,
    RT_STOP_MON = 7,
    RT_DROP_HEAD = 8,
    RT_CLEAN_ALL = 9
  };
  STATIC_CONST( SignalLength = 9 );
  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 requestFlag;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 tableId;
};

struct IndexStatImplReq {
  STATIC_CONST( SignalLength = 10 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 transId;
  Uint32 requestType;
  Uint32 requestFlag;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 fragCount;
};

struct IndexStatConf {
  STATIC_CONST( SignalLength = 3 );
  Uint32 senderRef;
  union { Uint32 senderData; Uint32 clientData; };
  Uint32 transId;
};

struct IndexStatImplConf {
  STATIC_CONST( SignalLength = 2 );
  Uint32 senderRef;
  Uint32 senderData;
};

struct IndexStatRef {
  enum ErrorCode {
    Busy = 701,
    NotMaster = 702,
    InvalidIndex = 913,
    InvalidRequest = 914,
    NoFreeStatOp = 915,
    InvalidSysTable = 916,
    InvalidSysTableData = 917,
    BusyUtilPrepare = 918,
    BusyUtilExecute = 919
  };
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderRef;
  union { Uint32 senderData; Uint32 clientData; };
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

struct IndexStatImplRef {
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine;
};

struct IndexStatRep {
  enum RequestType {
    RT_UPDATE_REQ = 1,  // TUX->DICT request stats update
    RT_UPDATE_CONF = 2  // TRIX->TUX report stats update
  };
  STATIC_CONST( SignalLength = 9 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 requestFlag;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 loadTime;
};

#endif

/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DROP_INDX_HPP
#define DROP_INDX_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 147


struct DropIndxReq {
  STATIC_CONST( SignalLength = 7 );

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct DropIndxConf {
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct DropIndxRef {
  STATIC_CONST( SignalLength = 9 );

  enum ErrorCode {
    NoError = 0,
    InvalidIndexVersion = 241,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    IndexNotFound = 4243,
    BadRequestType = 4247,
    InvalidName = 4248,
    NotAnIndex = 4254,
    SingleUser = 299
  };

  Uint32 senderRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};


#undef JAM_FILE_ID

#endif

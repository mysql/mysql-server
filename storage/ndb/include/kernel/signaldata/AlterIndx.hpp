/*
   Copyright (C) 2003, 2005-2007 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef ALTER_INDX_HPP
#define ALTER_INDX_HPP

#include "SignalData.hpp"
#include <Bitmask.hpp>
#include <trigger_definitions.h>

struct AlterIndxReq {
  STATIC_CONST( SignalLength = 7 );

  enum RequestFlag {
    RF_BUILD_OFFLINE = 1 << 8
  };

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct AlterIndxConf {
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct AlterIndxRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    IndexNotFound = 4243,
    IndexExists = 4244,
    BadRequestType = 4247,
    NotAnIndex = 4254,
    BadState = 4347,
    Inconsistency = 4348,
    InvalidIndexVersion = 241
  };

  STATIC_CONST( SignalLength = 9 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

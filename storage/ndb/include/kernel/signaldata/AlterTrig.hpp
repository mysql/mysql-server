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

#ifndef ALTER_TRIG_HPP
#define ALTER_TRIG_HPP

#include "SignalData.hpp"
#include <Bitmask.hpp>
#include <trigger_definitions.h>

struct AlterTrigReq {
  enum RequestType {
    AlterTriggerOnline = 1,
    AlterTriggerOffline = 2
  };

  STATIC_CONST( SignalLength = 8 );

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 triggerId;
};

struct AlterTrigConf {
  STATIC_CONST( InternalLength = 3 );
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 triggerId;
};

struct AlterTrigRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    TriggerNotFound = 4238,
    TriggerExists = 4239,
    BadRequestType = 4247
  };

  STATIC_CONST( SignalLength = 9 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 triggerId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

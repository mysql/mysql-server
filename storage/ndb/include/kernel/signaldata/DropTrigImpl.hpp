/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DROP_TRIG_IMPL_HPP
#define DROP_TRIG_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 167


struct DropTrigImplReq {
  STATIC_CONST( SignalLength = 11 );
  SECTION( TRIGGER_NAME_SECTION = 0 ); // optional

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 triggerNo;
  Uint32 triggerId;
  Uint32 triggerInfo;
  Uint32 receiverRef;
};

struct DropTrigImplConf {
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 triggerId;
};

struct DropTrigImplRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    TriggerNotFound = 4238,
    BadRequestType = 4247,
    InvalidName = 4248,
    InconsistentTC = 293
  };

  STATIC_CONST( SignalLength = 8 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 triggerId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};


#undef JAM_FILE_ID

#endif

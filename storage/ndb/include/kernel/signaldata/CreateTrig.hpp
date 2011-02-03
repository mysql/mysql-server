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

#ifndef CREATE_TRIG_HPP
#define CREATE_TRIG_HPP

#include "SignalData.hpp"
#include <Bitmask.hpp>
#include <trigger_definitions.h>
#include <AttributeList.hpp>

struct CreateTrigReq
{
  enum OnlineFlag
  {
    CreateTriggerOnline = 1,
    CreateTriggerOffline = 2
  };

  enum EndpointFlag
  {
    MainTrigger = 0,
    TriggerDst = 1, // TC  "consuming" block(s)
    TriggerSrc = 2  // LQH "producing" block(s)
  };

  STATIC_CONST( SignalLength = 13 );
  SECTION( TRIGGER_NAME_SECTION = 0 );
  SECTION( ATTRIBUTE_MASK_SECTION = 1 );

  static Uint32 getOnlineFlag(Uint32 i) { return i & 3; }
  static void setOnlineFlag(Uint32 & i, Uint32 v) { i |= (v & 3); }
  static Uint32 getEndpointFlag(Uint32 i) { return (i >> 2) & 3;}
  static void setEndpointFlag(Uint32 & i, Uint32 v) { i |= ((v & 3) << 2); }

  Uint32 clientRef;
  Uint32 clientData;

  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexId;       // only for index trigger
  Uint32 indexVersion;
  Uint32 triggerNo;     // only for index trigger
  Uint32 forceTriggerId;// only for NR/SR
  Uint32 triggerInfo;   // type | timing | event | flags
  Uint32 receiverRef;   // receiver for subscription trigger
};

struct CreateTrigConf {
  STATIC_CONST( SignalLength = 7 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 indexId;
  Uint32 triggerId;
  Uint32 triggerInfo;
};

struct CreateTrigRef
{
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    TriggerNameTooLong = 4236,
    TooManyTriggers = 4237,
    TriggerNotFound = 4238,
    TriggerExists = 4239,
    UnsupportedTriggerType = 4240,
    BadRequestType = 4247,
    InvalidName = 4248,
    InvalidTable = 4249,
    OutOfStringBuffer = 773,
    OutOfSectionMemory = 795
  };

  STATIC_CONST( SignalLength = 10 );

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 indexId;
  Uint32 triggerInfo;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef DROP_TRIG_HPP
#define DROP_TRIG_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <trigger_definitions.h>

#define JAM_FILE_ID 71


struct DropTrigReq
{
  enum EndpointFlag
  {
    MainTrigger = 0,
    TriggerDst = 1, // TC  "consuming" block(s)
    TriggerSrc = 2  // LQH "producing" block(s)
  };

  static Uint32 getEndpointFlag(Uint32 i) { return (i >> 2) & 3;}
  static void setEndpointFlag(Uint32 & i, Uint32 v) { i |= ((v & 3) << 2); }

  static constexpr Uint32 SignalLength = 11;
  SECTION( TRIGGER_NAME_SECTION = 0 ); // optional

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 triggerNo;
  Uint32 triggerId;
};

struct DropTrigConf {
  static constexpr Uint32 SignalLength = 6;

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 indexId;
  Uint32 triggerId;
};

struct DropTrigRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    TriggerNotFound = 4238,
    BadRequestType = 4247,
    InvalidName = 4248,
    InvalidTable = 4249,
    UnsupportedTriggerType = 4240
  };

  static constexpr Uint32 SignalLength = 11;

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 tableId;
  Uint32 indexId;
  Uint32 triggerId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};


#undef JAM_FILE_ID

#endif

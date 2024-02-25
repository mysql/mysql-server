/* Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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

#ifndef CREATE_TRIG_IMPL_HPP
#define CREATE_TRIG_IMPL_HPP

#include "SignalData.hpp"
#include <Bitmask.hpp>
#include <AttributeList.hpp>

#define JAM_FILE_ID 104


struct CreateTrigImplReq
{
  enum RequestType
  {
    CreateTriggerOnline = 1,
    CreateTriggerOffline = 2
  };

  static constexpr Uint32 SignalLength = 11 + 3;
  SECTION( ATTRIBUTE_MASK_SECTION = 0 );

  // tableVersion, indexVersion, name section used only within DICT

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
  Uint32 upgradeExtra[3]; // Send TriggerId's as defined in 6.3 here
};

struct CreateTrigImplConf {
  static constexpr Uint32 SignalLength = 5;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;       // BACKUP and SUMA want these back from TUP
  Uint32 triggerId;
  Uint32 triggerInfo;
};

struct CreateTrigImplRef {
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
    InconsistentTC = 293
  };

  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 triggerId;
  Uint32 triggerInfo;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};


#undef JAM_FILE_ID

#endif

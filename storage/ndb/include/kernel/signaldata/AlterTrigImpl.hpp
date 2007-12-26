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

#ifndef ALTER_TRIG_IMPL_HPP
#define ALTER_TRIG_IMPL_HPP

#include "SignalData.hpp"

struct AlterTrigImplReq {
  enum RequestType {
    AlterTriggerOnline = 1,
    AlterTriggerOffline = 2
  };

  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 triggerId;
};

struct AlterTrigImplConf {
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  // wl3600_todo BACKUP and SUMA dont use this so these are redundant
  Uint32 tableId;
  Uint32 triggerId;
};

struct AlterTrigImplRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    TriggerNotFound = 4238,
    TriggerExists = 4239,
    BadRequestType = 4247
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

#endif

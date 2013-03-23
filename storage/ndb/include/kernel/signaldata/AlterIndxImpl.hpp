/* Copyright (C) 2007, 2008 MySQL AB

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

#ifndef ALTER_INDX_IMPL_HPP
#define ALTER_INDX_IMPL_HPP

#include "SignalData.hpp"

struct AlterIndxImplReq {
  enum RequestType {
    AlterIndexOnline = 1,
    AlterIndexOffline = 2,
    AlterIndexAddPartition = 3,
    AlterIndexBuilding = 4
  };

  STATIC_CONST( SignalLength = 8 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 indexType;
};

struct AlterIndxImplConf {
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 senderData;
};

struct AlterIndxImplRef {
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
    InconsistentTC = 292
  };

  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

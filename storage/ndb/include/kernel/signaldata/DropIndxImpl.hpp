/* Copyright (C) 2007 MySQL AB

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

#ifndef DROP_INDX_IMPL_HPP
#define DROP_INDX_IMPL_HPP

#include "SignalData.hpp"

struct DropIndxImplReq {
  STATIC_CONST( SignalLength = 7 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct DropIndxImplConf {
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 senderData;
};

struct DropIndxImplRef {
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

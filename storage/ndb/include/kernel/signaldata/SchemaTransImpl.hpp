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

#ifndef SCHEMA_TRANS_IMPL_HPP
#define SCHEMA_TRANS_IMPL_HPP

#include <Bitmask.hpp>
#include "SignalData.hpp"
#include "GlobalSignalNumbers.h"

struct SchemaTransImplReq
{
  enum RequestType
  {
    RT_START         = 0x0,
    RT_PARSE         = 0x1,
    RT_PREPARE       = 0x2,
    RT_ABORT_PARSE   = 0x3,
    RT_ABORT_PREPARE = 0x4,
    RT_COMMIT        = 0x5,

    RT_COMPLETE      = 0x6,// Not yet used
    RT_FLUSH_SCHEMA  = 0x7 // Not yet used
  };

  STATIC_CONST( SignalLength = 9 );
  Uint32 senderRef;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;   // request type | op extra | global flags | local flags
  Uint32 clientRef;
  Uint32 opKey;
  Uint32 gsn;
};

struct SchemaTransImplConf
{
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderRef;
  Uint32 transKey;
  Uint32 opKey;
  Uint32 requestType;
};

struct SchemaTransImplRef
{
  STATIC_CONST( SignalLength = 8 );
  STATIC_CONST( GSN = GSN_SCHEMA_TRANS_IMPL_REF );
  enum ErrorCode {
    NoError = 0,
    NotMaster = 702,
    TooManySchemaTrans = 780,
    InvalidTransKey = 781,
    InvalidTransId = 782,
    TooManySchemaOps = 783,
    SeizeFailed = 783,
    InvalidTransState = 784,
    NF_FakeErrorREF = 1
  };
  Uint32 senderRef;
  union { Uint32 transKey, senderData; };
  Uint32 opKey;
  Uint32 requestType;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

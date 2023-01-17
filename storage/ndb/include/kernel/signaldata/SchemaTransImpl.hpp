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

#ifndef SCHEMA_TRANS_IMPL_HPP
#define SCHEMA_TRANS_IMPL_HPP

#include <Bitmask.hpp>
#include "SignalData.hpp"
#include "GlobalSignalNumbers.h"

#define JAM_FILE_ID 205


struct SchemaTransImplReq
{
  enum RequestType
  {
    RT_START         = 0x0,
    RT_PARSE         = 0x1,
    RT_FLUSH_PREPARE = 0x2,
    RT_PREPARE       = 0x3,
    RT_ABORT_PARSE   = 0x4,
    RT_ABORT_PREPARE = 0x5,
    RT_FLUSH_COMMIT  = 0x6,
    RT_COMMIT        = 0x7,
    RT_FLUSH_COMPLETE= 0x8,
    RT_COMPLETE      = 0x9,
    RT_END           = 0xa // release...
  };

  static constexpr Uint32 SignalLength = 8;
  static constexpr Uint32 SignalLengthStart = 9;
  static constexpr Uint32 GSN = GSN_SCHEMA_TRANS_IMPL_REQ;
  Uint32 senderRef;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;   // request type | op extra | global flags | local flags
  Uint32 opKey;
  union {
    struct {
      Uint32 clientRef;
      Uint32 objectId;
    } start;
    struct {
      Uint32 gsn;
    } parse;
  };
};

struct SchemaTransImplConf
{
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 GSN = GSN_SCHEMA_TRANS_IMPL_CONF;
  Uint32 senderRef;
  Uint32 transKey;
  Uint32 opKey;
  Uint32 requestType;
};

struct SchemaTransImplRef
{
  static constexpr Uint32 SignalLength = 8;
  static constexpr Uint32 GSN = GSN_SCHEMA_TRANS_IMPL_REF;
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    TooManySchemaTrans = 780,
    InvalidTransKey = 781,
    InvalidTransId = 782,
    TooManySchemaOps = 783,
    SeizeFailed = 783,
    InvalidTransState = 784,
    NF_FakeErrorREF = 99
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


#undef JAM_FILE_ID

#endif

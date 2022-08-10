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

#ifndef ALTER_INDX_HPP
#define ALTER_INDX_HPP

#include "SignalData.hpp"
#include <Bitmask.hpp>
#include <trigger_definitions.h>

#define JAM_FILE_ID 81


struct AlterIndxReq {
  static constexpr Uint32 SignalLength = 7;

  enum RequestFlag {
    RF_BUILD_OFFLINE = 1 << 8
  };

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct AlterIndxConf {
  static constexpr Uint32 SignalLength = 5;

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct AlterIndxRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    IndexNotFound = 4243,
    IndexExists = 4244,
    BadRequestType = 4247,
    NotAnIndex = 4254,
    BadState = 4347,
    Inconsistency = 4348,
    InvalidIndexVersion = 241
  };

  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  union { Uint32 clientData, senderData; };
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};


#undef JAM_FILE_ID

#endif

/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COPY_DATA_HPP
#define COPY_DATA_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 201

struct CopyDataReq {
  static constexpr Uint32 SignalLength = 9;

  enum RequestType {
    ReorgCopy = 0,
    ReorgDelete = 1
    // AlterTableCopy
  };

  enum Flags { TupOrder = 1, NoScanTakeOver = 2 };

  union {
    Uint32 clientRef;
    Uint32 senderRef;
  };
  union {
    Uint32 clientData;
    Uint32 senderData;
  };
  Uint32 transKey;
  Uint32 transId;
  Uint32 requestType;
  Uint32 requestInfo;
  Uint32 srcTableId;
  Uint32 dstTableId;
  Uint32 srcFragments;  // Only used for ReorgDelete
};

struct CopyDataConf {
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  Uint32 transId;
};

struct CopyDataRef {
  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 transId;
  Uint32 masterNodeId;
  Uint32 errorNodeId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorKey;
  Uint32 errorStatus;
};

typedef CopyDataReq CopyDataImplReq;
typedef CopyDataRef CopyDataImplRef;
typedef CopyDataConf CopyDataImplConf;

#undef JAM_FILE_ID

#endif

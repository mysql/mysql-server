/* Copyright (C) 2007 MySQL AB
   Use is subject to license terms

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

#ifndef CREATE_INDX_IMPL_HPP
#define CREATE_INDX_IMPL_HPP

#include "SignalData.hpp"

struct CreateIndxImplReq {
  STATIC_CONST( SignalLength = 8 );
  SECTION( ATTRIBUTE_LIST_SECTION = 0 );
  SECTION( INDEX_NAME_SECTION = 1 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexType;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct CreateIndxImplConf {
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 senderData;
};

struct CreateIndxImplRef {
  STATIC_CONST( SignalLength = 6 );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    BusyWithNR = 711,
    NotMaster = 702,
    IndexOnDiskAttributeError = 756,
    TriggerNotFound = 4238,
    TriggerExists = 4239,
    IndexNameTooLong = 4241,
    TooManyIndexes = 4242,
    IndexExists = 4244,
    AttributeNullable = 4246,
    BadRequestType = 4247,
    InvalidName = 4248,
    InvalidPrimaryTable = 4249,
    InvalidIndexType = 4250,
    NotUnique = 4251,
    AllocationError = 4252,
    CreateIndexTableFailed = 4253,
    DuplicateAttributes = 4258,
    TableIsTemporary = 776,
    TableIsNotTemporary = 777,
    NoLoggingTemporaryIndex = 778,
    InconsistentTC = 292
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

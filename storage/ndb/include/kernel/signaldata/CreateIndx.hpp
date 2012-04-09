/*
   Copyright (C) 2003, 2005-2007 MySQL AB, 2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CREATE_INDX_HPP
#define CREATE_INDX_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <signaldata/DictTabInfo.hpp>

struct CreateIndxReq 
{
  STATIC_CONST( SignalLength = 9 );
  SECTION( ATTRIBUTE_LIST_SECTION = 0 );
  SECTION( INDEX_NAME_SECTION = 1 );

  enum RequestType
  {
    RF_BUILD_OFFLINE = 1 << 8 // See BuildIndxReq
  };

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 transKey;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 indexType;
  Uint32 online;
};

struct CreateIndxConf {
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 indexId;
  Uint32 indexVersion;
};

struct CreateIndxRef {
  STATIC_CONST( SignalLength = 7 );

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
    SingleUser = 299,
    TableIsTemporary = 776,
    TableIsNotTemporary = 777,
    NoLoggingTemporaryIndex = 778
  };

  Uint32 senderRef;
  Uint32 clientData;
  Uint32 transId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

#endif

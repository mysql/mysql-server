/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BUILD_INDX_IMPL_HPP
#define BUILD_INDX_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 30


struct BuildIndxImplReq
{
  enum RequestFlag {
    RF_BUILD_OFFLINE = 1 << 8
    ,RF_NO_DISK      = 1 << 9         /* Indexed columns are not on disk */
  };

  STATIC_CONST( SignalLength = 10 );
  STATIC_CONST( INDEX_COLUMNS = 0 );
  STATIC_CONST( KEY_COLUMNS = 1 );
  STATIC_CONST( NoOfSections = 2 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 transId;
  Uint32 buildId;		// Suma subscription id
  Uint32 buildKey;		// Suma subscription key
  Uint32 tableId;
  Uint32 indexId;
  Uint32 indexType;
  Uint32 parallelism;
};

struct BuildIndxImplConf {
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 senderData;
};

struct BuildIndxImplRef {
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    BadRequestType = 4247,
    InvalidPrimaryTable = 4249,
    InvalidIndexType = 4250,
    IndexNotUnique = 4251,
    AllocationFailure = 4252,
    InternalError = 4346
  };

  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 masterNodeId;
};

struct mt_BuildIndxReq
{
  Uint32 senderRef;
  Uint32 senderData;

  Uint32 indexId;
  Uint32 tableId;
  Uint32 fragId;

  void * tux_ptr;              // ptr to Dbtux
  void * tup_ptr;
  Uint32 (* func_ptr)(void *); // c-function

  void * mem_buffer;  // allocated by FS
  Uint32 buffer_size; //

  Uint32 pad[3];

  STATIC_CONST( SignalLength = (6 + 3 + 4 * (sizeof(void*) / 4)));
};


#undef JAM_FILE_ID

#endif

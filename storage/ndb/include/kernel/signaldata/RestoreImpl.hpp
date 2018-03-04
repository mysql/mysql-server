/*
   Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RESTORE_SIGNAL_DATA_HPP
#define RESTORE_SIGNAL_DATA_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 214


struct RestoreLcpReq 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lcpNo;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 lcpId;
  Uint32 restoreGcpId;
  Uint32 maxGciCompleted;
  Uint32 createGci;
  STATIC_CONST( SignalLength = 9 );
};

struct RestoreLcpRef
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 extra[1];
  STATIC_CONST( SignalLength = 3 );

  enum ErrorCode 
  {
    OK = 0,
    NoFileRecord = 1,
    OutOfDataBuffer = 2,
    OutOfReadBufferPages = 3,
    InvalidFileFormat = 4
  };
};

struct RestoreLcpConf 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 restoredLcpId;
  Uint32 restoredLocalLcpId;
  Uint32 maxGciCompleted;
  Uint32 afterRestore;
  STATIC_CONST( SignalLength = 6 );
};

struct RestoreContinueB {
  
  enum {
    RESTORE_NEXT = 0, 
    READ_FILE = 1
  };
};


#undef JAM_FILE_ID

#endif

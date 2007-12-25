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

#ifndef DROP_TAB_HPP
#define DROP_TAB_HPP

#include "SignalData.hpp"

struct DropTabReq {
  STATIC_CONST( SignalLength = 5 );

  enum RequestType {
    OnlineDropTab = 0,
    CreateTabDrop = 1,
    RestartDropTab = 2
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
};

struct DropTabConf {
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
};

struct DropTabRef {
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    NoSuchTable = 1,
    DropWoPrep = 2, // Calling Drop with first calling PrepDrop
    PrepDropInProgress = 3,
    DropInProgress = 4,
    NF_FakeErrorREF = 5
  };
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 errorCode;
};

#endif

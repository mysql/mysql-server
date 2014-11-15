/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COPY_TAB_HPP
#define COPY_TAB_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 493


class CopyTabReq {
  friend class Dbdih;
public:
  STATIC_CONST( SignalLengthExtra = 23 );
  STATIC_CONST( SignalLength = 21 );

  enum TabLcpStatus
  {
    LcpCompleted = 0,
    LcpActive = 1
  };

private:
  Uint32 senderRef;
  Uint32 reqinfo;
  Uint32 tableId;
  Uint32 tableSchemaVersion;
  Uint32 noOfWords;
  Uint32 tableWords[16];
  Uint32 tabLcpStatus;
  Uint32 currentLcpId;
};

class CopyTabConf {
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 nodeId;
  Uint32 tableId;
};

#undef JAM_FILE_ID

#endif

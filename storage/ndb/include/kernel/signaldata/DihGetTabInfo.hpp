/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DIH_GET_INFO_TAB_HPP
#define DIH_GET_INFO_TAB_HPP

#include "SignalData.hpp"

/**
 * DihGetTabInfo - Get table info from DIH
 */
struct DihGetTabInfoReq
{
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

public:
  STATIC_CONST( SignalLength = 5 );
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestInfo; // Bitmask of DihGetTabInfoReq::RequestType
  Uint32 tableId;

  enum RequestInfoBits
  {
  };
};

struct DihGetTabInfoRef
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;

  enum ErrorCode
  {
    OutOfConnectionRecords = 350,
    TableBusy = 351,
    TableNotDefined = 352
  };
};

struct DihGetTabInfoConf
{
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderData;
  Uint32 senderRef;
};

#endif

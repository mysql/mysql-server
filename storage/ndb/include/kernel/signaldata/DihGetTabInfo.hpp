/*
   Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DIH_GET_INFO_TAB_HPP
#define DIH_GET_INFO_TAB_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 83


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


#undef JAM_FILE_ID

#endif

/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef CREATE_TAB_HPP
#define CREATE_TAB_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 46


struct CreateTabReq
{
  static constexpr Uint32 SignalLength = 6;
  static constexpr Uint32 SignalLengthLDM = 6 + 11;

  enum RequestType {
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 requestType;
  Uint32 gci;

  /**
   * Used when sending to LQH++
   */
  Uint32 noOfCharsets;
  Uint32 tableType;           // DictTabInfo::TableType
  Uint32 primaryTableId;      // table of index or RNIL
  Uint32 tablespace_id;       // RNIL for MM table
  Uint32 forceVarPartFlag;
  Uint32 noOfAttributes;
  Uint32 noOfNullAttributes;
  Uint32 noOfKeyAttr;
  Uint32 checksumIndicator;
  Uint32 GCPIndicator;
  Uint32 extraRowAuthorBits;

  SECTION( DICT_TAB_INFO = 0 );
  SECTION( FRAGMENTATION = 1 );
};

struct CreateTabConf {
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;

  union {
    Uint32 lqhConnectPtr;
    Uint32 tuxConnectPtr;
    Uint32 tupConnectPtr;
  };
};

struct CreateTabRef {
  static constexpr Uint32 SignalLength = 6;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 errorStatus;
};

/**
 * TcSchVerReq is CreateTab but towards TC...
 *   should be removed in favor of CreateTab
 */
struct TcSchVerReq
{
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 tableLogged;
  Uint32 senderRef;
  Uint32 tableType;
  Uint32 senderData;
  Uint32 noOfPrimaryKeys;
  Uint32 singleUserMode;
  Uint32 userDefinedPartition;
  Uint32 readBackup;
  Uint32 fullyReplicated;
  static constexpr Uint32 SignalLength = 11;
};

struct TcSchVerConf
{
  Uint32 senderRef;
  Uint32 senderData;
  static constexpr Uint32 SignalLength = 2;
};


#undef JAM_FILE_ID

#endif

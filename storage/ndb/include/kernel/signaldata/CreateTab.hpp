/*
   Copyright (C) 2003, 2005-2008 MySQL AB
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

#ifndef CREATE_TAB_HPP
#define CREATE_TAB_HPP

#include "SignalData.hpp"

struct CreateTabReq
{
  STATIC_CONST( SignalLength = 6 );
  STATIC_CONST( SignalLengthLDM = 6 + 11 );

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
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;

  union {
    Uint32 lqhConnectPtr;
    Uint32 tuxConnectPtr;
    Uint32 tupConnectPtr;
  };
};

struct CreateTabRef {
  STATIC_CONST( SignalLength = 6 );

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
  STATIC_CONST( SignalLength = 9 );
};

struct TcSchVerConf
{
  Uint32 senderRef;
  Uint32 senderData;
  STATIC_CONST( SignalLength = 2 );
};

#endif

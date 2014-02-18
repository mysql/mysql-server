/* Copyright (C) 2008 MySQL AB
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

#ifndef DIH_SCAN_TAB_HPP
#define DIH_SCAN_TAB_HPP

#include "SignalData.hpp"

/**
 * DihScanTabReq
 */
struct DihScanTabReq
{
  STATIC_CONST( SignalLength = 4 );
  STATIC_CONST( RetryInterval = 5 );

  Uint32 tableId;
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 schemaTransId;
};

/**
 * DihScanTabConf
 */
struct DihScanTabConf
{
  STATIC_CONST( SignalLength = 6 );

  Uint32 tableId;
  Uint32 senderData;
  Uint32 fragmentCount;
  Uint32 noOfBackups;
  Uint32 scanCookie;
  Uint32 reorgFlag;
};

struct DihScanGetNodesReq
{
  STATIC_CONST( SignalLength = 5 );
  Uint32 tableId;
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 fragId;
  Uint32 scanCookie;
};

struct DihScanGetNodesConf
{
  STATIC_CONST( SignalLength = 9 );

  Uint32 senderData;
  Uint32 nodes[4];
  Uint32 count;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 instanceKey;
};

/**
 * DihScanTabRef
 */
struct DihScanTabRef
{
  enum ErrorCode {
    ErroneousState = 0,
    ErroneousTableState = 1
  };
  STATIC_CONST( SignalLength = 5 );

  Uint32 tableId;
  Uint32 senderData;
  Uint32 error;
  Uint32 tableStatus; // Dbdih::TabRecord::tabStatus
  Uint32 schemaTransId;
};

struct DihScanTabCompleteRep
{
  STATIC_CONST( SignalLength = 2 );

  Uint32 tableId;
  Uint32 scanCookie;
};

#endif

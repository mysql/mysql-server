/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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

#define JAM_FILE_ID 108


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
  STATIC_CONST( InvalidCookie = RNIL );

  Uint32 tableId;
  Uint32 senderData;
  Uint32 fragmentCount;
  Uint32 noOfBackups;
  Uint32 scanCookie;
  Uint32 reorgFlag;
};

struct DihScanGetNodesReq
{
  STATIC_CONST( FixedSignalLength = 4 );
  STATIC_CONST( MAX_DIH_FRAG_REQS = 64); // Max #FragItem in REQ/CONF

  Uint32 tableId;
  Uint32 senderRef;
  Uint32 scanCookie;
  Uint32 fragCnt;

  struct FragItem
  {
    STATIC_CONST( Length = 2 );

    Uint32 senderData;
    Uint32 fragId;
  };

  /**
   * DihScanGetNodesReq request information about specific fragments.
   * - These are either specified in a seperate section (long request)
   *   containing multiple FragItems.
   * - Or directly in a single fragItem[] below (short signal) if it 
   *   contain only a single FragItem.
   */
  FragItem fragItem[1];
};

struct DihScanGetNodesConf
{
  STATIC_CONST( FixedSignalLength = 2 );
  Uint32 tableId;
  Uint32 fragCnt;

  struct FragItem
  {
    STATIC_CONST( Length = 8 );

    Uint32 senderData;
    Uint32 fragId;
    Uint32 instanceKey;
    Uint32 count;
    Uint32 nodes[4];
  };

  /**
   * DihScanGetNodesConf supply information about specific fragments.
   * - These are either specified in a seperate section (long request)
   *   containing multiple FragItems.
   * - Or directly in a single fragItem[] below (short signal) if it 
   *   contain only a single FragItem.
   * Type of long/short Conf-reply will always be the same as the REQuest
   */
  FragItem fragItem[1];
};

struct DihScanGetNodesRef
{
  STATIC_CONST( FixedSignalLength = 3 );
  Uint32 tableId;
  Uint32 fragCnt;
  Uint32 errCode;

  /**
   * DihScanGetNodesRef signals failure of a DihScanGetNodesReq.
   * As this is likely due to a sectioned memory alloc failure,
   * we avoid further alloc problems by returning the same FragItem[]
   * list as in the DihScanGetNodesReq.
   *
   * Depending on 'fragCnt', the fragItem[] is either:
   * - These are either specified in a seperate section (long request)
   *   containing multiple FragItems.
   * - Or directly in a single fragItem[] below (short signal) if it 
   *   contain only a single FragItem.
   */
  typedef DihScanGetNodesReq::FragItem FragItem; // Reused, see above

  FragItem fragItem[1];
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


#undef JAM_FILE_ID

#endif

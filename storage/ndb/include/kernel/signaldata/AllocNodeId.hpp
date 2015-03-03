/*
   Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ALLOC_NODE_ID_HPP
#define ALLOC_NODE_ID_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 26


/**
 * Request to allocate node id
 */
class AllocNodeIdReq {
public:
  STATIC_CONST( SignalLength = 5 );
  STATIC_CONST( SignalLengthQMGR = 7 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 nodeId;
  Uint32 nodeType;
  Uint32 timeout;

  Uint32 secret_lo;
  Uint32 secret_hi;
};

class AllocNodeIdConf {
public:
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 nodeId;
  Uint32 secret_lo;
  Uint32 secret_hi;
};

class AllocNodeIdRef {
public:
  STATIC_CONST( SignalLength = 5 );

  enum ErrorCodes {
    NoError = 0,
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy  = 701,
    NotMaster  = 702,
    NodeReserved = 1701,
    NodeConnected = 1702,
    NodeFailureHandlingNotCompleted = 1703,
    NodeTypeMismatch = 1704
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 nodeId;
  Uint32 errorCode;
  Uint32 masterRef;
};

class AllocNodeIdRep
{
private:
  friend class Dbdih;
  friend class Qmgr;
  STATIC_CONST( SignalLength = 1);

  Uint32 nodeId;
};

#undef JAM_FILE_ID

#endif

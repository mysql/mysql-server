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

#ifndef ALLOC_NODE_ID_HPP
#define ALLOC_NODE_ID_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * Request to allocate node id
 */
class AllocNodeIdReq {
public:
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 nodeId;
  Uint32 nodeType;
};

class AllocNodeIdConf {
public:
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 nodeId;
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
#endif

/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COPY_FRAG_HPP
#define COPY_FRAG_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 45


class CopyFragReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
public:
  STATIC_CONST( SignalLength = 11 );

private:

  enum
  {
    CFR_TRANSACTIONAL = 1,    // Copy rows >= gci in transactional fashion
    CFR_NON_TRANSACTIONAL = 2 // Copy rows <= gci in non transactional fashion
  };
  union {
    Uint32 userPtr;
    Uint32 senderData;
  };
  union {
    Uint32 userRef;
    Uint32 senderRef;
  };
  Uint32 tableId;
  Uint32 fragId;
  Uint32 nodeId;
  Uint32 schemaVersion;
  Uint32 distributionKey;
  Uint32 gci;
  Uint32 nodeCount;
  Uint32 nodeList[1];
  //Uint32 maxPage; is stored in nodeList[nodeCount]
  //Uint32 requestInfo is stored after maxPage
};

class CopyFragConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 7 );

private:
  union {
    Uint32 userPtr;
    Uint32 senderData;
  };
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 rows_lo;
  Uint32 bytes_lo;
};
class CopyFragRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 6 );

private:
  Uint32 userPtr;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 errorCode;
};

struct UpdateFragDistKeyOrd
{
  Uint32 tableId;
  Uint32 fragId;
  Uint32 fragDistributionKey;

  STATIC_CONST( SignalLength = 3 );
};

struct PrepareCopyFragReq
{
  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 copyNodeId;
  Uint32 startingNodeId;
};

struct PrepareCopyFragRef
{
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 copyNodeId;
  Uint32 startingNodeId;
  Uint32 errorCode;

  STATIC_CONST( SignalLength = 7 );
};

struct PrepareCopyFragConf
{
  STATIC_CONST( SignalLength = 7 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 copyNodeId;
  Uint32 startingNodeId;
  Uint32 maxPageNo;
};


#undef JAM_FILE_ID

#endif

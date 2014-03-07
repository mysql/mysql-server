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

#ifndef COPY_ACTIVE_HPP
#define COPY_ACTIVE_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 175


class CopyActiveReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
public:
  STATIC_CONST( SignalLength = 6 );

  enum Flags
  {
    CAR_NO_WAIT = 0x1
    ,CAR_NO_LOGGING = 0x2
  };

private:
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 distributionKey;
  Uint32 flags;
};

class CopyActiveConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 5 );

private:
  Uint32 userPtr;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 startGci;
};
class CopyActiveRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 5 );

private:
  Uint32 userPtr;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif

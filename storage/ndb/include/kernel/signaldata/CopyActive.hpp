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
  static constexpr Uint32 SignalLength = 6;

  enum Flags
  {
    CAR_NO_WAIT = 0x1
    ,CAR_NO_LOGGING = 0x2
    ,CAR_LOCAL_SEND = 0x4
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
  static constexpr Uint32 SignalLength = 5;

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
  static constexpr Uint32 SignalLength = 5;

private:
  Uint32 userPtr;
  Uint32 startingNodeId;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif

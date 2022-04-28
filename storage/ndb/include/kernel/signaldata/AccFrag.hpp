/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef ACC_FRAG_HPP
#define ACC_FRAG_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 166


class AccFragReq {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbacc;
public:
  static constexpr Uint32 SignalLength = 12;

private:
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 tableId;
  Uint32 reqInfo;
  Uint32 fragId;
  Uint32 localKeyLen;
  Uint32 maxLoadFactor;
  Uint32 minLoadFactor;
  Uint32 kValue;
  Uint32 lhFragBits;
  Uint32 lhDirBits;
  Uint32 keyLength;
};

class AccFragConf {
  /**
   * Sender(s)
   */
  friend class Dbacc;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
public:
  static constexpr Uint32 SignalLength = 7;

private:
  Uint32 userPtr;
  Uint32 rootFragPtr;
  Uint32 fragId[2];
  Uint32 fragPtr[2];
  Uint32 rootHashCheck;
};

class AccFragRef {
  /**
   * Sender(s)
   */
  friend class Dbacc;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
public:
  static constexpr Uint32 SignalLength = 2;

private:
  Uint32 userPtr;
  Uint32 errorCode;
};


#undef JAM_FILE_ID

#endif

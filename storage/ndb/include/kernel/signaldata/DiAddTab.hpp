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

#ifndef DIADDTABREQ_HPP
#define DIADDTABREQ_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 210


class DiAddTabReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
public:
  static constexpr Uint32 SignalLength = 14;
  SECTION( FRAGMENTATION = 0 );
  SECTION( TS_RANGE = 0 );
  
private:
  Uint32 connectPtr;
  Uint32 tableId;
  Uint32 fragType;
  Uint32 kValue;
  Uint32 noOfReplicas; //Currently not used
  Uint32 loggedTable;
  Uint32 tableType;
  Uint32 schemaVersion;
  Uint32 primaryTableId;
  Uint32 temporaryTable;
  Uint32 schemaTransId;
  Uint32 hashMapPtrI;
  Uint32 fullyReplicated;
  Uint32 partitionCount;
};

class DiAddTabRef {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;
public:
  static constexpr Uint32 SignalLength = 2;
  
private:
  union {
    Uint32 connectPtr;
    Uint32 senderData;
  };
  Uint32 errorCode;
};

class DiAddTabConf {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;
public:
  static constexpr Uint32 SignalLength = 1;
  
private:
  union {
    Uint32 connectPtr;
    Uint32 senderData;
  };
};



#undef JAM_FILE_ID

#endif

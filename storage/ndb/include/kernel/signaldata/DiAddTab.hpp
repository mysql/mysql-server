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

#ifndef DIADDTABREQ_HPP
#define DIADDTABREQ_HPP

#include "SignalData.hpp"

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
  STATIC_CONST( SignalLength = 10 );
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
  STATIC_CONST( SignalLength = 2 );
  
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
  STATIC_CONST( SignalLength = 1 );
  
private:
  union {
    Uint32 connectPtr;
    Uint32 senderData;
  };
};


#endif

/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CREATE_FRAGMENTATION_REQ_HPP
#define CREATE_FRAGMENTATION_REQ_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 80


class CreateFragmentationReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbdih;
  
  friend bool printCREATE_FRAGMENTATION_REQ(FILE *, 
					    const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 8 );
  
  enum RequestInfo {
    RI_CREATE_FRAGMENTATION = 0x0,
    RI_ADD_FRAGMENTS = 0x1,
    RI_GET_FRAGMENTATION = 0x2
  };
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;
  Uint32 fragmentationType;
  Uint32 noOfFragments;
  Uint32 primaryTableId;  // use same fragmentation as this table if not RNIL
  Uint32 map_ptr_i;
  Uint32 partitionBalance;
  Uint32 partitionCount;
};

class CreateFragmentationRef {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;
  
  friend bool printCREATE_FRAGMENTATION_REF(FILE *, 
					    const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
 
  enum ErrorCode {
    OK = 0
    ,InvalidNodeGroup = 771
    ,InvalidFragmentationType = 772
    ,InvalidPrimaryTable = 749
  };
 
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

class CreateFragmentationConf {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;
  
  friend bool printCREATE_FRAGMENTATION_CONF(FILE *, 
					     const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  SECTION( FRAGMENTS = 0 );
  
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 noOfReplicas;
  Uint32 noOfFragments;
};


#undef JAM_FILE_ID

#endif

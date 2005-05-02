/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CREATE_FRAGMENTATION_REQ_HPP
#define CREATE_FRAGMENTATION_REQ_HPP

#include "SignalData.hpp"

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
  STATIC_CONST( SignalLength = 6 );
  
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 fragmentationType;
  Uint32 noOfFragments;
  Uint32 fragmentNode;
  Uint32 primaryTableId;  // use same fragmentation as this table if not RNIL
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
    ,InvalidFragmentationType = 1
    ,InvalidNodeId = 2
    ,InvalidNodeType = 3
    ,InvalidPrimaryTable = 4
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

#endif

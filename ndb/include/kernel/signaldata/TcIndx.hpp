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

#ifndef TC_INDX_H
#define TC_INDX_H

#include "SignalData.hpp"
#include "TcKeyReq.hpp"

class TcIndxConf {

  /**
   * Reciver(s)
   */
  friend class Ndb;
  friend class NdbConnection;

  /**
   * Sender(s)
   */
  friend class Dbtc; 

  /**
   * For printing
   */
  friend bool printTCINDXCONF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 5 );

private:
  /**
   * DATA VARIABLES
   */
  //-------------------------------------------------------------
  // Unconditional part. First 5 words
  //-------------------------------------------------------------

  Uint32 apiConnectPtr;
  Uint32 gci;
  Uint32 confInfo;
  Uint32 transId1;
  Uint32 transId2;

  struct OperationConf {
    Uint32 apiOperationPtr;
    Uint32 attrInfoLen;
  };
  //-------------------------------------------------------------
  // Operations confirmations,
  // No of actually sent = getNoOfOperations(confInfo)
  //-------------------------------------------------------------
  OperationConf operations[10];
  
  /**
   * Get:ers for confInfo
   */
  static Uint32 getNoOfOperations(const Uint32 & confInfo);
  static Uint32 getCommitFlag(const Uint32 & confInfo);
  static bool getMarkerFlag(const Uint32 & confInfo);
  
  /**
   * Set:ers for confInfo
   */
  static void setCommitFlag(Uint32 & confInfo, Uint8 flag);
  static void setNoOfOperations(Uint32 & confInfo, Uint32 noOfOps);
  static void setMarkerFlag(Uint32 & confInfo, Uint32 flag);
};

inline
Uint32
TcIndxConf::getNoOfOperations(const Uint32 & confInfo){
  return confInfo & 65535;
}

inline
Uint32
TcIndxConf::getCommitFlag(const Uint32 & confInfo){
  return ((confInfo >> 16) & 1);
}

inline
bool
TcIndxConf::getMarkerFlag(const Uint32 & confInfo){
  const Uint32 bits = 3 << 16; // Marker only valid when doing commit
  return (confInfo & bits) == bits;
}

inline
void 
TcIndxConf::setNoOfOperations(Uint32 & confInfo, Uint32 noOfOps){
  ASSERT_MAX(noOfOps, 65535, "TcIndxConf::setNoOfOperations");
  confInfo |= noOfOps;
}

inline
void 
TcIndxConf::setCommitFlag(Uint32 & confInfo, Uint8 flag){
  ASSERT_BOOL(flag, "TcIndxConf::setCommitFlag");
  confInfo |= (flag << 16);
}

inline
void
TcIndxConf::setMarkerFlag(Uint32 & confInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxConf::setMarkerFlag");
  confInfo |= (flag << 17);
}

#endif

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

#ifndef ACC_SCAN_HPP
#define ACC_SCAN_HPP

#include "SignalData.hpp"

/*
 * Used by ACC and TUX scan.
 */

class AccScanReq {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Reciver(s)
   */
  friend class Dbacc;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 8 );
  
private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentNo;
  Uint32 requestInfo;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 savePointId;

  /**
   * Previously there where also a scan type
   */
  static Uint32 getLockMode(const Uint32 & requestInfo);
  static Uint32 getReadCommittedFlag(const Uint32 & requestInfo);
  
  static void setLockMode(Uint32 & requestInfo, Uint32 lockMode);
  static void setReadCommittedFlag(Uint32 & requestInfo, Uint32 readCommitted);
};

/**
 * Request Info
 *
 * l = Lock Mode             - 1  Bit 2
 * h = Read Committed        - 1  Bit 5
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *   l  h    
 */
#define AS_LOCK_MODE_SHIFT       (2)
#define AS_LOCK_MODE_MASK        (1)
#define AS_READ_COMMITTED_SHIFT  (5)

inline 
Uint32
AccScanReq::getLockMode(const Uint32 & requestInfo){
  return (requestInfo >> AS_LOCK_MODE_SHIFT) & AS_LOCK_MODE_MASK;
}

inline
Uint32
AccScanReq::getReadCommittedFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_READ_COMMITTED_SHIFT) & 1;
}

inline
void
AccScanReq::setLockMode(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, AS_LOCK_MODE_MASK, "AccScanReq::setLockMode");
  requestInfo |= (val << AS_LOCK_MODE_SHIFT);
}

inline
void
AccScanReq::setReadCommittedFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setReadCommittedFlag");
  requestInfo |= (val << AS_READ_COMMITTED_SHIFT);
}

class AccScanConf {
  /**
   * Sender(s)
   */
  friend class Dbacc;
  friend class Dbtux;

  /**
   * Reciver(s)
   */
  friend class Dblqh;

  enum {
    ZEMPTY_FRAGMENT = 0,
    ZNOT_EMPTY_FRAGMENT = 1
  };

public:
  STATIC_CONST( SignalLength = 8 );
  
private:
  Uint32 scanPtr;
  Uint32 accPtr;
  Uint32 unused1;
  Uint32 unused2;
  Uint32 unused3;
  Uint32 unused4;
  Uint32 unused5;
  Uint32 flag;
};

class AccCheckScan {
  friend class Dbacc;
  friend class Dbtux;
  friend class Dblqh;
  enum {
    ZCHECK_LCP_STOP = 0,
    ZNOT_CHECK_LCP_STOP = 1
  };
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 accPtr;                // scanptr.i in ACC or TUX
  Uint32 checkLcpStop;          // from enum
};

#endif

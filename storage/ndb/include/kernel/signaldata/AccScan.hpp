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

#ifndef ACC_SCAN_HPP
#define ACC_SCAN_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 121


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
  friend class Dbtup;
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
  union {
    Uint32 savePointId;
    Uint32 gci;
  };
  Uint32 maxPage;

  /**
   * Previously there where also a scan type
   */
  static Uint32 getLockMode(const Uint32 & requestInfo);
  static Uint32 getReadCommittedFlag(const Uint32 & requestInfo);
  static Uint32 getDescendingFlag(const Uint32 & requestInfo);
  
  static void setLockMode(Uint32 & requestInfo, Uint32 lockMode);
  static void setReadCommittedFlag(Uint32 & requestInfo, Uint32 readCommitted);
  static void setDescendingFlag(Uint32 & requestInfo, Uint32 descending);

  static Uint32 getNoDiskScanFlag(const Uint32 & requestInfo);
  static void setNoDiskScanFlag(Uint32 & requestInfo, Uint32 nodisk);

  static Uint32 getNRScanFlag(const Uint32 & requestInfo);
  static void setNRScanFlag(Uint32 & requestInfo, Uint32 nr);

  static Uint32 getLcpScanFlag(const Uint32 & requestInfo);
  static void setLcpScanFlag(Uint32 & requestInfo, Uint32 nr);

  static Uint32 getStatScanFlag(const Uint32 & requestInfo);
  static void setStatScanFlag(Uint32 & requestInfo, Uint32 nr);
};

/**
 * Request Info
 *
 * l = Lock Mode             - 1  Bit 2
 * h = Read Committed        - 1  Bit 5
 * z = Descending (TUX)      - 1  Bit 6
 * d = No disk scan          - 1  Bit 7
 * n = Node recovery scan    - 1  Bit 8
 * c = LCP scan              - 1  Bit 9
 * s = Statistics scan       - 1  Bit 4
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *   l shzdn   
 */
#define AS_LOCK_MODE_SHIFT       (2)
#define AS_LOCK_MODE_MASK        (1)
#define AS_READ_COMMITTED_SHIFT  (5)
#define AS_DESCENDING_SHIFT      (6)
#define AS_NO_DISK_SCAN          (7)
#define AS_NR_SCAN               (8)
#define AS_LCP_SCAN              (9)
#define AS_STAT_SCAN             (4)

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
Uint32
AccScanReq::getDescendingFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_DESCENDING_SHIFT) & 1;
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

inline
void
AccScanReq::setDescendingFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setDescendingFlag");
  requestInfo |= (val << AS_DESCENDING_SHIFT);
}

inline
Uint32
AccScanReq::getNoDiskScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_NO_DISK_SCAN) & 1;
}

inline
void
AccScanReq::setNoDiskScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setNoDiskScanFlag");
  requestInfo |= (val << AS_NO_DISK_SCAN);
}

inline
Uint32
AccScanReq::getNRScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_NR_SCAN) & 1;
}

inline
void
AccScanReq::setNRScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setNoDiskScanFlag");
  requestInfo |= (val << AS_NR_SCAN);
}

inline
Uint32
AccScanReq::getLcpScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_LCP_SCAN) & 1;
}

inline
void
AccScanReq::setLcpScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setNoDiskScanFlag");
  requestInfo |= (val << AS_LCP_SCAN);
}

inline
Uint32
AccScanReq::getStatScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_STAT_SCAN) & 1;
}

inline
void
AccScanReq::setStatScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setStatScanScanFlag");
  requestInfo |= (val << AS_STAT_SCAN);
}

class AccScanConf {
  /**
   * Sender(s)
   */
  friend class Dbacc;
  friend class Dbtux;
  friend class Dbtup;

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

class AccScanRef {
  friend class Dbtux;
  friend class Dblqh;

  enum ErrorCode {
    TuxNoFreeScanOp = 909,
    TuxIndexNotOnline = 910,
    TuxNoFreeStatOp = 911,
    TuxInvalidLockMode = 912
  };

public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 scanPtr;
  Uint32 accPtr;
  Uint32 errorCode;
};

class AccCheckScan {
  friend class Dbacc;
  friend class Dbtux;
  friend class Dbtup;
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


#undef JAM_FILE_ID

#endif

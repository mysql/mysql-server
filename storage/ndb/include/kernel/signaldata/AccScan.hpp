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

#ifndef ACC_SCAN_HPP
#define ACC_SCAN_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 121


/*
 * Used by ACC and TUX and TUP scan.
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
  static constexpr Uint32 SignalLength = 8;
  
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

  static Uint32 getCopyFragScanFlag(const Uint32 & requestInfo);
  static void setCopyFragScanFlag(Uint32 & requestInfo, Uint32 nr);
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
 * f = Copy fragment scan    - 1  Bit 10
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *   l shzdncf   
 */
#define AS_LOCK_MODE_SHIFT       (2)
#define AS_LOCK_MODE_MASK        (1)
#define AS_READ_COMMITTED_SHIFT  (5)
#define AS_DESCENDING_SHIFT      (6)
#define AS_NO_DISK_SCAN          (7)
#define AS_NR_SCAN               (8)
#define AS_LCP_SCAN              (9)
#define AS_STAT_SCAN             (4)
#define AS_COPY_FRAG_SCAN        (10)

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

inline
Uint32
AccScanReq::getCopyFragScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> AS_COPY_FRAG_SCAN) & 1;
}

inline
void
AccScanReq::setCopyFragScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "AccScanReq::setCopyFragScanFlag");
  requestInfo |= (val << AS_COPY_FRAG_SCAN);
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
  static constexpr Uint32 SignalLength = 8;
  
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
  friend class Dbtup;
  friend class Dbacc;

  enum ErrorCode {
    TuxNoFreeScanOp = 909,
    TuxIndexNotOnline = 910,
    TuxInvalidLockMode = 912,
    TuxNoFreeStatOp = 915,
    TupNoFreeScanOp = 925,
    AccNoFreeScanOp = 926,
  };

public:
  static constexpr Uint32 SignalLength = 3;

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
    ZCHECK_LCP_STOP     = 0,   // Execution should check-in with LQH
    ZNOT_CHECK_LCP_STOP = 1    // Execution should not check-in with LQH
  };

public:
  static constexpr Uint32 SignalLength = 2;
private:
  Uint32 accPtr;                // scanptr.i in ACC/TUX/TUP
  Uint32 checkLcpStop;          // from enum
};

class CheckLcpStop
{
  friend class Dbacc;
  friend class Dbtux;
  friend class Dbtup;
  friend class Dblqh;

  enum ScanState
  {
    ZSCAN_RUNNABLE = 0,               // Scan runnable immediately
    ZSCAN_RESOURCE_WAIT = 1,          // Scan waiting for something
    ZSCAN_RUNNABLE_YIELD = 2,         // Scan runnable, yielding cpu
    ZSCAN_RESOURCE_WAIT_STOPPABLE = 3 // Scan waiting for something
  };

  enum Reply
  {
    // In signal[0] after EXECUTE_DIRECT
    ZTAKE_A_BREAK = RNIL,
    ZABORT_SCAN = 0
  };
public:
  static constexpr Uint32 SignalLength = 2;
private:
  Uint32 scanPtrI;            // scanptr.i from ACC/TUX/TUP
  Uint32 scanState;
};

#undef JAM_FILE_ID

#endif

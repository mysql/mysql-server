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

#ifndef SCAN_FRAG_HPP
#define SCAN_FRAG_HPP

#include "SignalData.hpp"
#include "ndb_limits.h"

class ScanFragReq {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Backup;
  friend class Suma;

  /**
   * Reciver(s)
   */
  friend class Dblqh;
public:
  STATIC_CONST( SignalLength = 12 );
  
  friend bool printSCAN_FRAGREQ(FILE *, const Uint32*, Uint32, Uint16);
  
public:
  Uint32 senderData;
  Uint32 resultRef;       // Where to send the result
  Uint32 savePointId;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 fragmentNoKeyLen;
  Uint32 schemaVersion;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 clientOpPtr;
  Uint32 batch_size_rows;
  Uint32 batch_size_bytes;
  
  static Uint32 getLockMode(const Uint32 & requestInfo);
  static Uint32 getHoldLockFlag(const Uint32 & requestInfo);
  static Uint32 getKeyinfoFlag(const Uint32 & requestInfo);
  static Uint32 getReadCommittedFlag(const Uint32 & requestInfo);
  static Uint32 getRangeScanFlag(const Uint32 & requestInfo);
  static Uint32 getAttrLen(const Uint32 & requestInfo);
  static Uint32 getScanPrio(const Uint32 & requestInfo);
  
  static void setLockMode(Uint32 & requestInfo, Uint32 lockMode);
  static void setHoldLockFlag(Uint32 & requestInfo, Uint32 holdLock);
  static void setKeyinfoFlag(Uint32 & requestInfo, Uint32 keyinfo);
  static void setReadCommittedFlag(Uint32 & requestInfo, Uint32 readCommitted);
  static void setRangeScanFlag(Uint32 & requestInfo, Uint32 rangeScan);
  static void setAttrLen(Uint32 & requestInfo, Uint32 attrLen);
  static void setScanPrio(Uint32& requestInfo, Uint32 prio);
};

class KeyInfo20 {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class NdbOperation;
  friend class NdbScanReceiver;
public:
  STATIC_CONST( HeaderLength = 5);
  STATIC_CONST( DataLength = 20 );

  
  static Uint32 setScanInfo(Uint32 noOfOps, Uint32 scanNo);
  static Uint32 getScanNo(Uint32 scanInfo);
  static Uint32 getScanOp(Uint32 scanInfo);
  
public:
  Uint32 clientOpPtr;
  Uint32 keyLen;
  Uint32 scanInfo_Node;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 keyData[DataLength];
};

class ScanFragConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Reciver(s)
   */
  friend class Dbtc;
  friend class Backup;
  friend class Suma;
public:
  STATIC_CONST( SignalLength = 6 );
  
public:
  Uint32 senderData;
  Uint32 completedOps;
  Uint32 fragmentCompleted;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 total_len;
};

class ScanFragRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Reciver(s)
   */
  friend class Dbtc;
  friend class Backup;
  friend class Suma;
public:
  STATIC_CONST( SignalLength = 4 );
public:
  enum ErrorCode {
    ZNO_FREE_TC_CONREC_ERROR = 484,
    ZTOO_FEW_CONCURRENT_OPERATIONS = 485,
    ZTOO_MANY_CONCURRENT_OPERATIONS = 486,
    ZSCAN_NO_FRAGMENT_ERROR = 487,
    ZTOO_MANY_ACTIVE_SCAN_ERROR = 488,
    ZNO_FREE_SCANREC_ERROR = 489,
    ZWRONG_BATCH_SIZE = 1230,
    ZSTANDBY_SCAN_ERROR = 1209,
    ZSCAN_BOOK_ACC_OP_ERROR = 1219,
    ZUNKNOWN_TRANS_ERROR = 1227
  };
  
  Uint32 senderData;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 errorCode;
};

/**
 * This is part of Scan Fragment protocol
 *
 * Not to be confused with ScanNextReq in Scan Table protocol
 */
class ScanFragNextReq {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Backup;
  friend class Suma;
  
  /**
   * Reciver(s)
   */
  friend class Dblqh;

  friend bool printSCANFRAGNEXTREQ(FILE * output, const Uint32 * theData, 
				   Uint32 len, Uint16 receiverBlockNo);
public:
  STATIC_CONST( SignalLength = 6 );
  
public:
  Uint32 senderData;
  Uint32 closeFlag;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 batch_size_rows;
  Uint32 batch_size_bytes;
};

/**
 * Request Info
 *
 * a = Length of attrinfo    - 16 Bits (16-31)
 * l = Lock Mode             - 1  Bit 5
 * h = Hold lock             - 1  Bit 7
 * k = Keyinfo               - 1  Bit 8
 * r = read committed        - 1  Bit 9
 * x = range scan            - 1  Bit 6
 * p = Scan prio             - 4  Bits (12-15) -> max 15
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *      lxhkr  ppppaaaaaaaaaaaaaaaa 
 */
#define SF_LOCK_MODE_SHIFT   (5)
#define SF_LOCK_MODE_MASK    (1)

#define SF_HOLD_LOCK_SHIFT   (7)
#define SF_KEYINFO_SHIFT     (8)
#define SF_READ_COMMITTED_SHIFT  (9)
#define SF_RANGE_SCAN_SHIFT (6)

#define SF_ATTR_LEN_SHIFT    (16)
#define SF_ATTR_LEN_MASK     (65535)

#define SF_PRIO_SHIFT 12
#define SF_PRIO_MASK 15

inline 
Uint32
ScanFragReq::getLockMode(const Uint32 & requestInfo){
  return (requestInfo >> SF_LOCK_MODE_SHIFT) & SF_LOCK_MODE_MASK;
}

inline
Uint32
ScanFragReq::getHoldLockFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_HOLD_LOCK_SHIFT) & 1;
}

inline
Uint32
ScanFragReq::getKeyinfoFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_KEYINFO_SHIFT) & 1;
}

inline
Uint32
ScanFragReq::getRangeScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_RANGE_SCAN_SHIFT) & 1;
}

inline
Uint32
ScanFragReq::getReadCommittedFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_READ_COMMITTED_SHIFT) & 1;
}

inline 
Uint32
ScanFragReq::getAttrLen(const Uint32 & requestInfo){
  return (requestInfo >> SF_ATTR_LEN_SHIFT) & SF_ATTR_LEN_MASK;
}

inline
Uint32
ScanFragReq::getScanPrio(const Uint32 & requestInfo){
  return (requestInfo >> SF_PRIO_SHIFT) & SF_PRIO_MASK;
}

inline
void
ScanFragReq::setScanPrio(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, SF_PRIO_MASK, "ScanFragReq::setScanPrio");
  requestInfo |= (val << SF_PRIO_SHIFT);
}

inline
void
ScanFragReq::setLockMode(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, SF_LOCK_MODE_MASK, "ScanFragReq::setLockMode");
  requestInfo |= (val << SF_LOCK_MODE_SHIFT);
}

inline
void
ScanFragReq::setHoldLockFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setHoldLockFlag");
  requestInfo |= (val << SF_HOLD_LOCK_SHIFT);
}

inline
void
ScanFragReq::setKeyinfoFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setKeyinfoFlag");
  requestInfo |= (val << SF_KEYINFO_SHIFT);
}

inline
void
ScanFragReq::setReadCommittedFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setReadCommittedFlag");
  requestInfo |= (val << SF_READ_COMMITTED_SHIFT);
}

inline
void
ScanFragReq::setRangeScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setRangeScanFlag");
  requestInfo |= (val << SF_RANGE_SCAN_SHIFT);
}

inline
void
ScanFragReq::setAttrLen(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, SF_ATTR_LEN_MASK, "ScanFragReq::setAttrLen");
  requestInfo |= (val << SF_ATTR_LEN_SHIFT);
}

inline
Uint32
KeyInfo20::setScanInfo(Uint32 opNo, Uint32 scanNo){
  ASSERT_MAX(opNo, 1023, "KeyInfo20::setScanInfo");
  ASSERT_MAX(scanNo, 255, "KeyInfo20::setScanInfo");
  return (opNo << 8) + scanNo;
}

inline
Uint32
KeyInfo20::getScanNo(Uint32 scanInfo){
  return scanInfo & 0xFF;
}

inline
Uint32
KeyInfo20::getScanOp(Uint32 scanInfo){
  return (scanInfo >> 8) & 0x3FF;
}

#endif

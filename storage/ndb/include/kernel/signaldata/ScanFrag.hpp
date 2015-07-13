/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SCAN_FRAG_HPP
#define SCAN_FRAG_HPP

#include "SignalData.hpp"
#include "ndb_limits.h"

#define JAM_FILE_ID 134


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
  friend class Dbspj;
public:
  STATIC_CONST( SignalLength = 12 );

  STATIC_CONST( AttrInfoSectionNum = 0 );
  STATIC_CONST( KeyInfoSectionNum = 1 );
  
  friend bool printSCAN_FRAGREQ(FILE *, const Uint32*, Uint32, Uint16);
  friend bool printSCAN_FRAGCONF(FILE *, const Uint32*, Uint32, Uint16);
  
public:
  enum ReorgFlag
  {
    REORG_ALL = 0
    ,REORG_NOT_MOVED = 1 // Only return not moved rows
    ,REORG_MOVED = 2    // Only return moved rows
  };

  Uint32 senderData;
  Uint32 resultRef;       // Where to send the result
  Uint32 savePointId;
  Uint32 requestInfo;
  Uint32 tableId;
  Uint32 fragmentNoKeyLen;
  Uint32 schemaVersion;
  Uint32 transId1;
  Uint32 transId2;
  union {
    Uint32 clientOpPtr;
    Uint32 resultData;
  };
  Uint32 batch_size_rows;
  Uint32 batch_size_bytes;
  Uint32 variableData[1];
  
  static Uint32 getLockMode(const Uint32 & requestInfo);
  static Uint32 getHoldLockFlag(const Uint32 & requestInfo);
  static Uint32 getKeyinfoFlag(const Uint32 & requestInfo);
  static Uint32 getReadCommittedFlag(const Uint32 & requestInfo);
  static Uint32 getRangeScanFlag(const Uint32 & requestInfo);
  static Uint32 getDescendingFlag(const Uint32 & requestInfo);
  static Uint32 getTupScanFlag(const Uint32 & requestInfo);
  static Uint32 getAttrLen(const Uint32 & requestInfo);
  static Uint32 getScanPrio(const Uint32 & requestInfo);
  static Uint32 getNoDiskFlag(const Uint32 & requestInfo);
  static Uint32 getLcpScanFlag(const Uint32 & requestInfo);
  static Uint32 getStatScanFlag(const Uint32 & requestInfo);
  static Uint32 getPrioAFlag(const Uint32 & requestInfo);
  /**
   * To ensure backwards compatibility we set the flag when NOT using
   * interpreted mode, previously scans always used interpreted mode. Now
   * it is possible to perform scans (especially LCP scans and Backup
   * scans) without using the interpreted programs. This way the code will
   * interact nicely with old code that always set this flag to 0 and want
   * to use interpreted execution based on that.
   */
  static Uint32 getNotInterpretedFlag(const Uint32 & requestInfo);

  static void setLockMode(Uint32 & requestInfo, Uint32 lockMode);
  static void setHoldLockFlag(Uint32 & requestInfo, Uint32 holdLock);
  static void setKeyinfoFlag(Uint32 & requestInfo, Uint32 keyinfo);
  static void setReadCommittedFlag(Uint32 & requestInfo, Uint32 readCommitted);
  static void setRangeScanFlag(Uint32 & requestInfo, Uint32 rangeScan);
  static void setDescendingFlag(Uint32 & requestInfo, Uint32 descending);
  static void setTupScanFlag(Uint32 & requestInfo, Uint32 tupScan);
  static void setAttrLen(Uint32 & requestInfo, Uint32 attrLen);
  static void clearAttrLen(Uint32 & requestInfo);
  static void setScanPrio(Uint32& requestInfo, Uint32 prio);
  static void setNoDiskFlag(Uint32& requestInfo, Uint32 val);
  static void setLcpScanFlag(Uint32 & requestInfo, Uint32 val);
  static void setStatScanFlag(Uint32 & requestInfo, Uint32 val);
  static void setPrioAFlag(Uint32 & requestInfo, Uint32 val);
  static void setNotInterpretedFlag(Uint32 & requestInfo, Uint32 val);

  static void setReorgFlag(Uint32 & requestInfo, Uint32 val);
  static Uint32 getReorgFlag(const Uint32 & requestInfo);

  static void setCorrFactorFlag(Uint32 & requestInfo, Uint32 val);
  static Uint32 getCorrFactorFlag(const Uint32 & requestInfo);
};

/*
  The KEYINFO20 signal is sent from LQH to API for each row in a scan when the
  ScanTabReq::getKeyinfoFlag() is set in requestInfo in the SCAN_TABREQ signal.

  The '20' in the signal name refers to the number of keyInfo data words in
  the signal, which is actually a bit misleading since now it is sent as a
  single long signal if the keyinfo has more than 20 words.

  The information in this signal is used in the NDB API to request the take
  over of a lock from the scan with a TCKEYREQ, using the primary key info
  sent as data and the scanInfo_Node word to identify the lock.
*/
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
  /*
    The scanInfo_Node word contains the information needed to identify the
    row and lock to take over in the TCKEYREQ signal. It has two parts:
     1. ScanInfo      Lower 20 bits
     2. ScanFragment  Upper 14 bits
  */
  Uint32 scanInfo_Node;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 keyData[DataLength];
  /*
    Note that if the key info data does not fit within the maximum of 20
    in-signal words, the entire key info is instead sent in long signal
    section 0.
    The data here is a word string suitable for sending as KEYINFO in
    the TCKEYREQ signal.
  */
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
  Uint32 total_len;  // Total #Uint32 returned as TRANSID_AI
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
    TABLE_NOT_DEFINED_ERROR = 723,
    DROP_TABLE_IN_PROGRESS_ERROR = 1226, /* Reported on LCP scans */
    ZWRONG_BATCH_SIZE = 1230,
    ZSTANDBY_SCAN_ERROR = 1209,
    NO_TC_CONNECT_ERROR = 1217,
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
  Uint32 requestInfo;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 batch_size_rows;
  Uint32 batch_size_bytes;
  Uint32 variableData[1];

  static Uint32 getCloseFlag(const Uint32&);
  static void setCloseFlag(Uint32&, Uint32);

  static Uint32 getPrioAFlag(const Uint32&);
  static void setPrioAFlag(Uint32&, Uint32);

  static Uint32 getCorrFactorFlag(const Uint32&);
  static void setCorrFactorFlag(Uint32&);
};

/**
 * Request Info (SCANFRAGREQ)
 *
 * a = Length of attrinfo    - 16 Bits (16-31) (Short only)
 * c = LCP scan              - 1  Bit 3
 * d = No disk               - 1  Bit 4
 * l = Lock Mode             - 1  Bit 5
 * h = Hold lock             - 1  Bit 7
 * k = Keyinfo               - 1  Bit 8
 * r = read committed        - 1  Bit 9
 * x = range scan            - 1  Bit 6
 * z = descending            - 1  Bit 10
 * t = tup scan              - 1  Bit 11 (implies x=z=0)
 * p = Scan prio             - 4  Bits (12-15) -> max 15
 * r = Reorg flag            - 2  Bits (1-2)
 * C = corr value flag       - 1  Bit  (16)
 * s = Stat scan             - 1  Bit 17
 * a = Prio A scan           - 1  Bit 18
 * i = Not interpreted flag  - 1  Bit 19
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 *  rrcdlxhkrztppppaaaaaaaaaaaaaaaa   Short variant ( < 6.4.0)
 *  rrcdlxhkrztppppCs                 Long variant (6.4.0 +)
 */
#define SF_LOCK_MODE_SHIFT   (5)
#define SF_LOCK_MODE_MASK    (1)

#define SF_NO_DISK_SHIFT     (4)
#define SF_HOLD_LOCK_SHIFT   (7)
#define SF_KEYINFO_SHIFT     (8)
#define SF_READ_COMMITTED_SHIFT  (9)
#define SF_RANGE_SCAN_SHIFT (6)
#define SF_DESCENDING_SHIFT (10)
#define SF_TUP_SCAN_SHIFT   (11)
#define SF_LCP_SCAN_SHIFT   (3)

#define SF_ATTR_LEN_SHIFT    (16)
#define SF_ATTR_LEN_MASK     (65535)

#define SF_PRIO_SHIFT 12
#define SF_PRIO_MASK 15

#define SF_REORG_SHIFT      (1)
#define SF_REORG_MASK       (3)

#define SF_CORR_FACTOR_SHIFT  (16)

#define SF_STAT_SCAN_SHIFT  (17)
#define SF_PRIO_A_SHIFT     (18)
#define SF_NOT_INTERPRETED_SHIFT (19)

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
ScanFragReq::getDescendingFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_DESCENDING_SHIFT) & 1;
}

inline
Uint32
ScanFragReq::getTupScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_TUP_SCAN_SHIFT) & 1;
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
ScanFragReq::setDescendingFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setDescendingFlag");
  requestInfo |= (val << SF_DESCENDING_SHIFT);
}

inline
void
ScanFragReq::setTupScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setTupScanFlag");
  requestInfo |= (val << SF_TUP_SCAN_SHIFT);
}

inline
void
ScanFragReq::setAttrLen(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, SF_ATTR_LEN_MASK, "ScanFragReq::setAttrLen");
  requestInfo |= (val << SF_ATTR_LEN_SHIFT);
}

inline
void
ScanFragReq::clearAttrLen(Uint32 & requestInfo)
{
  requestInfo &= ~((Uint32)SF_ATTR_LEN_MASK << SF_ATTR_LEN_SHIFT);
}

inline
Uint32
ScanFragReq::getNoDiskFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_NO_DISK_SHIFT) & 1;
}

inline
void
ScanFragReq::setNoDiskFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setNoDiskFlag");
  requestInfo |= (val << SF_NO_DISK_SHIFT);
}

inline
Uint32
ScanFragReq::getLcpScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_LCP_SCAN_SHIFT) & 1;
}

inline
void
ScanFragReq::setLcpScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setLcpScanFlag");
  requestInfo |= (val << SF_LCP_SCAN_SHIFT);
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

inline
Uint32
ScanFragReq::getReorgFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_REORG_SHIFT) & SF_REORG_MASK;
}

inline
void
ScanFragReq::setReorgFlag(UintR & requestInfo, UintR val){
  ASSERT_MAX(val, SF_REORG_MASK, "ScanFragReq::setLcpScanFlag");
  requestInfo |= (val << SF_REORG_SHIFT);
}

inline
Uint32
ScanFragReq::getCorrFactorFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_CORR_FACTOR_SHIFT) & 1;
}

inline
void
ScanFragReq::setCorrFactorFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setCorrFactorFlag");
  requestInfo |= (val << SF_CORR_FACTOR_SHIFT);
}

inline
Uint32
ScanFragReq::getStatScanFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_STAT_SCAN_SHIFT) & 1;
}

inline
void
ScanFragReq::setStatScanFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setStatScanFlag");
  requestInfo |= (val << SF_STAT_SCAN_SHIFT);
}

inline
Uint32
ScanFragReq::getPrioAFlag(const Uint32 & requestInfo){
  return (requestInfo >> SF_PRIO_A_SHIFT) & 1;
}

inline
void
ScanFragReq::setPrioAFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setPrioAFlag");
  requestInfo |= (val << SF_PRIO_A_SHIFT);
}

inline
Uint32
ScanFragReq::getNotInterpretedFlag(const Uint32 & requestInfo)
{
  return (requestInfo >> SF_NOT_INTERPRETED_SHIFT) & 1;
}

inline
void
ScanFragReq::setNotInterpretedFlag(UintR & requestInfo, UintR val)
{
  ASSERT_BOOL(val, "ScanFragReq::setStatScanFlag");
  requestInfo |= (val << SF_NOT_INTERPRETED_SHIFT);
}

/**
 * Request Info (SCAN_NEXTREQ)
 *
 * c = close                 - 1  Bit 0
 * C = corr value flag       - 1  Bit 1
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * cC
 */
#define SFN_CLOSE_SHIFT 0
#define SFN_CORR_SHIFT  1
#define SFN_PRIO_A_SHIFT 2

inline
Uint32
ScanFragNextReq::getCorrFactorFlag(const Uint32 & ri)
{
  return (ri >> SFN_CORR_SHIFT) & 1;
}

inline
void
ScanFragNextReq::setCorrFactorFlag(Uint32 & ri)
{
  ri |= (1 << SFN_CORR_SHIFT);
}

inline
Uint32
ScanFragNextReq::getCloseFlag(const Uint32 & requestInfo){
  return requestInfo & 1;
}

inline
void
ScanFragNextReq::setCloseFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setCloseFlag");
  requestInfo |= val;
}

inline
Uint32
ScanFragNextReq::getPrioAFlag(const Uint32 & requestInfo){
  return (requestInfo >> SFN_PRIO_A_SHIFT) & 1;
}

inline
void
ScanFragNextReq::setPrioAFlag(UintR & requestInfo, UintR val){
  ASSERT_BOOL(val, "ScanFragReq::setPrioAFlag");
  requestInfo |= (val << SFN_PRIO_A_SHIFT);
}

#undef JAM_FILE_ID

#endif

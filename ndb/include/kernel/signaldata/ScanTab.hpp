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

#ifndef SCAN_TAB_H
#define SCAN_TAB_H

#include "SignalData.hpp"

/**
 * 
 * SENDER:  API
 * RECIVER: Dbtc
 */
class ScanTabReq {
  /**
   * Reciver(s)
   */
  friend class Dbtc;         // Reciver

  /**
   * Sender(s)
   */
  friend class NdbConnection;
  friend class NdbScanOperation; 

  /**
   * For printing
   */
  friend bool printSCANTABREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 25 );

private:

  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;        // DATA 0
  UintR attrLen;              // DATA 1
  UintR requestInfo;          // DATA 2
  UintR tableId;              // DATA 3
  UintR tableSchemaVersion;   // DATA 4
  UintR storedProcId;         // DATA 5
  UintR transId1;             // DATA 6
  UintR transId2;             // DATA 7
  UintR buddyConPtr;          // DATA 8
  UintR apiOperationPtr[16];  // DATA 9-25
  
  /**
   * Get:ers for requestInfo
   */
  static Uint8 getParallelism(const UintR & requestInfo);
  static Uint8 getLockMode(const UintR & requestInfo);
  static Uint8 getHoldLockFlag(const UintR & requestInfo);
  static Uint8 getReadCommittedFlag(const UintR & requestInfo);
  static Uint8 getRangeScanFlag(const UintR & requestInfo);
  static Uint8 getScanBatch(const UintR & requestInfo);

  /**
   * Set:ers for requestInfo
   */
  static void clearRequestInfo(UintR & requestInfo);
  static void setParallelism(UintR & requestInfo, Uint32 flag);
  static void setLockMode(UintR & requestInfo, Uint32 flag);
  static void setHoldLockFlag(UintR & requestInfo, Uint32 flag);
  static void setReadCommittedFlag(UintR & requestInfo, Uint32 flag);
  static void setRangeScanFlag(UintR & requestInfo, Uint32 flag);
  static void setScanBatch(Uint32& requestInfo, Uint32 sz);
};

/**
 * Request Info
 *
 p = Parallelism           - 8  Bits -> Max 256 (Bit 0-7)
 l = Lock mode             - 1  Bit 8
 h = Hold lock mode        - 1  Bit 10
 c = Read Committed        - 1  Bit 11
 x = Range Scan (TUX)      - 1  Bit 15
 b = Scan batch            - 5  Bit 16-19 (max 15)

           1111111111222222222233
 01234567890123456789012345678901
 ppppppppl hc   xbbbbb
*/

#define PARALLELL_SHIFT     (0)
#define PARALLELL_MASK      (255)

#define LOCK_MODE_SHIFT     (8)
#define LOCK_MODE_MASK      (1)

#define HOLD_LOCK_SHIFT     (10)
#define HOLD_LOCK_MASK      (1)

#define READ_COMMITTED_SHIFT     (11)
#define READ_COMMITTED_MASK      (1)

#define RANGE_SCAN_SHIFT        (15)
#define RANGE_SCAN_MASK         (1)

#define SCAN_BATCH_SHIFT (16)
#define SCAN_BATCH_MASK  (31)

inline
Uint8
ScanTabReq::getParallelism(const UintR & requestInfo){
  return (Uint8)((requestInfo >> PARALLELL_SHIFT) & PARALLELL_MASK);
}

inline
Uint8
ScanTabReq::getLockMode(const UintR & requestInfo){
  return (Uint8)((requestInfo >> LOCK_MODE_SHIFT) & LOCK_MODE_MASK);
}

inline
Uint8
ScanTabReq::getHoldLockFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> HOLD_LOCK_SHIFT) & HOLD_LOCK_MASK);
}

inline
Uint8
ScanTabReq::getReadCommittedFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> READ_COMMITTED_SHIFT) & READ_COMMITTED_MASK);
}

inline
Uint8
ScanTabReq::getRangeScanFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> RANGE_SCAN_SHIFT) & RANGE_SCAN_MASK);
}

inline
Uint8
ScanTabReq::getScanBatch(const Uint32 & requestInfo){
  return (Uint8)((requestInfo >> SCAN_BATCH_SHIFT) & SCAN_BATCH_MASK);
}

inline
void 
ScanTabReq::clearRequestInfo(UintR & requestInfo){
  requestInfo = 0;
}

inline
void 
ScanTabReq::setParallelism(UintR & requestInfo, Uint32 type){
  ASSERT_MAX(type, PARALLELL_MASK, "ScanTabReq::setParallellism");
  requestInfo |= (type << PARALLELL_SHIFT);
}

inline
void 
ScanTabReq::setLockMode(UintR & requestInfo, Uint32 mode){
  ASSERT_MAX(mode, LOCK_MODE_MASK,  "ScanTabReq::setLockMode");
  requestInfo |= (mode << LOCK_MODE_SHIFT);
}

inline
void 
ScanTabReq::setHoldLockFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "ScanTabReq::setHoldLockFlag");
  requestInfo |= (flag << HOLD_LOCK_SHIFT);
}

inline
void 
ScanTabReq::setReadCommittedFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "ScanTabReq::setReadCommittedFlag");
  requestInfo |= (flag << READ_COMMITTED_SHIFT);
}

inline
void 
ScanTabReq::setRangeScanFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "ScanTabReq::setRangeScanFlag");
  requestInfo |= (flag << RANGE_SCAN_SHIFT);
}

inline
void
ScanTabReq::setScanBatch(Uint32 & requestInfo, Uint32 flag){
  ASSERT_MAX(flag, SCAN_BATCH_MASK,  "ScanTabReq::setScanBatch");
  requestInfo |= (flag << SCAN_BATCH_SHIFT);
}

/**
 * 
 * SENDER:  Dbtc
 * RECIVER: API
 */
class ScanTabConf {
  /**
   * Reciver(s) 
   */
  friend class NdbConnection;         // Reciver

  /**
   * Sender(s)
   */
  friend class Dbtc; 

  /**
   * For printing
   */
  friend bool printSCANTABCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 4 );
  static const Uint32 EndOfData = (1 << 31);
  
private:

  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;        // DATA 0
  UintR requestInfo;          // DATA 1
  UintR transId1;             // DATA 2
  UintR transId2;             // DATA 3

  struct OpData {
    Uint32 apiPtrI;
    Uint32 tcPtrI;
    Uint32 info;
  };

  static Uint32 getLength(Uint32 opDataInfo) { return opDataInfo >> 5; };
  static Uint32 getRows(Uint32 opDataInfo) { return opDataInfo & 31;}
};

/**
 * request info
 *
 o = received operations        - 7  Bits -> Max 255 (Bit 0-7)
 s = status of scan             - 2  Bits -> Max ??? (Bit 8-?) 

           1111111111222222222233
 01234567890123456789012345678901
 ooooooooss
*/

#define OPERATIONS_SHIFT     (0)
#define OPERATIONS_MASK      (0xFF)

#define STATUS_SHIFT     (8)
#define STATUS_MASK      (0xFF)


/**
 * 
 * SENDER:  Dbtc
 * RECIVER: API
 */
class ScanTabRef {
  /**
   * Reciver(s)
   */
  friend class NdbConnection;         // Reciver

  /**
   * Sender(s)
   */
  friend class Dbtc; 

  /**
   * For printing
   */
  friend bool printSCANTABREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 5 );

private:

  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;        // DATA 0
  UintR transId1;             // DATA 1
  UintR transId2;             // DATA 2
  UintR errorCode;            // DATA 3
  UintR closeNeeded;          // DATA 4
 
};

/**
 * 
 * SENDER:  API
 * RECIVER: Dbtc
 */
class ScanNextReq {
  /**
   * Reciver(s)
   */
  friend class Dbtc;         // Reciver

  /**
   * Sender(s)
   */
  friend class NdbOperation; 

  /**
   * For printing
   */
  friend bool printSCANNEXTREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 4 );

private:

  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;        // DATA 0
  UintR stopScan;             // DATA 1
  UintR transId1;             // DATA 2
  UintR transId2;             // DATA 3

  // stopScan = 1, stop this scan
 
};

#endif

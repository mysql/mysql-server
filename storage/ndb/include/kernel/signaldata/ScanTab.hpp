/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SCAN_TAB_H
#define SCAN_TAB_H

#include "SignalData.hpp"

#define JAM_FILE_ID 56

/**
 *
 * SENDER:  API
 * RECIVER: Dbtc
 */
class ScanTabReq {
  /**
   * Reciver(s)
   */
  friend class Dbtc;  // Reciver

  /**
   * Sender(s)
   */
  friend class NdbTransaction;
  friend class NdbScanOperation;
  friend class NdbIndexScanOperation;
  friend class NdbQueryImpl;
  friend class NdbScanFilterImpl;

  /**
   * For printing
   */
  friend bool printSCANTABREQ(FILE *output, const Uint32 *theData, Uint32 len,
                              Uint16 receiverBlockNo);

 public:
  /**
   * Length of signal
   */
  static constexpr Uint32 StaticLength = 11;
  static constexpr Uint32 MaxTotalAttrInfo = 0xFFFF;

  /**
   * Long section nums
   */
  static constexpr Uint32 ReceiverIdSectionNum = 0;
  static constexpr Uint32 AttrInfoSectionNum = 1; /* Long SCANTABREQ only */
  static constexpr Uint32 KeyInfoSectionNum = 2;  /* Long SCANTABREQ only */

 private:
  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;  // DATA 0
  union {
    UintR attrLenKeyLen;  // DATA 1 : Short SCANTABREQ (Versions < 6.4.0)
    UintR spare;          // DATA 1 : Long SCANTABREQ
  };
  UintR requestInfo;  // DATA 2
  /*
    Table ID. Note that for a range scan of a table using an ordered index,
    tableID is the ID of the index, not of the underlying table.
  */
  UintR tableId;             // DATA 3
  UintR tableSchemaVersion;  // DATA 4
  UintR storedProcId;        // DATA 5
  UintR transId1;            // DATA 6
  UintR transId2;            // DATA 7
  UintR buddyConPtr;         // DATA 8
  UintR batch_byte_size;     // DATA 9
  UintR first_batch_size;    // DATA 10

  /**
   * Optional
   */
  Uint32 distributionKey;

  /**
   * Get:ers for requestInfo
   */
  static Uint8 getParallelism(const UintR &requestInfo);
  static Uint8 getLockMode(const UintR &requestInfo);
  static Uint8 getHoldLockFlag(const UintR &requestInfo);
  static Uint8 getReadCommittedFlag(const UintR &requestInfo);
  static Uint8 getRangeScanFlag(const UintR &requestInfo);
  static Uint8 getDescendingFlag(const UintR &requestInfo);
  static Uint8 getTupScanFlag(const UintR &requestInfo);
  static Uint8 getKeyinfoFlag(const UintR &requestInfo);
  static Uint16 getScanBatch(const UintR &requestInfo);
  static Uint8 getDistributionKeyFlag(const UintR &requestInfo);
  static UintR getNoDiskFlag(const UintR &requestInfo);
  static Uint32 getViaSPJFlag(const Uint32 &requestInfo);
  static Uint32 getPassAllConfsFlag(const Uint32 &requestInfo);
  static Uint32 getExtendedConf(const Uint32 &);
  static Uint8 getReadCommittedBaseFlag(const UintR &requestInfo);
  static Uint32 getMultiFragFlag(const Uint32 &requestInfo);

  /**
   * Set:ers for requestInfo
   */
  static void clearRequestInfo(UintR &requestInfo);
  static void setParallelism(UintR &requestInfo, Uint32 flag);
  static void setLockMode(UintR &requestInfo, Uint32 flag);
  static void setHoldLockFlag(UintR &requestInfo, Uint32 flag);
  static void setReadCommittedFlag(UintR &requestInfo, Uint32 flag);
  static void setRangeScanFlag(UintR &requestInfo, Uint32 flag);
  static void setDescendingFlag(UintR &requestInfo, Uint32 flag);
  static void setTupScanFlag(UintR &requestInfo, Uint32 flag);
  static void setKeyinfoFlag(UintR &requestInfo, Uint32 flag);
  static void setScanBatch(Uint32 &requestInfo, Uint32 sz);
  static void setDistributionKeyFlag(Uint32 &requestInfo, Uint32 flag);
  static void setNoDiskFlag(UintR &requestInfo, UintR val);
  static void setViaSPJFlag(Uint32 &requestInfo, Uint32 val);
  static void setPassAllConfsFlag(Uint32 &requestInfo, Uint32 val);
  static void setExtendedConf(Uint32 &requestInfo, Uint32 val);
  static void setReadCommittedBaseFlag(Uint32 &requestInfo, Uint32 val);
  static void setMultiFragFlag(Uint32 &requestInfo, Uint32 val);
};

/**
 * Request Info
 *
 p = Parallelism           - 8  Bits -> Max 255 (Bit 0-7).
                                        Note: these bits are ignored since
                                        7.0.34, 7.1.23, 7.2.7 and should be
                                        zero-filled until future reuse.
 l = Lock mode             - 1  Bit 8
 h = Hold lock mode        - 1  Bit 10
 c = Read Committed        - 1  Bit 11
 k = Keyinfo               - 1  Bit 12  If set, LQH will send back a KEYINFO20
                                        signal for each scanned row,
                                        containing information needed to
                                        identify the row for subsequent
                                        TCKEYREQ signal(s).
 t = Tup scan              - 1  Bit 13
 z = Descending (TUX)      - 1  Bit 14
 x = Range Scan (TUX)      - 1  Bit 15
 b = Scan batch            - 10 Bit 16-25 (max 1023)
 d = Distribution key flag - 1  Bit 26
 n = No disk flag          - 1  Bit 9
 j = Via SPJ flag          - 1  Bit 27
 a = Pass all confs flag   - 1  Bit 28
 f = 4 word conf           - 1  Bit 29
 R = Read Committed base   - 1  Bit 30

           1111111111222222222233
 01234567890123456789012345678901
 pppppppplnhcktzxbbbbbbbbbbdjafR
*/

#define PARALLEL_SHIFT (0)
#define PARALLEL_MASK (255)

#define LOCK_MODE_SHIFT (8)
#define LOCK_MODE_MASK (1)

#define HOLD_LOCK_SHIFT (10)
#define HOLD_LOCK_MASK (1)

#define KEYINFO_SHIFT (12)
#define KEYINFO_MASK (1)

#define READ_COMMITTED_SHIFT (11)
#define READ_COMMITTED_MASK (1)

#define RANGE_SCAN_SHIFT (15)
#define RANGE_SCAN_MASK (1)

#define DESCENDING_SHIFT (14)
#define DESCENDING_MASK (1)

#define TUP_SCAN_SHIFT (13)
#define TUP_SCAN_MASK (1)

#define SCAN_BATCH_SHIFT (16)
#define SCAN_BATCH_MASK (1023)

#define SCAN_DISTR_KEY_SHIFT (26)
#define SCAN_DISTR_KEY_MASK (1)

#define SCAN_NODISK_SHIFT (9)
#define SCAN_NODISK_MASK (1)

#define SCAN_SPJ_SHIFT (27)
#define SCAN_PASS_CONF_SHIFT (28)
#define SCAN_EXTENDED_CONF_SHIFT (29)
#define SCAN_READ_COMMITTED_BASE_SHIFT (30)
#define SCAN_MULTI_FRAG_SHIFT (31)

inline Uint8 ScanTabReq::getReadCommittedBaseFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> SCAN_READ_COMMITTED_BASE_SHIFT) & 1);
}

inline Uint8 ScanTabReq::getParallelism(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> PARALLEL_SHIFT) & PARALLEL_MASK);
}

inline Uint8 ScanTabReq::getLockMode(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> LOCK_MODE_SHIFT) & LOCK_MODE_MASK);
}

inline Uint8 ScanTabReq::getHoldLockFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> HOLD_LOCK_SHIFT) & HOLD_LOCK_MASK);
}

inline Uint8 ScanTabReq::getReadCommittedFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> READ_COMMITTED_SHIFT) & READ_COMMITTED_MASK);
}

inline Uint8 ScanTabReq::getRangeScanFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> RANGE_SCAN_SHIFT) & RANGE_SCAN_MASK);
}

inline Uint8 ScanTabReq::getDescendingFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> DESCENDING_SHIFT) & DESCENDING_MASK);
}

inline Uint8 ScanTabReq::getTupScanFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> TUP_SCAN_SHIFT) & TUP_SCAN_MASK);
}

inline Uint16 ScanTabReq::getScanBatch(const Uint32 &requestInfo) {
  return (Uint16)((requestInfo >> SCAN_BATCH_SHIFT) & SCAN_BATCH_MASK);
}

inline void ScanTabReq::clearRequestInfo(UintR &requestInfo) {
  requestInfo = 0;
}

inline void ScanTabReq::setReadCommittedBaseFlag(UintR &requestInfo,
                                                 Uint32 type) {
  ASSERT_MAX(type, 1, "ScanTabReq::setReadCommittedBase");
  requestInfo = (requestInfo & ~(1 << SCAN_READ_COMMITTED_BASE_SHIFT)) |
                ((type & 1) << SCAN_READ_COMMITTED_BASE_SHIFT);
}

inline void ScanTabReq::setParallelism(UintR &requestInfo, Uint32 type) {
  ASSERT_MAX(type, PARALLEL_MASK, "ScanTabReq::setParallelism");
  requestInfo = (requestInfo & ~(PARALLEL_MASK << PARALLEL_SHIFT)) |
                ((type & PARALLEL_MASK) << PARALLEL_SHIFT);
}

inline void ScanTabReq::setLockMode(UintR &requestInfo, Uint32 mode) {
  ASSERT_MAX(mode, LOCK_MODE_MASK, "ScanTabReq::setLockMode");
  requestInfo = (requestInfo & ~(LOCK_MODE_MASK << LOCK_MODE_SHIFT)) |
                ((mode & LOCK_MODE_MASK) << LOCK_MODE_SHIFT);
}

inline void ScanTabReq::setHoldLockFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setHoldLockFlag");
  requestInfo = (requestInfo & ~(HOLD_LOCK_MASK << HOLD_LOCK_SHIFT)) |
                ((flag & HOLD_LOCK_MASK) << HOLD_LOCK_SHIFT);
}

inline void ScanTabReq::setReadCommittedFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setReadCommittedFlag");
  requestInfo = (requestInfo & ~(READ_COMMITTED_MASK << READ_COMMITTED_SHIFT)) |
                ((flag & READ_COMMITTED_MASK) << READ_COMMITTED_SHIFT);
}

inline void ScanTabReq::setRangeScanFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setRangeScanFlag");
  requestInfo = (requestInfo & ~(RANGE_SCAN_MASK << RANGE_SCAN_SHIFT)) |
                ((flag & RANGE_SCAN_MASK) << RANGE_SCAN_SHIFT);
}

inline void ScanTabReq::setDescendingFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setDescendingFlag");
  requestInfo = (requestInfo & ~(DESCENDING_MASK << DESCENDING_SHIFT)) |
                ((flag & DESCENDING_MASK) << DESCENDING_SHIFT);
}

inline void ScanTabReq::setTupScanFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setTupScanFlag");
  requestInfo = (requestInfo & ~(TUP_SCAN_MASK << TUP_SCAN_SHIFT)) |
                ((flag & TUP_SCAN_MASK) << TUP_SCAN_SHIFT);
}

inline void ScanTabReq::setScanBatch(Uint32 &requestInfo, Uint32 flag) {
  ASSERT_MAX(flag, SCAN_BATCH_MASK, "ScanTabReq::setScanBatch");
  requestInfo = (requestInfo & ~(SCAN_BATCH_MASK << SCAN_BATCH_SHIFT)) |
                ((flag & SCAN_BATCH_MASK) << SCAN_BATCH_SHIFT);
}

inline Uint8 ScanTabReq::getKeyinfoFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> KEYINFO_SHIFT) & KEYINFO_MASK);
}

inline void ScanTabReq::setKeyinfoFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setKeyinfoFlag");
  requestInfo = (requestInfo & ~(KEYINFO_MASK << KEYINFO_SHIFT)) |
                ((flag & KEYINFO_MASK) << KEYINFO_SHIFT);
}

inline Uint8 ScanTabReq::getDistributionKeyFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> SCAN_DISTR_KEY_SHIFT) & SCAN_DISTR_KEY_MASK);
}

inline void ScanTabReq::setDistributionKeyFlag(UintR &requestInfo,
                                               Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setKeyinfoFlag");
  requestInfo = (requestInfo & ~(SCAN_DISTR_KEY_MASK << SCAN_DISTR_KEY_SHIFT)) |
                ((flag & SCAN_DISTR_KEY_MASK) << SCAN_DISTR_KEY_SHIFT);
}

inline UintR ScanTabReq::getNoDiskFlag(const UintR &requestInfo) {
  return (requestInfo >> SCAN_NODISK_SHIFT) & SCAN_NODISK_MASK;
}

inline void ScanTabReq::setNoDiskFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setNoDiskFlag");
  requestInfo = (requestInfo & ~(SCAN_NODISK_MASK << SCAN_NODISK_SHIFT)) |
                ((flag & SCAN_NODISK_MASK) << SCAN_NODISK_SHIFT);
}

inline UintR ScanTabReq::getViaSPJFlag(const UintR &requestInfo) {
  return (requestInfo >> SCAN_SPJ_SHIFT) & 1;
}

inline void ScanTabReq::setViaSPJFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setViaSPJFlag");
  requestInfo |= (flag << SCAN_SPJ_SHIFT);
}

inline UintR ScanTabReq::getPassAllConfsFlag(const UintR &requestInfo) {
  return (requestInfo >> SCAN_PASS_CONF_SHIFT) & 1;
}

inline void ScanTabReq::setPassAllConfsFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setPassAllConfs");
  requestInfo |= (flag << SCAN_PASS_CONF_SHIFT);
}

inline UintR ScanTabReq::getExtendedConf(const UintR &requestInfo) {
  return (requestInfo >> SCAN_EXTENDED_CONF_SHIFT) & 1;
}

inline void ScanTabReq::setExtendedConf(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "ScanTabReq::setExtendedConf");
  requestInfo |= (flag << SCAN_EXTENDED_CONF_SHIFT);
}

/**
 * MULTI_FRAG flag can currently only be used together
 * with ViaSPJ flag.
 */
inline UintR ScanTabReq::getMultiFragFlag(const UintR &requestInfo) {
  return (requestInfo >> SCAN_MULTI_FRAG_SHIFT) & 1;
}

inline void ScanTabReq::setMultiFragFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setMultiFragFlag");
  requestInfo = (requestInfo & ~(1 << SCAN_MULTI_FRAG_SHIFT)) |
                (flag << SCAN_MULTI_FRAG_SHIFT);
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
  friend class NdbTransaction;  // Reciver

  /**
   * Sender(s)
   */
  friend class Dbtc;

  /**
   * For printing
   */
  friend bool printSCANTABCONF(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 receiverBlockNo);

 public:
  /**
   * Length of signal
   */
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 EndOfData = (1 << 31);

 private:
  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;  // DATA 0
  UintR requestInfo;    // DATA 1
  UintR transId1;       // DATA 2
  UintR transId2;       // DATA 3

  struct OpData {
    Uint32 apiPtrI;
    /*
      tcPtrI is the scan fragment record pointer, used in SCAN_NEXTREQ to
      acknowledge the reception of the batch of rows from a fragment scan.
      If RNIL, this means that this particular fragment is done scanning.
    */
    Uint32 tcPtrI;

    Uint32 rows;
    Uint32 len;
  };

  /** for 3 word conf */
  static Uint32 getLength(Uint32 opDataInfo) { return opDataInfo >> 10; }
  static Uint32 getRows(Uint32 opDataInfo) { return opDataInfo & 1023; }
};

/**
 * request info
 *
 o = received operations        - 8  Bits -> Max 255 (Bit 0-7)
 e = end of data                - 1  bit (31)

           1111111111222222222233
 01234567890123456789012345678901
 oooooooo                       e
*/

#define OPERATIONS_SHIFT (0)
#define OPERATIONS_MASK (0xFF)

#define STATUS_SHIFT (8)
#define STATUS_MASK (0xFF)

/**
 *
 * SENDER:  Dbtc
 * RECIVER: API
 */
class ScanTabRef {
  /**
   * Reciver(s)
   */
  friend class NdbTransaction;  // Reciver

  /**
   * Sender(s)
   */
  friend class Dbtc;

  /**
   * For printing
   */
  friend bool printSCANTABREF(FILE *output, const Uint32 *theData, Uint32 len,
                              Uint16 receiverBlockNo);

 public:
  /**
   * Length of signal
   */
  static constexpr Uint32 SignalLength = 5;

 private:
  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;  // DATA 0
  UintR transId1;       // DATA 1
  UintR transId2;       // DATA 2
  UintR errorCode;      // DATA 3
  UintR closeNeeded;    // DATA 4
};

/*
  SENDER:  API
  RECIVER: Dbtc

  This signal is sent by API to acknowledge the reception of batches of rows
  from one or more fragment scans, and to request the fetching of the next
  batches of rows.

  Any locks held by the transaction on rows in the previously fetched batches
  are released (unless explicitly transferred to this or another transaction in
  a TCKEYREQ signal with TakeOverScanFlag set).

  The fragment scan batches to acknowledge are identified by the tcPtrI words
  in the list of struct OpData received in ScanTabConf (scan fragment record
  pointer).

  The list of scan fragment record pointers is sent as an array of words,
  inline in the signal if <= 21 words, else as the first section in a long
  signal.
 */
class ScanNextReq {
  /**
   * Reciver(s)
   */
  friend class Dbtc;  // Reciver

  /**
   * Sender(s)
   */
  friend class NdbScanOperation;
  friend class NdbQueryImpl;

  /**
   * For printing
   */
  friend bool printSCANNEXTREQ(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 receiverBlockNo);

 public:
  /**
   * Length of signal
   */
  static constexpr Uint32 SignalLength = 4;

  /**
   * Section carrying receiverIds if num receivers > 21
   */
  static constexpr Uint32 ReceiverIdsSectionNum = 0;

 private:
  // Type definitions

  /**
   * DATA VARIABLES
   */
  UintR apiConnectPtr;  // DATA 0
  UintR stopScan;       // DATA 1
  UintR transId1;       // DATA 2
  UintR transId2;       // DATA 3

  // stopScan = 1, stop this scan

  /*
    After this data comes the list of scan fragment record pointers for the
    fragment scans to acknowledge, if they fit within the 25 words available
    in the signal (else they are sent in the first long signal section).
  */
};

#undef JAM_FILE_ID

#endif

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

#ifndef LQH_KEY_H
#define LQH_KEY_H

#include <trigger_definitions.h>
#include "SignalData.hpp"

#define JAM_FILE_ID 27

class LqhKeyReq {
  /**
   * Receiver(s)
   */
  friend class Dblqh;  // Reciver

  /**
   * Sender(s)
   */
  friend class Dbspj;
  friend class Dbtc;
  friend class Restore;

  /**
   * Users
   */
  friend class Dbtup;

  /**
   * For printing
   */
  friend bool printLQHKEYREQ(FILE *output, const Uint32 *theData, Uint32 len,
                             Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 FixedSignalLength = 11;
  static constexpr Uint32 MaxKeyInfo = 4;
  static constexpr Uint32 MaxAttrInfo = 5;

  /* Long LQHKEYREQ definitions */
  static constexpr Uint32 KeyInfoSectionNum = 0;
  static constexpr Uint32 AttrInfoSectionNum = 1;

  static constexpr Uint32 UnlockKeyLen = 2;

 private:
  /**
   * DATA VARIABLES
   */
  //-------------------------------------------------------------
  // Unconditional part. First 10 words
  //-------------------------------------------------------------
  UintR clientConnectPtr;    // DATA 0
  UintR attrLen;             // DATA 1
  UintR hashValue;           // DATA 2
  UintR requestInfo;         // DATA 3
  UintR tcBlockref;          // DATA 4
  UintR tableSchemaVersion;  // DATA 5
  UintR fragmentData;        // DATA 6
  UintR transId1;            // DATA 7
  UintR transId2;            // DATA 8
  UintR savePointId;         // DATA 9
  union {
    /**
     * When sent from  TC -> LQH this variable contains scanInfo
     * When send from LQH -> LQH this variable contains numFiredTriggers
     */
    UintR numFiredTriggers;  // DATA 10
    Uint32 scanInfo;         // DATA 10
  };

  //-------------------------------------------------------------
  // Variable sized key part. Those will be placed to
  // pack the signal in an appropriate manner.
  //-------------------------------------------------------------
  UintR variableData[10];  // DATA 11 - 21

  static UintR getAttrLen(const UintR &scanInfoAttrLen);
  static UintR getScanTakeOverFlag(const UintR &scanInfoAttrLen);
  static UintR getStoredProcFlag(const UintR &scanData);
  static UintR getDistributionKey(const UintR &scanData);
  static UintR getReorgFlag(const UintR &scanData);
  static void setReorgFlag(UintR &scanData, Uint32 val);

  static UintR getTableId(const UintR &tableSchemaVersion);
  static UintR getSchemaVersion(const UintR &tableSchemaVersion);

  static UintR getFragmentId(const UintR &fragmentData);
  static UintR getNextReplicaNodeId(const UintR &fragmentData);

  static Uint8 getLockType(const UintR &requestInfo);
  static Uint8 getDirtyFlag(const UintR &requestInfo);
  static Uint8 getInterpretedFlag(const UintR &requestInfo);
  static Uint8 getSimpleFlag(const UintR &requestInfo);
  static Uint8 getOperation(const UintR &requestInfo);
  static Uint8 getSeqNoReplica(const UintR &requestInfo);
  static Uint8 getLastReplicaNo(const UintR &requestInfo);
  static Uint8 getAIInLqhKeyReq(const UintR &requestInfo);
  static UintR getKeyLen(const UintR &requestInfo);
  static UintR getSameClientAndTcFlag(const UintR &requestInfo);
  static UintR getReturnedReadLenAIFlag(const UintR &requestInfo);
  static UintR getApplicationAddressFlag(const UintR &requestInfo);
  static UintR getMarkerFlag(const UintR &requestInfo);
  static UintR getNoDiskFlag(const UintR &requestInfo);

  /**
   * Setters
   */

  static void setAttrLen(UintR &scanInfoAttrLen, UintR val);
  static void setScanTakeOverFlag(UintR &scanInfoAttrLen, UintR val);
  /* stored procedure flag is deprecated if ever used */
  static void setStoredProcFlag(UintR &scanData, UintR val);
  static void setDistributionKey(UintR &scanData, UintR val);

  static void setTableId(UintR &tableSchemaVersion, UintR val);
  static void setSchemaVersion(UintR &tableSchemaVersion, UintR val);

  static void setFragmentId(UintR &fragmentData, UintR val);
  static void setNextReplicaNodeId(UintR &fragmentData, UintR val);

  static void setLockType(UintR &requestInfo, UintR val);
  static void setDirtyFlag(UintR &requestInfo, UintR val);
  static void setInterpretedFlag(UintR &requestInfo, UintR val);
  static void setSimpleFlag(UintR &requestInfo, UintR val);
  static void setOperation(UintR &requestInfo, UintR val);
  static void setSeqNoReplica(UintR &requestInfo, UintR val);
  static void setLastReplicaNo(UintR &requestInfo, UintR val);
  static void setAIInLqhKeyReq(UintR &requestInfo, UintR val);
  static void clearAIInLqhKeyReq(UintR &requestInfo);
  static void setKeyLen(UintR &requestInfo, UintR val);
  static void setSameClientAndTcFlag(UintR &requestInfo, UintR val);
  static void setReturnedReadLenAIFlag(UintR &requestInfo, UintR val);
  static void setApplicationAddressFlag(UintR &requestInfo, UintR val);
  static void setMarkerFlag(UintR &requestInfo, UintR val);
  static void setNoDiskFlag(UintR &requestInfo, UintR val);

  static UintR getRowidFlag(const UintR &requestInfo);
  static void setRowidFlag(UintR &requestInfo, UintR val);

  /**
   * When doing DIRTY WRITES
   */
  static UintR getGCIFlag(const UintR &requestInfo);
  static void setGCIFlag(UintR &requestInfo, UintR val);

  static UintR getNrCopyFlag(const UintR &requestInfo);
  static void setNrCopyFlag(UintR &requestInfo, UintR val);

  static UintR getQueueOnRedoProblemFlag(const UintR &requestInfo);
  static void setQueueOnRedoProblemFlag(UintR &requestInfo, UintR val);

  /**
   * Do normal protocol (LQHKEYCONF/REF) even if doing dirty read
   */
  static UintR getNormalProtocolFlag(const UintR &requestInfo);
  static void setNormalProtocolFlag(UintR &requestInfo, UintR val);

  /**
   * Include corr factor
   */
  static UintR getCorrFactorFlag(const UintR &requestInfo);
  static void setCorrFactorFlag(UintR &requestInfo, UintR val);

  /**
   * Include deferred constraints
   */
  static UintR getDeferredConstraints(const UintR &requestInfo);
  static void setDeferredConstraints(UintR &requestInfo, UintR val);

  /**
   * Include disable foreign keys
   */
  static UintR getDisableFkConstraints(const UintR &requestInfo);
  static void setDisableFkConstraints(UintR &requestInfo, UintR val);

  /**
   * Get mask of currently undefined bits
   */
  static UintR getLongClearBits(const UintR &requestInfo);

  /**
   * Trigger flag ensuring that requests based on fully replicated triggers
   * doesn't trigger a new trigger itself.
   */
  static UintR getNoTriggersFlag(const UintR &requestInfo);
  static void setNoTriggersFlag(UintR &requestInfo, UintR val);

  static UintR getUtilFlag(const UintR &requestInfo);
  static void setUtilFlag(UintR &requestInfo, UintR val);

  static UintR getNoWaitFlag(const UintR &requestInfo);
  static void setNoWaitFlag(UintR &requestInfo, UintR val);

  enum RequestInfo {
    RI_KEYLEN_SHIFT = 0,
    RI_KEYLEN_MASK = 1023, /* legacy for short LQHKEYREQ */
    RI_DISABLE_FK = 0,
    RI_NO_TRIGGERS = 1,
    RI_UTIL_SHIFT = 2,
    RI_NOWAIT_SHIFT = 3,

    /* Currently unused */
    RI_CLEAR_SHIFT5 = 5,
    RI_CLEAR_SHIFT6 = 6,
    RI_CLEAR_SHIFT7 = 7,
    RI_CLEAR_SHIFT8 = 8,
    RI_CLEAR_SHIFT9 = 9,

    RI_LAST_REPL_SHIFT = 10,
    RI_LAST_REPL_MASK = 3,
    RI_LOCK_TYPE_SHIFT = 12,
    RI_LOCK_TYPE_MASK = 7, /* legacy before ROWID_VERSION */
    RI_GCI_SHIFT = 12,
    RI_NR_COPY_SHIFT = 13,
    RI_QUEUE_REDO_SHIFT = 14,
    RI_APPL_ADDR_SHIFT = 15,
    RI_DIRTY_SHIFT = 16,
    RI_INTERPRETED_SHIFT = 17,
    RI_SIMPLE_SHIFT = 18,
    RI_OPERATION_SHIFT = 19,
    RI_OPERATION_MASK = 7,
    RI_SEQ_REPLICA_SHIFT = 22,
    RI_SEQ_REPLICA_MASK = 3,
    RI_AI_IN_THIS_SHIFT = 24,
    RI_AI_IN_THIS_MASK = 7, /* legacy for short LQHKEYREQ */
    RI_CORR_FACTOR_VALUE = 24,
    RI_NORMAL_DIRTY = 25,
    RI_DEFERRED_CONSTRAINTS = 26,
    RI_SAME_CLIENT_SHIFT = 27,
    RI_RETURN_AI_SHIFT = 28,
    RI_MARKER_SHIFT = 29,
    RI_NODISK_SHIFT = 30,
    RI_ROWID_SHIFT = 31,
  };

  enum ScanInfo {
    SI_ATTR_LEN_SHIFT = 0,
    SI_ATTR_LEN_MASK = 65535,
    SI_STORED_PROC_SHIFT = 16,
    SI_DISTR_KEY_SHIFT = 17,
    SI_DISTR_KEY_MASK = 255,
    SI_SCAN_TO_SHIFT = 25,
    SI_REORG_SHIFT = 26,
    SI_REORG_MASK = 3,
  };
};

/**
 * Request Info
 *
 * k = Key len                - (Short LQHKEYREQ only)
 *                              10 Bits (0-9) max 1023
 * l = Last Replica No        - 2  Bits -> Max 3 (10-11)

 IF version < NDBD_ROWID_VERSION
   * t = Lock type            - 3  Bits -> Max 7 (12-14)
 * p = Application Addr. Ind  - 1  Bit (15)
 * d = Dirty indicator        - 1  Bit (16)
 * i = Interpreted indicator  - 1  Bit (17)
 * s = Simple indicator       - 1  Bit (18)
 * o = Operation              - 3  Bits (19-21)
 * r = Sequence replica       - 2  Bits (22-23)
 * a = Attr Info in LQHKEYREQ - (Short LQHKEYREQ only)
                                3  Bits (24-26)
 * c = Same client and tc     - 1  Bit (27)
 * u = Read Len Return Ind    - 1  Bit (28)
 * m = Commit ack marker      - 1  Bit (29)
 * x = No disk usage          - 1  Bit (30)
 * z = Use rowid for insert   - 1  Bit (31)
 * g = gci flag               - 1  Bit (12)
 * n = NR copy                - 1  Bit (13)
 * q = Queue on redo problem  - 1  Bit (14)
 * A = CorrFactor flag        - 1  Bit (24)
 * P = Do normal protocol even if dirty-read - 1 Bit (25)
 * D = Deferred constraints   - 1  Bit (26)
 * F = Disable FK constraints - 1  Bit (0)
 * T = no triggers            - 1  Bit (1)
 * U = Operation came from UTIL - 1 Bit (2)
 * w = NoWait flag            = 1 Bit (3)
 * Q = Query Thread Flag      = 1 Bit (4)

 * Short LQHKEYREQ :
 *             1111111111222222222233
 *   01234567890123456789012345678901
 *   kkkkkkkkkklltttpdisooorraaacumxz
 *   kkkkkkkkkkllgn pdisooorraaacumxz
 *
 * Long LQHKEYREQ :
 *             1111111111222222222233
 *   01234567890123456789012345678901
 *   FTUwQ     llgnqpdisooorrAPDcumxz
 *
 */

/**
 * Scan Info
 *
 * a = Attr Len                 - (Short LQHKEYREQ only)
 *                                 16 Bits -> max 65535 (0-15)
 * p = Stored Procedure Ind     -  1 Bit (16)
 * d = Distribution key         -  8 Bit  -> max 255 (17-24)
 * t = Scan take over indicator -  1 Bit (25)
 * m = Reorg value              -  2 Bit (26-27)
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * aaaaaaaaaaaaaaaapddddddddtmm       (Short LQHKEYREQ)
 *                 pddddddddtmm       (Long LQHKEYREQ)
 */

inline UintR LqhKeyReq::getAttrLen(const UintR &scanData) {
  return (scanData >> SI_ATTR_LEN_SHIFT) & SI_ATTR_LEN_MASK;
}

inline Uint32 LqhKeyReq::getScanTakeOverFlag(const UintR &scanData) {
  return (scanData >> SI_SCAN_TO_SHIFT) & 1;
}

inline UintR LqhKeyReq::getStoredProcFlag(const UintR &scanData) {
  return (scanData >> SI_STORED_PROC_SHIFT) & 1;
}

inline UintR LqhKeyReq::getDistributionKey(const UintR &scanData) {
  return (scanData >> SI_DISTR_KEY_SHIFT) & SI_DISTR_KEY_MASK;
}

inline UintR LqhKeyReq::getTableId(const UintR &tableSchemaVersion) {
  return tableSchemaVersion & 0xFFFF;
}

inline UintR LqhKeyReq::getSchemaVersion(const UintR &tableSchemaVersion) {
  return tableSchemaVersion >> 16;
}

inline UintR LqhKeyReq::getFragmentId(const UintR &fragmentData) {
  return fragmentData & 0xFFFF;
}

inline UintR LqhKeyReq::getNextReplicaNodeId(const UintR &fragmentData) {
  return fragmentData >> 16;
}

inline Uint8 LqhKeyReq::getLastReplicaNo(const UintR &requestInfo) {
  return (requestInfo >> RI_LAST_REPL_SHIFT) & RI_LAST_REPL_MASK;
}

inline Uint8 LqhKeyReq::getLockType(const UintR &requestInfo) {
  return (requestInfo >> RI_LOCK_TYPE_SHIFT) & RI_LOCK_TYPE_MASK;
}

inline Uint8 LqhKeyReq::getDirtyFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_DIRTY_SHIFT) & 1;
}

inline Uint8 LqhKeyReq::getInterpretedFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_INTERPRETED_SHIFT) & 1;
}

inline Uint8 LqhKeyReq::getSimpleFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_SIMPLE_SHIFT) & 1;
}

inline Uint8 LqhKeyReq::getOperation(const UintR &requestInfo) {
  return (requestInfo >> RI_OPERATION_SHIFT) & RI_OPERATION_MASK;
}

inline Uint8 LqhKeyReq::getSeqNoReplica(const UintR &requestInfo) {
  return (requestInfo >> RI_SEQ_REPLICA_SHIFT) & RI_SEQ_REPLICA_MASK;
}

inline Uint8 LqhKeyReq::getAIInLqhKeyReq(const UintR &requestInfo) {
  return (requestInfo >> RI_AI_IN_THIS_SHIFT) & RI_AI_IN_THIS_MASK;
}

inline UintR LqhKeyReq::getKeyLen(const UintR &requestInfo) {
  return (requestInfo >> RI_KEYLEN_SHIFT) & RI_KEYLEN_MASK;
}

inline UintR LqhKeyReq::getSameClientAndTcFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_SAME_CLIENT_SHIFT) & 1;
}

inline UintR LqhKeyReq::getReturnedReadLenAIFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_RETURN_AI_SHIFT) & 1;
}

inline UintR LqhKeyReq::getApplicationAddressFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_APPL_ADDR_SHIFT) & 1;
}

inline void LqhKeyReq::setAttrLen(UintR &scanInfoAttrLen, UintR val) {
  ASSERT_MAX(val, SI_ATTR_LEN_MASK, "LqhKeyReq::setAttrLen");
  scanInfoAttrLen |= (val << SI_ATTR_LEN_SHIFT);
}

inline void LqhKeyReq::setScanTakeOverFlag(UintR &scanInfoAttrLen, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setScanTakeOverFlag");
  scanInfoAttrLen |= (val << SI_SCAN_TO_SHIFT);
}

inline void LqhKeyReq::setStoredProcFlag(UintR &scanData, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setStoredProcFlag");
  scanData |= (val << SI_STORED_PROC_SHIFT);
}

inline void LqhKeyReq::setDistributionKey(UintR &scanData, UintR val) {
  ASSERT_MAX(val, SI_DISTR_KEY_MASK, "LqhKeyReq::setDistributionKey");
  scanData |= (val << SI_DISTR_KEY_SHIFT);
}

inline Uint32 LqhKeyReq::getReorgFlag(const UintR &scanData) {
  return (scanData >> SI_REORG_SHIFT) & SI_REORG_MASK;
}

inline void LqhKeyReq::setReorgFlag(UintR &scanData, UintR val) {
  ASSERT_MAX(val, SI_REORG_MASK, "LqhKeyReq::setMovingFlag");
  scanData |= (val << SI_REORG_SHIFT);
}

#if 0  
inline
void

LqhKeyReq::setTableId(UintR & tableSchemaVersion, UintR val){
  
}
inline
void
LqhKeyReq::setSchemaVersion(UintR & tableSchemaVersion, UintR val);

inline
void
LqhKeyReq::setFragmentId(UintR & fragmentData, UintR val);

inline
void
LqhKeyReq::setNextReplicaNodeId(UintR & fragmentData, UintR val);
#endif

inline void LqhKeyReq::setLockType(UintR &requestInfo, UintR val) {
  ASSERT_MAX(val, RI_LOCK_TYPE_MASK, "LqhKeyReq::setLockType");
  requestInfo |= (val << RI_LOCK_TYPE_SHIFT);
}

inline void LqhKeyReq::setDirtyFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setDirtyFlag");
  requestInfo |= (val << RI_DIRTY_SHIFT);
}

inline void LqhKeyReq::setInterpretedFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setInterpretedFlag");
  requestInfo |= (val << RI_INTERPRETED_SHIFT);
}

inline void LqhKeyReq::setSimpleFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setSimpleFlag");
  requestInfo |= (val << RI_SIMPLE_SHIFT);
}

inline void LqhKeyReq::setOperation(UintR &requestInfo, UintR val) {
  ASSERT_MAX(val, RI_OPERATION_MASK, "LqhKeyReq::setOperation");
  requestInfo |= (val << RI_OPERATION_SHIFT);
}

inline void LqhKeyReq::setSeqNoReplica(UintR &requestInfo, UintR val) {
  ASSERT_MAX(val, RI_SEQ_REPLICA_MASK, "LqhKeyReq::setSeqNoReplica");
  requestInfo |= (val << RI_SEQ_REPLICA_SHIFT);
}

inline void LqhKeyReq::setLastReplicaNo(UintR &requestInfo, UintR val) {
  ASSERT_MAX(val, RI_LAST_REPL_MASK, "LqhKeyReq::setLastReplicaNo");
  requestInfo |= (val << RI_LAST_REPL_SHIFT);
}

inline void LqhKeyReq::setAIInLqhKeyReq(UintR &requestInfo, UintR val) {
  ASSERT_MAX(val, RI_AI_IN_THIS_MASK, "LqhKeyReq::setAIInLqhKeyReq");
  requestInfo |= (val << RI_AI_IN_THIS_SHIFT);
}

inline void LqhKeyReq::clearAIInLqhKeyReq(UintR &requestInfo) {
  requestInfo &= ~((Uint32)RI_AI_IN_THIS_MASK << RI_AI_IN_THIS_SHIFT);
}

inline void LqhKeyReq::setKeyLen(UintR &requestInfo, UintR val) {
  ASSERT_MAX(val, RI_KEYLEN_MASK, "LqhKeyReq::setKeyLen");
  requestInfo |= (val << RI_KEYLEN_SHIFT);
}

inline void LqhKeyReq::setSameClientAndTcFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setSameClientAndTcFlag");
  requestInfo |= (val << RI_SAME_CLIENT_SHIFT);
}

inline void LqhKeyReq::setReturnedReadLenAIFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setReturnedReadLenAIFlag");
  requestInfo |= (val << RI_RETURN_AI_SHIFT);
}

inline void LqhKeyReq::setApplicationAddressFlag(UintR &requestInfo,
                                                 UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setApplicationAddressFlag");
  requestInfo |= (val << RI_APPL_ADDR_SHIFT);
}

/**** */

inline void LqhKeyReq::setMarkerFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setMarkerFlag");
  requestInfo |= (val << RI_MARKER_SHIFT);
}

inline UintR LqhKeyReq::getMarkerFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_MARKER_SHIFT) & 1;
}

inline void LqhKeyReq::setNoDiskFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setNoDiskFlag");
  requestInfo |= (val << RI_NODISK_SHIFT);
}

inline UintR LqhKeyReq::getNoDiskFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_NODISK_SHIFT) & 1;
}

inline void LqhKeyReq::setRowidFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setRowidFlag");
  requestInfo |= (val << RI_ROWID_SHIFT);
}

inline UintR LqhKeyReq::getRowidFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_ROWID_SHIFT) & 1;
}

inline void LqhKeyReq::setGCIFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setGciFlag");
  requestInfo |= (val << RI_GCI_SHIFT);
}

inline UintR LqhKeyReq::getGCIFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_GCI_SHIFT) & 1;
}

inline void LqhKeyReq::setNrCopyFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setNrCopyFlag");
  requestInfo |= (val << RI_NR_COPY_SHIFT);
}

inline UintR LqhKeyReq::getNrCopyFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_NR_COPY_SHIFT) & 1;
}

inline void LqhKeyReq::setNormalProtocolFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setNrCopyFlag");
  requestInfo |= (val << RI_NORMAL_DIRTY);
}

inline UintR LqhKeyReq::getNormalProtocolFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_NORMAL_DIRTY) & 1;
}

inline void LqhKeyReq::setCorrFactorFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setCorrFactorFlag");
  requestInfo |= (val << RI_CORR_FACTOR_VALUE);
}

inline UintR LqhKeyReq::getCorrFactorFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_CORR_FACTOR_VALUE) & 1;
}

inline void LqhKeyReq::setDeferredConstraints(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setDeferredConstraints");
  requestInfo |= (val << RI_DEFERRED_CONSTRAINTS);
}

inline UintR LqhKeyReq::getDeferredConstraints(const UintR &requestInfo) {
  return (requestInfo >> RI_DEFERRED_CONSTRAINTS) & 1;
}

inline void LqhKeyReq::setDisableFkConstraints(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setDisableFkConstraints");
  requestInfo |= (val << RI_DISABLE_FK);
}

inline UintR LqhKeyReq::getDisableFkConstraints(const UintR &requestInfo) {
  return (requestInfo >> RI_DISABLE_FK) & 1;
}

inline UintR LqhKeyReq::getLongClearBits(const UintR &requestInfo) {
  const Uint32 mask = (1 << RI_CLEAR_SHIFT5) | (1 << RI_CLEAR_SHIFT6) |
                      (1 << RI_CLEAR_SHIFT7) | (1 << RI_CLEAR_SHIFT8) |
                      (1 << RI_CLEAR_SHIFT9);

  return (requestInfo & mask);
}

inline void LqhKeyReq::setNoTriggersFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setNoTriggersFlag");
  requestInfo |= (val << RI_NO_TRIGGERS);
}

inline UintR LqhKeyReq::getNoTriggersFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_NO_TRIGGERS) & 1;
}

inline void LqhKeyReq::setUtilFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setUtilFlag");
  requestInfo |= (val << RI_UTIL_SHIFT);
}

inline UintR LqhKeyReq::getUtilFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_UTIL_SHIFT) & 1;
}

inline void LqhKeyReq::setNoWaitFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setNoWaitFlag");
  requestInfo |= (val << RI_NOWAIT_SHIFT);
}

inline UintR LqhKeyReq::getNoWaitFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_NOWAIT_SHIFT) & 1;
}

inline Uint32 table_version_major_lqhkeyreq(Uint32 x) {
  // LQHKEYREQ only contains 16-bit schema version...
  return x & 0xFFFF;
}

inline void LqhKeyReq::setQueueOnRedoProblemFlag(UintR &requestInfo,
                                                 UintR val) {
  ASSERT_BOOL(val, "LqhKeyReq::setQueueOnRedoProblem");
  requestInfo |= (val << RI_QUEUE_REDO_SHIFT);
}

inline UintR LqhKeyReq::getQueueOnRedoProblemFlag(const UintR &requestInfo) {
  return (requestInfo >> RI_QUEUE_REDO_SHIFT) & 1;
}

class LqhKeyConf {
  /**
   * Reciver(s)
   */
  friend class Dbtc;
  friend class Restore;
  friend class Dbspj;

  /**
   * Sender(s)
   */
  friend class Dblqh;

  // Sent in a packed signal
  friend class PackedSignal;
  /**
   * For printing
   */
  friend bool printPACKED_SIGNAL(FILE *output, const Uint32 *theData,
                                 Uint32 len, Uint16 receiverBlockNo);
  friend bool printLQHKEYCONF(FILE *output, const Uint32 *theData, Uint32 len,
                              Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 SignalLength = 7;

 private:
  /**
   * DATA VARIABLES
   */
  Uint32 connectPtr;
  Uint32 opPtr;
  Uint32 userRef;
  union {
    /**
     * For read operations this variable contains the number of bytes read
     * For unlock operations this variable contains the unlocked op's TC REF
     */
    Uint32 readLen;
    Uint32 unlockTcRef;
  };
  Uint32 transId1;
  Uint32 transId2;
  Uint32 numFiredTriggers;  // bit 31 deferred trigger

  static Uint32 getFiredCount(Uint32 v) {
    return NoOfFiredTriggers::getFiredCount(v);
  }
  static Uint32 getDeferredUKBit(Uint32 v) {
    return NoOfFiredTriggers::getDeferredUKBit(v);
  }
  static void setDeferredUKBit(Uint32 &v) {
    NoOfFiredTriggers::setDeferredUKBit(v);
  }
  static Uint32 getDeferredFKBit(Uint32 v) {
    return NoOfFiredTriggers::getDeferredFKBit(v);
  }
  static void setDeferredFKBit(Uint32 &v) {
    NoOfFiredTriggers::setDeferredFKBit(v);
  }
};

class LqhKeyRef {
  /**
   * Reciver(s)
   */
  friend class Dbtc;
  friend class Dbspj;
  friend class Restore;

  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * For printing
   */
  friend bool printLQHKEYREF(FILE *output, const Uint32 *theData, Uint32 len,
                             Uint16 receiverBlockNo);

 public:
  static constexpr Uint32 SignalLengthWithoutFlags = 5;
  static constexpr Uint32 SignalLength = 6;

 private:
  /**
   * DATA VARIABLES
   */
  Uint32 userRef;
  Uint32 connectPtr;
  Uint32 errorCode;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 flags;

  static Uint32 getReplicaErrorFlag(const Uint32 &flags);
  static void setReplicaErrorFlag(Uint32 &flags, Uint32 val);

  enum Flags { LKR_REPLICA_ERROR_SHIFT = 0 };
};

inline Uint32 LqhKeyRef::getReplicaErrorFlag(const Uint32 &flags) {
  return ((flags >> LKR_REPLICA_ERROR_SHIFT) & 0x1);
}

inline void LqhKeyRef::setReplicaErrorFlag(Uint32 &flags, Uint32 val) {
  ASSERT_BOOL(val, "LqhKeyRef::setReplicaErrorFlag");
  flags |= (val << LKR_REPLICA_ERROR_SHIFT);
}

#undef JAM_FILE_ID

#endif

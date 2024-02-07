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

#ifndef TC_KEY_REQ_H
#define TC_KEY_REQ_H

#include <transporter/TransporterDefinitions.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 127

/**
 * @class TcKeyReq
 * @brief Contains KeyInfo and AttrInfo and is commonly followed by more signals
 *
 * - SENDER:    API, NDBCNTR
 * - RECEIVER:  TC
 *
 * Short TCKEYREQ
 * Prior to NDB 6.4.0, TCKEYREQ was always sent as a short signal train with
 * up to 8 words of KeyInfo and 5 words of AttrInfo in the TCKEYREQ signal, and
 * all other Key and AttrInfo sent in separate signal trains.  This format is
 * supported for non NdbRecord operations, backwards compatibility, and for
 * internal TCKEYREQ signals received from non-API clients.
 *
 * Long TCKEYREQ
 * From NDB 6.4.0, for NdbRecord operations the API nodes send long TCKEYREQ
 * signals with all KeyInfo and AttrInfo in long sections sent with the
 * TCKEYREQ signal.  As each section has a section length, and no Key/AttrInfo
 * is sent in the TCKEYREQ signal itself, the KeyLength, AttrInfoLen and
 * AIInTcKeyReq fields of the header are no longer required, and their bits
 * can be reused in future.
 */
class TcKeyReq {
  /**
   * Receiver(s)
   */
  friend class Dbtc;  // Receiver

  /**
   * Sender(s)
   */
  friend class Ndbcntr;
  friend class NdbQueryImpl;
  friend class NdbOperation;
  friend class NdbIndexOperation;
  friend class NdbScanOperation;
  friend class NdbBlob;
  friend class DbUtil;
  friend class Trix;

  /**
   * For printing
   */
  friend bool printTCKEYREQ(FILE *, const Uint32 *, Uint32, Uint16);
  friend bool printTCINDXREQ(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  /**
   * Length of signal
   */
  static constexpr Uint32 StaticLength = 8;
  static constexpr Uint32 SignalLength = 25;
  static constexpr Uint32 MaxKeyInfo = 8;
  static constexpr Uint32 MaxAttrInfo = 5;
  static constexpr Uint32 MaxTotalAttrInfo =
      ((MAX_SEND_MESSAGE_BYTESIZE / 4) - SignalLength);

  /**
   * Long signal variant of TCKEYREQ
   */
  static constexpr Uint32 KeyInfoSectionNum = 0;
  static constexpr Uint32 AttrInfoSectionNum = 1;

  static constexpr Uint32 UnlockKeyLen = 2;

 private:
  enum AbortOption {
    CommitIfFailFree = 0,
    AbortOnError = 0,
    CommitAsMuchAsPossible = 2,
    IgnoreError = 2
  };

  typedef AbortOption CommitType;

  /**
   * DATA VARIABLES
   */

  // ----------------------------------------------------------------------
  //  Unconditional part = must be present in signal.  First 8 words
  // ----------------------------------------------------------------------
  Uint32 apiConnectPtr;  // DATA 0
  union {
    Uint32 senderData;
    UintR apiOperationPtr;  // DATA 1
  };
  /**
   * Short TCKEYREQ only :
   *   ATTRIBUTE INFO (attrinfo) LENGTH
   *   This is the total length of all attribute info that is sent from
   *   the application as part of this operation.
   *   It includes all attribute info sent in possible attrinfo
   *   signals as well as the attribute info sent in TCKEYREQ.
   *
   * Long TCKEYREQ
   *   ATTRIBUTE INFO (attrinfo) LENGTH is unused in signal.
   *   Get AttrInfoLength from length of section 1, if present.
   *
   */
  UintR attrLen;             // DATA 2
  UintR tableId;             // DATA 3
  UintR requestInfo;         // DATA 4   Various transaction flags
  UintR tableSchemaVersion;  // DATA 5
  UintR transId1;            // DATA 6
  UintR transId2;            // DATA 7

  // ----------------------------------------------------------------------
  //  Conditional part = can be present in signal.
  //  These four words will be sent only if their indicator is set.
  // ----------------------------------------------------------------------
  UintR scanInfo;             // DATA 8   Various flags for scans, see below
  UintR distrGroupHashValue;  // DATA 9
  UintR distributionKeySize;  // DATA 10
  UintR storedProcId;         // DATA 11

  // ----------------------------------------------------------------------
  //  Variable sized KEY and ATTRINFO part.
  //  These will be placed to pack the signal in an appropriate manner.
  // ----------------------------------------------------------------------
  UintR keyInfo[MaxKeyInfo];    // DATA 12 - 19
  UintR attrInfo[MaxAttrInfo];  // DATA 20 - 24

  /**
   * Get:ers for attrLen
   */

  static Uint16 getAttrinfoLen(const UintR &attrLen);
  static void setAttrinfoLen(UintR &attrLen, Uint16 aiLen);

  /**
   * Get:ers for requestInfo
   */
  static Uint8 getCommitFlag(const UintR &requestInfo);
  static Uint8 getAbortOption(const UintR &requestInfo);
  static Uint8 getStartFlag(const UintR &requestInfo);
  static Uint8 getSimpleFlag(const UintR &requestInfo);
  static Uint8 getDirtyFlag(const UintR &requestInfo);
  static Uint8 getInterpretedFlag(const UintR &requestInfo);
  static Uint8 getDistributionKeyFlag(const UintR &requestInfo);
  static Uint8 getViaSPJFlag(const UintR &requestInfo);
  static Uint8 getScanIndFlag(const UintR &requestInfo);
  static Uint8 getOperationType(const UintR &requestInfo);
  static Uint8 getExecuteFlag(const UintR &requestInfo);
  static Uint8 getReadCommittedBaseFlag(const UintR &TrequestInfo);

  static Uint16 getKeyLength(const UintR &requestInfo);
  static Uint8 getAIInTcKeyReq(const UintR &requestInfo);
  static UintR getNoDiskFlag(const UintR &requestInfo);

  static UintR getCoordinatedTransactionFlag(const UintR &requestInfo);

  /**
   * Get:ers for scanInfo
   */
  static Uint8 getTakeOverScanFlag(const UintR &scanInfo);
  static Uint16 getTakeOverScanFragment(const UintR &scanInfo);
  static Uint32 getTakeOverScanInfo(const UintR &scanInfo);

  /**
   * Set:ers for requestInfo
   */
  static void clearRequestInfo(UintR &requestInfo);
  static void setAbortOption(UintR &requestInfo, Uint32 type);
  static void setCommitFlag(UintR &requestInfo, Uint32 flag);
  static void setStartFlag(UintR &requestInfo, Uint32 flag);
  static void setSimpleFlag(UintR &requestInfo, Uint32 flag);
  static void setDirtyFlag(UintR &requestInfo, Uint32 flag);
  static void setInterpretedFlag(UintR &requestInfo, Uint32 flag);
  static void setDistributionKeyFlag(UintR &requestInfo, Uint32 flag);
  static void setViaSPJFlag(UintR &requestInfo, Uint32 flag);
  static void setScanIndFlag(UintR &requestInfo, Uint32 flag);
  static void setExecuteFlag(UintR &requestInfo, Uint32 flag);
  static void setOperationType(UintR &requestInfo, Uint32 type);
  static void setReadCommittedBaseFlag(UintR &requestInfo, Uint32 flag);

  static void setKeyLength(UintR &requestInfo, Uint32 len);
  static void setAIInTcKeyReq(UintR &requestInfo, Uint32 len);
  static void setNoDiskFlag(UintR &requestInfo, UintR val);

  static void setReorgFlag(UintR &requestInfo, UintR val);
  static UintR getReorgFlag(const UintR &requestInfo);
  static void setCoordinatedTransactionFlag(UintR &requestInfo, UintR val);
  static void setQueueOnRedoProblemFlag(UintR &requestInfo, UintR val);
  static UintR getQueueOnRedoProblemFlag(const UintR &requestInfo);

  /**
   * Check constraints deferred
   */
  static UintR getDeferredConstraints(const UintR &requestInfo);
  static void setDeferredConstraints(UintR &requestInfo, UintR val);

  /**
   * Foreign key constraints disabled
   */
  static UintR getDisableFkConstraints(const UintR &requestInfo);
  static void setDisableFkConstraints(UintR &requestInfo, UintR val);

  /**
   * Set:ers for scanInfo
   */
  static void setTakeOverScanFlag(UintR &scanInfo, Uint8 flag);
  static void setTakeOverScanFragment(UintR &scanInfo, Uint16 fragment);
  static void setTakeOverScanInfo(UintR &scanInfo, Uint32 aScanInfo);

  /**
   * Nowait option
   */
  static void setNoWaitFlag(UintR &requestInfo, UintR val);
  static UintR getNoWaitFlag(const UintR &requestInfo);
};

/**
 * Request Info
 *
 a = Attr Info in TCKEYREQ - 3  Bits -> Max 7 (Bit 16-18)
     (Short TCKEYREQ only, for long req a == 0)
 b = Distribution Key Ind  - 1  Bit 2
 v = Via SPJ               - 1  Bit 3
 c = Commit Indicator      - 1  Bit 4
 d = Dirty Indicator       - 1  Bit 0
 e = Scan Indicator        - 1  Bit 14
 i = Interpreted Indicator - 1  Bit 15
 k = Key length            - 12 Bits -> Max 4095 (Bit 20 - 31)
     (Short TCKEYREQ only, for long req use length of
      section 0)
 o = Operation Type        - 3  Bits -> Max 7 (Bit 5-7)
 l = Execute               - 1  Bit 10
 p = Simple Indicator      - 1  Bit 8
 s = Start Indicator       - 1  Bit 11
 y = Commit Type           - 2  Bit 12-13
 n = No disk flag          - 1  Bit 1
 r = reorg flag            - 1  Bit 19
 x = Coordinated Tx flag   - 1  Bit 16
 q = Queue on redo problem - 1  Bit 9
 D = deferred constraint   - 1  Bit 17
 f = Disable FK constraint - 1  Bit 18

 * Read committed base is using a bit that is only available
 * in Long TCKEYREQ signals. So this feature is only available
 * when using Long TCKEYREQ signals. Short TCKEYREQ are only
 * used for backwards compatibility against old nodes not
 * supporting Read Committed base flag anyway and in special
 * test cases that also don't use Read Committed base.

 R = Read Committed base   - 1  Bit 20
 w = NoWait read           - 1  Bit 21

           1111111111222222222233
 01234567890123456789012345678901
 dnb cooop lsyyeiaaarkkkkkkkkkkkk  (Short TCKEYREQ)
 dnbvcooopqlsyyeixDfrRw            (Long TCKEYREQ)
*/

#define TCKEY_NODISK_SHIFT (1)
#define COMMIT_SHIFT (4)
#define START_SHIFT (11)
#define SIMPLE_SHIFT (8)
#define DIRTY_SHIFT (0)
#define EXECUTE_SHIFT (10)
#define INTERPRETED_SHIFT (15)
#define DISTR_KEY_SHIFT (2)
#define VIA_SPJ_SHIFT (3)
#define SCAN_SHIFT (14)

#define OPERATION_SHIFT (5)
#define OPERATION_MASK (7)

#define AINFO_SHIFT (16)
#define AINFO_MASK (7)

#define KEY_LEN_SHIFT (20)
#define KEY_LEN_MASK (4095)

#define COMMIT_TYPE_SHIFT (12)
#define COMMIT_TYPE_MASK (3)

#define TC_REORG_SHIFT (19)
#define QUEUE_ON_REDO_SHIFT (9)

#define TC_COORDINATED_SHIFT (16)
#define TC_DEFERRED_CONSTAINTS_SHIFT (17)

#define TC_DISABLE_FK_SHIFT (18)
#define TC_READ_COMMITTED_BASE_SHIFT (20)
#define TC_NOWAIT_SHIFT (21)

/**
 * Scan Info
 *
 * Scan Info is used to identify the row and lock to take over from a scan.
 *
 * If "Scan take over indicator" is set, this operation will take over a lock
 * currently held on a row being scanned.
 * Scan locks not taken over in this way (by same or other transaction) are
 * released when fetching the next batch of rows (SCAN_NEXTREQ signal).
 * The value for "take over node" and "scan info" are obtained from the
 * KEYINFO20 signal sent to NDB API by LQH if requested in SCAN_TABREQ.
 *
 t = Scan take over indicator -  1 Bit
 n = Take over node           - 12 Bits -> max 4095
 p = Scan Info                - 18 Bits -> max 0x3ffff

           1111111111222222222233
 01234567890123456789012345678901
 tpppppppppppppppppp nnnnnnnnnnnn
*/

#define TAKE_OVER_SHIFT (0)

#define TAKE_OVER_FRAG_SHIFT (20)
#define TAKE_OVER_FRAG_MASK (4095)

#define SCAN_INFO_SHIFT (1)
#define SCAN_INFO_MASK (0x3ffff)

/**
 * Attr Len
 *
 n = Attrinfo length(words)   - 16 Bits -> max 65535 (Short TCKEYREQ only)
 a = removed was API version no  - 16 Bits -> max 65535
 API version no is more than 16 bits, was not used in kernel
 (removed in 7.3.3, 7.2.14, 7.1.29, 7.0.40, 6.3.53)

           1111111111222222222233
 01234567890123456789012345678901
 aaaaaaaaaaaaaaaannnnnnnnnnnnnnnn   (Short TCKEYREQ)
 aaaaaaaaaaaaaaaa                   (Long TCKEYREQ)
*/

#define ATTRLEN_SHIFT (0)
#define ATTRLEN_MASK (65535)

inline Uint8 TcKeyReq::getCommitFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> COMMIT_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getAbortOption(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> COMMIT_TYPE_SHIFT) & COMMIT_TYPE_MASK);
}

inline Uint8 TcKeyReq::getStartFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> START_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getSimpleFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> SIMPLE_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getExecuteFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> EXECUTE_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getReadCommittedBaseFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> TC_READ_COMMITTED_BASE_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getDirtyFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> DIRTY_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getInterpretedFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> INTERPRETED_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getDistributionKeyFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> DISTR_KEY_SHIFT) & 1);
}

inline UintR TcKeyReq::getCoordinatedTransactionFlag(const UintR &requestInfo) {
  return (UintR)((requestInfo >> TC_COORDINATED_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getViaSPJFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> VIA_SPJ_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getScanIndFlag(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> SCAN_SHIFT) & 1);
}

inline Uint8 TcKeyReq::getOperationType(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> OPERATION_SHIFT) & OPERATION_MASK);
}

inline Uint16 TcKeyReq::getKeyLength(const UintR &requestInfo) {
  return (Uint16)((requestInfo >> KEY_LEN_SHIFT) & KEY_LEN_MASK);
}

inline Uint8 TcKeyReq::getAIInTcKeyReq(const UintR &requestInfo) {
  return (Uint8)((requestInfo >> AINFO_SHIFT) & AINFO_MASK);
}

inline void TcKeyReq::clearRequestInfo(UintR &requestInfo) { requestInfo = 0; }

inline void TcKeyReq::setAbortOption(UintR &requestInfo, Uint32 type) {
  ASSERT_MAX(type, COMMIT_TYPE_MASK, "TcKeyReq::setAbortOption");
  requestInfo &= ~(COMMIT_TYPE_MASK << COMMIT_TYPE_SHIFT);
  requestInfo |= (type << COMMIT_TYPE_SHIFT);
}

inline void TcKeyReq::setCommitFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setCommitFlag");
  requestInfo &= ~(1 << COMMIT_SHIFT);
  requestInfo |= (flag << COMMIT_SHIFT);
}

inline void TcKeyReq::setStartFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setStartFlag");
  requestInfo &= ~(1 << START_SHIFT);
  requestInfo |= (flag << START_SHIFT);
}

inline void TcKeyReq::setSimpleFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setSimpleFlag");
  requestInfo &= ~(1 << SIMPLE_SHIFT);
  requestInfo |= (flag << SIMPLE_SHIFT);
}

inline void TcKeyReq::setDirtyFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setDirstFlag");
  requestInfo &= ~(1 << DIRTY_SHIFT);
  requestInfo |= (flag << DIRTY_SHIFT);
}

inline void TcKeyReq::setExecuteFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setExecuteFlag");
  requestInfo &= ~(1 << EXECUTE_SHIFT);
  requestInfo |= (flag << EXECUTE_SHIFT);
}

inline void TcKeyReq::setReadCommittedBaseFlag(UintR &requestInfo,
                                               Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setReadCommittedBaseFlag");
  requestInfo &= ~(1 << TC_READ_COMMITTED_BASE_SHIFT);
  requestInfo |= (flag << TC_READ_COMMITTED_BASE_SHIFT);
}

inline void TcKeyReq::setInterpretedFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setInterpretedFlag");
  requestInfo &= ~(1 << INTERPRETED_SHIFT);
  requestInfo |= (flag << INTERPRETED_SHIFT);
}

inline void TcKeyReq::setDistributionKeyFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setDistributionKeyFlag");
  requestInfo &= ~(1 << DISTR_KEY_SHIFT);
  requestInfo |= (flag << DISTR_KEY_SHIFT);
}

inline void TcKeyReq::setCoordinatedTransactionFlag(UintR &requestInfo,
                                                    UintR flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setCoordinatedTransactionFlag");
  requestInfo &= ~(1 << TC_COORDINATED_SHIFT);
  requestInfo |= (flag << TC_COORDINATED_SHIFT);
}

inline void TcKeyReq::setViaSPJFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setViaSPJFlag");
  requestInfo &= ~(1 << VIA_SPJ_SHIFT);
  requestInfo |= (flag << VIA_SPJ_SHIFT);
}

inline void TcKeyReq::setScanIndFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setScanIndFlag");
  requestInfo &= ~(1 << SCAN_SHIFT);
  requestInfo |= (flag << SCAN_SHIFT);
}

inline void TcKeyReq::setOperationType(UintR &requestInfo, Uint32 type) {
  ASSERT_MAX(type, OPERATION_MASK, "TcKeyReq::setOperationType");
  requestInfo &= ~(OPERATION_MASK << OPERATION_SHIFT);
  requestInfo |= (type << OPERATION_SHIFT);
}

inline void TcKeyReq::setKeyLength(UintR &requestInfo, Uint32 len) {
  ASSERT_MAX(len, KEY_LEN_MASK, "TcKeyReq::setKeyLength");
  requestInfo &= ~(KEY_LEN_MASK << KEY_LEN_SHIFT);
  requestInfo |= (len << KEY_LEN_SHIFT);
}

inline void TcKeyReq::setAIInTcKeyReq(UintR &requestInfo, Uint32 len) {
  ASSERT_MAX(len, AINFO_MASK, "TcKeyReq::setAIInTcKeyReq");
  requestInfo &= ~(AINFO_MASK << AINFO_SHIFT);
  requestInfo |= (len << AINFO_SHIFT);
}

inline Uint8 TcKeyReq::getTakeOverScanFlag(const UintR &scanInfo) {
  return (Uint8)((scanInfo >> TAKE_OVER_SHIFT) & 1);
}

inline Uint16 TcKeyReq::getTakeOverScanFragment(const UintR &scanInfo) {
  return (Uint16)((scanInfo >> TAKE_OVER_FRAG_SHIFT) & TAKE_OVER_FRAG_MASK);
}

inline Uint32 TcKeyReq::getTakeOverScanInfo(const UintR &scanInfo) {
  return (Uint32)((scanInfo >> SCAN_INFO_SHIFT) & SCAN_INFO_MASK);
}

inline void TcKeyReq::setTakeOverScanFlag(UintR &scanInfo, Uint8 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setTakeOverScanFlag");
  scanInfo |= (flag << TAKE_OVER_SHIFT);
}

inline void TcKeyReq::setTakeOverScanFragment(UintR &scanInfo, Uint16 node) {
  //  ASSERT_MAX(node, TAKE_OVER_NODE_MASK, "TcKeyReq::setTakeOverScanNode");
  scanInfo |= (node << TAKE_OVER_FRAG_SHIFT);
}

inline void TcKeyReq::setTakeOverScanInfo(UintR &scanInfo, Uint32 aScanInfo) {
  //  ASSERT_MAX(aScanInfo, SCAN_INFO_MASK, "TcKeyReq::setTakeOverScanInfo");
  scanInfo |= (aScanInfo << SCAN_INFO_SHIFT);
}

inline Uint16 TcKeyReq::getAttrinfoLen(const UintR &anAttrLen) {
  return (Uint16)((anAttrLen)&ATTRLEN_MASK);
}

inline void TcKeyReq::setAttrinfoLen(UintR &anAttrLen, Uint16 aiLen) {
  //  ASSERT_MAX(aiLen, ATTRLEN_MASK, "TcKeyReq::setAttrinfoLen");
  anAttrLen |= aiLen;
}

inline UintR TcKeyReq::getNoDiskFlag(const UintR &requestInfo) {
  return (requestInfo >> TCKEY_NODISK_SHIFT) & 1;
}

inline void TcKeyReq::setNoDiskFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setNoDiskFlag");
  requestInfo &= ~(1 << TCKEY_NODISK_SHIFT);
  requestInfo |= (flag << TCKEY_NODISK_SHIFT);
}

inline UintR TcKeyReq::getReorgFlag(const UintR &requestInfo) {
  return (requestInfo >> TC_REORG_SHIFT) & 1;
}

inline void TcKeyReq::setReorgFlag(UintR &requestInfo, Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setReorgFlag");
  requestInfo |= (flag << TC_REORG_SHIFT);
}

inline UintR TcKeyReq::getQueueOnRedoProblemFlag(const UintR &requestInfo) {
  return (requestInfo >> QUEUE_ON_REDO_SHIFT) & 1;
}

inline void TcKeyReq::setQueueOnRedoProblemFlag(UintR &requestInfo,
                                                Uint32 flag) {
  ASSERT_BOOL(flag, "TcKeyReq::setNoDiskFlag");
  requestInfo |= (flag << QUEUE_ON_REDO_SHIFT);
}

inline void TcKeyReq::setDeferredConstraints(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "TcKeyReq::setDeferredConstraints");
  requestInfo |= (val << TC_DEFERRED_CONSTAINTS_SHIFT);
}

inline UintR TcKeyReq::getDeferredConstraints(const UintR &requestInfo) {
  return (requestInfo >> TC_DEFERRED_CONSTAINTS_SHIFT) & 1;
}

inline void TcKeyReq::setDisableFkConstraints(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "TcKeyReq::setDisableFkConstraints");
  requestInfo |= (val << TC_DISABLE_FK_SHIFT);
}

inline UintR TcKeyReq::getDisableFkConstraints(const UintR &requestInfo) {
  return (requestInfo >> TC_DISABLE_FK_SHIFT) & 1;
}

inline void TcKeyReq::setNoWaitFlag(UintR &requestInfo, UintR val) {
  ASSERT_BOOL(val, "TcKeyReq::setNoWaitFlag");
  requestInfo |= (val << TC_NOWAIT_SHIFT);
}

inline UintR TcKeyReq::getNoWaitFlag(const UintR &requestInfo) {
  return (requestInfo >> TC_NOWAIT_SHIFT) & 1;
}

#undef JAM_FILE_ID

#endif

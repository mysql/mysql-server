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

#ifndef TC_KEY_REQ_H
#define TC_KEY_REQ_H

#include "SignalData.hpp"

/**
 * @class TcKeyReq
 * @brief Contains KeyInfo and AttrInfo and is commonly followed by more signals
 *
 * - SENDER:    API, NDBCNTR
 * - RECEIVER:  TC
 */
class TcKeyReq {
  /**
   * Receiver(s)
   */
  friend class Dbtc;         // Receiver

  /**
   * Sender(s)
   */
  friend class Ndbcntr;      
  friend class NdbOperation; 
  friend class NdbIndexOperation;
  friend class NdbScanOperation;
  friend class DbUtil;

  /**
   * For printing
   */
  friend bool printTCKEYREQ(FILE *, const Uint32 *, Uint32, Uint16);
  friend bool printTCINDXREQ(FILE *, const Uint32 *, Uint32, Uint16);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( StaticLength = 8 );
  STATIC_CONST( SignalLength = 25 );
  STATIC_CONST( MaxKeyInfo = 8 );
  STATIC_CONST( MaxAttrInfo = 5 );
  STATIC_CONST( MaxTotalAttrInfo = 0xFFFF );

private:

  enum AbortOption {
    CommitIfFailFree = 0, AbortOnError = 0,
    CommitAsMuchAsPossible = 2, IgnoreError = 2
  };
  
  typedef AbortOption CommitType;
  
  /**
   * DATA VARIABLES
   */

  // ----------------------------------------------------------------------
  //  Unconditional part = must be present in signal.  First 8 words
  // ----------------------------------------------------------------------
  Uint32 apiConnectPtr;        // DATA 0
  union {
    Uint32 senderData;
    UintR apiOperationPtr;      // DATA 1
  };
  /**
   * ATTRIBUTE INFO (attrinfo) LENGTH
   * This is the total length of all attribute info that is sent from
   * the application as part of this operation. 
   * It includes all attribute info sent in possible attrinfo 
   * signals as well as the attribute info sent in TCKEYREQ.
   */
  UintR attrLen;              // DATA 2   (also stores API Version)
  UintR tableId;              // DATA 3
  UintR requestInfo;          // DATA 4   Various transaction flags
  UintR tableSchemaVersion;   // DATA 5
  UintR transId1;             // DATA 6
  UintR transId2;             // DATA 7

  // ----------------------------------------------------------------------
  //  Conditional part = can be present in signal. 
  //  These four words will be sent only if their indicator is set.
  // ----------------------------------------------------------------------
  UintR scanInfo;             // DATA 8   Various flags for scans
  UintR distrGroupHashValue;  // DATA 9
  UintR distributionKeySize;  // DATA 10
  UintR storedProcId;         // DATA 11

  // ----------------------------------------------------------------------
  //  Variable sized KEY and ATTRINFO part. 
  //  These will be placed to pack the signal in an appropriate manner.
  // ----------------------------------------------------------------------
  UintR keyInfo[MaxKeyInfo];           // DATA 12 - 19
  UintR attrInfo[MaxAttrInfo];          // DATA 20 - 24
  
  /**
   * Get:ers for attrLen
   */ 

  static Uint16  getAPIVersion(const UintR & attrLen);
  static Uint16  getAttrinfoLen(const UintR & attrLen);
  static void setAPIVersion(UintR & attrLen, Uint16 apiVersion);
  static void setAttrinfoLen(UintR & attrLen, Uint16 aiLen);


  /**
   * Get:ers for requestInfo
   */
  static Uint8 getCommitFlag(const UintR & requestInfo);
  static Uint8 getAbortOption(const UintR & requestInfo);
  static Uint8 getStartFlag(const UintR & requestInfo);
  static Uint8 getSimpleFlag(const UintR & requestInfo);
  static Uint8 getDirtyFlag(const UintR & requestInfo);
  static Uint8 getInterpretedFlag(const UintR & requestInfo);
  static Uint8 getDistributionGroupFlag(const UintR & requestInfo);
  static Uint8 getDistributionGroupTypeFlag(const UintR & requestInfo);
  static Uint8 getDistributionKeyFlag(const UintR & requestInfo);
  static Uint8 getScanIndFlag(const UintR & requestInfo);
  static Uint8 getOperationType(const UintR & requestInfo);
  static Uint8 getExecuteFlag(const UintR & requestInfo);

  static Uint16 getKeyLength(const UintR & requestInfo);
  static Uint8  getAIInTcKeyReq(const UintR & requestInfo);
  static Uint8  getExecutingTrigger(const UintR & requestInfo);

  /**
   * Get:ers for scanInfo
   */
  static Uint8  getTakeOverScanFlag(const UintR & scanInfo);
  static Uint16 getTakeOverScanNode(const UintR & scanInfo);
  static Uint32 getTakeOverScanInfo(const UintR & scanInfo);


  /**
   * Set:ers for requestInfo
   */
  static void clearRequestInfo(UintR & requestInfo);
  static void setAbortOption(UintR & requestInfo, Uint32 type);
  static void setCommitFlag(UintR & requestInfo, Uint32 flag);
  static void setStartFlag(UintR & requestInfo, Uint32 flag);
  static void setSimpleFlag(UintR & requestInfo, Uint32 flag);
  static void setDirtyFlag(UintR & requestInfo, Uint32 flag);
  static void setInterpretedFlag(UintR & requestInfo, Uint32 flag);
  static void setDistributionGroupFlag(UintR & requestInfo, Uint32 flag);
  static void setDistributionGroupTypeFlag(UintR & requestInfo, Uint32 flag);
  static void setDistributionKeyFlag(UintR & requestInfo, Uint32 flag);
  static void setScanIndFlag(UintR & requestInfo, Uint32 flag);
  static void setExecuteFlag(UintR & requestInfo, Uint32 flag);  
  static void setOperationType(UintR & requestInfo, Uint32 type);
  
  static void setKeyLength(UintR & requestInfo, Uint32 len);
  static void setAIInTcKeyReq(UintR & requestInfo, Uint32 len);
  static void setExecutingTrigger(UintR & requestInfo, Uint32 flag);

  /**
   * Set:ers for scanInfo
   */
  static void setTakeOverScanFlag(UintR & scanInfo, Uint8 flag);
  static void setTakeOverScanNode(UintR & scanInfo, Uint16 node);
  static void setTakeOverScanInfo(UintR & scanInfo, Uint32 aScanInfo);
};

/**
 * Request Info
 *
 a = Attr Info in TCKEYREQ - 3  Bits -> Max 7 (Bit 16-18)
 b = Distribution Key Ind  - 1  Bit 2
 c = Commit Indicator      - 1  Bit 4
 d = Dirty Indicator       - 1  Bit 0
 e = Scan Indicator        - 1  Bit 14
 f = Execute fired trigger - 1  Bit 19 
 g = Distribution Group Ind- 1  Bit 1
 i = Interpreted Indicator - 1  Bit 15
 k = Key length            - 12 Bits -> Max 4095 (Bit 20 - 31)
 o = Operation Type        - 3  Bits -> Max 7 (Bit 5-7)
 l = Execute               - 1  Bit 10
 p = Simple Indicator      - 1  Bit 8
 s = Start Indicator       - 1  Bit 11
 t = Distribution GroupType- 1  Bit 3
 y = Commit Type           - 2  Bit 12-13

           1111111111222222222233
 01234567890123456789012345678901
 dgbtcooop lsyyeiaaafkkkkkkkkkkkk
*/

#define COMMIT_SHIFT       (4)
#define START_SHIFT        (11)
#define SIMPLE_SHIFT       (8)
#define DIRTY_SHIFT        (0)
#define EXECUTE_SHIFT      (10)
#define INTERPRETED_SHIFT  (15)
#define DISTR_GROUP_SHIFT  (1)
#define DISTR_GROUP_TYPE_SHIFT  (3)
#define DISTR_KEY_SHIFT    (2)
#define SCAN_SHIFT         (14)

#define OPERATION_SHIFT   (5)
#define OPERATION_MASK    (7)

#define AINFO_SHIFT       (16)
#define AINFO_MASK        (7)

#define KEY_LEN_SHIFT     (20)
#define KEY_LEN_MASK      (4095)

#define COMMIT_TYPE_SHIFT  (12)
#define COMMIT_TYPE_MASK   (3)

#define EXECUTING_TRIGGER_SHIFT (19)

/**
 * Scan Info
 *
 t = Scan take over indicator -  1 Bit
 n = Take over node           - 12 Bits -> max 65535
 p = Scan Info                - 18 Bits -> max 4095

           1111111111222222222233
 01234567890123456789012345678901
 tpppppppppppppppppp nnnnnnnnnnnn
*/

#define TAKE_OVER_SHIFT      (0)

#define TAKE_OVER_NODE_SHIFT (20)
#define TAKE_OVER_NODE_MASK  (4095)

#define SCAN_INFO_SHIFT      (1)
#define SCAN_INFO_MASK       (262143)

/**
 * Attr Len
 *
 n = Attrinfo length(words)   - 16 Bits -> max 65535
 a = API version no           - 16 Bits -> max 65535

           1111111111222222222233
 01234567890123456789012345678901
 aaaaaaaaaaaaaaaannnnnnnnnnnnnnnn
*/

#define API_VER_NO_SHIFT     (16)
#define API_VER_NO_MASK      (65535)

#define ATTRLEN_SHIFT        (0)
#define ATTRLEN_MASK         (65535)

inline
Uint8
TcKeyReq::getCommitFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> COMMIT_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getAbortOption(const UintR & requestInfo){
  return (Uint8)((requestInfo >> COMMIT_TYPE_SHIFT) & COMMIT_TYPE_MASK);
}

inline
Uint8
TcKeyReq::getStartFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> START_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getSimpleFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> SIMPLE_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getExecuteFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> EXECUTE_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getDirtyFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DIRTY_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getInterpretedFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> INTERPRETED_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getDistributionGroupFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DISTR_GROUP_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getDistributionGroupTypeFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DISTR_GROUP_TYPE_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getDistributionKeyFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DISTR_KEY_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getScanIndFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> SCAN_SHIFT) & 1);
}

inline
Uint8
TcKeyReq::getOperationType(const UintR & requestInfo){
  return (Uint8)((requestInfo >> OPERATION_SHIFT) & OPERATION_MASK);
}

inline
Uint16 
TcKeyReq::getKeyLength(const UintR & requestInfo){
  return (Uint16)((requestInfo >> KEY_LEN_SHIFT) & KEY_LEN_MASK);
}

inline
Uint8
TcKeyReq::getAIInTcKeyReq(const UintR & requestInfo){
  return (Uint8)((requestInfo >> AINFO_SHIFT) & AINFO_MASK);
}

inline
Uint8
TcKeyReq::getExecutingTrigger(const UintR & requestInfo){
  return (Uint8)((requestInfo >> EXECUTING_TRIGGER_SHIFT) & 1);
}

inline
void 
TcKeyReq::clearRequestInfo(UintR & requestInfo){
  requestInfo = 0;
}

inline
void 
TcKeyReq::setAbortOption(UintR & requestInfo, Uint32 type){
  ASSERT_MAX(type, COMMIT_TYPE_MASK, "TcKeyReq::setAbortOption");
  requestInfo &= ~(COMMIT_TYPE_MASK << COMMIT_TYPE_SHIFT);
  requestInfo |= (type << COMMIT_TYPE_SHIFT);
}

inline
void 
TcKeyReq::setCommitFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setCommitFlag");
  requestInfo &= ~(1 << COMMIT_SHIFT);
  requestInfo |= (flag << COMMIT_SHIFT);
}

inline
void 
TcKeyReq::setStartFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setStartFlag");
  requestInfo &= ~(1 << START_SHIFT);
  requestInfo |= (flag << START_SHIFT);
}

inline
void 
TcKeyReq::setSimpleFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setSimpleFlag");
  requestInfo &= ~(1 << SIMPLE_SHIFT);
  requestInfo |= (flag << SIMPLE_SHIFT);
}

inline
void 
TcKeyReq::setDirtyFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setDirstFlag");
  requestInfo &= ~(1 << DIRTY_SHIFT);
  requestInfo |= (flag << DIRTY_SHIFT);
}

inline
void 
TcKeyReq::setExecuteFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setExecuteFlag");
  requestInfo &= ~(1 << EXECUTE_SHIFT);
  requestInfo |= (flag << EXECUTE_SHIFT);
}

inline
void 
TcKeyReq::setInterpretedFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setInterpretedFlag");
  requestInfo &= ~(1 << INTERPRETED_SHIFT);
  requestInfo |= (flag << INTERPRETED_SHIFT);
}

inline
void 
TcKeyReq::setDistributionGroupTypeFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setDistributionGroupTypeFlag");
  requestInfo &= ~(1 << DISTR_GROUP_TYPE_SHIFT);
  requestInfo |= (flag << DISTR_GROUP_TYPE_SHIFT);
}

inline
void 
TcKeyReq::setDistributionGroupFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setDistributionGroupFlag");
  requestInfo &= ~(1 << DISTR_GROUP_SHIFT);
  requestInfo |= (flag << DISTR_GROUP_SHIFT);
}

inline
void 
TcKeyReq::setDistributionKeyFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setDistributionKeyFlag");
  requestInfo &= ~(1 << DISTR_KEY_SHIFT);
  requestInfo |= (flag << DISTR_KEY_SHIFT);
}

inline
void 
TcKeyReq::setScanIndFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setScanIndFlag");
  requestInfo &= ~(1 << SCAN_SHIFT);
  requestInfo |= (flag << SCAN_SHIFT);
}

inline
void 
TcKeyReq::setOperationType(UintR & requestInfo, Uint32 type){
  ASSERT_MAX(type, OPERATION_MASK, "TcKeyReq::setOperationType");
  requestInfo &= ~(OPERATION_MASK << OPERATION_SHIFT);
  requestInfo |= (type << OPERATION_SHIFT);
}

inline
void 
TcKeyReq::setKeyLength(UintR & requestInfo, Uint32 len){
  ASSERT_MAX(len, KEY_LEN_MASK, "TcKeyReq::setKeyLength");
  requestInfo &= ~(KEY_LEN_MASK << KEY_LEN_SHIFT);
  requestInfo |= (len << KEY_LEN_SHIFT);
}

inline
void 
TcKeyReq::setAIInTcKeyReq(UintR & requestInfo, Uint32 len){
  ASSERT_MAX(len, AINFO_MASK, "TcKeyReq::setAIInTcKeyReq");
  requestInfo &= ~(AINFO_MASK << AINFO_SHIFT);
  requestInfo |= (len << AINFO_SHIFT);
}

inline
void 
TcKeyReq::setExecutingTrigger(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setExecutingTrigger");
  requestInfo &= ~(1 << EXECUTING_TRIGGER_SHIFT);
  requestInfo |= (flag << EXECUTING_TRIGGER_SHIFT);
}

inline
Uint8 
TcKeyReq::getTakeOverScanFlag(const UintR & scanInfo){
  return (Uint8)((scanInfo >> TAKE_OVER_SHIFT) & 1);
}

inline
Uint16
TcKeyReq::getTakeOverScanNode(const UintR & scanInfo){
  return (Uint16)((scanInfo >> TAKE_OVER_NODE_SHIFT) & TAKE_OVER_NODE_MASK);
}

inline
Uint32 
TcKeyReq::getTakeOverScanInfo(const UintR & scanInfo){
  return (Uint32)((scanInfo >> SCAN_INFO_SHIFT) & SCAN_INFO_MASK);
}


inline
void
TcKeyReq::setTakeOverScanFlag(UintR & scanInfo, Uint8 flag){
  ASSERT_BOOL(flag, "TcKeyReq::setTakeOverScanFlag");
  scanInfo |= (flag << TAKE_OVER_SHIFT);
}

inline
void
TcKeyReq::setTakeOverScanNode(UintR & scanInfo, Uint16 node){
//  ASSERT_MAX(node, TAKE_OVER_NODE_MASK, "TcKeyReq::setTakeOverScanNode");
  scanInfo |= (node << TAKE_OVER_NODE_SHIFT);
}

inline
void
TcKeyReq::setTakeOverScanInfo(UintR & scanInfo, Uint32 aScanInfo){
//  ASSERT_MAX(aScanInfo, SCAN_INFO_MASK, "TcKeyReq::setTakeOverScanInfo");
  scanInfo |= (aScanInfo << SCAN_INFO_SHIFT);
}


inline
Uint16
TcKeyReq::getAPIVersion(const UintR & anAttrLen){
  return (Uint16)((anAttrLen >> API_VER_NO_SHIFT) & API_VER_NO_MASK);
}

inline
void
TcKeyReq::setAPIVersion(UintR & anAttrLen, Uint16 apiVersion){
// ASSERT_MAX(apiVersion, API_VER_NO_MASK, "TcKeyReq::setAPIVersion");
  anAttrLen |= (apiVersion << API_VER_NO_SHIFT);
}

inline
Uint16
TcKeyReq::getAttrinfoLen(const UintR & anAttrLen){
  return (Uint16)((anAttrLen) & ATTRLEN_MASK);
}

inline
void
TcKeyReq::setAttrinfoLen(UintR & anAttrLen, Uint16 aiLen){
//  ASSERT_MAX(aiLen, ATTRLEN_MASK, "TcKeyReq::setAttrinfoLen");
  anAttrLen |= aiLen;
}


#endif

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

#ifndef TC_INDX_H
#define TC_INDX_H

#include "SignalData.hpp"

class TcIndxReq {
  /**
   * Reciver(s)
   */
  friend class Dbtc;         // Reciver

  /**
   * Sender(s)
   */
  friend class NdbIndexOperation; 

  /**
   * For printing
   */
  friend bool printTCINDXREQ(FILE *, const Uint32 *, Uint32, Uint16);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( StaticLength = 8 );
  STATIC_CONST( SignalLength = 25 );
  STATIC_CONST( MaxKeyInfo = 8 );
  STATIC_CONST( MaxAttrInfo = 5 );

private:

  enum CommitType {
    CommitIfFailFree = 0,
    TryCommit = 1,
    CommitAsMuchAsPossible = 2
  };

  /**
   * DATA VARIABLES
   */
//-------------------------------------------------------------
// Unconditional part. First 8 words
//-------------------------------------------------------------
  UintR apiConnectPtr;        // DATA 0
  UintR senderData;           // DATA 1
  UintR attrLen;              // DATA 2 (including API Version)
  UintR indexId;              // DATA 3
  UintR requestInfo;          // DATA 4
  UintR indexSchemaVersion;   // DATA 5
  UintR transId1;             // DATA 6
  UintR transId2;             // DATA 7
//-------------------------------------------------------------
// Conditional part. Those four words will be sent only if their
// indicator is set.
//-------------------------------------------------------------
  UintR scanInfo;             // DATA 8
  UintR distrGroupHashValue;  // DATA 9
  UintR distributionKeySize;  // DATA 10
  UintR storedProcId;         // DATA 11

//-------------------------------------------------------------
// Variable sized key and attrinfo part. Those will be placed to
// pack the signal in an appropriate manner.
//-------------------------------------------------------------
  UintR keyInfo[MaxKeyInfo];           // DATA 12 - 19
  UintR attrInfo[MaxAttrInfo];         // DATA 20 - 24

  static Uint8  getAPIVersion(const UintR & attrLen);

  /**
   * Get:ers for requestInfo
   */
  static Uint8 getCommitFlag(const UintR & requestInfo);
  static Uint8 getCommitType(const UintR & requestInfo);
  static Uint8 getStartFlag(const UintR & requestInfo);
  static Uint8 getSimpleFlag(const UintR & requestInfo);
  static Uint8 getDirtyFlag(const UintR & requestInfo);
  static Uint8 getInterpretedFlag(const UintR & requestInfo);
  static Uint8 getDistributionGroupFlag(const UintR & requestInfo);
  static Uint8 getDistributionGroupTypeFlag(const UintR & requestInfo);
  static Uint8 getDistributionKeyFlag(const UintR & requestInfo);
  static Uint8 getScanIndFlag(const UintR & requestInfo);
  
  static Uint8 getOperationType(const UintR & requestInfo);

  static Uint16 getIndexLength(const UintR & requestInfo);
  static Uint8  getAIInTcIndxReq(const UintR & requestInfo);

  /**
   * Get:ers for scanInfo
   */

  static void setAPIVersion(UintR & attrLen, Uint16 apiVersion);

  /**
   * Set:ers for requestInfo
   */
  static void clearRequestInfo(UintR & requestInfo);
  static void setCommitType(UintR & requestInfo, Uint32 type);
  static void setCommitFlag(UintR & requestInfo, Uint32 flag);
  static void setStartFlag(UintR & requestInfo, Uint32 flag);
  static void setSimpleFlag(UintR & requestInfo, Uint32 flag);
  static void setDirtyFlag(UintR & requestInfo, Uint32 flag);
  static void setInterpretedFlag(UintR & requestInfo, Uint32 flag);
  static void setDistributionGroupFlag(UintR & requestInfo, Uint32 flag);
  static void setDistributionGroupTypeFlag(UintR & requestInfo, Uint32 flag);
  static void setDistributionKeyFlag(UintR & requestInfo, Uint32 flag);
  static void setScanIndFlag(UintR & requestInfo, Uint32 flag);
  
  static void setOperationType(UintR & requestInfo, Uint32 type);
  
  static void setIndexLength(UintR & requestInfo, Uint32 len);
  static void setAIInTcIndxReq(UintR & requestInfo, Uint32 len);

  /**
   * Set:ers for scanInfo
   */

};

#define API_VER_NO_SHIFT     (16)
#define API_VER_NO_MASK      (65535)

/**
 * Request Info
 *
 a = Attr Info in TCINDXREQ - 3  Bits -> Max 7 (Bit 16-18)
 b = Distribution Key Ind   - 1  Bit 2
 c = Commit Indicator       - 1  Bit 4
 d = Dirty Indicator        - 1  Bit 0
 e = Scan Indicator         - 1  Bit 14
 g = Distribution Group Ind - 1  Bit 1
 i = Interpreted Indicator  - 1  Bit 15
 k = Index lengt            - 12 Bits -> Max 4095 (Bit 20 - 31)
 o = Operation Type         - 3  Bits -> Max 7 (Bit 5-7)
 p = Simple Indicator       - 1  Bit 8
 s = Start Indicator        - 1  Bit 11
 t = Distribution GroupType - 1  Bit 3
 y = Commit Type            - 2  Bit 12-13
 x = Last Op in execute     - 1  Bit 19

           1111111111222222222233
 01234567890123456789012345678901
 dgbtcooop  syyeiaaa-kkkkkkkkkkkk
*/

#define COMMIT_SHIFT       (4)
#define START_SHIFT        (11)
#define SIMPLE_SHIFT       (8)
#define DIRTY_SHIFT        (0)
#define INTERPRETED_SHIFT  (15)
#define DISTR_GROUP_SHIFT  (1)
#define DISTR_GROUP_TYPE_SHIFT  (3)
#define DISTR_KEY_SHIFT    (2)
#define SCAN_SHIFT         (14)

#define OPERATION_SHIFT   (5)
#define OPERATION_MASK    (7)

#define AINFO_SHIFT       (16)
#define AINFO_MASK        (7)

#define INDEX_LEN_SHIFT     (20)
#define INDEX_LEN_MASK      (4095)

#define COMMIT_TYPE_SHIFT  (12)
#define COMMIT_TYPE_MASK   (3)

#define LAST_OP_IN_EXEC_SHIFT (19)

/**
 * Scan Info
 *
 

           1111111111222222222233
 01234567890123456789012345678901
 
*/

inline
Uint8
TcIndxReq::getCommitFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> COMMIT_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getCommitType(const UintR & requestInfo){
  return (Uint8)((requestInfo >> COMMIT_TYPE_SHIFT) & COMMIT_TYPE_MASK);
}

inline
Uint8
TcIndxReq::getStartFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> START_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getSimpleFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> SIMPLE_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getDirtyFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DIRTY_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getInterpretedFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> INTERPRETED_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getDistributionGroupFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DISTR_GROUP_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getDistributionGroupTypeFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DISTR_GROUP_TYPE_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getDistributionKeyFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> DISTR_KEY_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getScanIndFlag(const UintR & requestInfo){
  return (Uint8)((requestInfo >> SCAN_SHIFT) & 1);
}

inline
Uint8
TcIndxReq::getOperationType(const UintR & requestInfo){
  return (Uint8)((requestInfo >> OPERATION_SHIFT) & OPERATION_MASK);
}

inline
Uint16 
TcIndxReq::getIndexLength(const UintR & requestInfo){
  return (Uint16)((requestInfo >> INDEX_LEN_SHIFT) & INDEX_LEN_MASK);
}

inline
Uint8
TcIndxReq::getAIInTcIndxReq(const UintR & requestInfo){
  return (Uint8)((requestInfo >> AINFO_SHIFT) & AINFO_MASK);
}

inline
void 
TcIndxReq::clearRequestInfo(UintR & requestInfo){
  requestInfo = 0;
}

inline
void 
TcIndxReq::setCommitType(UintR & requestInfo, Uint32 type){
  ASSERT_MAX(type, COMMIT_TYPE_MASK, "TcIndxReq::setCommitType");
  requestInfo |= (type << COMMIT_TYPE_SHIFT);
}

inline
void 
TcIndxReq::setCommitFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setCommitFlag");
  requestInfo &= ~(1 << COMMIT_SHIFT);
  requestInfo |= (flag << COMMIT_SHIFT);
}

inline
void 
TcIndxReq::setStartFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setStartFlag");
  requestInfo &= ~(1 << START_SHIFT);
  requestInfo |= (flag << START_SHIFT);
}

inline
void 
TcIndxReq::setSimpleFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setSimpleFlag");
  requestInfo &= ~(1 << SIMPLE_SHIFT);
  requestInfo |= (flag << SIMPLE_SHIFT);
}

inline
void 
TcIndxReq::setDirtyFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setDirtyFlag");
  requestInfo &= ~(1 << DIRTY_SHIFT);
  requestInfo |= (flag << DIRTY_SHIFT);
}

inline
void 
TcIndxReq::setInterpretedFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setInterpretedFlag");
  requestInfo &= ~(1 << INTERPRETED_SHIFT);
  requestInfo |= (flag << INTERPRETED_SHIFT);
}

inline
void 
TcIndxReq::setDistributionGroupTypeFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setDistributionGroupTypeFlag");
  requestInfo &= ~(1 << DISTR_GROUP_TYPE_SHIFT);
  requestInfo |= (flag << DISTR_GROUP_TYPE_SHIFT);
}

inline
void 
TcIndxReq::setDistributionGroupFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setDistributionGroupFlag");
  requestInfo &= ~(1 << DISTR_GROUP_SHIFT);
  requestInfo |= (flag << DISTR_GROUP_SHIFT);
}

inline
void 
TcIndxReq::setDistributionKeyFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setDistributionKeyFlag");
  requestInfo &= ~(1 << DISTR_KEY_SHIFT);
  requestInfo |= (flag << DISTR_KEY_SHIFT);
}

inline
void 
TcIndxReq::setScanIndFlag(UintR & requestInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxReq::setScanIndFlag");
  requestInfo &= ~(1 << SCAN_SHIFT);
  requestInfo |= (flag << SCAN_SHIFT);
}

inline
void 
TcIndxReq::setOperationType(UintR & requestInfo, Uint32 type){
  ASSERT_MAX(type, OPERATION_MASK, "TcIndxReq::setOperationType");
  requestInfo |= (type << OPERATION_SHIFT);
}

inline
void 
TcIndxReq::setIndexLength(UintR & requestInfo, Uint32 len){
  ASSERT_MAX(len, INDEX_LEN_MASK, "TcIndxReq::setKeyLength");
  requestInfo |= (len << INDEX_LEN_SHIFT);
}

inline
void 
TcIndxReq::setAIInTcIndxReq(UintR & requestInfo, Uint32 len){
  ASSERT_MAX(len, AINFO_MASK, "TcIndxReq::setAIInTcIndxReq");
  requestInfo |= (len << AINFO_SHIFT);
}

inline
Uint8 
TcIndxReq::getAPIVersion(const UintR & anAttrLen){
  return (Uint16)((anAttrLen >> API_VER_NO_SHIFT) & API_VER_NO_MASK);
}

inline
void
TcIndxReq::setAPIVersion(UintR & anAttrLen, Uint16 apiVersion){
//  ASSERT_MAX(apiVersion, API_VER_NO_MASK, "TcIndxReq::setAPIVersion");
  anAttrLen |= (apiVersion << API_VER_NO_SHIFT);
}

class TcIndxConf {

  /**
   * Reciver(s)
   */
  friend class Ndb;
  friend class NdbConnection;

  /**
   * Sender(s)
   */
  friend class Dbtc; 

  /**
   * For printing
   */
  friend bool printTCINDXCONF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 5 );

private:
  /**
   * DATA VARIABLES
   */
  //-------------------------------------------------------------
  // Unconditional part. First 5 words
  //-------------------------------------------------------------

  Uint32 apiConnectPtr;
  Uint32 gci;
  Uint32 confInfo;
  Uint32 transId1;
  Uint32 transId2;

  struct OperationConf {
    Uint32 apiOperationPtr;
    Uint32 attrInfoLen;
  };
  //-------------------------------------------------------------
  // Operations confirmations,
  // No of actually sent = getNoOfOperations(confInfo)
  //-------------------------------------------------------------
  OperationConf operations[10];
  
  /**
   * Get:ers for confInfo
   */
  static Uint32 getNoOfOperations(const Uint32 & confInfo);
  static Uint32 getCommitFlag(const Uint32 & confInfo);
  static bool getMarkerFlag(const Uint32 & confInfo);
  
  /**
   * Set:ers for confInfo
   */
  static void setCommitFlag(Uint32 & confInfo, Uint8 flag);
  static void setNoOfOperations(Uint32 & confInfo, Uint32 noOfOps);
  static void setMarkerFlag(Uint32 & confInfo, Uint32 flag);
};

inline
Uint32
TcIndxConf::getNoOfOperations(const Uint32 & confInfo){
  return confInfo & 65535;
}

inline
Uint32
TcIndxConf::getCommitFlag(const Uint32 & confInfo){
  return ((confInfo >> 16) & 1);
}

inline
bool
TcIndxConf::getMarkerFlag(const Uint32 & confInfo){
  const Uint32 bits = 3 << 16; // Marker only valid when doing commit
  return (confInfo & bits) == bits;
}

inline
void 
TcIndxConf::setNoOfOperations(Uint32 & confInfo, Uint32 noOfOps){
  ASSERT_MAX(noOfOps, 65535, "TcIndxConf::setNoOfOperations");
  confInfo |= noOfOps;
}

inline
void 
TcIndxConf::setCommitFlag(Uint32 & confInfo, Uint8 flag){
  ASSERT_BOOL(flag, "TcIndxConf::setCommitFlag");
  confInfo |= (flag << 16);
}

inline
void
TcIndxConf::setMarkerFlag(Uint32 & confInfo, Uint32 flag){
  ASSERT_BOOL(flag, "TcIndxConf::setMarkerFlag");
  confInfo |= (flag << 17);
}

class TcIndxRef {

  /**
   * Reciver(s)
   */
  friend class NdbIndexOperation;

  /**
   * Sender(s)
   */
  friend class Dbtc; 

  /**
   * For printing
   */
  friend bool printTCINDXREF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  /**
   * Length of signal
   */
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 errorCode;
};

#endif

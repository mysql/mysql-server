/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef BUILD_INDX_HPP
#define BUILD_INDX_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <signaldata/DictTabInfo.hpp>

/**
 * BuildIndxReq
 *
 * This signal is sent by DICT to TRIX(n)
 * as a request to build a secondary index
 */
class BuildIndxReq {
  friend bool printBUILD_INDX_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  enum RequestType {
    RT_UNDEFINED = 0,
    RT_USER = 1,
    RT_ALTER_INDEX = 2,
    RT_SYSTEMRESTART = 3,
    RT_DICT_PREPARE = 1 << 4,
    RT_DICT_TC = 5 << 4,
    RT_DICT_TRIX = 7 << 4,
    RT_DICT_TUX = 8 << 4,
    RT_DICT_COMMIT = 0xC << 4,
    RT_DICT_ABORT = 0xF << 4,
    RT_TRIX = 7 << 8
  };
  STATIC_CONST( SignalLength = 9 );
  STATIC_CONST( INDEX_COLUMNS = 0 );
  STATIC_CONST( KEY_COLUMNS = 1 );
  STATIC_CONST( NoOfSections = 2 );

private:
  Uint32 m_userRef;             // user block reference
  Uint32 m_connectionPtr;       // user "schema connection"
  Uint32 m_requestInfo;
  Uint32 m_buildId;		// Suma subscription id
  Uint32 m_buildKey;		// Suma subscription key
  Uint32 m_tableId;             // table being indexed
  Uint32 m_indexType;           // from DictTabInfo::TableType
  Uint32 m_indexId;             // table storing index
  Uint32 m_parallelism;		// number of parallel insert transactions
  // extra
  Uint32 m_opKey;
  // Sent data ends here
  Uint32 m_slack[25 - SignalLength - 1];
  Uint32 m_sectionBuffer[MAX_ATTRIBUTES_IN_TABLE * 2];

public:
  Uint32 getUserRef() const {
    return m_userRef;
  }
  void setUserRef(Uint32 val) {
    m_userRef = val;
  }
  Uint32 getConnectionPtr() const {
    return m_connectionPtr;
  }
  void setConnectionPtr(Uint32 val) {
    m_connectionPtr = val;
  }
  BuildIndxReq::RequestType getRequestType() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_requestInfo, 0, 16);
    return (BuildIndxReq::RequestType)val;
  }
  void setRequestType(BuildIndxReq::RequestType val) {
    m_requestInfo = (Uint32)val;
  }
  Uint32 getRequestFlag() const {
    const Uint32 val = BitmaskImpl::getField(1, &m_requestInfo, 16, 16);
    return (BuildIndxReq::RequestType)val;
  };
  void addRequestFlag(Uint32 val) {
    val |= BitmaskImpl::getField(1, &m_requestInfo, 16, 16);
    BitmaskImpl::setField(1, &m_requestInfo, 16, 16, val);
  };
  Uint32 getTableId() const {
    return m_tableId;
  }
  void setTableId(Uint32 val) {
    m_tableId = val;
  }
  Uint32 getBuildId() const {
    return m_buildId;
  }
  void setBuildId(Uint32 val) {
    m_buildId = val;
  }
  Uint32 getBuildKey() const {
    return m_buildKey;
  }
  void setBuildKey(Uint32 val) {
    m_buildKey = val;
  }
  Uint32 getIndexType() const {
    return m_indexType;
  }
  void setIndexType(Uint32 val) {
    m_indexType = val;
  }
  Uint32 getIndexId() const {
    return m_indexId;
  }
  void setIndexId(Uint32 val) {
    m_indexId = val;
  }
  Uint32 getParallelism() const {
    return m_parallelism;
  }
  void setParallelism(Uint32 val) {
    m_parallelism = val;
  }
  Uint32 getOpKey() const {
    return m_opKey;
  }
  void setOpKey(Uint32 val) {
    m_opKey = val;
  }
  // Column order
  void setColumnOrder(Uint32* indexBuf, Uint32 indexLen,
		      Uint32* keyBuf, Uint32 keyLen,
		      struct LinearSectionPtr orderPtr[]);
};

inline
void BuildIndxReq::setColumnOrder(Uint32* indexBuf, Uint32 indexLen, 
				  Uint32* keyBuf, Uint32 keyLen,
				  struct LinearSectionPtr orderPtr[])

{
  printf("BuildIndxReq::setColumnOrder: indexLen %u, keyLen %u\n", indexLen, keyLen);
  // Copy buffers
  MEMCOPY_NO_WORDS(m_sectionBuffer, indexBuf, indexLen);
  MEMCOPY_NO_WORDS(m_sectionBuffer + indexLen, keyBuf, keyLen);
  orderPtr[INDEX_COLUMNS].p = m_sectionBuffer;
  orderPtr[INDEX_COLUMNS].sz = indexLen;
  orderPtr[KEY_COLUMNS].p = m_sectionBuffer + indexLen;
  orderPtr[KEY_COLUMNS].sz = keyLen;
}

/**
 * BuildIndxConf
 *
 * This signal is sent back to DICT from TRIX
 * as confirmation of succesfull index build
 * (BuildIndxReq).
 */
class BuildIndxConf {
  friend bool printBUILD_INDX_CONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( InternalLength = 3 );
  STATIC_CONST( SignalLength = 6 );

private:
  friend class BuildIndxRef;
  Uint32 m_userRef;
  Uint32 m_connectionPtr;
  Uint32 m_requestInfo;
  Uint32 m_tableId;
  Uint32 m_indexType;
  Uint32 m_indexId;

public:
  Uint32 getUserRef() const {
    return m_userRef;
  }
  void setUserRef(Uint32 val) {
    m_userRef = val;
  }
  Uint32 getConnectionPtr() const {
    return m_connectionPtr;
  }
  void setConnectionPtr(Uint32 val) {
    m_connectionPtr = val;
  }
  BuildIndxReq::RequestType getRequestType() const {
    return (BuildIndxReq::RequestType)m_requestInfo;
  }
  void setRequestType(BuildIndxReq::RequestType val) {
    m_requestInfo = (Uint32)val;
  }
  Uint32 getTableId() const {
    return m_tableId;
  }
  void setTableId(Uint32 val) {
    m_tableId = val;
  }
  Uint32 getIndexType() const {
    return m_indexType;
  }
  void setIndexType(Uint32 val) {
    m_indexType = val;
  }
  Uint32 getIndexId() const {
    return m_indexId;
  }
  void setIndexId(Uint32 val) {
    m_indexId = val;
  }
};

/**
 * BuildIndxRef
 *
 * This signal is sent back to API from DICT/TRIX
 * as refusal of a failed index creation
 * (BuildIndxReq). It is also sent as refusal
 * from TC to TRIX and TRIX to DICT.
 */
class BuildIndxRef {
  friend bool printBUILD_INDX_REF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    BadRequestType = 4247,
    InvalidPrimaryTable = 4249,
    InvalidIndexType = 4250,
    IndexNotUnique = 4251,
    AllocationFailure = 4252,
    InternalError = 4346
  };
  STATIC_CONST( SignalLength = BuildIndxConf::SignalLength + 2 );

  //Uint32 m_userRef;
  //Uint32 m_connectionPtr;
  //Uint32 m_requestInfo;
  //Uint32 m_tableId;
  //Uint32 m_indexType;
  //Uint32 m_indexId;
  BuildIndxConf m_conf;
  Uint32 m_errorCode;
  Uint32 masterNodeId;

public:
  BuildIndxConf* getConf() {
    return &m_conf;
  }
  const BuildIndxConf* getConf() const {
    return &m_conf;
  }
  Uint32 getUserRef() const {
    return m_conf.getUserRef();
  }
  void setUserRef(Uint32 val) {
    m_conf.setUserRef(val);
  }
  Uint32 getConnectionPtr() const {
    return m_conf.getConnectionPtr();
  }
  void setConnectionPtr(Uint32 val) {
    m_conf.setConnectionPtr(val);
  }
  BuildIndxReq::RequestType getRequestType() const {
    return m_conf.getRequestType();
  }
  void setRequestType(BuildIndxReq::RequestType val) {
    m_conf.setRequestType(val);
  }
  Uint32 getTableId() const {
    return m_conf.getTableId();
  }
  void setTableId(Uint32 val) {
    m_conf.setTableId(val);
  }
  Uint32 getIndexType() const {
    return m_conf.getIndexType();
  }
  void setIndexType(Uint32 val) {
    m_conf.setIndexType(val);
  }
  Uint32 getIndexId() const {
    return m_conf.getIndexId();
  }
  void setIndexId(Uint32 val) {
    m_conf.setIndexId(val);
  }
  BuildIndxRef::ErrorCode getErrorCode() const {
    return (BuildIndxRef::ErrorCode)m_errorCode;
  }
  void setErrorCode(BuildIndxRef::ErrorCode val) {
    m_errorCode = (Uint32)val;
  }
};

#endif
